// ======================================================================
//
// SwgFileControl.cpp
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#include "FirstSwgFileControl.h"
#include "SwgFileControl.h"

#include "ConfigFileControl.h"
#include "FileControlClient.h"
#include "FileControlServer.h"

#include "sharedFoundation/CommandLine.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/SetupSharedFoundation.h"
#include "sharedCompression/SetupSharedCompression.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFile/SetupSharedFile.h"
#include "sharedLog/Log.h"
#include "sharedLog/SetupSharedLog.h"
#include "sharedNetwork/SetupSharedNetwork.h"
#include "sharedThread/SetupSharedThread.h"

#include <cstdio>
#include <cstring>

// ======================================================================

extern int main(int argc, char ** argv);

std::string SwgFileControl::ms_updateFile;
std::string SwgFileControl::ms_downloadFile;
std::string SwgFileControl::ms_compareFile;
std::string SwgFileControl::ms_testRunFile;
std::string SwgFileControl::ms_fileServerKey;

// ======================================================================
// Command line definitions
// ======================================================================

static const char * const LNAME_HELP             = "help";
static const char * const LNAME_UPDATE_FILE      = "update-file";
static const char * const LNAME_DOWNLOAD_FILE    = "download-file";
static const char * const LNAME_COMPARE_FILE     = "compare-file";
static const char * const LNAME_TEST_RUN         = "test-run";
static const char * const LNAME_FILE_SERVER_KEY  = "fileServerKey";

static const char         SNAME_HELP             = 'h';
static const char         SNAME_UPDATE_FILE      = 'u';
static const char         SNAME_DOWNLOAD_FILE    = 'd';
static const char         SNAME_COMPARE_FILE     = 'c';
static const char         SNAME_TEST_RUN         = 't';
static const char         SNAME_FILE_SERVER_KEY  = 'k';

static CommandLine::OptionSpec optionSpecArray[] =
{
	OP_BEGIN_SWITCH(OP_NODE_REQUIRED),

		// help
		OP_SINGLE_SWITCH_NODE(SNAME_HELP, LNAME_HELP, OP_ARG_NONE, OP_MULTIPLE_DENIED),

		// server mode (no args needed, driven by config)
		OP_BEGIN_SWITCH_NODE(OP_MULTIPLE_DENIED),
			OP_BEGIN_LIST(),

				// client commands
				OP_SINGLE_LIST_NODE(SNAME_UPDATE_FILE,     LNAME_UPDATE_FILE,     OP_ARG_REQUIRED, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),
				OP_SINGLE_LIST_NODE(SNAME_DOWNLOAD_FILE,   LNAME_DOWNLOAD_FILE,   OP_ARG_REQUIRED, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),
				OP_SINGLE_LIST_NODE(SNAME_COMPARE_FILE,    LNAME_COMPARE_FILE,    OP_ARG_REQUIRED, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),
				OP_SINGLE_LIST_NODE(SNAME_TEST_RUN,        LNAME_TEST_RUN,        OP_ARG_REQUIRED, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),
				OP_SINGLE_LIST_NODE(SNAME_FILE_SERVER_KEY, LNAME_FILE_SERVER_KEY, OP_ARG_REQUIRED, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),

			OP_END_LIST(),
		OP_END_SWITCH_NODE(),

	OP_END_SWITCH()
};

static const int optionSpecCount = sizeof(optionSpecArray) / sizeof(optionSpecArray[0]);

// ======================================================================

int main(int argc, char ** argv)
{
	SetupSharedThread::install();
	SetupSharedDebug::install(4096);

	{
		SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_console);
		data.argc = argc;
		data.argv = argv;
		data.configFile = "SwgFileControl.cfg";
#if defined(PLATFORM_WIN32)
		data.demoMode = true;
#endif
		SetupSharedFoundation::install(data);
	}

	SetupSharedCompression::install();
	SetupSharedFile::install(false);

	SetupSharedNetwork::SetupData networkSetupData;
	SetupSharedNetwork::getDefaultClientSetupData(networkSetupData);
	SetupSharedNetwork::install(networkSetupData);

	ConfigFileControl::install();

	SetupSharedFoundation::callbackWithExceptionHandling(SwgFileControl::run);
	SetupSharedFoundation::remove();
	SetupSharedThread::remove();

	return 0;
}

// ======================================================================

void SwgFileControl::run()
{
	printf("SwgFileControl " __DATE__ " " __TIME__ "\n");
	fflush(stdout);

	printf("  isServerMode = %s\n", ConfigFileControl::isServerMode() ? "true" : "false");
	fflush(stdout);

	// Check server mode FIRST, before command line parsing
	if (ConfigFileControl::isServerMode())
	{
		runServer();
		return;
	}

	// Not server mode — parse command line for client commands
	const CommandLine::MatchCode mc = CommandLine::parseOptions(optionSpecArray, optionSpecCount);

	if (mc == CommandLine::MC_MATCH)
	{
		if (CommandLine::getOccurrenceCount(SNAME_HELP))
		{
			usage();
			return;
		}

		if (CommandLine::getOccurrenceCount(SNAME_FILE_SERVER_KEY))
			ms_fileServerKey = CommandLine::getOptionString(SNAME_FILE_SERVER_KEY);

		if (CommandLine::getOccurrenceCount(SNAME_UPDATE_FILE))
			ms_updateFile = CommandLine::getOptionString(SNAME_UPDATE_FILE);
		else if (CommandLine::getOccurrenceCount(SNAME_DOWNLOAD_FILE))
			ms_downloadFile = CommandLine::getOptionString(SNAME_DOWNLOAD_FILE);
		else if (CommandLine::getOccurrenceCount(SNAME_COMPARE_FILE))
			ms_compareFile = CommandLine::getOptionString(SNAME_COMPARE_FILE);
		else if (CommandLine::getOccurrenceCount(SNAME_TEST_RUN))
			ms_testRunFile = CommandLine::getOptionString(SNAME_TEST_RUN);

		if (!ms_updateFile.empty() || !ms_downloadFile.empty() || !ms_compareFile.empty() || !ms_testRunFile.empty())
		{
			runClient();
			return;
		}
	}

	printf("No command specified and no [SwgFileControl] server config found.\n");
	fflush(stdout);
	usage();
}

