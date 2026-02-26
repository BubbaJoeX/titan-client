// ======================================================================
//
// FileControlClient.cpp
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#include "FirstSwgFileControl.h"
#include "FileControlClient.h"

#include "ConfigFileControl.h"
#include "FileControlConnection.h"

#include "sharedFoundation/Crc.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Os.h"
#include "sharedLog/Log.h"
#include "sharedNetwork/NetworkSetupData.h"

#include "Archive/ByteStream.h"
#include "Archive/AutoByteStream.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

#if defined(PLATFORM_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <windows.h>
#define STAT_FUNC _stat
#define STAT_STRUCT struct _stat
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#define STAT_FUNC stat
#define STAT_STRUCT struct stat
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

// ======================================================================

FileControlClient::LogCallback FileControlClient::ms_logCallback = 0;
int64                   FileControlClient::ms_rawSocket      = -1;
FileControlConnection * FileControlClient::ms_connection     = 0;
bool                    FileControlClient::ms_connected      = false;
bool                    FileControlClient::ms_authenticated  = false;
bool                    FileControlClient::ms_done           = false;
FileControlClient::ClientCommand FileControlClient::ms_pendingCommand = CC_NONE;
std::string             FileControlClient::ms_pendingPath;

#if defined(PLATFORM_WIN32)
static CRITICAL_SECTION s_apiLock;
static bool             s_apiLockInitialized = false;
#endif

// ======================================================================

void FileControlClient::lockApi()
{
#if defined(PLATFORM_WIN32)
	if (!s_apiLockInitialized)
	{
		InitializeCriticalSection(&s_apiLock);
		s_apiLockInitialized = true;
	}
	EnterCriticalSection(&s_apiLock);
#endif
}

void FileControlClient::unlockApi()
{
#if defined(PLATFORM_WIN32)
	LeaveCriticalSection(&s_apiLock);
#endif
}

// ======================================================================

void FileControlClient::setLogCallback(LogCallback cb)
{
	ms_logCallback = cb;
}

// ----------------------------------------------------------------------

void FileControlClient::logMessage(const char * fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (ms_logCallback)
		ms_logCallback(buf);
}

// ======================================================================

void FileControlClient::install()
{
	ExitChain::add(remove, "FileControlClient::remove");
}

// ----------------------------------------------------------------------

void FileControlClient::remove()
{
	if (ms_rawSocket >= 0)
	{
		closesocket(static_cast<SOCKET>(ms_rawSocket));
		ms_rawSocket = -1;
	}
	if (ms_connection)
	{
		ms_connection->disconnect();
		ms_connection = 0;
	}
	ms_connected = false;
	ms_authenticated = false;
}

// ----------------------------------------------------------------------

void FileControlClient::update()
{
	if (ms_done)
		return;

	if (!ms_connected)
	{
		if (!connect())
		{
			ms_done = true;
			return;
		}
	}
}

// ======================================================================
// Raw TCP helpers
// ======================================================================

static bool sendAllBytes(SOCKET s, const char * buf, int len)
{
	int totalSent = 0;
	while (totalSent < len)
	{
		int sent = ::send(s, buf + totalSent, len - totalSent, 0);
		if (sent <= 0)
			return false;
		totalSent += sent;
	}
	return true;
}

static bool recvAllBytes(SOCKET s, char * buf, int len, int timeoutMs)
{
	int totalRecv = 0;
	while (totalRecv < len)
	{
		if (timeoutMs > 0)
		{
			fd_set readSet;
			FD_ZERO(&readSet);
			FD_SET(s, &readSet);
			struct timeval tv;
			tv.tv_sec = timeoutMs / 1000;
			tv.tv_usec = (timeoutMs % 1000) * 1000;
			int sel = select(static_cast<int>(s) + 1, &readSet, 0, 0, &tv);
			if (sel <= 0)
				return false;
		}

		int received = recv(s, buf + totalRecv, len - totalRecv, 0);
		if (received <= 0)
			return false;
		totalRecv += received;
	}
	return true;
}

