// ======================================================================
//
// FileControlConnection.cpp
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#include "FirstSwgFileControl.h"
#include "FileControlConnection.h"

#include "ConfigFileControl.h"
#include "FileControlReloadHandler.h"
#include "FileControlServer.h"

#include "Archive/ByteStream.h"
#include "Archive/AutoByteStream.h"
#include "sharedFoundation/Crc.h"
#include "sharedLog/Log.h"

#include <cstring>

// ======================================================================

FileControlConnection::FileControlConnection(const std::string & remoteAddress, const unsigned short remotePort, const NetworkSetupData & setup) :
Connection(remoteAddress, remotePort, setup),
m_authenticated(false),
m_machineId()
{
}

// ----------------------------------------------------------------------

FileControlConnection::FileControlConnection(UdpConnectionMT * u, TcpClient * t) :
Connection(u, t),
m_authenticated(false),
m_machineId()
{
}

// ----------------------------------------------------------------------

FileControlConnection::~FileControlConnection()
{
}

// ----------------------------------------------------------------------

void FileControlConnection::onConnectionClosed()
{
	LOG("FileControl", ("Connection closed from %s:%d (machineId=%s)", getRemoteAddress().c_str(), getRemotePort(), m_machineId.c_str()));
	if (FileControlServer::isRunning())
		FileControlServer::onConnectionClosed(this);
}

// ----------------------------------------------------------------------

void FileControlConnection::onConnectionOpened()
{
	LOG("FileControl", ("Connection opened from %s:%d", getRemoteAddress().c_str(), getRemotePort()));
	if (FileControlServer::isRunning())
		FileControlServer::onConnectionOpened(this);
}

// ----------------------------------------------------------------------

void FileControlConnection::onReceive(const Archive::ByteStream & bs)
{
	Archive::ReadIterator ri = bs.begin();

	while (ri.getSize() > 0)
	{
		uint8 messageType = 0;
		Archive::get(ri, messageType);

		switch (messageType)
		{
		case MT_AUTH_REQUEST:
			{
				std::string key;
				std::string machineId;
				Archive::get(ri, key);
				Archive::get(ri, machineId);
				m_machineId = machineId;

				const char * serverKey = ConfigFileControl::getFileServerKey();
				if (serverKey && key == serverKey)
				{
					m_authenticated = true;
					sendAuthResponse(true, "Authentication successful");
					LOG("FileControl", ("Client %s:%d authenticated (machineId=%s)", getRemoteAddress().c_str(), getRemotePort(), machineId.c_str()));
				}
				else
				{
					m_authenticated = false;
					sendAuthResponse(false, "Invalid fileServerKey");
					LOG("FileControl", ("FAILED authentication from %s:%d (machineId=%s) - invalid key", getRemoteAddress().c_str(), getRemotePort(), machineId.c_str()));
					WARNING(true, ("FileControl: FAILED authentication from %s:%d (machineId=%s)", getRemoteAddress().c_str(), getRemotePort(), machineId.c_str()));
				}
			}
			break;

		case MT_AUTH_RESPONSE:
			{
				bool success = false;
				std::string message;
				Archive::get(ri, success);
				Archive::get(ri, message);

				m_authenticated = success;
				if (success)
					LOG("FileControl", ("Authenticated with server: %s", message.c_str()));
				else
					WARNING(true, ("FileControl: Authentication failed: %s", message.c_str()));
			}
			break;

		case MT_FILE_UPLOAD:
			{
				std::string relativePath;
				bool compressed = false;
				uint32 dataSize = 0;
				Archive::get(ri, relativePath);
				Archive::get(ri, compressed);
				Archive::get(ri, dataSize);

				std::vector<unsigned char> data;
				if (dataSize > 0)
				{
					data.resize(dataSize);
					for (uint32 i = 0; i < dataSize && ri.getSize() > 0; ++i)
					{
						uint8 byte = 0;
						Archive::get(ri, byte);
						data[i] = byte;
					}
				}

				if (FileControlServer::isRunning())
					FileControlServer::onFileUploadReceived(this, relativePath, data, compressed);
			}
			break;

		case MT_FILE_DOWNLOAD_REQ:
			{
				std::string relativePath;
				Archive::get(ri, relativePath);
				if (FileControlServer::isRunning())
					FileControlServer::onFileDownloadRequested(this, relativePath);
			}
			break;

		case MT_FILE_DOWNLOAD_RSP:
			{
				std::string relativePath;
				bool compressed = false;
				uint32 dataSize = 0;
				Archive::get(ri, relativePath);
				Archive::get(ri, compressed);
				Archive::get(ri, dataSize);

				std::vector<unsigned char> data;
				if (dataSize > 0)
				{
					data.resize(dataSize);
					for (uint32 i = 0; i < dataSize && ri.getSize() > 0; ++i)
					{
						uint8 byte = 0;
						Archive::get(ri, byte);
						data[i] = byte;
					}
				}

				LOG("FileControl", ("Received file download: %s (%u bytes)", relativePath.c_str(), dataSize));
				FileControlReloadHandler::handleReceivedFile(relativePath, data);
			}
			break;

		case MT_FILE_COMPARE_REQ:
			{
				std::string relativePath;
				Archive::get(ri, relativePath);
				if (FileControlServer::isRunning())
					FileControlServer::onFileCompareRequested(this, relativePath);
			}
			break;

		case MT_FILE_COMPARE_RSP:
			{
				std::string relativePath;
				uint32 remoteSize = 0;
				uint32 remoteCrc = 0;
				Archive::get(ri, relativePath);
				Archive::get(ri, remoteSize);
				Archive::get(ri, remoteCrc);

				LOG("FileControl", ("Compare response: %s (size=%u, crc=0x%08x)", relativePath.c_str(), remoteSize, remoteCrc));
			}
			break;

		case MT_FILE_TEST_REQ:
			{
				std::string relativePath;
				Archive::get(ri, relativePath);
				if (FileControlServer::isRunning())
					FileControlServer::onFileTestRequested(this, relativePath);
			}
			break;

		case MT_FILE_TEST_RSP:
			{
				bool reachable = false;
				std::string info;
				Archive::get(ri, reachable);
				Archive::get(ri, info);

				LOG("FileControl", ("Test response: reachable=%s info=%s", reachable ? "yes" : "no", info.c_str()));
			}
			break;

		case MT_FILE_LIST_REQ:
			{
				std::string rootPath;
				Archive::get(ri, rootPath);
				if (FileControlServer::isRunning())
					FileControlServer::onFileListRequested(this, rootPath);
			}
			break;

		case MT_FILE_LIST_RSP:
			{
				uint32 count = 0;
				Archive::get(ri, count);

				for (uint32 i = 0; i < count; ++i)
				{
					std::string fileName;
					uint32 fileSize = 0;
					Archive::get(ri, fileName);
					Archive::get(ri, fileSize);
				}

				LOG("FileControl", ("Received file list: %u files", count));
			}
			break;

		case MT_RELOAD_NOTIFY:
			{
				std::string relativePath;
				std::string reloadType;
				Archive::get(ri, relativePath);
				Archive::get(ri, reloadType);

				LOG("FileControl", ("Reload notification: %s (type=%s)", relativePath.c_str(), reloadType.c_str()));
				FileControlReloadHandler::handleReloadNotify(relativePath, reloadType);
			}
			break;

		case MT_STATUS:
			{
				std::string message;
				Archive::get(ri, message);
				LOG("FileControl", ("Status: %s", message.c_str()));
			}
			break;

		case MT_ERROR:
			{
				std::string message;
				Archive::get(ri, message);
				WARNING(true, ("FileControl: Error from remote: %s", message.c_str()));
			}
			break;

		default:
			WARNING(true, ("FileControl: Unknown message type 0x%02x from %s:%d", messageType, getRemoteAddress().c_str(), getRemotePort()));
			return;
		}
	}
}

