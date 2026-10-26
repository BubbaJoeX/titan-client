// ======================================================================
//
// DevBridge.cpp
//
// See DevBridge.h. Polled from Game::runGameLoopOnce after the UI update, so
// commands run in the same frame position as typed chat. Listens on
// 127.0.0.1 only and serves one client at a time. Every request carries the
// token the client writes at start-up to %LOCALAPPDATA%\SwgTitan\devbridge.json
// (with the port and process id), so only the user's own tools can drive it.
// Development builds only: compiled out where PRODUCTION is 1.
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/DevBridge.h"

#include "clientGame/ConfigClientGame.h"
#include "clientGame/DevTestMenu.h"
#include "clientGame/DevTestTools.h"
#include "clientGame/FreeCamera.h"
#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientGraphics/Camera.h"
#include "clientGraphics/Graphics.h"
#include "clientSkeletalAnimation/SkeletalAppearance2.h"
#include "clientGame/GameNetwork.h"
#include "clientUserInterface/CuiConsoleHelper.h"
#include "clientUserInterface/CuiConsoleHelperOutputGenerated.h"
#include "clientUserInterface/CuiDynamicUIFont.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "clientUserInterface/CuiPreferences.h"
#include "sharedCollision/CollisionProperty.h"
#include "sharedCollision/CollisionWorld.h"
#include "sharedCollision/Footprint.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/NetworkIdArchive.h"
#include "sharedFoundation/Os.h"
#include "sharedGame/RadialMenuManager.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedNetworkMessages/ObjectMenuSelectMessage.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/Object.h"
#include "sharedObject/PortalProperty.h"
#include "sharedObject/World.h"
#include "sharedTerrain/TerrainObject.h"

#include "UIButton.h"
#include "UICheckbox.h"
#include "UIManager.h"
#include "UIPage.h"
#include "UIText.h"
#include "UITextStyle.h"
#include "UIWidget.h"

#if PRODUCTION == 0

#include <winsock2.h>
#include <wincrypt.h>

#include <cmath>
#include <cstdio>
#include <deque>
#include <set>
#include <string>

// ======================================================================

namespace DevBridgeNamespace
{
	struct LogLine
	{
		int         seq;
		std::string text;
	};

	class OutputListener : public MessageDispatch::Callback
	{
	public:
		OutputListener() : MessageDispatch::Callback(), capturing(false), captured()
		{
			connect(*this, &OutputListener::onOutput, static_cast<CuiConsoleHelper::Messages::OutputGenerated *>(0));
		}

		void onOutput(CuiConsoleHelperOutputGenerated const & msg);

		bool        capturing;
		std::string captured;
	};

	bool             s_started = false;
	bool             s_failed = false;
	SOCKET           s_listen = INVALID_SOCKET;
	SOCKET           s_client = INVALID_SOCKET;
	std::string      s_in;
	OutputListener * s_listener = 0;
	std::deque<LogLine> s_log;
	int              s_logSeq = 0;
	unsigned int     s_frame = 0;
	int const        cs_maxLog = 500;
	int              s_shotNumber = 0;
	std::string      s_token;

	// walk in progress: one short step per frame, resolved by the client's own collision (CollisionWorld::update
	// runs after every object's alter), so walls / floor edges stop it and portals change cell exactly as walking
	struct Walk
	{
		bool        active;
		int         id;
		Vector      target;
		bool        hasFace;
		Vector      face;
		float       speed;          // m/s
		float       bestDistance;
		int         stalled;
		int         frames;
		std::string cells;
		CellProperty const * lastCell;
		DWORD       lastTick;
	};
	Walk s_walk = { false, 0, Vector::zero, false, Vector::zero, 6.0f, 0.0f, 0, 0, std::string(), 0, 0 };

	// screenshot waiting for the end of the frame (DevBridge::beforePresent): reading the back buffer there works
	// whatever covers the window, where the front buffer is a copy of the screen and comes back black
	struct PendingShot
	{
		bool        active;
		int         id;
		std::string path;
	};
	PendingShot s_shot = { false, 0, std::string() };

	// ------------------------------------------------------------------

	void addLog(std::string const & text)
	{
		LogLine line;
		line.seq = ++s_logSeq;
		line.text = text;
		s_log.push_back(line);
		while (static_cast<int>(s_log.size()) > cs_maxLog)
			s_log.pop_front();
	}

	// ------------------------------------------------------------------
	// JSON helpers: the requests are flat objects of strings and integers.

