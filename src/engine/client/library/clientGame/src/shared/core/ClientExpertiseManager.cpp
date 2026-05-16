//======================================================================
//
// ClientExpertiseManager.cpp
// copyright (c) 2006 Sony Online Entertainment
//
//======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/ClientExpertiseManager.h"

#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientGame/GameNetwork.h"
#include "clientGame/PlayerObject.h"
#include "clientUserInterface/CuiSkillManager.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedNetworkMessages/ExpertiseRequestMessage.h"
#include "sharedSkillSystem/ExpertiseManager.h"
#include "sharedSkillSystem/SkillManager.h"
#include "sharedSkillSystem/SkillObject.h"
#include "sharedUtility/DataTable.h"
#include "sharedUtility/DataTableManager.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <vector>
#include <set>

#include <zlib.h>

//======================================================================

namespace ClientExpertiseManagerNamespace
{
	bool s_installed = false;

	std::string const cs_expertiseTreeStringTable("expertise_n");
	std::string const cs_expertiseDescriptionStringTable("expertise_d");

	ExpertiseManager::ExpertiseCoord const cs_invalidCoord(0, 0, 0, 0);
	ClientExpertiseManager::PostreqList const cs_emptyPostreqList;
	DataTable const cs_unusedDataTable;

	// grid x,y,z -> arrow flags
	typedef std::map<ExpertiseManager::ExpertiseCoord, ClientExpertiseManager::ExpertiseArrowFlags> ExpertiseArrowGrid;
	ExpertiseArrowGrid s_arrowGrid;

	// grid x,y,z -> postreq skills the square leads to
	typedef std::map<ExpertiseManager::ExpertiseCoord, std::set<std::string> > ExpertisePostreqGrid;
	ExpertisePostreqGrid s_postreqGrid;

	void buildGrid(DataTable const & datatable);
	ExpertiseManager::ExpertiseCoord const connectExpertises(ExpertiseManager::ExpertiseCoord const & startCoord, SkillObject const * targetSkill, int direction);
	SkillObject const * findExpertisePostreqBelow(SkillObject const * skill, int tree, int tier, int grid);
	SkillObject const * findExpertisePostreqToLeft(SkillObject const * skill, int tree, int tier, int grid);
	SkillObject const * findExpertisePostreqToRight(SkillObject const * skill, int tree, int tier, int grid);

	std::vector<std::string> s_allocatedExpertises;

	std::string s_emptyString;

	char const cs_buildMagic[] = "SWGEXP1";

	void trimBuildToken(std::string &token)
	{
		std::string::size_type a = 0;
		while (a < token.size() && (token[a] == ' ' || token[a] == '\t' || token[a] == '\r'))
			++a;
		std::string::size_type b = token.size();
		while (b > a && (token[b - 1] == ' ' || token[b - 1] == '\t' || token[b - 1] == '\r'))
			--b;
		if (a != 0 || b != token.size())
			token = token.substr(a, b - a);
	}

	bool playerTreeIdAllowed(int treeId)
	{
		ClientExpertiseManager::TreeIdList const & allowed = ClientExpertiseManager::getExpertiseTreesForPlayer();
		for (ClientExpertiseManager::TreeIdList::const_iterator it = allowed.begin(); it != allowed.end(); ++it)
		{
			if (*it == treeId)
				return true;
		}
		return false;
	}

	bool compareExpertiseSkillNames(std::string const &a, std::string const &b)
	{
		int const ta = ExpertiseManager::getExpertiseTree(a);
		int const tb = ExpertiseManager::getExpertiseTree(b);
		if (ta != tb)
			return ta < tb;
		int const tia = ExpertiseManager::getExpertiseTier(a);
		int const tib = ExpertiseManager::getExpertiseTier(b);
		if (tia != tib)
			return tia < tib;
		int const ga = ExpertiseManager::getExpertiseGrid(a);
		int const gb = ExpertiseManager::getExpertiseGrid(b);
		if (ga != gb)
			return ga < gb;
		int const ra = ExpertiseManager::getExpertiseRank(a);
		int const rb = ExpertiseManager::getExpertiseRank(b);
		if (ra != rb)
			return ra < rb;
		return a < b;
	}

	void collectFullExpertiseBuildSkillNames(std::set<std::string> & out)
	{
		out.clear();
		CreatureObject const * const player = Game::getPlayerCreature();
		if (!player)
			return;
		CreatureObject::SkillList expertiseList;
		ClientExpertiseManager::getExpertisesForPlayer(expertiseList);
		for (CreatureObject::SkillList::const_iterator i = expertiseList.begin(); i != expertiseList.end(); ++i)
		{
			if (*i)
			{
				std::string const &n = (*i)->getSkillName();
				if (n != "expertise")
					out.insert(n);
			}
		}
		for (std::vector<std::string>::const_iterator j = s_allocatedExpertises.begin(); j != s_allocatedExpertises.end(); ++j)
		{
			if (!j->empty())
				out.insert(*j);
		}
	}

	bool parseBuildBody(std::string const & sourceSkillTemplate, std::vector<std::string> const & rawSkills,
	                    std::string & resultMessage)
	{
		CreatureObject const * const player = Game::getPlayerCreature();
		if (!player)
		{
			resultMessage = "No player creature.";
			return false;
		}

		std::string const currentTemplate = CuiSkillManager::getSkillTemplate();
		bool professionMismatch = !sourceSkillTemplate.empty() && !currentTemplate.empty() &&
		                          sourceSkillTemplate != currentTemplate;

		std::set<std::string> targetSkills;
		int skippedUnknown = 0;
		int skippedWrongTree = 0;

		for (std::vector<std::string>::const_iterator it = rawSkills.begin(); it != rawSkills.end(); ++it)
		{
			std::string name = *it;
			trimBuildToken(name);
			if (name.empty())
				continue;
			SkillObject const * sk = SkillManager::getInstance().getSkill(name);
			if (!sk || !ExpertiseManager::isExpertise(sk))
			{
				++skippedUnknown;
				continue;
			}
			int const tree = ExpertiseManager::getExpertiseTree(name);
			if (!playerTreeIdAllowed(tree))
			{
				++skippedWrongTree;
				continue;
			}
			targetSkills.insert(name);
		}

		ClientExpertiseManager::clearAllocatedExpertises();

		std::vector<std::string> ordered(targetSkills.begin(), targetSkills.end());
		std::sort(ordered.begin(), ordered.end(), compareExpertiseSkillNames);

		int allocatedPasses = 0;
		bool progress = true;
		while (progress)
		{
			progress = false;
			for (std::vector<std::string>::const_iterator o = ordered.begin(); o != ordered.end(); ++o)
			{
				if (ClientExpertiseManager::playerHasExpertise(*o))
					continue;
				if (ClientExpertiseManager::hasAllocatedExpertise(*o))
					continue;
				if (ClientExpertiseManager::allocateExpertise(*o, true))
				{
					progress = true;
					++allocatedPasses;
				}
			}
		}

		int couldNotAllocate = 0;
		for (std::vector<std::string>::const_iterator o = ordered.begin(); o != ordered.end(); ++o)
		{
			if (!ClientExpertiseManager::playerHasExpertise(*o) && !ClientExpertiseManager::hasAllocatedExpertise(*o))
				++couldNotAllocate;
		}

		std::ostringstream msg;
		if (professionMismatch)
			msg << "Warning: build is for profession \"" << sourceSkillTemplate << "\" but you are \"" << currentTemplate << "\".\n";
		msg << "Imported toward " << (ordered.size() - couldNotAllocate) << " of " << ordered.size()
		    << " valid expertise skills (" << allocatedPasses << " new allocations pending train).";
		if (skippedUnknown)
			msg << "\nSkipped " << skippedUnknown << " unknown or non-expertise names.";
		if (skippedWrongTree)
			msg << "\nSkipped " << skippedWrongTree << " skills not in your expertise trees.";
		if (couldNotAllocate)
			msg << "\nCould not reach " << couldNotAllocate << " skills (points, prerequisites, or tier gates).";
		resultMessage = msg.str();
		return true;
	}
};