bool FileControlClient::sendRawMessage(const std::vector<unsigned char> & msg)
{
	if (ms_rawSocket < 0)
		return false;

	SOCKET s = static_cast<SOCKET>(ms_rawSocket);

	uint32 len = static_cast<uint32>(msg.size());
	if (!sendAllBytes(s, reinterpret_cast<const char *>(&len), 4))
		return false;

	if (len > 0)
	{
		if (!sendAllBytes(s, reinterpret_cast<const char *>(&msg[0]), static_cast<int>(len)))
			return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool FileControlClient::recvRawMessage(std::vector<unsigned char> & msg, int timeoutMs)
{
	msg.clear();

	if (ms_rawSocket < 0)
		return false;

	SOCKET s = static_cast<SOCKET>(ms_rawSocket);

	uint32 len = 0;
	if (!recvAllBytes(s, reinterpret_cast<char *>(&len), 4, timeoutMs))
		return false;

	if (len == 0)
		return true;

	if (len > 10 * 1024 * 1024)
		return false;

	msg.resize(len);
	if (!recvAllBytes(s, reinterpret_cast<char *>(&msg[0]), static_cast<int>(len), timeoutMs))
		return false;

	return true;
}

// ----------------------------------------------------------------------

void FileControlClient::handleIncomingMessage(const std::vector<unsigned char> & msg)
{
	if (msg.empty())
		return;

	Archive::ByteStream rbs(&msg[0], static_cast<int>(msg.size()));
	Archive::ReadIterator ri = rbs.begin();
	uint8 msgType = 0;
	Archive::get(ri, msgType);

	switch (msgType)
	{
	case 0x02: // MT_AUTH_RESPONSE
		{
			bool success = false;
			std::string message;
			Archive::get(ri, success);
			Archive::get(ri, message);
			ms_authenticated = success;
			logMessage("FileControl: Auth %s: %s", success ? "OK" : "FAILED", message.c_str());
		}
		break;

	case 0x40: // MT_STATUS
		{
			std::string message;
			Archive::get(ri, message);
			logMessage("FileControl: Server: %s", message.c_str());
		}
		break;

	case 0x41: // MT_ERROR
		{
			std::string message;
			Archive::get(ri, message);
			logMessage("FileControl: Server error: %s", message.c_str());
		}
		break;

	case 0x10: // MT_FILE_UPLOAD (unsolicited file data)
		{
			std::string path;
			Archive::get(ri, path);
			logMessage("FileControl: Discarding unsolicited file data for %s", path.c_str());
		}
		break;

	case 0x14: // MT_FILE_COMPARE_RSP (unsolicited compare)
		{
			std::string path;
			uint32 sz = 0, crc = 0;
			Archive::get(ri, path);
			Archive::get(ri, sz);
			Archive::get(ri, crc);
			logMessage("FileControl: Discarding unsolicited compare for %s", path.c_str());
		}
		break;

	default:
		logMessage("FileControl: Unhandled message 0x%02x (%d bytes)", msgType, static_cast<int>(msg.size()));
		break;
	}
}

// ----------------------------------------------------------------------

bool FileControlClient::recvExpectedMessage(uint8 expectedType, std::vector<unsigned char> & outPayload, int timeoutMs)
{
	outPayload.clear();

	for (int attempt = 0; attempt < 5; ++attempt)
	{
		std::vector<unsigned char> msg;
		if (!recvRawMessage(msg, timeoutMs) || msg.empty())
			return false;

		uint8 msgType = msg[0];
		if (msgType == expectedType)
		{
			outPayload.swap(msg);
			return true;
		}

		handleIncomingMessage(msg);
	}

	logMessage("FileControl: Too many unexpected messages while waiting for 0x%02x", expectedType);
	return false;
}

// ======================================================================
// Connection
// ======================================================================

bool FileControlClient::connect()
{
	const char * host = ConfigFileControl::getClientHost();
	int port = ConfigFileControl::getClientPort();

	logMessage("FileControl: connect() host=%s port=%d", host ? host : "(null)", port);

	if (!host || host[0] == '\0')
	{
		logMessage("FileControl: ERROR - No clientHost in [ClientGame]");
		return false;
	}

	if (port <= 0 || port > 65535)
	{
		logMessage("FileControl: ERROR - Invalid port %d", port);
		return false;
	}

#if defined(PLATFORM_WIN32)
	WSADATA wsaData;
	int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaResult != 0)
	{
		logMessage("FileControl: ERROR - WSAStartup failed (%d)", wsaResult);
		return false;
	}
#endif

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		logMessage("FileControl: ERROR - socket() failed");
		return false;
	}

	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(static_cast<unsigned short>(port));

	unsigned long addr = inet_addr(host);
	if (addr == INADDR_NONE)
	{
		logMessage("FileControl: Resolving '%s'...", host);
		struct hostent * he = gethostbyname(host);
		if (!he)
		{
			logMessage("FileControl: ERROR - Cannot resolve '%s'", host);
			closesocket(sock);
			return false;
		}
		memcpy(&serverAddr.sin_addr, he->h_addr_list[0], sizeof(serverAddr.sin_addr));
		logMessage("FileControl: Resolved to %d.%d.%d.%d",
			(unsigned char)he->h_addr_list[0][0],
			(unsigned char)he->h_addr_list[0][1],
			(unsigned char)he->h_addr_list[0][2],
			(unsigned char)he->h_addr_list[0][3]);
	}
	else
	{
		serverAddr.sin_addr.s_addr = addr;
	}

	logMessage("FileControl: Connecting to %s:%d ...", host, port);

	if (::connect(sock, reinterpret_cast<struct sockaddr *>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
	{
#if defined(PLATFORM_WIN32)
		int err = WSAGetLastError();
		logMessage("FileControl: ERROR - connect() failed (WSA %d)", err);
#else
		logMessage("FileControl: ERROR - connect() failed (errno %d)", errno);
#endif
		closesocket(sock);
		return false;
	}

	ms_rawSocket = static_cast<int64>(sock);
	ms_connected = true;
	logMessage("FileControl: TCP connected to %s:%d", host, port);

	const char * key = ConfigFileControl::getClientFileServerKey();
	logMessage("FileControl: authKey=%s", (key && key[0]) ? key : "(empty)");

	if (key && key[0] != '\0')
	{
		std::string machineId = getMachineId();

		Archive::ByteStream bs;
		Archive::put(bs, static_cast<uint8>(0x01));
		Archive::put(bs, std::string(key));
		Archive::put(bs, machineId);

		std::vector<unsigned char> authMsg(bs.getBuffer(), bs.getBuffer() + bs.getSize());
		logMessage("FileControl: Sending auth (%d bytes)...", static_cast<int>(authMsg.size()));

		if (!sendRawMessage(authMsg))
		{
			logMessage("FileControl: ERROR - Failed to send auth");
			return false;
		}

		logMessage("FileControl: Auth sent, waiting for response...");

		std::vector<unsigned char> response;
		if (recvRawMessage(response, 15000) && !response.empty())
		{
			handleIncomingMessage(response);
		}
		else
		{
			logMessage("FileControl: No auth response (timeout)");
		}
	}
	else
	{
		logMessage("FileControl: No auth key, skipping auth");
	}

	logMessage("FileControl: Connected successfully");
	return true;
}

// ======================================================================
// Command execution (standalone CLI)
// ======================================================================

bool FileControlClient::executeCommand(ClientCommand command, const std::string & filePath)
{
	switch (command)
	{
	case CC_UPDATE_FILE:
		return doUpdateFile(filePath);
	case CC_DOWNLOAD_FILE:
		return doDownloadFile(filePath);
	case CC_COMPARE_FILE:
		return doCompareFile(filePath);
	case CC_TEST_RUN:
		return doTestRun(filePath);
	default:
		printf("ERROR: Unknown command\n");
		return false;
	}
}

// ----------------------------------------------------------------------

bool FileControlClient::doUpdateFile(const std::string & filePath)
{
	printf("Uploading: %s\n", filePath.c_str());

	std::vector<unsigned char> data;
	if (!readLocalFile(filePath, data))
	{
		printf("ERROR: Cannot read local file: %s\n", filePath.c_str());
		return false;
	}

	printf("File size: %d bytes\n", static_cast<int>(data.size()));

	if (!ms_connection || !ms_connected)
	{
		printf("ERROR: Not connected to server\n");
		return false;
	}

	bool compress = ConfigFileControl::getClientEnableCompression();
	ms_connection->sendFileData(filePath, data, compress);

	printf("File sent to server. Waiting for confirmation...\n");
	LOG("FileControl", ("Uploaded %s (%d bytes, compressed=%s)", filePath.c_str(), static_cast<int>(data.size()), compress ? "yes" : "no"));

	return true;
}

// ----------------------------------------------------------------------

bool FileControlClient::doDownloadFile(const std::string & filePath)
{
	printf("Requesting download: %s\n", filePath.c_str());

	if (!ms_connection || !ms_connected)
	{
		printf("ERROR: Not connected to server\n");
		return false;
	}

	ms_connection->sendFileRequest(filePath);
	LOG("FileControl", ("Download requested: %s", filePath.c_str()));

	return true;
}

// ----------------------------------------------------------------------

bool FileControlClient::doCompareFile(const std::string & filePath)
{
	printf("Comparing: %s\n", filePath.c_str());

	if (!ms_connection || !ms_connected)
	{
		printf("ERROR: Not connected to server\n");
		return false;
	}

	unsigned long localSize = getLocalFileSize(filePath);
	unsigned long localCrc = getLocalFileCrc(filePath);

	printf("Local:  size=%lu  crc=0x%08lx\n", localSize, localCrc);

	ms_connection->sendCompareRequest(filePath);
	LOG("FileControl", ("Compare requested: %s (local size=%lu, crc=0x%08lx)", filePath.c_str(), localSize, localCrc));

	return true;
}

// ----------------------------------------------------------------------

bool FileControlClient::doTestRun(const std::string & filePath)
{
	printf("Test run: %s\n", filePath.c_str());
	printf("Testing network connectivity (no transfer)...\n");

	if (!ms_connection || !ms_connected)
	{
		printf("ERROR: Not connected to server\n");
		return false;
	}

	ms_connection->sendTestRequest(filePath);

	printf("Connection: OK\n");
	printf("Server:     %s:%d\n", ConfigFileControl::getClientHost(), ConfigFileControl::getClientPort());
	printf("Auth:       %s\n", ms_authenticated ? "authenticated" : "pending");

	LOG("FileControl", ("Test run for %s - connectivity OK", filePath.c_str()));

	return true;
}

// ======================================================================
// File I/O
// ======================================================================

static std::string resolveLocalPath(const std::string & filePath)
{
	STAT_STRUCT st;
	if (STAT_FUNC(filePath.c_str(), &st) == 0)
		return filePath;

	std::string alt = "../../" + filePath;
	if (STAT_FUNC(alt.c_str(), &st) == 0)
		return alt;

	return filePath;
}

// ----------------------------------------------------------------------

bool FileControlClient::readLocalFile(const std::string & filePath, std::vector<unsigned char> & data)
{
	std::string resolved = resolveLocalPath(filePath);
	FILE * fp = fopen(resolved.c_str(), "rb");
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

	return bytesRead == static_cast<size_t>(fileSize);
}

// ----------------------------------------------------------------------

bool FileControlClient::writeLocalFile(const std::string & filePath, const std::vector<unsigned char> & data)
{
	std::string resolved = resolveLocalPath(filePath);
	FILE * fp = fopen(resolved.c_str(), "wb");
	if (!fp)
		return false;

	if (!data.empty())
	{
		size_t written = fwrite(&data[0], 1, data.size(), fp);
		fclose(fp);
		return written == data.size();
	}

	fclose(fp);
	return true;
}

// ----------------------------------------------------------------------

unsigned long FileControlClient::getLocalFileCrc(const std::string & filePath)
{
	std::vector<unsigned char> data;
	if (!readLocalFile(filePath, data) || data.empty())
		return 0;

	return Crc::calculate(&data[0], static_cast<int>(data.size()));
}

// ----------------------------------------------------------------------

unsigned long FileControlClient::getLocalFileSize(const std::string & filePath)
{
	std::string resolved = resolveLocalPath(filePath);
	STAT_STRUCT st;
	if (STAT_FUNC(resolved.c_str(), &st) != 0)
		return 0;
	return static_cast<unsigned long>(st.st_size);
}

// ----------------------------------------------------------------------

std::string FileControlClient::getMachineId()
{
#if defined(PLATFORM_WIN32)
	char computerName[MAX_COMPUTERNAME_LENGTH + 1];
	DWORD size = sizeof(computerName);
	if (GetComputerNameA(computerName, &size))
		return std::string(computerName);
	return "unknown-win32";
#else
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) == 0)
		return std::string(hostname);
	return "unknown-linux";
#endif
}

