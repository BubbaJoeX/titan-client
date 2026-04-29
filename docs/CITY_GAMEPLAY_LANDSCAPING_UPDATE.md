# City Gameplay & Landscaping Update - Implementation Plan

## Overview

This document outlines the implementation plan for major city enhancements:
1. **City Terrain Painting System** - Mayors can modify landscape/shaders (radius + road painting)
2. **City Bulldoze System** - Flatten entire city terrain with persistence
3. **Enhanced Taxation System** - More granular tax controls including Starship Landing Tax
4. **Eviction Process & City Court** - Formal eviction/appeal system with elected judges
5. **Rank 6 Industrial Starport** - Interplanetary travel from player cities
6. **City Expulsion Warning System** - 120s warning with militia enforcement and TEF restrictions

---

## Design Decisions

### Terrain Painting Persistence
**Decision: YES** - Painted terrain will persist across server restarts. Layer specifications are serialized and stored as objvars on the City Hall object, then synced to clients when they enter the city.

### City Judge Selection
**Decision: ELECTED** - City Judges are elected by citizens rather than appointed by the mayor. This ensures impartial dispute resolution. Election occurs alongside mayoral elections or as a separate ballot.

### Expulsion TEF Restrictions
**Decision: YES** - Militia attacking expelled players WILL generate TEF (Temporary Enemy Flag). While in TEF state, expelled players cannot enter buildings, use terminals, or interact with city services until TEF wears off. This prevents exploit scenarios while maintaining balance.

---

## Feature 1: City Terrain Painting System (CLIENT-ORIENTED)

### Architecture Overview

The terrain painting system uses a **client-oriented approach** where:
1. Server stores terrain region definitions (objvars on City Hall)
2. Server broadcasts terrain data to clients via network messages
3. Client-side `CityTerrainLayerManager` handles shader overlays and height modifications
4. Clients dynamically enumerate available shaders from terrain files

This approach ensures:
- Dynamic shader discovery (no hardcoded lists)
- Efficient network sync (only metadata sent, not textures)
- Proper client-side rendering integration
- Real-time updates for all players in city

### Paint Modes

#### Mode 1: Circle Painting
- Select shader from dynamically populated dropdown
- Set radius (5m - 50m based on city rank)
- Stand at center location and apply

#### Mode 2: Road/Line Painting
1. Select shader from dropdown
2. Set road width (2m - 20m)
3. Walk to first point, click "Set Marker"
4. Walk to second point, click "Set Marker"
5. Apply to paint road segment

#### Mode 3: Flatten
- Set target height
- Set blend distance for smooth edges
- Stand at center and apply

### Client Components

**CityTerrainLayerManager** (`clientTerrain/`):
- Manages terrain modification overlays
- Enumerates available shaders from TreeFile
- Provides shader override queries for terrain chunks
- Handles height modification for flatten regions
- Processes network messages from server

**SwgCuiCityTerrainPainter** (`swgClientUserInterface/`):
- Full UI for terrain painting
- Dynamic shader dropdown populated from client files
- Mode selection (circle/line/flatten)
- Live preview of selected shader
- Marker system for line mode

### Server Components

**city_terrain_handler.java** (`systems/city/`):
- Syncs terrain data when player enters city
- Sends terrain modifications to clients
- Handles OnEnteredCity/OnLeftCity events

**cmd_cityTerrainPaint.java** (`commands/`):
- Command handler for `/cityTerrainPaint`
- Validates mayor permissions and city rank
- Stores region data in objvars
- Broadcasts updates to all players in city

### Data Storage (Objvars on City Hall)

```
city.terrain.region_ids[]           - Array of region IDs
city.terrain.<id>.type              - "RADIUS", "ROAD", or "FLATTEN"
city.terrain.<id>.shader            - Shader template path
city.terrain.<id>.center_x/z        - Center coordinates
city.terrain.<id>.radius            - For circle mode
city.terrain.<id>.end_x/z           - For line mode end point
city.terrain.<id>.width             - For line mode
city.terrain.<id>.height            - For flatten mode
city.terrain.<id>.blend_dist        - Edge blending distance
city.terrain.<id>.created           - Timestamp
city.terrain.<id>.creator           - Player who created it
```

### Network Messages

**CityTerrainModifyMessage** (server → client):
- Modification type, region ID, shader, coordinates, dimensions

**CityTerrainSyncMessage** (server → client on city enter):
- All regions for a city in one message

**CityTerrainShaderListMessage** (server → client):
- Available shaders for the planet

### Rank Requirements

| City Rank | Max Radius | Can Paint | Can Flatten |
|-----------|------------|-----------|-------------|
| 1         | 10m        | No        | No          |
| 2         | 15m        | Yes       | No          |
| 3         | 20m        | Yes       | Yes         |
| 4         | 30m        | Yes       | Yes         |
| 5         | 40m        | Yes       | Yes         |
| 6         | 50m        | Yes       | Yes         |

### Implementation Status

#### Client Files Created:
- `clientTerrain/src/shared/core/CityTerrainLayerManager.h` - Manages terrain overlays
- `clientTerrain/src/shared/core/CityTerrainLayerManager.cpp` - Implementation
- `clientTerrain/include/public/clientTerrain/CityTerrainLayerManager.h` - Public header
- `swgClientUserInterface/src/shared/page/SwgCuiCityTerrainPainter.h` - UI header
- `swgClientUserInterface/src/shared/page/SwgCuiCityTerrainPainter.cpp` - UI sends `CityTerrainPaintRequestMessage`
- `ui/ui_city_terrain_painter.inc` - UI page definition

#### Server Files Created:
- `serverGame/src/shared/city/CityTerrainService.h` - C++ message handler
- `serverGame/src/shared/city/CityTerrainService.cpp` - Validates requests, stores data, broadcasts
- `script/systems/city/city_terrain_handler.java` - Player terrain sync handler + script triggers
- `script/systems/city/city_terrain_manager.java` - City Hall radial menu management
- `script/systems/city/player_city_terrain_sync.java` - Player city enter/leave handler

#### Network Messages (Client → Server):
- `CityTerrainPaintRequestMessage` - Client requests terrain paint
- `CityTerrainRemoveRequestMessage` - Client requests region removal
- `CityTerrainSyncRequestMessage` - Client requests terrain sync

#### Network Messages (Server → Client):
- `CityTerrainModifyMessage` - Server broadcasts terrain modification
- `CityTerrainPaintResponseMessage` - Server confirms paint success/failure

### Terrain Sync Timing (Prevent Floating Buildings)

To prevent buildings from appearing to float when terrain modifications exist, terrain data is synced to clients at multiple points:

1. **On Client Ready** (before player sees anything):
   - `CreatureObject::onClientReady()` calls `CityTerrainService::sendNearbyCitiesTerrainSync()`
   - Sends terrain data for ALL cities within 1500m of player position
   - This ensures terrain is modified before buildings are rendered

2. **On City Enter** (reinforcement):
   - `CreatureObject::setLocatedInCityId()` calls `CityTerrainService::sendTerrainSyncToClient()`
   - Sends terrain data for the specific city being entered
   - Handles edge cases where player approaches from far away

