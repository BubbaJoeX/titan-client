// ======================================================================
//
// DevTestMenu.cpp
//
// See DevTestMenu.h. The action lines reuse the existing console commands
// (/scene mountShip, /mount mountCreature, /scene playAnimation, /scene
// setTime, /scene wanderCreate...); the /devtest verbs below supply only the
// glue those commands lack offline: a spawn in front of the player that
// remembers its id, look-at targeting without a mouse, dismounting from a
// vehicle or ship, saved points and debug-flag toggles.
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/DevTestMenu.h"

#include "clientGame/ClientObject.h"
#include "clientGame/ClientWorld.h"
#include "clientGame/ContainerInterface.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/DevTestTools.h"
#include "clientGame/Game.h"
#include "clientGame/RemoteCreatureController.h"
#include "clientGame/TangibleObject.h"
#include "clientGame/GamePlaybackScript.h"
#include "clientGame/PlayerShipController.h"
#include "clientGame/RtCameraManager.h"
#include "clientGraphics/RenderWorld.h"
#include "clientGraphics/Texture.h"
#include "clientGraphics/TextureList.h"
#include "clientSkeletalAnimation/AnimationTrackId.h"
#include "clientSkeletalAnimation/SkeletalAppearance2.h"
#include "clientSkeletalAnimation/StateHierarchyAnimationController.h"
#include "clientSkeletalAnimation/TrackAnimationController.h"
#include "clientUserInterface/CuiCombatManager.h"
#include "clientUserInterface/CuiConsoleHelper.h"
#include "sharedCollision/CollisionWorld.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/Clock.h"
#include "sharedFoundation/ConstCharCrcLowerString.h"
#include "sharedMath/Quaternion.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/CustomizationData.h"
#include "sharedObject/CustomizationDataProperty.h"
#include "sharedObject/CustomizationVariable.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/Object.h"
#include "sharedObject/ObjectTemplate.h"
#include "sharedObject/PortalProperty.h"
#include "sharedObject/World.h"
#include "sharedObject/RangedIntCustomizationVariable.h"
#include "sharedObject/SlotIdManager.h"
#include "sharedObject/SlottedContainmentProperty.h"
#include "sharedFoundation/ConstCharCrcString.h"
#include "sharedTerrain/TerrainObject.h"
#include "swgSharedUtility/States.def"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <set>

// ======================================================================

#if PRODUCTION == 0