// ======================================================================
// GodClient API — raw TCP socket to FileControl server
// ======================================================================

bool FileControlClient::ensureConnected()
{
	if (ms_connected && ms_rawSocket >= 0)
		return true;

	logMessage("FileControl: Not connected, attempting connection...");
	return connect();
}

// ----------------------------------------------------------------------

bool FileControlClient::requestSendAsset(const std::string & relativePath)
{
	lockApi();

	if (!ensureConnected())
	{
		unlockApi();
		return false;
	}

	std::vector<unsigned char> data;
	if (!readLocalFile(relativePath, data))
	{
		logMessage("FileControl: Send failed - cannot read local file: %s", relativePath.c_str());
		unlockApi();
		return false;
	}

	logMessage("FileControl: Sending %s (%u bytes)...", relativePath.c_str(), static_cast<unsigned>(data.size()));

	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(0x10));
	Archive::put(bs, relativePath);
	Archive::put(bs, false);
	uint32 dataSize = static_cast<uint32>(data.size());
	Archive::put(bs, dataSize);
	if (!data.empty())
		bs.put(&data[0], static_cast<int>(data.size()));

	std::vector<unsigned char> msg(bs.getBuffer(), bs.getBuffer() + bs.getSize());
	if (!sendRawMessage(msg))
	{
		logMessage("FileControl: Send failed - network error for %s", relativePath.c_str());
		unlockApi();
		return false;
	}

	logMessage("FileControl: Sent %s OK", relativePath.c_str());
	unlockApi();
	return true;
}