3. **On Terrain Paint** (real-time updates):
   - `CityTerrainService::broadcastToCity()` sends to all players near the city
   - Ensures all players see changes in real-time

The 1500m range in the initial sync covers typical view distances plus margin for building LOD pop-in, preventing the "floating building" artifact where terrain is rendered at original height but building expects modified height.

### Object Height Adjustment System

When terrain is modified within city limits, ALL objects (structures, decorations, buildout objects) must be adjusted to the new terrain height. This ensures:
- Player structures don't float above or sink into modified terrain
- NPC vendors and objects remain accessible
- Buildout objects (static world objects) snap to terrain properly

#### Functions

1. **`adjustAllObjectsInModifiedArea(cityId, centerX, centerZ, radius)`**
   - Called immediately after terrain is painted/flattened
   - Scans all objects within radius + 50m buffer
   - Calculates proper height at each object's position using `getModifiedTerrainHeight()`
   - Adjusts object positions (excluding players, creatures, interior objects)
   - Logs all adjusted structures including player-placed buildings

2. **`adjustBuildoutObjectsInCity(cityId)`**
   - Called when city hall loads (server restart)
   - Scans entire city radius + margin
   - Adjusts ALL objects (persisted and buildout) to terrain height
   - Ensures structures snap to correct height after restart

3. **`adjustObjectToTerrainHeight(object)`**
   - Single object adjustment using `getModifiedTerrainHeight()`
   - Respects minimum height difference threshold (0.1m)
   - Skips interior objects (attached to cells)

4. **`getModifiedTerrainHeight(x, z, originalHeight, outHeight)`**
   - Core calculation function
   - Considers all active flatten regions with priority ordering
   - Blends heights at region edges for smooth transitions
   - Returns true if modification applies, false for original terrain

#### On Region Removal

When a flatten region is removed:
- Region data is saved before deletion
- Objects in the removed region are re-adjusted
- They snap to remaining flatten regions or original terrain
- Prevents objects from being stuck at old flatten height

---

## Feature 1.5: City Bulldoze System

#### Database Schema

```sql
-- Add to city tables or create new table
CREATE TABLE city_terrain_regions (
    city_id NUMBER NOT NULL,
    region_id VARCHAR2(64) NOT NULL,
    region_type VARCHAR2(16) NOT NULL, -- 'RADIUS' or 'ROAD'
    center_x NUMBER,
    center_z NUMBER,
    radius NUMBER,
    -- Road-specific fields
    start_x NUMBER,
    start_z NUMBER,
    end_x NUMBER,
    end_z NUMBER,
    road_width NUMBER,
    -- Common fields
    shader_template VARCHAR2(256),
    affector_type VARCHAR2(64),
    layer_data BLOB,
    created_time NUMBER,
    CONSTRAINT pk_city_terrain PRIMARY KEY (city_id, region_id)
);
```

#### Available Terrain Shaders/Affectors

Pull from existing terrain files:
- Grass variants (green, brown, dry)
- Sand/desert textures
- Stone/rock surfaces
- Dirt/mud textures
- **Cobblestone road** (new)
- **Duracrete path** (new)
- **Gravel road** (new)
- Custom city-specific textures

#### UI Implementation

Add to `terminal_city.java`:
```java
// New menu options under City Management
public static final string_id SID_TERRAIN_PAINT = new string_id(STF, "terrain_paint");
public static final string_id SID_TERRAIN_PAINT_RADIUS = new string_id(STF, "terrain_paint_radius");
public static final string_id SID_TERRAIN_PAINT_ROAD = new string_id(STF, "terrain_paint_road");
public static final string_id SID_ROAD_SET_MARKER_1 = new string_id(STF, "road_set_marker_1");
public static final string_id SID_ROAD_SET_MARKER_2 = new string_id(STF, "road_set_marker_2");
public static final string_id SID_ROAD_SELECT_SHADER = new string_id(STF, "road_select_shader");
public static final string_id SID_ROAD_PAINT = new string_id(STF, "road_paint");

// Road marker tracking objvars on mayor
"city.road_paint.marker1_x" - float
"city.road_paint.marker1_z" - float
"city.road_paint.marker2_x" - float
"city.road_paint.marker2_z" - float
"city.road_paint.width" - float
"city.road_paint.shader" - String
```

---

## Feature 1.5: City Bulldoze System

### Concept

Allow mayors to **flatten the entire city terrain** to a uniform height. All buildings and city objects within city limits are adjusted to sit on this flattened surface. The bulldozed terrain persists across server restarts and uses smooth blending at city edges to prevent client terrain rendering issues.

### Bulldoze Process

1. Mayor selects "Bulldoze City" from City Hall terminal
2. Confirmation dialog warns of irreversible action
3. System calculates average terrain height within city bounds
4. Terrain is flattened to calculated height (or mayor-specified height)
5. All structures are adjusted to new ground level
6. Edge blending applied within 20m of city boundary
7. Changes synced to all clients and persisted

### Technical Implementation

**New Script Methods:**
```cpp
// Calculate average height within city
float cityCalculateAverageTerrainHeight(int cityId);

// Execute bulldoze operation
void cityBulldozeTerrain(int cityId, float targetHeight, float edgeBlendDistance);

// Adjust all structures to new ground level
void cityAdjustStructuresToTerrain(int cityId);
```

**CityTerrainManager Extension:**
```cpp
class CityTerrainManager
{
public:
    // ...existing methods...
    
    // Bulldoze system
    static void bulldozeTerrain(int cityId, float targetHeight);
    static float calculateAverageHeight(int cityId, int samplePoints);
    static void applyEdgeBlending(int cityId, float blendDistance);
    static void adjustStructurePositions(int cityId, float heightDelta);
    static void persistBulldozeState(int cityId, float height);
    static void loadBulldozeState(int cityId);
};
```

**Bulldoze Algorithm:**
```cpp
void CityTerrainManager::bulldozeTerrain(int cityId, float targetHeight)
{
    CityInfo const& cityInfo = CityInterface::getCityInfo(cityId);
    Vector2d cityCenter(cityInfo.getX(), cityInfo.getZ());
    float radius = static_cast<float>(cityInfo.getRadius());
    
    // Create flattening affector layer
    TerrainGenerator::Layer* flattenLayer = 
        TerrainModificationHelper::importLayer("terrain/city_flatten.lay");
    
    // Set layer parameters
    TerrainModificationHelper::setPosition(flattenLayer, cityCenter);
    TerrainModificationHelper::setHeight(flattenLayer, targetHeight);
    
    // Configure circular boundary matching city radius
    AffectorHeightConstant* heightAffector = findHeightAffector(flattenLayer);
    if (heightAffector)
    {
        heightAffector->setHeight(targetHeight);
    }
    
    // Apply edge blending for smooth transition
    const float BLEND_DISTANCE = 20.0f;
    BoundaryCircle* boundary = findBoundary(flattenLayer);
    if (boundary)
    {
        boundary->setRadius(radius);
        boundary->setFeatherDistance(BLEND_DISTANCE);
        boundary->setFeatherType(BoundaryCircle::FT_Linear);
    }
    
    // Apply layer to terrain system
    TerrainObject::getInstance()->addLayer(flattenLayer);
    
    // Adjust all structures within city
    adjustStructurePositions(cityId, targetHeight);
    
    // Sync to all clients
    broadcastTerrainUpdate(cityId);
    
    // Persist to database
    persistBulldozeState(cityId, targetHeight);
}

void CityTerrainManager::adjustStructurePositions(int cityId, float newHeight)
{
    std::vector<NetworkId> structures;
    CityInterface::getCityStructureIds(cityId, structures);
    
    for (const NetworkId& structureId : structures)
    {
        ServerObject* structure = ServerWorld::findObjectByNetworkId(structureId);
        if (structure)
        {
            Vector currentPos = structure->getPosition_w();
            float heightDelta = newHeight - currentPos.y;
            
            // Only adjust if significant height change
            if (std::abs(heightDelta) > 0.1f)
            {
                Vector newPos(currentPos.x, newHeight, currentPos.z);
                structure->setPosition_w(newPos);
                
                // Trigger terrain modification recalculation for building
                structure->onTerrainChanged();
            }
        }
    }
}
```

