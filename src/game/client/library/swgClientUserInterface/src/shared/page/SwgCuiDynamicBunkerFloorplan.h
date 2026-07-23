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

	NetworkId getSessionBuildingId() const;

	// Called from SwgCuiHud when the player clicks the world view while placing portal corners.
	static bool tryConsumeWorldPlaceClick();

	static SwgCuiDynamicBunkerFloorplan * getActiveInstance();

private:



	SwgCuiDynamicBunkerFloorplan(SwgCuiDynamicBunkerFloorplan const &);

	SwgCuiDynamicBunkerFloorplan & operator=(SwgCuiDynamicBunkerFloorplan const &);



	void applySessionData(DynamicBunkerOpenFloorplanMessage const & message);

	void refreshUiFromSession();

	void rebuildSocketsFromLocalBuilding();

	void updateSocketMapCoordsFromLocalBuilding();

	void buildLocalRoomCatalog();

	void refreshRoomList();

	void refreshSocketList();

	void refreshFloorMap();

	void clearFloorMapNodes();

	void clearLayoutMapOverlay();

	void updateLayoutMapViewer();

	void createMapButton(int socketIndex, float canvasX, float canvasY);

	NetworkId resolveBuildingId() const;

	int findSocketIndex(int cellIndex, int portalIndex) const;

	void updatePreview();

	void clearPreviewObject();

	void assignSelectedRoom();

	void unassignSelectedSocket();

	void createCustomSocket();

	void startPlacePointsMode();

	void cancelPlacePointsMode();

	void clearPlacePointMarkers();

	void addPlacePoint(Vector const & worldPoint, NetworkId const & cellNetworkId);

	bool handleWorldPlaceClick(int mouseX, int mouseY);

	bool isClickInsideFloorplanPanel(UIBaseObject const * widget) const;

	void computePortalTransformFromPlacePoints();

	void updatePlacePointVisuals();

	int resolvePlaceCellIndex() const;

	int resolvePlayerCellIndex() const;

	void updatePlaceCellDisplay();

	void selectSocketByIndex(int socketIndex);

	void updateActionButtons();

	void updateStatus(char const * text);

	bool selectedSocketIsOpen() const;

	int  catalogIndexFromListRow(int listRow) const;

	int  listRowFromCatalogIndex(int catalogIndex) const;

	void computeWallTransform(int wallIndex, Transform & outTransform_o2p) const;

	static void computePortalTransformFromCorners(Vector const & corner0_cell, Vector const & corner1_cell, Transform & outTransform_cell, float & outDoorwayWidth, float & outDoorwayHeight);

	void registerWidget(UIBaseObject * widget);



private:



	UIButton * m_buttonAssign;

	UIButton * m_buttonUnassign;

	UIButton * m_buttonCancel;

	UIButton * m_buttonCreatePortal;

	UIButton * m_buttonPlacePoints;

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

	UITabbedPane * m_tabs;



	NetworkId m_buildingId;

	NetworkId m_terminalId;

	int m_selectedCellIndex;

	int m_selectedPortalIndex;

	int m_selectedRoomIndex;

	int m_selectedSocketRow;

	int m_selectedPlaceWall;

	bool m_placingPortalPoints;

	int m_placePointCount;

	bool m_placePointsReady;

	Vector m_placePoint0_cell;

	Vector m_placePoint1_cell;

	Vector m_placePoint0_w;

	Vector m_placePoint1_w;

	Transform m_portalDoorTransform_cell;

	std::vector<Object *> m_placePointMarkers;

	static SwgCuiDynamicBunkerFloorplan * s_activePlacePointsInstance;

	static SwgCuiDynamicBunkerFloorplan * s_activeFloorplanInstance;



	DynamicBunkerOpenFloorplanMessage::RoomList m_rooms;

	DynamicBunkerOpenFloorplanMessage::SocketList m_sockets;

	DynamicBunkerOpenFloorplanMessage::BridgeList m_bridges;

	std::vector<int> m_filteredRoomIndices;

	std::vector<UIButton *> m_mapNodeButtons;

	std::vector<int> m_mapNodeSocketIndices;

	std::vector<UIPage *> m_mapCellOverlays;



	Object * m_previewObject;

	bool m_needsUiRefresh;

	bool m_suppressPortalRefresh;

	int m_pendingCreateSnapCellIndex;

	int m_trackedPlaceCellIndex;

	bool m_hasServerRoomCatalog;

	int m_nextMapButtonId;

	UIPage * m_overlayCallbackPage;

};



// ======================================================================



#endif

