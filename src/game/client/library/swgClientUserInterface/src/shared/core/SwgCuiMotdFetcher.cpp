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

	HANDLE      s_fetchThread = NULL;

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
		s_fetching = true;
		s_fetchFailed = false;
		s_hasMotd = false;

		HINTERNET hInternet = InternetOpenA(
			"SWGTitan/1.0",
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0
		);

		if (!hInternet)
		{
			s_fetchFailed = true;
			s_fetching = false;
			return 1;
		}

		HINTERNET hUrl = InternetOpenUrlA(
			hInternet,
			s_motdUrl.c_str(),
			NULL,
			0,
			INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
			0
		);

		if (!hUrl)
		{
			InternetCloseHandle(hInternet);
			s_fetchFailed = true;
			s_fetching = false;
			return 1;
		}

		// Read the response
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
			s_fetchFailed = true;
			s_fetching = false;
			return 1;
		}

		// Null-terminate the buffer
		buffer.push_back('\0');
		std::string jsonResponse(buffer.empty() ? "" : &buffer[0]);

		// Parse JSON response
		// Expected format: {"title": "...", "text": "...", "image": "..."}
		s_motdTitle = extractJsonString(jsonResponse, "title");
		s_motdText = extractJsonString(jsonResponse, "text");
		s_motdImage = extractJsonString(jsonResponse, "image");

		if (!s_motdTitle.empty() || !s_motdText.empty())
		{
			s_hasMotd = true;
		}
		else
		{
			s_fetchFailed = true;
		}

		s_fetching = false;
		return 0;
	}
}

using namespace SwgCuiMotdFetcherNamespace;

// ======================================================================

void SwgCuiMotdFetcher::install()
{
	DEBUG_FATAL(s_installed, ("SwgCuiMotdFetcher already installed.\n"));

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

	// Wait for fetch thread to complete if still running
	if (s_fetchThread != NULL)
	{
		WaitForSingleObject(s_fetchThread, 5000);
		CloseHandle(s_fetchThread);
		s_fetchThread = NULL;
	}

	s_installed = false;
	s_motdTitle.clear();
	s_motdText.clear();
	s_motdImage.clear();
}

// ----------------------------------------------------------------------

void SwgCuiMotdFetcher::update(float)
{
	// Clean up completed fetch thread
	if (s_fetchThread != NULL && !s_fetching)
	{
		CloseHandle(s_fetchThread);
		s_fetchThread = NULL;
	}

	// Start fetch if requested and not already fetching
	if (s_fetchRequested && !s_fetching && s_fetchThread == NULL)
	{
		s_fetchRequested = false;
		s_fetchThread = CreateThread(NULL, 0, fetchThreadProc, NULL, 0, NULL);
	}
}

// ----------------------------------------------------------------------

void SwgCuiMotdFetcher::fetchMotd()
{
	if (s_fetching || s_motdUrl.empty())
		return;

	s_fetchRequested = true;
}

// ----------------------------------------------------------------------

bool SwgCuiMotdFetcher::hasMotd()
{
	return s_hasMotd;
}

// ----------------------------------------------------------------------

std::string const & SwgCuiMotdFetcher::getMotdTitle()
{
	return s_motdTitle;
}

// ----------------------------------------------------------------------

std::string const & SwgCuiMotdFetcher::getMotdText()
{
	return s_motdText;
}

// ----------------------------------------------------------------------

std::string const & SwgCuiMotdFetcher::getMotdImage()
{
	return s_motdImage;
}

// ----------------------------------------------------------------------

bool SwgCuiMotdFetcher::isFetching()
{
	return s_fetching;
}

// ----------------------------------------------------------------------

bool SwgCuiMotdFetcher::hasFetchFailed()
{
	return s_fetchFailed;
}

// ======================================================================