**Edge Blending for Smooth Terrain:**
```cpp
void CityTerrainManager::applyEdgeBlending(int cityId, float blendDistance)
{
    // Create gradient falloff at city edges
    // Uses linear interpolation from city height to natural terrain height
    // Prevents hard edges that cause client rendering artifacts
    
    CityInfo const& cityInfo = CityInterface::getCityInfo(cityId);
    float radius = static_cast<float>(cityInfo.getRadius());
    float bulldozedHeight = cityInfo.getBulldozedHeight();
    
    // Sample natural terrain heights at city boundary
    const int SAMPLE_COUNT = 36; // Every 10 degrees
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        float angle = (2.0f * PI * i) / SAMPLE_COUNT;
        Vector2d edgePoint = cityCenter + Vector2d(
            cos(angle) * radius,
            sin(angle) * radius
        );
        
        float naturalHeight = TerrainObject::getInstance()->
            getHeight(edgePoint.x, edgePoint.y);
        
        // Create blend zone from (radius - blendDistance) to radius
        createBlendZone(edgePoint, bulldozedHeight, naturalHeight, blendDistance);
    }
}
```

**Persistence Schema:**
```sql
-- Add bulldoze state to city table or separate table
CREATE TABLE city_bulldoze (
    city_id NUMBER PRIMARY KEY,
    bulldozed_height NUMBER NOT NULL,
    bulldozed_time NUMBER NOT NULL,
    edge_blend_distance NUMBER DEFAULT 20,
    CONSTRAINT fk_city_bulldoze FOREIGN KEY (city_id) 
        REFERENCES cities(city_id) ON DELETE CASCADE
);
```

**Server Startup Load:**
```cpp
void CityTerrainManager::initializeAllCities()
{
    std::vector<int> cityIds;
    CityInterface::getAllCityIds(cityIds);
    
    for (int cityId : cityIds)
    {
        // Load persisted terrain paint regions
        loadPersistedTerrain(cityId);
        
        // Load bulldoze state if exists
        if (hasBulldozeState(cityId))
        {
            loadBulldozeState(cityId);
        }
    }
}
```

**UI Implementation:**
```java
// terminal_city.java additions
public static final string_id SID_BULLDOZE_CITY = new string_id(STF, "bulldoze_city");
public static final string_id SID_BULLDOZE_CONFIRM = new string_id(STF, "bulldoze_confirm");
public static final string_id SID_BULLDOZE_HEIGHT_PROMPT = new string_id(STF, "bulldoze_height_prompt");

// Confirmation dialog
public void confirmBulldoze(obj_id player, int cityId) throws InterruptedException {
    String prompt = "WARNING: This will flatten all terrain within city limits. " +
                   "All structures will be adjusted to the new height. " +
                   "This action is irreversible. Continue?";
    sui.msgbox(self, player, prompt, sui.YES_NO, 
               "@city/city:bulldoze_city", "handleBulldozeConfirm");
}

// Height selection (optional - can use auto-calculated average)
public void selectBulldozeHeight(obj_id player, int cityId) throws InterruptedException {
    float avgHeight = cityCalculateAverageTerrainHeight(cityId);
    String prompt = "Enter target height (current average: " + avgHeight + "):";
    sui.inputbox(self, player, prompt, sui.OK_CANCEL, 
                 "@city/city:bulldoze_height", sui.INPUT_NORMAL,
                 String.valueOf((int)avgHeight), "handleBulldozeHeight", null);
}
```

---

## Feature 2: Enhanced City Taxation System

### Current State

The city system already supports three tax types:
- **Income Tax** (0-2000 credits) - Weekly citizen payment
- **Property Tax** (0-50%) - Structure maintenance multiplier  
- **Sales Tax** (0-20%) - Vendor transaction percentage
- **Travel Fee** (1-500 credits) - Shuttle usage

### New Tax Types

| Tax Type | Range | Description | Collection Point |
|----------|-------|-------------|------------------|
| Crafting Tax | 0-10% | Tax on items crafted within city | On craft completion |
| Vendor License | 0-5000/week | Weekly fee for vendor owners | Weekly pulse |
| Structure Placement Fee | 0-25000 | One-time fee for placing structures | On structure place |
| Event Permit Fee | 0-10000 | Fee for hosting city events | On event creation |
| **Starship Landing Tax** | **0-50000** | **Fee for autopilot landing within city** | **On landing completion** |

### Starship Landing Tax - Detailed Implementation

When a player uses autopilot to land within city limits (atmospheric flight landing points), the city collects a landing tax. If the player cannot afford the tax, their ship is ejected outside city limits.

#### Collection Flow
```
Player autopilots to landing point within city
         ↓
System checks if landing point is in a city
         ↓
   ┌─────┴─────┐
   ↓           ↓
 Not in      In city
 city           ↓
   ↓        Check landing tax
 Normal        ↓
 landing  ┌────┴────┐
          ↓         ↓
       Can pay   Cannot pay
          ↓         ↓
       Deduct    Eject ship
       tax       (city_radius)m away
          ↓         ↓
       Land      Notify player
       normally  "Insufficient funds"
```

#### Script Implementation

