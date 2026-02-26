// ======================================================================
//
// FileControlServer.cpp
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#include "FirstSwgFileControl.h"
#include "FileControlServer.h"

#include "ConfigFileControl.h"
#include "FileControlConnection.h"
#include "FileControlReloadHandler.h"
#include "HotReloadManager.h"

#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/Crc.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Os.h"
#include "sharedLog/Log.h"
#include "sharedNetwork/NetworkHandler.h"
#include "sharedNetwork/NetworkSetupData.h"
#include "sharedNetwork/Service.h"

#include "sharedCompression/ZlibCompressor.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

#if defined(PLATFORM_WIN32)
#include <direct.h>
#include <io.h>
#include <windows.h>
#define PATH_SEPARATOR '\\'
#define STAT_FUNC _stat
#define STAT_STRUCT struct _stat
#else
#include <dirent.h>
#include <unistd.h>
#define PATH_SEPARATOR '/'
#define STAT_FUNC stat
#define STAT_STRUCT struct stat
#endif

// ======================================================================

Service *                          FileControlServer::ms_service     = 0;
FileControlQueue                   FileControlServer::ms_queue;
std::set<FileControlConnection *>  FileControlServer::ms_connections;
bool                               FileControlServer::ms_running     = false;
std::string                        FileControlServer::ms_dataRoot;
std::string                        FileControlServer::ms_channel;
std::map<std::string, unsigned long> FileControlServer::ms_versionMap;
unsigned long                      FileControlServer::ms_bytesSentThisSecond = 0;
unsigned long                      FileControlServer::ms_lastRateLimitTime   = 0;

// ======================================================================

void FileControlServer::install()
{
	if (ms_running)
		return;

	const char * host = ConfigFileControl::getHost();
	const int port = ConfigFileControl::getPort();

	ms_channel = ConfigFileControl::getChannel();
	LOG("FileControl", ("Starting server on %s:%d (channel=%s)", host, port, ms_channel.c_str()));

#if defined(PLATFORM_WIN32)
	char fullPath[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, fullPath);
	ms_dataRoot = fullPath;
	ms_dataRoot += "\\";
#else
	ms_dataRoot = ConfigFileControl::getAntBuildPath();
	if (!ms_dataRoot.empty() && ms_dataRoot[ms_dataRoot.size() - 1] != '/')
		ms_dataRoot += "/";
#endif

	LOG("FileControl", ("Data root: %s", ms_dataRoot.c_str()));

	printf("  Binding to %s:%d ...\n", host, port);
	printf("  Data root: %s\n", ms_dataRoot.c_str());
	fflush(stdout);

	NetworkSetupData setupData;
	setupData.port = static_cast<unsigned short>(port);
	setupData.maxConnections = ConfigFileControl::getFileServerMaxConnections();
	setupData.bindInterface = host;
	setupData.useTcp = true;

	printf("  Creating network service...\n");
	fflush(stdout);

	ms_service = new Service(ConnectionAllocator<FileControlConnection>(), setupData);

	printf("  Network service created.\n");
	fflush(stdout);

	ms_running = true;

	FileControlReloadHandler::install();

	ExitChain::add(remove, "FileControlServer::remove");

	LOG("FileControl", ("Server started successfully. Max connections: %d, Rate limit: %d KBps",
		ConfigFileControl::getFileServerMaxConnections(),
		ConfigFileControl::getRateLimitKBps()));

	printf("SwgFileControl server listening on %s:%d\n", host, port);
	printf("  Channel:    %s\n", ConfigFileControl::getChannel());
	printf("  Updates:    %s\n", ConfigFileControl::getAllowUpdates() ? "yes" : "no");
	printf("  Hot reload: %s\n", ConfigFileControl::getDisableReload() ? "disabled" : "enabled");
	printf("  Compress:   %s\n", ConfigFileControl::getEnableCompression() ? "enabled" : "disabled");
	printf("  Backup:     %s\n", ConfigFileControl::getBackupBeforeReload() ? "yes" : "no");
	fflush(stdout);
}

// ----------------------------------------------------------------------

void FileControlServer::remove()
{
	ms_running = false;
	ms_connections.clear();
	ms_queue.clear();

	FileControlReloadHandler::remove();

	delete ms_service;
	ms_service = 0;

	LOG("FileControl", ("Server shut down"));
}

// ----------------------------------------------------------------------

void FileControlServer::update()
{
	if (!ms_running || !ms_service)
		return;

	NetworkHandler::update();
	NetworkHandler::dispatch();

	unsigned long now = static_cast<unsigned long>(Os::getRealSystemTime());
	if (now != ms_lastRateLimitTime)
	{
		ms_bytesSentThisSecond = 0;
		ms_lastRateLimitTime = now;
	}

	processQueue();
}