namespace DevTestMenuNamespace
{
	DevTestMenu::Action const cs_actions[] =
	{
		// -- vehicles: a client-side mount creature spawned beside the player and mounted (/mount is shadowed by the
		//    server command of the same name, so offline it never reaches SwgCuiCommandParserMount)
		{"vehicle.swoop",      "Vehicles",            "Swoop bike",               "/devtest dismount\n/devtest spawn object/mobile/vehicle/shared_speederbike_swoop.iff 3 0 mount\n/devtest mount last"},
		{"vehicle.speeder",    "Vehicles",            "Speeder bike",             "/devtest dismount\n/devtest spawn object/mobile/vehicle/shared_speederbike.iff 3 0 mount\n/devtest mount last"},
		{"vehicle.x34",        "Vehicles",            "X-34 landspeeder",         "/devtest dismount\n/devtest spawn object/mobile/vehicle/shared_landspeeder_x34.iff 4 0 mount\n/devtest mount last"},
		{"vehicle.barc",       "Vehicles",            "BARC speeder",             "/devtest dismount\n/devtest spawn object/mobile/vehicle/shared_barc_speeder.iff 3 0 mount\n/devtest mount last"},
		{"vehicle.atst",       "Vehicles",            "AT-ST walker",             "/devtest dismount\n/devtest spawn object/mobile/vehicle/shared_walker_at_st.iff 6 0 mount\n/devtest mount last"},
		{"vehicle.jetpack",    "Vehicles",            "Jetpack",                  "/devtest dismount\n/devtest spawn object/mobile/vehicle/shared_jetpack.iff 2 0 mount\n/devtest mount last"},
		{"vehicle.dismount",   "Vehicles",            "Dismount",                 "/devtest dismount"},
		{"vehicle.pick",       "Vehicles",           "Pick any vehicle...",      "/devtest pick object/mobile/vehicle/"},
		{"vehicle.telemetry",  "Vehicles",           "Speed readout on",         "/devtest telemetry on"},
		{"vehicle.telemetryoff", "Vehicles",           "Speed readout off",        "/devtest telemetry off"},

		// -- atmospheric flight: a ship spawned above the ground ahead, targeted and boarded with /scene mountShip
		{"flight.xwing",       "Atmospheric flight",  "X-wing",                   "/devtest dismount\n/devtest spawn object/ship/player/shared_player_xwing.iff 20 30 mount\n/devtest target last\n/scene mountShip\n/devtest target clear"},
		{"flight.tie",         "Atmospheric flight",  "TIE fighter",              "/devtest dismount\n/devtest spawn object/ship/player/shared_player_tiefighter.iff 20 30 mount\n/devtest target last\n/scene mountShip\n/devtest target clear"},
		{"flight.ywing",       "Atmospheric flight",  "Y-wing",                   "/devtest dismount\n/devtest spawn object/ship/player/shared_player_ywing.iff 20 30 mount\n/devtest target last\n/scene mountShip\n/devtest target clear"},
		{"flight.n1",          "Atmospheric flight",  "Naboo N-1",                "/devtest dismount\n/devtest spawn object/ship/player/shared_player_naboo_n1.iff 20 30 mount\n/devtest target last\n/scene mountShip\n/devtest target clear"},
		{"flight.yt1300",      "Atmospheric flight",  "YT-1300",                  "/devtest dismount\n/devtest spawn object/ship/player/shared_player_yt1300.iff 40 40 mount\n/devtest target last\n/scene mountShip\n/devtest target clear"},
		{"flight.climb",       "Atmospheric flight",  "Climb 100 m",              "/devtest hop 100"},
		// -- flight presets around the Titan atmo starport at 45, 450 (spawned there if it is missing)
		{"flight.approach",    "Atmospheric flight", "Starport approach (X-wing)", "/devtest ensure object/building/titan/shared_titan_atmo_starport.iff 45 450 0\n/devtest dismount\n/devtest spawnat object/ship/player/shared_player_xwing.iff 45 150 0 120 mount\n/devtest target last\n/scene mountShip\n/devtest target clear\n/devtest telemetry on"},
		{"flight.well",        "Atmospheric flight", "Hover over starport well (N-1)", "/devtest ensure object/building/titan/shared_titan_atmo_starport.iff 45 450 0\n/devtest dismount\n/devtest spawnat object/ship/player/shared_player_naboo_n1.iff 45 450 0 160 mount\n/devtest target last\n/scene mountShip\n/devtest target clear\n/devtest telemetry on"},
		{"flight.view",        "Atmospheric flight", "Aerial view of the starport", "/devtest ensure object/building/titan/shared_titan_atmo_starport.iff 45 450 0\n/devtest camera free 245 260 250 45 0 450"},
		{"flight.pick",        "Atmospheric flight", "Pick any player ship...",  "/devtest pick object/ship/player/"},
		{"flight.telemetry",   "Atmospheric flight", "Speed / altitude readout on", "/devtest telemetry on"},
		{"flight.telemetryoff", "Atmospheric flight", "Speed / altitude readout off", "/devtest telemetry off"},
		{"flight.exit",        "Atmospheric flight",  "Leave ship (to ground)",   "/devtest dismount"},

		// -- animations: /scene playAnimation on the look-at target, or the player when there is none
		{"anim.wave",          "Animations",          "Player: wave",             "/devtest target clear\n/scene playAnimation all_b_emt_wave1.ans 0"},
		{"anim.salute",        "Animations",          "Player: salute",           "/devtest target clear\n/scene playAnimation all_b_emt_salute1.ans 0"},
		{"anim.laugh",         "Animations",          "Player: laugh",            "/devtest target clear\n/scene playAnimation all_b_emt_belly_laugh.ans 0"},
		{"anim.jump",          "Animations",          "Player: jump",             "/devtest target clear\n/scene playAnimation all_b_loc_standing_jump_up.ans 0"},
		{"anim.dance",         "Animations",          "Player: dance (loop)",     "/devtest target clear\n/scene playAnimation all_b_dnc_bowie_loop.ans 1"},
		{"anim.flex",          "Animations",          "Player: flex (loop)",      "/devtest target clear\n/scene playAnimation all_b_emt_flex_biceps.ans 1"},
		{"anim.bantha",        "Animations",          "Bantha: happy (loop)",     "/devtest spawn object/mobile/shared_bantha.iff 10\n/devtest wait 5\n/devtest target last\n/scene playAnimation bantha_emt_stand_happy.ans 1\n/devtest target clear"},
		{"anim.dewback",       "Animations",          "Dewback: confused (loop)", "/devtest spawn object/mobile/shared_dewback.iff 8\n/devtest wait 5\n/devtest target last\n/scene playAnimation dewback_emt_stand_confused.ans 1\n/devtest target clear"},
		{"anim.stop",          "Animations",          "Stop loops (player)",      "/devtest target clear\n/devtest animstop"},
		{"anim.stoplast",      "Animations",          "Stop loops (last spawn)",  "/devtest target last\n/devtest animstop\n/devtest target clear"},
		{"anim.browse",        "Animations",         "Browse: player...",        "/devtest target clear\n/devtest anims target"},
		{"anim.browselast",    "Animations",         "Browse: last spawn...",    "/devtest target last\n/devtest anims target"},
		{"anim.species",       "Animations",         "Browse by species...",     "/devtest animspecies"},

		// -- spawns
		{"spawn.bantha",       "Spawn",               "Bantha (standing)",        "/devtest spawn object/mobile/shared_bantha.iff 10"},
		{"spawn.rancor",       "Spawn",               "Rancor (standing)",        "/devtest spawn object/mobile/shared_rancor.iff 14"},
		{"spawn.wander",       "Spawn",               "Dewback (wanders)",        "/scene wanderCreate object/mobile/shared_dewback.iff"},
		{"spawn.follow",       "Spawn",               "Bantha (follows you)",     "/scene followCreate object/mobile/shared_bantha.iff"},
		{"spawn.naboo",        "Spawn",               "Naboo filler building",    "/devtest spawn object/building/naboo/shared_filler_building_naboo_style_10.iff 30"},
		{"spawn.tatooine",     "Spawn",               "Tatooine filler building", "/devtest spawn object/building/tatooine/shared_filler_building_tatt_style01_05.iff 30"},
		{"spawn.cantina",      "Spawn",               "Tatooine cantina",         "/devtest spawn object/building/tatooine/shared_cantina_tatooine.iff 50"},
		{"spawn.starport",     "Spawn",               "Titan atmo starport",      "/devtest spawn object/building/titan/shared_titan_atmo_starport.iff 150"},
		{"spawn.pickcreature", "Spawn",              "Pick a creature...",       "/devtest pick object/mobile/shared_"},
		{"spawn.pickbuilding", "Spawn",              "Pick a building...",       "/devtest pick object/building/"},
		{"spawn.picktitan",    "Spawn",              "Pick a Titan building...", "/devtest pick object/building/titan/"},
		{"spawn.pickfurniture", "Spawn",              "Pick furniture...",        "/devtest pick object/tangible/furniture/"},
		{"spawn.list",         "Spawn",               "List my spawns",           "/devtest spawns"},
		{"spawn.clear",        "Spawn",               "Delete my spawns",         "/devtest clear"},

		// -- warp points (saved ones are added to the menu by name)
		{"warp.save",          "Warp",                "Save point 'quick'",       "/devtest point save quick"},
		{"warp.quick",         "Warp",                "Go to 'quick'",            "/devtest point go quick"},
		{"warp.origin",        "Warp",                "World origin (0, 0)",      "/devtest goto 0 0"},
		{"warp.up",            "Warp",                "Up 50 m",                  "/devtest hop 50"},
		{"warp.list",          "Warp",                "List saved points",        "/devtest point list"},

		// -- camera (the free camera; /devtest camera chase returns to normal)
		{"camera.orbit",       "Camera",             "Orbit me: 25 m, 30 down",  "/devtest camera orbit 25 30"},
		{"camera.high",        "Camera",             "Orbit me: 80 m, 60 down",  "/devtest camera orbit 80 60"},
		{"camera.top",         "Camera",             "Top-down: 150 m",          "/devtest camera orbit 150 89"},
		{"camera.last",        "Camera",             "Orbit the last spawn",     "/devtest target last\n/devtest camera orbit 30 25 0 last"},
		{"camera.chase",       "Camera",             "Back to chase camera",     "/devtest camera chase"},
		{"camera.info",        "Camera",             "Where is the camera?",     "/devtest camera info"},

		// -- debug views
		{"debug.floors",       "Debug views",         "Floors",                   "/devtest flag SharedCollision drawFloors"},
		{"debug.extents",      "Debug views",         "Collision extents",        "/devtest flag SharedCollision drawExtents"},
		{"debug.cmeshes",      "Debug views",         "Collision meshes",         "/devtest flag SharedCollision drawCMeshes"},
		{"debug.footprints",   "Debug views",         "Footprints",               "/devtest flag SharedCollision drawFootprints"},
		{"debug.pathnodes",    "Debug views",         "Path nodes",               "/devtest flag SharedCollision drawPathNodes"},
		{"debug.portals",      "Debug views",         "Portals",                  "/devtest flag ClientGraphics/Dpvs renderPortals"},
		{"debug.skeleton",     "Debug views",         "Skeletons",                "/devtest flag ClientSkeletalAnimation/Character showSkeleton"},
		{"debug.hardpoints",   "Debug views",         "Hardpoint names",          "/devtest flag SharedObject renderHardpointNames"},
		{"debug.shipextent",   "Debug views",         "Ship extents",             "/devtest flag ClientGame/PlayerShipController renderShipAppearanceExtent"},
		{"debug.fps",          "Debug views",         "FPS plot",                 "/devtest flag ClientGame/Plotter 1_fps"},
		{"debug.ids",          "Debug views",         "Network ids on",           "/scene drawNetworkIds 1"},
		{"debug.idsoff",       "Debug views",         "Network ids off",          "/scene drawNetworkIds 0"},
		{"debug.noon",         "Debug views",         "Time: noon",               "/scene setTime 0.5"},
		{"debug.night",        "Debug views",         "Time: night",              "/scene setTime 0.0"},
	};

