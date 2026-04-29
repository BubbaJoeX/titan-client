# Calendar System Database Implementation

## Overview
The calendar system uses Oracle database tables to persist calendar events and settings across server restarts.

## Files Created/Modified

### Database Schema Files (for new environments)
- `D:\titan\src\game\server\database\schema\calendar_events.tab` - Calendar events table definition
- `D:\titan\src\game\server\database\schema\calendar_settings.tab` - Calendar settings table definition

### Database Update Files (for existing environments)
- `D:\titan\src\game\server\database\updates\272.sql` - Version 272 update script
- `D:\titan\src\game\server\database\updates\calendar_system_update.sql` - Standalone update script

### Package Files
- `D:\titan\src\game\server\database\packages\loader.plsqlh` - Added calendar loader function declarations
- `D:\titan\src\game\server\database\packages\loader.plsql` - Added calendar loader function implementations
- `D:\titan\src\game\server\database\packages\persister.plsqlh` - Added calendar persister procedure declarations
- `D:\titan\src\game\server\database\packages\persister.plsql` - Added calendar persister procedure implementations

### Server Network Messages
- `D:\titan\src\engine\server\library\serverNetworkMessages\src\shared\gameGameServer\CalendarEventMessage.h`
- `D:\titan\src\engine\server\library\serverNetworkMessages\src\shared\gameGameServer\CalendarEventMessage.cpp`
- `D:\titan\src\engine\server\library\serverNetworkMessages\include\public\serverNetworkMessages\CalendarEventMessage.h`

### Calendar Service Updates
- `D:\titan\src\engine\server\library\serverGame\src\shared\calendar\CalendarService.h` - Added database persistence methods
- `D:\titan\src\engine\server\library\serverGame\src\shared\calendar\CalendarService.cpp` - Implemented database persistence

---

## SQL Script to Run on Dev Database

Copy and paste the following SQL into SQL*Plus or your Oracle client to update an existing database:

```sql
-- ======================================================================
-- Calendar System Database Update Script
-- Run this on your existing SWG database to add calendar support
-- ======================================================================

SET SERVEROUTPUT ON;

-- Calendar Events table
DECLARE
  cnt NUMBER;
BEGIN
  SELECT COUNT(*) INTO cnt FROM user_tables WHERE table_name = 'CALENDAR_EVENTS';
  IF (cnt = 0) THEN
    EXECUTE IMMEDIATE '
      CREATE TABLE calendar_events
      (
        event_id VARCHAR2(64) NOT NULL,
        title VARCHAR2(256) NOT NULL,
        description VARCHAR2(2000),
        event_type INT NOT NULL,
        event_year INT NOT NULL,
        event_month INT NOT NULL,
        event_day INT NOT NULL,
        event_hour INT NOT NULL,
        event_minute INT NOT NULL,
        duration INT NOT NULL,
        guild_id INT DEFAULT 0,
        city_id INT DEFAULT 0,
        server_event_key VARCHAR2(64),
        recurring CHAR(1) DEFAULT ''N'',
        recurrence_type INT DEFAULT 0,
        broadcast_start CHAR(1) DEFAULT ''N'',
        active CHAR(1) DEFAULT ''N'',
        creator_id NUMBER(20),
        created_date DATE DEFAULT SYSDATE,
        PRIMARY KEY (event_id)
      )';
    DBMS_OUTPUT.PUT_LINE('Created table: CALENDAR_EVENTS');
  ELSE
    DBMS_OUTPUT.PUT_LINE('Table CALENDAR_EVENTS already exists');
  END IF;
END;
/

-- Calendar Events indexes
DECLARE
  cnt NUMBER;
BEGIN
  SELECT COUNT(*) INTO cnt FROM user_indexes WHERE index_name = 'CAL_EVT_TYPE_IDX';
  IF (cnt = 0) THEN
    EXECUTE IMMEDIATE 'CREATE INDEX cal_evt_type_idx ON calendar_events (event_type)';
    DBMS_OUTPUT.PUT_LINE('Created index: CAL_EVT_TYPE_IDX');
  END IF;
  
  SELECT COUNT(*) INTO cnt FROM user_indexes WHERE index_name = 'CAL_EVT_DATE_IDX';
  IF (cnt = 0) THEN
    EXECUTE IMMEDIATE 'CREATE INDEX cal_evt_date_idx ON calendar_events (event_year, event_month, event_day)';
    DBMS_OUTPUT.PUT_LINE('Created index: CAL_EVT_DATE_IDX');
  END IF;
  
  SELECT COUNT(*) INTO cnt FROM user_indexes WHERE index_name = 'CAL_EVT_GUILD_IDX';
  IF (cnt = 0) THEN
    EXECUTE IMMEDIATE 'CREATE INDEX cal_evt_guild_idx ON calendar_events (guild_id)';
    DBMS_OUTPUT.PUT_LINE('Created index: CAL_EVT_GUILD_IDX');
  END IF;
  
  SELECT COUNT(*) INTO cnt FROM user_indexes WHERE index_name = 'CAL_EVT_CITY_IDX';
  IF (cnt = 0) THEN
    EXECUTE IMMEDIATE 'CREATE INDEX cal_evt_city_idx ON calendar_events (city_id)';
    DBMS_OUTPUT.PUT_LINE('Created index: CAL_EVT_CITY_IDX');
  END IF;
  
  SELECT COUNT(*) INTO cnt FROM user_indexes WHERE index_name = 'CAL_EVT_ACTIVE_IDX';
  IF (cnt = 0) THEN
    EXECUTE IMMEDIATE 'CREATE INDEX cal_evt_active_idx ON calendar_events (active)';
    DBMS_OUTPUT.PUT_LINE('Created index: CAL_EVT_ACTIVE_IDX');
  END IF;
END;
/

-- Grant permissions on calendar_events
BEGIN
  EXECUTE IMMEDIATE 'GRANT SELECT ON calendar_events TO PUBLIC';
  DBMS_OUTPUT.PUT_LINE('Granted SELECT on CALENDAR_EVENTS to PUBLIC');
EXCEPTION
  WHEN OTHERS THEN NULL;
END;
/

-- Calendar Settings table
DECLARE
  cnt NUMBER;
BEGIN
  SELECT COUNT(*) INTO cnt FROM user_tables WHERE table_name = 'CALENDAR_SETTINGS';
  IF (cnt = 0) THEN
    EXECUTE IMMEDIATE '
      CREATE TABLE calendar_settings
      (
        setting_id INT NOT NULL,
        bg_texture VARCHAR2(256),
        src_x INT DEFAULT 0,
        src_y INT DEFAULT 0,
        src_w INT DEFAULT 512,
        src_h INT DEFAULT 512,
        last_modified DATE DEFAULT SYSDATE,
        modified_by NUMBER(20),
        PRIMARY KEY (setting_id)
      )';
    DBMS_OUTPUT.PUT_LINE('Created table: CALENDAR_SETTINGS');
    
    -- Insert default settings row
    EXECUTE IMMEDIATE 'INSERT INTO calendar_settings (setting_id, bg_texture, src_x, src_y, src_w, src_h) VALUES (1, ''ui_calendar_bg.dds'', 0, 0, 512, 512)';
    COMMIT;
    DBMS_OUTPUT.PUT_LINE('Inserted default calendar settings');
  ELSE
    DBMS_OUTPUT.PUT_LINE('Table CALENDAR_SETTINGS already exists');
  END IF;
END;
/

-- Grant permissions on calendar_settings
BEGIN
  EXECUTE IMMEDIATE 'GRANT SELECT ON calendar_settings TO PUBLIC';
  DBMS_OUTPUT.PUT_LINE('Granted SELECT on CALENDAR_SETTINGS to PUBLIC');
EXCEPTION
  WHEN OTHERS THEN NULL;
END;
/

-- Update version number
DECLARE
  cnt NUMBER;
BEGIN
  SELECT COUNT(*) INTO cnt FROM user_tables WHERE table_name = 'VERSION_NUMBER';
  IF (cnt > 0) THEN
    UPDATE version_number SET version_number = GREATEST(version_number, 272), min_version_number = GREATEST(min_version_number, 272);
    COMMIT;
    DBMS_OUTPUT.PUT_LINE('Updated VERSION_NUMBER to 272');
  END IF;
END;
/

DBMS_OUTPUT.PUT_LINE('');
DBMS_OUTPUT.PUT_LINE('Calendar system database update complete!');
/
```

