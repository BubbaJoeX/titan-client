// ======================================================================
//
// DevTestTools.cpp
//
// See DevTestTools.h.
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/DevTestTools.h"

#include "clientGame/ContainerInterface.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/FreeCamera.h"
#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientUserInterface/CuiCombatManager.h"
#include "clientUserInterface/CuiConsoleHelper.h"
#include "sharedFoundation/Clock.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/Object.h"
#include "sharedTerrain/TerrainObject.h"
#include "swgSharedUtility/States.def"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <map>

// ======================================================================

#if PRODUCTION == 0

namespace DevTestToolsNamespace
{
	char const * const cs_templatesFile  = "devtest_templates.txt";
	char const * const cs_animationsFile = "devtest_animations.txt";
	float const        cs_degrees        = 57.29578f;

	struct Telemetry
	{
		bool          printing;
		bool          valid;
		NetworkId     moverId;        // the speed restarts when the mover changes (mount, dismount)
		Vector        last;
		unsigned long lastMs;
		unsigned long lastPrintMs;
		float         speed;          // m/s, smoothed
	};
	Telemetry s_telemetry = { false, false, NetworkId::cms_invalid, Vector::zero, 0, 0, 0.0f };
	float const cs_teleportSpeed = 1000.0f; // m/s; a faster step is a warp or hop, not movement

	typedef DevTestTools::StringVector StringVector;
	typedef DevTestTools::Items        Items;

	std::string format(char const * fmt, ...)
	{
		char buf[512];
		va_list args;
		va_start(args, fmt);
		IGNORE_RETURN(_vsnprintf(buf, sizeof(buf) - 1, fmt, args));
		va_end(args);
		buf[sizeof(buf) - 1] = '\0';
		return buf;
	}

