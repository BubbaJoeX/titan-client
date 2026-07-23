// ======================================================================
//
// SwgCuiDynamicBunkerFloorplan.h
// copyright 2026 Titan
//
// Floorplan-style bunker room picker with top-down 3D preview and snap sockets.
// ======================================================================

#ifndef INCLUDED_SwgCuiDynamicBunkerFloorplan_H
#define INCLUDED_SwgCuiDynamicBunkerFloorplan_H

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedNetworkMessages/DynamicBunkerMessages.h"

#include <string>
#include <vector>

class CuiWidget3dObjectListViewer;
class Object;
class UIButton;
class UIList;
class UIPage;
class UIText;

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

	virtual void OnButtonPressed(UIWidget * context);
	virtual void OnGenericSelectionChanged(UIWidget * context);

	void setSession(DynamicBunkerOpenFloorplanMessage const & message);

private:

	SwgCuiDynamicBunkerFloorplan(SwgCuiDynamicBunkerFloorplan const &);
	SwgCuiDynamicBunkerFloorplan & operator=(SwgCuiDynamicBunkerFloorplan const &);

	void refreshRoomList();
	void refreshSocketList();
	void updatePreview();
	void clearPreviewObject();
	void assignSelectedRoom();
	void updateStatus(char const * text);

private:

	UIButton * m_buttonAssign;
	UIButton * m_buttonCancel;
	UIButton * m_buttonClose;
	UIList * m_listRooms;
	UIList * m_listSockets;
	UIText * m_textStatus;
	UIText * m_textRoomName;
	UIText * m_textSocketHint;
	CuiWidget3dObjectListViewer * m_viewer;

	NetworkId m_buildingId;
	NetworkId m_terminalId;
	int m_selectedCellIndex;
	int m_selectedPortalIndex;
	int m_selectedRoomRow;
	int m_selectedSocketRow;

	DynamicBunkerOpenFloorplanMessage::RoomList m_rooms;
	DynamicBunkerOpenFloorplanMessage::SocketList m_sockets;

	Object * m_previewObject;
};

// ======================================================================

#endif
