// ======================================================================
//
// SwgCuiCommandParserDevTest.cpp
//
// See SwgCuiCommandParserDevTest.h. The menus are UIPopupMenus built in code,
// so they need no .ui data: the main menu (one submenu per DevTestMenu
// category, plus the saved warp points), the template picker and the
// animation browser. Every entry is a console line, run through the active
// console exactly as if it was typed in chat.
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiCommandParserDevTest.h"

#if PRODUCTION == 0

#include "clientGame/DevTestMenu.h"
#include "clientGame/DevTestTools.h"
#include "clientUserInterface/CuiConsoleHelper.h"
#include "UIDataSource.h"
#include "UIDataSourceContainer.h"
#include "UIEventCallback.h"
#include "UIManager.h"
#include "UIPage.h"
#include "UIPopupMenu.h"
#include "UnicodeUtils.h"

#include <set>

// ======================================================================

namespace SwgCuiCommandParserDevTestNamespace
{
	typedef CommandParser::StringVector_t StringVector;
	typedef CommandParser::String_t       String;

	CommandParser::CmdInfo const cs_commands[] =
	{
		{"menu",     0, "",                                   "Open the offline test menu"},
		{"list",     0, "",                                   "List the menu actions"},
		{"run",      1, "<action>",                           "Run a menu action, e.g. vehicle.swoop"},
		{"spawn",    1, "<template> [dist] [height] [mount]", "Spawn ahead of the player ($last = its id)"},
		{"spawns",   0, "",                                   "List what /devtest spawned"},
		{"clear",    0, "",                                   "Delete what /devtest spawned"},
		{"target",   1, "<last|clear|id>",                    "Set the look-at target"},
		{"mount",    1, "<last|id>",                          "Ride a creature vehicle"},
		{"dismount", 0, "",                                   "Leave the vehicle or ship"},
		{"animstop", 0, "",                                   "Stop the loop track on the target or player"},
		{"anim",     1, "<name|path> [loop|action] [once]",   "Play an animation on the player"},
		{"point",    1, "<save|go|delete> <name> | list",     "Saved warp points"},
		{"goto",     2, "<x> <z> [above]",                    "Warp onto the terrain"},
		{"hop",      1, "<metres>",                           "Move the player (or their ship) up"},
		{"flag",     2, "<section> <name> [0|1|toggle]",      "Set a debug flag"},
		{"wait",     1, "<frames>",                           "In an action: run the lines after it later"},
		{"spawnat",  3, "<template> <x> <z> [heading] [height] [mount]", "Spawn at a world point"},
		{"ensure",   3, "<template> <x> <z> [heading]",       "spawnat unless one is already within 100 m"},
		{"camera",   0, "free <x> <y> <z> [<lx> <ly> <lz>] | orbit <dist> <pitch> [yaw] [last] | chase | info", "Place the camera"},
		{"telemetry",0, "on|off|once",                        "Speed and altitude of what you ride or fly"},
		{"pick",     0, "<filter words> [page]",              "Pick a template to spawn"},
		{"anims",    0, "[prefix|target] [page] [loop|once]", "Browse animations by species"},
		{"animspecies", 0, "",                                "Pick an animation species to browse"},
		{"custom",   0, "...",                                "Customization variables of the target or player"},
		{"screentex",2, "<last|id> <texture>",                "Put a texture on the screen/canvas surfaces (video path)"},
		{"rtfeed",   2, "<camera last|id> <screen id> [fov]", "Show an RT camera feed on a screen"},
		{"vessel",   2, "<last|id> hpdyn|fly|stop|info ...",  "Boardable vessel: hp_dyn parts, client-side flight"},
		{"",         0, "",                                   ""} // this must be last
	};

	// ------------------------------------------------------------------

	class MenuCallback : public UIEventCallback
	{
	public:
		virtual void OnPopupMenuSelection(UIWidget * context);
	};

	MenuCallback s_menuCallback;

	// ------------------------------------------------------------------

	// a menu entry: a console line, or a submenu
	struct Node
	{
		std::string       label;
		std::string       command;
		std::vector<Node> children;
	};

	// the console lines of the open menus; an entry's UI name is "c<index>" (names cannot hold the line itself)
	std::vector<std::string> s_commands;

	Node makeLeaf(std::string const & label, std::string const & command)
	{
		Node node;
		node.label = label;
		node.command = command;
		return node;
	}

	UIDataSource * addLeaf(UIDataSourceContainer & container, std::string const & name, std::string const & label)
	{
		UIDataSource * const ds = new UIDataSource;
		ds->SetName(name);
		ds->SetProperty(UIPopupMenu::DataProperties::Text, Unicode::narrowToWide(label));
		ds->SetProperty(UIPopupMenu::DataProperties::LocalText, Unicode::narrowToWide(label));
		container.AddChild(ds);
		return ds;
	}

