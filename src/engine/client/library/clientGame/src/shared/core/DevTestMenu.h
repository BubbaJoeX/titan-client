// ======================================================================
//
// DevTestMenu.h
//
// Offline test actions for the development client: one table of named
// actions (vehicles, atmospheric flight, animations, spawns, warp points,
// debug views), each a short list of console lines run as if typed in chat.
// The in-game test menu (/devtest menu) and the DevBridge "menu" command
// both run actions from this table. The /devtest console verbs that the
// action lines use for glue (spawn in front of the player, target the last
// spawn, dismount, saved points, debug-flag toggles) are implemented here.
// Development builds only: compiled out where PRODUCTION is 1.
//
// Action lines may use $last: the network id of the most recent /devtest
// spawn, substituted when the line runs.
//
// ======================================================================

#ifndef INCLUDED_DevTestMenu_H
#define INCLUDED_DevTestMenu_H

// ======================================================================

#include <string>
#include <vector>

// ======================================================================

class DevTestMenu
{
public:

	struct Action
	{
		char const * id;        // "vehicle.swoop"
		char const * category;  // "Vehicles"
		char const * label;     // "Swoop bike"
		char const * lines;     // console lines separated by '\n'
	};

	typedef std::vector<std::string> StringVector;

	static int            getActionCount();
	static Action const & getAction(int index);
	static Action const * findAction(std::string const & id);
	static void           getCategories(StringVector & categories);

	// the action's console lines, $last expanded: call once per line, just before running it
	static void           getLines(Action const & action, StringVector & lines);
	static std::string    expandLine(std::string const & line);

	// "/devtest wait <frames>" in an action: the lines after it run that many frames later (a creature spawned this
	// frame has no animation controller yet). Runners stop at a wait line and hand the rest to defer().
	static bool           isWaitLine(std::string const & line, int & frames);
	static void           defer(StringVector const & lines, int frames);
	static void           update();

	// saved warp points (devtest_points.txt in the working directory)
	static void           getPointNames(StringVector & names);

	// run one /devtest verb (argv[0] is the verb); false for an unknown verb or a bad argument count
	static bool           performVerb(StringVector const & argv, std::string & result);
	static char const *   getVerbHelp();

private:

	DevTestMenu();
	DevTestMenu(DevTestMenu const &);
	DevTestMenu & operator=(DevTestMenu const &);
};

// ======================================================================

#endif
