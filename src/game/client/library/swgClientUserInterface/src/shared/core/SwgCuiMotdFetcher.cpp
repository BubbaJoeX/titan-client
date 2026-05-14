// ======================================================================
//
// SwgCuiMotdFetcher.cpp
// copyright (c) 2024
//
// Fetches Message of the Day from a remote JSON endpoint
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiMotdFetcher.h"

#include "sharedFoundation/ConfigFile.h"
#include "sharedDebug/DebugFlags.h"

#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

#include <string>
#include <vector>

// ======================================================================

namespace SwgCuiMotdFetcherNamespace
{
	bool        s_installed = false;
	bool        s_fetching = false;
	bool        s_hasMotd = false;
	bool        s_fetchFailed = false;
	bool        s_fetchRequested = false;
	
	std::string s_motdTitle;
	std::string s_motdText;
	std::string s_motdImage;
	std::string s_motdUrl;

	HANDLE           s_fetchThread = NULL;
	CRITICAL_SECTION s_motdLock;
	bool             s_motdLockInitialized = false;

	// Simple JSON value extraction (finds "key": "value" patterns)
	std::string extractJsonString(std::string const & json, std::string const & key)
	{
		std::string searchKey = "\"" + key + "\"";
		size_t keyPos = json.find(searchKey);
		if (keyPos == std::string::npos)
			return "";

		// Find the colon after the key
		size_t colonPos = json.find(':', keyPos + searchKey.length());
		if (colonPos == std::string::npos)
			return "";

		// Find the opening quote of the value
		size_t startQuote = json.find('"', colonPos + 1);
		if (startQuote == std::string::npos)
			return "";

		// Find the closing quote (handle escaped quotes)
		size_t endQuote = startQuote + 1;
		while (endQuote < json.length())
		{
			if (json[endQuote] == '"' && json[endQuote - 1] != '\\')
				break;
			++endQuote;
		}

		if (endQuote >= json.length())
			return "";

		std::string value = json.substr(startQuote + 1, endQuote - startQuote - 1);

		// Handle basic escape sequences
		std::string result;
		result.reserve(value.length());
		for (size_t i = 0; i < value.length(); ++i)
		{
			if (value[i] == '\\' && i + 1 < value.length())
			{
				char next = value[i + 1];
				if (next == 'n')
				{
					result += '\n';
					++i;
				}
				else if (next == 'r')
				{
					result += '\r';
					++i;
				}
				else if (next == 't')
				{
					result += '\t';
					++i;
				}
				else if (next == '"')
				{
					result += '"';
					++i;
				}
				else if (next == '\\')
				{
					result += '\\';
					++i;
				}
				else
				{
					result += value[i];
				}
			}
			else
			{
				result += value[i];
			}
		}

		return result;
	}

	DWORD WINAPI fetchThreadProc(LPVOID)
	{
		EnterCriticalSection(&s_motdLock);
		s_fetching = true;
		s_fetchFailed = false;
		s_hasMotd = false;
		LeaveCriticalSection(&s_motdLock);

		std::string urlCopy;
		EnterCriticalSection(&s_motdLock);
		urlCopy = s_motdUrl;
		LeaveCriticalSection(&s_motdLock);

		HINTERNET const hInternet = InternetOpenA(
			"SWGTitan/1.0",
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0
		);

		if (!hInternet)
		{
			EnterCriticalSection(&s_motdLock);
			s_fetchFailed = true;
			s_fetching = false;
			LeaveCriticalSection(&s_motdLock);
			return 1;
		}

		HINTERNET const hUrl = InternetOpenUrlA(
			hInternet,
			urlCopy.c_str(),
			NULL,
			0,
			INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
			0
		);

		if (!hUrl)
		{
			InternetCloseHandle(hInternet);
			EnterCriticalSection(&s_motdLock);
			s_fetchFailed = true;
			s_fetching = false;
			LeaveCriticalSection(&s_motdLock);
			return 1;
		}

		std::vector<char> buffer;
		char readBuffer[4096];
		DWORD bytesRead = 0;

		while (InternetReadFile(hUrl, readBuffer, sizeof(readBuffer), &bytesRead) && bytesRead > 0)
		{
			buffer.insert(buffer.end(), readBuffer, readBuffer + bytesRead);
		}

		InternetCloseHandle(hUrl);
		InternetCloseHandle(hInternet);

		if (buffer.empty())
		{
			EnterCriticalSection(&s_motdLock);
			s_fetchFailed = true;
			s_fetching = false;
			LeaveCriticalSection(&s_motdLock);
			return 1;
		}

		buffer.push_back('\0');
		std::string const jsonResponse(&buffer[0]);

		std::string parsedTitle = extractJsonString(jsonResponse, "title");
		std::string parsedText = extractJsonString(jsonResponse, "text");
		std::string parsedImage = extractJsonString(jsonResponse, "image");

		if (!parsedImage.empty())
		{
			size_t const ddsPos = parsedImage.rfind(".dds");
			if (ddsPos != std::string::npos && ddsPos == parsedImage.length() - 4)
				parsedImage = parsedImage.substr(0, ddsPos);
		}

		EnterCriticalSection(&s_motdLock);
		s_motdTitle.swap(parsedTitle);
		s_motdText.swap(parsedText);
		s_motdImage.swap(parsedImage);

		if (!s_motdTitle.empty() || !s_motdText.empty())
			s_hasMotd = true;
		else
			s_fetchFailed = true;

		s_fetching = false;
		LeaveCriticalSection(&s_motdLock);
		return 0;
	}
}