	int const cs_actionCount = static_cast<int>(sizeof(cs_actions) / sizeof(cs_actions[0]));

	char const * const cs_pointsFile = "devtest_points.txt";

	NetworkId              s_last;
	std::vector<NetworkId> s_spawned;

	struct DeferredLine
	{
		unsigned int frame;
		std::string  line;
	};
	std::vector<DeferredLine> s_deferred;
	unsigned int              s_frame = 0;

	// ------------------------------------------------------------------

	struct Point
	{
		std::string name;
		Vector      position;
	};

	typedef std::vector<Point> Points;

	void loadPoints(Points & points)
	{
		points.clear();
		FILE * const f = fopen(cs_pointsFile, "rt");
		if (!f)
			return;
		char name[128];
		Point p;
		while (fscanf(f, " %127s %f %f %f", name, &p.position.x, &p.position.y, &p.position.z) == 4)
		{
			p.name = name;
			points.push_back(p);
		}
		fclose(f);
	}

	bool savePoints(Points const & points)
	{
		FILE * const f = fopen(cs_pointsFile, "wt");
		if (!f)
			return false;
		for (Points::const_iterator i = points.begin(); i != points.end(); ++i)
			fprintf(f, "%s %.3f %.3f %.3f\n", i->name.c_str(), i->position.x, i->position.y, i->position.z);
		fclose(f);
		return true;
	}

	// ------------------------------------------------------------------

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

	// what moves when the player moves: the vehicle or ship they sit in, else the player
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

	void warpTo(Object & mover, Vector const & position)
	{
		// a piloted ship: through its controller, or its flight model puts it straight back
		PlayerShipController * const shipController = dynamic_cast<PlayerShipController *>(mover.getController());
		if (shipController)
		{
			Transform t = mover.getTransform_o2p();
			t.setPosition_p(position);
			shipController->warpForDevTest(t);
			return;
		}
		if (mover.getAttachedTo() != 0)
			mover.setParentCell(CellProperty::getWorldCellProperty());
		CellProperty::setPortalTransitionsEnabled(false);
			mover.setPosition_p(position);
		CellProperty::setPortalTransitionsEnabled(true);
		CollisionWorld::objectWarped(&mover);
	}

	// ------------------------------------------------------------------
	// vessels (boardable POB ships): a client-side flight, so walking aboard a moving ship, the view through its
	// windows and its moving parts can be tried without a server

	struct VesselMove
	{
		NetworkId id;
		Vector    delta_o;      // in the vessel's frame at the start of the move
		float     yaw;          // radians
		float     seconds;
		float     elapsed;
		bool      started;
		Transform from;
		Transform to;
	};

	std::vector<VesselMove> s_vesselMoves;

	void updateVesselMoves()
	{
		float const dt = Clock::frameTime();
		std::set<NetworkId> busy;
		for (std::vector<VesselMove>::iterator i = s_vesselMoves.begin(); i != s_vesselMoves.end(); )
		{
			if (busy.count(i->id))
			{
				++i;
				continue;
			}
			IGNORE_RETURN(busy.insert(i->id));
			Object * const object = NetworkIdManager::getObjectById(i->id);
			if (!object)
			{
				i = s_vesselMoves.erase(i);
				continue;
			}
			if (!i->started)
			{
				i->started = true;
				i->from = object->getTransform_o2p();
				i->to = i->from;
				i->to.move_l(i->delta_o);
				i->to.yaw_l(i->yaw);
			}
			i->elapsed += dt;
			float const t = (i->seconds > 0.0f) ? std::min(1.0f, i->elapsed / i->seconds) : 1.0f;
			float const eased = t * t * (3.0f - 2.0f * t);
			Transform pose;
			Quaternion(i->from).slerp(Quaternion(i->to), eased).getTransform(&pose);
			pose.setPosition_p(Vector::linearInterpolate(i->from.getPosition_p(), i->to.getPosition_p(), eased));
			CellProperty::setPortalTransitionsEnabled(false);
				object->setTransform_o2p(pose);
			CellProperty::setPortalTransitionsEnabled(true);
			if (t >= 1.0f)
				i = s_vesselMoves.erase(i);
			else
				++i;
		}
	}