// ----------------------------------------------------------------------

void FileControlServer::processQueue()
{
	if (ms_queue.isEmpty() || ms_queue.isProcessing())
		return;

	FileControlQueue::QueueEntry entry;
	if (!ms_queue.dequeue(entry))
		return;

	ms_queue.setProcessing(true);

	bool success = false;

	switch (entry.operation)
	{
	case FileControlQueue::OT_UPLOAD:
		success = handleFileUpload(entry);
		break;
	case FileControlQueue::OT_DOWNLOAD:
		success = handleFileDownload(entry);
		break;
	case FileControlQueue::OT_COMPARE:
		success = handleFileCompare(entry);
		break;
	case FileControlQueue::OT_TEST:
		success = true;
		break;
	}

	if (!success)
	{
		LOG("FileControl", ("Queue operation failed for %s", entry.relativePath.c_str()));
	}

	ms_queue.setProcessing(false);
}

// ======================================================================
// File operations
// ======================================================================

bool FileControlServer::handleFileUpload(const FileControlQueue::QueueEntry & entry)
{
	if (!ConfigFileControl::getAllowUpdates())
	{
		LOG("FileControl", ("Upload rejected - updates not allowed"));
		return false;
	}

	if (!ConfigFileControl::getFileServerAllowUpdates())
	{
		LOG("FileControl", ("Upload rejected - fileServerAllowUpdates is false"));
		return false;
	}

	if (isDsrcPath(entry.relativePath))
	{
		LOG("FileControl", ("Upload rejected - dsrc source path not allowed (compile first): %s", entry.relativePath.c_str()));
		return false;
	}

	if (!validatePath(entry.relativePath))
	{
		LOG("FileControl", ("Upload rejected - invalid path: %s", entry.relativePath.c_str()));
		return false;
	}

	if (isPathBlacklisted(entry.relativePath, true))
	{
		LOG("FileControl", ("Upload rejected - blacklisted path: %s", entry.relativePath.c_str()));
		return false;
	}

	if (!isExtensionWhitelisted(entry.relativePath))
	{
		LOG("FileControl", ("Upload rejected - extension not whitelisted: %s", entry.relativePath.c_str()));
		return false;
	}

	int maxSize = ConfigFileControl::getMaxSendSize();
	if (maxSize > 0 && static_cast<int>(entry.fileData.size()) > maxSize)
	{
		LOG("FileControl", ("Upload rejected - file exceeds maxSendSize (%d > %d): %s", static_cast<int>(entry.fileData.size()), maxSize, entry.relativePath.c_str()));
		return false;
	}

	std::vector<unsigned char> actualData = entry.fileData;

	if (entry.compressed && ConfigFileControl::getEnableCompression())
	{
		std::vector<unsigned char> decompressed;
		if (decompressData(entry.fileData, decompressed, 0))
		{
			actualData = decompressed;
			LOG("FileControl", ("Decompressed upload: %d -> %d bytes", static_cast<int>(entry.fileData.size()), static_cast<int>(decompressed.size())));
		}
	}

	if (ConfigFileControl::getBackupBeforeReload() && fileExists(entry.relativePath))
	{
		if (!backupFile(entry.relativePath))
		{
			WARNING(true, ("FileControl: Failed to backup %s before overwrite", entry.relativePath.c_str()));
		}
	}

	if (!storeFile(entry.relativePath, actualData))
	{
		LOG("FileControl", ("Failed to store file: %s", entry.relativePath.c_str()));
		return false;
	}

	LOG("FileControl", ("File stored: %s (%d bytes)", entry.relativePath.c_str(), static_cast<int>(actualData.size())));

	ms_versionMap[entry.relativePath] = ms_versionMap[entry.relativePath] + 1;
	LOG("FileControl", ("Version stamp: %s -> v%lu", entry.relativePath.c_str(), ms_versionMap[entry.relativePath]));

	if (!ConfigFileControl::getDisableReload())
	{
		HotReloadManager::ReloadAction action;
		if (HotReloadManager::determineReloadAction(entry.relativePath, action))
		{
			if (action.requiresCrcRebuild)
			{
				LOG("FileControl", ("Running CRC table rebuild..."));
				if (!runAntBuildCrcTable())
				{
					WARNING(true, ("FileControl: CRC table rebuild failed for %s - rolling back", entry.relativePath.c_str()));
					if (ConfigFileControl::getBackupBeforeReload())
						rollbackFile(entry.relativePath);
					return false;
				}
			}

			if (action.requiresQuestCrcRebuild)
			{
				LOG("FileControl", ("Quest CRC rebuild needed for %s", entry.relativePath.c_str()));
			}

			if (!executeHotReload(entry.relativePath, action))
			{
				WARNING(true, ("FileControl: Hot reload failed for %s", entry.relativePath.c_str()));
			}
		}
	}

	std::vector<unsigned char> distributeData = actualData;
	if (ConfigFileControl::getEnableCompression())
	{
		std::vector<unsigned char> compressed;
		if (compressData(actualData, compressed))
		{
			distributeData = compressed;
			LOG("FileControl", ("Compressed for distribution: %d -> %d bytes", static_cast<int>(actualData.size()), static_cast<int>(compressed.size())));
		}
	}

	distributeToClients(entry.relativePath, distributeData);

	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::handleFileDownload(const FileControlQueue::QueueEntry & entry)
{
	if (isPathBlacklisted(entry.relativePath, false))
	{
		LOG("FileControl", ("Download rejected - blacklisted send path: %s", entry.relativePath.c_str()));
		return false;
	}

	std::vector<unsigned char> data;
	if (!loadFile(entry.relativePath, data))
	{
		LOG("FileControl", ("Download failed - file not found: %s", entry.relativePath.c_str()));
		return false;
	}

	LOG("FileControl", ("File downloaded: %s (%d bytes)", entry.relativePath.c_str(), static_cast<int>(data.size())));
	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::handleFileCompare(const FileControlQueue::QueueEntry & entry)
{
	if (!fileExists(entry.relativePath))
	{
		LOG("FileControl", ("Compare: file not found on server: %s", entry.relativePath.c_str()));
		return true;
	}

	unsigned long size = getFileSize(entry.relativePath);
	unsigned long crc = getFileCrc(entry.relativePath);

	LOG("FileControl", ("Compare: %s (size=%lu, crc=0x%08lx)", entry.relativePath.c_str(), size, crc));
	return true;
}

// ======================================================================
// File I/O
// ======================================================================

std::string FileControlServer::getAbsolutePath(const std::string & relativePath)
{
	return ms_dataRoot + relativePath;
}

// ----------------------------------------------------------------------

bool FileControlServer::createDirectoryForFile(const std::string & absolutePath)
{
	std::string dirPath = absolutePath;

	std::string::size_type lastSep = dirPath.find_last_of("/\\");
	if (lastSep == std::string::npos)
		return true;

	dirPath = dirPath.substr(0, lastSep);

	for (std::string::size_type i = 1; i < dirPath.size(); ++i)
	{
		if (dirPath[i] == '/' || dirPath[i] == '\\')
		{
			std::string partial = dirPath.substr(0, i);
#if defined(PLATFORM_WIN32)
			CreateDirectoryA(partial.c_str(), NULL);
#else
			mkdir(partial.c_str(), 0755);
#endif
		}
	}

#if defined(PLATFORM_WIN32)
	CreateDirectoryA(dirPath.c_str(), NULL);
#else
	mkdir(dirPath.c_str(), 0755);
#endif

	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::storeFile(const std::string & relativePath, const std::vector<unsigned char> & data)
{
	std::string absPath = getAbsolutePath(relativePath);

	if (!createDirectoryForFile(absPath))
		return false;

	FILE * fp = fopen(absPath.c_str(), "wb");
	if (!fp)
	{
		WARNING(true, ("FileControl: Cannot open file for writing: %s", absPath.c_str()));
		return false;
	}

	if (!data.empty())
	{
		size_t written = fwrite(&data[0], 1, data.size(), fp);
		if (written != data.size())
		{
			fclose(fp);
			WARNING(true, ("FileControl: Short write to %s (%d of %d bytes)", absPath.c_str(), static_cast<int>(written), static_cast<int>(data.size())));
			return false;
		}
	}

	fclose(fp);
	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::loadFile(const std::string & relativePath, std::vector<unsigned char> & data)
{
	std::string absPath = getAbsolutePath(relativePath);

	FILE * fp = fopen(absPath.c_str(), "rb");
	if (!fp)
		return false;

	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (fileSize <= 0)
	{
		fclose(fp);
		data.clear();
		return true;
	}

	data.resize(static_cast<size_t>(fileSize));
	size_t bytesRead = fread(&data[0], 1, static_cast<size_t>(fileSize), fp);
	fclose(fp);

	if (bytesRead != static_cast<size_t>(fileSize))
	{
		data.clear();
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::backupFile(const std::string & relativePath)
{
	std::string absPath = getAbsolutePath(relativePath);
	std::string backupPath = absPath + ".bak";

	std::vector<unsigned char> data;
	if (!loadFile(relativePath, data))
		return false;

	FILE * fp = fopen(backupPath.c_str(), "wb");
	if (!fp)
		return false;

	if (!data.empty())
		fwrite(&data[0], 1, data.size(), fp);

	fclose(fp);

	LOG("FileControl", ("Backup created: %s", backupPath.c_str()));
	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::fileExists(const std::string & relativePath)
{
	std::string absPath = getAbsolutePath(relativePath);
	STAT_STRUCT st;
	return STAT_FUNC(absPath.c_str(), &st) == 0;
}

// ----------------------------------------------------------------------

unsigned long FileControlServer::getFileSize(const std::string & relativePath)
{
	std::string absPath = getAbsolutePath(relativePath);
	STAT_STRUCT st;
	if (STAT_FUNC(absPath.c_str(), &st) != 0)
		return 0;
	return static_cast<unsigned long>(st.st_size);
}

// ----------------------------------------------------------------------

unsigned long FileControlServer::getFileCrc(const std::string & relativePath)
{
	std::vector<unsigned char> data;
	if (!loadFile(relativePath, data))
		return 0;

	if (data.empty())
		return 0;

	return Crc::calculate(&data[0], static_cast<int>(data.size()));
}

// ======================================================================
// Path validation
// ======================================================================

bool FileControlServer::validatePath(const std::string & relativePath)
{
	if (relativePath.empty())
		return false;

	if (relativePath.find("..") != std::string::npos)
	{
		WARNING(true, ("FileControl: Path traversal attempt blocked: %s", relativePath.c_str()));
		return false;
	}

	if (relativePath.find("data/") != 0 && relativePath.find("data\\") != 0)
	{
		WARNING(true, ("FileControl: Path must start with data/: %s", relativePath.c_str()));
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::isPathBlacklisted(const std::string & relativePath, bool isUpload)
{
	const char * blacklist = isUpload
		? ConfigFileControl::getBlacklistUploadPaths()
		: ConfigFileControl::getBlacklistSendPaths();

	if (!blacklist || blacklist[0] == '\0')
		return false;

	std::string bl(blacklist);
	std::string::size_type start = 0;
	std::string::size_type end = 0;

	while ((end = bl.find(';', start)) != std::string::npos)
	{
		std::string pattern = bl.substr(start, end - start);
		if (!pattern.empty() && relativePath.find(pattern) != std::string::npos)
			return true;
		start = end + 1;
	}

	std::string lastPattern = bl.substr(start);
	if (!lastPattern.empty() && relativePath.find(lastPattern) != std::string::npos)
		return true;

	return false;
}

// ----------------------------------------------------------------------

bool FileControlServer::isExtensionWhitelisted(const std::string & relativePath)
{
	const char * whitelist = ConfigFileControl::getWhitelistExtensions();
	if (!whitelist || whitelist[0] == '\0')
		return true;

	std::string::size_type dotPos = relativePath.rfind('.');
	if (dotPos == std::string::npos)
		return false;

	std::string ext = relativePath.substr(dotPos);

	for (std::string::size_type i = 0; i < ext.size(); ++i)
		ext[i] = static_cast<char>(tolower(ext[i]));

	std::string wl(whitelist);
	return wl.find(ext) != std::string::npos;
}

// ======================================================================
// Compression
// ======================================================================

bool FileControlServer::compressData(const std::vector<unsigned char> & input, std::vector<unsigned char> & output)
{
	if (input.empty())
		return false;

	int compressedBufSize = static_cast<int>(input.size()) + static_cast<int>(input.size() / 100) + 128;
	output.resize(static_cast<size_t>(compressedBufSize) + sizeof(uint32));

	uint32 originalSize = static_cast<uint32>(input.size());
	memcpy(&output[0], &originalSize, sizeof(uint32));

	ZlibCompressor compressor;
	int result = compressor.compress(&input[0], static_cast<int>(input.size()),
		&output[sizeof(uint32)], compressedBufSize);

	if (result <= 0)
	{
		output.clear();
		return false;
	}

	output.resize(static_cast<size_t>(result) + sizeof(uint32));
	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::decompressData(const std::vector<unsigned char> & input, std::vector<unsigned char> & output, unsigned long originalSize)
{
	if (input.size() <= sizeof(uint32))
		return false;

	if (originalSize == 0)
	{
		uint32 storedSize = 0;
		memcpy(&storedSize, &input[0], sizeof(uint32));
		originalSize = storedSize;
	}

	if (originalSize > 1073741824UL)
	{
		WARNING(true, ("FileControl: Decompression rejected - claimed size %lu exceeds 1GB limit", originalSize));
		return false;
	}

	output.resize(originalSize);

	ZlibCompressor compressor;
	int result = compressor.expand(&input[sizeof(uint32)], static_cast<int>(input.size() - sizeof(uint32)),
		&output[0], static_cast<int>(originalSize));

	if (result <= 0)
	{
		output.clear();
		return false;
	}

	output.resize(static_cast<size_t>(result));
	return true;
}

// ======================================================================
// Hot reload execution
// ======================================================================

bool FileControlServer::executeHotReload(const std::string & relativePath, const HotReloadManager::ReloadAction & action)
{
	bool success = true;

	LOG("FileControl", ("Executing hot reload for %s (command=%s, target=%s)",
		relativePath.c_str(), action.reloadCommand.c_str(), action.reloadTarget.c_str()));

	if (action.reloadOnGameServers)
	{
		if (!sendReloadCommandToGameServers(action.reloadCommand, action.reloadTarget))
		{
			WARNING(true, ("FileControl: Failed to send reload command to game servers"));
			success = false;
		}
	}

	if (action.sendToClients)
	{
		std::vector<unsigned char> data;
		if (loadFile(relativePath, data))
		{
			distributeToClients(relativePath, data);
			LOG("FileControl", ("Distributed %s to clients for reload", relativePath.c_str()));
		}

		for (std::set<FileControlConnection *>::iterator it = ms_connections.begin(); it != ms_connections.end(); ++it)
		{
			FileControlConnection * conn = *it;
			if (conn && conn->isAuthenticated())
			{
				conn->sendReloadNotify(relativePath, action.reloadCommand);
			}
		}
	}

	LOG("FileControl", ("Hot reload %s for %s", success ? "completed" : "partially failed", relativePath.c_str()));
	return success;
}

// ----------------------------------------------------------------------

bool FileControlServer::runAntBuildCrcTable()
{
	const char * antPath = ConfigFileControl::getAntBuildPath();
	if (!antPath || antPath[0] == '\0')
	{
		WARNING(true, ("FileControl: antBuildPath not configured, cannot rebuild CRC table"));
		return false;
	}

	char command[2048];

#if defined(PLATFORM_WIN32)
	snprintf(command, sizeof(command), "cd /d \"%s\" && ant build_object_crc_table", antPath);
#else
	snprintf(command, sizeof(command), "cd \"%s\" && ant build_object_crc_table", antPath);
#endif

	LOG("FileControl", ("Executing: %s", command));

	int result = system(command);
	if (result != 0)
	{
		WARNING(true, ("FileControl: ant build_object_crc_table failed with exit code %d", result));
		return false;
	}

	LOG("FileControl", ("CRC table rebuild completed successfully"));

	distributeCrcTableToClients();
	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::runAntProcessTemplates()
{
	const char * antPath = ConfigFileControl::getAntBuildPath();
	if (!antPath || antPath[0] == '\0')
	{
		WARNING(true, ("FileControl: antBuildPath not configured, cannot run process_templates"));
		return false;
	}

	char command[2048];

#if defined(PLATFORM_WIN32)
	snprintf(command, sizeof(command), "cd /d \"%s\" && ant process_templates", antPath);
#else
	snprintf(command, sizeof(command), "cd \"%s\" && ant process_templates", antPath);
#endif

	LOG("FileControl", ("Executing: %s", command));

	int result = system(command);
	if (result != 0)
	{
		WARNING(true, ("FileControl: ant process_templates failed with exit code %d", result));
		return false;
	}

	LOG("FileControl", ("process_templates completed successfully"));
	return true;
}

// ----------------------------------------------------------------------

void FileControlServer::distributeCrcTableToClients()
{
	std::string crcPath = "data/sku.0/sys.shared/compiled/game/misc/object_template_crc_string_table.iff";
	std::vector<unsigned char> crcData;
	if (loadFile(crcPath, crcData))
	{
		distributeToClients(crcPath, crcData);
		LOG("FileControl", ("Distributed CRC table to all clients"));
	}
	else
	{
		WARNING(true, ("FileControl: Could not load CRC table for distribution"));
	}
}

// ----------------------------------------------------------------------

bool FileControlServer::sendReloadCommandToGameServers(const std::string & command, const std::string & target)
{
	LOG("FileControl", ("Sending reload to game servers: %s %s", command.c_str(), target.c_str()));

	FileControlReloadHandler::handleReloadNotify(target, command);

	printf("[RELOAD] %s %s -> all game servers (via ReloadHandler + RELOAD_NOTIFY broadcast)\n", command.c_str(), target.c_str());
	return true;
}

// ----------------------------------------------------------------------

void FileControlServer::distributeToClients(const std::string & relativePath, const std::vector<unsigned char> & data)
{
	int distributed = 0;
	int rateLimitKBps = ConfigFileControl::getRateLimitKBps();
	unsigned long rateLimitBytes = (rateLimitKBps > 0) ? static_cast<unsigned long>(rateLimitKBps) * 1024UL : 0;

	for (std::set<FileControlConnection *>::iterator it = ms_connections.begin(); it != ms_connections.end(); ++it)
	{
		FileControlConnection * conn = *it;
		if (conn && conn->isAuthenticated())
		{
			if (rateLimitBytes > 0 && ms_bytesSentThisSecond + data.size() > rateLimitBytes)
			{
				LOG("FileControl", ("Rate limit reached (%lu/%lu bytes), deferring distribution to %s:%d",
					ms_bytesSentThisSecond, rateLimitBytes,
					conn->getRemoteAddress().c_str(), conn->getRemotePort()));
				continue;
			}

			conn->sendFileData(relativePath, data, ConfigFileControl::getEnableCompression());
			conn->sendReloadNotify(relativePath, "update");
			ms_bytesSentThisSecond += static_cast<unsigned long>(data.size());
			++distributed;
		}
	}

	LOG("FileControl", ("Distributed %s to %d connected clients", relativePath.c_str(), distributed));
}

// ======================================================================
// Connection management
// ======================================================================

void FileControlServer::onConnectionOpened(FileControlConnection * connection)
{
	if (!connection)
		return;

	if (static_cast<int>(ms_connections.size()) >= ConfigFileControl::getFileServerMaxConnections())
	{
		connection->sendError("Server at maximum connections");
		connection->disconnect();
		LOG("FileControl", ("Connection rejected - max connections reached"));
		return;
	}

	ms_connections.insert(connection);
	LOG("FileControl", ("Client connected (%d total)", static_cast<int>(ms_connections.size())));
}

// ----------------------------------------------------------------------

void FileControlServer::onConnectionClosed(FileControlConnection * connection)
{
	if (!connection)
		return;

	ms_connections.erase(connection);
	LOG("FileControl", ("Client disconnected (%d remaining)", static_cast<int>(ms_connections.size())));
}

// ----------------------------------------------------------------------

void FileControlServer::onFileUploadReceived(FileControlConnection * connection, const std::string & relativePath, const std::vector<unsigned char> & data, bool compressed)
{
	if (!connection || !connection->isAuthenticated())
	{
		if (connection)
		{
			connection->sendError("Not authenticated");
			logAuthFailureToCentral(connection->getRemoteAddress(), connection->getRemotePort(), connection->getMachineId());
		}
		return;
	}

	FileControlQueue::QueueEntry entry;
	entry.operation = FileControlQueue::OT_UPLOAD;
	entry.relativePath = relativePath;
	entry.fileData = data;
	entry.compressed = compressed;
	ms_queue.enqueue(entry);

	connection->sendStatus("File queued for processing: " + relativePath);
}

// ----------------------------------------------------------------------

void FileControlServer::onFileDownloadRequested(FileControlConnection * connection, const std::string & relativePath)
{
	if (!connection || !connection->isAuthenticated())
	{
		if (connection)
			connection->sendError("Not authenticated");
		return;
	}

	std::vector<unsigned char> data;
	if (loadFile(relativePath, data))
	{
		if (ConfigFileControl::getEnableCompression())
		{
			std::vector<unsigned char> compressed;
			if (compressData(data, compressed))
			{
				connection->sendFileData(relativePath, compressed, true);
				return;
			}
		}
		connection->sendFileData(relativePath, data, false);
	}
	else
	{
		connection->sendError("File not found: " + relativePath);
	}
}

// ----------------------------------------------------------------------

void FileControlServer::onFileCompareRequested(FileControlConnection * connection, const std::string & relativePath)
{
	if (!connection || !connection->isAuthenticated())
	{
		if (connection)
			connection->sendError("Not authenticated");
		return;
	}

	unsigned long size = getFileSize(relativePath);
	unsigned long crc = getFileCrc(relativePath);
	connection->sendCompareResponse(relativePath, size, crc);
}

// ----------------------------------------------------------------------

void FileControlServer::onFileTestRequested(FileControlConnection * connection, const std::string & relativePath)
{
	if (!connection)
		return;

	bool exists = fileExists(relativePath);
	char info[512];
	snprintf(info, sizeof(info), "File %s: %s (server reachable, channel=%s)",
		relativePath.c_str(),
		exists ? "exists" : "not found",
		ConfigFileControl::getChannel());

	connection->sendTestResponse(true, info);
}

// ----------------------------------------------------------------------

void FileControlServer::onFileListRequested(FileControlConnection * connection, const std::string & rootPath)
{
	if (!connection || !connection->isAuthenticated())
	{
		if (connection)
			connection->sendError("Not authenticated");
		return;
	}

	std::vector<std::string> files;
	std::vector<unsigned long> sizes;
	std::vector<unsigned long> crcs;

	collectDirectoryFiles(ms_dataRoot, rootPath, files, sizes, crcs);

	connection->sendFileListResponse(files, sizes);
	LOG("FileControl", ("Sent directory listing for %s (%d files)", rootPath.c_str(), static_cast<int>(files.size())));
}

// ======================================================================
// Auth failure logging
// ======================================================================

void FileControlServer::logAuthFailureToCentral(const std::string & remoteAddress, int remotePort, const std::string & machineId)
{
	WARNING(true, ("FileControl: SECURITY - Unauthenticated access attempt from %s:%d (machineId=%s). Logging to CentralServer.",
		remoteAddress.c_str(), remotePort, machineId.c_str()));

	LOG("FileControl", ("AUTH_FAILURE ip=%s port=%d machineId=%s channel=%s",
		remoteAddress.c_str(), remotePort, machineId.c_str(), ms_channel.c_str()));

	printf("[SECURITY] Auth failure: %s:%d machineId=%s\n", remoteAddress.c_str(), remotePort, machineId.c_str());
}

// ======================================================================
// Directory traversal
// ======================================================================

void FileControlServer::collectDirectoryFiles(const std::string & dirPath, const std::string & prefix, std::vector<std::string> & outFiles, std::vector<unsigned long> & outSizes, std::vector<unsigned long> & outCrcs)
{
	std::string fullDir = dirPath + prefix;

#if defined(PLATFORM_WIN32)
	std::string searchPath = fullDir;
	if (!searchPath.empty() && searchPath[searchPath.size() - 1] != '\\' && searchPath[searchPath.size() - 1] != '/')
		searchPath += "\\";
	searchPath += "*";

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do
	{
		std::string name = findData.cFileName;
		if (name == "." || name == "..")
			continue;

		std::string relativePath = prefix;
		if (!relativePath.empty() && relativePath[relativePath.size() - 1] != '/' && relativePath[relativePath.size() - 1] != '\\')
			relativePath += "/";
		relativePath += name;

		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			relativePath += "/";
			outFiles.push_back(relativePath);
			outSizes.push_back(0);
			outCrcs.push_back(0);
		}
		else
		{
			outFiles.push_back(relativePath);
			outSizes.push_back(static_cast<unsigned long>(findData.nFileSizeLow));
			outCrcs.push_back(0);
		}
	} while (FindNextFileA(hFind, &findData));

	FindClose(hFind);
#else
	DIR * dir = opendir(fullDir.c_str());
	if (!dir)
		return;

	struct dirent * entry;
	while ((entry = readdir(dir)) != 0)
	{
		std::string name = entry->d_name;
		if (name == "." || name == "..")
			continue;

		std::string relativePath = prefix;
		if (!relativePath.empty() && relativePath[relativePath.size() - 1] != '/')
			relativePath += "/";
		relativePath += name;

		std::string absolutePath = dirPath + relativePath;
		struct stat st;
		if (stat(absolutePath.c_str(), &st) == 0)
		{
			if (S_ISDIR(st.st_mode))
			{
				relativePath += "/";
				outFiles.push_back(relativePath);
				outSizes.push_back(0);
				outCrcs.push_back(0);
			}
			else
			{
				outFiles.push_back(relativePath);
				outSizes.push_back(static_cast<unsigned long>(st.st_size));
				outCrcs.push_back(0);
			}
		}
	}

	closedir(dir);
#endif
}

// ======================================================================
// dsrc guard
// ======================================================================

bool FileControlServer::isDsrcPath(const std::string & relativePath)
{
	std::string lower = relativePath;
	for (size_t i = 0; i < lower.size(); ++i)
		lower[i] = static_cast<char>(tolower(lower[i]));

	if (lower.find("dsrc/") == 0 || lower.find("dsrc\\") == 0)
		return true;

	return false;
}

// ======================================================================
// Rollback
// ======================================================================

bool FileControlServer::rollbackFile(const std::string & relativePath)
{
	std::string absPath = getAbsolutePath(relativePath);
	std::string backupPath = absPath + ".bak";

	struct stat st;
	if (stat(backupPath.c_str(), &st) != 0)
	{
		WARNING(true, ("FileControl: No backup found for rollback: %s", relativePath.c_str()));
		return false;
	}

	if (::remove(absPath.c_str()) != 0 && fileExists(relativePath))
	{
		WARNING(true, ("FileControl: Could not remove current file for rollback: %s", relativePath.c_str()));
		return false;
	}

	if (rename(backupPath.c_str(), absPath.c_str()) != 0)
	{
		WARNING(true, ("FileControl: Rollback rename failed for %s", relativePath.c_str()));
		return false;
	}

	LOG("FileControl", ("Rolled back %s from backup", relativePath.c_str()));
	return true;
}

// ======================================================================
// Version tracking
// ======================================================================

unsigned long FileControlServer::getFileVersion(const std::string & relativePath)
{
	std::map<std::string, unsigned long>::const_iterator it = ms_versionMap.find(relativePath);
	if (it != ms_versionMap.end())
		return it->second;
	return 0;
}

// ======================================================================
// Channel accessor
// ======================================================================

const std::string & FileControlServer::getChannel()
{
	return ms_channel;
}

// ----------------------------------------------------------------------

int FileControlServer::getConnectedClientCount()
{
	return static_cast<int>(ms_connections.size());
}

// ======================================================================
// Public API for GodClient integration
// ======================================================================

bool FileControlServer::requestSendAsset(const std::string & relativePath)
{
	if (!ms_running)
		return false;

	if (isDsrcPath(relativePath))
	{
		WARNING(true, ("FileControl: Rejecting dsrc path upload: %s", relativePath.c_str()));
		return false;
	}

	if (!validatePath(relativePath))
		return false;

	std::vector<unsigned char> data;
	if (!loadFile(relativePath, data))
	{
		WARNING(true, ("FileControl: Could not load local file for send: %s", relativePath.c_str()));
		return false;
	}

	FileControlQueue::QueueEntry entry;
	entry.operation = FileControlQueue::OT_UPLOAD;
	entry.relativePath = relativePath;
	entry.fileData = data;
	entry.compressed = false;
	ms_queue.enqueue(entry);

	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::requestRetrieveAsset(const std::string & relativePath, std::vector<unsigned char> & outData)
{
	if (!ms_running)
		return false;

	return loadFile(relativePath, outData);
}

// ----------------------------------------------------------------------

bool FileControlServer::requestReloadAsset(const std::string & relativePath)
{
	if (!ms_running)
		return false;

	HotReloadManager::ReloadAction action;
	if (!HotReloadManager::determineReloadAction(relativePath, action))
		return false;
	return executeHotReload(relativePath, action);
}

// ----------------------------------------------------------------------

bool FileControlServer::requestBroadcastUpdate()
{
	if (!ms_running)
		return false;

	LOG("FileControl", ("Broadcast update requested - distributing all queued updates"));

	for (std::map<std::string, unsigned long>::const_iterator it = ms_versionMap.begin(); it != ms_versionMap.end(); ++it)
	{
		std::vector<unsigned char> data;
		if (loadFile(it->first, data))
		{
			distributeToClients(it->first, data);
		}
	}

	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::requestUpdateDbTemplates()
{
	if (!ms_running)
		return false;

	return runAntProcessTemplates();
}

// ----------------------------------------------------------------------

bool FileControlServer::requestVerifyAsset(const std::string & relativePath, unsigned long & outSize, unsigned long & outCrc)
{
	if (!ms_running)
		return false;

	if (!fileExists(relativePath))
		return false;

	outSize = getFileSize(relativePath);
	outCrc = getFileCrc(relativePath);
	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::requestFlush()
{
	if (!ms_running)
		return false;

	LOG("FileControl", ("Flush requested - reloading all tracked assets"));

	for (std::map<std::string, unsigned long>::const_iterator it = ms_versionMap.begin(); it != ms_versionMap.end(); ++it)
	{
		requestReloadAsset(it->first);
	}

	return true;
}

// ----------------------------------------------------------------------

bool FileControlServer::requestDirectoryListing(const std::string & rootPath, std::vector<std::string> & outFiles, std::vector<unsigned long> & outSizes, std::vector<unsigned long> & outCrcs)
{
	if (!ms_running)
		return false;

	collectDirectoryFiles(ms_dataRoot, rootPath, outFiles, outSizes, outCrcs);
	return true;
}

// ======================================================================
