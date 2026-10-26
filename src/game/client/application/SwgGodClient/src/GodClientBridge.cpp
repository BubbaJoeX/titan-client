// ======================================================================
//
// GodClientBridge.cpp
//
// See GodClientBridge.h. Modelled on the game client's DevBridge (clientGame):
// the same framing, token check and newest-client-wins socket, but editor
// commands, and none of DevBridge's port or token file. Polled from
// GameWidget::runGameLoop after Game::runGameLoopOnce, inside its re-entry
// guard, so a command runs between frames like a menu action and a modal
// dialog it opens pauses the bridge instead of nesting another frame.
// Listens on 127.0.0.1 only and serves one client at a time.
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "GodClientBridge.h"

#include "sharedFile/TreeFile.h"
#include "sharedFoundation/Os.h"
#include "sharedMath/Transform.h"
#include "sharedMath/Vector2d.h"
#include "sharedNetworkMessages/ConsoleChannelMessages.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/PortalProperty.h"
#include "sharedObject/World.h"

#include "clientGame/ClientObject.h"
#include "clientGame/ClientWorld.h"
#include "clientGame/FreeCamera.h"
#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientGraphics/Graphics.h"

#include "ConfigGodClient.h"
#include "GameWidget.h"
#include "GodClientData.h"
#include "GodClientTerrainEditor.h"
#include "MainFrame.h"
#include "ModificationHistory.h"
#include "ServerCommander.h"
#include "TerrainDock.h"

#include <winsock2.h>
#include <wincrypt.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <utility>
#include <vector>

// ======================================================================

namespace GodClientBridgeNamespace
{
	struct LogLine
	{
		int          seq;
		unsigned int msgId;
		std::string  text;
	};

	/// Server console replies (ConGenericMessage), as the Console dock gets them.
	class ConsoleListener : public MessageDispatch::Receiver
	{
	public:
		ConsoleListener() : MessageDispatch::Receiver()
		{
			connectToMessage("ConGenericMessage");
		}

		virtual void receiveMessage(MessageDispatch::Emitter const & source, MessageDispatch::MessageBase const & message);
	};

	/// A console request waiting for the server's reply, which carries the request's message id.
	struct ConsoleWait
	{
		bool         active;
		int          id;
		unsigned int msgId;
		DWORD        startTick;
		DWORD        timeoutMs;
		bool         answered;
		std::string  line;
		std::string  reply;
	};

	bool              s_started = false;
	SOCKET            s_listen = INVALID_SOCKET;
	SOCKET            s_client = INVALID_SOCKET;
	std::string       s_in;
	std::string       s_token;
	std::string       s_tokenFile;
	int               s_port = 0;
	ConsoleListener * s_listener = 0;
	std::deque<LogLine> s_log;
	int               s_logSeq = 0;
	int const         cs_maxLog = 500;
	unsigned int      s_frame = 0;
	int               s_shotNumber = 0;
	ConsoleWait       s_wait = { false, 0, 0, 0, 0, false, std::string(), std::string() };

	float const       cs_degrees = 57.29578f;
	int const         cs_maxListed = 500;
	size_t const      cs_maxStrokeSamples = 20000;
	DWORD const       cs_maxConsoleWaitMs = 30000;

	/// TerrainDock::ToolMode names, in enum order
	char const * const cs_toolNames[] =
	{
		"none", "raise", "lower", "flatten", "smooth", "noise", "setheight", "shader", "flora", "water", "radial",
		"ribbon", "road", "environment", "exclude", "boundary_polygon", "boundary_polyline", "boundary_polyroad",
		"stamp", "apply_environment", "select"
	};
	static_assert(sizeof(cs_toolNames) / sizeof(cs_toolNames[0]) == TerrainDock::TM_Count, "cs_toolNames must follow TerrainDock::ToolMode");

	// ------------------------------------------------------------------

	void addLog(std::string const & text, unsigned int msgId)
	{
		LogLine line;
		line.seq = ++s_logSeq;
		line.msgId = msgId;
		line.text = text;
		s_log.push_back(line);
		while (static_cast<int>(s_log.size()) > cs_maxLog)
			s_log.pop_front();
	}

	// ------------------------------------------------------------------
	// JSON helpers: requests are flat objects of strings, numbers, booleans and number arrays.

