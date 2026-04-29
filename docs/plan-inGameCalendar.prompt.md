# In-Game Calendar System Implementation Plan

## Overview

Create a comprehensive in-game calendar system that displays and manages events across multiple scopes: Staff Events, Guild Events, City Events, and Server Events. The system features a **custom visual grid calendar UI** with clickable day cells, customizable backgrounds, and integration with the existing holiday/event system.

## Architecture

**Pure UI-based system with network messages** - No command-based data exchange. All calendar operations use dedicated network messages between client and server. Data is stored in the `CalendarService` (C++ server-side singleton), not in objvars.

## Implementation Status: COMPLETE ✓

### Files Created/Modified

#### Server C++ (`/src`)

**Network Messages** (`sharedNetworkMessages/src/shared/clientGameServer/`):
- `CalendarMessages.h` - Message class definitions:
  - `CalendarEventData` - Event data structure
  - `CalendarGetEventsMessage` - Client requests events
  - `CalendarEventsResponseMessage` - Server sends events
  - `CalendarCreateEventMessage` - Client creates event
  - `CalendarCreateEventResponseMessage` - Server confirms creation
  - `CalendarDeleteEventMessage` - Client deletes event
  - `CalendarDeleteEventResponseMessage` - Server confirms deletion
  - `CalendarEventNotificationMessage` - Server pushes updates to clients
  - `CalendarGetSettingsMessage` - Client requests settings
  - `CalendarSettingsResponseMessage` - Server sends settings
  - `CalendarApplySettingsMessage` - Client applies settings
- `CalendarMessages.cpp` - Message implementations with Archive serialization

**Calendar Service** (`serverGame/src/shared/calendar/`):
- `CalendarService.h` - Singleton service header
- `CalendarService.cpp` - Service implementation:
  - Event storage in memory (std::map)
  - CRUD operations: `createEvent()`, `updateEvent()`, `deleteEvent()`
  - Query functions: `getEventsForPlayer()`, `getEventsForDay()`, `getEventsForGuild()`, etc.
  - Permission checks: `canCreateEvent()`, `canEditEvent()`, `canDeleteEvent()`
  - Notification system: `notifyEventCreated()`, `notifyEventUpdated()`, `notifyEventDeleted()`
  - Message handlers for client requests
  - Settings management

**Client Integration** (`serverGame/src/shared/core/Client.cpp`):
- Added message handlers for all calendar messages
- Routes to `CalendarService::getInstance().handle*()`

#### Client C++ (`/client/src`)

**Mediator Types** (`swgClientUserInterface/src/shared/core/SwgCuiMediatorTypes.h`):
- Added `WS_Calendar`, `WS_CalendarEventEditor`, `WS_CalendarSettings`

**Mediator Classes** (`swgClientUserInterface/src/shared/page/`):
- `SwgCuiCalendar.h/.cpp` - Main calendar window mediator
  - Implements `MessageDispatch::Receiver` for network messages
  - Sends `CalendarGetEventsMessage` on activate and month navigation
  - Receives `CalendarEventsResponseMessage` to populate event list
  - Receives `CalendarEventNotificationMessage` for real-time updates
  - 42 day button grid with click handling
  - Month/year navigation
- `SwgCuiCalendarEventEditor.h/.cpp` - Event creation dialog
  - Sends `CalendarCreateEventMessage` with form data
  - Form fields for title, description, type, date/time, duration
- `SwgCuiCalendarSettings.h/.cpp` - Staff settings dialog
  - Sends `CalendarGetSettingsMessage` on activate
  - Sends `CalendarApplySettingsMessage` on apply

**Public Headers** (`include/public/swgClientUserInterface/`):
- `SwgCuiCalendar.h`
- `SwgCuiCalendarEventEditor.h`
- `SwgCuiCalendarSettings.h`

**Button Bar Integration** (`swgClientUserInterface/src/shared/page/SwgCuiButtonBar.h/.cpp`):
- Added `m_calendarButton` member variable
- Added button initialization with `getCodeDataObject(TUIButton, m_calendarButton, "buttonCalendar", true)`
- Added button press handler to open `CuiMediatorTypes::WS_Calendar`
- Calendar button always visible in the button bar menu

