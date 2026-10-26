// ======================================================================

//

// SwgCuiDynamicBunkerFloorplan.h

// copyright 2026 Titan

//

// Floorplan-style bunker room picker with catalog, layout map, and custom placement.

// ======================================================================



#ifndef INCLUDED_SwgCuiDynamicBunkerFloorplan_H

#define INCLUDED_SwgCuiDynamicBunkerFloorplan_H



#include "clientUserInterface/CuiMediator.h"

#include "UIEventCallback.h"

#include "sharedFoundation/NetworkId.h"

#include "sharedMath/Transform.h"

#include "sharedNetworkMessages/DynamicBunkerMessages.h"



#include <string>

#include <vector>



class CuiWidget3dObjectListViewer;

class Object;

class UIBaseObject;

class UIButton;

class UIList;

class UIPage;

class UITabbedPane;

class UIText;

class UITextbox;



// ======================================================================



class SwgCuiDynamicBunkerFloorplan :

	public CuiMediator,

	public UIEventCallback

{

public:



	explicit SwgCuiDynamicBunkerFloorplan(UIPage & page);

	virtual ~SwgCuiDynamicBunkerFloorplan();



	virtual void performActivate();

	virtual void performDeactivate();

	virtual void update(float deltaTimeSecs);



	virtual void OnButtonPressed(UIWidget * context);

	virtual void OnGenericSelectionChanged(UIWidget * context);

	virtual void OnTextboxChanged(UIWidget * context);

	virtual void OnTabbedPaneChanged(UIWidget * context);

	virtual bool OnMessage(UIWidget * context, UIMessage const & msg);



	void setSession(DynamicBunkerOpenFloorplanMessage const & message);

	void notifyBuildingPortalsChanged(NetworkId const & buildingId);

	void deferPortalNotify(NetworkId const & buildingId);

	NetworkId getSessionBuildingId() const;

	// Called from SwgCuiHud when the player clicks the world view while placing portal corners.
	static bool tryConsumeWorldPlaceClick();

	// Called from SwgCuiHud when the player presses Ctrl+Enter while placing a doorway.
	static bool tryConsumeWorldPlaceKey(UIMessage const & msg);

	static SwgCuiDynamicBunkerFloorplan * getActiveInstance();

private:



	SwgCuiDynamicBunkerFloorplan(SwgCuiDynamicBunkerFloorplan const &);

	SwgCuiDynamicBunkerFloorplan & operator=(SwgCuiDynamicBunkerFloorplan const &);



	void applySessionData(DynamicBunkerOpenFloorplanMessage const & message);

	void refreshUiFromSession();

	void rebuildSocketsFromLocalBuilding();

	void buildLocalRoomCatalog();

	void refreshRoomList();

	void refreshSocketList();

	void refreshFloorMap();

	void requestFloorMapRefresh();

	void clearFloorMapNodes();

	void clearLayoutMapOverlay();

	void updateLayoutMapViewer();

	void createMapButton(int socketIndex, float canvasX, float canvasY);

	NetworkId resolveBuildingId() const;

	int findSocketIndex(int cellIndex, int portalIndex) const;

	void updatePreview();

	void clearPreviewObject();

	void assignSelectedRoom();

	void executePendingAssignIfReady();

	void unassignSelectedSocket();

	void revertBuildingToRawPob();

	void createCustomSocket();

	void startPlacePointsMode();

	void cancelPlacePointsMode();

	void updateSnapPlacementPreview();

	char const * resolveSnapPreviewDoorStyle() const;

	bool getSelectedGraftCellIndex(int & outGraftedCellIndex) const;

	void nudgeSelectedGraft(Vector const & delta_cell, float deltaYaw);

	void applySelectedGraftAdjust();

	void confirmSnapPlacement();

	void clearSnapGhost();

	void clearPlacePointMarkers();

	bool handleWorldPlaceClick(int mouseX, int mouseY);

	bool isClickInsideFloorplanPanel(UIBaseObject const * widget) const;

	void updatePlacePointVisuals();

	static bool getCatalogPortalDimensions(
		DynamicBunkerOpenFloorplanMessage::RoomEntry const & room,
		float & outWidth,
		float & outHeight);

	static void computePortalTransformFromWallHit(
		Vector const & hitPoint_cell,
		Vector const & wallNormalIntoCell_cell,
		float doorwayWidth,
		float doorwayHeight,
		Transform & outTransform_cell,
		Vector const * playerPos_cell = 0);

	int resolvePlaceCellIndex() const;

	int resolvePlayerCellIndex() const;

	void updatePlaceCellDisplay();

	void autoSelectExtensionSocketInPlayerCell(bool force);

	void selectSocketByIndex(int socketIndex);

	void updateActionButtons();

	void updateStatus(char const * text);

	bool selectedSocketIsOpen() const;

	int  catalogIndexFromListRow(int listRow) const;

	int  listRowFromCatalogIndex(int catalogIndex) const;

	void computeWallTransform(int wallIndex, Transform & outTransform_o2p) const;

	static void computePortalTransformFromCorners(
		Vector const & corner0_cell,
		Vector const & corner1_cell,
		Transform & outTransform_cell,
		float & outDoorwayWidth,
		float & outDoorwayHeight,
		Vector const * wallNormal_cell = 0,
		Vector const * playerPos_cell = 0,
		Vector const * cellInteriorCenter_cell = 0);

	void registerWidget(UIBaseObject * widget);



private:



	UIButton * m_buttonAssign;

	UIButton * m_buttonUnassign;

	UIButton * m_buttonRevertPob;

	UIButton * m_buttonCancel;

	UIButton * m_buttonCreatePortal;

	UIButton * m_buttonPlacePoints;

	UIButton * m_buttonAdjustLeft;
	UIButton * m_buttonAdjustRight;
	UIButton * m_buttonAdjustForward;
	UIButton * m_buttonAdjustBack;
	UIButton * m_buttonAdjustRotateLeft;
	UIButton * m_buttonAdjustRotateRight;
	UIButton * m_buttonAdjustApply;

	UIButton * m_placeWallButtons[9];

	UIList * m_listRooms;

	UIList * m_listSockets;

	UITextbox * m_textboxFilter;

	UITextbox * m_textboxPlaceCell;

	UIText * m_textStatus;

	UIText * m_textRoomName;

	UIText * m_textSocketHint;

	UIText * m_textLayoutHint;

	UIText * m_textPlaceHint;

	CuiWidget3dObjectListViewer * m_viewer;

	CuiWidget3dObjectListViewer * m_layoutMapViewer;

	UIPage * m_layoutMapCanvas;

	UIButton * m_mapNodeTemplate;

	UITabbedPane * m_tabs;



	NetworkId m_buildingId;

	NetworkId m_terminalId;

	int m_selectedCellIndex;

	int m_selectedPortalIndex;

	int m_selectedRoomIndex;

	int m_selectedSocketRow;

	int m_lastExtensionAutoSelectCell;

	int m_selectedPlaceWall;

	bool m_placingPortalPoints;

	bool m_snapPreviewValid;

	int m_snapPreviewMissFrames;

	Vector m_placePoint0_cell;

	Vector m_placePoint1_cell;

	Vector m_placePoint0_w;

	Vector m_placePoint1_w;

	Vector m_placeWallNormal_cell;

	bool m_hasPlaceWallNormal;

	NetworkId m_wallHitObjectId;

	Transform m_portalDoorTransform_cell;

	float m_placeDoorwayWidth;

	float m_placeDoorwayHeight;

	Object * m_snapGhostDoor;

	std::string m_snapGhostDoorAppearance;

	std::vector<Object *> m_placePointMarkers;

	static SwgCuiDynamicBunkerFloorplan * s_activePlacePointsInstance;

	static SwgCuiDynamicBunkerFloorplan * s_activeFloorplanInstance;



	DynamicBunkerOpenFloorplanMessage::RoomList m_rooms;

	DynamicBunkerOpenFloorplanMessage::SocketList m_sockets;

	DynamicBunkerOpenFloorplanMessage::BridgeList m_bridges;

	std::vector<int> m_filteredRoomIndices;

	std::vector<UIWidget *> m_mapNodeButtons;

	std::vector<int> m_mapNodeSocketIndices;

	std::vector<UIPage *> m_mapCellOverlays;



	Object * m_previewObject;

	bool m_needsUiRefresh;

	bool m_suppressPortalRefresh;

	int m_pendingCreateSnapCellIndex;

	int m_pendingAssignRoomIndex;

	int m_trackedPlaceCellIndex;

	bool m_hasServerRoomCatalog;

	bool m_deferredFloorMapRefresh;

	NetworkId m_deferredPortalNotifyBuildingId;

	int m_nextMapButtonId;

	UIPage * m_overlayCallbackPage;

};



// ======================================================================



#endif