	bool runLine(std::string const & line)
	{
		CuiConsoleHelper * const helper = CuiConsoleHelper::getActiveConsoleHelper();
		if (!helper)
			return false;
		std::set<Unicode::String> recursion;
		return helper->processInput(Unicode::narrowToWide(DevTestMenu::expandLine(line)), recursion, false);
	}

	void runSelection(std::string const & name)
	{
		if (name.size() < 2 || name[0] != 'c')
			return;
		int const index = atoi(name.c_str() + 1);
		if (index >= 0 && index < static_cast<int>(s_commands.size()))
		{
			// next frame, outside the popup's own callback: the line may open a new menu, which closes this one
			DevTestMenu::StringVector lines;
			if (!s_commands[static_cast<size_t>(index)].empty())
				lines.push_back(s_commands[static_cast<size_t>(index)]);
			DevTestMenu::defer(lines, 1);
		}
	}

	// fill a popup data source container with the nodes; submenu containers go after the items, as
	// CuiMenuInfoHelper builds them
	void fill(UIDataSourceContainer & container, std::vector<Node> const & nodes, std::string const & path)
	{
		std::vector<UIDataSourceContainer *> subMenus;
		for (size_t i = 0; i < nodes.size(); ++i)
		{
			Node const & node = nodes[i];
			if (node.children.empty())
			{
				char name[16];
				sprintf(name, "c%d", static_cast<int>(s_commands.size()));
				s_commands.push_back(node.command);
				IGNORE_RETURN(addLeaf(container, name, node.label));
				continue;
			}
			char subName[64];
			sprintf(subName, "%s_s%d", path.c_str(), static_cast<int>(i));
			UIDataSource * const ds = addLeaf(container, std::string(subName) + "_item", node.label);
			ds->SetPropertyNarrow(UIPopupMenu::DataProperties::SubMenu, subName);
			UIDataSourceContainer * const sub = new UIDataSourceContainer;
			sub->SetName(subName);
			fill(*sub, node.children, subName);
			subMenus.push_back(sub);
		}
		for (std::vector<UIDataSourceContainer *>::const_iterator s = subMenus.begin(); s != subMenus.end(); ++s)
			container.AddChild(*s);
	}

	void openPopup(std::string const & title, std::vector<Node> const & nodes)
	{
		UIPage * const root = UIManager::gUIManager().GetRootPage();
		if (!root || nodes.empty())
			return;
		UIManager::gUIManager().PopContextWidgets(0);           // one devtest menu at a time
		s_commands.clear();

		UIPopupMenu * const pop = new UIPopupMenu(root);
		pop->SetStyle(root->FindPopupStyle());
		pop->SetName("DevTestMenu");
		UIDataSourceContainer * const dsc = new UIDataSourceContainer;
		dsc->SetName("DefaultDataSource");
		std::vector<Node> all;
		if (!title.empty())
			all.push_back(makeLeaf("- " + title + " -", ""));
		all.insert(all.end(), nodes.begin(), nodes.end());
		fill(*dsc, all, "devtest");
		pop->SetDataSourceContainer(dsc);

		UISize const size = root->GetSize();
		pop->SetLocation(UIPoint(size.x / 3, size.y / 4));
		UIManager::gUIManager().PushContextWidget(*pop);
		pop->AddCallback(&s_menuCallback);
	}

	std::vector<Node> toNodes(DevTestTools::Items const & items)
	{
		std::vector<Node> nodes;
		for (DevTestTools::Items::const_iterator i = items.begin(); i != items.end(); ++i)
			nodes.push_back(makeLeaf(i->label, i->command));
		return nodes;
	}

	bool isNumber(std::string const & s)
	{
		return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
	}
}

using namespace SwgCuiCommandParserDevTestNamespace;

// ======================================================================

void SwgCuiCommandParserDevTestNamespace::MenuCallback::OnPopupMenuSelection(UIWidget * context)
{
	if (!context || !context->IsA(TUIPopupMenu))
		return;
	UIPopupMenu * const pop = safe_cast<UIPopupMenu *>(context);
	std::string const selection = pop->GetSelectedName();
	if (!selection.empty())
		runSelection(selection);
}

// ======================================================================

SwgCuiCommandParserDevTest::SwgCuiCommandParserDevTest() :
	CommandParser("devtest", 0, "...", "offline test menu: vehicles, flight, animations, spawns, warps, debug views", 0)
{
	createDelegateCommands(cs_commands);
}

// ----------------------------------------------------------------------

SwgCuiCommandParserDevTest::~SwgCuiCommandParserDevTest()
{
}

