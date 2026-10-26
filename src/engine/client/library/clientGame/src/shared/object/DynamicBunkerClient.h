// ======================================================================
//
// DynamicBunkerClient.h
// copyright 2026 Titan
//
// ======================================================================

#ifndef INCLUDED_DynamicBunkerClient_H
#define INCLUDED_DynamicBunkerClient_H

class DynamicBunkerBuildingRevertedMessage;
class DynamicBunkerCustomSocketSyncMessage;
class DynamicBunkerGraftMessage;
class DynamicBunkerGraftTransformMessage;
class DynamicBunkerOpenFloorplanMessage;
class DynamicBunkerUngraftMessage;
class CellProperty;
class NetworkId;
class Object;
class PortalProperty;
class Transform;
struct PortalVolumeCut;
class Vector;

// ======================================================================

class DynamicBunkerClient
{
public:

	static void install();
	static void remove();
	static void handleGraftMessage(DynamicBunkerGraftMessage const &message);
	static void handleGraftTransformMessage(DynamicBunkerGraftTransformMessage const &message);
	static void handleUngraftMessage(DynamicBunkerUngraftMessage const &message);
	static void handleCustomSocketSyncMessage(DynamicBunkerCustomSocketSyncMessage const &message);
	static void handleBuildingRevertedMessage(DynamicBunkerBuildingRevertedMessage const &message);
	static void syncCustomSocketsFromOpenFloorplan(DynamicBunkerOpenFloorplanMessage const &message);
	static void onCellLoaded(Object &building, PortalProperty &portalProperty, int cellIndex);
	static void tryLinkAllPendingGrafts(Object &building, PortalProperty &portalProperty);
	static void finalizeBuildingPortalChanges(Object &building, PortalProperty &portalProperty);

	static void applyPreviewWallCut(
		Object & building,
		int cellIndex,
		Transform const & doorTransform_cell,
		float width,
		float height,
		PortalVolumeCut const * portalVolume = 0);
	static void clearPreviewWallCut(Object & building);
	static void applyPlacementWallCut(
		Object & building,
		int cellIndex,
		Transform const & doorTransform_cell,
		float width,
		float height,
		NetworkId const & wallHitObjectId);
	static void setPreviewWallHitObject(NetworkId const & buildingId, int cellIndex, NetworkId const & wallHitObjectId);
	static void setPendingSocketWallHit(NetworkId const & buildingId, int cellIndex, NetworkId const & wallHitObjectId);
	static void snapDoorTransformYToCellFloor(CellProperty const * cell, Transform & doorTransform_cell);

	static bool raycastCellWallForPlacement(
		PortalProperty const & portalProperty,
		int cellIndex,
		Vector const & worldStart,
		Vector const & worldEnd,
		Vector & outWorldPoint,
		Vector & outWorldNormal,
		NetworkId & outHitObjectId);

	static bool buildPortalVolumeCutFromDimensions(float width, float height, PortalVolumeCut & outVolume);
	static bool buildPortalVolumeCutFromDoorStyle(
		char const * doorStyle,
		float fallbackWidth,
		float fallbackHeight,
		PortalVolumeCut & outVolume);
	static bool buildDonorPortalVolumeCut(
		char const * donorPob,
		int donorCellIndex,
		int donorPortalIndex,
		PortalVolumeCut & outVolume);
	static void ensureRightHandedCellTransform(Transform & cellTransform_o2p);

private:

	DynamicBunkerClient();
	DynamicBunkerClient(DynamicBunkerClient const &);
	DynamicBunkerClient &operator =(DynamicBunkerClient const &);
};

// ======================================================================

#endif