using namespace ClientExpertiseManagerNamespace;

//======================================================================

void ClientExpertiseManagerNamespace::buildGrid(DataTable const & datatable)
{
	UNREF(datatable); // unused; required for callbacks only

	s_arrowGrid.clear();
	s_postreqGrid.clear();

	ExpertiseManager::TreeIdList treeIdList;
	ExpertiseManager::getExpertiseTrees(treeIdList);

	for (ExpertiseManager::TreeIdList::const_iterator i = treeIdList.begin(); i != treeIdList.end(); ++i)
	{
		int tree = *i;
		for (int tier = 1; tier <= ExpertiseManager::getNumExpertiseTiers(); ++tier)
		{
			for (int grid = 1; grid <= ExpertiseManager::getNumExpertiseColumns(); ++grid)
			{
				SkillObject const * expertise = ExpertiseManager::getExpertiseSkillAt(tree, tier, grid);
				if (!expertise)
				{
					continue;
				}

				ExpertiseManager::ExpertiseCoord startCoord(tree, tier, grid);
				ExpertiseManager::ExpertiseCoord nextStartCoord;

				int rankMax = ExpertiseManager::getExpertiseRankMax(expertise->getSkillName());
				SkillObject const * expertiseMaxRank = ExpertiseManager::getExpertiseSkillAt(tree, tier, grid, rankMax);

				SkillObject const * postreqBelow   = findExpertisePostreqBelow(expertiseMaxRank, tree, tier, grid);
				SkillObject const * postreqToLeft  = findExpertisePostreqToLeft(expertiseMaxRank, tree, tier, grid);
				SkillObject const * postreqToRight = findExpertisePostreqToRight(expertiseMaxRank, tree, tier, grid);

				if (postreqBelow)
				{
					// Draw DOWN then left or right.
					nextStartCoord = connectExpertises(startCoord, postreqBelow, ClientExpertiseManager::EAF_Down);
					if (postreqBelow == postreqToLeft && nextStartCoord != cs_invalidCoord)
					{
						IGNORE_RETURN(connectExpertises(nextStartCoord, postreqBelow, ClientExpertiseManager::EAF_LeftOutgoing));
					}
					else if (postreqBelow == postreqToRight && nextStartCoord != cs_invalidCoord)
					{
						IGNORE_RETURN(connectExpertises(nextStartCoord, postreqBelow, ClientExpertiseManager::EAF_RightOutgoing));
					}
				}
				if (postreqToLeft && postreqToLeft != postreqBelow)
				{
					// Draw LEFT then down.
					nextStartCoord = connectExpertises(startCoord, postreqToLeft, ClientExpertiseManager::EAF_LeftOutgoing);
					if (nextStartCoord != cs_invalidCoord)
					{
						IGNORE_RETURN(connectExpertises(nextStartCoord, postreqToLeft, ClientExpertiseManager::EAF_Down));
					}
				}
				if (postreqToRight && postreqToRight != postreqBelow)
				{
					// Draw RIGHT then down.
					nextStartCoord = connectExpertises(startCoord, postreqToRight, ClientExpertiseManager::EAF_RightOutgoing);
					if (nextStartCoord != cs_invalidCoord)
					{
						IGNORE_RETURN(connectExpertises(nextStartCoord, postreqToRight, ClientExpertiseManager::EAF_Down));
					}
				}
			}
		}
	}

}

//----------------------------------------------------------------------

/**
 * @param startCoord - grid square to start from
 * @param targetSkill - skill to "draw" towards
 * @param direction - direction to "draw" in
 * 
 * @return the coordinate the routine stopped drawing at
 * (if destination was NOT reached), or an invalid
 * coordinate (if destination WAS reached).
 * 
 * this allows the caller to draw an L-shape by calling
 * this routine twice: i.e. making the 2nd call with the
 * starting coordinate set to the previous call's ending
 * coordinate.
 */
ExpertiseManager::ExpertiseCoord const ClientExpertiseManagerNamespace::connectExpertises(ExpertiseManager::ExpertiseCoord const & startCoord, SkillObject const * targetSkill, int direction)
{
	ExpertiseManager::ExpertiseCoord nextCoord(startCoord);

	ExpertiseManager::ExpertiseCoord endCoord;
	std::string const & targetSkillName = targetSkill->getSkillName();
	endCoord.tree = ExpertiseManager::getExpertiseTree(targetSkillName);
	endCoord.tier = ExpertiseManager::getExpertiseTier(targetSkillName);
	endCoord.grid = ExpertiseManager::getExpertiseGrid(targetSkillName);

	bool changingDirection = false;

	do
	{
		// Mark exit point from starting square
		int arrowFlags = s_arrowGrid[nextCoord];
		arrowFlags |= direction;
		s_arrowGrid[nextCoord] = static_cast<ClientExpertiseManager::ExpertiseArrowFlags>(arrowFlags);

		// Move to next square in desired direction. Early exit possible due to the need
		// to draw L shapes (i.e. stop at the turn, requiring a draw in another direction).
		if (direction == ClientExpertiseManager::EAF_Down)
		{
			if (nextCoord.tier < endCoord.tier)
			{
				++nextCoord.tier;
			}
			else
			{
				break;
			}
		}
		else if (direction == ClientExpertiseManager::EAF_LeftOutgoing)
		{
			if (nextCoord.grid > endCoord.grid)
			{
				--nextCoord.grid;
			}
			else
			{
				break;
			}
		}
		else if (direction == ClientExpertiseManager::EAF_RightOutgoing)
		{
			if (nextCoord.grid < endCoord.grid)
			{
				++nextCoord.grid;
			}
			else
			{
				break;
			}
		}
		else
		{
			DEBUG_WARNING(true, ("ClientExpertiseManager: bad ExpertiseArrowFlag %d", direction));
			break;
		}

		// Mark square as leading to the targetSkill for highlighting purposes.
		ClientExpertiseManager::PostreqList & postreqList = s_postreqGrid[nextCoord];
		postreqList.insert(targetSkillName);

		// Set arrow flags for square.
		// Only set flag for far side of square if we know we're going further in that direction.
		arrowFlags = s_arrowGrid[nextCoord];
		if (direction & ClientExpertiseManager::EAF_Down)
		{
			arrowFlags |= ClientExpertiseManager::EAF_Up;
			if (nextCoord.tier != endCoord.tier)
			{
				arrowFlags |= ClientExpertiseManager::EAF_Down;
			}
			else
			{
				changingDirection = true;
			}
		}
		if (direction & ClientExpertiseManager::EAF_LeftOutgoing)
		{
			if (nextCoord == endCoord)
			{
				arrowFlags |= ClientExpertiseManager::EAF_RightIncoming;
			}
			else
			{
				arrowFlags |= ClientExpertiseManager::EAF_RightOutgoing;
				if (nextCoord.grid != endCoord.grid)
				{
					arrowFlags |= ClientExpertiseManager::EAF_LeftOutgoing;
				}
				else
				{
					changingDirection = true;
				}
			}
		}
		if (direction & ClientExpertiseManager::EAF_RightOutgoing)
		{
			if (nextCoord == endCoord)
			{
				arrowFlags |= ClientExpertiseManager::EAF_LeftIncoming;
			}
			else
			{
				arrowFlags |= ClientExpertiseManager::EAF_LeftOutgoing;
				if (nextCoord.grid != endCoord.grid)
				{
					arrowFlags |= ClientExpertiseManager::EAF_RightOutgoing;
				}
				else
				{
					changingDirection = true;
				}
			}
		}
		s_arrowGrid[nextCoord] = static_cast<ClientExpertiseManager::ExpertiseArrowFlags>(arrowFlags);

	} while (nextCoord != endCoord && !changingDirection);

	// If target was reached by this draw, there is no need to return
	// a valid nextCoord as the starting point for a 2nd draw.
	if (nextCoord == endCoord)
	{
		nextCoord = cs_invalidCoord;
	}

	return nextCoord;
}