**Modify `space/atmo/atmo_landing_point.java`:**
```java
public int handleLandingAttempt(obj_id ship, obj_id landingPoint) 
    throws InterruptedException {
    
    location landingLoc = getLocation(landingPoint);
    int cityId = getCityAtLocation(landingLoc, 0);
    
    if (cityId > 0 && cityExists(cityId)) {
        int landingTax = cityGetStarshipLandingTax(cityId);
        
        if (landingTax > 0) {
            obj_id pilot = getPilot(ship);
            
            // Check if pilot can afford the tax
            int playerCash = getTotalMoney(pilot);
            
            if (playerCash < landingTax) {
                // Eject ship outside city limits
                ejectShipFromCity(ship, cityId);
                
                String cityName = cityGetName(cityId);
                prose_package pp = prose.getPackage(
                    SID_LANDING_TAX_INSUFFICIENT,
                    cityName,
                    landingTax,
                    playerCash
                );
                sendSystemMessage(pilot, pp);
                
                return SCRIPT_OVERRIDE; // Cancel landing
            }
            
            // Collect landing tax
            obj_id cityHall = cityGetCityHall(cityId);
            if (money.pay(pilot, cityHall, landingTax, "landing_tax", null, false)) {
                String cityName = cityGetName(cityId);
                prose_package pp = prose.getPackage(
                    SID_LANDING_TAX_PAID,
                    cityName,
                    landingTax
                );
                sendSystemMessage(pilot, pp);
            }
        }
    }
    
    return SCRIPT_CONTINUE;
}

public void ejectShipFromCity(obj_id ship, int cityId) throws InterruptedException {
    // Get city center and radius
    location cityLoc = cityGetLocation(cityId);
    int cityRadius = cityGetRadius(cityId);
    
    // Calculate ejection point outside city radius
    float ejectDistance = cityRadius + 50.0f; // 50m buffer
    
    // Random angle from city center
    float angle = rand(0.0f, 2.0f * PI);
    float ejectX = cityLoc.x + (float)(Math.cos(angle) * ejectDistance);
    float ejectZ = cityLoc.z + (float)(Math.sin(angle) * ejectDistance);
    
    // Get terrain height at ejection point + safe altitude
    float terrainHeight = getHeightAtLocation(ejectX, ejectZ);
    float ejectY = terrainHeight + 200.0f; // 200m above terrain
    
    // Warp ship to ejection point
    location ejectLoc = new location(ejectX, ejectY, ejectZ, 
                                     cityLoc.area, cityLoc.cell);
    setLocation(ship, ejectLoc);
    
    // Clear autopilot state
    removeObjVar(ship, "atmo.landing");
    removeObjVar(ship, "atmo.docked");
    
    // Resume normal flight
    messageTo(ship, "handleUndock", null, 0.0f, false);
}
```

**String Table Entries:**
```java
public static final string_id SID_LANDING_TAX_PAID = 
    new string_id("city/city", "landing_tax_paid");
    // "The Imperial Docking Authority of %TU has collected %DI credits in landing fees."

public static final string_id SID_LANDING_TAX_INSUFFICIENT = 
    new string_id("city/city", "landing_tax_insufficient");
    // "You cannot afford the %DI credit landing fee for %TU. You only have %DI credits. 
    //  Your ship has been redirected outside city limits."
```

### Implementation

**CityInfo Extension (src/engine/server/library/serverGame/src/shared/city/CityInfo.h):**
```cpp
int m_craftingTax;
int m_vendorLicenseFee;
int m_structurePlacementFee;
int m_eventPermitFee;
int m_starshipLandingTax;  // NEW
```

**New Controller Messages:**
```cpp
CM_citySetCraftingTax,
CM_citySetVendorLicenseFee,
CM_citySetStructurePlacementFee,
CM_citySetEventPermitFee,
CM_citySetStarshipLandingTax,  // NEW
```

**Script Methods (ScriptMethodsCity.cpp):**
```cpp
void citySetCraftingTax(int cityId, int tax);
int cityGetCraftingTax(int cityId);
void citySetVendorLicenseFee(int cityId, int fee);
int cityGetVendorLicenseFee(int cityId);
void citySetStructurePlacementFee(int cityId, int fee);
int cityGetStructurePlacementFee(int cityId);
void citySetStarshipLandingTax(int cityId, int tax);  // NEW
int cityGetStarshipLandingTax(int cityId);            // NEW
```

**Script Changes (city.java):**
```java
// Add new tax constants
public static final int TAX_CRAFTING = 4;
public static final int TAX_VENDOR_LICENSE = 5;
public static final int TAX_STRUCTURE_PLACE = 6;
public static final int TAX_EVENT_PERMIT = 7;
public static final int TAX_STARSHIP_LANDING = 8;  // NEW

// Add new min/max arrays
public static final int[] TAX_MAX_EXTENDED = {
    2000,   // Income
    50,     // Property
    20,     // Sales
    500,    // Travel
    30,     // Garage
    10,     // Crafting (%)
    5000,   // Vendor License
    25000,  // Structure Placement
    10000,  // Event Permit
    50000   // Starship Landing (NEW)
};
```

**Hook Points:**
- Crafting: `crafting_base.java` - `completeCraft()` method
- Vendor: `vendor_start.java` - Weekly maintenance cycle
- Structure: `player_structure.java` - `placeStructure()` method
- **Starship Landing: `space/atmo/atmo_landing_point.java` - `handleLandingAttempt()` method (NEW)**

---

## Feature 3: Eviction Process & City Court System

### Eviction Process Flow

```
Mayor initiates eviction
         ↓
Citizen receives warning mail (7-day grace period)
         ↓
   ┌─────┴─────┐
   ↓           ↓
Citizen     Citizen files
leaves      appeal
   ↓           ↓
Eviction   Elected Judge
complete   reviews case
              ↓
       ┌──────┴──────┐
       ↓             ↓
    Upheld       Reversed
       ↓             ↓
    Eviction     Citizen
    proceeds     stays
```

### Judge Election System

City Judges are **elected by citizens** to ensure impartial dispute resolution. Elections occur on a regular schedule alongside or separate from mayoral elections.

#### Election Rules
- **Eligibility:** Any citizen in good standing (not under eviction) can run
- **Term Length:** 30 days
- **Max Judges:** 1 judge per 20 citizens (minimum 1, maximum 5)
- **Voting:** All citizens can vote; ranked choice or simple majority
- **Election Trigger:** Automatic when term expires or seat vacant

#### Election Implementation

