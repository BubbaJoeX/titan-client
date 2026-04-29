# Creating Interior Structures (Buildings and POB Ships)

This document outlines the complete steps required to create a new interior structure (building or POB ship) in the SWG codebase.

## Overview

Interior structures in SWG are composed of several key components:
1. **Portal Layout (.pob)** - Defines the 3D geometry, cells, portals (doorways), and collision
2. **Interior Layout (.ilf)** - Defines objects to spawn inside the structure
3. **Object Templates (.iff)** - Server and shared templates that reference the layouts
4. **Structure Footprint (.sfp)** - Defines placement restrictions on terrain

## File Types and Locations

| File Type | Extension | Location | Purpose |
|-----------|-----------|----------|---------|
| Portal Layout | .pob | appearance/ | 3D interior geometry and portals |
| Interior Layout | .ilf | interiorlayout/ | Objects to spawn in cells |
| Shared Template | .iff | object/building/... or object/ship/... | Client-side object definition |
| Server Template | .iff | object/building/... or object/ship/... | Server-side object definition |
| Structure Footprint | .sfp | footprint/ | Terrain placement rules |
| Appearance | .apt/.msh | appearance/ | Exterior mesh/appearance |

## Step-by-Step Creation Process

### Step 1: Create the Portal Layout (.pob)

The Portal Layout defines the interior geometry of the structure. This file is typically created using 3D modeling tools and exported.

**Maya (SwgMayaEditor):** [MAYA_POB_FROM_SCRATCH.md](./MAYA_POB_FROM_SCRATCH.md) walks through **File → Import / Export Selection**, Outliner, and Attribute Editor for `.pob` layout (optional MEL presets at the end). **Kitbash / combine** imported `.sat`, `.apt`, `.msh`: [MAYA_KITBASH_IMPORT_COMBINE.md](./MAYA_KITBASH_IMPORT_COMBINE.md). Command-level detail: `client/src/engine/client/application/MayaModern/how-to.md`.

**Structure:**
- **Cells**: Named interior spaces (rooms)
- **Portals**: Doorways connecting cells
- **Collision**: Floor and wall collision meshes
- **Lights**: Interior lighting definitions

**Key Concepts:**
- Cell index 0 is always the exterior
- Each cell has a unique name (e.g., "room1", "hallway", "cockpit")
- Portals connect cells and define passability

**Location:** `appearance/[structure_name].pob`

**Example Portal Layout Cell Structure:**
```
Cell 0: exterior (outside the building)
Cell 1: main_room
Cell 2: back_room
Portals:
  - Portal 0: connects cell 0 to cell 1 (front door)
  - Portal 1: connects cell 1 to cell 2 (interior door)
```

### Step 2: Create the Interior Layout (.ilf)

The Interior Layout defines what objects spawn inside each cell.

**File Structure:**
```
FORM INLY
  FORM NODE (for each cell)
    - cellName (string)
    - objects[] (template path + transform)
```

**Location:** `interiorlayout/[structure_name].ilf` or `interiorlayout/space/[ship_name].ilf`

**Contents:**
- Cell name references (must match .pob cell names)
- Object template paths
- Transform data (position/rotation relative to cell)

**Example Objects to Place:**
- Furniture (chairs, tables, beds)
- Terminals (banks, bazaars, mission terminals)
- Decorations (lights, paintings)
- NPCs or spawn points

### Step 3: Create the Shared Object Template (.tpf → .iff)

**Template File (.tpf):**

For a building:
```
@base object/building/base/shared_base_building.iff

@class building_object_template 1
interiorLayoutFileName = "interiorlayout/[your_building].ilf"

@class tangible_object_template 5
structureFootprintFileName = "footprint/building/[your_building].sfp"

@class object_template 6
objectName = "building_name" "[string_id]"
detailedDescription = "building_detail" "[string_id]"
lookAtText = "building_lookat" "[string_id]"
portalLayoutFilename = "appearance/[your_building].pob"
appearanceFilename = ""  // exterior appearance if needed
clearFloraRadius = [radius in meters]
snapToTerrain = true
```