**UI Definition Files** (button bar):
- `ui_ground_hud_buttonbar.inc` - Added `buttonCalendar` data reference and composite widget
- `ui_ground_hud_buttonbar_skinned.inc` - Added `buttonCalendar` data reference and composite widget
- All copies updated in `/client/datasources/ui/`, `/exe/win32_rel/ui/`, `/exe/tre/ux/ui/`

#### Server Scripts (`/dsrc`)
- `script/library/calendar.java` - Helper library with `openCalendarCUI()` method
- `script/command/command_calendar.java` - Simple `/calendar` command to open UI
- `script/systems/calendar/calendar_manager.java` - Event lifecycle manager

---

## Server/Client Communication

### Network Messages (Pure UI System)

All data exchange uses typed network messages, not commands or objvars:

| Message | Direction | Purpose |
|---------|-----------|---------|
| `CalendarGetEventsMessage` | Client→Server | Request events for year/month |
| `CalendarEventsResponseMessage` | Server→Client | Send event list |
| `CalendarCreateEventMessage` | Client→Server | Create new event |
| `CalendarCreateEventResponseMessage` | Server→Client | Confirm creation |
| `CalendarDeleteEventMessage` | Client→Server | Delete event |
| `CalendarDeleteEventResponseMessage` | Server→Client | Confirm deletion |
| `CalendarEventNotificationMessage` | Server→Client | Push notification (create/update/delete/start/end) |
| `CalendarGetSettingsMessage` | Client→Server | Request settings |
| `CalendarSettingsResponseMessage` | Server→Client | Send settings |
| `CalendarApplySettingsMessage` | Client→Server | Apply new settings |

### Communication Flow

```
Client opens calendar
    ↓
Client sends CalendarGetEventsMessage(year, month)
    ↓
Server CalendarService.handleGetEventsRequest()
    ↓
Server sends CalendarEventsResponseMessage(events)
    ↓
Client SwgCuiCalendar.onEventsResponse() updates UI
```

### Multi-Client Synchronization

When Client A creates an event:
1. Client A sends `CalendarCreateEventMessage`
2. Server creates event in `CalendarService`
3. Server sends `CalendarCreateEventResponseMessage` to Client A
4. Server calls `notifyEventCreated()` which broadcasts `CalendarEventNotificationMessage` to all relevant clients
5. Client B receives notification and refreshes its event list

---

## Data Storage

Events are stored in the `CalendarService` singleton on the server:
- In-memory `std::map<std::string, CalendarEventData>` for fast access
- Periodic persistence to cluster-wide data storage
- No objvars used for event storage

### CalendarEventData Structure (C++)
```cpp
struct CalendarEventData {
    std::string eventId;
    std::string title;
    std::string description;
    int32       eventType;      // 0=Staff, 1=Guild, 2=City, 3=Server
    int32       year, month, day, hour, minute;
    int32       duration;       // minutes
    int32       guildId;
    int32       cityId;
    std::string serverEventKey;
    bool        recurring;
    int32       recurrenceType;
    bool        broadcastStart;
    bool        active;
    NetworkId   creatorId;
};
```

---

## Architecture

### Components

1. **Server-Side Scripts** (`/dsrc`)
   - `script/systems/calendar/calendar_manager.java` - Central calendar management
   - `script/systems/calendar/calendar_event.java` - Event data handling
   - `script/systems/calendar/calendar_broadcast.java` - Galaxy-wide event broadcasts
   - `script/library/calendar.java` - Calendar utility library
   - `script/terminal/terminal_calendar.java` - Terminal interaction script
   - `script/commands/command_calendar.java` - /calendar command handler

2. **Client-Side UI** (`/client`)
   - `ui/calendar_window.ui` - Main calendar window definition
   - `ui/calendar_event_editor.ui` - Event creation/editing dialog
   - `ui/calendar_settings.ui` - Staff settings for background/widgets

3. **Server Engine Integration** (`/src`)
   - Calendar data persistence in cluster-wide storage
   - Real-time event checking and triggering

---

## Data Structures

### Calendar Event Object

