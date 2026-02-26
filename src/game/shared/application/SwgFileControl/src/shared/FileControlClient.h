// ======================================================================
//
// FileControlClient.h
// copyright 2024 Sony Online Entertainment
//
// Client mode for SwgFileControl - connects to server, executes
// commands (--update-file, --download-file, --compare-file, --test-run).
//
// ======================================================================

#ifndef INCLUDED_FileControlClient_H
#define INCLUDED_FileControlClient_H

// ======================================================================

#include <string>
#include <vector>

class FileControlConnection;

// ======================================================================

class FileControlClient
{
public:

	typedef void (*LogCallback)(const char * message);

	enum ClientCommand
	{
		CC_NONE,
		CC_UPDATE_FILE,
		CC_DOWNLOAD_FILE,
		CC_COMPARE_FILE,
		CC_TEST_RUN
	};

	static void install();
	static void remove();
	static void update();
	static void setLogCallback(LogCallback cb);

	static bool executeCommand(ClientCommand command, const std::string & filePath);
	static bool isConnected();
	static bool isDone();

	// GodClient API — works over a raw TCP socket to the FileControl server.
	// All request* methods are thread-safe (serialized via internal lock).
	static bool ensureConnected();
	static bool requestSendAsset(const std::string & relativePath);
	static bool requestRetrieveAsset(const std::string & relativePath, std::vector<unsigned char> & outData);
	static bool requestReloadAsset(const std::string & relativePath);
	static bool requestBroadcastUpdate();
	static bool requestUpdateDbTemplates();
	static bool requestVerifyAsset(const std::string & relativePath, unsigned long & outSize, unsigned long & outCrc);
	static bool requestFlush();
	static bool requestDirectoryListing(const std::string & rootPath, std::vector<std::string> & outFiles, std::vector<unsigned long> & outSizes, std::vector<unsigned long> & outCrcs);
	static void lockApi();
	static void unlockApi();

private:

	FileControlClient();
	FileControlClient(const FileControlClient &);
	FileControlClient & operator=(const FileControlClient &);

	static bool connect();
	static bool sendRawMessage(const std::vector<unsigned char> & msg);
	static bool recvRawMessage(std::vector<unsigned char> & msg, int timeoutMs);
	static void handleIncomingMessage(const std::vector<unsigned char> & msg);
	static bool recvExpectedMessage(uint8 expectedType, std::vector<unsigned char> & outPayload, int timeoutMs);

	static bool readLocalFile(const std::string & filePath, std::vector<unsigned char> & data);
	static bool writeLocalFile(const std::string & filePath, const std::vector<unsigned char> & data);
	static unsigned long getLocalFileCrc(const std::string & filePath);
	static unsigned long getLocalFileSize(const std::string & filePath);
	static std::string getMachineId();

	static bool doUpdateFile(const std::string & filePath);
	static bool doDownloadFile(const std::string & filePath);
	static bool doCompareFile(const std::string & filePath);
	static bool doTestRun(const std::string & filePath);

	static void logMessage(const char * fmt, ...);

	static LogCallback             ms_logCallback;
	static int64                   ms_rawSocket;
	static FileControlConnection * ms_connection;
	static bool                    ms_connected;
	static bool                    ms_authenticated;
	static bool                    ms_done;
	static ClientCommand           ms_pendingCommand;
	static std::string             ms_pendingPath;
};

// ======================================================================

inline bool FileControlClient::isConnected()
{
	return ms_connected;
}

inline bool FileControlClient::isDone()
{
	return ms_done;
}

// ======================================================================

#endif