**New Script: `dsrc/sku.0/sys.server/compiled/game/script/systems/city/city_judge_election.java`**
```java
package script.systems.city;

public class city_judge_election extends script.base_script {
    
    public static final int JUDGE_TERM_LENGTH = 30 * 24 * 60 * 60; // 30 days
    public static final int ELECTION_DURATION = 3 * 24 * 60 * 60;  // 3 days voting
    public static final int CITIZENS_PER_JUDGE = 20;
    public static final int MIN_JUDGES = 1;
    public static final int MAX_JUDGES = 5;
    
    public static int getMaxJudgeCount(int cityId) throws InterruptedException {
        obj_id[] citizens = cityGetCitizenIds(cityId);
        int citizenCount = (citizens != null) ? citizens.length : 0;
        int maxJudges = Math.max(MIN_JUDGES, citizenCount / CITIZENS_PER_JUDGE);
        return Math.min(maxJudges, MAX_JUDGES);
    }
    
    public static void startJudgeElection(int cityId) throws InterruptedException {
        // Create election ballot
        setObjVar(cityGetCityHall(cityId), "city.judge_election.active", 1);
        setObjVar(cityGetCityHall(cityId), "city.judge_election.start_time", getGameTime());
        setObjVar(cityGetCityHall(cityId), "city.judge_election.end_time", 
                  getGameTime() + ELECTION_DURATION);
        
        // Notify all citizens
        broadcastToCitizens(cityId, SID_JUDGE_ELECTION_STARTED);
        
        // Schedule election end
        dictionary params = new dictionary();
        params.put("cityId", cityId);
        messageTo(cityGetCityHall(cityId), "handleJudgeElectionEnd", 
                  params, ELECTION_DURATION, false);
    }
    
    public static void registerCandidate(obj_id citizen, int cityId) 
        throws InterruptedException {
        
        // Verify citizen is eligible
        if (!city.isCitizenOfCity(citizen, cityId)) {
            sendSystemMessage(citizen, SID_NOT_CITIZEN);
            return;
        }
        
        // Check not under eviction
        if (hasObjVar(citizen, "city.eviction.initiated_time")) {
            sendSystemMessage(citizen, SID_CANNOT_RUN_UNDER_EVICTION);
            return;
        }
        
        // Add to candidate list
        obj_id cityHall = cityGetCityHall(cityId);
        obj_id[] candidates = getObjIdArrayObjVar(cityHall, 
                                                   "city.judge_election.candidates");
        candidates = utils.addElement(candidates, citizen);
        setObjVar(cityHall, "city.judge_election.candidates", candidates);
        
        sendSystemMessage(citizen, SID_REGISTERED_AS_JUDGE_CANDIDATE);
    }
    
    public static void castVote(obj_id voter, obj_id candidate, int cityId) 
        throws InterruptedException {
        
        // Verify voter is citizen
        if (!city.isCitizenOfCity(voter, cityId)) {
            return;
        }
        
        // Check hasn't already voted
        obj_id cityHall = cityGetCityHall(cityId);
        obj_id[] hasVoted = getObjIdArrayObjVar(cityHall, 
                                                 "city.judge_election.voted");
        if (utils.isObjIdInArray(voter, hasVoted)) {
            sendSystemMessage(voter, SID_ALREADY_VOTED);
            return;
        }
        
        // Record vote
        String voteKey = "city.judge_election.votes." + candidate;
        int currentVotes = getIntObjVar(cityHall, voteKey);
        setObjVar(cityHall, voteKey, currentVotes + 1);
        
        hasVoted = utils.addElement(hasVoted, voter);
        setObjVar(cityHall, "city.judge_election.voted", hasVoted);
        
        sendSystemMessage(voter, SID_VOTE_RECORDED);
    }
    
    public static void finalizeElection(int cityId) throws InterruptedException {
        obj_id cityHall = cityGetCityHall(cityId);
        obj_id[] candidates = getObjIdArrayObjVar(cityHall, 
                                                   "city.judge_election.candidates");
        
        if (candidates == null || candidates.length == 0) {
            // No candidates - extend election or leave seats vacant
            return;
        }
        
        // Sort candidates by vote count
        int maxJudges = getMaxJudgeCount(cityId);
        obj_id[] winners = getTopVoteGetters(cityId, candidates, maxJudges);
        
        // Clear old judges
        clearJudges(cityId);
        
        // Install new judges
        for (obj_id winner : winners) {
            city.addJudge(cityId, winner);
            
            // Set term expiration
            setObjVar(winner, "city.judge.term_expires", 
                      getGameTime() + JUDGE_TERM_LENGTH);
        }
        
        // Clean up election vars
        removeObjVar(cityHall, "city.judge_election");
        
        // Notify city
        broadcastToCitizens(cityId, SID_JUDGE_ELECTION_COMPLETE);
    }
}
```

### Implementation

**New Permission Flag (city.java):**
```java
public static final int CP_JUDGE = (1 << 7);
```

**New Objvars:**
```java
// On citizen being evicted
"city.eviction.initiated_time" - int (game time)
"city.eviction.initiated_by" - obj_id (mayor)
"city.eviction.reason" - string
"city.eviction.appeal_pending" - int (0/1)
"city.eviction.appeal_time" - int (game time)
"city.eviction.judge_assigned" - obj_id

// On elected judge
"city.judge.term_expires" - int (game time)
"city.judge.elected_time" - int (game time)
"city.judge.votes_received" - int
```

**New Script: `dsrc/sku.0/sys.server/compiled/game/script/systems/city/city_court.java`**
```java
package script.systems.city;

public class city_court extends script.base_script {
    
    public static final int EVICTION_GRACE_PERIOD = 7 * 24 * 60 * 60; // 7 days
    public static final int APPEAL_REVIEW_PERIOD = 3 * 24 * 60 * 60; // 3 days
    
    public static boolean beginEviction(obj_id mayor, obj_id citizen, int cityId, 
                                        String reason) {
        // Check mayor has authority
        // Check citizen is not already being evicted
        // Set eviction objvars
        // Send mail notification
        // Schedule messageTo for grace period expiration
    }
    
    public static boolean appealEviction(obj_id citizen, int cityId, String defense) {
        // Check citizen has active eviction
        // Check appeal not already filed
        // Set appeal pending
        // Notify judges
    }
    
    public static void handleJudgeDecision(obj_id judge, obj_id citizen, 
                                          int cityId, boolean upheld) {
        // Validate judge has CP_JUDGE
        // Process decision
        // Notify all parties
        // If upheld, complete eviction
        // If reversed, clear eviction vars
    }
}
```

**terminal_city.java Menu Additions:**
```java
// For Mayor
mi.addSubMenu(menu, menu_info_types.SERVER_MENU19, SID_BEGIN_EVICTION);
mi.addSubMenu(menu, menu_info_types.SERVER_MENU20, SID_MANAGE_JUDGES);

// For Judge
mi.addSubMenu(menu, menu_info_types.SERVER_MENU21, SID_REVIEW_APPEALS);

// For Citizens
mi.addSubMenu(menu, menu_info_types.SERVER_MENU22, SID_MY_EVICTION_STATUS);
```

### Database Schema

```sql
CREATE TABLE city_evictions (
    eviction_id NUMBER PRIMARY KEY,
    city_id NUMBER NOT NULL,
    citizen_id NUMBER NOT NULL,
    initiated_by NUMBER NOT NULL,
    initiated_time NUMBER NOT NULL,
    reason VARCHAR2(512),
    status VARCHAR2(32), -- PENDING, APPEALED, UPHELD, REVERSED, COMPLETED, CANCELLED
    appeal_filed_time NUMBER,
    appeal_defense VARCHAR2(1024),
    judge_id NUMBER,
    judge_decision_time NUMBER,
    judge_notes VARCHAR2(512)
);

-- Judges are ELECTED, not appointed
CREATE TABLE city_judges (
    city_id NUMBER NOT NULL,
    citizen_id NUMBER NOT NULL,
    elected_time NUMBER NOT NULL,
    term_expires NUMBER NOT NULL,
    votes_received NUMBER NOT NULL,
    CONSTRAINT pk_city_judges PRIMARY KEY (city_id, citizen_id)
);

-- Track judge election history
CREATE TABLE city_judge_elections (
    election_id NUMBER PRIMARY KEY,
    city_id NUMBER NOT NULL,
    start_time NUMBER NOT NULL,
    end_time NUMBER NOT NULL,
    total_votes NUMBER,
    status VARCHAR2(32) -- ACTIVE, COMPLETED, CANCELLED
);

-- Track individual votes (for audit/recount)
CREATE TABLE city_judge_votes (
    election_id NUMBER NOT NULL,
    voter_id NUMBER NOT NULL,
    candidate_id NUMBER NOT NULL,
    vote_time NUMBER NOT NULL,
    CONSTRAINT pk_judge_votes PRIMARY KEY (election_id, voter_id)
);
```