	bool vessel(DevTestMenu::StringVector const & argv, std::string & result)
	{
		// /devtest vessel <id|last> hpdyn <kind> <field> ... [| <kind> ...]   apply an hp_dyn pack locally
		// /devtest vessel <id|last> fly <dx> <dy> <dz> <seconds> [yaw]       move it (own frame), queued per vessel
		// /devtest vessel <id|last> video <url> [aspect]                     play a stream on a screen (offline)
		// /devtest vessel <id|last> furnish <template> <cell> <x> <y> <z> <heading>  an interior piece in a cell
		// /devtest vessel <id|last> stop | info
		size_t const n = argv.size();
		NetworkId const id = argv[1] == "last" ? s_last : NetworkId(argv[1]);
		Object * const object = NetworkIdManager::getObjectById(id);
		ClientObject * const clientObject = object ? object->asClientObject() : 0;
		TangibleObject * const tangible = clientObject ? clientObject->asTangibleObject() : 0;
		if (!tangible)
		{
			result += "vessel: no tangible object " + argv[1] + "\n";
			return true;
		}
		std::string const & sub = argv[2];
		if (sub == "hpdyn")
		{
			std::string packed("v1");
			std::string line;
			for (size_t a = 3; a <= n; ++a)
			{
				if (a == n || argv[a] == "|")
				{
					if (!line.empty())
						packed += "\n" + line;
					line.clear();
					continue;
				}
				if (!line.empty())
					line.push_back('\t');
				line += argv[a];
			}
			tangible->applyDynamicHardpointsStateLocally(packed);
			result += format("vessel: hp_dyn applied (%d bytes) to %s\n", static_cast<int>(packed.size()), id.getValueString().c_str());
			return true;
		}
		if (sub == "mood" && n >= 4)
		{
			// /devtest vessel <id|last> mood <name>   a creature's animation mood, as the server's setAnimationMood
			CreatureObject * const creature = clientObject->asCreatureObject();
			if (creature)
			{
				creature->setAnimationMood(argv[3]);
				creature->signalAnimationMoodModified(argv[3]);
			}
			result += format("vessel: mood %s %s\n", argv[3].c_str(), creature ? "set" : "FAILED (not a creature)");
			return true;
		}
		if (sub == "anim" && n >= 4)
		{
			// /devtest vessel <id|last> anim <appearance/animation/x.ans> [action|loop]   play it on that object once
			char const * const track = (n >= 5) ? argv[4].c_str() : "action";
			bool const ok = GamePlaybackScript::playAnimation(*object, track, argv[3].c_str(), false);
			result += format("vessel: anim %s %s on %s\n", ok ? "playing" : "FAILED", argv[3].c_str(), track);
			return true;
		}
		if (sub == "condition" && n >= 4)
		{
			// /devtest vessel <id|last> condition <hex mask> [on|off]   set or clear condition bits (marker tests)
			int const mask = static_cast<int>(strtoul(argv[3].c_str(), 0, 16));
			if (n >= 5 && argv[4] == "off")
				tangible->clearCondition(mask);
			else
				tangible->setCondition(mask);
			result += format("vessel: condition 0x%08x on %s\n", static_cast<unsigned>(tangible->getCondition()), id.getValueString().c_str());
			return true;
		}
		if (sub == "fly" && n >= 7)
		{
			VesselMove m;
			m.id = id;
			m.delta_o = Vector(static_cast<float>(atof(argv[3].c_str())), static_cast<float>(atof(argv[4].c_str())), static_cast<float>(atof(argv[5].c_str())));
			m.seconds = static_cast<float>(atof(argv[6].c_str()));
			m.yaw = n >= 8 ? convertDegreesToRadians(static_cast<float>(atof(argv[7].c_str()))) : 0.0f;
			m.elapsed = 0.0f;
			m.started = false;
			s_vesselMoves.push_back(m);
			result += format("vessel: queued move %.1f %.1f %.1f over %.1f s\n", m.delta_o.x, m.delta_o.y, m.delta_o.z, m.seconds);
			return true;
		}
		if (sub == "furnish" && n >= 9)
		{
			// the interior buildout offline: ship interior table rows are in the hull frame, which is cell space
			PortalProperty * const portals = object->getPortalProperty();
			CellProperty * const cell = portals ? portals->getCell(argv[4].c_str()) : 0;
			if (!cell)
			{
				result += "vessel: no cell " + argv[4] + "\n";
				return true;
			}
			Object * const piece = ObjectTemplate::createObject(argv[3].c_str());
			ClientObject * const clientPiece = piece ? piece->asClientObject() : 0;
			if (!clientPiece)
			{
				delete piece;
				result += "vessel: cannot create an object from " + argv[3] + "\n";
				return true;
			}
			clientPiece->setParentCell(cell);
			Transform pose;
			pose.yaw_l(convertDegreesToRadians(static_cast<float>(atof(argv[8].c_str()))));
			pose.setPosition_p(Vector(static_cast<float>(atof(argv[5].c_str())), static_cast<float>(atof(argv[6].c_str())), static_cast<float>(atof(argv[7].c_str()))));
			clientPiece->setTransform_o2p(pose);
			RenderWorld::addObjectNotifications(*clientPiece);
			clientPiece->endBaselines();
			clientPiece->addToWorld();
			s_last = clientPiece->getNetworkId();
			s_spawned.push_back(s_last);
			result += format("vessel: %s in %s id %s\n", argv[3].c_str(), argv[4].c_str(), s_last.getValueString().c_str());
			return true;
		}
		if (sub == "video" && n >= 4)
		{
			// the cabin screen offline: what script.space.vessel.cabin_media sets on the server
			tangible->setRemoteVideoStreamLocally(argv[3], n >= 5 ? argv[4] : std::string("16:9"));
			result += "vessel: video " + argv[3] + " on " + id.getValueString() + "\n";
			return true;
		}
		if (sub == "stop")
		{
			for (std::vector<VesselMove>::iterator i = s_vesselMoves.begin(); i != s_vesselMoves.end(); )
				i = (i->id == id) ? s_vesselMoves.erase(i) : i + 1;
			result += "vessel: stopped\n";
			return true;
		}
		if (sub == "info")
		{
			Vector const p = object->getPosition_w();
			int moves = 0;
			for (std::vector<VesselMove>::const_iterator i = s_vesselMoves.begin(); i != s_vesselMoves.end(); ++i)
				moves += (i->id == id) ? 1 : 0;
			PortalProperty const * const portals = object->getPortalProperty();
			result += format("vessel: %s at %.2f %.2f %.2f yaw %.1f, %d cells, %d moves queued\n", id.getValueString().c_str(), p.x, p.y, p.z,
				convertRadiansToDegrees(object->getObjectFrameK_w().theta()), portals ? portals->getNumberOfCells() : 0, moves);
			return true;
		}
		result += "vessel: hpdyn | fly | video <url> [aspect] | stop | info\n";
		return true;
	}

	void setTarget(NetworkId const & id)
	{
		CuiCombatManager::setLookAtTarget(id);
		CreatureObject * const player = Game::getPlayerCreature();
		if (player)
			player->setLookAtTarget(id);
	}

	bool spawnObject(std::string const & templateName, Vector const & position, float yaw, bool mount, std::string & result);

	// ------------------------------------------------------------------
	// /devtest spawn <template> [distance=6] [height=0] [mount]
	// Placed ahead of the player (or their vehicle) on the terrain, raised by height. Plain spawns face the player;
	// "mount" ones (vehicles, ships) face along the player's heading and get the mountable condition.

	bool spawn(DevTestMenu::StringVector const & argv, std::string & result)
	{
		if (!Game::getSinglePlayer())
		{
			result += "spawn: single player (Load Scene) only\n";
			return true;
		}
		Object * const anchor = getMover();
		if (!anchor)
		{
			result += "spawn: no player\n";
			return true;
		}
		std::string const & templateName = argv[1];
		float const distance = argv.size() > 2 ? static_cast<float>(atof(argv[2].c_str())) : 6.0f;
		float const height   = argv.size() > 3 ? static_cast<float>(atof(argv[3].c_str())) : 0.0f;
		bool const  mount    = argv.size() > 4 && argv[4] == "mount";

		Vector forward = anchor->getObjectFrameK_w();
		forward.y = 0.0f;
		if (!forward.normalize())
			forward = Vector::unitZ;
		Vector position = anchor->getPosition_w() + forward * distance;
		float ground = 0.0f;
		if (terrainHeight(position, ground))
			position.y = ground;
		position.y += height;

		float const yaw = mount ? atan2(forward.x, forward.z) : atan2(-forward.x, -forward.z);
		return spawnObject(templateName, position, yaw, mount, result);
	}