	std::string lower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), tolower);
		return s;
	}

	bool loadList(char const * fileName, StringVector & lines)
	{
		lines.clear();
		FILE * const f = fopen(fileName, "rt");
		if (!f)
			return false;
		char buf[512];
		while (fgets(buf, sizeof(buf), f))
		{
			std::string line(buf);
			while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r' || line[line.size() - 1] == ' '))
				line.erase(line.size() - 1);
			if (!line.empty() && line[0] != '#')
				lines.push_back(line);
		}
		fclose(f);
		return true;
	}

	// the lists are read once; a client restart picks up a regenerated file
	StringVector const & getTemplates()
	{
		static StringVector s_templates;
		static bool s_loaded = false;
		if (!s_loaded)
			s_loaded = loadList(cs_templatesFile, s_templates);
		return s_templates;
	}

	StringVector const & getAnimations()
	{
		static StringVector s_animations;
		static bool s_loaded = false;
		if (!s_loaded)
			s_loaded = loadList(cs_animationsFile, s_animations);
		return s_animations;
	}

	// all_b_emt_wave1.ans -> all_b (the shared humanoid set); bantha_emt_stand_happy.ans -> bantha
	std::string speciesOf(std::string const & animation)
	{
		std::string::size_type p = animation.find('_');
		if (p == std::string::npos)
			return animation;
		if (animation.compare(0, p, "all") == 0)
		{
			std::string::size_type const q = animation.find('_', p + 1);
			if (q != std::string::npos)
				p = q;
		}
		return animation.substr(0, p);
	}

	// ------------------------------------------------------------------

	Object * getMover()
	{
		CreatureObject * const player = Game::getPlayerCreature();
		if (!player)
			return 0;
		Object * const container = ContainerInterface::getContainedByObject(*player);
		if (container && !container->getCellProperty())
			return container;
		return player;
	}

	bool terrainHeight(Vector const & position, float & height)
	{
		TerrainObject const * const terrain = TerrainObject::getConstInstance();
		return terrain && terrain->getHeight(position, height);
	}

	void print(std::string const & text)
	{
		CuiConsoleHelper * const helper = CuiConsoleHelper::getActiveConsoleHelper();
		if (helper)
			IGNORE_RETURN(helper->appendOutputText(Unicode::narrowToWide(text)));
	}

	std::string telemetryLine()
	{
		Object const * const mover = getMover();
		if (!mover)
			return "telemetry: no player\n";
		Vector const p = mover->getPosition_w();
		float ground = 0.0f;
		bool const hasGround = terrainHeight(p, ground);
		Vector const k = mover->getObjectFrameK_w();
		return format("%s: %.1f m/s (%.0f km/h), altitude %.1f m, above ground %s, heading %.0f\n",
			mover == Game::getPlayer() ? "on foot" : (mover->getObjectTemplateName() ? mover->getObjectTemplateName() : "?"),
			s_telemetry.speed, s_telemetry.speed * 3.6f, p.y, hasGround ? format("%.1f m", p.y - ground).c_str() : "?",
			atan2(k.x, k.z) * cs_degrees);
	}

	// ------------------------------------------------------------------
	// /devtest camera free <x> <y> <z> [<lx> <ly> <lz>]   free camera at a world point looking at another (default: the
	//                                                     player); for aerial screenshots
	// /devtest camera orbit <distance> <pitch> [yaw] [last] around the player (or the last spawn), pitch down in degrees
	// /devtest camera chase                               back to the normal chase camera
	// /devtest camera info                                where the camera is

	bool camera(StringVector const & argv, std::string & result)
	{
		GroundScene * const scene = dynamic_cast<GroundScene *>(Game::getScene());
		if (!scene)
		{
			result += "camera: no ground scene\n";
			return true;
		}
		std::string const mode = argv.size() > 1 ? argv[1] : "info";
		if (mode == "chase")
		{
			scene->setView(GroundScene::CI_freeChase);
			result += "camera: chase\n";
			return true;
		}
		if (mode == "info")
		{
			Object const * const camera = Game::getConstCamera();
			if (camera)
			{
				Vector const p = camera->getPosition_w();
				Vector const k = camera->getObjectFrameK_w();
				result += format("camera view %d at %.1f %.1f %.1f, yaw %.1f, pitch %.1f\n", scene->getCurrentView(), p.x, p.y, p.z,
					atan2(k.x, k.z) * cs_degrees, atan2(-k.y, sqrt(k.x * k.x + k.z * k.z)) * cs_degrees);
			}
			return true;
		}

		FreeCamera * const freeCamera = dynamic_cast<FreeCamera *>(scene->getCamera(GroundScene::CI_free));
		if (!freeCamera || (mode != "free" && mode != "orbit"))
			return false;

		FreeCamera::Info info;
		if (mode == "free")
		{
			if (argv.size() < 5)
				return false;
			info.translate = Vector(static_cast<float>(atof(argv[2].c_str())), static_cast<float>(atof(argv[3].c_str())), static_cast<float>(atof(argv[4].c_str())));
			Vector look;
			if (argv.size() >= 8)
				look = Vector(static_cast<float>(atof(argv[5].c_str())), static_cast<float>(atof(argv[6].c_str())), static_cast<float>(atof(argv[7].c_str())));
			else
			{
				Object const * const mover = getMover();
				look = mover ? mover->getPosition_w() + Vector(0.0f, 1.5f, 0.0f) : Vector::zero;
			}
			Vector const d = look - info.translate;
			info.yaw = atan2(d.x, d.z);
			info.pitch = atan2(-d.y, sqrt(d.x * d.x + d.z * d.z));
			info.distance = 0.0f;
		}
		else
		{
			if (argv.size() < 4)
				return false;
			Object const * centre = getMover();
			if (argv.size() > 5 && argv[5] == "last")
				centre = CuiCombatManager::getLookAtTarget().getObject();
			if (!centre)
			{
				result += "camera: nothing to orbit\n";
				return true;
			}
			Vector const k = centre->getObjectFrameK_w();
			info.translate = centre->getPosition_w() + Vector(0.0f, 1.5f, 0.0f);
			info.distance = static_cast<float>(atof(argv[2].c_str()));
			info.pitch = static_cast<float>(atof(argv[3].c_str())) / cs_degrees;
			info.yaw = argv.size() > 4 ? static_cast<float>(atof(argv[4].c_str())) / cs_degrees : atan2(k.x, k.z);
		}

		scene->setView(GroundScene::CI_free);
		freeCamera->setInterpolating(false);
		freeCamera->setInitializeFromFreeCamera(true);          // keep this info, don't re-frame on the player
		freeCamera->alignPivotWithCell(CellProperty::getWorldCellProperty());
		freeCamera->setMode(mode == "orbit" ? FreeCamera::M_pivot : FreeCamera::M_fly);
		freeCamera->setInfo(info);
		result += format("camera %s at %.1f %.1f %.1f, yaw %.1f, pitch %.1f%s\n", mode.c_str(), info.translate.x, info.translate.y, info.translate.z,
			info.yaw * cs_degrees, info.pitch * cs_degrees, mode == "orbit" ? format(", distance %.1f", info.distance).c_str() : "");
		return true;
	}

	// ------------------------------------------------------------------

	void addPaging(Items & items, std::string const & command, int page, int total)
	{
		int const pages = (total + DevTestTools::cs_pageSize - 1) / DevTestTools::cs_pageSize;
		if (page > 0)
		{
			DevTestTools::Item item;
			item.label = format("<< page %d of %d", page, pages);
			item.command = format("%s %d", command.c_str(), page - 1);
			items.push_back(item);
		}
		if (page + 1 < pages)
		{
			DevTestTools::Item item;
			item.label = format(">> page %d of %d", page + 2, pages);
			item.command = format("%s %d", command.c_str(), page + 1);
			items.push_back(item);
		}
	}
}