---

## Feature 4: Rank 6 Industrial Starport

### City Rank Extension

**Update `datatables/city/city_rank.tab`:**

| RANK | RADIUS | POPULATION | STRING |
|------|--------|------------|--------|
| 0 | 0 | 0 | @city/city:rank0 |
| 1 | 150 | 10 | @city/city:rank1 |
| 2 | 200 | 15 | @city/city:rank2 |
| 3 | 300 | 25 | @city/city:rank3 |
| 4 | 400 | 35 | @city/city:rank4 |
| 5 | 500 | 45 | @city/city:rank5 |
| 6 | 600 | 55 | @city/city:rank6 |

**Update city.java:**
```java
public static final int RANK_MAX = 6; // Was 4
```

### Industrial Starport Structure

**New Structure Template:**
`dsrc/sku.0/sys.server/compiled/game/object/building/municipal/starport_industrial.tpf`

```
@base object/building/municipal/base/starport_base.iff

detailedDescription	= "string_id_table" "city/city" "starport_industrial_d"
lookAtText		= "string_id_table" "city/city" "starport_industrial_n"
objectName		= "string_id_table" "city/city" "starport_industrial_n"

// Structure requirements
scripts = ["systems/city/city_structure", "structure/municipal/starport_city"]

// Require Rank 6
// Costs more maintenance
// Supports interplanetary travel
```

**Shared Template:**
`dsrc/sku.0/sys.shared/compiled/game/object/building/municipal/shared_starport_industrial.tpf`

Uses larger starport appearance (imperial/rebel style commercial port).

### Script Changes

**Update starport_city.java:**
```java
public int setupStartport(obj_id self, dictionary params) throws InterruptedException {
    int city_id = getCityAtLocation(getLocation(self), 0);
    int city_rank = city.getCityRank(city_id);
    
    String template = getTemplateName(self);
    boolean isIndustrial = template.contains("starport_industrial");
    
    // Industrial starports enable interplanetary
    boolean interplanetary = isIndustrial && city_rank >= 6;
    
    String cityName = cityGetName(city_id);
    int travel_cost = isIndustrial ? 500 : 100; // Higher cost for industrial
    
    travel.initializeStarport(self, cityName, travel_cost, interplanetary);
    
    if (interplanetary) {
        city.addStarport(self, getLocation(self), travel_cost, true);
    }
    
    return SCRIPT_CONTINUE;
}
```

**New Structure Flag (city.java):**
```java
public static final int SF_INDUSTRIAL_STARPORT = 67108864; // 1 << 26
```

### Maintenance Costs

Add to `CITY_MAINTENANCE_COSTS` array:
```java
// Index 6 = Industrial Starport maintenance
public static final int[] CITY_MAINTENANCE_COSTS = {
    2500,   // City Hall
    7500,   // High cost civic
    2000,   // Medium cost civic
    150,    // Low cost civic
    1000,   // Small garden
    3000,   // Large garden
    15000   // Industrial Starport (new)
};
```

---

## Feature 5: City Expulsion Warning System

### Concept

When a mayor or authorized militia expels someone from the city:
1. Target receives immediate warning with 120-second countdown
2. If target doesn't leave within 120 seconds, they become attackable by militia
3. Attack state persists until target leaves city boundaries
4. **Militia attacks DO generate TEF** - while in TEF state, expelled players cannot enter buildings, use terminals, or interact with city services

### TEF (Temporary Enemy Flag) Restrictions

When an expelled player is attacked by militia and gains TEF:
- **Cannot enter buildings** (doors reject entry)
- **Cannot use terminals** (banks, bazaars, city terminals)
- **Cannot use vendors**
- **Cannot use shuttleports/starports**
- **Cannot use cloning facilities** (except on death)
- **TEF duration:** Standard 5 minutes, resets on each hostile action

This prevents exploits where expelled players could flee into buildings or use city services while under attack.

### Implementation

**New Objvars on Expelled Player:**
```java
"city.expulsion.city_id" - int
"city.expulsion.expel_time" - int (game time)
"city.expulsion.grace_expires" - int (game time + 120)
"city.expulsion.attackable" - int (0/1)
"city.expulsion.tef_active" - int (0/1) // NEW - tracks if player has TEF from militia
```

**city.java Additions:**
```java
public static final int EXPULSION_GRACE_PERIOD = 120; // seconds

public static boolean beginExpulsion(obj_id target, int cityId, obj_id initiator) 
    throws InterruptedException {
    
    // Validate initiator is mayor or militia
    if (!isTheCityMayor(initiator, cityId) && !isMilitiaOfCity(initiator, cityId)) {
        return false;
    }
    
    // Check target is in city
    if (!isInCityBounds(target, cityId)) {
        return false;
    }
    
    // Set expulsion tracking
    int curTime = getGameTime();
    setObjVar(target, "city.expulsion.city_id", cityId);
    setObjVar(target, "city.expulsion.expel_time", curTime);
    setObjVar(target, "city.expulsion.grace_expires", curTime + EXPULSION_GRACE_PERIOD);
    setObjVar(target, "city.expulsion.attackable", 0);
    
    // Attach handler script
    if (!hasScript(target, "systems.city.city_expulsion_handler")) {
        attachScript(target, "systems.city.city_expulsion_handler");
    }
    
    // Send warning message to target
    String cityName = cityGetName(cityId);
    prose_package pp = prose.getPackage(SID_EXPULSION_WARNING, cityName, 
                                        EXPULSION_GRACE_PERIOD);
    sendSystemMessage(target, pp);
    
    // Display countdown UI
    showExpulsionCountdown(target, EXPULSION_GRACE_PERIOD);
    
    // Schedule grace period expiration
    dictionary params = new dictionary();
    params.put("cityId", cityId);
    messageTo(target, "handleExpulsionGraceExpired", params, 
              EXPULSION_GRACE_PERIOD, false);
    
    return true;
}

public static boolean isExpelledFromCity(obj_id player, int cityId) 
    throws InterruptedException {
    
    int expelledCityId = getIntObjVar(player, "city.expulsion.city_id");
    if (expelledCityId != cityId) {
        return false;
    }
    
    return getIntObjVar(player, "city.expulsion.attackable") == 1;
}

public static boolean canMilitiaAttackExpelled(obj_id militia, obj_id target) 
    throws InterruptedException {
    
    // Get militia's city
    int cityId = getCitizenOfCityId(militia);
    if (cityId <= 0) {
        return false;
    }
    
    // Check militia has flag
    if (!hasMilitiaFlag(militia, cityId)) {
        return false;
    }
    
    // Check target is expelled and attackable from this city
    return isExpelledFromCity(target, cityId);
}
```