	char const * jsonBool(bool const b)
	{
		return b ? "true" : "false";
	}

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
		return p < obj.size() ? p : std::string::npos;
	}

	bool hasKey(std::string const & obj, char const * key)
	{
		std::string::size_type const p = findValue(obj, key);
		return p != std::string::npos && obj.compare(p, 4, "null") != 0;
	}

	/// The JSON string starting at obj[p] (which is the opening quote); p ends past the closing quote.
	std::string readString(std::string const & obj, std::string::size_type & p)
	{
		std::string out;
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
		++p;
		return out;
	}

	std::string getString(std::string const & obj, char const * key)
	{
		std::string::size_type p = findValue(obj, key);
		if (p == std::string::npos || obj[p] != '"')
			return std::string();
		return readString(obj, p);
	}

	int getInt(std::string const & obj, char const * key, int defaultValue)
	{
		std::string::size_type const p = findValue(obj, key);
		return p == std::string::npos ? defaultValue : atoi(obj.c_str() + p);
	}

	float getFloat(std::string const & obj, char const * key, float defaultValue)
	{
		std::string::size_type const p = findValue(obj, key);
		if (p == std::string::npos || obj.compare(p, 4, "null") == 0)
			return defaultValue;
		return static_cast<float>(atof(obj.c_str() + p));
	}

	bool getBool(std::string const & obj, char const * key, bool defaultValue)
	{
		std::string::size_type const p = findValue(obj, key);
		if (p == std::string::npos || obj.compare(p, 4, "null") == 0)
			return defaultValue;
		if (obj.compare(p, 4, "true") == 0)
			return true;
		if (obj.compare(p, 5, "false") == 0)
			return false;
		return atoi(obj.c_str() + p) != 0;
	}

	bool getVector(std::string const & obj, char const * key, Vector & v)
	{
		std::string::size_type const p = findValue(obj, key);
		if (p == std::string::npos || obj[p] != '[')
			return false;
		return sscanf(obj.c_str() + p, "[%f , %f , %f]", &v.x, &v.y, &v.z) == 3;
	}

	/// ["123", 456, ...]: network ids, as strings or numbers
	void getIdList(std::string const & obj, char const * key, std::vector<std::string> & ids)
	{
		std::string::size_type p = findValue(obj, key);
		if (p == std::string::npos || obj[p] != '[')
			return;
		for (++p; p < obj.size() && obj[p] != ']'; )
		{
			if (obj[p] == '"')
				ids.push_back(readString(obj, p));
			else if (obj[p] == '-' || (obj[p] >= '0' && obj[p] <= '9'))
			{
				std::string::size_type const end = obj.find_first_not_of("-0123456789", p);
				ids.push_back(obj.substr(p, end == std::string::npos ? std::string::npos : end - p));
				p = end == std::string::npos ? obj.size() : end;
			}
			else
				++p;
		}
	}

	/// [[x,z], [x,z], ...]: world x/z points
	bool getPoints(std::string const & obj, char const * key, std::vector<Vector2d> & points)
	{
		std::string::size_type p = findValue(obj, key);
		if (p == std::string::npos || obj[p] != '[')
			return false;
		for (++p; p < obj.size() && obj[p] != ']'; ++p)
		{
			if (obj[p] != '[')
				continue;
			float x = 0.0f;
			float z = 0.0f;
			if (sscanf(obj.c_str() + p, "[%f , %f", &x, &z) == 2)
				points.push_back(Vector2d(x, z));
			p = obj.find(']', p);
			if (p == std::string::npos)
				break;
		}
		return true;
	}

	std::string vec(Vector const & v)
	{
		char buf[96];
		sprintf(buf, "[%.3f,%.3f,%.3f]", v.x, v.y, v.z);
		return buf;
	}

	// ------------------------------------------------------------------

	/// The editing commands need a loaded ground scene with its avatar: GodClientData selects relative to the
	/// avatar's cell.
	GroundScene * getReadyScene()
	{
		GroundScene * const scene = dynamic_cast<GroundScene *>(Game::getScene());
		return scene && scene->getPlayer() && !Game::isSceneLoading() ? scene : 0;
	}

	std::string cellLabel(CellProperty const * cell)
	{
		return cell && cell != CellProperty::getWorldCellProperty() && cell->getCellName() ? cell->getCellName() : "world";
	}

	/// Position (parent space), yaw / pitch / roll in degrees as the transform command applies them (yaw, then
	/// pitch, then roll, in object space) and scale.
	std::string describeTransform(Transform const & transform, Vector const & scale)
	{
		Vector const i(transform.getLocalFrameI_p());
		Vector const j(transform.getLocalFrameJ_p());
		Vector const k(transform.getLocalFrameK_p());
		float const sinPitch = std::max(-1.0f, std::min(1.0f, -k.y));
		char buf[256];
		sprintf(buf, "\"pos_p\":%s,\"yaw\":%.2f,\"pitch\":%.2f,\"roll\":%.2f,\"scale\":%s", vec(transform.getPosition_p()).c_str(),
			atan2(k.x, k.z) * cs_degrees, asin(sinPitch) * cs_degrees, atan2(i.y, j.y) * cs_degrees, vec(scale).c_str());
		return buf;
	}

	std::string describeObject(Object const & object, Object const * ghost)
	{
		char const * const templateName = object.getObjectTemplateName();
		std::string out = "{\"id\":" + quoted(object.getNetworkId().getValueString())
			+ ",\"template\":" + quoted(templateName ? templateName : "")
			+ ",\"name\":" + quoted(object.getDebugName() ? object.getDebugName() : "");
		CellProperty const * const cell = object.getParentCell();
		out += ",\"cell\":" + quoted(cellLabel(cell));
		PortalProperty const * const portalProperty = cell && cell != CellProperty::getWorldCellProperty() ? cell->getPortalProperty() : 0;
		if (portalProperty)
			out += ",\"building_id\":" + quoted(portalProperty->getOwner().getNetworkId().getValueString());
		out += ",\"pos_w\":" + vec(object.getPosition_w()) + "," + describeTransform(object.getTransform_o2p(), object.getScale());
		if (ghost)
			out += ",\"ghost\":{\"pos_w\":" + vec(ghost->getPosition_w()) + "," + describeTransform(ghost->getTransform_o2p(), ghost->getScale()) + "}";
		else
			out += ",\"ghost\":null";
		return out + "}";
	}

	std::string selection(int const limit)
	{
		GodClientData & gcd = GodClientData::getInstance();
		GodClientData::ObjectList_t objects;
		gcd.getSelection(objects);
		std::string out;
		int n = 0;
		for (GodClientData::ObjectList_t::const_iterator it = objects.begin(); it != objects.end() && n < limit; ++it, ++n)
			out += (out.empty() ? "" : ",") + describeObject(**it, gcd.getSelectionGhost(**it));
		return "[" + out + "]";
	}

	std::string cameraInfo(GroundScene & scene)
	{
		FreeCamera * const camera = scene.getGodClientCamera();
		if (!camera)
			return "null";
		FreeCamera::Info const & info = camera->getInfo();
		char buf[256];
		sprintf(buf, "{\"pivot\":%s,\"distance\":%.3f,\"yaw\":%.2f,\"pitch\":%.2f,\"pos_w\":%s,\"active\":%s}", vec(info.translate).c_str(),
			info.distance, info.yaw * cs_degrees, info.pitch * cs_degrees, vec(camera->getPosition_w()).c_str(),
			jsonBool(scene.getCurrentCamera() == camera));
		return buf;
	}

	std::string terrainInfo()
	{
		MainFrame * const mainFrame = MainFrame::getInstanceNullable();
		TerrainDock * const dock = mainFrame ? mainFrame->getTerrainDock() : 0;
		if (!dock)
			return "null";
		GodClientTerrainEditor const * const editor = GodClientTerrainEditor::getInstanceNullable();
		int const tool = static_cast<int>(dock->getToolMode());
		char buf[320];
		sprintf(buf, "{\"tool\":\"%s\",\"terrain_loaded\":%s,\"editing\":%s,\"brush_size\":%.1f,\"brush_strength\":%.2f,\"can_undo\":%s,\"can_redo\":%s}",
			tool >= 0 && tool < static_cast<int>(TerrainDock::TM_Count) ? cs_toolNames[tool] : "?", jsonBool(dock->hasActiveTerrain()),
			jsonBool(dock->isTerrainEditingActive()), dock->getBrushSize(), dock->getBrushStrength(),
			jsonBool(editor && editor->canUndo()), jsonBool(editor && editor->canRedo()));
		return buf;
	}

	std::string state()
	{
		char buf[192];
		sprintf(buf, "{\"frame\":%u,\"port\":%d,\"single_player\":%s,\"scene_loading\":%s", s_frame, s_port,
			jsonBool(Game::getSinglePlayer()), jsonBool(Game::isSceneLoading()));
		std::string out(buf);
		out += ",\"scene\":" + quoted(Game::getSceneId());
		GroundScene * const scene = getReadyScene();
		if (!scene)
			return out + ",\"ready\":false}";

		GodClientData & gcd = GodClientData::getInstance();
		Object const * const player = scene->getPlayer();
		out += ",\"ready\":true,\"camera\":" + cameraInfo(*scene) + ",\"cursor_w\":" + vec(gcd.getMouseCursorIntersection());
		out += ",\"player\":{\"pos_w\":" + vec(player->getPosition_w()) + ",\"cell\":" + quoted(cellLabel(player->getParentCell())) + "}";
		sprintf(buf, ",\"selection_count\":%d,\"ghosts\":%d", gcd.getSelectionSize(), gcd.countGhosts());
		out += buf;
		out += ",\"selection\":" + selection(50);
		std::string const copied(gcd.describeCopiedTransform());
		out += ",\"copied_transform\":" + (copied.empty() ? std::string("null") : quoted(copied));
		sprintf(buf, ",\"drop_to_terrain\":%s,\"align_to_terrain\":%s,\"transform_space\":\"%s\"", jsonBool(gcd.isToggleDropToTerrainOn()),
			jsonBool(gcd.isToggleAlignToTerrainOn()), gcd.isTransformSpaceLocal() ? "local" : "world");
		out += buf;
		ModificationHistory const & history = ModificationHistory::getInstance();
		sprintf(buf, ",\"undo\":{\"objects\":{\"can_undo\":%s,\"can_redo\":%s},\"terrain\":", jsonBool(history.canUndo()), jsonBool(history.canRedo()));
		out += buf + terrainInfo() + "}";
		return out + "}";
	}

	// ------------------------------------------------------------------

	/// Loaded objects nearest a world point (default: the camera pivot), optionally only templates containing a
	/// substring: {"center":[x,y,z],"radius":64,"template":"chair","limit":100}
	std::string objects(std::string const & request)
	{
		GodClientData & gcd = GodClientData::getInstance();
		Vector centre;
		if (!getVector(request, "center", centre))
			centre = gcd.getCameraPivotPoint();
		float const radius = getFloat(request, "radius", 64.0f);
		std::string const filter(getString(request, "template"));
		int const limit = std::max(1, std::min(cs_maxListed, getInt(request, "limit", 100)));

		typedef std::pair<float, Object const *> Hit;
		std::vector<Hit> hits;
		int const lists[] = { static_cast<int>(WOL_Tangible), static_cast<int>(WOL_TangibleNotTargetable) };
		for (size_t l = 0; l < sizeof(lists) / sizeof(lists[0]); ++l)
		{
			for (int i = 0; i < ClientWorld::getNumberOfObjects(lists[l]); ++i)
			{
				Object const * const o = ClientWorld::getObject(lists[l], i);
				if (!o)
					continue;
				float const distance = o->getPosition_w().magnitudeBetween(centre);
				char const * const name = o->getObjectTemplateName();
				if (distance > radius || (!filter.empty() && (!name || !strstr(name, filter.c_str()))))
					continue;
				bool isSelection = false;
				bool isGhost = false;
				IGNORE_RETURN(gcd.getIsSelectionOrGhost(*o, isSelection, isGhost));
				if (!isGhost)
					hits.push_back(Hit(distance, o));
			}
		}
		std::sort(hits.begin(), hits.end());

		std::string out;
		for (size_t h = 0; h < hits.size() && static_cast<int>(h) < limit; ++h)
		{
			Object const & o = *hits[h].second;
			bool isSelection = false;
			bool isGhost = false;
			IGNORE_RETURN(gcd.getIsSelectionOrGhost(o, isSelection, isGhost));
			char buf[64];
			sprintf(buf, ",\"distance\":%.2f,\"selected\":%s", hits[h].first, jsonBool(isSelection));
			out += (out.empty() ? "" : ",") + std::string("{\"id\":") + quoted(o.getNetworkId().getValueString())
				+ ",\"template\":" + quoted(o.getObjectTemplateName() ? o.getObjectTemplateName() : "")
				+ ",\"pos_w\":" + vec(o.getPosition_w()) + ",\"cell\":" + quoted(cellLabel(o.getParentCell())) + buf
				+ ",\"building\":" + jsonBool(o.getPortalProperty() != 0) + "}";
		}
		char head[64];
		sprintf(head, "{\"count\":%d,\"objects\":[", static_cast<int>(hits.size()));
		return head + out + "]}";
	}

	// ------------------------------------------------------------------

	std::string selectObjects(std::string const & request, bool & ok)
	{
		GodClientData & gcd = GodClientData::getInstance();
		bool const clear = getBool(request, "clear", false);
		bool const add = getBool(request, "add", false);
		std::vector<std::string> ids;
		getIdList(request, "ids", ids);
		std::string const templateName(getString(request, "template"));
		ok = false;
		if (ids.empty() && templateName.empty() && !clear)
			return quoted("select needs ids:[...], template:\"object/....iff\" or clear:1");

		if (clear || !add)
			gcd.clearSelection();

		std::string rejected;
		for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++it)
		{
			ClientObject * const obj = dynamic_cast<ClientObject *>(NetworkIdManager::getObjectById(NetworkId(*it)));
			char const * reason = 0;
			if (!obj)
				reason = "not loaded";
			else if (!obj->getAppearance())
				reason = "no appearance (a cell or planet object)";
			else
			{
				gcd.addSelection(obj);
				bool isSelection = false;
				bool isGhost = false;
				IGNORE_RETURN(gcd.getIsSelectionOrGhost(*obj, isSelection, isGhost));
				if (!isSelection)
					reason = "in another cell than the avatar";
			}
			if (reason)
				rejected += (rejected.empty() ? "" : ",") + std::string("{\"id\":") + quoted(*it) + ",\"reason\":" + quoted(reason) + "}";
		}
		if (!templateName.empty())
			IGNORE_RETURN(gcd.selectObjectsWithTemplate(templateName.c_str()));

		ok = true;
		char head[48];
		sprintf(head, "{\"count\":%d,\"rejected\":[", gcd.getSelectionSize());
		return head + rejected + "],\"selection\":" + selection(cs_maxListed) + "}";
	}

	// ------------------------------------------------------------------

	/// Place a template in the world cell: {"template":"object/...iff","pos":[x,y,z],"yaw":90,"select":1}. Offline
	/// the object exists at once; connected, the server creates it and it is selected when it arrives.
	std::string create(std::string const & request, bool & ok)
	{
		ok = false;
		std::string const templateName(getString(request, "template"));
		if (templateName.size() < 12 || templateName.compare(0, 7, "object/") != 0 || templateName.compare(templateName.size() - 4, 4, ".iff") != 0)
			return quoted("create needs template:\"object/....iff\" (shared_ for a client object, server template for a buildout)");
		Vector position;
		if (!getVector(request, "pos", position))
			return quoted("create needs pos:[x,y,z] (world)");
		// ServerCommander opens a message box for a shared template it cannot load
		if (templateName.find("/shared_") != std::string::npos && !TreeFile::exists(templateName.c_str()))
			return quoted("no such template on the search path: " + templateName);

		Transform transform;
		transform.yaw_l(getFloat(request, "yaw", 0.0f) / cs_degrees);
		transform.setPosition_p(position);

		bool const selectIt = getBool(request, "select", true);
		GodClientData & gcd = GodClientData::getInstance();
		gcd.setObjectCreationPending(selectIt);           // ServerObjectData selects the next object that arrives
		ClientObject * const obj = dynamic_cast<ClientObject *>(GameWidget::createObjectFromTemplateWithTransform(templateName, CellProperty::getWorldCellProperty(), transform));
		if (obj)
		{
			if (selectIt)
				gcd.setSelection(obj);
			ok = true;
			return "{\"mode\":\"local\",\"object\":" + describeObject(*obj, 0) + "}";
		}
		if (!Game::getSinglePlayer())
		{
			ok = true;
			return "{\"mode\":\"server\",\"note\":" + quoted(selectIt ? "sent to the server; the object is selected when it arrives (poll selection)"
				: "sent to the server; find it with objects") + "}";
		}
		gcd.setObjectCreationPending(false);
		return quoted("not created (see the Console dock)");
	}

	// ------------------------------------------------------------------

	/// Ghost every selected object and set parts of its transform, as the Set Transform dialog and the transform
	/// clipboard do; "apply" then moves the objects (Ctrl+T).
	std::string transformSelection(std::string const & request, bool & ok)
	{
		GodClientData & gcd = GodClientData::getInstance();
		ok = false;
		if (gcd.getSelectionEmpty())
			return quoted("nothing selected");

		if (getBool(request, "copy", false) && !gcd.copySelectionTransform())
			return quoted("cannot copy the primary selection's transform");

		std::string const paste(getString(request, "paste"));
		if (!paste.empty())
		{
			bool const all = paste == "all";
			if (!all && paste != "position" && paste != "rotation")
				return quoted("paste is position, rotation or all");
			if (!gcd.hasCopiedTransform())
				return quoted("no copied transform: send copy:1 with the source object selected first");
			IGNORE_RETURN(gcd.pasteSelectionTransform(all || paste == "position", all || paste == "rotation", all));
		}

		Vector position;
		if (getVector(request, "pos", position))
		{
			gcd.setSelectionX(position.x);
			gcd.setSelectionY(position.y);
			gcd.setSelectionZ(position.z);
		}

		if (hasKey(request, "yaw") || hasKey(request, "pitch") || hasKey(request, "roll"))
		{
			gcd.resetGhostsRotations();
			gcd.setSelectionYaw(getFloat(request, "yaw", 0.0f) / cs_degrees);
			gcd.setSelectionPitch(getFloat(request, "pitch", 0.0f) / cs_degrees);
			gcd.setSelectionRoll(getFloat(request, "roll", 0.0f) / cs_degrees);
		}

		if (hasKey(request, "scale"))
		{
			Vector scale;
			if (!getVector(request, "scale", scale))
				scale.x = scale.y = scale.z = getFloat(request, "scale", 1.0f);
			gcd.setSelectionScaleX(scale.x);
			gcd.setSelectionScaleY(scale.y);
			gcd.setSelectionScaleZ(scale.z);
		}

		bool const apply = getBool(request, "apply", false);
		if (apply)
			gcd.synchronizeSelectionWithGhosts();          // ActionsEdit::onApplyTransform

		ok = true;
		return std::string("{\"applied\":") + jsonBool(apply) + ",\"selection\":" + selection(cs_maxListed) + "}";
	}

	// ------------------------------------------------------------------

	/// Ctrl+Z / Ctrl+Y: "auto" undoes terrain strokes first while a terrain tool is armed, then object edits.
	std::string undoRedo(std::string const & request, bool const redo, bool & ok)
	{
		std::string target(getString(request, "target"));
		if (target.empty())
			target = "auto";
		ok = false;
		if (target != "auto" && target != "objects" && target != "terrain")
			return quoted("target is auto, objects or terrain");

		MainFrame * const mainFrame = MainFrame::getInstanceNullable();
		TerrainDock * const dock = mainFrame ? mainFrame->getTerrainDock() : 0;
		if (target == "terrain")
		{
			GodClientTerrainEditor const * const editor = GodClientTerrainEditor::getInstanceNullable();
			if (!dock || !editor || !(redo ? editor->canRedo() : editor->canUndo()))
				return quoted(redo ? "no terrain stroke to redo" : "no terrain stroke to undo");
			if (redo)
				dock->onRedo();
			else
				dock->onUndo();
			ok = true;
			return "{\"done\":\"terrain\"}";
		}
		if (target == "auto" && dock && (redo ? dock->tryConsumeRedoShortcut() : dock->tryConsumeUndoShortcut()))
		{
			ok = true;
			return "{\"done\":\"terrain\"}";
		}
		ModificationHistory & history = ModificationHistory::getInstance();
		if (redo ? !history.canRedo() : !history.canUndo())
			return quoted(redo ? "nothing to redo" : "nothing to undo");
		ok = redo ? history.redo() : history.undo();
		return ok ? "{\"done\":\"objects\"}" : quoted("the modification history refused");
	}

	// ------------------------------------------------------------------

	/// A Console dock line: {"line":"object get 123","wait":5}. Server console replies carry the request's message
	/// id, so with "wait" the reply goes out when it arrives (or the wait runs out); "/name args" is a slash command
	/// on "target" (default: the primary selection), which has no reply.
	std::string console(std::string const & request, int const id, bool & deferred, bool & ok)
	{
		deferred = false;
		ok = false;
		std::string const line(getString(request, "line"));
		if (line.empty())
			return quoted("console needs line");

		NetworkId target(NetworkId::cms_invalid);
		std::string const targetId(getString(request, "target"));
		if (!targetId.empty())
			target = NetworkId(targetId);
		else
		{
			GodClientData::ObjectList_t objects;
			GodClientData::getInstance().getSelection(objects);
			if (!objects.empty())
				target = objects.front()->getNetworkId();
		}

		addLog("> " + line, 0);
		unsigned int const msgId = ServerCommander::getInstance().consoleCommand(line, target);
		bool const slash = line[0] == '/';
		float const wait = getFloat(request, "wait", 0.0f);
		ok = true;
		if (!slash && wait > 0.0f && !Game::getSinglePlayer())
		{
			s_wait.active = true;
			s_wait.id = id;
			s_wait.msgId = msgId;
			s_wait.startTick = GetTickCount();
			s_wait.timeoutMs = std::min(cs_maxConsoleWaitMs, static_cast<DWORD>(wait * 1000.0f));
			s_wait.answered = false;
			s_wait.line = line;
			s_wait.reply.clear();
			deferred = true;
			return std::string();
		}
		char buf[64];
		sprintf(buf, ",\"message_id\":%u,\"slash\":%s}", msgId, jsonBool(slash));
		return "{\"line\":" + quoted(line) + buf;
	}

	/// The reply to the waiting console request once the server answered or the wait ran out, else empty.
	std::string finishConsoleWait()
	{
		bool const timedOut = GetTickCount() - s_wait.startTick >= s_wait.timeoutMs;
		if (!s_wait.answered && !timedOut)
			return std::string();
		s_wait.active = false;
		char head[96];
		sprintf(head, "{\"id\":%d,\"ok\":%s,\"result\":{\"message_id\":%u,\"answered\":%s,\"line\":", s_wait.id, jsonBool(s_wait.answered),
			s_wait.msgId, jsonBool(s_wait.answered));
		return head + quoted(s_wait.line) + ",\"reply\":" + quoted(s_wait.reply) + "}}\n";
	}

	// ------------------------------------------------------------------

	/// God camera (FreeCamera pivot mode): any of pivot [x,y,z], distance, yaw / pitch in degrees; "smooth"
	/// interpolates like a bookmark jump.
	std::string camera(std::string const & request, GroundScene & scene, bool & ok)
	{
		ok = false;
		FreeCamera * const freeCamera = scene.getGodClientCamera();
		if (!freeCamera)
			return quoted("no god camera");
		bool const set = hasKey(request, "pivot") || hasKey(request, "distance") || hasKey(request, "yaw") || hasKey(request, "pitch");
		if (set)
		{
			FreeCamera::Info info(freeCamera->getInfo());
			Vector pivot;
			if (getVector(request, "pivot", pivot))
				info.translate = pivot;
			info.distance = std::max(0.1f, getFloat(request, "distance", info.distance));
			info.yaw = getFloat(request, "yaw", info.yaw * cs_degrees) / cs_degrees;
			info.pitch = getFloat(request, "pitch", info.pitch * cs_degrees) / cs_degrees;
			scene.activateGodClientCamera();
			freeCamera->setMode(FreeCamera::M_pivot);
			if (getBool(request, "smooth", false))
			{
				freeCamera->setTargetInfo(info, CONST_REAL(1));
				freeCamera->setInterpolating(true);
			}
			else
			{
				freeCamera->setInterpolating(false);
				freeCamera->setInfo(info);
			}
		}
		ok = true;
		return cameraInfo(scene);                         // pos_w follows on the next frame
	}

	// ------------------------------------------------------------------

	/// Offline single-player scene from the login screen, as its Load Scene (Local) button does, so editing works
	/// without a server: {"terrain":"terrain/tatooine.trn","avatar":"object/creature/player/shared_human_male.iff"}.
	std::string loadScene(std::string const & request, bool & ok)
	{
		ok = false;
		std::string terrain(getString(request, "terrain"));
		std::string avatar(getString(request, "avatar"));
		if (terrain.empty())
			terrain = "terrain/simple.trn";
		if (avatar.empty())
			avatar = "object/creature/player/shared_human_male.iff";
		if (Game::getScene())
			return quoted("already in a scene: restart the God Client to load another offline");
		if (!TreeFile::exists(terrain.c_str()))
			return quoted("no such terrain: " + terrain);
		if (!TreeFile::exists(avatar.c_str()))
			return quoted("no such avatar template: " + avatar);
		Game::setSinglePlayer(true);
		Game::setScene(true, terrain.c_str(), avatar.c_str(), 0);
		ok = true;
		return quoted("loading " + terrain + " as " + avatar + " (poll state until ready)");
	}

	// ------------------------------------------------------------------

	std::string screenshot(std::string path)
	{
		if (path.empty())
		{
			char buf[64];
			sprintf(buf, "screenshots/godbridge%04d", ++s_shotNumber);
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

		// The 3D view only. The default front-buffer copy takes the desktop at the editor frame's corner (menus, docks,
		// whatever lies over it); a windowed device swaps with D3DSWAPEFFECT_COPY, so the back buffer still holds the
		// frame just presented. The front buffer stays the fallback for a back buffer format it cannot save.
		Graphics::setForceScreenShotBackBuffer(true, true);
		bool const backBuffer = Graphics::screenShot(path.c_str());
		Graphics::setForceScreenShotBackBuffer(false, true);
		bool const ok = backBuffer || Graphics::screenShot(path.c_str());

		char full[MAX_PATH];
		std::string const file(path + ext);
		DWORD const n = GetFullPathNameA(file.c_str(), sizeof(full), full, 0);
		std::string out = std::string("{\"ok\":") + jsonBool(ok) + ",\"file\":" + quoted(n > 0 && n < sizeof(full) ? std::string(full) : file)
			+ ",\"source\":" + (backBuffer ? "\"back_buffer\"" : "\"front_buffer\"");
		if (!ok)
			out += ",\"error\":" + quoted(Graphics::getLastError().getText());
		return out + "}";
	}

	// ------------------------------------------------------------------

	/// Points every \a spacing metres along the polyline, so a two-point line strokes like a drag.
	void resample(std::vector<Vector2d> const & points, float const spacing, std::vector<Vector2d> & samples)
	{
		samples.push_back(points.front());
		for (size_t i = 1; i < points.size() && samples.size() < cs_maxStrokeSamples; ++i)
		{
			Vector2d const a(points[i - 1]);
			Vector2d const step(points[i] - a);
			int const count = std::max(1, static_cast<int>(ceil(step.magnitude() / spacing)));
			for (int s = 1; s <= count && samples.size() < cs_maxStrokeSamples; ++s)
				samples.push_back(a + step * (static_cast<float>(s) / static_cast<float>(count)));
		}
	}

	/// One terrain brush stroke: {"tool":"raise","points":[[x,z],...],"size":16,"strength":0.5,"spacing":4}.
	/// tool / size / strength go on the Terrain dock (omitted: what it has); raise and lower add their speed per dab.
	std::string terrainStroke(std::string const & request, bool & ok)
	{
		ok = false;
		MainFrame * const mainFrame = MainFrame::getInstanceNullable();
		TerrainDock * const dock = mainFrame ? mainFrame->getTerrainDock() : 0;
		if (!dock)
			return quoted("no terrain dock");

		TerrainDock::ToolMode tool = TerrainDock::TM_None;
		std::string const toolName(getString(request, "tool"));
		if (!toolName.empty())
		{
			int index = 1;
			while (index < static_cast<int>(TerrainDock::TM_Count) && toolName != cs_toolNames[index])
				++index;
			if (index == static_cast<int>(TerrainDock::TM_Count))
				return quoted("unknown tool '" + toolName + "' (raise, lower, flatten, smooth, noise, setheight, shader, flora, water, radial)");
			tool = static_cast<TerrainDock::ToolMode>(index);
		}

		std::vector<Vector2d> points;
		if (!getPoints(request, "points", points) || points.empty())
			return quoted("terrain_stroke needs points:[[x,z],...] (world)");

		float const size = getFloat(request, "size", 0.0f);
		float const strength = getFloat(request, "strength", 0.0f);
		float spacing = getFloat(request, "spacing", 0.0f);
		if (spacing <= 0.0f)
			spacing = std::max(0.5f, (size > 0.0f ? size : dock->getBrushSize()) * 0.25f);
		std::vector<Vector2d> samples;
		resample(points, std::max(0.1f, spacing), samples);

		std::string error;
		if (!dock->scriptedBrushStroke(tool, size, strength, samples, error))
			return quoted(error);

		ok = true;
		char buf[96];
		sprintf(buf, "{\"dabs\":%d,\"spacing\":%.2f,\"terrain\":", static_cast<int>(samples.size()), spacing);
		return buf + terrainInfo() + "}";
	}

	// ------------------------------------------------------------------

	std::string consoleLog(std::string const & request)
	{
		int const since = getInt(request, "since", 0);
		std::string lines;
		for (std::deque<LogLine>::const_iterator i = s_log.begin(); i != s_log.end(); ++i)
		{
			if (i->seq <= since)
				continue;
			char head[64];
			sprintf(head, "{\"seq\":%d,\"message_id\":%u,\"text\":", i->seq, i->msgId);
			lines += (lines.empty() ? "" : ",") + std::string(head) + quoted(i->text) + "}";
		}
		char last[32];
		sprintf(last, "{\"last\":%d,\"lines\":[", s_logSeq);
		return last + lines + "]}";
	}

	// ------------------------------------------------------------------

	std::string answer(bool const ok, std::string const & result)
	{
		return std::string("\"ok\":") + jsonBool(ok) + ",\"result\":" + result;
	}

	/// The reply line, or empty when it is deferred (a console request waiting for its server reply).
	std::string handle(std::string const & request)
	{
		int const id = getInt(request, "id", 0);
		std::string const cmd = getString(request, "cmd");
		char head[48];
		sprintf(head, "{\"id\":%d,", id);
		std::string out = head;
		bool ok = false;

		if (cmd == "ping")
			out += answer(true, "\"pong\"");
		else if (cmd == "state")
			out += answer(true, state());
		else if (cmd == "log")
			out += answer(true, consoleLog(request));
		else if (cmd == "screenshot")
			out += answer(true, screenshot(getString(request, "path")));
		else if (cmd == "loadscene")
		{
			std::string const result = loadScene(request, ok);
			out += answer(ok, result);
		}
		else if (cmd == "console")
		{
			bool deferred = false;
			std::string const result = console(request, id, deferred, ok);
			if (deferred)
				return std::string();
			out += answer(ok, result);
		}
		else if (cmd == "undo" || cmd == "redo")
		{
			std::string const result = undoRedo(request, cmd == "redo", ok);
			out += answer(ok, result);
		}
		else if (cmd == "selection" || cmd == "select" || cmd == "objects" || cmd == "create" || cmd == "transform" || cmd == "apply"
			|| cmd == "camera" || cmd == "terrain_stroke")
		{
			GroundScene * const scene = getReadyScene();
			if (!scene)
				out += answer(false, quoted("no scene loaded yet"));
			else if (cmd == "selection")
				out += answer(true, selection(cs_maxListed));
			else if (cmd == "objects")
				out += answer(true, objects(request));
			else if (cmd == "apply")
			{
				GodClientData::getInstance().synchronizeSelectionWithGhosts();
				out += answer(true, selection(cs_maxListed));
			}
			else
			{
				std::string result;
				if (cmd == "select")
					result = selectObjects(request, ok);
				else if (cmd == "create")
					result = create(request, ok);
				else if (cmd == "transform")
					result = transformSelection(request, ok);
				else if (cmd == "camera")
					result = camera(request, *scene, ok);
				else
					result = terrainStroke(request, ok);
				out += answer(ok, result);
			}
		}
		else
			out += "\"ok\":false,\"error\":" + quoted("unknown cmd '" + cmd + "' (ping, state, loadscene, selection, select, objects, create, transform, "
				"apply, undo, redo, console, log, camera, screenshot, terrain_stroke)");
		return out + "}\n";
	}

	// ------------------------------------------------------------------

	void closeClient()
	{
		if (s_client != INVALID_SOCKET)
			IGNORE_RETURN(closesocket(s_client));
		s_client = INVALID_SOCKET;
		s_in.clear();
		s_wait.active = false;                        // its reply has nowhere to go
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

	/// %LOCALAPPDATA%\SwgTitan\godclient_bridge.json: never devbridge.json, which game-client tools pin tokens from
	bool writeTokenFile(int port)
	{
		char const * const appData = getenv("LOCALAPPDATA");      // per-user profile folder: not readable by other accounts
		if (!appData || !*appData)
			return false;
		std::string const dir = std::string(appData) + "\\SwgTitan";
		IGNORE_RETURN(CreateDirectoryA(dir.c_str(), 0));
		s_tokenFile = dir + "\\godclient_bridge.json";
		FILE * const f = fopen(s_tokenFile.c_str(), "wb");
		if (!f)
			return false;
		fprintf(f, "{\"port\":%d,\"token\":\"%s\",\"pid\":%lu}\n", port, s_token.c_str(), GetCurrentProcessId());
		fclose(f);
		return true;
	}

	void start()
	{
		s_started = true;
		int const port = ConfigGodClient::getData().bridgePort;
		if (port <= 0)
			return;
		if (!makeToken())
		{
			WARNING(true, ("GodClientBridge: no random token, staying off"));
			return;
		}
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			return;
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
			WARNING(true, ("GodClientBridge: cannot listen on 127.0.0.1:%d", port));
			if (s_listen != INVALID_SOCKET)
				IGNORE_RETURN(closesocket(s_listen));
			s_listen = INVALID_SOCKET;
			return;
		}
		if (!writeTokenFile(port))
		{
			WARNING(true, ("GodClientBridge: cannot write the token file, staying off"));
			IGNORE_RETURN(closesocket(s_listen));
			s_listen = INVALID_SOCKET;
			return;
		}
		s_port = port;
		s_listener = new ConsoleListener;

		char buf[96];
		sprintf(buf, "God Client bridge: listening on 127.0.0.1:%d", port);
		REPORT_LOG(true, ("%s\n", buf));
		MainFrame * const mainFrame = MainFrame::getInstanceNullable();
		if (mainFrame)
			mainFrame->textToConsole(buf);
	}
}