using namespace DevTestToolsNamespace;

#endif

// ======================================================================

bool DevTestTools::performVerb(StringVector const & argv, std::string & result)
{
#if PRODUCTION == 0
	if (argv.empty())
		return false;
	if (argv[0] == "camera")
	{
		if (!camera(argv, result))
			result += "camera free <x> <y> <z> [<lx> <ly> <lz>] | orbit <distance> <pitch> [yaw] [last] | chase | info\n";
		return true;
	}
	if (argv[0] == "telemetry")
	{
		std::string const what = argv.size() > 1 ? argv[1] : "once";
		if (what == "on" || what == "off")
		{
			s_telemetry.printing = what == "on";
			result += std::string("telemetry ") + what + "\n";
		}
		else
			result += telemetryLine();
		return true;
	}
	return false;
#else
	UNREF(argv);
	UNREF(result);
	return false;
#endif
}

// ----------------------------------------------------------------------

void DevTestTools::update()
{
#if PRODUCTION == 0
	Object const * const mover = getMover();
	unsigned long const now = Clock::timeMs();
	if (!mover)
	{
		s_telemetry.valid = false;
		return;
	}
	Vector const p = mover->getPosition_w();
	if (s_telemetry.valid && mover->getNetworkId() != s_telemetry.moverId)
	{
		s_telemetry.valid = false;
		s_telemetry.speed = 0.0f;
	}
	if (s_telemetry.valid && now > s_telemetry.lastMs)
	{
		float const instant = p.magnitudeBetween(s_telemetry.last) * 1000.0f / static_cast<float>(now - s_telemetry.lastMs);
		if (instant < cs_teleportSpeed)
			s_telemetry.speed += (instant - s_telemetry.speed) * 0.2f;
	}
	s_telemetry.valid = true;
	s_telemetry.moverId = mover->getNetworkId();
	s_telemetry.last = p;
	s_telemetry.lastMs = now;
	if (s_telemetry.printing && now - s_telemetry.lastPrintMs >= 1000)
	{
		s_telemetry.lastPrintMs = now;
		print(telemetryLine());
	}
#endif
}