---

## Tables Created

### CALENDAR_EVENTS
| Column | Type | Description |
|--------|------|-------------|
| event_id | VARCHAR2(64) | Primary key, unique event identifier |
| title | VARCHAR2(256) | Event title |
| description | VARCHAR2(2000) | Event description |
| event_type | INT | 0=Staff, 1=Guild, 2=City, 3=Server |
| event_year | INT | Year of the event |
| event_month | INT | Month (1-12) |
| event_day | INT | Day (1-31) |
| event_hour | INT | Hour (0-23) |
| event_minute | INT | Minute (0-59) |
| duration | INT | Duration in minutes |
| guild_id | INT | Guild ID (for guild events) |
| city_id | INT | City ID (for city events) |
| server_event_key | VARCHAR2(64) | Link to server events |
| recurring | CHAR(1) | Y/N - Is recurring event |
| recurrence_type | INT | 0=None, 1=Daily, 2=Weekly, 3=Monthly, 4=Yearly |
| broadcast_start | CHAR(1) | Y/N - Broadcast when event starts |
| active | CHAR(1) | Y/N - Is event currently active |
| creator_id | NUMBER(20) | NetworkId of creator |
| created_date | DATE | When the event was created |

### CALENDAR_SETTINGS
| Column | Type | Description |
|--------|------|-------------|
| setting_id | INT | Primary key (always 1) |
| bg_texture | VARCHAR2(256) | Background texture path |
| src_x | INT | Source rectangle X |
| src_y | INT | Source rectangle Y |
| src_w | INT | Source rectangle width |
| src_h | INT | Source rectangle height |
| last_modified | DATE | Last modification time |
| modified_by | NUMBER(20) | NetworkId of modifier |

---

## Post-Installation Steps

1. After running the SQL script, recompile the PL/SQL packages:
   ```sql
   @loader.plsqlh
   @loader.plsql
   @persister.plsqlh
   @persister.plsql
   ```

2. Rebuild the server with the new CalendarService code

3. Restart the game server

4. Calendar events and settings will now persist across server restarts