	// ------------------------------------------------------------------
	// Create a client-side object in the world cell, as /scene objectCreate does, and remember it as $last.

	bool spawnObject(std::string const & templateName, Vector const & position, float yaw, bool mount, std::string & result)
	{
		Object * const object = ObjectTemplate::createObject(templateName.c_str());
		ClientObject * const clientObject = object ? object->asClientObject() : 0;
		if (!clientObject)
		{
			delete object;
			result += format("spawn: cannot create an object from %s\n", templateName.c_str());
			return true;
		}

		clientObject->setPosition_p(position);
		clientObject->yaw_o(yaw);

		CreatureObject * const creature = dynamic_cast<CreatureObject *>(clientObject);
		RemoteCreatureController * controller = 0;
		if (creature)
		{
			controller = new RemoteCreatureController(creature);
			creature->setController(controller);
			if (mount)
				creature->setCondition(TangibleObject::C_mount);
		}

		RenderWorld::addObjectNotifications(*clientObject);
		if (creature)
			clientObject->addNotification(ClientWorld::getTangibleNotification());

		PortalProperty * const portalProperty = clientObject->getPortalProperty();
		if (portalProperty)
			portalProperty->clientSinglePlayerInitializeFirstTimeObject();

		clientObject->endBaselines();
		clientObject->addToWorld();
		if (controller)
			controller->updateDeadReckoningModel(clientObject->getTransform_o2w());

		s_last = clientObject->getNetworkId();
		s_spawned.push_back(s_last);
		result += format("spawned %s id %s at %.1f %.1f %.1f\n", templateName.c_str(), s_last.getValueString().c_str(), position.x, position.y, position.z);
		return true;
	}

	// ------------------------------------------------------------------
	// /devtest spawnat <template> <x> <z> [heading=0] [height=0] [mount]: at a world x/z on the terrain, raised by height.
	// /devtest ensure <template> <x> <z> [heading=0]: the same, unless one is already within 100 m (fixed scenery such as
	// the starport the flight presets fly to).

	bool spawnAt(DevTestMenu::StringVector const & argv, bool onlyIfMissing, std::string & result)
	{
		if (!Game::getSinglePlayer())
		{
			result += "spawn: single player (Load Scene) only\n";
			return true;
		}
		std::string const & templateName = argv[1];
		Vector position(static_cast<float>(atof(argv[2].c_str())), 0.0f, static_cast<float>(atof(argv[3].c_str())));
		float const heading = argv.size() > 4 ? static_cast<float>(atof(argv[4].c_str())) : 0.0f;
		float const height  = argv.size() > 5 ? static_cast<float>(atof(argv[5].c_str())) : 0.0f;
		bool const  mount   = argv.size() > 6 && argv[6] == "mount";

		if (onlyIfMissing)
		{
			for (int list = static_cast<int>(WOL_Tangible); list <= static_cast<int>(WOL_TangibleNotTargetable); ++list)
				for (int i = 0; i < World::getNumberOfObjects(list); ++i)
				{
					Object const * const o = World::getObject(list, i);
					char const * const name = o ? o->getObjectTemplateName() : 0;
					Vector const p = o ? o->getPosition_w() : Vector::zero;
					if (name && templateName == name && Vector(p.x - position.x, 0.0f, p.z - position.z).magnitude() < 100.0f)
					{
						result += format("%s is already at %.1f %.1f %.1f\n", templateName.c_str(), p.x, p.y, p.z);
						return true;
					}
				}
		}

		float ground = 0.0f;
		if (terrainHeight(position, ground))
			position.y = ground;
		position.y += height;
		return spawnObject(templateName, position, heading * PI / 180.0f, mount, result);
	}

	// ------------------------------------------------------------------
	// Out of the vehicle or ship the player sits in, onto the ground beside it.

	bool dismount(std::string & result)
	{
		CreatureObject * const player = Game::getPlayerCreature();
		if (!player)
		{
			result += "dismount: no player\n";
			return true;
		}
		Object * const container = ContainerInterface::getContainedByObject(*player);
		if (!container || container->getCellProperty())
		{
			result += "dismount: not riding or piloting anything\n";
			return true;
		}

		Vector position = container->getPosition_w() + container->getObjectFrameI_w() * 4.0f;
		float ground = 0.0f;
		if (terrainHeight(position, ground))
			position.y = ground;

		CreatureObject * const mountCreature = dynamic_cast<CreatureObject *>(container);
		if (mountCreature)
			mountCreature->setState(States::MountedCreature, false);
		player->setState(States::RidingMount, false);
		player->setState(States::PilotingShip, false);

		Transform transform;
		transform.setPosition_p(position);
		if (!ContainerInterface::transferItemToWorld(*player, transform))
		{
			result += "dismount: transfer to the world failed\n";
			return true;
		}
		CollisionWorld::objectWarped(player);
		result += format("dismounted at %.1f %.1f %.1f\n", position.x, position.y, position.z);
		return true;
	}

	// ------------------------------------------------------------------
	// Mount a creature vehicle: the rider slot transfer of SwgCuiCommandParserMount::doMountCreature.

	bool mount(std::string const & which, std::string & result)
	{
		CreatureObject * const rider = Game::getPlayerCreature();
		NetworkId const id = which == "last" ? s_last : NetworkId(which);
		CreatureObject * const mountObject = dynamic_cast<CreatureObject *>(NetworkIdManager::getObjectById(id));
		if (!rider || !mountObject)
		{
			result += "mount: no player or no creature " + id.getValueString() + "\n";
			return true;
		}
		SlottedContainmentProperty * const containment = ContainerInterface::getSlottedContainmentProperty(*rider);
		int const arrangement = containment ? containment->getBestArrangementForSlot(SlotIdManager::findSlotId(ConstCharCrcString("rider"))) : -1;
		if (arrangement < 0 || arrangement >= containment->getNumberOfArrangements())
		{
			result += "mount: the player has no rider arrangement\n";
			return true;
		}
		rider->setState(States::RidingMount, true);
		mountObject->setState(States::MountedCreature, true);
		if (!ContainerInterface::transferItemToSlottedContainer(*mountObject, *rider, arrangement))
		{
			rider->setState(States::RidingMount, false);
			mountObject->setState(States::MountedCreature, false);
			result += "mount: transfer into " + id.getValueString() + " failed (not a mount?)\n";
			return true;
		}
		result += "mounted " + id.getValueString() + "\n";
		return true;
	}

