// ======================================================================
//
// ConfigFileControl.cpp
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#include "FirstSwgFileControl.h"
#include "ConfigFileControl.h"

#include "sharedFoundation/ConfigFile.h"

// ======================================================================

namespace ConfigFileControlNamespace
{
	// Server [SwgFileControl]
	const char * ms_host                       = 0;
	int          ms_port                       = 44411;
	const char * ms_channel                    = 0;
	int          ms_maxSendSize                = 1073741824; // 1GB
	bool         ms_allowUpdates               = true;
	int          ms_maxBufferSizes             = 65536;
	const char * ms_fileServerKey              = 0;
	bool         ms_fileServerAllowUpdates     = true;
	int          ms_fileServerMaxConnections   = 32;
	int          ms_rateLimitKBps              = 40000;
	int          ms_logLevel                   = 1;
	bool         ms_enableCompression          = true;
	const char * ms_blacklistUploadPaths       = 0;
	const char * ms_blacklistSendPaths         = 0;
	const char * ms_whitelistExtensions        = 0;
	bool         ms_backupBeforeReload         = true;
	bool         ms_disableReload              = false;
	const char * ms_antBuildPath               = 0;

	// Cluster console (server-only, but kept in sync)
	const char * ms_centralConsoleHost        = 0;
	int          ms_centralConsolePort        = 61000;
	bool         ms_enableClusterReload       = false;

	// Client [ClientGame]
	const char * ms_clientHost                 = 0;
	int          ms_clientPort                 = 44411;
	const char * ms_clientFileServerKey        = 0;
	bool         ms_autoReload                 = true;
	bool         ms_verifyChecksum             = true;
	bool         ms_clientEnableCompression    = true;

	bool         ms_isServerMode               = false;
}

using namespace ConfigFileControlNamespace;

// ======================================================================

#define KEY_STRING(section, var, def) (ms_ ## var = ConfigFile::getKeyString(#section, #var, def))
#define KEY_INT(section, var, def)    (ms_ ## var = ConfigFile::getKeyInt(#section, #var, def))
#define KEY_BOOL(section, var, def)   (ms_ ## var = ConfigFile::getKeyBool(#section, #var, def))

// ======================================================================

void ConfigFileControl::install()
{
	// Server configuration
	KEY_STRING(SwgFileControl, host,                       "0.0.0.0");
	KEY_INT   (SwgFileControl, port,                       44411);
	KEY_STRING(SwgFileControl, channel,                    "live");
	KEY_INT   (SwgFileControl, maxSendSize,                1073741824);
	KEY_BOOL  (SwgFileControl, allowUpdates,               true);
	KEY_INT   (SwgFileControl, maxBufferSizes,             65536);
	KEY_STRING(SwgFileControl, fileServerKey,              "");
	KEY_BOOL  (SwgFileControl, fileServerAllowUpdates,     true);
	KEY_INT   (SwgFileControl, fileServerMaxConnections,   32);
	KEY_INT   (SwgFileControl, rateLimitKBps,              40000);
	KEY_INT   (SwgFileControl, logLevel,                   1);
	KEY_BOOL  (SwgFileControl, enableCompression,          true);
	KEY_STRING(SwgFileControl, blacklistUploadPaths,       "");
	KEY_STRING(SwgFileControl, blacklistSendPaths,         "");
	KEY_STRING(SwgFileControl, whitelistExtensions,        ".iff;.class;.cdf;.sht;.trn;.ws;.msh;.lod;.pob;.apt;.sat;.skt;.lmg;.mgn;.ans;.ash;.lsb;.stf");
	KEY_BOOL  (SwgFileControl, backupBeforeReload,         true);
	KEY_BOOL  (SwgFileControl, disableReload,              false);
	KEY_STRING(SwgFileControl, antBuildPath,               "/home/swg/swg-main");

	// Cluster console
	KEY_STRING(SwgFileControl, centralConsoleHost,         "");
	KEY_INT   (SwgFileControl, centralConsolePort,         61000);
	KEY_BOOL  (SwgFileControl, enableClusterReload,        false);

	// Client configuration
	KEY_STRING(ClientGame, clientHost,                     "127.0.0.1");
	KEY_INT   (ClientGame, clientPort,                     44411);
	KEY_STRING(ClientGame, clientFileServerKey,            "");
	KEY_BOOL  (ClientGame, autoReload,                     true);
	KEY_BOOL  (ClientGame, verifyChecksum,                 true);
	KEY_BOOL  (ClientGame, clientEnableCompression,        true);

	// Determine mode: if [SwgFileControl] host is configured, we are server
	ms_isServerMode = (ConfigFile::getKeyString("SwgFileControl", "host", 0) != 0);
}

// ----------------------------------------------------------------------

void ConfigFileControl::remove()
{
}

// ======================================================================
// Server accessors
// ======================================================================

const char * ConfigFileControl::getHost()                       { return ms_host; }
int          ConfigFileControl::getPort()                       { return ms_port; }
const char * ConfigFileControl::getChannel()                    { return ms_channel; }
int          ConfigFileControl::getMaxSendSize()                { return ms_maxSendSize; }
bool         ConfigFileControl::getAllowUpdates()                { return ms_allowUpdates; }
int          ConfigFileControl::getMaxBufferSizes()             { return ms_maxBufferSizes; }
const char * ConfigFileControl::getFileServerKey()              { return ms_fileServerKey; }
bool         ConfigFileControl::getFileServerAllowUpdates()     { return ms_fileServerAllowUpdates; }
int          ConfigFileControl::getFileServerMaxConnections()   { return ms_fileServerMaxConnections; }
int          ConfigFileControl::getRateLimitKBps()              { return ms_rateLimitKBps; }
int          ConfigFileControl::getLogLevel()                   { return ms_logLevel; }
bool         ConfigFileControl::getEnableCompression()          { return ms_enableCompression; }
const char * ConfigFileControl::getBlacklistUploadPaths()       { return ms_blacklistUploadPaths; }
const char * ConfigFileControl::getBlacklistSendPaths()         { return ms_blacklistSendPaths; }
const char * ConfigFileControl::getWhitelistExtensions()        { return ms_whitelistExtensions; }
bool         ConfigFileControl::getBackupBeforeReload()         { return ms_backupBeforeReload; }
bool         ConfigFileControl::getDisableReload()              { return ms_disableReload; }
const char * ConfigFileControl::getAntBuildPath()               { return ms_antBuildPath; }
const char * ConfigFileControl::getCentralConsoleHost()         { return ms_centralConsoleHost; }
int          ConfigFileControl::getCentralConsolePort()         { return ms_centralConsolePort; }
bool         ConfigFileControl::getEnableClusterReload()        { return ms_enableClusterReload; }

// ======================================================================
// Client accessors
// ======================================================================

const char * ConfigFileControl::getClientHost()                 { return ms_clientHost; }
int          ConfigFileControl::getClientPort()                 { return ms_clientPort; }
const char * ConfigFileControl::getClientFileServerKey()        { return ms_clientFileServerKey; }
bool         ConfigFileControl::getAutoReload()                 { return ms_autoReload; }
bool         ConfigFileControl::getVerifyChecksum()             { return ms_verifyChecksum; }
bool         ConfigFileControl::getClientEnableCompression()    { return ms_clientEnableCompression; }

// ======================================================================
// Mode
// ======================================================================

bool ConfigFileControl::isServerMode()                          { return ms_isServerMode; }

// ======================================================================