For a POB ship:
```
@base object/ship/base/shared_ship_base.iff

@class ship_object_template 0
interiorLayoutFileName = "interiorlayout/space/[your_ship].ilf"

@class tangible_object_template 5

@class object_template 6
objectName = "ship_name" "[string_id]"
portalLayoutFilename = "appearance/[your_ship].pob"
gameObjectType = GOT_ship
```

**Location:** 
- Buildings: `dsrc/sku.0/sys.shared/compiled/game/object/building/[category]/shared_[name].tpf`
- Ships: `dsrc/sku.0/sys.shared/compiled/game/object/ship/[category]/shared_[name].tpf`

### Step 4: Create the Server Object Template (.tpf → .iff)

**Template File (.tpf):**

For a building:
```
@base object/building/base/base_building.iff

@class building_object_template 1
// Building-specific properties

@class tangible_object_template 5
maxHitPoints = [hp]
armor = AR_armorNone

@class object_template 6
sharedTemplate = "object/building/[category]/shared_[name].iff"
scripts = ["systems.building.building_script"]
```

For a POB ship:
```
@base object/ship/base/base_ship.iff

@class ship_object_template 0
// Ship-specific properties

@class tangible_object_template 5
maxHitPoints = [hp]

@class object_template 6
sharedTemplate = "object/ship/[category]/shared_[name].iff"
scripts = ["space.ship.ship_interior"]
```

**Location:**
- Buildings: `dsrc/sku.0/sys.server/compiled/game/object/building/[category]/[name].tpf`
- Ships: `dsrc/sku.0/sys.server/compiled/game/object/ship/[category]/[name].tpf`

### Step 5: Create the Structure Footprint (.sfp)

For player-placeable structures, defines:
- Lot size requirements
- Build area shape
- Terrain restrictions

**Location:** `footprint/building/[category]/[structure_name].sfp`

### Step 6: Compile Templates

Run the template compiler to convert .tpf files to .iff:

```bash
# From dsrc directory
ant compile_tpf
```

Or manually:
```bash
TemplateCompiler -compile [input.tpf] -o [output.iff]
```

### Step 7: Add String IDs (Localization)

Add entries to string tables:
- `string/en/building_name.stf` - Display names
- `string/en/building_detail.stf` - Detailed descriptions
- `string/en/building_lookat.stf` - Examine text

## Portal System Deep Dive

### Cell Properties

Each cell in a portal layout has:
- **Name**: Unique identifier
- **Appearance**: Visual mesh for the cell interior
- **Floor**: Walkable surface mesh
- **Collision Extent**: Bounding volume
- **Portals**: Doorways to other cells

### Portal Properties

Each portal has:
- **Target Cell**: Which cell it connects to
- **Geometry**: The doorway shape
- **Passable**: Whether creatures can traverse
- **Door**: Optional door object template

### Code References

Key classes for portals:
- `PortalProperty` - Runtime portal container on objects
- `PortalPropertyTemplate` - Static portal data loaded from .pob
- `CellProperty` - Individual cell data
- `CellObject` - Server-side cell object

## Interior Layout System

### InteriorLayoutReaderWriter

The `InteriorLayoutReaderWriter` class loads .ilf files and provides:
- List of cell names
- Objects per cell
- Transform data for each object

### Object Spawning

When a portal structure loads:
1. Server creates cell objects for each cell in .pob
2. `PortalProperty::serverEndBaselines()` processes the structure
3. Interior layout objects spawn in their designated cells
4. Scripts attach to the building/ship

## Special Considerations

### Buildings vs POB Ships