using namespace GodClientBridgeNamespace;

// ======================================================================

void GodClientBridgeNamespace::ConsoleListener::receiveMessage(MessageDispatch::Emitter const & source, MessageDispatch::MessageBase const & message)
{
	UNREF(source);
	if (!message.isType("ConGenericMessage"))
		return;
	Archive::ReadIterator ri = dynamic_cast<GameNetworkMessage const &>(message).getByteStream().begin();
	ConGenericMessage const cg(ri);
	addLog(cg.getMsg(), cg.getMsgId());
	if (s_wait.active && cg.getMsgId() == s_wait.msgId)
	{
		s_wait.reply += cg.getMsg();
		s_wait.answered = true;
	}
}

// ======================================================================

void GodClientBridge::update()
{
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
	if (s_wait.active)
	{
		std::string const done = finishConsoleWait();
		if (done.empty())
			return;                           // requests wait until the console reply is in
		sendAll(done);
	}
	std::string::size_type eol;
	while (!s_wait.active && s_client != INVALID_SOCKET && (eol = s_in.find('\n')) != std::string::npos)
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
}

// ----------------------------------------------------------------------

void GodClientBridge::remove()
{
	closeClient();
	if (s_listen != INVALID_SOCKET)
	{
		IGNORE_RETURN(closesocket(s_listen));
		IGNORE_RETURN(DeleteFileA(s_tokenFile.c_str()));   // only the listening editor wrote it
	}
	s_listen = INVALID_SOCKET;
	s_wait.active = false;
	delete s_listener;
	s_listener = 0;
}

// ======================================================================