//----------------------------------------------------------------------

/**
 * @param skill - expertise to start from
 * @param tree - starting tree id number
 * @param tier - starting tier number (aka "y coordinate")
 * @param grid - starting grid number (aka "x coordinate")
 * 
 * @return - skill object for closest prereq expertise searching
 *         DOWN same column, then alternating down columns to
 *         left and right. returns NULL if none found
 */
SkillObject const * ClientExpertiseManagerNamespace::findExpertisePostreqBelow(SkillObject const * skill, int tree, int tier, int grid)
{
	SkillObject const * nextSkill = 0;

	int nextGridLeft  = grid;
	int nextGridRight = grid;

	while (nextGridLeft >= 1 || nextGridRight <= ExpertiseManager::getNumExpertiseColumns())
	{
		int nextTier = tier;
		while (nextTier < ExpertiseManager::getNumExpertiseTiers())
		{
			++nextTier;
			if (nextGridLeft >= 1)
			{
				nextSkill = ExpertiseManager::getExpertiseSkillAt(tree, nextTier, nextGridLeft);
				if (nextSkill)
				{
					if (nextSkill->dependsUponSkill(*skill, true))
					{
						return nextSkill;
					}
				}
			}
			if (nextGridRight <= ExpertiseManager::getNumExpertiseColumns())
			{
				nextSkill = ExpertiseManager::getExpertiseSkillAt(tree, nextTier, nextGridRight);
				if (nextSkill)
				{
					if (nextSkill->dependsUponSkill(*skill, true))
					{
						return nextSkill;
					}
				}
			}
		}
		--nextGridLeft;
		++nextGridRight;
	}

	return 0;
}

//----------------------------------------------------------------------

/**
 * @param skill - expertise to start from
 * @param tree - starting tree id number
 * @param tier - starting tier number (aka "y coordinate")
 * @param grid - starting grid number (aka "x coordinate")
 * 
 * @return - skill object for closest prereq expertise searching
 *         LEFT then DOWN. does not search directly down (i.e.
 *         not in same column). returns NULL if none found
 */
SkillObject const * ClientExpertiseManagerNamespace::findExpertisePostreqToLeft(SkillObject const * skill, int tree, int tier, int grid)
{
	while (tier <= ExpertiseManager::getNumExpertiseTiers())
	{
		int nextGrid = grid;
		while (nextGrid > 1)
		{
			--nextGrid;
			SkillObject const * nextSkill = ExpertiseManager::getExpertiseSkillAt(tree, tier, nextGrid);
			if (nextSkill)
			{
				if (nextSkill->dependsUponSkill(*skill, true))
				{
					return nextSkill;
				}
			}
		}
		++tier;
	}

	return 0;
}

//----------------------------------------------------------------------

/**
 * @param skill - expertise to start from
 * @param tree - starting tree id number
 * @param tier - starting tier number (aka "y coordinate")
 * @param grid - starting grid number (aka "x coordinate")
 * 
 * @return - skill object for closest prereq expertise searching
 *         RIGHT then DOWN. does not search directly down (i.e.
 *         not in same column). returns NULL if none found
 */
SkillObject const * ClientExpertiseManagerNamespace::findExpertisePostreqToRight(SkillObject const * skill, int tree, int tier, int grid)
{
	while (tier <= ExpertiseManager::getNumExpertiseTiers())
	{
		int nextGrid = grid;
		while (nextGrid < ExpertiseManager::getNumExpertiseColumns())
		{
			++nextGrid;
			SkillObject const * nextSkill = ExpertiseManager::getExpertiseSkillAt(tree, tier, nextGrid);
			if (nextSkill)
			{
				if (nextSkill->dependsUponSkill(*skill, true))
				{
					return nextSkill;
				}
			}
		}
		++tier;
	}

	return 0;
}

//======================================================================

void ClientExpertiseManager::install()
{
	DEBUG_FATAL(s_installed, ("ClientExpertiseManager already installed"));
	s_installed = true;

	buildGrid(cs_unusedDataTable);
	DataTableManager::addReloadCallback(ExpertiseManager::getExpertiseDatatableName(), &buildGrid);

	ExitChain::add(ClientExpertiseManager::remove, "ClientExpertiseManager");
}

//----------------------------------------------------------------------

void ClientExpertiseManager::remove()
{
	DEBUG_FATAL(!s_installed, ("ClientExpertiseManager not installed"));
	s_installed = false;
}

//----------------------------------------------------------------------

/**
 * @param expertiseList - list to be populated with character's expertise skills
 */
void ClientExpertiseManager::getExpertisesForPlayer(CreatureObject::SkillList & expertiseList)
{
	expertiseList.clear();

	CreatureObject const * const player = Game::getPlayerCreature();
	if (player)
	{
		CreatureObject::SkillList const & skillList = player->getSkills();

		for (CreatureObject::SkillList::const_iterator i = skillList.begin(); i != skillList.end(); ++i)
		{
			if (*i)
			{
				if (ExpertiseManager::isExpertise(*i))
				{
					expertiseList.insert(*i);
				}
			}
		}
	}
}

//----------------------------------------------------------------------

/**
 * @return int - max Expertise Points available to the character
 */
int ClientExpertiseManager::getExpertisePointsTotalForPlayer()
{
	int totalPoints = 0;

	CreatureObject const * const player = Game::getPlayerCreature();
	if (player)
	{
		int16 level = player->getLevel();
		totalPoints = ExpertiseManager::getExpertisePointsForLevel(level);
	}

	return totalPoints;
}

//----------------------------------------------------------------------

/**
 * @return int - number of Expertise Points spent by the character
 *               (1 point per expertise possessed) includes allocated points
 */
int ClientExpertiseManager::getExpertisePointsSpentForPlayer()
{
	int spentPoints = 0;

	CreatureObject const * const player = Game::getPlayerCreature();
	if (player)
	{
		int count = 0;
		CreatureObject::SkillList const & skillList = player->getSkills();
		for (CreatureObject::SkillList::const_iterator i = skillList.begin(); i != skillList.end(); ++i)
		{
			const SkillObject *skill = *i;
			if(skill->getSkillName() == "expertise")
				continue;
			if(ExpertiseManager::isExpertise(skill))
				count++;
		}
		spentPoints = count;
	}

	spentPoints += getExpertisePointsAllocatedForPlayer();

	return spentPoints;
}

//----------------------------------------------------------------------

/**
 * @return int - number of Expertise Points spent by the character in specified tree
 */