// ----------------------------------------------------------------------

void SwgCuiCommandParserDevTest::openMenu()
{
	DevTestMenu::StringVector categories;
	DevTestMenu::getCategories(categories);

	std::vector<Node> nodes;
	for (size_t c = 0; c < categories.size(); ++c)
	{
		Node category;
		category.label = categories[c];
		for (int i = 0; i < DevTestMenu::getActionCount(); ++i)
		{
			DevTestMenu::Action const & action = DevTestMenu::getAction(i);
			if (categories[c] == action.category)
				category.children.push_back(makeLeaf(action.label, std::string("/devtest run ") + action.id));
		}
		if (categories[c] == "Warp")
		{
			DevTestMenu::StringVector points;
			DevTestMenu::getPointNames(points);
			for (DevTestMenu::StringVector::const_iterator p = points.begin(); p != points.end(); ++p)
				category.children.push_back(makeLeaf("Point: " + *p, "/devtest point go " + *p));
		}
		nodes.push_back(category);
	}
	openPopup("", nodes);
}

// ----------------------------------------------------------------------

bool SwgCuiCommandParserDevTest::runAction(std::string const & id, std::string & result)
{
	DevTestMenu::Action const * const action = DevTestMenu::findAction(id);
	if (!action)
	{
		result += "no devtest action '" + id + "' (/devtest list)\n";
		return false;
	}
	DevTestMenu::StringVector lines;
	DevTestMenu::getLines(*action, lines);
	bool ok = true;
	for (DevTestMenu::StringVector::const_iterator i = lines.begin(); i != lines.end(); ++i)
	{
		int frames = 0;
		if (DevTestMenu::isWaitLine(*i, frames))
		{
			DevTestMenu::StringVector rest;
			for (DevTestMenu::StringVector::const_iterator r = i + 1; r != lines.end(); ++r)
				rest.push_back(*r);
			DevTestMenu::defer(rest, frames);
			break;
		}
		ok = runLine(*i) && ok;
	}
	result += std::string(action->category) + ": " + action->label + (ok ? "\n" : " (a step failed)\n");
	return ok;
}

// ----------------------------------------------------------------------

bool SwgCuiCommandParserDevTest::performParsing(NetworkId const & userId, StringVector_t const & argv, String_t const & originalCommand, String_t & result, CommandParser const * node)
{
	UNREF(userId);
	UNREF(originalCommand);
	UNREF(node);

	if (argv.empty())
		return false;

	DevTestMenu::StringVector args;
	for (StringVector_t::const_iterator i = argv.begin(); i != argv.end(); ++i)
		args.push_back(Unicode::wideToNarrow(*i));

	std::string out;
	bool ok = true;
	if (args[0] == "menu")
		openMenu();
	else if (args[0] == "list")
	{
		for (int i = 0; i < DevTestMenu::getActionCount(); ++i)
		{
			DevTestMenu::Action const & action = DevTestMenu::getAction(i);
			out += std::string(action.id) + "  [" + action.category + "] " + action.label + "\n";
		}
	}
	else if (args[0] == "run")
		ok = runAction(args[1], out);
	else if (args[0] == "pick" || args[0] == "anims" || args[0] == "animspecies")
	{
		// the list goes to the console too, so the bridge and scripts can read it
		DevTestTools::Items items;
		std::string title;
		if (args[0] == "pick")
		{
			DevTestMenu::StringVector words;
			for (size_t w = 1; w < args.size(); ++w)
				words.push_back(args[w]);
			int page = 0;
			if (words.size() > 1 && isNumber(words.back()))
			{
				page = atoi(words.back().c_str());
				words.pop_back();
			}
			IGNORE_RETURN(DevTestTools::pickTemplates(words, page, items, title));
		}
		else if (args[0] == "anims")
		{
			std::string const prefix = args.size() > 1 ? args[1] : "target";
			int const page = args.size() > 2 && isNumber(args[2]) ? atoi(args[2].c_str()) : 0;
			bool const loop = args.size() > 3 && args[3] == "loop";
			IGNORE_RETURN(DevTestTools::browseAnimations(prefix, page, loop, items, title));
		}
		else
		{
			DevTestTools::getAnimationSpecies(items);
			title = "Animation species";
		}
		out += title + "\n";
		for (DevTestTools::Items::const_iterator i = items.begin(); i != items.end(); ++i)
			out += "  " + i->label + "  ->  " + i->command + "\n";
		openPopup(title, toNodes(items));
	}
	else if (!DevTestMenu::performVerb(args, out))
	{
		out += DevTestMenu::getVerbHelp();
		ok = false;
	}
	result += Unicode::narrowToWide(out);
	return ok;
}

// ======================================================================

#endif

// ======================================================================