**New Script: `dsrc/sku.0/sys.server/compiled/game/script/systems/city/city_expulsion_handler.java`**
```java
package script.systems.city;

import script.*;
import script.library.*;

public class city_expulsion_handler extends script.base_script {
    
    public int handleExpulsionGraceExpired(obj_id self, dictionary params) 
        throws InterruptedException {
        
        int cityId = params.getInt("cityId");
        
        // Check still in city
        if (!city.isInCityBounds(self, cityId)) {
            clearExpulsion(self);
            return SCRIPT_CONTINUE;
        }
        
        // Make attackable
        setObjVar(self, "city.expulsion.attackable", 1);
        
        // Notify player
        sendSystemMessage(self, city.SID_NOW_ATTACKABLE_BY_MILITIA);
        
        // Visual indicator (red pulsing effect or similar)
        playClientEffectObj(self, "clienteffect/combat/target_indicator.cef", 
                           self, "root");
        
        return SCRIPT_CONTINUE;
    }
    
    public int OnHeartbeat(obj_id self, float time) throws InterruptedException {
        // Check if player has left city
        int cityId = getIntObjVar(self, "city.expulsion.city_id");
        
        if (!city.isInCityBounds(self, cityId)) {
            clearExpulsion(self);
        }
        
        return SCRIPT_CONTINUE;
    }
    
    private void clearExpulsion(obj_id self) throws InterruptedException {
        removeObjVar(self, "city.expulsion");
        detachScript(self, "systems.city.city_expulsion_handler");
        sendSystemMessage(self, city.SID_LEFT_CITY_EXPULSION_CLEARED);
    }
}
```

### PVP Integration

**TEF-Based Approach (Selected)**

Militia attacking expelled players generates standard TEF. The expelled player gains TEF restrictions that prevent building entry and terminal usage.

**Modify `combat.java` canAttack checks:**
```java
public static boolean canAttack(obj_id attacker, obj_id target) 
    throws InterruptedException {
    
    // ... existing checks ...
    
    // Check militia vs expelled player - allows attack, generates TEF
    if (city.canMilitiaAttackExpelled(attacker, target)) {
        return true;
    }
    
    return pvpCanAttack(attacker, target);
}
```

**Modify building entry checks (`player_structure.java`):**
```java
public int OnAboutToReceiveItem(obj_id self, obj_id srcContainer, 
                                obj_id transferer, obj_id item) 
    throws InterruptedException {
    
    // Check if player has active city expulsion TEF
    if (isPlayer(item) && hasCityExpulsionTEF(item)) {
        int cityId = city.getCityAtLocation(getLocation(self), 0);
        int expulsionCityId = getIntObjVar(item, "city.expulsion.city_id");
        
        if (cityId == expulsionCityId) {
            sendSystemMessage(item, city.SID_CANNOT_ENTER_BUILDING_TEF);
            return SCRIPT_OVERRIDE;
        }
    }
    
    return SCRIPT_CONTINUE;
}

public static boolean hasCityExpulsionTEF(obj_id player) throws InterruptedException {
    if (!hasObjVar(player, "city.expulsion.tef_active")) {
        return false;
    }
    return getIntObjVar(player, "city.expulsion.tef_active") == 1;
}
```

**Modify terminal usage checks (add to relevant terminal scripts):**
```java
// Check at start of terminal interaction
public int OnObjectMenuSelect(obj_id self, obj_id player, int item) 
    throws InterruptedException {
    
    // Block expelled players with TEF
    if (city.hasCityExpulsionTEF(player)) {
        int cityId = city.getCityAtLocation(getLocation(self), 0);
        int expulsionCityId = getIntObjVar(player, "city.expulsion.city_id");
        
        if (cityId == expulsionCityId) {
            sendSystemMessage(player, city.SID_CANNOT_USE_TERMINAL_TEF);
            return SCRIPT_OVERRIDE;
        }
    }
    
    // ... continue normal processing ...
}
```

**TEF Application on Attack:**
```java
// In combat processing when militia attacks expelled player
public void applyExpulsionTEF(obj_id attacker, obj_id target) 
    throws InterruptedException {
    
    if (city.canMilitiaAttackExpelled(attacker, target)) {
        // Set TEF flag on target
        setObjVar(target, "city.expulsion.tef_active", 1);
        
        // Standard TEF duration - 5 minutes
        int tefExpires = getGameTime() + (5 * 60);
        setObjVar(target, "city.expulsion.tef_expires", tefExpires);
        
        // Schedule TEF clear
        dictionary params = new dictionary();
        messageTo(target, "handleExpulsionTEFExpired", params, 5 * 60, false);
        
        // Apply visual indicator
        buff.applyBuff(target, "city_expulsion_tef");
        
        sendSystemMessage(target, city.SID_TEF_APPLIED_RESTRICTIONS);
    }
}
```

### UI Components

**Expulsion Countdown Display:**
- Use existing buff/debuff timer display system
- Or create floating text countdown like space combat timers
- Red border/tint on screen during warning period

**terminal_city.java Menu:**
```java
// For Mayor/Militia
mi.addSubMenu(menu, menu_info_types.SERVER_MENU23, SID_EXPEL_FROM_CITY);

// String table entries
public static final string_id SID_EXPEL_FROM_CITY = new string_id(STF, "expel_from_city");
public static final string_id SID_EXPULSION_WARNING = new string_id(STF, "expulsion_warning");
public static final string_id SID_NOW_ATTACKABLE_BY_MILITIA = 
    new string_id(STF, "now_attackable_by_militia");
public static final string_id SID_LEFT_CITY_EXPULSION_CLEARED = 
    new string_id(STF, "left_city_expulsion_cleared");
```

---

## Database Schema Summary

### New Tables

```sql
-- City Terrain Regions (radius painting and roads)
CREATE TABLE city_terrain_regions (
    city_id NUMBER NOT NULL,
    region_id VARCHAR2(64) NOT NULL,
    region_type VARCHAR2(16) NOT NULL, -- 'RADIUS' or 'ROAD'
    center_x NUMBER,
    center_z NUMBER,
    radius NUMBER,
    -- Road-specific fields
    start_x NUMBER,
    start_z NUMBER,
    end_x NUMBER,
    end_z NUMBER,
    road_width NUMBER,
    -- Common fields
    shader_template VARCHAR2(256),
    affector_type VARCHAR2(64),
    layer_data BLOB,
    created_time NUMBER,
    CONSTRAINT pk_city_terrain PRIMARY KEY (city_id, region_id)
);

-- City Bulldoze State (persisted terrain flattening)
CREATE TABLE city_bulldoze (
    city_id NUMBER PRIMARY KEY,
    bulldozed_height NUMBER NOT NULL,
    bulldozed_time NUMBER NOT NULL,
    edge_blend_distance NUMBER DEFAULT 20,
    CONSTRAINT fk_city_bulldoze FOREIGN KEY (city_id) 
        REFERENCES cities(city_id) ON DELETE CASCADE
);

-- City Evictions
CREATE TABLE city_evictions (
    eviction_id NUMBER PRIMARY KEY,
    city_id NUMBER NOT NULL,
    citizen_id NUMBER NOT NULL,
    initiated_by NUMBER NOT NULL,
    initiated_time NUMBER NOT NULL,
    reason VARCHAR2(512),
    status VARCHAR2(32),
    appeal_filed_time NUMBER,
    appeal_defense VARCHAR2(1024),
    judge_id NUMBER,
    judge_decision_time NUMBER,
    judge_notes VARCHAR2(512)
);

-- City Judges (ELECTED)
CREATE TABLE city_judges (
    city_id NUMBER NOT NULL,
    citizen_id NUMBER NOT NULL,
    elected_time NUMBER NOT NULL,
    term_expires NUMBER NOT NULL,
    votes_received NUMBER NOT NULL,
    CONSTRAINT pk_city_judges PRIMARY KEY (city_id, citizen_id)
);

-- Judge Election History
CREATE TABLE city_judge_elections (
    election_id NUMBER PRIMARY KEY,
    city_id NUMBER NOT NULL,
    start_time NUMBER NOT NULL,
    end_time NUMBER NOT NULL,
    total_votes NUMBER,
    status VARCHAR2(32)
);

-- City Extended Taxes (if not adding to existing CityInfo)
CREATE TABLE city_extended_taxes (
    city_id NUMBER PRIMARY KEY,
    crafting_tax NUMBER DEFAULT 0,
    vendor_license_fee NUMBER DEFAULT 0,
    structure_placement_fee NUMBER DEFAULT 0,
    event_permit_fee NUMBER DEFAULT 0,
    starship_landing_tax NUMBER DEFAULT 0  -- NEW
);
```