| Feature | Building | POB Ship |
|---------|----------|----------|
| Base Template | shared_base_building.iff | shared_ship_base.iff |
| Placement | Terrain | Space/Terrain |
| Interior Layout | interiorlayout/*.ilf | interiorlayout/space/*.ilf |
| Movement | Static | Can move in space |
| Cell Persistence | Always | Persists in world |

### Ejection Points

Buildings define ejection transforms for when players are ejected:
- Configured in `datatables/buildout/pob_ejection_points.tab`
- Maps POB names to safe ejection coordinates

### Path Graph

Large buildings need pathfinding data:
- Built automatically from floor meshes
- Enables NPC navigation inside buildings

## GodClient Interior Editing

The GodClient provides tools for editing interiors:

### Object Placement (`ActionsEdit.cpp`)

1. **Creating Objects in Cells**:
   - Select a cell by entering the building interior
   - Use Ctrl+N to create from selected template
   - Objects spawn at cursor intersection point
   - Cell context is determined from player location or paste location

2. **Moving Objects in Cells**:
   - Select object(s)
   - Drag to create ghost(s)
   - Press Space or Apply Transform to confirm
   - Transform is sent with parent cell ID

3. **Pasting Objects**:
   - Copy selection (Ctrl+C)
   - Move to new cell
   - Paste (Ctrl+V) - uses player's current cell

### Transform Handling

Interior objects use `sendTransformUsingParent()` which sends:
- Transform relative to parent cell
- Parent cell NetworkId
- Proper containment for persistence

### Server Commands Used

```
object createTranslateRotate <template> <x> <y> <z> <qw> <qx> <qy> <qz>
object cellCreateTranslateRotate <template> <cellId> <x> <y> <z> <qw> <qx> <qy> <qz>
object moveTranslateRotate <objId> <x> <y> <z> <qw> <qx> <qy> <qz>
object cellMoveTranslateRotate <objId> <cellId> <x> <y> <z> <qw> <qx> <qy> <qz>
```

### Single Player Mode

In single player mode, GodClient:
- Creates client-only objects locally
- Handles portal transitions with `CellProperty::setPortalTransitionsEnabled(false)`
- Initializes portal property with `clientSinglePlayerInitializeFirstTimeObject()`

### Known Issues and Solutions

**Issue: Translating objects crashes**
- Cause: Cell context lost during transform
- Solution: Ensure `setTransform_o2p()` is called with portal transitions disabled:
```cpp
CellProperty::setPortalTransitionsEnabled(false);
obj->setTransform_o2p(transform);
CellProperty::setPortalTransitionsEnabled(true);
```

**Issue: Objects spawn outside cell**
- Cause: Using world cell instead of interior cell
- Solution: Use player's parent cell when creating in interiors:
```cpp
GroundScene const * const gs = dynamic_cast<GroundScene const *>(Game::getScene());
if (gs && gs->getPlayer())
    cellProperty = gs->getPlayer()->getParentCell();
```

**Issue: Object doesn't appear in correct cell after restart**
- Cause: Transform sent without parent cell info
- Solution: Use `cellCreateTranslateRotate` or `cellMoveTranslateRotate` commands

### Buildout Mode

For buildout editing (single player):
- Uses `BuildoutAreaSupport` class
- Objects have NetworkId < cms_invalid
- Transforms stored in buildout files
- Use unlock actions to enable editing of buildout objects

## Complete File Checklist

For a new interior building named "my_building":

```
□ appearance/my_building.pob                     (portal layout)
□ interiorlayout/my_building.ilf                 (interior objects)
□ footprint/building/player/my_building.sfp      (if player-placeable)

□ dsrc/.../object/building/.../shared_my_building.tpf  (shared template source)
□ dsrc/.../object/building/.../my_building.tpf         (server template source)

□ data/.../object/building/.../shared_my_building.iff  (compiled shared)
□ data/.../object/building/.../my_building.iff         (compiled server)

□ string/en/building_name.stf                    (add entry)
□ string/en/building_detail.stf                  (add entry)
```

## Troubleshooting

### "Cell not found"
- Ensure .ilf cell names match .pob cell names exactly
- Cell names are case-sensitive

### Objects spawn at wrong position
- Check transform data in .ilf
- Transforms are relative to cell origin

### Portal not passable
- Check portal passability flags in .pob
- Verify door objects if any

### Building has no interior
- Verify portalLayoutFilename in template
- Check .pob file exists in TRE/filesystem

### Objects fall through floor
- Floor mesh may be missing or incorrect
- Check collision extent in .pob cell