int ClientExpertiseManager::getExpertisePointsSpentForPlayerInTree(int tree)
{
	int spentPoints = 0;

	CreatureObject::SkillList expertiseList;
	getExpertisesForPlayer(expertiseList);

	for (CreatureObject::SkillList::const_iterator i = expertiseList.begin(); i != expertiseList.end(); ++i)
	{
		SkillObject const * expertise = *i;
		if (expertise)
		{
			std::string const & expertiseName = expertise->getSkillName();
			if(expertiseName == "expertise")
				continue;
			if (ExpertiseManager::getExpertiseTree(expertiseName) == tree)
			{
				spentPoints += 1;
			}
		}
	}

	for (std::vector<std::string>::const_iterator i2 = s_allocatedExpertises.begin(); i2 != s_allocatedExpertises.end(); ++i2)
	{
		std::string const &expertiseName = *i2;
		if (!expertiseName.empty())
		{
			if (ExpertiseManager::getExpertiseTree(expertiseName) == tree)
			{
				spentPoints += 1;
			}
		}
	}
	return spentPoints;
}

//----------------------------------------------------------------------

int ClientExpertiseManager::getExpertisePointsSpentForPlayerInTreeUpToTier(int tree, int tier)
{
	int spentPoints = 0;

	CreatureObject::SkillList expertiseList;
	getExpertisesForPlayer(expertiseList);

	for (CreatureObject::SkillList::const_iterator i = expertiseList.begin(); i != expertiseList.end(); ++i)
	{
		SkillObject const * expertise = *i;
		if (expertise)
		{
			std::string const & expertiseName = expertise->getSkillName();
			if(expertiseName == "expertise")
				continue;
			if (ExpertiseManager::getExpertiseTree(expertiseName) == tree)
			{
				if(ExpertiseManager::getExpertiseTier(expertiseName) <= tier)
					spentPoints += 1;
			}
		}
	}

	for (std::vector<std::string>::const_iterator i2 = s_allocatedExpertises.begin(); i2 != s_allocatedExpertises.end(); ++i2)
	{
		std::string const &expertiseName = *i2;
		if (!expertiseName.empty())
		{
			if (ExpertiseManager::getExpertiseTree(expertiseName) == tree)
			{
				if(ExpertiseManager::getExpertiseTier(expertiseName) <= tier)
					spentPoints += 1;
			}
		}
	}
	return spentPoints;

}

//----------------------------------------------------------------------

/**
 * @return int - the number of Expertise Points available for
 *         the character to spend
 */
int ClientExpertiseManager::getExpertisePointsRemainingForPlayer()
{
	if (PlayerObject::isAdmin()) {
		return 999; // gods always have infinite expertise points
	}
	else {
		return getExpertisePointsTotalForPlayer() - getExpertisePointsSpentForPlayer();
	}
}

//----------------------------------------------------------------------

/**
* @return int - the number of Expertise Points allocated by the player
*/
int ClientExpertiseManager::getExpertisePointsAllocatedForPlayer()
{
	return s_allocatedExpertises.size();
}

//----------------------------------------------------------------------

int ClientExpertiseManager::getExpertisePointsAllocatedForPlayerInTree(int tree)
{
	int spentPoints = 0;
	for (std::vector<std::string>::const_iterator i2 = s_allocatedExpertises.begin(); i2 != s_allocatedExpertises.end(); ++i2)
	{
		std::string const &expertiseName = *i2;
		if (!expertiseName.empty())
		{
			if (ExpertiseManager::getExpertiseTree(expertiseName) == tree)
			{
				spentPoints += 1;
			}
		}
	}
	return spentPoints;
}

//----------------------------------------------------------------------

/**
 * @return list of expertise tree id's possessed by the character
 */
ClientExpertiseManager::TreeIdList const & ClientExpertiseManager::getExpertiseTreesForPlayer()
{
	std::string const & skillTemplate = CuiSkillManager::getSkillTemplate();
	return ExpertiseManager::getExpertiseTreesForProfession(skillTemplate);
}

//----------------------------------------------------------------------

/**
 * @return bool - true if any expertise trees exist for the
 *         character's profession
 */
bool ClientExpertiseManager::hasExpertiseTrees()
{
	return !getExpertiseTreesForPlayer().empty();
}

//----------------------------------------------------------------------

/**
 * @param expertiseName - skill name of expertise
 * @return int - current rank possessed by character in given expertise
 */
int ClientExpertiseManager::getExpertiseRankForPlayer(std::string const & expertiseName, bool countAllocated)
{
	int result = 0;

	int rankMax = ExpertiseManager::getExpertiseRankMax(expertiseName);

	int tree = ExpertiseManager::getExpertiseTree(expertiseName);
	int tier = ExpertiseManager::getExpertiseTier(expertiseName);
	int grid = ExpertiseManager::getExpertiseGrid(expertiseName);
	int rank = 1;

	CreatureObject const * const player = Game::getPlayerCreature();
	if (player)
	{
		while (rank <= rankMax)
		{
			SkillObject const * skill = ExpertiseManager::getExpertiseSkillAt(tree, tier, grid, rank);
			if(!skill)
				break;
			if ( (countAllocated && playerHasExpertiseOrHasAllocated(skill->getSkillName())) ||
				 (!countAllocated && playerHasExpertise(skill->getSkillName())) )
			{
				result = rank;
				rank++;
			}
			else
			{
				break;
			}
		}
	}

	return result;
}

//----------------------------------------------------------------------

/**
 * @param treeId - int id of an expertise tree
 * @param localizedTreeName - ref to string to be populated with result
 * 
 * @return bool - false if localization failed or treeId not found
 */