```
calendar_event {
    string event_id;           // Unique identifier (UUID)
    string title;              // Event title
    string description;        // Event description
    int event_type;            // EVENT_TYPE_STAFF, EVENT_TYPE_GUILD, EVENT_TYPE_CITY, EVENT_TYPE_SERVER
    int start_timestamp;       // Unix timestamp for start
    int end_timestamp;         // Unix timestamp for end
    int duration_minutes;      // Duration in minutes
    string creator_id;         // Player ID who created it
    int guild_id;              // Guild ID (if guild event)
    int city_id;               // City ID (if city event)
    string server_event_key;   // Linked server event (halloween, lifeday, etc.)
    boolean recurring;         // Is this a recurring event
    int recurrence_type;       // DAILY, WEEKLY, MONTHLY, YEARLY
    boolean broadcast_start;   // Broadcast when event starts
    boolean active;            // Is event currently active
}
```

### Event Types

```java
public static final int EVENT_TYPE_STAFF = 0;    // Staff-created server-wide events
public static final int EVENT_TYPE_GUILD = 1;    // Guild events (visible to guild members)
public static final int EVENT_TYPE_CITY = 2;     // City events (visible to citizens)
public static final int EVENT_TYPE_SERVER = 3;   // System events (holidays, etc.)
```

### Calendar Settings (Staff Only)

```
calendar_settings {
    string background_texture;     // Path to .dds texture
    int source_rect_x;             // Source rectangle X
    int source_rect_y;             // Source rectangle Y
    int source_rect_width;         // Source rectangle width
    int source_rect_height;        // Source rectangle height
}
```

---

## Server Scripts (`/dsrc`)

### 1. `script/library/calendar.java`

Core library with static utility methods:

```java
package script.library;

public class calendar extends script.base_script
{
    // Event types
    public static final int EVENT_TYPE_STAFF = 0;
    public static final int EVENT_TYPE_GUILD = 1;
    public static final int EVENT_TYPE_CITY = 2;
    public static final int EVENT_TYPE_SERVER = 3;
    
    // Recurrence types
    public static final int RECUR_NONE = 0;
    public static final int RECUR_DAILY = 1;
    public static final int RECUR_WEEKLY = 2;
    public static final int RECUR_MONTHLY = 3;
    public static final int RECUR_YEARLY = 4;
    
    // Server event keys (link to existing holiday system)
    public static final String[] SERVER_EVENTS = {
        "halloween",
        "lifeday",
        "loveday",
        "empireday_ceremony"
    };
    
    // Cluster-wide objvar storage location
    public static final String CALENDAR_CLUSTER_OBJVAR = "calendar.events";
    public static final String CALENDAR_SETTINGS_OBJVAR = "calendar.settings";
    
    // Permission checks
    public static boolean canCreateEvent(obj_id player, int eventType);
    public static boolean canEditEvent(obj_id player, String eventId);
    public static boolean canDeleteEvent(obj_id player, String eventId);
    public static boolean canViewEvent(obj_id player, dictionary eventData);
    
    // Event CRUD operations
    public static String createEvent(obj_id creator, dictionary eventData);
    public static boolean updateEvent(String eventId, dictionary eventData);
    public static boolean deleteEvent(String eventId);
    public static dictionary getEvent(String eventId);
    public static dictionary[] getEventsForPlayer(obj_id player);
    public static dictionary[] getEventsForMonth(int year, int month);
    public static dictionary[] getEventsForDay(int year, int month, int day);
    
    // Guild/City helpers
    public static dictionary[] getGuildEvents(int guildId);
    public static dictionary[] getCityEvents(int cityId);
    public static dictionary[] getStaffEvents();
    public static dictionary[] getServerEvents();
    
    // Time utilities
    public static int getCurrentYear();
    public static int getCurrentMonth();
    public static int getCurrentDay();
    public static int getDaysInMonth(int year, int month);
    public static int getDayOfWeek(int year, int month, int day);
    public static String formatDate(int timestamp);
    public static String formatTime(int timestamp);
    
    // Event triggering
    public static void checkAndTriggerEvents();
    public static void triggerServerEvent(String eventKey);
    public static void stopServerEvent(String eventKey);
    
    // Broadcasting
    public static void broadcastEventStart(dictionary eventData);
    public static void broadcastToGuild(int guildId, String message);
    public static void broadcastToCity(int cityId, String message);
    public static void broadcastGalaxyWide(String message);
    
    // Settings (staff only)
    public static dictionary getCalendarSettings();
    public static void setCalendarSettings(dictionary settings);
}
```

### 2. `script/systems/calendar/calendar_manager.java`

Attached to a cluster-wide object (like the holiday controller), manages event lifecycle:

