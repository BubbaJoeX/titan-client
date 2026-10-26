// ======================================================================
//
// SwgCuiCommandParserDevTest.h
//
// /devtest: the offline test menu (vehicles, atmospheric flight,
// animations, spawns, warp points, debug views) and the console verbs its
// actions use. The actions themselves live in clientGame DevTestMenu so the
// DevBridge can run them too. Development builds only.
//
// ======================================================================

#ifndef INCLUDED_SwgCuiCommandParserDevTest_H
#define INCLUDED_SwgCuiCommandParserDevTest_H

// ======================================================================

#include "sharedCommandParser/CommandParser.h"

// ======================================================================

class SwgCuiCommandParserDevTest : public CommandParser
{
public:

	SwgCuiCommandParserDevTest();
	virtual ~SwgCuiCommandParserDevTest();

	virtual bool  performParsing(NetworkId const & userId, StringVector_t const & argv, String_t const & originalCommand, String_t & result, CommandParser const * node);

	static void   openMenu();
	static bool   runAction(std::string const & id, std::string & result);

private:

	SwgCuiCommandParserDevTest(SwgCuiCommandParserDevTest const &);
	SwgCuiCommandParserDevTest & operator=(SwgCuiCommandParserDevTest const &);
};

// ======================================================================

#endif