// ======================================================================

void SwgFileControl::runServer()
{
	printf("Starting in SERVER mode...\n");
	fflush(stdout);
	LOG("FileControl", ("Starting server mode"));

	printf("  Installing FileControlServer...\n");
	fflush(stdout);

	FileControlServer::install();

	printf("  Server installed. Listening on %s:%d\n", ConfigFileControl::getHost(), ConfigFileControl::getPort());
	printf("  Channel: %s\n", ConfigFileControl::getChannel());
	printf("  Press Ctrl+C to stop.\n");
	fflush(stdout);

	while (FileControlServer::isRunning())
	{
		FileControlServer::update();
		Os::sleep(10);
	}

	printf("  Server shutting down...\n");
	fflush(stdout);

	FileControlServer::remove();

	printf("  Server stopped.\n");
	fflush(stdout);
}

// ======================================================================

void SwgFileControl::runClient()
{
	printf("Starting in CLIENT mode...\n");
	LOG("FileControl", ("Starting client mode"));

	FileControlClient::install();

	if (!ms_updateFile.empty())
	{
		FileControlClient::executeCommand(FileControlClient::CC_UPDATE_FILE, ms_updateFile);
	}
	else if (!ms_downloadFile.empty())
	{
		FileControlClient::executeCommand(FileControlClient::CC_DOWNLOAD_FILE, ms_downloadFile);
	}
	else if (!ms_compareFile.empty())
	{
		FileControlClient::executeCommand(FileControlClient::CC_COMPARE_FILE, ms_compareFile);
	}
	else if (!ms_testRunFile.empty())
	{
		FileControlClient::executeCommand(FileControlClient::CC_TEST_RUN, ms_testRunFile);
	}

	// Wait for response
	int timeout = 300; // 30 seconds at 100ms intervals
	while (!FileControlClient::isDone() && timeout > 0)
	{
		FileControlClient::update();
		Os::sleep(100);
		--timeout;
	}

	if (timeout <= 0)
	{
		printf("WARNING: Operation timed out\n");
	}

	FileControlClient::remove();
}

// ======================================================================

void SwgFileControl::usage()
{
	printf("\n");
	printf("SwgFileControl - SWG File Distribution Server/Client\n");
	printf("=====================================================\n");
	printf("\n");
	printf("SERVER MODE:\n");
	printf("  SwgFileControl\n");
	printf("    Starts as server when [SwgFileControl] section exists in config.\n");
	printf("    Configuration via SwgFileControl.cfg\n");
	printf("\n");
	printf("CLIENT MODE:\n");
	printf("  SwgFileControl --update-file=<path> [--fileServerKey=<key>]\n");
	printf("    Uploads compiled asset to file server.\n");
	printf("\n");
	printf("  SwgFileControl --download-file=<path> [--fileServerKey=<key>]\n");
	printf("    Retrieves asset from file server.\n");
	printf("\n");
	printf("  SwgFileControl --compare-file=<path> [--fileServerKey=<key>]\n");
	printf("    Reports size/checksum delta for verification.\n");
	printf("\n");
	printf("  SwgFileControl --test-run=<path> [--fileServerKey=<key>]\n");
	printf("    Tests network connectivity without transfer.\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("  -h, --help              Show this help message\n");
	printf("  -u, --update-file       Upload compiled asset to server\n");
	printf("  -d, --download-file     Download asset from server\n");
	printf("  -c, --compare-file      Compare local vs server file\n");
	printf("  -t, --test-run          Test connectivity (no transfer)\n");
	printf("  -k, --fileServerKey     Authentication key (overrides config)\n");
	printf("\n");
	printf("EXAMPLES:\n");
	printf("  SwgFileControl --update-file=data/sku.0/sys.shared/compiled/game/datatables/skill/skills.iff --fileServerKey=password\n");
	printf("  SwgFileControl --download-file=data/sku.0/sys.server/compiled/game/script/bubbajoe/cmd.class\n");
	printf("  SwgFileControl --compare-file=data/sku.0/sys.shared/compiled/game/datatables/crafting/component.iff\n");
	printf("  SwgFileControl --test-run=data/sku.0/sys.server/compiled/game/object/tangible/terminal/terminal_character_builder.iff\n");
	printf("\n");
	printf("PATHS:\n");
	printf("  WIN32_ROOT = D:/titan\n");
	printf("  LINUX_ROOT = /home/swg/swg-main\n");
	printf("  Paths must conform to: data/sku.0/...\n");
	printf("\n");
}

// ======================================================================
