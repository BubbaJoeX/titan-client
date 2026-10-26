//======================================================================
//
// SwgCuiCalendar.cpp
// copyright (c) 2026 Titan Project
//
// In-Game Calendar System - Main Calendar Window
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiCalendar.h"
#include "swgClientUserInterface/SwgCuiCalendarEventDetails.h"

#include "clientGame/Game.h"
#include "clientGame/GameNetwork.h"
#include "clientGame/PlayerObject.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "clientUserInterface/CuiMessageBox.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedNetworkMessages/CalendarMessages.h"
#include "swgClientUserInterface/SwgCuiMediatorTypes.h"

#include "UIButton.h"
#include "UIData.h"
#include "UIDataSource.h"
#include "UIImage.h"
#include "UIList.h"
#include "UIPage.h"
#include "UIText.h"
#include "Unicode.h"

#include <ctime>

//======================================================================

int SwgCuiCalendar::ms_selectedYear  = 2026;
int SwgCuiCalendar::ms_selectedMonth = 3;
int SwgCuiCalendar::ms_selectedDay   = 6;

namespace
{
	const char * const s_monthNames[] = {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	};

	const char * const s_eventTypeNames[] = { "Staff", "Guild", "City", "Server" };

	UIColor getEventTypeColor(int eventType)
	{
		switch (eventType)
		{
			case 0: return UIColor(255, 215, 0);   // Staff - Gold
			case 1: return UIColor(0, 255, 0);     // Guild - Green
			case 2: return UIColor(0, 191, 255);   // City - Blue
			case 3: return UIColor(255, 69, 0);    // Server - Red/Orange
			default: return UIColor(255, 255, 255);
		}
	}

	int getDaysInMonth(int year, int month)
	{
		static const int daysPerMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
		if (month < 1 || month > 12)
			return 30;

		int days = daysPerMonth[month - 1];

		if (month == 2)
		{
			bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
			if (isLeap)
				days = 29;
		}

		return days;
	}