	// ------------------------------------------------------------------
	// Customization variables (offline the server command table shadows /customization, as it does /mount):
	//   custom list [id]                 every variable with its value and range
	//   custom set <variable> <value> [id]
	//   custom persist [id]              write the persisted string and load it back (format version, values kept)
	// [id] is "last" (the last spawn), an object id, or else the look-at target, else the player.

	void listCustomizationVariable(std::string const & name, CustomizationVariable * variable, void * context)
	{
		std::string & out = *static_cast<std::string *>(context);
		RangedIntCustomizationVariable const * const ranged = dynamic_cast<RangedIntCustomizationVariable const *>(variable);
		if (ranged)
		{
			int lo = 0;
			int hi = 0;
			ranged->getRange(lo, hi);
			out += format("  %s = %d [%d, %d)\n", name.c_str(), ranged->getValue(), lo, hi);
		}
		else
			out += "  " + name + "\n";
	}

	void collectCustomizationValue(std::string const & name, CustomizationVariable * variable, void * context)
	{
		RangedIntCustomizationVariable const * const ranged = dynamic_cast<RangedIntCustomizationVariable const *>(variable);
		if (ranged)
			(*static_cast<std::map<std::string, int> *>(context))[name] = ranged->getValue();
	}

	bool custom(DevTestMenu::StringVector const & argv, std::string & result)
	{
		std::string const & sub = argv.size() > 1 ? argv[1] : std::string("list");
		size_t const idIndex = sub == "set" ? 4 : 2;
		Object * object = 0;
		if (argv.size() > idIndex)
			object = NetworkIdManager::getObjectById(argv[idIndex] == "last" ? s_last : NetworkId(argv[idIndex]));
		else
		{
			object = CuiCombatManager::getLookAtTarget().getObject();
			if (!object)
				object = Game::getPlayer();
		}
		CustomizationDataProperty * const property = object ? safe_cast<CustomizationDataProperty *>(object->getProperty(CustomizationDataProperty::getClassPropertyId())) : 0;
		CustomizationData * const data = property ? property->fetchCustomizationData() : 0;
		if (!data)
		{
			result += "custom: no customization data on that object\n";
			return true;
		}
		if (sub == "set" && argv.size() >= 4)
		{
			RangedIntCustomizationVariable * const variable = dynamic_cast<RangedIntCustomizationVariable *>(data->findVariable(argv[2]));
			if (variable)
			{
				variable->setValue(atoi(argv[3].c_str()));
				result += format("%s %s = %d\n", object->getNetworkId().getValueString().c_str(), argv[2].c_str(), variable->getValue());
			}
			else
				result += "custom: no int variable " + argv[2] + " on " + object->getNetworkId().getValueString() + "\n";
		}
		else if (sub == "persist")
		{
			std::map<std::string, int> before;
			std::map<std::string, int> after;
			data->iterateOverVariables(collectCustomizationValue, &before, false);
			std::string const persisted = data->writeLocalDataToString();
			data->loadLocalDataFromString(persisted);
			data->iterateOverVariables(collectCustomizationValue, &after, false);
			int changed = 0;
			for (std::map<std::string, int>::const_iterator i = before.begin(); i != before.end(); ++i)
				if (after[i->first] != i->second)
				{
					result += format("  changed %s: %d -> %d\n", i->first.c_str(), i->second, after[i->first]);
					++changed;
				}
			result += format("persisted %d bytes, format version %d, %d variables, %d changed on reload\n",
				static_cast<int>(persisted.size()), persisted.empty() ? -1 : static_cast<int>(persisted[0]),
				static_cast<int>(before.size()), changed);
		}
		else
		{
			result += object->getNetworkId().getValueString() + " " + object->getObjectTemplateName() + ":\n";
			data->iterateOverVariables(listCustomizationVariable, &result, false);
		}
		data->release();
		return true;
	}

	// ------------------------------------------------------------------
	// Stop looping animations (the "loop" track /scene playAnimation uses) on the look-at target or the player.

	bool animStop(std::string & result)
	{
		Object * object = CuiCombatManager::getLookAtTarget().getObject();
		if (!object)
			object = Game::getPlayer();
		SkeletalAppearance2 * const appearance = object ? dynamic_cast<SkeletalAppearance2 *>(object->getAppearance()) : 0;
		if (!appearance)
		{
			result += "animstop: no skeletal appearance on the target\n";
			return true;
		}
		TransformAnimationController * const base = appearance->getAnimationController();
		TrackAnimationController * controller = dynamic_cast<TrackAnimationController *>(base);
		if (!controller)
		{
			StateHierarchyAnimationController * const sh = dynamic_cast<StateHierarchyAnimationController *>(base);
			if (sh)
				controller = &sh->getTrackAnimationController();
		}
		AnimationTrackId trackId(AnimationTrackId::cms_invalid);
		if (!controller || !TrackAnimationController::getTrackIdByName(ConstCharCrcLowerString("loop"), trackId))
		{
			result += "animstop: no track animation controller\n";
			return true;
		}
		controller->stopAnimation(trackId);
		result += format("stopped the loop track on %s\n", object->getNetworkId().getValueString().c_str());
		return true;
	}

	// ------------------------------------------------------------------

	bool point(DevTestMenu::StringVector const & argv, std::string & result)
	{
		if (argv.size() < 2)
			return false;
		std::string const & what = argv[1];
		Points points;
		loadPoints(points);

		if (what == "list")
		{
			if (points.empty())
				result += "no saved points\n";
			for (Points::const_iterator i = points.begin(); i != points.end(); ++i)
				result += format("%s %.1f %.1f %.1f\n", i->name.c_str(), i->position.x, i->position.y, i->position.z);
			return true;
		}
		if (argv.size() < 3)
			return false;
		std::string const & name = argv[2];
		Points::iterator found = points.begin();
		while (found != points.end() && found->name != name)
			++found;

		if (what == "save")
		{
			Object * const mover = getMover();
			if (!mover)
			{
				result += "point: no player\n";
				return true;
			}
			if (found == points.end())
			{
				points.push_back(Point());
				found = points.end() - 1;
				found->name = name;
			}
			found->position = mover->getPosition_w();
			result += savePoints(points) ? format("saved %s at %.1f %.1f %.1f\n", name.c_str(), found->position.x, found->position.y, found->position.z)
				: std::string("point: cannot write ") + cs_pointsFile + "\n";
			return true;
		}
		if (found == points.end())
		{
			result += format("point: no point named %s\n", name.c_str());
			return true;
		}
		if (what == "go")
		{
			Object * const mover = getMover();
			if (mover)
				warpTo(*mover, found->position);
			result += format("warped to %s\n", name.c_str());
			return true;
		}
		if (what == "delete")
		{
			points.erase(found);
			IGNORE_RETURN(savePoints(points));
			result += format("deleted %s\n", name.c_str());
			return true;
		}
		return false;
	}

