// ======================================================================
//
// ConfigFileControl.h
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_ConfigFileControl_H
#define INCLUDED_ConfigFileControl_H

// ======================================================================

class ConfigFileControl
{
public:

	static void install();
	static void remove();

	// Server configuration [SwgFileControl]
	static const char * getHost();
	static int          getPort();
	static const char * getChannel();
	static int          getMaxSendSize();
	static bool         getAllowUpdates();
	static int          getMaxBufferSizes();
	static const char * getFileServerKey();
	static bool         getFileServerAllowUpdates();
	static int          getFileServerMaxConnections();
	static int          getRateLimitKBps();
	static int          getLogLevel();
	static bool         getEnableCompression();
	static const char * getBlacklistUploadPaths();
	static const char * getBlacklistSendPaths();
	static const char * getWhitelistExtensions();
	static bool         getBackupBeforeReload();
	static bool         getDisableReload();
	static const char * getAntBuildPath();

	// Cluster console connection (for broadcasting reloads to game servers)
	static const char * getCentralConsoleHost();
	static int          getCentralConsolePort();
	static bool         getEnableClusterReload();

	// Client configuration [ClientGame]
	static const char * getClientHost();
	static int          getClientPort();
	static const char * getClientFileServerKey();
	static bool         getAutoReload();
	static bool         getVerifyChecksum();
	static bool         getClientEnableCompression();

	// Mode detection
	static bool         isServerMode();

private:
	ConfigFileControl();
	ConfigFileControl(const ConfigFileControl &);
	ConfigFileControl & operator=(const ConfigFileControl &);
};

// ======================================================================

#endif
