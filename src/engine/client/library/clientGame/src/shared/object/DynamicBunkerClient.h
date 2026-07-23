// ======================================================================
//
// DynamicBunkerClient.h
// copyright 2026 Titan
//
// ======================================================================

#ifndef INCLUDED_DynamicBunkerClient_H
#define INCLUDED_DynamicBunkerClient_H

class DynamicBunkerGraftMessage;

// ======================================================================

class DynamicBunkerClient
{
public:

	static void install();
	static void remove();
	static void handleGraftMessage(DynamicBunkerGraftMessage const &message);

private:

	DynamicBunkerClient();
	DynamicBunkerClient(DynamicBunkerClient const &);
	DynamicBunkerClient &operator =(DynamicBunkerClient const &);
};

// ======================================================================

#endif