	// ------------------------------------------------------------------

	bool flag(DevTestMenu::StringVector const & argv, std::string & result)
	{
		bool * const value = DebugFlags::findFlag(argv[1].c_str(), argv[2].c_str());
		if (!value)
		{
			result += format("flag: no debug flag %s/%s\n", argv[1].c_str(), argv[2].c_str());
			return true;
		}
		if (argv.size() > 3 && argv[3] != "toggle")
			*value = argv[3] != "0";
		else
			*value = !*value;
		result += format("%s/%s = %d\n", argv[1].c_str(), argv[2].c_str(), *value ? 1 : 0);
		return true;
	}

	// ------------------------------------------------------------------

	bool clear(std::string & result)
	{
		CreatureObject * const player = Game::getPlayerCreature();
		Object * const container = player ? ContainerInterface::getContainedByObject(*player) : 0;
		if (container && !container->getCellProperty())
			IGNORE_RETURN(dismount(result));
		int removed = 0;
		for (std::vector<NetworkId>::const_iterator i = s_spawned.begin(); i != s_spawned.end(); ++i)
		{
			Object * const object = NetworkIdManager::getObjectById(*i);
			if (object && object != player)
			{
				object->kill();
				++removed;
			}
		}
		s_spawned.clear();
		s_last = NetworkId::cms_invalid;
		result += format("deleted %d spawned objects\n", removed);
		return true;
	}

	bool listSpawns(std::string & result)
	{
		int alive = 0;
		for (std::vector<NetworkId>::const_iterator i = s_spawned.begin(); i != s_spawned.end(); ++i)
		{
			Object const * const object = NetworkIdManager::getObjectById(*i);
			if (!object)
				continue;
			Vector const p = object->getPosition_w();
			result += format("%s %s %.1f %.1f %.1f\n", i->getValueString().c_str(), object->getObjectTemplateName() ? object->getObjectTemplateName() : "?", p.x, p.y, p.z);
			++alive;
		}
		if (!alive)
			result += "no spawns\n";
		return true;
	}
}

using namespace DevTestMenuNamespace;

#endif

// ======================================================================

int DevTestMenu::getActionCount()
{
#if PRODUCTION == 0
	return cs_actionCount;
#else
	return 0;
#endif
}

// ----------------------------------------------------------------------

DevTestMenu::Action const & DevTestMenu::getAction(int index)
{
#if PRODUCTION == 0
	VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(0, index, cs_actionCount);
	return cs_actions[index];
#else
	UNREF(index);
	static Action const none = {"", "", "", ""};
	return none;
#endif
}

// ----------------------------------------------------------------------

DevTestMenu::Action const * DevTestMenu::findAction(std::string const & id)
{
	for (int i = 0; i < getActionCount(); ++i)
		if (id == getAction(i).id)
			return &getAction(i);
	return 0;
}

// ----------------------------------------------------------------------

void DevTestMenu::getCategories(StringVector & categories)
{
	categories.clear();
	for (int i = 0; i < getActionCount(); ++i)
	{
		std::string const category = getAction(i).category;
		if (std::find(categories.begin(), categories.end(), category) == categories.end())
			categories.push_back(category);
	}
}

// ----------------------------------------------------------------------

void DevTestMenu::getLines(Action const & action, StringVector & lines)
{
	lines.clear();
	std::string const all = action.lines;
	std::string::size_type start = 0;
	while (start < all.size())
	{
		std::string::size_type end = all.find('\n', start);
		if (end == std::string::npos)
			end = all.size();
		if (end > start)
			lines.push_back(all.substr(start, end - start));
		start = end + 1;
	}
}

// ----------------------------------------------------------------------

std::string DevTestMenu::expandLine(std::string const & line)
{
#if PRODUCTION == 0
	std::string out = line;
	std::string::size_type p;
	while ((p = out.find("$last")) != std::string::npos)
		out.replace(p, 5, s_last.getValueString());
	return out;
#else
	return line;
#endif
}

// ----------------------------------------------------------------------

bool DevTestMenu::isWaitLine(std::string const & line, int & frames)
{
	if (line.compare(0, 14, "/devtest wait ") != 0)
		return false;
	frames = atoi(line.c_str() + 14);
	if (frames < 1)
		frames = 1;
	return true;
}

// ----------------------------------------------------------------------

void DevTestMenu::defer(StringVector const & lines, int frames)
{
#if PRODUCTION == 0
	for (StringVector::const_iterator i = lines.begin(); i != lines.end(); ++i)
	{
		DeferredLine d;
		d.frame = s_frame + static_cast<unsigned int>(frames);
		d.line = *i;
		s_deferred.push_back(d);
	}
#else
	UNREF(lines);
	UNREF(frames);
#endif
}

// ----------------------------------------------------------------------

void DevTestMenu::update()
{
#if PRODUCTION == 0
	++s_frame;
	DevTestTools::update();
	updateVesselMoves();
	if (s_deferred.empty() || s_deferred.front().frame > s_frame)
		return;
	CuiConsoleHelper * const helper = CuiConsoleHelper::getActiveConsoleHelper();
	if (!helper)
		return;
	// due lines run in order; a further wait pushes everything after it back
	while (!s_deferred.empty() && s_deferred.front().frame <= s_frame)
	{
		std::string const line = s_deferred.front().line;
		s_deferred.erase(s_deferred.begin());
		int frames = 0;
		if (isWaitLine(line, frames))
		{
			for (std::vector<DeferredLine>::iterator i = s_deferred.begin(); i != s_deferred.end(); ++i)
				i->frame = s_frame + static_cast<unsigned int>(frames);
			return;
		}
		std::set<Unicode::String> recursion;
		IGNORE_RETURN(helper->processInput(Unicode::narrowToWide(expandLine(line)), recursion, false));
	}
#endif
}

// ----------------------------------------------------------------------

void DevTestMenu::getPointNames(StringVector & names)
{
	names.clear();
#if PRODUCTION == 0
	Points points;
	loadPoints(points);
	for (Points::const_iterator i = points.begin(); i != points.end(); ++i)
		names.push_back(i->name);
#endif
}

// ----------------------------------------------------------------------

