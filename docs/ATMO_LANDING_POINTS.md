# Atmospheric Flight Landing Points System

## Overview

This feature adds landing points to atmospheric flight, enabling players to land their POB ships at designated locations on the planet map. Landing points can be placed at high altitudes to support vertical gameplay and elevated platforms.

## Features

### Landing Point Spawn Eggs
When a spawn egg is placed with `atmo.landing_point` objvar root during atmospheric flight, it will:
- Register as an entry on the planet map under the "Landing Points" category
- Show availability status (AVAILABLE in green, OCCUPIED in red, RESERVED in yellow)
- Allow pilots to right-click and select "Land Here" to initiate autopilot landing

### Required ObjVars
| ObjVar | Type | Description |
|--------|------|-------------|
| `atmo.landing_point.loc` | location | Fly-to location (includes height coordinate) |
| `atmo.landing_point.name` | string | Display name on planet map (e.g., "Docking Bay 327") |

### Optional ObjVars
| ObjVar | Type | Description |
|--------|------|-------------|
| `atmo.landing_point.disembark_loc` | location | Location where players disembark when landed |
| `atmo.landing_point.yaw` | float | Yaw angle (0-360 degrees) for ship orientation on landing |
| `atmo.landing_point.time_to_disembark` | int | Seconds allowed docked (-1 = unlimited) |
| `atmo.landing_point.loc_offset` | location | Optional offset for small platforms |

### Landing Mechanics
- **Cruise Altitude**: 1200 meters above terrain (for safe travel)
- **Landing Altitude**: Uses `loc.y` directly from the landing point objvar
- Ships ascend to cruise altitude, fly to destination, then descend to landing altitude
- Ships use existing autopilot methods for smooth flight
- Upon arrival, ship yaw is adjusted to match the landing point's `yaw` objvar

### Docking State
When a ship lands at a landing point, it enters a **DOCKED** state:
- Ship cannot be piloted manually
- Auto-pilot cannot be engaged
- Ship cannot be summoned elsewhere
- Ship cannot land at another location

To leave the docked state, players must use the **Starship Management Terminal**:
1. Navigate to **Docking Control** menu
2. Select **Undock Ship**
3. Confirm undocking

Undocking will:
- Move the ship to a safe altitude (100m above current position)
- Add a random horizontal offset (±50m) to prevent collision
- Account for terrain height to ensure the ship is safely above ground
- Clear the docked state, allowing normal operations

### Docking Timer System
When `time_to_disembark` is set (not -1):
- Players receive warnings at 60s, 30s, and 10s remaining via commPlayer
- **Docking Control menu available on Starship Management Terminal** for all players aboard
- Players can check remaining time and extend docking via the terminal
- Extension costs 20,000 credits for 5 minutes
- On expiry, players are teleported 200m away and the ship is automatically undocked

### Planet Map Integration
- Landing points appear under "Landing Points" category on planet map
- Right-click shows "Land Here" option when in atmospheric flight with a POB ship
- Status dynamically updates: AVAILABLE (green), OCCUPIED (red), RESERVED (yellow)

### Summon Ship Integration
- Remote landing available via summon ship item radial menu
- Shows list of all landing points with availability status
- Allows landing ship remotely while not aboard

## GM Configuration

Attach script `gm.atmo_landing_spawner_config` to any spawn egg to configure via radial menus:

- **Set Name**: Enter landing point display name
- **Set Location (From Position)**: Sets fly-to location from current position
- **Set Disembark Location**: Sets disembark point from current position
- **Set Yaw Angle**: Enter ship heading (0-360 degrees)
- **Set Time Limit**: Enter docking duration in seconds (-1 for unlimited)
- **Show Configuration**: Display current settings
- **Clear Configuration**: Remove all landing point settings
- **Apply & Activate**: Enable the landing point and register on map

## File Changes

### New Scripts (dsrc/sku.0/sys.server/compiled/game/script/)
- `space/atmo/atmo_landing_registry.java` - Library for managing landing points
- `space/atmo/atmo_landing_point.java` - Landing point spawn egg script
- `space/atmo/atmo_landing_docked.java` - Ship docking timer management
- `gm/atmo_landing_spawner_config.java` - GM configuration tool

### Modified Scripts
- `space/ship/summon_ship.java` - Added remote landing functionality
- `space/combat/combat_ship.java` - Added landing point arrival handling
- `player/player_vehicle.java` - Added AtmoLandingRequest message handler
- `terminal/terminal_pob_ship.java` - Added Docking Control menu for landing points

### Client Changes (C++)
- `SwgCuiPlanetMap.cpp` - Added "Land Here" popup option and handler
- `Client.cpp` - Added AtmoLandingRequest message handler

### Data Changes
- `datatables/player/planet_map_cat.tab` - Added `atmo_landing` category (index 87)
- `string/en/map_loc_cat_n.tab` - Added "Landing Points" display name
- `string/en/space/atmo.tab` - New string file for UI text

## Usage Example

1. Create a spawn egg at your desired landing location
2. Attach script: `/attachScript <egg_oid> gm.atmo_landing_spawner_config`
3. Use radial menu to configure:
   - Set name to "Sky Platform Alpha"
   - Fly your ship to the exact landing position
   - Use "Set Location" to capture the position
   - Set yaw to desired heading
   - Set time limit (or -1 for unlimited)
4. Use "Apply & Activate" to enable the landing point
5. Players in atmospheric flight will see "Sky Platform Alpha (AVAILABLE)" on their planet map
6. Right-click and select "Land Here" to initiate autopilot landing

## Technical Notes

- Landing points are dynamically registered/unregistered based on atmospheric flight scene
- Status updates occur on reservation, arrival, and departure
- ETA-based preemption prevents race conditions when multiple ships approach
- All data is stored in objvars - no datatables required for landing point configuration