// ----------------------------------------------------------------------

std::string DevTestTools::describeMover()
{
#if PRODUCTION == 0
	CreatureObject const * const player = Game::getPlayerCreature();
	Object const * const mover = getMover();
	if (!player || !mover)
		return "null";
	Vector const p = mover->getPosition_w();
	float ground = 0.0f;
	bool const hasGround = terrainHeight(p, ground);
	Vector const k = mover->getObjectFrameK_w();
	std::string const templateName = mover->getObjectTemplateName() ? mover->getObjectTemplateName() : "";
	return format("{\"template\":\"%s\",\"riding\":%s,\"piloting\":%s,\"speed\":%.2f,\"altitude\":%.2f,\"above_ground\":%s,\"heading\":%.1f}",
		templateName.c_str(), player->getState(States::RidingMount) ? "true" : "false", player->getState(States::PilotingShip) ? "true" : "false",
		s_telemetry.speed, p.y, hasGround ? format("%.2f", p.y - ground).c_str() : "null", atan2(k.x, k.z) * cs_degrees);
#else
	return "null";
#endif
}

// ----------------------------------------------------------------------

int DevTestTools::pickTemplates(StringVector const & filterWords, int page, Items & items, std::string & title)
{
	items.clear();
#if PRODUCTION == 0
	StringVector const & templates = getTemplates();
	if (templates.empty())
	{
		title = std::string("no ") + cs_templatesFile + " (run SwgMayaMCP examples/make_devtest_lists.py)";
		return 0;
	}
	StringVector words;
	std::string joined;
	for (StringVector::const_iterator w = filterWords.begin(); w != filterWords.end(); ++w)
	{
		words.push_back(lower(*w));
		joined += (joined.empty() ? "" : " ") + *w;
	}
	int total = 0;
	int const first = page * cs_pageSize;
	for (StringVector::const_iterator t = templates.begin(); t != templates.end(); ++t)
	{
		std::string const name = lower(*t);
		bool match = true;
		for (StringVector::const_iterator w = words.begin(); w != words.end() && match; ++w)
			match = name.find(*w) != std::string::npos;
		if (!match)
			continue;
		if (total >= first && total < first + cs_pageSize)
		{
			// buildings well clear of the player, ships and vehicles beside them, everything else close
			char const * placement = " 4";
			if (name.find("object/building/") == 0)
				placement = " 40";
			else if (name.find("object/ship/") == 0)
				placement = " 20";
			else if (name.find("object/mobile/") == 0)
				placement = " 8";
			Item item;
			std::string::size_type const slash = t->find_last_of('/');
			item.label = t->substr(slash == std::string::npos ? 0 : slash + 1);
			if (item.label.compare(0, 7, "shared_") == 0)
				item.label.erase(0, 7);
			item.command = "/devtest spawn " + *t + placement;
			items.push_back(item);
		}
		++total;
	}
	addPaging(items, "/devtest pick " + (joined.empty() ? std::string("object/") : joined), page, total);
	title = format("Spawn '%s': %d templates", joined.c_str(), total);
	return total;
#else
	UNREF(filterWords);
	UNREF(page);
	UNREF(title);
	return 0;
#endif
}

// ----------------------------------------------------------------------