### Loader/Persister Updates

Update PL/SQL packages in:
- `dsrc/sku.0/sys.server/compiled/game/database/packages/loader.plsqlh`
- `dsrc/sku.0/sys.server/compiled/game/database/packages/loader.plsql`
- `dsrc/sku.0/sys.server/compiled/game/database/packages/persister.plsqlh`
- `dsrc/sku.0/sys.server/compiled/game/database/packages/persister.plsql`

---

## Network Messages Summary

### New Controller Messages

Add to `GameControllerMessage.def` (both client and server):

```cpp
// City Terrain Painting
CM_cityPaintTerrain,
CM_cityPaintRoad,
CM_cityRemovePaintedTerrain,
CM_cityTerrainSyncRequest,
CM_cityTerrainSyncResponse,

// City Bulldoze
CM_cityBulldozeTerrain,
CM_cityBulldozeSync,

// Extended Taxes
CM_citySetCraftingTax,
CM_citySetVendorLicenseFee,
CM_citySetStructurePlacementFee,
CM_citySetEventPermitFee,
CM_citySetStarshipLandingTax,

// Judge Elections
CM_cityStartJudgeElection,
CM_cityRegisterJudgeCandidate,
CM_cityCastJudgeVote,
CM_cityJudgeElectionResult,

// Expulsion
CM_cityBeginExpulsion,
CM_cityExpulsionUpdate,
CM_cityExpulsionTEFApplied,
```

---

## Testing Checklist

### Terrain Painting (Radius)
- [ ] Mayor can paint terrain within city bounds only
- [ ] Terrain updates sync to all clients in range
- [ ] Painted terrain persists across server restart
- [ ] Cannot paint terrain outside city radius
- [ ] Multiple painted regions supported
- [ ] Remove painted region works correctly

### Terrain Painting (Roads)
- [ ] Mayor can set first marker at current position
- [ ] Mayor can set second marker at current position
- [ ] Road paints between markers with correct width
- [ ] Road edges blend smoothly with surrounding terrain
- [ ] Roads persist across server restart
- [ ] Cannot paint roads outside city bounds
- [ ] Road shader selection works

### City Bulldoze
- [ ] Mayor can initiate bulldoze from terminal
- [ ] Confirmation dialog appears before execution
- [ ] Terrain flattens to specified/average height
- [ ] All structures adjust to new ground level
- [ ] Edge blending prevents terrain artifacts
- [ ] Bulldozed state persists across server restart
- [ ] Clients receive terrain update and render correctly

### Enhanced Taxation
- [ ] All new tax types save/load correctly
- [ ] Crafting tax applies on craft completion
- [ ] Vendor license fee collected weekly
- [ ] Structure placement fee deducted on place
- [ ] **Starship landing tax collected on autopilot landing**
- [ ] **Ship ejected outside city if cannot pay landing tax**
- [ ] Tax UI displays all options for mayor

### Eviction/Court System
- [ ] Mayor can initiate eviction
- [ ] Citizen receives warning mail
- [ ] Citizen can file appeal within grace period
- [ ] **Judge elections can be started**
- [ ] **Citizens can register as judge candidates**
- [ ] **Citizens can vote for judges**
- [ ] **Winners become judges after election ends**
- [ ] Judge can review and decide appeals
- [ ] Eviction completes after grace period if no appeal
- [ ] Reversed appeal clears eviction state

### Rank 6 Industrial Starport
- [ ] City can advance to Rank 5 and 6
- [ ] Industrial Starport placeable at Rank 6 only
- [ ] Interplanetary travel works from city
- [ ] Correct maintenance cost applied
- [ ] Structure appears on planetary map

### City Expulsion
- [ ] Expulsion warning shows countdown
- [ ] Player becomes attackable after 120s
- [ ] **Militia attacks generate TEF on expelled player**
- [ ] **TEF prevents building entry**
- [ ] **TEF prevents terminal usage**
- [ ] **TEF expires after 5 minutes of no combat**
- [ ] Leaving city clears expulsion state
- [ ] Non-militia cannot attack expelled players
- [ ] Expulsion clears on server restart (intentional)

---

## Implementation Priority

1. **Phase 1 (Foundation)**
   - Add Rank 5/6 to datatable
   - Update RANK_MAX constant
   - Add string table entries

2. **Phase 2 (Core Systems)**
   - City Expulsion Warning System with TEF restrictions
   - Enhanced Taxation (new tax types including Starship Landing Tax)
   - Judge Election System

3. **Phase 3 (Major Features)**
   - Rank 6 Industrial Starport
   - Eviction/Court System

4. **Phase 4 (Advanced Terrain)**
   - Terrain Radius Painting (persisted)
   - Terrain Road Painting (marker-to-marker)
   - City Bulldoze System

---

## File Change Summary

### Server (src/)
- `GameControllerMessage.def` - New controller messages
- `CityObject.h/cpp` - Extended tax fields, terrain storage, bulldoze state
- `CityInfo.h/cpp` - New tax fields including starship landing tax
- `ScriptMethodsCity.cpp` - New script methods for terrain, taxes, elections
- `CityTerrainManager.h/cpp` - NEW - Terrain painting, roads, bulldoze
- `Pvp.cpp` - Militia expulsion attack handling

### Scripts (dsrc/)
- `library/city.java` - RANK_MAX, new flags, expulsion functions, TEF checks
- `terminal/terminal_city.java` - New menu options, UI handlers
- `systems/city/city_court.java` - NEW - Court system
- `systems/city/city_judge_election.java` - NEW - Judge election system
- `systems/city/city_expulsion_handler.java` - NEW - Expulsion tracking with TEF
- `structure/municipal/starport_city.java` - Industrial starport support
- `space/atmo/atmo_landing_point.java` - Starship landing tax collection

### Client (client/)
- `GameControllerMessage.def` - Mirror server changes
- Expulsion countdown UI (buff system or new UI element)
- Terrain sync handlers

### Data
- `datatables/city/city_rank.tab` - Add Rank 5, 6
- `string/en/city/city.stf` - New string entries

### Database
- New tables (city_terrain_regions, city_bulldoze, city_evictions, city_judges, city_judge_elections, city_extended_taxes)
- Extended CityInfo persistence