bool ClientExpertiseManager::localizeExpertiseTreeNameFromId(int treeId, Unicode::String & localizedTreeName)
{
	std::string const & treeName = ExpertiseManager::getExpertiseTreeNameFromId(treeId);
	return StringId(cs_expertiseTreeStringTable, treeName).localize(localizedTreeName);
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::localizeExpertiseTreeDescriptionFromId(int treeId, Unicode::String & localizedTreeDescription)
{
	std::string const & treeName = ExpertiseManager::getExpertiseTreeNameFromId(treeId);
	return StringId(cs_expertiseDescriptionStringTable, treeName).localize(localizedTreeDescription);
}

//----------------------------------------------------------------------

/**
 * @param tree - tree id number
 * @param tier - tier number (aka "y coordinate")
 * @param grid - grid number (aka "x coordinate")
 * 
 * @return - bool flags struct indicating which tree components
 *         should be displayed at grid square
 */
ClientExpertiseManager::ExpertiseArrowFlags const ClientExpertiseManager::getArrowFlagsAt(int tree, int tier, int grid)
{
	ExpertiseManager::ExpertiseCoord expertiseCoord(tree, tier, grid);

	ExpertiseArrowGrid::const_iterator i = s_arrowGrid.find(expertiseCoord);
	if (i != s_arrowGrid.end())
	{
		return (*i).second;
	}

	return ClientExpertiseManager::EAF_None;
}

//----------------------------------------------------------------------

/**
 * @param tree - tree id number
 * @param tier - tier number (aka "y coordinate")
 * @param grid - grid number (aka "x coordinate")
 * 
 * @return - set of names of postrequisite skills that the grid square leads to
 */
ClientExpertiseManager::PostreqList const & ClientExpertiseManager::getPostreqListAt(int tree, int tier, int grid)
{
	ExpertiseManager::ExpertiseCoord expertiseCoord(tree, tier, grid);

	ExpertisePostreqGrid::const_iterator i = s_postreqGrid.find(expertiseCoord);
	if (i != s_postreqGrid.end())
	{
		return (*i).second;
	}

	return cs_emptyPostreqList;
}

//----------------------------------------------------------------------

/**
 * @param tree - tree id number
 * @param tier - tier number (aka "y coordinate")
 * @param grid - grid number (aka "x coordinate")
 * 
 * @return - skill the player is currently eligible for at that grid square (or NULL if none)
 */
SkillObject const * ClientExpertiseManager::getExpertisePlayerIsEligibleForAt(int tree, int tier, int grid)
{
	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
	{
		return 0;
	}

	SkillObject const * expertise = ExpertiseManager::getExpertiseSkillAt(tree, tier, grid);
	if (!expertise)
	{
		return 0;
	}

	std::string nextSkillName = getNextExpertiseNameGivenBaseExpertise(expertise->getSkillName());

	if(nextSkillName.empty())
		return 0;

	if(!canAllocateExpertise(nextSkillName))
		return 0;

	
	return SkillManager::getInstance().getSkill(nextSkillName);
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::localizeExpertiseName(std::string const &expertiseName, Unicode::String & localizedExpertiseName)
{
	return CuiSkillManager::localizeSkillName(expertiseName, localizedExpertiseName);
}


//----------------------------------------------------------------------

bool ClientExpertiseManager::localizeExpertiseDescription(std::string const &expertiseName, Unicode::String & localizedExpertiseDescription)
{
	return CuiSkillManager::localizeSkillDescription(expertiseName, localizedExpertiseDescription);
}

void ClientExpertiseManager::clearAllocatedExpertises()
{
	s_allocatedExpertises.clear();
}

//----------------------------------------------------------------------

void ClientExpertiseManager::clearAllocatedExpertisesInTree(int tree)
{
	std::vector<std::string> expertisesToDeallocate;
	for (std::vector<std::string>::const_iterator i = s_allocatedExpertises.begin(); i != s_allocatedExpertises.end(); ++i)
	{
		std::string const &expertiseName = *i;
		if (!expertiseName.empty())
		{
			if (ExpertiseManager::getExpertiseTree(expertiseName) == tree)
			{
				expertisesToDeallocate.push_back(expertiseName);
			}
		}
	}
	for(std::vector<std::string>::const_iterator i2 = expertisesToDeallocate.begin(); i2 != expertisesToDeallocate.end(); ++i2)
	{
		std::string const &expertiseName = *i2;
		//We don't have to check restrictions here because there are no cross-tree restrictions
		deallocateExpertise(expertiseName, false);
	}
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::hasAllocatedExpertise(std::string const & expertiseName)
{
	return find(s_allocatedExpertises.begin(), s_allocatedExpertises.end(), expertiseName) != s_allocatedExpertises.end();
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::allocateExpertise(std::string const & expertiseName, bool checkRestrictions)
{
	if(checkRestrictions && !canAllocateExpertise(expertiseName))
		return false;
	s_allocatedExpertises.push_back(expertiseName);
	return true;
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::deallocateExpertise(std::string const & expertiseName, bool checkRestrictions)
{
	if(checkRestrictions && !canDeallocateExpertise(expertiseName))
		return false;
	s_allocatedExpertises.erase(find(s_allocatedExpertises.begin(), s_allocatedExpertises.end(), expertiseName));
	return true;
}

//----------------------------------------------------------------------

int ClientExpertiseManager::getNumAllocatedExpertises()
{
	return s_allocatedExpertises.size();
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::playerHasExpertiseOrHasAllocated(std::string const & expertiseName)
{
	SkillObject const * skill = SkillManager::getInstance().getSkill(expertiseName);
	if(!skill)
		return false;
	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
		return false;
	return (player->hasSkill(*skill) || hasAllocatedExpertise(expertiseName));
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::playerHasExpertise(std::string const & expertiseName)
{
	SkillObject const * skill = SkillManager::getInstance().getSkill(expertiseName);
	if(!skill)
		return false;
	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
		return false;
	return player->hasSkill(*skill);
}

//----------------------------------------------------------------------

std::string ClientExpertiseManager::getNextExpertiseNameGivenBaseExpertise(std::string const & baseExpertiseName)
{
	int rankMax = ExpertiseManager::getExpertiseRankMax(baseExpertiseName);

	int tree = ExpertiseManager::getExpertiseTree(baseExpertiseName);
	int tier = ExpertiseManager::getExpertiseTier(baseExpertiseName);
	int grid = ExpertiseManager::getExpertiseGrid(baseExpertiseName);
	int rank = 1;

	CreatureObject const * const player = Game::getPlayerCreature();
	if (player)
	{
		while (rank <= rankMax)
		{
			SkillObject const * skill = ExpertiseManager::getExpertiseSkillAt(tree, tier, grid, rank);
			if (skill && playerHasExpertiseOrHasAllocated(skill->getSkillName()))
			{
				rank++;
			}
			else
			{
				return skill->getSkillName();
			}
		}
	}

	//Player has all the ranks of this skill
	return s_emptyString;
}

//----------------------------------------------------------------------

std::string ClientExpertiseManager::getTopExpertiseNameGivenBaseExpertise(std::string const & baseExpertiseName)
{
	int rankMax = ExpertiseManager::getExpertiseRankMax(baseExpertiseName);

	int tree = ExpertiseManager::getExpertiseTree(baseExpertiseName);
	int tier = ExpertiseManager::getExpertiseTier(baseExpertiseName);
	int grid = ExpertiseManager::getExpertiseGrid(baseExpertiseName);
	int rank = 1;

	CreatureObject const * const player = Game::getPlayerCreature();
	std::string topSkill;
	if (player)
	{
		while (rank <= rankMax)
		{
			SkillObject const * skill = ExpertiseManager::getExpertiseSkillAt(tree, tier, grid, rank);
			if (skill && playerHasExpertiseOrHasAllocated(skill->getSkillName()))
			{
				rank++;
				topSkill = skill->getSkillName();
			}
			else
			{
				return topSkill;
			}
		}
	}

	//Player has all the ranks of this skill
	return topSkill;
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::isExpertiseCommandType(std::string const & expertiseName)
{
	SkillObject const * skill = SkillManager::getInstance().getSkill(expertiseName);
	if(!skill)
		return false;
	const std::vector<std::string> & commands = skill->getCommandsProvided();
	return !commands.empty();
}

//----------------------------------------------------------------------

std::string ClientExpertiseManager::getExpertiseCommand(std::string const & expertiseName)
{
	SkillObject const * skill = SkillManager::getInstance().getSkill(expertiseName);
	if(!skill)
		return false;
	const std::vector<std::string> & commands = skill->getCommandsProvided();
	if(!commands.empty())
	{
		DEBUG_FATAL(commands.size() != 1, ("expertise %s has %d commands which is too many", expertiseName.c_str(), commands.size()));
		return commands[0];
	}
	else
		return s_emptyString;
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::isExpertiseSchematicType(std::string const & expertiseName)
{
	//@TODO
	UNREF(expertiseName);
	return false;
}

//----------------------------------------------------------------------

std::string ClientExpertiseManager::getExpertiseSchematic(std::string const & expertiseName)
{
	//@TODO
	UNREF(expertiseName);
	return s_emptyString;
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::isExpertiseSkillModType(std::string const & expertiseName)
{
	SkillObject const * skill = SkillManager::getInstance().getSkill(expertiseName);
	if(!skill)
		return false;
	SkillObject::GenericModVector const & mods = skill->getStatisticModifiers();
	return !mods.empty();
}

//----------------------------------------------------------------------

void ClientExpertiseManager::getExpertiseSkillMods(std::string const & baseExpertiseName, ExpertiseSkillModStruct & expertiseSkillMods)
{
	int rankMax = ExpertiseManager::getExpertiseRankMax(baseExpertiseName);

	int tree = ExpertiseManager::getExpertiseTree(baseExpertiseName);
	int tier = ExpertiseManager::getExpertiseTier(baseExpertiseName);
	int grid = ExpertiseManager::getExpertiseGrid(baseExpertiseName);
	int rank = 1;

	int c;
	for(c = 0; c < MAX_NUM_SKILL_MODS_PER_EXPERTISE * MAX_NUM_EXPERTISE_RANKS; c++)
		expertiseSkillMods.values[c] = SKILL_MOD_TAG_VALUE;

	if(rankMax > MAX_NUM_EXPERTISE_RANKS)
	{
		DEBUG_WARNING(true, ("ClientExpertiseManager WARNING max rank for %s returned %d > %d.  This is actually super bad.", baseExpertiseName.c_str(), 
			rankMax, MAX_NUM_EXPERTISE_RANKS));
		rankMax = MAX_NUM_EXPERTISE_RANKS;
	}

	unsigned int modCount = 0;
	while (rank <= rankMax)
	{
		SkillObject const * skill = ExpertiseManager::getExpertiseSkillAt(tree, tier, grid, rank);
		if(!skill)
		{
			DEBUG_WARNING(true, ("ClientExpertiseManager getExpertiseSkillMods asked for %d %d %d %d got null.  This is actually super bad.",
				tree, tier, grid, rank));
			continue;
		}
		SkillObject::GenericModVector const & mods = skill->getStatisticModifiers();
		if(rank == 1)
		{
			modCount = mods.size();
		}
		for(unsigned int i = 0; ((i < MAX_NUM_SKILL_MODS_PER_EXPERTISE) && (i < mods.size())); ++i)
		{
			DEBUG_WARNING(mods.size() > MAX_NUM_SKILL_MODS_PER_EXPERTISE, ("expertise %s had more than %d skill mods %d", 
				baseExpertiseName.c_str(), MAX_NUM_SKILL_MODS_PER_EXPERTISE, mods.size()));
			if(rank == 1)
			{
				expertiseSkillMods.names[i] = mods[i].first;   // Fill out names on the first rank
			}
			else
			{
				DEBUG_WARNING(modCount != mods.size(), ("expertise %s had different numbers of skill mods rank 1 = %d rank %d = %d",
					baseExpertiseName.c_str(), modCount, rank, mods.size()));
				DEBUG_WARNING(expertiseSkillMods.names[i] != mods[i].first, ("expertise %s had differently named skill mods rank 1 = %s rank %d = %s",
					baseExpertiseName.c_str(), expertiseSkillMods.names[i].c_str(), rank, mods[i].first.c_str()));
			}
			expertiseSkillMods.values[i * MAX_NUM_EXPERTISE_RANKS + rank - 1] = mods[i].second; 
		}
		++rank;
	}
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::canAllocateExpertise(std::string const & expertiseName)
{
	if(playerHasExpertiseOrHasAllocated(expertiseName))
		return false;

	SkillObject const * skill = SkillManager::getInstance().getSkill(expertiseName);
	if(!skill)
		return false;
	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
		return false;

	// you can allocate the expertise if you're in god mode
	if (PlayerObject::isAdmin()) {
		return true;
	}
	
	//Check if the player has all the prerequisites
	SkillObject::SkillVector const prereqs = skill->getPrerequisiteSkills();
	for (SkillObject::SkillVector::const_iterator i = prereqs.begin(); i != prereqs.end(); ++i)
	{
		SkillObject const * prereq = (*i);
		if (!prereq || !playerHasExpertiseOrHasAllocated(prereq->getSkillName()))
		{
			return 0;
		}
	}
	// Do you have a free point?
	int pointsRemaining = getExpertisePointsRemainingForPlayer();
	if (pointsRemaining < 1)
	{
		return false;
	}

	//Check if the player has enough points for a skill of this tier
	int tree = ExpertiseManager::getExpertiseTree(expertiseName);
	int pointsInTree = getExpertisePointsSpentForPlayerInTree(tree);
	int tier = ExpertiseManager::getExpertiseTier(expertiseName);
	if (pointsInTree < (tier - 1) * MAX_NUM_EXPERTISE_RANKS)
	{
		return false;
	}

	return true;
}

//----------------------------------------------------------------------

bool ClientExpertiseManager::canDeallocateExpertise(std::string const & expertiseName)
{
	SkillObject const * skill = SkillManager::getInstance().getSkill(expertiseName);
	if(!skill)
		return false;
	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
		return false;
	if(player->hasSkill(*skill))
		return false;
	if(!hasAllocatedExpertise(expertiseName))
		return false;

	// For use in calculating points
	int tree = ExpertiseManager::getExpertiseTree(expertiseName);
	int tier = ExpertiseManager::getExpertiseTier(expertiseName);

	//If player has any expertises allocated that depend on this one, return false
	for(std::vector<std::string>::iterator i = s_allocatedExpertises.begin(); i != s_allocatedExpertises.end(); ++i)
	{
		std::string const &s = *i;
		SkillObject const * potentialPostReq = SkillManager::getInstance().getSkill(s);
		if(potentialPostReq)
		{
			SkillObject::SkillVector const prereqs = potentialPostReq->getPrerequisiteSkills();
			for (SkillObject::SkillVector::const_iterator i = prereqs.begin(); i != prereqs.end(); ++i)
			{
				SkillObject const * prereq = (*i);
				if (prereq && (prereq->getSkillName() == expertiseName))
				{
					return false;
				}
			}
		}

		//If player has any expertises allocated that depend on having a certain number of
		//points, and deallocating this expertise would make the player go below, then return
		//false
		int subTree = ExpertiseManager::getExpertiseTree(potentialPostReq->getSkillName());
		int subTier = ExpertiseManager::getExpertiseTier(potentialPostReq->getSkillName());
		if((subTree == tree) &&(subTier > tier))
		{
			int pointsUpToTierAfter = ClientExpertiseManager::getExpertisePointsSpentForPlayerInTreeUpToTier(tree, subTier - 1) - 1;
			int pointsRequired = (subTier - 1) * MAX_NUM_EXPERTISE_RANKS;
			if(pointsRequired > pointsUpToTierAfter)
			{
				return false;
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------

void ClientExpertiseManager::sendAllocatedExpertiseListAndClear()
{
	ExpertiseRequestMessage erm;
	erm.setAddExpertisesList(s_allocatedExpertises);
	erm.setClearAllExpertisesFirst(false);
	GameNetwork::send(erm, true);
	s_allocatedExpertises.clear();
}

//----------------------------------------------------------------------

namespace
{
	static char const cs_opaquePrefix[] = "SWG";

	static uint8_t const cs_opaqueWireRaw = 1;
	static uint8_t const cs_opaqueWireZlib = 2;

	static bool zlibUncompressPayload(uint8_t const * compressed, size_t compressedLen, std::vector<uint8_t> & out, std::string & err)
	{
		if (!compressed || compressedLen == 0)
		{
			err = "Invalid expertise code (empty compressed block).";
			return false;
		}
		for (size_t bufLen = std::max(compressedLen * 4, size_t(4096)); bufLen <= 1024u * 1024u;
		     bufLen = (bufLen < 256u * 1024u) ? bufLen * 2u : bufLen + bufLen / 2u)
		{
			out.resize(bufLen);
			uLongf destLen = static_cast<uLongf>(bufLen);
			int const zr = uncompress(out.data(), &destLen, compressed, static_cast<uLong>(compressedLen));
			if (zr == Z_OK)
			{
				out.resize(static_cast<size_t>(destLen));
				return true;
			}
			if (zr != Z_BUF_ERROR && zr != Z_MEM_ERROR)
				break;
		}
		err = "Invalid expertise code (zlib decompress failed).";
		return false;
	}

	static int base64UrlDecodeChar(unsigned char c)
	{
		if (c >= 'A' && c <= 'Z')
			return static_cast<int>(c - 'A');
		if (c >= 'a' && c <= 'z')
			return static_cast<int>(c - 'a' + 26);
		if (c >= '0' && c <= '9')
			return static_cast<int>(c - '0' + 52);
		if (c == '-')
			return 62;
		if (c == '_')
			return 63;
		return -1;
	}

	static void appendBase64UrlEncoded(std::vector<uint8_t> const & data, std::string & out)
	{
		static char const digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
		size_t const len = data.size();
		for (size_t i = 0; i < len; i += 3)
		{
			uint32_t const b = (static_cast<uint32_t>(data[i]) << 16) |
				(i + 1 < len ? static_cast<uint32_t>(data[i + 1]) << 8 : 0) |
				(i + 2 < len ? static_cast<uint32_t>(data[i + 2]) : 0);
			out += digits[(b >> 18) & 63];
			out += digits[(b >> 12) & 63];
			if (i + 1 < len)
				out += digits[(b >> 6) & 63];
			if (i + 2 < len)
				out += digits[b & 63];
		}
	}

	static bool base64UrlDecode(std::string const & in, std::vector<uint8_t> & out)
	{
		std::string s;
		s.reserve(in.size());
		for (size_t k = 0; k < in.size(); ++k)
		{
			unsigned char const c = static_cast<unsigned char>(in[k]);
			if (std::isspace(c))
				continue;
			s += static_cast<char>(c);
		}
		if (s.empty())
			return false;
		switch (s.size() % 4)
		{
		case 2:
			s += "==";
			break;
		case 3:
			s += "=";
			break;
		case 1:
			return false;
		default:
			break;
		}
		out.clear();
		out.reserve(s.size() * 3 / 4);
		for (size_t k = 0; k < s.size(); k += 4)
		{
			if (k + 4 > s.size())
				return false;
			int const v0 = base64UrlDecodeChar(static_cast<unsigned char>(s[k]));
			int const v1 = base64UrlDecodeChar(static_cast<unsigned char>(s[k + 1]));
			unsigned char const c2 = static_cast<unsigned char>(s[k + 2]);
			unsigned char const c3 = static_cast<unsigned char>(s[k + 3]);
			if (v0 < 0 || v1 < 0)
				return false;
			if (c2 == '=' && c3 == '=')
			{
				uint32_t const triple = (static_cast<uint32_t>(v0) << 18) | (static_cast<uint32_t>(v1) << 12);
				out.push_back(static_cast<uint8_t>((triple >> 16) & 0xFF));
			}
			else if (c3 == '=')
			{
				int const v2 = base64UrlDecodeChar(c2);
				if (v2 < 0)
					return false;
				uint32_t const triple =
					(static_cast<uint32_t>(v0) << 18) | (static_cast<uint32_t>(v1) << 12) | (static_cast<uint32_t>(v2) << 6);
				out.push_back(static_cast<uint8_t>((triple >> 16) & 0xFF));
				out.push_back(static_cast<uint8_t>((triple >> 8) & 0xFF));
			}
			else
			{
				int const v2 = base64UrlDecodeChar(c2);
				int const v3 = base64UrlDecodeChar(c3);
				if (v2 < 0 || v3 < 0)
					return false;
				uint32_t const triple = (static_cast<uint32_t>(v0) << 18) | (static_cast<uint32_t>(v1) << 12) |
					(static_cast<uint32_t>(v2) << 6) | static_cast<uint32_t>(v3);
				out.push_back(static_cast<uint8_t>((triple >> 16) & 0xFF));
				out.push_back(static_cast<uint8_t>((triple >> 8) & 0xFF));
				out.push_back(static_cast<uint8_t>(triple & 0xFF));
			}
		}
		return true;
	}

	static void appendLe16(std::vector<uint8_t> & blob, uint16_t v)
	{
		blob.push_back(static_cast<uint8_t>(v & 0xFF));
		blob.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
	}

	static bool readLe16(std::vector<uint8_t> const & blob, size_t & o, uint16_t & v)
	{
		if (o + 2 > blob.size())
			return false;
		v = static_cast<uint16_t>(blob[o] | (static_cast<uint16_t>(blob[o + 1]) << 8));
		o += 2;
		return true;
	}

	static bool parseOpaqueExpertiseCode(std::string raw, std::string & templateOut, std::vector<std::string> & skillsOut, std::string & err)
	{
		while (!raw.empty() && (raw[0] == ' ' || raw[0] == '\t' || raw[0] == '\r' || raw[0] == '\n'))
			raw.erase(0, 1);
		while (!raw.empty() && (raw[raw.size() - 1] == ' ' || raw[raw.size() - 1] == '\t' || raw[raw.size() - 1] == '\r' ||
		                        raw[raw.size() - 1] == '\n'))
			raw.erase(raw.size() - 1, 1);
		if (raw.size() < 5)
		{
			err.clear();
			return false;
		}
		for (size_t p = 0; p < 3; ++p)
			if (p < raw.size())
				raw[p] = static_cast<char>(std::toupper(static_cast<unsigned char>(raw[p])));
		if (raw.compare(0, 3, cs_opaquePrefix) != 0)
		{
			err.clear();
			return false;
		}
		char const ver = raw[3];
		if (!std::isdigit(static_cast<unsigned char>(ver)))
		{
			err.clear();
			return false;
		}
		if (ver != '1')
		{
			err = "Unsupported expertise code version.";
			return false;
		}
		std::string const b64 = raw.substr(4);
		std::vector<uint8_t> bin;
		if (!base64UrlDecode(b64, bin) || bin.empty())
		{
			err = "Invalid expertise code (corrupt encoding).";
			return false;
		}
		if (bin[0] == cs_opaqueWireZlib)
		{
			if (bin.size() < 2)
			{
				err = "Invalid expertise code payload.";
				return false;
			}
			std::vector<uint8_t> decomp;
			if (!zlibUncompressPayload(bin.data() + 1, bin.size() - 1, decomp, err))
				return false;
			bin.swap(decomp);
		}
		else if (bin[0] != cs_opaqueWireRaw)
		{
			err = "Invalid expertise code payload.";
			return false;
		}
		size_t o = 0;
		if (o >= bin.size() || bin[o] != 1)
		{
			err = "Invalid expertise code payload.";
			return false;
		}
		++o;
		uint16_t tlen = 0;
		if (!readLe16(bin, o, tlen))
		{
			err = "Invalid expertise code (truncated).";
			return false;
		}
		if (o + static_cast<size_t>(tlen) > bin.size())
		{
			err = "Invalid expertise code (truncated).";
			return false;
		}
		templateOut.assign(reinterpret_cast<char const *>(&bin[o]), tlen);
		o += tlen;
		uint16_t nskills = 0;
		if (!readLe16(bin, o, nskills))
		{
			err = "Invalid expertise code (truncated).";
			return false;
		}
		skillsOut.clear();
		for (uint16_t i = 0; i < nskills; ++i)
		{
			uint16_t slen = 0;
			if (!readLe16(bin, o, slen))
			{
				err = "Invalid expertise code (truncated).";
				return false;
			}
			if (o + static_cast<size_t>(slen) > bin.size())
			{
				err = "Invalid expertise code (truncated).";
				return false;
			}
			std::string sk(reinterpret_cast<char const *>(&bin[o]), slen);
			o += slen;
			skillsOut.push_back(sk);
		}
		if (o != bin.size())
		{
			err = "Invalid expertise code (extra data).";
			return false;
		}
		err.clear();
		return true;
	}

	void splitByChar(std::string const &s, char delim, std::vector<std::string> &out)
	{
		out.clear();
		std::string::size_type a = 0;
		while (a <= s.size())
		{
			std::string::size_type b = s.find(delim, a);
			if (b == std::string::npos)
			{
				out.push_back(s.substr(a));
				break;
			}
			out.push_back(s.substr(a, b - a));
			a = b + 1;
		}
	}

	bool parseExpertiseBuildImportText(std::string raw, std::string & templateOut, std::vector<std::string> & skillsOut, std::string & err)
	{
		while (!raw.empty() && (raw[0] == ' ' || raw[0] == '\t' || raw[0] == '\r' || raw[0] == '\n'))
			raw.erase(0, 1);
		while (!raw.empty() && (raw[raw.size() - 1] == ' ' || raw[raw.size() - 1] == '\t' || raw[raw.size() - 1] == '\r' ||
		                        raw[raw.size() - 1] == '\n'))
			raw.erase(raw.size() - 1, 1);
		if (raw.empty())
		{
			err = "Empty build data.";
			return false;
		}

		std::string opaqueErr;
		if (parseOpaqueExpertiseCode(raw, templateOut, skillsOut, opaqueErr))
			return true;
		if (!opaqueErr.empty())
		{
			err = opaqueErr;
			return false;
		}

		bool const usePipe =
			(raw.find('\n') == std::string::npos && raw.find('\r') == std::string::npos && raw.find('|') != std::string::npos);

		if (usePipe)
		{
			std::vector<std::string> parts;
			splitByChar(raw, '|', parts);
			if (parts.size() < 3)
			{
				err = "Invalid compact build code (expected SWGEXP1|<profession>|<skill>|...).";
				return false;
			}
			ClientExpertiseManagerNamespace::trimBuildToken(parts[0]);
			if (parts[0] != ClientExpertiseManagerNamespace::cs_buildMagic)
			{
				err = "Unknown build format (must start with SWGEXP1).";
				return false;
			}
			templateOut = parts[1];
			ClientExpertiseManagerNamespace::trimBuildToken(templateOut);
			skillsOut.clear();
			for (size_t i = 2; i < parts.size(); ++i)
			{
				std::string sk = parts[i];
				ClientExpertiseManagerNamespace::trimBuildToken(sk);
				if (!sk.empty())
					skillsOut.push_back(sk);
			}
			return true;
		}

		std::vector<std::string> lines;
		std::string::size_type pos = 0;
		while (pos < raw.size())
		{
			std::string::size_type const end = raw.find_first_of("\r\n", pos);
			std::string line = (end == std::string::npos) ? raw.substr(pos) : raw.substr(pos, end - pos);
			if (end == std::string::npos)
				pos = raw.size();
			else
			{
				pos = end + 1;
				if (end < raw.size() && raw[end] == '\r' && pos < raw.size() && raw[pos] == '\n')
					++pos;
			}
			ClientExpertiseManagerNamespace::trimBuildToken(line);
			if (!line.empty() && line[0] != '#')
				lines.push_back(line);
		}
		if (lines.size() < 2)
		{
			err = "Invalid build text (need SWGEXP1 header and profession line).";
			return false;
		}
		if (lines[0] != ClientExpertiseManagerNamespace::cs_buildMagic)
		{
			err = "Unknown build format (first line must be SWGEXP1).";
			return false;
		}
		templateOut = lines[1];
		skillsOut.assign(lines.begin() + 2, lines.end());
		return true;
	}

	std::string buildOpaqueExpertiseShareCode(std::string const & tpl, std::vector<std::string> const & ordered)
	{
		std::vector<uint8_t> blob;
		blob.push_back(1);
		appendLe16(blob, static_cast<uint16_t>(tpl.size()));
		blob.insert(blob.end(), tpl.begin(), tpl.end());
		appendLe16(blob, static_cast<uint16_t>(ordered.size()));
		for (size_t i = 0; i < ordered.size(); ++i)
		{
			std::string const & sk = ordered[i];
			appendLe16(blob, static_cast<uint16_t>(sk.size()));
			blob.insert(blob.end(), sk.begin(), sk.end());
		}
		uLongf compBound = compressBound(static_cast<uLong>(blob.size()));
		std::vector<uint8_t> zlibBuf(compBound);
		int const zr = compress2(zlibBuf.data(), &compBound, blob.data(), static_cast<uLong>(blob.size()), Z_DEFAULT_COMPRESSION);
		std::vector<uint8_t> wire;
		if (zr == Z_OK)
		{
			zlibBuf.resize(static_cast<size_t>(compBound));
			wire.reserve(1 + zlibBuf.size());
			wire.push_back(cs_opaqueWireZlib);
			wire.insert(wire.end(), zlibBuf.begin(), zlibBuf.end());
		}
		else
			wire.swap(blob);
		std::string result = std::string(cs_opaquePrefix) + '1';
		appendBase64UrlEncoded(wire, result);
		return result;
	}
}

std::string ClientExpertiseManager::exportExpertiseBuildCompactLine()
{
	std::set<std::string> names;
	ClientExpertiseManagerNamespace::collectFullExpertiseBuildSkillNames(names);
	std::vector<std::string> ordered(names.begin(), names.end());
	std::sort(ordered.begin(), ordered.end(), ClientExpertiseManagerNamespace::compareExpertiseSkillNames);
	return buildOpaqueExpertiseShareCode(CuiSkillManager::getSkillTemplate(), ordered);
}

std::string ClientExpertiseManager::exportExpertiseBuildMultiline()
{
	std::set<std::string> names;
	ClientExpertiseManagerNamespace::collectFullExpertiseBuildSkillNames(names);
	std::vector<std::string> ordered(names.begin(), names.end());
	std::sort(ordered.begin(), ordered.end(), ClientExpertiseManagerNamespace::compareExpertiseSkillNames);
	std::ostringstream os;
	os << ClientExpertiseManagerNamespace::cs_buildMagic << '\n';
	os << CuiSkillManager::getSkillTemplate() << '\n';
	for (size_t i = 0; i < ordered.size(); ++i)
		os << ordered[i] << '\n';
	return os.str();
}

bool ClientExpertiseManager::importExpertiseBuildFromText(std::string const & rawText, std::string & resultMessage)
{
	std::string tpl;
	std::vector<std::string> skills;
	std::string err;
	if (!parseExpertiseBuildImportText(rawText, tpl, skills, err))
	{
		resultMessage = err;
		return false;
	}
	return ClientExpertiseManagerNamespace::parseBuildBody(tpl, skills, resultMessage);
}

//----------------------------------------------------------------------

std::string ClientExpertiseManager::getPreviousExpertise(std::string const & baseExpertiseName)
{
	SkillObject const * skill = SkillManager::getInstance().getSkill(baseExpertiseName);
	if(!skill)
		return s_emptyString;
	SkillObject::SkillVector const prereqs = skill->getPrerequisiteSkills();
	for (SkillObject::SkillVector::const_iterator i = prereqs.begin(); i != prereqs.end(); ++i)
	{
		SkillObject const * prereq = (*i);
		if (prereq && ExpertiseManager::isExpertise(prereq))
		{
			return prereq->getSkillName();
		}
	}
	return s_emptyString;
}

//----------------------------------------------------------------------

std::string ClientExpertiseManager::getBaseExpertiseNameForExpertise(std::string const & expertiseName)
{
	if(expertiseName.empty())
		return s_emptyString;
	SkillObject const * skill = ExpertiseManager::getExpertiseSkillAt(ExpertiseManager::getExpertiseTree(expertiseName), 
		ExpertiseManager::getExpertiseTier(expertiseName), ExpertiseManager::getExpertiseGrid(expertiseName), 1);
	if(skill)
		return skill->getSkillName();
	return s_emptyString;
}


//======================================================================