using namespace SwgCuiMotdFetcherNamespace;

// ======================================================================

void SwgCuiMotdFetcher::install()
{
	DEBUG_FATAL(s_installed, ("SwgCuiMotdFetcher already installed.\n"));

	InitializeCriticalSection(&s_motdLock);
	s_motdLockInitialized = true;

	s_installed = true;
	s_fetching = false;
	s_hasMotd = false;
	s_fetchFailed = false;
	s_fetchRequested = false;
	s_fetchThread = NULL;

	// Get MOTD URL from config, default to swgtitan.org/motd.json
	s_motdUrl = ConfigFile::getKeyString("ClientUserInterface", "motdUrl", "https://swgtitan.org/motd.json");

	// Automatically fetch MOTD on install if URL is configured
	if (!s_motdUrl.empty())
	{
		fetchMotd();
	}
}

// ----------------------------------------------------------------------

void SwgCuiMotdFetcher::remove()
{
	DEBUG_FATAL(!s_installed, ("SwgCuiMotdFetcher not installed.\n"));

	if (s_fetchThread != NULL)
	{
		WaitForSingleObject(s_fetchThread, 5000);
		CloseHandle(s_fetchThread);
		s_fetchThread = NULL;
	}

	EnterCriticalSection(&s_motdLock);
	s_installed = false;
	s_motdTitle.clear();
	s_motdText.clear();
	s_motdImage.clear();
	s_motdUrl.clear();
	s_fetchRequested = false;
	s_fetching = false;
	s_hasMotd = false;
	s_fetchFailed = false;
	LeaveCriticalSection(&s_motdLock);

	if (s_motdLockInitialized)
	{
		DeleteCriticalSection(&s_motdLock);
		s_motdLockInitialized = false;
	}
}

// ----------------------------------------------------------------------

void SwgCuiMotdFetcher::update(float)
{
	if (!s_motdLockInitialized)
		return;

	EnterCriticalSection(&s_motdLock);

	if (s_fetchThread != NULL && !s_fetching)
	{
		HANDLE const finished = s_fetchThread;
		s_fetchThread = NULL;
		LeaveCriticalSection(&s_motdLock);
		CloseHandle(finished);
		EnterCriticalSection(&s_motdLock);
	}

	if (s_fetchRequested && !s_fetching && s_fetchThread == NULL)
	{
		s_fetchRequested = false;
		HANDLE const th = CreateThread(NULL, 0, fetchThreadProc, NULL, 0, NULL);
		if (th)
			s_fetchThread = th;
		else
			s_fetchFailed = true;
	}

	LeaveCriticalSection(&s_motdLock);
}

// ----------------------------------------------------------------------

void SwgCuiMotdFetcher::fetchMotd()
{
	if (!s_motdLockInitialized)
		return;

	EnterCriticalSection(&s_motdLock);
	if (!s_fetching && !s_motdUrl.empty())
		s_fetchRequested = true;
	LeaveCriticalSection(&s_motdLock);
}

// ----------------------------------------------------------------------

bool SwgCuiMotdFetcher::hasMotd()
{
	if (!s_motdLockInitialized)
		return false;

	EnterCriticalSection(&s_motdLock);
	bool const r = s_hasMotd;
	LeaveCriticalSection(&s_motdLock);
	return r;
}

// ----------------------------------------------------------------------

std::string SwgCuiMotdFetcher::getMotdTitle()
{
	if (!s_motdLockInitialized)
		return std::string();

	EnterCriticalSection(&s_motdLock);
	std::string const copy = s_motdTitle;
	LeaveCriticalSection(&s_motdLock);
	return copy;
}

// ----------------------------------------------------------------------

std::string SwgCuiMotdFetcher::getMotdText()
{
	if (!s_motdLockInitialized)
		return std::string();

	EnterCriticalSection(&s_motdLock);
	std::string const copy = s_motdText;
	LeaveCriticalSection(&s_motdLock);
	return copy;
}

// ----------------------------------------------------------------------

std::string SwgCuiMotdFetcher::getMotdImage()
{
	if (!s_motdLockInitialized)
		return std::string();

	EnterCriticalSection(&s_motdLock);
	std::string const copy = s_motdImage;
	LeaveCriticalSection(&s_motdLock);
	return copy;
}

// ----------------------------------------------------------------------

bool SwgCuiMotdFetcher::isFetching()
{
	if (!s_motdLockInitialized)
		return false;

	EnterCriticalSection(&s_motdLock);
	bool const r = s_fetching;
	LeaveCriticalSection(&s_motdLock);
	return r;
}

// ----------------------------------------------------------------------

bool SwgCuiMotdFetcher::hasFetchFailed()
{
	if (!s_motdLockInitialized)
		return false;

	EnterCriticalSection(&s_motdLock);
	bool const r = s_fetchFailed;
	LeaveCriticalSection(&s_motdLock);
	return r;
}

// ======================================================================