```java
package script.systems.calendar;

public class calendar_manager extends script.base_script
{
    public static final int CHECK_INTERVAL = 60; // Check every 60 seconds
    
    public int OnInitialize(obj_id self);
    public int OnAttach(obj_id self);
    
    // Periodic check for events that need to start/end
    public int checkCalendarEvents(obj_id self, dictionary params);
    
    // Handle event start
    public int handleEventStart(obj_id self, dictionary params);
    
    // Handle event end
    public int handleEventEnd(obj_id self, dictionary params);
    
    // Link to existing holiday system
    public int triggerHolidayEvent(obj_id self, dictionary params);
    public int stopHolidayEvent(obj_id self, dictionary params);
}
```

### 3. `script/terminal/terminal_calendar.java`

Terminal script for accessing the calendar:

```java
package script.terminal;

public class terminal_calendar extends script.base_script
{
    // Radial menu options
    public static final int MENU_OPEN_CALENDAR = 100;
    public static final int MENU_CREATE_EVENT = 101;
    public static final int MENU_SETTINGS = 102;  // Staff only
    
    public int OnObjectMenuRequest(obj_id self, obj_id player, menu_info mi);
    public int OnObjectMenuSelect(obj_id self, obj_id player, int item);
    
    // Open calendar UI
    private void openCalendarWindow(obj_id player);
    
    // Show event creation dialog
    private void showCreateEventDialog(obj_id player);
    
    // Show settings dialog (staff only)
    private void showSettingsDialog(obj_id player);
}
```

### 4. `script/commands/command_calendar.java`

Command handler for `/calendar`:

```java
package script.commands;

public class command_calendar extends script.base_script
{
    // /calendar - Opens calendar
    // /calendar create - Create event (if permitted)
    // /calendar list - List upcoming events
    // /calendar settings - Staff settings
    
    public int OnAttach(obj_id self);
    public int cmdCalendar(obj_id self, obj_id target, String params, float defaultTime);
}
```

---

## Client UI (`/client`)

### 1. Main Calendar Window (`ui/calendar_window.ui`)

Grid-based calendar with:

- **Header**: Month/Year selector, navigation arrows
- **Day Headers**: Sun, Mon, Tue, Wed, Thu, Fri, Sat
- **Grid Cells**: 6 rows x 7 columns for days
- **Event List Panel**: Right side showing events for selected day
- **Control Buttons**: Create Event, Refresh, Close

```
+----------------------------------------------------------+
|  [<]  MARCH 2026  [>]                    [Create] [Close]|
+----------------------------------------------------------+
| Sun | Mon | Tue | Wed | Thu | Fri | Sat |  Selected Day  |
+-----+-----+-----+-----+-----+-----+-----+----------------+
|  1  |  2  |  3  |  4  |  5  |  6* |  7  | March 6, 2026  |
|     |     |     |     |     | [*] |     |                |
+-----+-----+-----+-----+-----+-----+-----+ Event List:    |
|  8  |  9  | 10  | 11  | 12  | 13  | 14  | - Staff Event  |
|     |     |     |     |     |     |     | - Guild Meet   |
+-----+-----+-----+-----+-----+-----+-----+                |
| 15  | 16  | 17  | 18  | 19  | 20  | 21  | [View Details] |
|     |     |     |     |     |     |     |                |
+-----+-----+-----+-----+-----+-----+-----+----------------+
| 22  | 23  | 24  | 25  | 26  | 27  | 28  |                |
+-----+-----+-----+-----+-----+-----+-----+                |
| 29  | 30  | 31  |     |     |     |     |                |
+----------------------------------------------------------+
| Legend: [Staff] [Guild] [City] [Server]                  |
+----------------------------------------------------------+
```

### 2. Event Editor Dialog (`ui/calendar_event_editor.ui`)

Fields:
- **Event Title**: Text input
- **Description**: Multi-line text input
- **Event Type**: Dropdown (Staff can select any, others locked to their scope)
- **Start Date**: Date picker (calendar widget)
- **Start Time**: Time picker (hour:minute)
- **Duration**: Dropdown (15min, 30min, 1hr, 2hr, 4hr, 8hr, All Day)
- **Recurring**: Checkbox + recurrence type dropdown
- **Broadcast on Start**: Checkbox
- **Server Event Link**: Dropdown (Staff only - links to halloween, lifeday, etc.)

### 3. Settings Dialog (`ui/calendar_settings.ui`) - Staff Only