	std::string jsonEscape(std::string const & s)
	{
		std::string out;
		out.reserve(s.size() + 8);
		for (std::string::const_iterator i = s.begin(); i != s.end(); ++i)
		{
			unsigned char const c = static_cast<unsigned char>(*i);
			switch (c)
			{
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20)
				{
					char buf[8];
					sprintf(buf, "\\u%04x", c);
					out += buf;
				}
				else
					out += static_cast<char>(c);
			}
		}
		return out;
	}

	std::string quoted(std::string const & s)
	{
		return "\"" + jsonEscape(s) + "\"";
	}

	std::string::size_type findValue(std::string const & obj, char const * key)
	{
		std::string const k = std::string("\"") + key + "\"";
		std::string::size_type p = obj.find(k);
		if (p == std::string::npos)
			return p;
		p = obj.find(':', p + k.size());
		if (p == std::string::npos)
			return p;
		++p;
		while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t'))
			++p;
		return p;
	}

	std::string getString(std::string const & obj, char const * key)
	{
		std::string::size_type p = findValue(obj, key);
		std::string out;
		if (p == std::string::npos || p >= obj.size() || obj[p] != '"')
			return out;
		for (++p; p < obj.size() && obj[p] != '"'; ++p)
		{
			char c = obj[p];
			if (c == '\\' && p + 1 < obj.size())
			{
				c = obj[++p];
				switch (c)
				{
				case 'n': c = '\n'; break;
				case 't': c = '\t'; break;
				case 'r': c = '\r'; break;
				case 'u':
					if (p + 4 < obj.size())
					{
						c = static_cast<char>(strtol(obj.substr(p + 1, 4).c_str(), 0, 16));
						p += 4;
					}
					break;
				default: break;              // \" \\ \/ stay as the character
				}
			}
			out += c;
		}
		return out;
	}

	int getInt(std::string const & obj, char const * key, int defaultValue)
	{
		std::string::size_type const p = findValue(obj, key);
		if (p == std::string::npos || p >= obj.size())
			return defaultValue;
		return atoi(obj.c_str() + p);
	}

	std::string vec(Vector const & v)
	{
		char buf[96];
		sprintf(buf, "[%.3f,%.3f,%.3f]", v.x, v.y, v.z);
		return buf;
	}

	// ------------------------------------------------------------------

	std::string describeObject(Object const & object)
	{
		std::string out = "{\"pos_w\":" + vec(object.getPosition_w()) + ",\"pos_p\":" + vec(object.getPosition_p());
		Vector const k = object.getObjectFrameK_w();
		char buf[64];
		sprintf(buf, ",\"yaw\":%.2f", atan2(k.x, k.z) * 57.29578f);
		out += buf;
		CellProperty const * const cell = object.getParentCell();
		if (cell && cell != CellProperty::getWorldCellProperty())
		{
			out += ",\"cell\":" + quoted(cell->getCellName() ? cell->getCellName() : "");
			sprintf(buf, ",\"cell_index\":%d", cell->getCellIndex());
			out += buf;
			PortalProperty const * const pp = cell->getPortalProperty();
			if (pp)
			{
				out += ",\"pob\":" + quoted(pp->getPobShortName() ? pp->getPobShortName() : "");
				Object const & building = pp->getOwner();
				out += ",\"building\":" + quoted(building.getObjectTemplateName() ? building.getObjectTemplateName() : "");
				out += ",\"building_id\":" + quoted(building.getNetworkId().getValueString());
				out += ",\"building_pos_w\":" + vec(building.getPosition_w());
			}
		}
		else
			out += ",\"cell\":\"world\"";
		return out + "}";
	}

	std::string state()
	{
		char buf[128];
		sprintf(buf, "{\"frame\":%u,\"scene_loading\":%s", s_frame, Game::isSceneLoading() ? "true" : "false");
		std::string out = buf;
		Object const * const player = Game::getPlayer();
		out += ",\"player\":" + (player ? describeObject(*player) : std::string("null"));
		Object const * const camera = Game::getConstCamera();
		out += ",\"camera\":" + (camera ? describeObject(*camera) : std::string("null"));
		out += ",\"mover\":" + DevTestTools::describeMover();
		char scale[192];
		sprintf(scale, ",\"ui_scale\":{\"auto\":%s,\"percent\":%d,\"factor\":%.3f,\"canvas\":%.3f,\"framebuffer\":[%d,%d]}",
			CuiPreferences::isUiScaleAuto() ? "true" : "false", CuiPreferences::getUiScalePercent(), CuiPreferences::getUiScaleFactor(),
			Graphics::getUiCanvasScale(), Graphics::getFrameBufferMaxWidth(), Graphics::getFrameBufferMaxHeight());
		out += scale;
		return out + "}";
	}

	// ------------------------------------------------------------------

	bool getVector(std::string const & obj, char const * key, Vector & v)
	{
		std::string::size_type p = findValue(obj, key);
		if (p == std::string::npos || p >= obj.size() || obj[p] != '[')
			return false;
		return sscanf(obj.c_str() + p, "[%f , %f , %f]", &v.x, &v.y, &v.z) == 3;
	}

	std::string cellLabel(CellProperty const * cell)
	{
		return cell && cell != CellProperty::getWorldCellProperty() && cell->getCellName() ? cell->getCellName() : "world";
	}

	// Clear the footprint's post-warp floating (a warp floats the player for up to 20 s and puts them back at the
	// warp point every frame) so they stand on the floor under them and steps are resolved normally.
	void settle(Object & player)
	{
		CollisionProperty * const collision = player.getCollisionProperty();
		Footprint * const foot = collision ? collision->getFootprint() : 0;
		if (foot && foot->isFloating())
			foot->alignToGroundNoFloat();
	}

	void faceToward(Object & player, Vector const & point)
	{
		Vector const dir = point - player.getPosition_w();
		player.resetRotate_o2p();
		player.yaw_o(atan2(dir.x, dir.z));
	}

	// Put the player into a named cell of the nearest building that has one, at a world position:
	// {"cell":"tower_bridge","to":[x,y,z]}. For rooms walking cannot reach offline (lifts are server-driven, and
	// /warpme always lands in the world cell).
	std::string cellWarp(std::string const & request)
	{
		Object * const player = Game::getPlayer();
		if (!player)
			return "\"ok\":false,\"error\":\"no player\"";
		std::string const name = getString(request, "cell");
		Vector to;
		if (name.empty() || !getVector(request, "to", to))
			return "\"ok\":false,\"error\":\"cellwarp needs \\\"cell\\\":name and \\\"to\\\":[x,y,z] (world)\"";
		CellProperty * best = 0;
		float bestDistance = 0.0f;
		for (int list = static_cast<int>(WOL_Tangible); list <= static_cast<int>(WOL_TangibleNotTargetable); ++list)
		{
			int const count = World::getNumberOfObjects(list);
			for (int i = 0; i < count; ++i)
			{
				Object * const o = World::getObject(list, i);
				PortalProperty * const pp = o ? o->getPortalProperty() : 0;
				CellProperty * const cell = pp ? pp->getCell(name.c_str()) : 0;
				float const distance = cell ? o->getPosition_w().magnitudeBetween(to) : 0.0f;
				if (cell && (!best || distance < bestDistance))
				{
					best = cell;
					bestDistance = distance;
				}
			}
		}
		if (!best)
			return "\"ok\":false,\"error\":" + quoted("no building near has a cell named " + name);
		player->setParentCell(best);
		CellProperty::setPortalTransitionsEnabled(false);
			player->setPosition_w(to);
		CellProperty::setPortalTransitionsEnabled(true);
		CollisionWorld::objectWarped(player);
		settle(*player);
		return "\"ok\":true,\"result\":" + describeObject(*player);
	}

	// Start walking the player to a world x/z: {"to":[x,y,z], "speed":6, "face":[x,y,z]}. The reply is sent when
	// they arrive or stop making progress (blocked), with the cells entered on the way.
	std::string startWalk(std::string const & request, int id, bool & deferred)
	{
		deferred = false;
		Object * const player = Game::getPlayer();
		if (!player)
			return "\"ok\":false,\"error\":\"no player\"";
		if (!getVector(request, "to", s_walk.target))
			return "\"ok\":false,\"error\":\"walk needs \\\"to\\\":[x,y,z] (world)\"";
		settle(*player);
		s_walk.hasFace = getVector(request, "face", s_walk.face);
		int const speed = getInt(request, "speed", 6);
		s_walk.speed = static_cast<float>(speed < 1 ? 1 : (speed > 20 ? 20 : speed));
		Vector const pos = player->getPosition_w();
		s_walk.bestDistance = Vector(s_walk.target.x - pos.x, 0.0f, s_walk.target.z - pos.z).magnitude();
		s_walk.stalled = 0;
		s_walk.frames = 0;
		s_walk.cells.clear();
		s_walk.lastCell = player->getParentCell();
		s_walk.lastTick = GetTickCount();
		s_walk.id = id;
		s_walk.active = true;
		deferred = true;
		return std::string();
	}

	std::string finishWalk(Object & player, char const * outcome)
	{
		s_walk.active = false;
		if (s_walk.hasFace)
			faceToward(player, s_walk.face);
		char head[96];
		sprintf(head, "{\"id\":%d,\"ok\":%s,\"result\":{\"outcome\":\"%s\",\"frames\":%d,", s_walk.id,
			strcmp(outcome, "arrived") == 0 ? "true" : "false", outcome, s_walk.frames);
		return std::string(head) + "\"player\":" + describeObject(player) + ",\"cells_entered\":[" + s_walk.cells + "]}}\n";
	}

	// One frame of the walk. Returns the reply once the walk is over, empty while it goes on.
	std::string stepWalk()
	{
		Object * const player = Game::getPlayer();
		if (!player)
		{
			s_walk.active = false;
			char buf[96];
			sprintf(buf, "{\"id\":%d,\"ok\":false,\"error\":\"player gone\"}\n", s_walk.id);
			return buf;
		}
		++s_walk.frames;
		CellProperty const * const cell = player->getParentCell();
		if (cell != s_walk.lastCell)
		{
			s_walk.cells += (s_walk.cells.empty() ? "" : ",") + quoted(cellLabel(cell));
			s_walk.lastCell = cell;
		}
		Vector const pos = player->getPosition_w();
		Vector flat(s_walk.target.x - pos.x, 0.0f, s_walk.target.z - pos.z);
		float const distance = flat.magnitude();
		if (distance < 0.2f)
			return finishWalk(*player, "arrived");
		// progress check on what collision let through last frame
		if (distance < s_walk.bestDistance - 0.02f)
		{
			s_walk.bestDistance = distance;
			s_walk.stalled = 0;
		}
		else if (++s_walk.stalled > 20)
			return finishWalk(*player, "blocked");
		if (s_walk.frames > 20000)
			return finishWalk(*player, "timeout");
		DWORD const now = GetTickCount();
		float dt = static_cast<float>(now - s_walk.lastTick) / 1000.0f;
		s_walk.lastTick = now;
		if (dt <= 0.0f || dt > 0.1f)
			dt = 0.033f;
		float step = s_walk.speed * dt;
		if (step > distance)
			step = distance;
		if (step > 1.5f)
			step = 1.5f;
		flat.normalize();
		player->setPosition_w(pos + flat * step);       // keep y: the floor contact sets the height
		faceToward(*player, pos + flat * 4.0f);           // look where we go (chase camera follows)
		return std::string();
	}

	// Terrain height (the generated heightmap, building layers applied) at world x/z points:
	// {"points":[[x,z],[x,z],...]} -> [h, h, ...] (null where there is no terrain)
	std::string terrain(std::string const & request)
	{
		TerrainObject const * const t = TerrainObject::getInstance();
		std::string::size_type p = findValue(request, "points");
		std::string out;
		if (!t || p == std::string::npos)
			return "[]";
		float x = 0.f, z = 0.f;
		int n = 0;
		bool const detail = request.find("\"detail\"") != std::string::npos;
		float const probeY = static_cast<float>(getInt(request, "y", 0));
		while ((p = request.find('[', p + 1)) != std::string::npos && n < 5000)
		{
			if (sscanf(request.c_str() + p, "[%f , %f]", &x, &z) != 2)
				continue;
			float h = 0.f;
			char buf[32];
			if (t->getHeight(Vector(x, 0.f, z), h))
				sprintf(buf, "%.3f", h);
			else
				strcpy(buf, "null");
			if (detail)
			{
				//-- {"detail":true,"y":Y}: also the full-detail chunk's height and the building floor under (x, Y, z)
				//-- (CollisionWorld::findObjectFloorHeightBelow) - what a ship over a landing well stands on
				float hf = 0.f, fl = 0.f;
				char more[96];
				bool const hasFull = t->getHeightForceChunkCreation(Vector(x, 0.f, z), hf);
				bool const hasFloor = CollisionWorld::findObjectFloorHeightBelow(Vector(x, probeY, z), fl);
				sprintf(more, "{\"h\":%s,\"full\":%s,\"floor\":%s}", buf, hasFull ? std::to_string(hf).c_str() : "null",
					hasFloor ? std::to_string(fl).c_str() : "null");
				out += (out.empty() ? "" : ",") + std::string(more);
				++n;
				continue;
			}
			out += (out.empty() ? "" : ",") + std::string(buf);
			++n;
		}
		return "[" + out + "]";
	}

	std::string objects(std::string const & request)
	{
		Object const * const player = Game::getPlayer();
		Vector const centre = player ? player->getPosition_w() : Vector::zero;
		float const radius = static_cast<float>(getInt(request, "radius", 150));
		std::string out;
		int n = 0;
		for (int list = static_cast<int>(WOL_Tangible); list <= static_cast<int>(WOL_TangibleNotTargetable) && n < 100; ++list)
		{
			int const count = World::getNumberOfObjects(list);
			for (int i = 0; i < count && n < 100; ++i)
			{
				Object const * const o = World::getObject(list, i);
				if (!o || o == player || o->getPosition_w().magnitudeBetween(centre) > radius)
					continue;
				out += (out.empty() ? "" : ",") + std::string("{\"template\":") + quoted(o->getObjectTemplateName() ? o->getObjectTemplateName() : "")
					+ ",\"id\":" + quoted(o->getNetworkId().getValueString()) + ",\"pos_w\":" + vec(o->getPosition_w())
					+ ",\"building\":" + (o->getPortalProperty() ? "true" : "false");
				// characters: the detail level drawn last frame (0 = most detailed) of how many
				SkeletalAppearance2 const * const skeletal = o->getAppearance() ? o->getAppearance()->asSkeletalAppearance2() : 0;
				if (skeletal)
				{
					char lod[48];
					sprintf(lod, ",\"lod\":%d,\"lods\":%d", skeletal->getDisplayLodIndex(), skeletal->getDetailLevelCount());
					out += lod;
				}
				out += "}";
				++n;
			}
		}
		return "[" + out + "]";
	}

	std::string exec(std::string const & line, bool & ok)
	{
		CuiConsoleHelper * const helper = CuiConsoleHelper::getActiveConsoleHelper();
		if (!helper)
		{
			ok = false;
			return "no active console (not in a scene yet?)";
		}
		std::set<Unicode::String> recursion;
		s_listener->capturing = true;
		s_listener->captured.clear();
		ok = helper->processInput(Unicode::narrowToWide(line), recursion, false);
		s_listener->capturing = false;
		return s_listener->captured;
	}

	// The offline test menu's actions (DevTestMenu): {"cmd":"menu"} lists them and the saved points;
	// {"cmd":"menu","action":"vehicle.swoop"} runs one, line by line through exec, with each line's output;
	// {"cmd":"menu","open":1} opens the in-game menu.
	std::string menu(std::string const & request, bool & ok)
	{
		ok = true;
		if (getInt(request, "open", 0))
		{
			std::string const text = exec("/devtest menu", ok);
			return "{\"opened\":" + std::string(ok ? "true" : "false") + ",\"output\":" + quoted(text) + "}";
		}
		std::string const id = getString(request, "action");
		if (id.empty())
		{
			std::string actions;
			for (int i = 0; i < DevTestMenu::getActionCount(); ++i)
			{
				DevTestMenu::Action const & action = DevTestMenu::getAction(i);
				DevTestMenu::StringVector lines;
				DevTestMenu::getLines(action, lines);
				std::string lineList;
				for (DevTestMenu::StringVector::const_iterator l = lines.begin(); l != lines.end(); ++l)
					lineList += (lineList.empty() ? "" : ",") + quoted(*l);
				actions += (actions.empty() ? "" : ",") + std::string("{\"id\":") + quoted(action.id) + ",\"category\":" + quoted(action.category)
					+ ",\"label\":" + quoted(action.label) + ",\"lines\":[" + lineList + "]}";
			}
			DevTestMenu::StringVector points;
			DevTestMenu::getPointNames(points);
			std::string pointList;
			for (DevTestMenu::StringVector::const_iterator p = points.begin(); p != points.end(); ++p)
				pointList += (pointList.empty() ? "" : ",") + quoted(*p);
			return "{\"actions\":[" + actions + "],\"points\":[" + pointList + "]}";
		}
		DevTestMenu::Action const * const action = DevTestMenu::findAction(id);
		if (!action)
		{
			ok = false;
			return quoted("unknown action '" + id + "'");
		}
		DevTestMenu::StringVector lines;
		DevTestMenu::getLines(*action, lines);
		std::string steps;
		std::string deferred;
		for (DevTestMenu::StringVector::const_iterator l = lines.begin(); l != lines.end(); ++l)
		{
			int frames = 0;
			if (DevTestMenu::isWaitLine(*l, frames))
			{
				DevTestMenu::StringVector rest;
				for (DevTestMenu::StringVector::const_iterator r = l + 1; r != lines.end(); ++r)
					rest.push_back(*r);
				DevTestMenu::defer(rest, frames);
				for (DevTestMenu::StringVector::const_iterator r = rest.begin(); r != rest.end(); ++r)
					deferred += (deferred.empty() ? "" : ",") + quoted(*r);
				break;
			}
			std::string const line = DevTestMenu::expandLine(*l);
			bool lineOk = false;
			std::string const text = exec(line, lineOk);
			ok = ok && lineOk;
			steps += (steps.empty() ? "" : ",") + std::string("{\"line\":") + quoted(line) + ",\"ok\":" + (lineOk ? "true" : "false")
				+ ",\"output\":" + quoted(text) + "}";
		}
		return "{\"action\":" + quoted(id) + ",\"steps\":[" + steps + "],\"deferred\":[" + deferred + "]}";
	}

	// Pick a radial menu item on an object, as clicking it in the radial does (the server runs the item whatever menu
	// it last sent): {"cmd":"menuselect","oid":"<network id>","item":"SERVER_MENU1"} (a menu_info_types name or number).
	std::string menuSelect(std::string const & request)
	{
		std::string const id = getString(request, "oid");
		std::string const item = getString(request, "item");
		int const type = !item.empty() && isdigit(static_cast<unsigned char>(item[0])) ? atoi(item.c_str()) : RadialMenuManager::getMenuTypeByName(item);
		if (id.empty() || type <= 0)
			return "\"ok\":false,\"result\":" + quoted("need oid and a known item: " + item);
		GameNetwork::send(ObjectMenuSelectMessage(NetworkId(id), static_cast<uint16>(type)), true);
		char buf[32];
		sprintf(buf, " (%d)", type);
		return "\"ok\":true,\"result\":" + quoted("selected " + item + buf + " on " + id);
	}

	// ------------------------------------------------------------------
	// Offline single-player scene straight from the login screen, as Load Scene / Next do there:
	// {"cmd":"loadscene","terrain":"terrain/simple.trn","avatar":"object/creature/player/shared_rodian_female.iff"}.
	std::string loadScene(std::string const & request)
	{
		std::string terrain = getString(request, "terrain");
		std::string avatar = getString(request, "avatar");
		if (terrain.empty())
			terrain = "terrain/simple.trn";
		if (avatar.empty())
			avatar = "object/creature/player/shared_human_male.iff";
		if (Game::getScene())
			return "\"ok\":false,\"result\":" + quoted("already in a scene: use /scene load <terrain> <species_sex> there");
		if (!TreeFile::exists(terrain.c_str()))
			return "\"ok\":false,\"result\":" + quoted("no such terrain: " + terrain);
		if (!TreeFile::exists(avatar.c_str()))
			return "\"ok\":false,\"result\":" + quoted("no such avatar template: " + avatar);
		Game::setScene(true, terrain.c_str(), avatar.c_str(), 0);
		return "\"ok\":true,\"result\":" + quoted("loading " + terrain + " as " + avatar);
	}

	// Aim the free camera for screenshots: {"cmd":"camera","pivot":[x,y,z],"distance":2,"yaw":180,"pitch":5}
	// orbits the world point `pivot` (yaw/pitch in degrees, yaw 0 looks along +z); {"cmd":"camera","mode":"chase"}
	// goes back to the chase camera. The modes of /devtest camera work too (DevTestTools):
	// {"mode":"free","pos":[x,y,z],"look":[x,y,z]} (look defaults to the player), {"mode":"orbit","distance":30,
	// "pitch":40,"yaw":0,"target":"last"} around the player or the look-at target, {"mode":"info"}.
	std::string camera(std::string const & request)
	{
		GroundScene * const scene = dynamic_cast<GroundScene *>(Game::getScene());
		if (!scene)
			return "\"ok\":false,\"result\":" + quoted("no ground scene");
		std::string const mode = getString(request, "mode");
		if (mode == "free" || mode == "orbit" || mode == "info")
		{
			DevTestMenu::StringVector argv;
			argv.push_back("camera");
			argv.push_back(mode);
			char buf[64];
			if (mode == "free")
			{
				Vector pos;
				if (!getVector(request, "pos", pos))
					return "\"ok\":false,\"result\":" + quoted("camera free needs pos:[x,y,z] (and look:[x,y,z])");
				Vector look;
				bool const hasLook = getVector(request, "look", look);
				float const values[6] = { pos.x, pos.y, pos.z, look.x, look.y, look.z };
				for (int i = 0; i < (hasLook ? 6 : 3); ++i)
				{
					sprintf(buf, "%.3f", values[i]);
					argv.push_back(buf);
				}
			}
			else if (mode == "orbit")
			{
				sprintf(buf, "%d", getInt(request, "distance", 30));
				argv.push_back(buf);
				sprintf(buf, "%d", getInt(request, "pitch", 30));
				argv.push_back(buf);
				std::string const target = getString(request, "target");
				if (findValue(request, "yaw") != std::string::npos || target == "last")
				{
					Object const * const player = Game::getPlayer();
					Vector const k = player ? player->getObjectFrameK_w() : Vector::unitZ;
					sprintf(buf, "%d", getInt(request, "yaw", static_cast<int>(atan2(k.x, k.z) * 57.29578f)));
					argv.push_back(buf);
				}
				if (target == "last")
					argv.push_back("last");
			}
			std::string text;
			IGNORE_RETURN(DevTestMenu::performVerb(argv, text));
			bool const ok = text.compare(0, 7, "camera ") == 0;
			return std::string("\"ok\":") + (ok ? "true" : "false") + ",\"result\":" + quoted(text);
		}
		if (mode == "chase")
		{
			scene->setView(GroundScene::CI_freeChase);
			return "\"ok\":true,\"result\":" + quoted("chase camera");
		}
		FreeCamera::Info info;
		if (!getVector(request, "pivot", info.translate))
			return "\"ok\":false,\"result\":" + quoted("camera needs pivot:[x,y,z] (or mode:chase)");
		std::string::size_type p = findValue(request, "distance");
		info.distance = p != std::string::npos ? static_cast<float>(atof(request.c_str() + p)) : 2.0f;
		p = findValue(request, "yaw");
		info.yaw = p != std::string::npos ? convertDegreesToRadians(static_cast<float>(atof(request.c_str() + p))) : 0.0f;
		p = findValue(request, "pitch");
		info.pitch = p != std::string::npos ? convertDegreesToRadians(static_cast<float>(atof(request.c_str() + p))) : 0.0f;
		if (scene->getCurrentView() != GroundScene::CI_free)
			scene->setView(GroundScene::CI_free);
		FreeCamera * const freeCamera = dynamic_cast<FreeCamera *>(scene->getCurrentCamera());
		if (!freeCamera)
			return "\"ok\":false,\"result\":" + quoted("no free camera");
		freeCamera->setMode(FreeCamera::M_pivot);
		freeCamera->setInterpolating(false);
		freeCamera->setInitializeFromFreeCamera(true);          // keep this info, don't re-frame on the player
		freeCamera->setInfo(info);
		return "\"ok\":true,\"result\":" + quoted("free camera on " + vec(info.translate));
	}

	std::string screenshot(std::string path)
	{
		if (path.empty())
		{
			char buf[64];
			sprintf(buf, "screenshots/devbridge%04d", ++s_shotNumber);
			path = buf;
		}
		std::string ext;
		switch (Graphics::getScreenShotFormat())
		{
		case GSSF_tga: ext = ".tga"; break;
		case GSSF_jpg: ext = ".jpg"; break;
		case GSSF_bmp: ext = ".bmp"; break;
		}
		std::string::size_type const slash = path.find_last_of("/\\");
		if (slash != std::string::npos)
			IGNORE_RETURN(Os::createDirectories(path.substr(0, slash + 1).c_str()));
		bool const ok = Graphics::screenShot(path.c_str());
		std::string out = std::string("{\"ok\":") + (ok ? "true" : "false") + ",\"file\":" + quoted(path + ext);
		if (!ok)
		{
			out += ",\"error\":" + quoted(Graphics::getLastError().getText());
			Graphics::clearLastError();         // reported here; Game would otherwise pop it up in a message box
		}
		return out + "}";
	}

	// ------------------------------------------------------------------
	// UI automation for interface work (client/tools/ui_modern):
	//   {"cmd":"ui","op":"tree","path":"/Opt.OptMain","depth":3}   objects under a path: type, name, visibility,
	//                                                             world rect (UI units) and the UI scale
	//   {"cmd":"ui","op":"get","path":"...","prop":"ActiveTab"} / {"op":"set",...,"value":"2"}   any property
	//   {"cmd":"ui","op":"press","path":"..."}                    press a button, or toggle a checkbox
	//   {"cmd":"ui","op":"open","value":"WS_Opt"} / "close"       activate / deactivate a mediator (/ui list names)
	//   {"cmd":"ui","op":"scale","value":125} / "fontscale" / {"op":"font","value":"Segoe UI"} ("" = stock fonts, "theme" = theme font)
	// Paths are UIManager paths, the same ones /ui set and the .inc files use.

	std::string uiObject(UIBaseObject const & object, int depth, int & budget)
	{
		std::string out = "{\"type\":" + quoted(object.GetTypeName()) + ",\"name\":" + quoted(object.GetName());
		if (object.IsA(TUIWidget))
		{
			UIWidget const & widget = static_cast<UIWidget const &>(object);
			UIRect const r = widget.GetWorldRect();
			char buf[128];
			sprintf(buf, ",\"visible\":%s,\"rect\":[%ld,%ld,%ld,%ld]", widget.IsVisible() ? "true" : "false", r.left, r.top, r.right, r.bottom);
			out += buf;
		}
		if (depth > 0)
		{
			std::string children;
			UIBaseObject::UIObjectList list;
			object.GetChildren(list);
			for (UIBaseObject::UIObjectList::const_iterator i = list.begin(); i != list.end() && budget > 0; ++i)
			{
				if ((*i)->IsA(TUIWidget) || (*i)->IsA(TUIPage))
				{
					--budget;
					children += (children.empty() ? "" : ",") + uiObject(**i, depth - 1, budget);
				}
			}
			out += ",\"children\":[" + children + "]";
		}
		return out + "}";
	}

	std::string ui(std::string const & request)
	{
		std::string const op = getString(request, "op");
		std::string const path = getString(request, "path");
		std::string const value = getString(request, "value");
		char buf[96];

		if (op == "scale" || op == "fontscale")
		{
			int const percent = getInt(request, "value", 100);
			if (op == "scale")
				CuiPreferences::setUiScalePercent(percent);
			else
				CuiPreferences::setUiFontScalePercent(percent);
			sprintf(buf, "{\"scale\":%d,\"factor\":%.3f,\"fontscale\":%d}", CuiPreferences::getUiScalePercent(), CuiPreferences::getUiScaleFactor(), CuiPreferences::getUiFontScalePercent());
			return "\"ok\":true,\"result\":" + std::string(buf);
		}
		if (op == "font")
		{
			bool ok = true;
			if (value == "theme")
				CuiDynamicUIFont::resetToThemeFont();          // what Options > Interface "(Theme font)" applies
			else
				ok = CuiDynamicUIFont::applyFontFaceUtf8(value);
			return std::string("\"ok\":") + (ok ? "true" : "false") + ",\"result\":" + quoted(CuiPreferences::getUiDefaultFontFaceUtf8());
		}
		if (op == "open" || op == "close")
		{
			std::string canonical;
			if (!CuiMediatorFactory::findMediatorCanonicalName(value, canonical))
				return "\"ok\":false,\"result\":" + quoted("no such mediator: " + value);
			bool const ok = op == "open" ? CuiMediatorFactory::activate(canonical.c_str()) != 0 : CuiMediatorFactory::deactivate(canonical.c_str());
			return std::string("\"ok\":") + (ok ? "true" : "false") + ",\"result\":" + quoted(canonical);
		}

		UIBaseObject * const object = UIManager::gUIManager().GetObjectFromPath(path.empty() ? "/" : path.c_str());
		if (!object)
			return "\"ok\":false,\"result\":" + quoted("no UI object at " + path);

		if (op == "tree")
		{
			int budget = getInt(request, "max", 400);
			sprintf(buf, ",\"scale\":%.3f}", CuiPreferences::getUiScaleFactor());
			return "\"ok\":true,\"result\":{\"object\":" + uiObject(*object, getInt(request, "depth", 2), budget) + buf;
		}
		std::string const prop = getString(request, "prop");
		if (op == "get")
		{
			// "__style": the text style object a UIText actually draws with (its logical name is shared, see
			// UITextStyleManager::GetFontForLogicalFont) and its line height
			if (prop == "__style" && object->IsA(TUIText))
			{
				UITextStyle const * const style = dynamic_cast<UITextStyle const *>(static_cast<UIText *>(object)->GetStyle());
				char buf[160];
				sprintf(buf, "%s leading=%ld", style ? style->GetName().c_str() : "(none)", style ? style->GetLeading() : 0L);
				return "\"ok\":true,\"result\":" + quoted(buf);
			}
			UIString v;
			bool const ok = object->GetProperty(UILowerString(prop), v);
			return std::string("\"ok\":") + (ok ? "true" : "false") + ",\"result\":" + quoted(Unicode::wideToNarrow(v));
		}
		if (op == "set")
		{
			bool const ok = object->SetProperty(UILowerString(prop), Unicode::narrowToWide(value));
			return std::string("\"ok\":") + (ok ? "true" : "false") + ",\"result\":" + quoted(prop + "=" + value);
		}
		if (op == "press")
		{
			if (object->IsA(TUICheckbox))
			{
				UICheckbox * const box = static_cast<UICheckbox *>(object);
				box->SetChecked(!box->IsChecked());
				return std::string("\"ok\":true,\"result\":") + (box->IsChecked() ? "\"checked\"" : "\"unchecked\"");
			}
			if (object->IsA(TUIButton))
			{
				static_cast<UIButton *>(object)->Press();
				return "\"ok\":true,\"result\":\"pressed\"";
			}
			return "\"ok\":false,\"result\":" + quoted(std::string("not a button: ") + object->GetTypeName());
		}
		return "\"ok\":false,\"result\":" + quoted("unknown ui op '" + op + "' (tree, get, set, press, open, close, scale, fontscale, font)");
	}

	std::string handle(std::string const & request)
	{
		int const id = getInt(request, "id", 0);
		std::string const cmd = getString(request, "cmd");
		char head[48];
		sprintf(head, "{\"id\":%d,", id);
		std::string out = head;
		if (cmd == "ping")
			out += "\"ok\":true,\"result\":\"pong\"";
		else if (cmd == "state")
			out += "\"ok\":true,\"result\":" + state();
		else if (cmd == "menuselect")
			out += menuSelect(request);
		else if (cmd == "exec")
		{
			bool ok = false;
			std::string const text = exec(getString(request, "line"), ok);
			out += std::string("\"ok\":") + (ok ? "true" : "false") + ",\"result\":" + quoted(text);
		}
		else if (cmd == "walk")
		{
			bool deferred = false;
			std::string const res = startWalk(request, id, deferred);
			if (deferred)
				return std::string();               // the reply goes out when the walk ends
			out += res;
		}
		else if (cmd == "settle")
		{
			Object * const player = Game::getPlayer();
			if (player)
				settle(*player);
			out += std::string("\"ok\":") + (player ? "true" : "false") + ",\"result\":" + (player ? describeObject(*player) : std::string("null"));
		}
		else if (cmd == "cellwarp")
			out += cellWarp(request);
		else if (cmd == "terrain")
			out += "\"ok\":true,\"result\":" + terrain(request);
		else if (cmd == "objects")
			out += "\"ok\":true,\"result\":" + objects(request);
		else if (cmd == "menu")
		{
			bool ok = false;
			std::string const res = menu(request, ok);
			out += std::string("\"ok\":") + (ok ? "true" : "false") + ",\"result\":" + res;
		}
		else if (cmd == "loadscene")
			out += loadScene(request);
		else if (cmd == "camera")
			out += camera(request);
		else if (cmd == "screenshot")
		{
			if (!getInt(request, "now", 0) && !s_shot.active)
			{
				s_shot.active = true;
				s_shot.id = id;
				s_shot.path = getString(request, "path");
				return std::string();               // the reply goes out from beforePresent
			}
			out += "\"ok\":true,\"result\":" + screenshot(getString(request, "path"));
		}
		else if (cmd == "ui")
			out += ui(request);
		else if (cmd == "log")
		{
			int const since = getInt(request, "since", 0);
			std::string lines;
			for (std::deque<LogLine>::const_iterator i = s_log.begin(); i != s_log.end(); ++i)
			{
				if (i->seq <= since)
					continue;
				char seq[24];
				sprintf(seq, "{\"seq\":%d,\"text\":", i->seq);
				lines += (lines.empty() ? "" : ",") + std::string(seq) + quoted(i->text) + "}";
			}
			char last[24];
			sprintf(last, "%d", s_logSeq);
			out += "\"ok\":true,\"result\":{\"last\":" + std::string(last) + ",\"lines\":[" + lines + "]}";
		}
		else
			out += "\"ok\":false,\"error\":" + quoted("unknown cmd '" + cmd + "' (ping, state, exec, walk, settle, cellwarp, objects, terrain, menu, loadscene, camera, screenshot, ui, log)");
		return out + "}\n";
	}

	// ------------------------------------------------------------------

	void closeClient()
	{
		if (s_client != INVALID_SOCKET)
			IGNORE_RETURN(closesocket(s_client));
		s_client = INVALID_SOCKET;
		s_in.clear();
	}

	void sendAll(std::string const & data)
	{
		char const * p = data.c_str();
		int left = static_cast<int>(data.size());
		int spins = 0;
		while (left > 0 && s_client != INVALID_SOCKET)
		{
			int const n = send(s_client, p, left, 0);
			if (n > 0)
			{
				p += n;
				left -= n;
			}
			else if (WSAGetLastError() == WSAEWOULDBLOCK && ++spins < 2000)
				Sleep(1);
			else
				closeClient();
		}
	}

	bool makeToken()
	{
		HCRYPTPROV prov = 0;
		unsigned char bytes[16];
		if (!CryptAcquireContextA(&prov, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
			return false;
		bool const ok = CryptGenRandom(prov, sizeof(bytes), bytes) != 0;
		IGNORE_RETURN(CryptReleaseContext(prov, 0));
		if (!ok)
			return false;
		char hex[40];
		for (int i = 0; i < 16; ++i)
			sprintf(hex + 2 * i, "%02x", bytes[i]);
		s_token = hex;
		return true;
	}

	bool writeTokenFile(int port)
	{
		char const * const appData = getenv("LOCALAPPDATA");      // per-user profile folder: not readable by other accounts
		if (!appData || !*appData)
			return false;
		std::string const dir = std::string(appData) + "\\SwgTitan";
		IGNORE_RETURN(CreateDirectoryA(dir.c_str(), 0));
		FILE * const f = fopen((dir + "\\devbridge.json").c_str(), "wb");
		if (!f)
			return false;
		fprintf(f, "{\"port\":%d,\"token\":\"%s\",\"pid\":%lu}\n", port, s_token.c_str(), GetCurrentProcessId());
		fclose(f);
		return true;
	}

	void start()
	{
		s_started = true;
		int const port = ConfigClientGame::getDevBridgePort();
		if (port <= 0)
			return;
		if (!makeToken())
		{
			WARNING(true, ("DevBridge: no random token, staying off"));
			s_failed = true;
			return;
		}
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			s_failed = true;
			return;
		}
		s_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);        // never reachable from another machine
		addr.sin_port = htons(static_cast<u_short>(port));
		u_long nonBlocking = 1;
		if (s_listen == INVALID_SOCKET || bind(s_listen, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0
			|| listen(s_listen, 1) != 0 || ioctlsocket(s_listen, FIONBIO, &nonBlocking) != 0)
		{
			WARNING(true, ("DevBridge: cannot listen on 127.0.0.1:%d", port));
			if (s_listen != INVALID_SOCKET)
				IGNORE_RETURN(closesocket(s_listen));
			s_listen = INVALID_SOCKET;
			s_failed = true;
			return;
		}
		if (!writeTokenFile(port))
		{
			WARNING(true, ("DevBridge: cannot write the token file, staying off"));
			IGNORE_RETURN(closesocket(s_listen));
			s_listen = INVALID_SOCKET;
			s_failed = true;
			return;
		}
		s_listener = new OutputListener;
		REPORT_LOG(true, ("DevBridge: listening on 127.0.0.1:%d\n", port));
	}
}