	int getFirstDayOfMonth(int year, int month)
	{
		if (month < 3)
		{
			month += 12;
			year--;
		}

		int k = year % 100;
		int j = year / 100;
		int dayOfWeek = (1 + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
		dayOfWeek = ((dayOfWeek + 6) % 7);

		return dayOfWeek + 1;
	}
}

//======================================================================

SwgCuiCalendar::SwgCuiCalendar(UIPage & page) :
CuiMediator         ("SwgCuiCalendar", page),
UIEventCallback     (),
MessageDispatch::Receiver(),
m_buttonPrevMonth   (0),
m_buttonNextMonth   (0),
m_buttonToday       (0),
m_buttonCreate      (0),
m_buttonSettings    (0),
m_buttonClose       (0),
m_textMonthYear     (0),
m_textSelectedDate  (0),
m_listEvents        (0),
m_dataEvents        (0),
m_buttonViewDetails (0),
m_buttonDeleteEvent (0),
m_pageDetail        (0),
m_textDetailTitle   (0),
m_textDetailDesc    (0),
m_textDetailTime    (0),
m_textDetailType    (0),
m_imageDetailImage  (0),
m_buttonDetailClose (0),
m_currentYear       (2026),
m_currentMonth      (3),
m_selectedDay       (6),
m_cachedEvents      (),
m_selectedDayEvents ()
{
	for (int i = 0; i < 42; ++i)
	{
		m_dayButtons[i] = 0;
	}

	getCodeDataObject(TUIButton, m_buttonPrevMonth,   "header.btnPrevMonth", false);
	getCodeDataObject(TUIButton, m_buttonNextMonth,   "header.btnNextMonth", false);
	getCodeDataObject(TUIButton, m_buttonToday,       "header.btnToday", false);
	getCodeDataObject(TUIButton, m_buttonCreate,      "header.btnCreate", false);
	getCodeDataObject(TUIButton, m_buttonSettings,    "header.btnSettings", false);
	getCodeDataObject(TUIButton, m_buttonClose,       "btnClose", false);
	getCodeDataObject(TUIText,   m_textMonthYear,     "header.lblMonthYear", false);

	getCodeDataObject(TUIText,       m_textSelectedDate,  "eventPanel.lblSelectedDate", false);
	getCodeDataObject(TUIList,       m_listEvents,        "eventPanel.lstEvents", false);
	getCodeDataObject(TUIDataSource, m_dataEvents,        "eventPanel.dataEvents", false);
	getCodeDataObject(TUIButton,     m_buttonViewDetails, "eventPanel.btnViewDetails", false);
	getCodeDataObject(TUIButton,     m_buttonDeleteEvent, "eventPanel.btnDeleteEvent", false);

	getCodeDataObject(TUIPage,   m_pageDetail,        "detailPanel", false);
	getCodeDataObject(TUIText,   m_textDetailTitle,   "detailPanel.lblDetailTitle", false);
	getCodeDataObject(TUIText,   m_textDetailDesc,    "detailPanel.lblDetailDesc", false);
	getCodeDataObject(TUIText,   m_textDetailTime,    "detailPanel.lblDetailTime", false);
	getCodeDataObject(TUIText,   m_textDetailType,    "detailPanel.lblDetailType", false);
	getCodeDataObject(TUIImage,  m_imageDetailImage,  "detailPanel.imgDetailImage", false);
	getCodeDataObject(TUIButton, m_buttonDetailClose, "detailPanel.btnDetailClose", false);

	char buttonName[64];
	for (int i = 0; i < 42; ++i)
	{
		int dayNum = i + 1;
		snprintf(buttonName, sizeof(buttonName), "calendarGrid.day%02d", dayNum);
		getCodeDataObject(TUIButton, m_dayButtons[i], buttonName, false);
	}

	time_t now = time(0);
	struct tm * timeinfo = localtime(&now);
	if (timeinfo)
	{
		m_currentYear = timeinfo->tm_year + 1900;
		m_currentMonth = timeinfo->tm_mon + 1;
		m_selectedDay = timeinfo->tm_mday;
	}

	ms_selectedYear  = m_currentYear;
	ms_selectedMonth = m_currentMonth;
	ms_selectedDay   = m_selectedDay;
}

//----------------------------------------------------------------------

SwgCuiCalendar::~SwgCuiCalendar()
{
	m_buttonPrevMonth   = 0;
	m_buttonNextMonth   = 0;
	m_buttonToday       = 0;
	m_buttonCreate      = 0;
	m_buttonSettings    = 0;
	m_buttonClose       = 0;
	m_textMonthYear     = 0;
	m_textSelectedDate  = 0;
	m_listEvents        = 0;
	m_dataEvents        = 0;
	m_buttonViewDetails = 0;
	m_buttonDeleteEvent = 0;
	m_pageDetail        = 0;
	m_textDetailTitle   = 0;
	m_textDetailDesc    = 0;
	m_textDetailTime    = 0;
	m_textDetailType    = 0;
	m_imageDetailImage  = 0;
	m_buttonDetailClose = 0;

	for (int i = 0; i < 42; ++i)
	{
		m_dayButtons[i] = 0;
	}
}

//----------------------------------------------------------------------

void SwgCuiCalendar::performActivate()
{
	connectToMessage("CalendarEventsResponseMessage");
	connectToMessage("CalendarEventNotificationMessage");
	connectToMessage("CalendarDeleteEventResponseMessage");
	connectToMessage("CalendarCreateEventResponseMessage");

	if (m_buttonPrevMonth)   m_buttonPrevMonth->AddCallback(this);
	if (m_buttonNextMonth)   m_buttonNextMonth->AddCallback(this);
	if (m_buttonToday)       m_buttonToday->AddCallback(this);
	if (m_buttonCreate)      m_buttonCreate->AddCallback(this);
	if (m_buttonSettings)    m_buttonSettings->AddCallback(this);
	if (m_buttonClose)       m_buttonClose->AddCallback(this);
	if (m_buttonViewDetails) m_buttonViewDetails->AddCallback(this);
	if (m_buttonDeleteEvent) m_buttonDeleteEvent->AddCallback(this);
	if (m_buttonDetailClose) m_buttonDetailClose->AddCallback(this);

	for (int i = 0; i < 42; ++i)
	{
		if (m_dayButtons[i])
			m_dayButtons[i]->AddCallback(this);
	}

	if (m_listEvents)
		m_listEvents->AddCallback(this);

	if (m_pageDetail)
		m_pageDetail->SetVisible(false);

	requestEvents();
	updateCalendarDisplay();
}

//----------------------------------------------------------------------

void SwgCuiCalendar::performDeactivate()
{
	disconnectFromMessage("CalendarEventsResponseMessage");
	disconnectFromMessage("CalendarEventNotificationMessage");
	disconnectFromMessage("CalendarDeleteEventResponseMessage");
	disconnectFromMessage("CalendarCreateEventResponseMessage");

	if (m_buttonPrevMonth)   m_buttonPrevMonth->RemoveCallback(this);
	if (m_buttonNextMonth)   m_buttonNextMonth->RemoveCallback(this);
	if (m_buttonToday)       m_buttonToday->RemoveCallback(this);
	if (m_buttonCreate)      m_buttonCreate->RemoveCallback(this);
	if (m_buttonSettings)    m_buttonSettings->RemoveCallback(this);
	if (m_buttonClose)       m_buttonClose->RemoveCallback(this);
	if (m_buttonViewDetails) m_buttonViewDetails->RemoveCallback(this);
	if (m_buttonDeleteEvent) m_buttonDeleteEvent->RemoveCallback(this);
	if (m_buttonDetailClose) m_buttonDetailClose->RemoveCallback(this);

	for (int i = 0; i < 42; ++i)
	{
		if (m_dayButtons[i])
			m_dayButtons[i]->RemoveCallback(this);
	}

	if (m_listEvents)
		m_listEvents->RemoveCallback(this);
}

//----------------------------------------------------------------------

void SwgCuiCalendar::OnButtonPressed(UIWidget *context)
{
	if (context == m_buttonPrevMonth)
	{
		navigateMonth(-1);
		requestEvents();
	}
	else if (context == m_buttonNextMonth)
	{
		navigateMonth(1);
		requestEvents();
	}
	else if (context == m_buttonToday)
	{
		goToToday();
		requestEvents();
	}
	else if (context == m_buttonCreate)
	{
		ms_selectedYear  = m_currentYear;
		ms_selectedMonth = m_currentMonth;
		ms_selectedDay   = m_selectedDay;
		CuiMediatorFactory::activateInWorkspace(CuiMediatorTypes::WS_CalendarEventEditor);
	}
	else if (context == m_buttonSettings)
	{
		CuiMediatorFactory::activateInWorkspace(CuiMediatorTypes::WS_CalendarSettings);
	}
	else if (context == m_buttonClose)
	{
		deactivate();
	}
	else if (context == m_buttonViewDetails)
	{
		showEventDetails();
	}
	else if (context == m_buttonDeleteEvent)
	{
		if (m_listEvents)
		{
			int selectedRow = m_listEvents->GetLastSelectedRow();
			if (selectedRow >= 0 && selectedRow < static_cast<int>(m_selectedDayEvents.size()))
			{
				CalendarEventData const & evt = m_selectedDayEvents[static_cast<size_t>(selectedRow)];
				CalendarDeleteEventMessage msg(evt.eventId);
				GameNetwork::send(msg, true);
			}
		}
	}
	else if (context == m_buttonDetailClose)
	{
		if (m_pageDetail)
			m_pageDetail->SetVisible(false);
	}
	else
	{
		for (int i = 0; i < 42; ++i)
		{
			if (context == m_dayButtons[i])
			{
				int firstDay = getFirstDayOfMonth(m_currentYear, m_currentMonth);
				int daysInMonth = getDaysInMonth(m_currentYear, m_currentMonth);

				int cellNum = i + 1;
				int dayNum = cellNum - firstDay + 1;

				if (dayNum >= 1 && dayNum <= daysInMonth)
				{
					selectDay(dayNum);
				}
				break;
			}
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCalendar::OnGenericSelectionChanged(UIWidget *context)
{
	if (context == m_listEvents)
	{
	}
}

//----------------------------------------------------------------------

void SwgCuiCalendar::updateCalendarDisplay()
{
	if (m_textMonthYear && m_currentMonth >= 1 && m_currentMonth <= 12)
	{
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%s %d", s_monthNames[m_currentMonth - 1], m_currentYear);
		m_textMonthYear->SetText(Unicode::narrowToWide(buffer));
	}

	if (m_textSelectedDate && m_currentMonth >= 1 && m_currentMonth <= 12)
	{
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%s %d, %d", s_monthNames[m_currentMonth - 1], m_selectedDay, m_currentYear);
		m_textSelectedDate->SetText(Unicode::narrowToWide(buffer));
	}

	int daysInMonth = getDaysInMonth(m_currentYear, m_currentMonth);
	int firstDay = getFirstDayOfMonth(m_currentYear, m_currentMonth);

	time_t now = time(0);
	struct tm * timeinfo = localtime(&now);
	int todayYear = timeinfo ? timeinfo->tm_year + 1900 : 0;
	int todayMonth = timeinfo ? timeinfo->tm_mon + 1 : 0;
	int todayDay = timeinfo ? timeinfo->tm_mday : 0;

	int dayNum = 1;
	for (int cell = 0; cell < 42; ++cell)
	{
		UIButton * btn = m_dayButtons[cell];
		if (!btn)
			continue;

		int cellNum = cell + 1;

		if (cellNum < firstDay || dayNum > daysInMonth)
		{
			btn->SetVisible(false);
			btn->SetText(Unicode::emptyString);
		}
		else
		{
			btn->SetVisible(true);

			char dayText[16];
			snprintf(dayText, sizeof(dayText), "%d", dayNum);
			btn->SetText(Unicode::narrowToWide(dayText));

			bool isToday = (dayNum == todayDay && m_currentMonth == todayMonth && m_currentYear == todayYear);
			bool isSelected = (dayNum == m_selectedDay);

			// Titan theme: the accent for the selected day, its deep tone for today, slate for the rest
			UIColor bgColor;
			if (isSelected)
			{
				bgColor = UIColor(47, 139, 216);
			}
			else if (isToday)
			{
				bgColor = UIColor(30, 90, 140);
			}
			else
			{
				bgColor = UIColor(36, 48, 62);
			}

			btn->SetBackgroundTint(bgColor);

			dayNum++;
		}
	}

	ms_selectedYear  = m_currentYear;
	ms_selectedMonth = m_currentMonth;
	ms_selectedDay   = m_selectedDay;

	refreshEventList();
}

//----------------------------------------------------------------------

void SwgCuiCalendar::selectDay(int day)
{
	int daysInMonth = getDaysInMonth(m_currentYear, m_currentMonth);
	if (day >= 1 && day <= daysInMonth)
	{
		m_selectedDay = day;
		updateCalendarDisplay();
	}
}

//----------------------------------------------------------------------

void SwgCuiCalendar::navigateMonth(int delta)
{
	m_currentMonth += delta;

	if (m_currentMonth > 12)
	{
		m_currentMonth = 1;
		m_currentYear++;
	}
	else if (m_currentMonth < 1)
	{
		m_currentMonth = 12;
		m_currentYear--;
	}

	int daysInMonth = getDaysInMonth(m_currentYear, m_currentMonth);
	if (m_selectedDay > daysInMonth)
	{
		m_selectedDay = daysInMonth;
	}

	updateCalendarDisplay();
}

//----------------------------------------------------------------------

void SwgCuiCalendar::goToToday()
{
	time_t now = time(0);
	struct tm * timeinfo = localtime(&now);
	if (timeinfo)
	{
		m_currentYear = timeinfo->tm_year + 1900;
		m_currentMonth = timeinfo->tm_mon + 1;
		m_selectedDay = timeinfo->tm_mday;
	}

	updateCalendarDisplay();
}

//----------------------------------------------------------------------

void SwgCuiCalendar::refreshEventList()
{
	if (m_dataEvents)
	{
		m_dataEvents->Clear();
	}

	if (m_listEvents)
	{
		m_listEvents->Clear();
	}

	m_selectedDayEvents.clear();

	for (std::vector<CalendarEventData>::const_iterator it = m_cachedEvents.begin(); it != m_cachedEvents.end(); ++it)
	{
		CalendarEventData const & evt = *it;

		if (evt.year == m_currentYear && evt.month == m_currentMonth && evt.day == m_selectedDay)
		{
			m_selectedDayEvents.push_back(evt);

			if (m_listEvents)
			{
				char timeStr[16];
				snprintf(timeStr, sizeof(timeStr), "%02d:%02d", evt.hour, evt.minute);

				// Add type indicator prefix for visual identification
				const char * typePrefix = "";
				switch (evt.eventType)
				{
					case 0: typePrefix = "[S] "; break; // Staff
					case 1: typePrefix = "[G] "; break; // Guild
					case 2: typePrefix = "[C] "; break; // City
					case 3: typePrefix = "[E] "; break; // Server/Event
					default: typePrefix = "";
				}

				std::string displayText = std::string(typePrefix) + timeStr + " - " + evt.title;
				m_listEvents->AddRow(Unicode::narrowToWide(displayText), evt.eventId);
			}
		}
	}

	// Show delete button only for admins
	if (m_buttonDeleteEvent)
	{
		m_buttonDeleteEvent->SetVisible(PlayerObject::isAdmin());
	}
}

//----------------------------------------------------------------------

void SwgCuiCalendar::showEventDetails()
{
	if (!m_listEvents)
		return;

	int selectedRow = m_listEvents->GetLastSelectedRow();
	if (selectedRow < 0 || selectedRow >= static_cast<int>(m_selectedDayEvents.size()))
		return;

	CalendarEventData const & evt = m_selectedDayEvents[static_cast<size_t>(selectedRow)];

	// Use the new details popup
	SwgCuiCalendarEventDetails::showDetails(evt);
}

//----------------------------------------------------------------------

void SwgCuiCalendar::requestEvents()
{
	if (!GameNetwork::isConnectedToConnectionServer())
	{
		REPORT_LOG(true, ("SwgCuiCalendar::requestEvents: NOT connected to connection server\n"));
		return;
	}

	REPORT_LOG(true, ("SwgCuiCalendar::requestEvents: requesting year=%d month=%d\n", m_currentYear, m_currentMonth));
	CalendarGetEventsMessage msg(m_currentYear, m_currentMonth);
	GameNetwork::send(msg, true);
}

//----------------------------------------------------------------------

void SwgCuiCalendar::receiveMessage(MessageDispatch::Emitter const & /*source*/, MessageDispatch::MessageBase const & message)
{
	REPORT_LOG(true, ("SwgCuiCalendar::receiveMessage: received message type=[%s]\n", message.getType() ? "known" : "unknown"));

	if (message.isType("CalendarEventsResponseMessage"))
	{
		REPORT_LOG(true, ("SwgCuiCalendar::receiveMessage: CalendarEventsResponseMessage\n"));
		Archive::ReadIterator ri = static_cast<const GameNetworkMessage &>(message).getByteStream().begin();
		CalendarEventsResponseMessage const msg(ri);
		onEventsResponse(msg.getEvents());
	}
	else if (message.isType("CalendarEventNotificationMessage"))
	{
		REPORT_LOG(true, ("SwgCuiCalendar::receiveMessage: CalendarEventNotificationMessage\n"));
		Archive::ReadIterator ri = static_cast<const GameNetworkMessage &>(message).getByteStream().begin();
		CalendarEventNotificationMessage const msg(ri);
		onEventNotification(msg.getNotificationType(), msg.getEventData());
	}
	else if (message.isType("CalendarDeleteEventResponseMessage"))
	{
		REPORT_LOG(true, ("SwgCuiCalendar::receiveMessage: CalendarDeleteEventResponseMessage\n"));
		Archive::ReadIterator ri = static_cast<const GameNetworkMessage &>(message).getByteStream().begin();
		CalendarDeleteEventResponseMessage const msg(ri);
		REPORT_LOG(true, ("SwgCuiCalendar: delete response success=%d\n", msg.getSuccess() ? 1 : 0));
		if (msg.getSuccess())
		{
			requestEvents();
		}
	}
	else if (message.isType("CalendarCreateEventResponseMessage"))
	{
		REPORT_LOG(true, ("SwgCuiCalendar::receiveMessage: CalendarCreateEventResponseMessage\n"));
		Archive::ReadIterator ri = static_cast<const GameNetworkMessage &>(message).getByteStream().begin();
		CalendarCreateEventResponseMessage const msg(ri);
		REPORT_LOG(true, ("SwgCuiCalendar: create response success=%d eventId=[%s] error=[%s]\n",
			msg.getSuccess() ? 1 : 0, msg.getEventId().c_str(), msg.getErrorMessage().c_str()));
		if (msg.getSuccess())
		{
			requestEvents();
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCalendar::onEventsResponse(std::vector<CalendarEventData> const & events)
{
	REPORT_LOG(true, ("SwgCuiCalendar::onEventsResponse: received %d events\n", static_cast<int>(events.size())));
	m_cachedEvents = events;
	refreshEventList();
	updateCalendarDisplay();
}

//----------------------------------------------------------------------

void SwgCuiCalendar::onEventNotification(int notificationType, CalendarEventData const & eventData)
{
	UNREF(eventData);
	switch (notificationType)
	{
		case 0:
		case 1:
		case 2:
			requestEvents();
			break;

		case 3:
		case 4:
			break;
	}
}

//----------------------------------------------------------------------

int SwgCuiCalendar::getSelectedYear()
{
	return ms_selectedYear;
}

//----------------------------------------------------------------------

int SwgCuiCalendar::getSelectedMonth()
{
	return ms_selectedMonth;
}

//----------------------------------------------------------------------

int SwgCuiCalendar::getSelectedDay()
{
	return ms_selectedDay;
}

//======================================================================