- **Background Texture**: File path input + Browse button
- **Source Rect X/Y/W/H**: Numeric inputs
- **Preview Panel**: Shows the texture with current source rect
- **Apply/Cancel buttons**

---

## Server Engine (`/src`)

### Modifications Required

1. **ClusterWideData storage for calendar events**
   - Use existing cluster-wide objvar system
   - Or create new `CalendarService` for dedicated storage

2. **Time-based event checking**
   - Integrate with existing game loop
   - Check every minute for events that need to start/end

3. **Broadcast system enhancement**
   - `broadcastGalaxyWide(String message)` - Send to all online players
   - Color-coded based on event type

---

## Integration with Existing Holiday System

Link the calendar to existing events in `holiday_controller.java`:

```java
// Map calendar server events to holiday system
public static final String[][] SERVER_EVENT_MAPPING = {
    {"halloween", "halloweenStartForReals", "halloweenStopForReals"},
    {"lifeday", "lifedayStartForReals", "lifedayStopForReals"},
    {"loveday", "lovedayStartForReals", "lovedayStopForReals"},
    {"empireday", "empiredayStartForReals", "empiredayStopForReals"}
};

// When calendar event starts, trigger the holiday system
public static void triggerServerEvent(String eventKey) {
    // Find holiday controller
    // Call appropriate start method
}
```

---

## Permission Matrix

| Action | Staff | Guild Leader | Guild Officer | City Mayor | Citizen | Regular Player |
|--------|-------|--------------|---------------|------------|---------|----------------|
| View Staff Events | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| View Server Events | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| View Guild Events | ✓ | Own Guild | Own Guild | ✓ | ✓ | Own Guild |
| View City Events | ✓ | ✓ | ✓ | Own City | Own City | Own City |
| Create Staff Events | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |
| Create Guild Events | ✓ | ✓ | ✓ | ✗ | ✗ | ✗ |
| Create City Events | ✓ | ✗ | ✗ | ✓ | ✗ | ✗ |
| Set Server Event Dates | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |
| Calendar Settings | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |

---

## Broadcast Messages

When an event starts and `broadcast_start` is true:

### Staff Events (Galaxy-Wide)
```
\#FFD700[GALACTIC EVENT]: {Event Title}
\#FFFFFF{Event Description}
```

### Guild Events (Guild-Wide)
```
\#00FF00[GUILD EVENT]: {Event Title}
\#FFFFFF{Event Description}
```

### City Events (City-Wide)
```
\#00BFFF[CITY EVENT]: {Event Title}
\#FFFFFF{Event Description}
```

### Server Events (Galaxy-Wide)
```
\#FF4500[SERVER EVENT]: {Event Title} has begun!
\#FFFFFFThe {event_key} event is now active across the galaxy!
```

---

## File Structure

```
/dsrc/sku.0/sys.server/compiled/game/script/
├── library/
│   └── calendar.java
├── systems/
│   └── calendar/
│       ├── calendar_manager.java
│       ├── calendar_event.java
│       └── calendar_broadcast.java
├── terminal/
│   └── terminal_calendar.java
└── commands/
    └── command_calendar.java

/client/
└── ui/
    ├── calendar_window.ui
    ├── calendar_event_editor.ui
    └── calendar_settings.ui

/data/sku.0/sys.shared/built/game/
└── string/en/
    └── ui_calendar.stf  (String table for UI text)
```

---

## Implementation Order

### Phase 1: Core Infrastructure
1. Create `script/library/calendar.java` with basic utilities
2. Create `script/systems/calendar/calendar_manager.java`
3. Set up cluster-wide storage for events

### Phase 2: Event Management
1. Implement CRUD operations in calendar library
2. Create terminal script for basic interaction
3. Add `/calendar` command

### Phase 3: UI Development
1. Create calendar window SUI
2. Create event editor SUI
3. Create settings SUI (staff only)

### Phase 4: Integration
1. Link to existing holiday system
2. Implement automatic event triggering
3. Add broadcast functionality

### Phase 5: Polish
1. Add calendar terminal object to game
2. Test all permission levels
3. Add visual indicators for event types

---

## Notes

- All timestamps should use server game time (getGameTime())
- Events should persist across server restarts
- Guild/City events should automatically clean up if guild/city is disbanded
- Staff events take precedence in display order
- Calendar should handle timezone display for players (future enhancement)
