// ======================================================================
//
// StaffRankDataTable.cpp
//
// ======================================================================

#include "sharedGame/FirstSharedGame.h"
#include "sharedGame/StaffRankDataTable.h"

#include "sharedFoundation/ExitChain.h"
#include "sharedUtility/DataTable.h"
#include "sharedUtility/DataTableManager.h"

#include <algorithm>
#include <vector>

// ======================================================================

namespace StaffRankDataTableNamespace
{
	char const * const cs_staffRankDataTableName = "datatables/admin/staff_ranks.iff";

	std::vector<std::string> s_rankTitles;
	int s_maxLevel = 0;
}

using namespace StaffRankDataTableNamespace;

// ======================================================================

void StaffRankDataTable::install()
{
	DataTable * table = DataTableManager::getTable(cs_staffRankDataTableName, true);
	FATAL(!table, ("staff rank datatable %s not found", cs_staffRankDataTableName));

	int const columnLevel = table->findColumnNumber("level");
	int const columnTitle = table->findColumnNumber("rankTitle");
	FATAL(columnLevel < 0, ("column \"level\" not found in %s", cs_staffRankDataTableName));
	FATAL(columnTitle < 0, ("column \"rankTitle\" not found in %s", cs_staffRankDataTableName));

	int const numRows = table->getNumRows();
	int maxSeen = 0;
	for (int i = 0; i < numRows; ++i)
	{
		int const lvl = table->getIntValue(columnLevel, i);
		std::string const title = table->getStringValue(columnTitle, i);
		if (lvl > maxSeen)
			maxSeen = lvl;
	}

	FATAL(maxSeen < 0, ("%s: no rank rows", cs_staffRankDataTableName));

	s_rankTitles.resize(static_cast<size_t>(maxSeen + 1), "PLAYER");
	s_maxLevel = maxSeen;

	for (int i = 0; i < numRows; ++i)
	{
		int const lvl = table->getIntValue(columnLevel, i);
		std::string const title = table->getStringValue(columnTitle, i);
		if (lvl >= 0 && lvl <= maxSeen)
			s_rankTitles[static_cast<size_t>(lvl)] = title;
	}

	DataTableManager::close(cs_staffRankDataTableName);

	ExitChain::add(remove, "StaffRankDataTable::remove");
}

// ----------------------------------------------------------------------

void StaffRankDataTable::remove()
{
	s_rankTitles.clear();
	s_maxLevel = 0;
}

// ----------------------------------------------------------------------

std::string StaffRankDataTable::getRankTitle(int adminLevel)
{
	if (s_rankTitles.empty())
		return "PLAYER";

	int const clamped = std::max(0, std::min(adminLevel, s_maxLevel));
	return s_rankTitles[static_cast<size_t>(clamped)];
}

// ----------------------------------------------------------------------

int StaffRankDataTable::getMaxDefinedLevel()
{
	return s_maxLevel;
}

// ======================================================================
