// ======================================================================
//
// SwgCuiDynamicBunkerFloorplan.cpp
// copyright 2026 Titan
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiDynamicBunkerFloorplan.h"

#include "clientGame/GameNetwork.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiWidget3dObjectListViewer.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/Object.h"
#include "sharedNetworkMessages/DynamicBunkerMessages.h"

#include "UIButton.h"
#include "UIData.h"
#include "UIList.h"
#include "UIPage.h"
#include "UIText.h"

#include <cstdio>

// ======================================================================

SwgCuiDynamicBunkerFloorplan::SwgCuiDynamicBunkerFloorplan(UIPage & page)
: CuiMediator("SwgCuiDynamicBunkerFloorplan", page),
	UIEventCallback(),
	m_buttonAssign(0),
	m_buttonCancel(0),
	m_buttonClose(0),
	m_listRooms(0),
	m_listSockets(0),
	m_textStatus(0),
	m_textRoomName(0),
	m_textSocketHint(0),
	m_viewer(0),
	m_buildingId(),
	m_terminalId(),
	m_selectedCellIndex(0),
	m_selectedPortalIndex(0),
	m_selectedRoomRow(-1),
	m_selectedSocketRow(-1),
	m_rooms(),
	m_sockets(),
	m_previewObject(0)
{
	getCodeDataObject(TUIButton, m_buttonAssign, "buttonAssign");
	getCodeDataObject(TUIButton, m_buttonCancel, "buttonCancel");
	getCodeDataObject(TUIButton, m_buttonClose, "buttonClose");
	getCodeDataObject(TUIList, m_listRooms, "listRooms");
	getCodeDataObject(TUIList, m_listSockets, "listSockets");
	getCodeDataObject(TUIText, m_textStatus, "textStatus");
	getCodeDataObject(TUIText, m_textRoomName, "textRoomName");
	getCodeDataObject(TUIText, m_textSocketHint, "textSocketHint", true);

	UIWidget * viewerWidget = 0;
	getCodeDataObject(TUIWidget, viewerWidget, "viewer");
	m_viewer = NON_NULL(dynamic_cast<CuiWidget3dObjectListViewer *>(viewerWidget));

	registerMediatorObject(*m_buttonAssign, true);
	registerMediatorObject(*m_buttonCancel, true);
	registerMediatorObject(*m_buttonClose, true);
	registerMediatorObject(*m_listRooms, true);
	registerMediatorObject(*m_listSockets, true);
}

// ----------------------------------------------------------------------