// ======================================================================
// Message senders
// ======================================================================

void FileControlConnection::sendAuthRequest(const std::string & key, const std::string & machineId)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_AUTH_REQUEST));
	Archive::put(bs, key);
	Archive::put(bs, machineId);
	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendAuthResponse(bool success, const std::string & message)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_AUTH_RESPONSE));
	Archive::put(bs, success);
	Archive::put(bs, message);
	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendFileData(const std::string & relativePath, const std::vector<unsigned char> & data, bool compressed)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_FILE_UPLOAD));
	Archive::put(bs, relativePath);
	Archive::put(bs, compressed);

	uint32 dataSize = static_cast<uint32>(data.size());
	Archive::put(bs, dataSize);

	if (!data.empty())
	{
		bs.put(&data[0], static_cast<int>(data.size()));
	}

	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendFileRequest(const std::string & relativePath)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_FILE_DOWNLOAD_REQ));
	Archive::put(bs, relativePath);
	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendCompareRequest(const std::string & relativePath)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_FILE_COMPARE_REQ));
	Archive::put(bs, relativePath);
	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendCompareResponse(const std::string & relativePath, unsigned long localSize, unsigned long localCrc)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_FILE_COMPARE_RSP));
	Archive::put(bs, relativePath);
	Archive::put(bs, static_cast<uint32>(localSize));
	Archive::put(bs, static_cast<uint32>(localCrc));
	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendTestRequest(const std::string & relativePath)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_FILE_TEST_REQ));
	Archive::put(bs, relativePath);
	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendTestResponse(bool reachable, const std::string & info)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_FILE_TEST_RSP));
	Archive::put(bs, reachable);
	Archive::put(bs, info);
	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendFileListRequest(const std::string & rootPath)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_FILE_LIST_REQ));
	Archive::put(bs, rootPath);
	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendFileListResponse(const std::vector<std::string> & files, const std::vector<unsigned long> & sizes)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_FILE_LIST_RSP));

	uint32 count = static_cast<uint32>(files.size());
	Archive::put(bs, count);

	for (uint32 i = 0; i < count; ++i)
	{
		Archive::put(bs, files[i]);
		Archive::put(bs, static_cast<uint32>(i < sizes.size() ? sizes[i] : 0));
	}

	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendReloadNotify(const std::string & relativePath, const std::string & reloadType)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_RELOAD_NOTIFY));
	Archive::put(bs, relativePath);
	Archive::put(bs, reloadType);
	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendStatus(const std::string & message)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_STATUS));
	Archive::put(bs, message);
	send(bs, true);
}

// ----------------------------------------------------------------------

void FileControlConnection::sendError(const std::string & message)
{
	Archive::ByteStream bs;
	Archive::put(bs, static_cast<uint8>(MT_ERROR));
	Archive::put(bs, message);
	send(bs, true);
}

// ======================================================================