// ----------------------------------------------------------------------

bool FileControlClient::requestRetrieveAsset(const std::string & relativePath, std::vector<unsigned char> & outData)
{
	lockApi();

	if (!ensureConnected())
	{
		unlockApi();
		return false;
	}

	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(0x11));
	Archive::put(bs, relativePath);

	std::vector<unsigned char> msg(bs.getBuffer(), bs.getBuffer() + bs.getSize());
	if (!sendRawMessage(msg))
	{
		unlockApi();
		return false;
	}

	std::vector<unsigned char> response;
	if (!recvExpectedMessage(0x10, response, 30000))
	{
		logMessage("FileControl: Retrieve failed - no response for %s", relativePath.c_str());
		unlockApi();
		return false;
	}

	Archive::ByteStream rbs(&response[0], static_cast<int>(response.size()));
	Archive::ReadIterator ri = rbs.begin();
	uint8 msgType = 0;
	Archive::get(ri, msgType);

	std::string path;
	bool compressed = false;
	uint32 sz = 0;
	Archive::get(ri, path);
	Archive::get(ri, compressed);
	Archive::get(ri, sz);

	logMessage("FileControl: Retrieve %s (%u bytes, compressed=%s)", path.c_str(), sz, compressed ? "yes" : "no");

	outData.resize(sz);
	for (uint32 i = 0; i < sz && ri.getSize() > 0; ++i)
	{
		uint8 byte = 0;
		Archive::get(ri, byte);
		outData[i] = byte;
	}

	if (compressed)
	{
		logMessage("FileControl: Decompressing %u bytes...", sz);
	}

	unlockApi();
	return true;
}

