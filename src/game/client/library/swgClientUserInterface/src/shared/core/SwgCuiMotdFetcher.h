// ======================================================================
//
// SwgCuiMotdFetcher.h
// copyright (c) 2024
//
// Fetches Message of the Day from a remote JSON endpoint
//
// ======================================================================

#ifndef INCLUDED_SwgCuiMotdFetcher_H
#define INCLUDED_SwgCuiMotdFetcher_H

#include <string>

// ======================================================================

class SwgCuiMotdFetcher
{
public:
	static void install();
	static void remove();
	static void update(float deltaTimeSecs);

	// Initiates async fetch of MOTD from configured URL
	static void fetchMotd();

	// Returns true if MOTD has been fetched successfully
	static bool hasMotd();

	// Returns the fetched MOTD title
	static std::string const & getMotdTitle();

	// Returns the fetched MOTD text/body
	static std::string const & getMotdText();

	// Returns the fetched MOTD image path (optional)
	static std::string const & getMotdImage();

	// Returns true if fetch is currently in progress
	static bool isFetching();

	// Returns true if fetch failed
	static bool hasFetchFailed();

private:
	SwgCuiMotdFetcher();
	~SwgCuiMotdFetcher();
	SwgCuiMotdFetcher(SwgCuiMotdFetcher const &);
	SwgCuiMotdFetcher & operator=(SwgCuiMotdFetcher const &);
};

// ======================================================================

#endif // INCLUDED_SwgCuiMotdFetcher_H
