// ======================================================================
//
// DevTestTools.h
//
// The offline test menu's tools that keep state across frames or read
// lists: camera placement (free / orbit / chase), vehicle and ship
// telemetry, and the template and animation lists behind the spawn picker
// and the animation browser. The /devtest verbs reach them through
// DevTestMenu::performVerb, the DevBridge through its camera command and
// the "mover" field of its state reply. Development builds only.
//
// The lists are text files in the working directory, one entry per line:
// devtest_templates.txt (object template paths) and devtest_animations.txt
// (file names under appearance/animation/). SwgMayaMCP
// examples/make_devtest_lists.py writes both from the game data and
// D:/exported.
//
// ======================================================================

#ifndef INCLUDED_DevTestTools_H
#define INCLUDED_DevTestTools_H

// ======================================================================

#include <string>
#include <vector>

// ======================================================================

class DevTestTools
{
public:

	typedef std::vector<std::string> StringVector;

	struct Item
	{
		std::string label;
		std::string command;      // console line run when the item is picked
	};
	typedef std::vector<Item> Items;

	static int const cs_pageSize = 24;

	// camera, telemetry: true when argv[0] is one of these verbs
	static bool        performVerb(StringVector const & argv, std::string & result);
	static void        update();

	// what the player rides or flies (or the player on foot) as a JSON object
	static std::string describeMover();

	// one page of the template picker / animation browser; returns the number of matches. Items include
	// previous / next page entries.
	static int         pickTemplates(StringVector const & filterWords, int page, Items & items, std::string & title);
	static int         browseAnimations(std::string const & prefixOrTarget, int page, bool loop, Items & items, std::string & title);
	static void        getAnimationSpecies(Items & items);

private:

	DevTestTools();
	DevTestTools(DevTestTools const &);
	DevTestTools & operator=(DevTestTools const &);
};

// ======================================================================

#endif