char const * DevTestMenu::getVerbHelp()
{
	return
		"/devtest menu                                  open the test menu\n"
		"/devtest list                                  list the menu actions\n"
		"/devtest run <action>                          run a menu action (e.g. vehicle.swoop)\n"
		"/devtest spawn <template> [dist] [height] [mount]  spawn ahead of the player ($last = its id)\n"
		"/devtest spawns | clear                        list / delete what /devtest spawned\n"
		"/devtest target last|clear|<id>                set the look-at target\n"
		"/devtest spawnat <template> <x> <z> [heading] [height] [mount]  spawn at a world point\n"
		"/devtest ensure <template> <x> <z> [heading]  spawnat unless one is already within 100 m\n"
		"/devtest camera free <x> <y> <z> [<lx> <ly> <lz>] | orbit <dist> <pitch> [yaw] | chase | info\n"
		"/devtest telemetry on|off|once                 speed / altitude of what you ride or fly\n"
		"/devtest pick <filter> [page]                  pick a template to spawn (devtest_templates.txt)\n"
		"/devtest anims [prefix|target] [page] [loop]   browse animations by species (devtest_animations.txt)\n"
		"/devtest mount last|<id>                       ride a creature vehicle\n"
		"/devtest dismount                              leave the vehicle or ship\n"
		"/devtest animstop                              stop the loop track on the target or player\n"
		"/devtest point save|go|delete <name> | list    saved warp points\n"
		"/devtest goto <x> <z> [above]                  warp onto the terrain\n"
		"/devtest hop <metres>                          move the player (or their ship) up\n"
		"/devtest flag <section> <name> [0|1|toggle]    set a debug flag\n"
		"/devtest custom list|persist [id]              customization variables (or set <variable> <value> [id])\n"
		"/devtest vessel <id|last> hpdyn|fly|stop|info  hp_dyn parts / client-side flight of a POB ship\n"
		"/devtest vessel <id|last> condition <hex> [off] set or clear condition bits (0x800 = interesting marker)\n"
		"/devtest vessel <id|last> mood <name> | anim <path.ans> [action|loop]  a creature's animation mood / play a clip on it\n"
		"/devtest wait <frames>                         (in an action) run the lines after it later\n";
}

// ----------------------------------------------------------------------

bool DevTestMenu::performVerb(StringVector const & argv, std::string & result)
{
#if PRODUCTION == 0
	if (argv.empty())
		return false;
	std::string const & verb = argv[0];
	size_t const n = argv.size();

	if (verb == "spawn" && n >= 2)
		return spawn(argv, result);
	if ((verb == "spawnat" || verb == "ensure") && n >= 4)
		return spawnAt(argv, verb == "ensure", result);
	if (DevTestTools::performVerb(argv, result))
		return true;
	if (verb == "spawns")
		return listSpawns(result);
	if (verb == "clear")
		return clear(result);
	if (verb == "dismount")
		return dismount(result);
	if (verb == "mount" && n >= 2)
		return mount(argv[1], result);
	if (verb == "animstop")
		return animStop(result);
	if (verb == "anim" && n >= 2)
	{
		// play an animation on the player: /devtest anim <name or appearance/animation/x.ans> [loop|action] [once]
		Object *const player = Game::getPlayer();
		std::string path = argv[1];
		if (path.find('/') == std::string::npos)
			path = "appearance/animation/" + path + (path.find(".ans") == std::string::npos ? ".ans" : "");
		char const *const track = (n >= 3) ? argv[2].c_str() : "loop";
		bool const loop = !(n >= 4 && argv[3] == "once");
		bool const ok = player && GamePlaybackScript::playAnimation(*player, track, path.c_str(), loop);
		result += format("anim: %s %s on %s\n", ok ? "playing" : "FAILED", path.c_str(), track);
		return true;
	}
	if (verb == "screentex" && n >= 3)
	{
		// the same path video / magic painting / RT feeds use: screen + canvas shaders only when present
		Object * const object = NetworkIdManager::getObjectById(argv[1] == "last" ? s_last : NetworkId(argv[1]));
		Texture const * const texture = object ? TextureList::fetch(argv[2].c_str()) : 0;
		if (!object || !texture)
		{
			result += "screentex: no object or texture\n";
			return true;
		}
		bool const screens = TangibleObject::applyScreenSurfaceTexture(*object, *texture);
		texture->release();
		result += format("screentex: %s on %s\n", screens ? "screen/canvas surfaces only" : "every MAIN texture (no screen surface)", argv[1].c_str());
		return true;
	}
	if (verb == "rtfeed" && n >= 3)
	{
		NetworkId const cameraId = argv[1] == "last" ? s_last : NetworkId(argv[1]);
		NetworkId const screenId(argv[2]);
		float const fov = n >= 4 ? static_cast<float>(atof(argv[3].c_str())) : 60.0f;
		bool const ok = RtCameraManager::registerFeed(cameraId, screenId, 512, fov);
		Texture const * const feedTexture = RtCameraManager::getScreenTexture(screenId);
		Object * const screen = NetworkIdManager::getObjectById(screenId);
		bool screens = false;
		if (feedTexture && screen)
			screens = TangibleObject::applyScreenSurfaceTexture(*screen, *feedTexture);
		result += format("rtfeed: register %s, texture %s, %s\n", ok ? "ok" : "failed", feedTexture ? "yes" : "no",
			screens ? "screen surfaces only" : "every MAIN texture");
		return true;
	}
	if (verb == "custom")
		return custom(argv, result);
	if (verb == "vessel" && n >= 3)
		return vessel(argv, result);
	if (verb == "point")
		return point(argv, result);
	if (verb == "flag" && n >= 3)
		return flag(argv, result);
	if (verb == "target" && n >= 2)
	{
		NetworkId id;
		if (argv[1] == "last")
			id = s_last;
		else if (argv[1] != "clear" && argv[1] != "self")
			id = NetworkId(argv[1]);
		setTarget(id);
		result += "look-at target " + (id.isValid() ? id.getValueString() : std::string("cleared")) + "\n";
		return true;
	}
	if ((verb == "goto" && n >= 3) || (verb == "hop" && n >= 2))
	{
		Object * const mover = getMover();
		if (!mover)
		{
			result += "no player\n";
			return true;
		}
		Vector position = mover->getPosition_w();
		if (verb == "goto")
		{
			position.x = static_cast<float>(atof(argv[1].c_str()));
			position.z = static_cast<float>(atof(argv[2].c_str()));
			float ground = 0.0f;
			position.y = terrainHeight(position, ground) ? ground : 0.0f;
			if (n >= 4)
				position.y += static_cast<float>(atof(argv[3].c_str()));
		}
		else
			position.y += static_cast<float>(atof(argv[1].c_str()));
		warpTo(*mover, position);
		result += format("warped to %.1f %.1f %.1f\n", position.x, position.y, position.z);
		return true;
	}
	return false;
#else
	UNREF(argv);
	UNREF(result);
	return false;
#endif
}

// ======================================================================