// ----------------------------------------------------------------------

bool FileControlClient::requestReloadAsset(const std::string & relativePath)
{
	if (!ensureConnected())
		return false;

	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(0x30));
	Archive::put(bs, relativePath);
	Archive::put(bs, std::string("asset"));

	std::vector<unsigned char> msg(bs.getBuffer(), bs.getBuffer() + bs.getSize());
	return sendRawMessage(msg);
}

// ----------------------------------------------------------------------

bool FileControlClient::requestBroadcastUpdate()
{
	if (!ensureConnected())
		return false;

	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(0x30));
	Archive::put(bs, std::string("*"));
	Archive::put(bs, std::string("broadcast"));

	std::vector<unsigned char> msg(bs.getBuffer(), bs.getBuffer() + bs.getSize());
	return sendRawMessage(msg);
}

// ----------------------------------------------------------------------

bool FileControlClient::requestUpdateDbTemplates()
{
	if (!ensureConnected())
		return false;

	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(0x30));
	Archive::put(bs, std::string("__process_templates__"));
	Archive::put(bs, std::string("templates"));

	std::vector<unsigned char> msg(bs.getBuffer(), bs.getBuffer() + bs.getSize());
	return sendRawMessage(msg);
}

// ----------------------------------------------------------------------

bool FileControlClient::requestVerifyAsset(const std::string & relativePath, unsigned long & outSize, unsigned long & outCrc)
{
	outSize = 0;
	outCrc = 0;

	lockApi();

	if (!ensureConnected())
	{
		unlockApi();
		return false;
	}

	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(0x13));
	Archive::put(bs, relativePath);

	std::vector<unsigned char> msg(bs.getBuffer(), bs.getBuffer() + bs.getSize());
	if (!sendRawMessage(msg))
	{
		unlockApi();
		return false;
	}

	std::vector<unsigned char> response;
	if (!recvExpectedMessage(0x14, response, 10000))
	{
		unlockApi();
		return false;
	}

	Archive::ByteStream rbs(&response[0], static_cast<int>(response.size()));
	Archive::ReadIterator ri = rbs.begin();
	uint8 msgType = 0;
	Archive::get(ri, msgType);

	std::string path;
	uint32 sz = 0;
	uint32 crc = 0;
	Archive::get(ri, path);
	Archive::get(ri, sz);
	Archive::get(ri, crc);
	outSize = sz;
	outCrc = crc;
	unlockApi();
	return true;
}