using namespace DevBridgeNamespace;

// ======================================================================

void DevBridgeNamespace::OutputListener::onOutput(CuiConsoleHelperOutputGenerated const & msg)
{
	std::string const text = Unicode::wideToNarrow(msg.str);
	addLog(text);
	if (capturing)
		captured += text;
}

#endif

// ======================================================================

void DevBridge::update()
{
#if PRODUCTION == 0
	++s_frame;
	if (!s_started)
		start();
	if (s_listen == INVALID_SOCKET)
		return;

	SOCKET const incoming = accept(s_listen, 0, 0);
	if (incoming != INVALID_SOCKET)
	{
		closeClient();                    // newest client wins
		s_client = incoming;
		u_long nonBlocking = 1;
		IGNORE_RETURN(ioctlsocket(s_client, FIONBIO, &nonBlocking));
	}
	if (s_client == INVALID_SOCKET)
		return;

	char buf[4096];
	for (;;)
	{
		int const n = recv(s_client, buf, sizeof(buf), 0);
		if (n > 0)
			s_in.append(buf, static_cast<size_t>(n));
		else
		{
			if (n == 0 || WSAGetLastError() != WSAEWOULDBLOCK)
				closeClient();
			break;
		}
	}
	if (s_walk.active)
	{
		std::string const done = stepWalk();
		if (!done.empty())
			sendAll(done);
		else
			return;                           // requests wait until the walk ends
	}
	std::string::size_type eol;
	while (!s_walk.active && s_client != INVALID_SOCKET && (eol = s_in.find('\n')) != std::string::npos)
	{
		std::string const request = s_in.substr(0, eol);
		s_in.erase(0, eol + 1);
		if (request.find_first_not_of(" \t\r") == std::string::npos)
			continue;
		if (getString(request, "token") != s_token)
		{
			sendAll("{\"ok\":false,\"error\":\"bad token\"}\n");
			closeClient();                // no second guess on this connection
			break;
		}
		std::string const reply = handle(request);
		if (!reply.empty())
			sendAll(reply);
	}
#endif
}

// ----------------------------------------------------------------------

void DevBridge::beforePresent()
{
#if PRODUCTION == 0
	if (!s_shot.active)
		return;
	s_shot.active = false;
	Graphics::setForceScreenShotBackBuffer(true, true);
	std::string const result = screenshot(s_shot.path);
	Graphics::setForceScreenShotBackBuffer(false, true);
	char head[48];
	sprintf(head, "{\"id\":%d,", s_shot.id);
	sendAll(std::string(head) + "\"ok\":true,\"result\":" + result + "}\n");
#endif
}

// ----------------------------------------------------------------------

void DevBridge::remove()
{
#if PRODUCTION == 0
	closeClient();
	if (s_listen != INVALID_SOCKET)
		IGNORE_RETURN(closesocket(s_listen));
	s_listen = INVALID_SOCKET;
	delete s_listener;
	s_listener = 0;
#endif
}

// ======================================================================