SwgCuiDynamicBunkerFloorplan::~SwgCuiDynamicBunkerFloorplan()
{
	clearPreviewObject();
	m_buttonAssign = 0;
	m_buttonCancel = 0;
	m_buttonClose = 0;
	m_listRooms = 0;
	m_listSockets = 0;
	m_textStatus = 0;
	m_textRoomName = 0;
	m_textSocketHint = 0;
	m_viewer = 0;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::performActivate()
{
	CuiManager::requestPointer(true);
	if (m_viewer)
	{
		m_viewer->setPaused(false);
		m_viewer->setCameraForceTarget(true);
		m_viewer->setRotateSpeed(0.35f);
	}
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::performDeactivate()
{
	CuiManager::requestPointer(false);
	clearPreviewObject();
	if (m_viewer)
		m_viewer->setPaused(true);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::setSession(DynamicBunkerOpenFloorplanMessage const & message)
{
	m_buildingId = message.getBuildingId();
	m_terminalId = message.getTerminalId();
	m_selectedCellIndex = message.getSelectedCellIndex();
	m_selectedPortalIndex = message.getSelectedPortalIndex();
	m_rooms = message.getRooms();
	m_sockets = message.getSockets();
	m_selectedRoomRow = m_rooms.empty() ? -1 : 0;
	m_selectedSocketRow = -1;

	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		if (m_sockets[i].cellIndex == m_selectedCellIndex && m_sockets[i].portalIndex == m_selectedPortalIndex)
		{
			m_selectedSocketRow = static_cast<int>(i);
			break;
		}
	}
	if (m_selectedSocketRow < 0)
	{
		for (size_t i = 0; i < m_sockets.size(); ++i)
		{
			if (m_sockets[i].open)
			{
				m_selectedSocketRow = static_cast<int>(i);
				m_selectedCellIndex = m_sockets[i].cellIndex;
				m_selectedPortalIndex = m_sockets[i].portalIndex;
				break;
			}
		}
	}

	refreshRoomList();
	refreshSocketList();
	updatePreview();
	updateStatus("Select a floorplan module, pick an open snap point, then Assign.");
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::OnButtonPressed(UIWidget * context)
{
	if (context == m_buttonClose || context == m_buttonCancel)
	{
		deactivate();
		return;
	}

	if (context == m_buttonAssign)
		assignSelectedRoom();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::OnGenericSelectionChanged(UIWidget * context)
{
	if (context == m_listRooms)
	{
		m_selectedRoomRow = m_listRooms ? m_listRooms->GetLastSelectedRow() : -1;
		updatePreview();
	}
	else if (context == m_listSockets)
	{
		m_selectedSocketRow = m_listSockets ? m_listSockets->GetLastSelectedRow() : -1;
		if (m_selectedSocketRow >= 0 && m_selectedSocketRow < static_cast<int>(m_sockets.size()))
		{
			m_selectedCellIndex = m_sockets[static_cast<size_t>(m_selectedSocketRow)].cellIndex;
			m_selectedPortalIndex = m_sockets[static_cast<size_t>(m_selectedSocketRow)].portalIndex;
			char buf[160];
			snprintf(buf, sizeof(buf), "Snap socket cell %d portal %d (%s).",
				m_selectedCellIndex, m_selectedPortalIndex,
				m_sockets[static_cast<size_t>(m_selectedSocketRow)].open ? "OPEN" : "linked");
			updateStatus(buf);
		}
	}
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::refreshRoomList()
{
	if (!m_listRooms)
		return;

	m_listRooms->Clear();
	for (size_t i = 0; i < m_rooms.size(); ++i)
	{
		DynamicBunkerOpenFloorplanMessage::RoomEntry const & room = m_rooms[i];
		std::string row = room.displayName;
		if (!room.socketType.empty())
		{
			row += "  [";
			row += room.socketType;
			row += "]";
		}
		m_listRooms->AddRow(Unicode::narrowToWide(row), room.roomId);
	}

	if (m_selectedRoomRow >= 0 && m_selectedRoomRow < static_cast<int>(m_rooms.size()))
		m_listRooms->SelectRow(m_selectedRoomRow);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::refreshSocketList()
{
	if (!m_listSockets)
		return;

	m_listSockets->Clear();
	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[i];
		char buf[192];
		snprintf(buf, sizeof(buf), "%s  cell %d / portal %d  %s",
			socket.open ? "[OPEN]" : "[LINKED]",
			socket.cellIndex,
			socket.portalIndex,
			socket.label.c_str());
		char data[64];
		snprintf(data, sizeof(data), "%d:%d", socket.cellIndex, socket.portalIndex);
		m_listSockets->AddRow(Unicode::narrowToWide(buf), data);
	}

	if (m_selectedSocketRow >= 0 && m_selectedSocketRow < static_cast<int>(m_sockets.size()))
		m_listSockets->SelectRow(m_selectedSocketRow);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::clearPreviewObject()
{
	if (m_viewer)
		m_viewer->clearObjects();

	delete m_previewObject;
	m_previewObject = 0;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::updatePreview()
{
	clearPreviewObject();

	if (m_selectedRoomRow < 0 || m_selectedRoomRow >= static_cast<int>(m_rooms.size()))
	{
		if (m_textRoomName)
			m_textRoomName->SetText(Unicode::narrowToWide("Select a room module"));
		return;
	}

	DynamicBunkerOpenFloorplanMessage::RoomEntry const & room = m_rooms[static_cast<size_t>(m_selectedRoomRow)];
	if (m_textRoomName)
		m_textRoomName->SetText(Unicode::narrowToWide(room.displayName));

	std::string appearance = room.appearanceHint;
	if (appearance.empty())
		appearance = room.donorPob;

	// Prefer .apt / .msh style path; if still a .pob, show status only.
	if (appearance.size() > 4 && appearance.substr(appearance.size() - 4) == ".pob")
	{
		appearance.replace(appearance.size() - 4, 4, ".apt");
	}

	Appearance * const app = AppearanceTemplateList::createAppearance(appearance.c_str());
	if (!app)
	{
		updateStatus(("Preview unavailable for " + appearance).c_str());
		return;
	}

	m_previewObject = new Object;
	m_previewObject->setAppearance(app);
	IGNORE_RETURN(m_previewObject->alter(0.0f));
	m_previewObject->conclude();

	if (m_viewer)
	{
		m_viewer->addObject(*m_previewObject);
		m_viewer->setCameraForceTarget(true);
		m_viewer->recomputeZoom();
		m_viewer->setCameraForceTarget(false);
	}
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::assignSelectedRoom()
{
	if (m_selectedRoomRow < 0 || m_selectedRoomRow >= static_cast<int>(m_rooms.size()))
	{
		updateStatus("Select a floorplan room first.");
		return;
	}

	if (m_selectedSocketRow < 0 || m_selectedSocketRow >= static_cast<int>(m_sockets.size()))
	{
		updateStatus("Select an open snap point first.");
		return;
	}

	DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(m_selectedSocketRow)];
	if (!socket.open)
	{
		updateStatus("That snap point is already linked. Choose an [OPEN] socket.");
		return;
	}

	DynamicBunkerOpenFloorplanMessage::RoomEntry const & room = m_rooms[static_cast<size_t>(m_selectedRoomRow)];
	DynamicBunkerAssignRoomMessage const msg(
		m_buildingId,
		m_terminalId,
		socket.cellIndex,
		socket.portalIndex,
		room.roomId);
	GameNetwork::send(msg, true);

	char buf[256];
	snprintf(buf, sizeof(buf), "Assigning '%s' to cell %d portal %d...",
		room.displayName.c_str(), socket.cellIndex, socket.portalIndex);
	updateStatus(buf);
	deactivate();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::updateStatus(char const * text)
{
	if (m_textStatus && text)
		m_textStatus->SetText(Unicode::narrowToWide(text));
}

// ======================================================================