// ----------------------------------------------------------------------

bool FileControlClient::requestFlush()
{
	if (!ensureConnected())
		return false;

	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(0x30));
	Archive::put(bs, std::string("__flush__"));
	Archive::put(bs, std::string("flush"));

	std::vector<unsigned char> msg(bs.getBuffer(), bs.getBuffer() + bs.getSize());
	return sendRawMessage(msg);
}

// ----------------------------------------------------------------------

bool FileControlClient::requestDirectoryListing(const std::string & rootPath, std::vector<std::string> & outFiles, std::vector<unsigned long> & outSizes, std::vector<unsigned long> & outCrcs)
{
	outFiles.clear();
	outSizes.clear();
	outCrcs.clear();

	lockApi();

	logMessage("FileControl: Listing '%s'...", rootPath.c_str());

	if (!ensureConnected())
	{
		logMessage("FileControl: Listing failed - not connected");
		unlockApi();
		return false;
	}

	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(0x20));
	Archive::put(bs, rootPath);

	std::vector<unsigned char> msg(bs.getBuffer(), bs.getBuffer() + bs.getSize());
	if (!sendRawMessage(msg))
	{
		logMessage("FileControl: Listing failed - send error");
		unlockApi();
		return false;
	}

	logMessage("FileControl: Listing request sent, waiting for response...");

	std::vector<unsigned char> response;
	if (!recvExpectedMessage(0x21, response, 15000))
	{
		logMessage("FileControl: Listing failed - no response");
		unlockApi();
		return false;
	}

	Archive::ByteStream rbs(&response[0], static_cast<int>(response.size()));
	Archive::ReadIterator ri = rbs.begin();
	uint8 msgType = 0;
	Archive::get(ri, msgType);

	uint32 count = 0;
	Archive::get(ri, count);

	for (uint32 i = 0; i < count && ri.getSize() > 0; ++i)
	{
		std::string fileName;
		uint32 fileSize = 0;
		Archive::get(ri, fileName);
		Archive::get(ri, fileSize);
		outFiles.push_back(fileName);
		outSizes.push_back(fileSize);
		outCrcs.push_back(0);
	}
	logMessage("FileControl: Listed %u entries", count);
	unlockApi();
	return true;
}

// ======================================================================
