// ======================================================================
//
// DynamicBunkerClient.h
// copyright 2026 Titan
//
// ======================================================================

#ifndef INCLUDED_DynamicBunkerClient_H
#define INCLUDED_DynamicBunkerClient_H

class DynamicBunkerCustomSocketSyncMessage;
class DynamicBunkerGraftMessage;
class DynamicBunkerOpenFloorplanMessage;
class DynamicBunkerUngraftMessage;
class Object;
class PortalProperty;

// ======================================================================

class DynamicBunkerClient
{
public:

	static void install();
	static void remove();
	static void handleGraftMessage(DynamicBunkerGraftMessage const &message);
	static void handleUngraftMessage(DynamicBunkerUngraftMessage const &message);
	static void handleCustomSocketSyncMessage(DynamicBunkerCustomSocketSyncMessage const &message);
	static void syncCustomSocketsFromOpenFloorplan(DynamicBunkerOpenFloorplanMessage const &message);
	static void finalizeBuildingPortalChanges(Object &building, PortalProperty &portalProperty);

private:

	DynamicBunkerClient();
	DynamicBunkerClient(DynamicBunkerClient const &);
	DynamicBunkerClient &operator =(DynamicBunkerClient const &);
};

// ======================================================================

#endif