int DevTestTools::browseAnimations(std::string const & prefixOrTarget, int page, bool loop, Items & items, std::string & title)
{
	items.clear();
#if PRODUCTION == 0
	StringVector const & animations = getAnimations();
	if (animations.empty())
	{
		title = std::string("no ") + cs_animationsFile + " (run SwgMayaMCP examples/make_devtest_lists.py)";
		return 0;
	}
	std::string prefix = lower(prefixOrTarget);
	if (prefix.empty() || prefix == "target")
	{
		// the species of the look-at target (or the player) from its appearance: appearance/bantha.sat -> bantha;
		// the humanoid player species share the all_b set
		Object const * object = CuiCombatManager::getLookAtTarget().getObject();
		if (!object)
			object = Game::getPlayer();
		std::string name = object && object->getAppearance() && object->getAppearance()->getAppearanceTemplateName() ? lower(object->getAppearance()->getAppearanceTemplateName()) : "";
		std::string::size_type const slash = name.find_last_of('/');
		if (slash != std::string::npos)
			name.erase(0, slash + 1);
		std::string::size_type const dot = name.find('.');
		if (dot != std::string::npos)
			name.erase(dot);
		std::map<std::string, int> species;
		for (StringVector::const_iterator a = animations.begin(); a != animations.end(); ++a)
			++species[speciesOf(*a)];
		prefix = "all_b";
		while (!name.empty())
		{
			if (species.find(name) != species.end())
			{
				prefix = name;
				break;
			}
			std::string::size_type const cut = name.find_last_of('_');
			name = cut == std::string::npos ? std::string() : name.substr(0, cut);
		}
	}
	std::string const prefixUnderscore = prefix + "_";

	int total = 0;
	int const first = page * cs_pageSize;
	std::string const loopArg = loop ? " 1" : " 0";
	for (StringVector::const_iterator a = animations.begin(); a != animations.end(); ++a)
	{
		if (a->compare(0, prefixUnderscore.size(), prefixUnderscore) != 0)
			continue;
		if (total >= first && total < first + cs_pageSize)
		{
			Item item;
			item.label = a->substr(prefixUnderscore.size());
			if (item.label.size() > 4 && item.label.compare(item.label.size() - 4, 4, ".ans") == 0)
				item.label.erase(item.label.size() - 4);
			item.command = "/scene playAnimation " + *a + loopArg;
			items.push_back(item);
		}
		++total;
	}
	{
		Item toggle;
		toggle.label = loop ? "[loop on] - switch to play once" : "[loop off] - switch to loop";
		toggle.command = format("/devtest anims %s %d %s", prefix.c_str(), page, loop ? "once" : "loop");
		items.insert(items.begin(), toggle);
	}
	addPaging(items, "/devtest anims " + prefix, page, total);
	// the paging entries keep the loop state
	for (Items::iterator i = items.begin(); i != items.end(); ++i)
		if (i->label.compare(0, 2, "<<") == 0 || i->label.compare(0, 2, ">>") == 0)
			i->command += loop ? " loop" : " once";
	title = format("Animations '%s': %d (on the look-at target, else the player)", prefix.c_str(), total);
	return total;
#else
	UNREF(prefixOrTarget);
	UNREF(page);
	UNREF(loop);
	UNREF(title);
	return 0;
#endif
}

// ----------------------------------------------------------------------

void DevTestTools::getAnimationSpecies(Items & items)
{
	items.clear();
#if PRODUCTION == 0
	std::map<std::string, int> species;
	StringVector const & animations = getAnimations();
	for (StringVector::const_iterator a = animations.begin(); a != animations.end(); ++a)
		++species[speciesOf(*a)];
	// the 40 largest sets, alphabetical
	std::vector<std::pair<int, std::string> > bySize;
	for (std::map<std::string, int>::const_iterator s = species.begin(); s != species.end(); ++s)
		if (s->second >= 10)
			bySize.push_back(std::make_pair(-s->second, s->first));
	std::sort(bySize.begin(), bySize.end());
	if (bySize.size() > 40)
		bySize.resize(40);
	std::vector<std::string> names;
	for (std::vector<std::pair<int, std::string> >::const_iterator b = bySize.begin(); b != bySize.end(); ++b)
		names.push_back(b->second);
	std::sort(names.begin(), names.end());
	for (std::vector<std::string>::const_iterator n = names.begin(); n != names.end(); ++n)
	{
		Item item;
		item.label = format("%s (%d)", n->c_str(), species[*n]);
		item.command = "/devtest anims " + *n;
		items.push_back(item);
	}
#endif
}

// ======================================================================
