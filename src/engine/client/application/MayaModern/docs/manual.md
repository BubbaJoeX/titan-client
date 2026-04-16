# SwgMayaEditor — Manual

**Maya 2026 Plugin for Star Wars Galaxies Asset Authoring**

Version 1.0 — Full round-trip support.

Documentation map: [README.md](README.md) · Command-focused guide: [guide.md](guide.md)

---

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [File Type Reference](#file-type-reference)
5. [Scene Preparation](#scene-preparation)
6. [Import Workflows](#import-workflows)
7. [Export Workflows](#export-workflows)
8. [MEL Command Reference](#mel-command-reference)
9. [Configuration](#configuration)
10. [Coordinate System](#coordinate-system)
11. [Limitations and Known Issues](#limitations-and-known-issues)
12. [Troubleshooting](#troubleshooting)

---

## Overview

SwgMayaEditor is a Maya 2026 plugin that enables complete round-trip editing of Star Wars Galaxies game assets. It supports importing, editing, and exporting all major SWG file formats including meshes, skeletons, animations, skeletal appearances, portal objects (buildings), and shaders.

### What It Does

- **Import** SWG binary files (IFF format) into Maya scenes with proper hierarchy, materials, and metadata
- **Export** Maya scenes back to SWG binary format with game-compatible coordinate systems
- **Convert** textures between DDS (game format) and TGA (Maya-editable format)
- **Preserve** game-specific metadata (hardpoints, floor references, portal data, skin weights)
- **Validate** exported data for game compatibility

### What It Does Not Do

- **Compile** shader source code (uses existing `.sht` templates)
- **Generate** collision meshes automatically (must be authored separately)
- **Create** new skeleton rigs from scratch (import existing `.skt` first, then modify)
- **Export** floor files (`.flr`) — import only
- **Export** DDS textures directly — uses NVIDIA Texture Tools for TGA→DDS conversion
- **Handle** encrypted or compressed game archives — work with extracted files only

---

## Installation

### Requirements

- **Maya 2026** (64-bit)
- **Maya 2026 Devkit** (separate download from Autodesk)
- **Visual Studio 2022** (17.8.3+) for building
- **CMake** 3.13+
- **NVIDIA Texture Tools** (optional, for shader texture export)

### Building from Source

```batch
cd D:\titan\client\src\engine\client\application\MayaModern

# Configure with Visual Studio 2022
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build Release
cmake --build build --config Release
```

Output: `build/Release/SwgMayaEditor.mll` plus MEL scripts.

### Installing the Plugin

**Option A: User plugins folder**

```
Copy SwgMayaEditor.mll and *.mel to:
C:\Users\<username>\Documents\maya\2026\plug-ins\
```

**Option B: System plugins folder**

```
Copy to: C:\Program Files\Autodesk\Maya2026\bin\plug-ins\
(Requires admin rights)
```

**Option C: Environment variable**

```batch
set MAYA_PLUG_IN_PATH=D:\titan\client\src\engine\client\application\MayaModern\build\Release
```

### Loading in Maya

```mel
loadPlugin "SwgMayaEditor";
```

Or use Window > Settings/Preferences > Plug-in Manager and check "Loaded" for SwgMayaEditor.mll.

---

## Quick Start

### Basic Setup

```mel
// Load the plugin
loadPlugin "SwgMayaEditor";

// Set your data root (where extracted game files live)
// This is required for path resolution
setBaseDir "D:/swg_data";

// Or set environment variable before launching Maya:
// set TITAN_DATA_ROOT=D:/swg_data
```

### Import a Character

```mel
// Import a skeletal appearance (character/creature)
importSat -i "appearance/mesh/appearance_data/humanoid_male/humanoid_male.sat";

// Browse and apply animations
swgAnimationBrowser;
```

### Import a Static Object

```mel
// Import a static mesh
importLodMesh -i "appearance/mesh/thm_all_furn_chair_s01";

// Or use File > Import and select the .msh/.apt/.lod file
```

### Export After Editing

```mel
// Select the mesh and export
select -r myMesh;
exportStaticMesh;

// Or use File > Export Selection with appropriate file type
```

---

## File Type Reference

### Complete Support Matrix


| Extension | Description                  | Import | Export | MEL Type |
| --------- | ---------------------------- | ------ | ------ | -------- |
| `.msh`    | Static mesh geometry         | ✅      | ✅      | `SwgMsh` |
| `.apt`    | Appearance redirect          | ✅      | ✅      | `SwgMsh` |
| `.mgn`    | Skeletal mesh (skinned)      | ✅      | ✅      | `SwgMgn` |
| `.skt`    | Skeleton template            | ✅      | ✅      | `SwgSkt` |
| `.ans`    | Keyframe animation           | ✅      | ✅      | `SwgAns` |
| `.sat`    | Skeletal appearance template | ✅      | ✅      | `SwgSat` |
| `.pob`    | Portal object (building)     | ✅      | ✅      | `SwgPob` |
| `.lod`    | LOD mesh container           | ✅      | ✅      | `SwgLod` |
| `.lmg`    | Skeletal LOD container       | ✅      | ✅      | `SwgLmg` |
| `.sht`    | Shader template              | ✅      | ✅      | Command  |
| `.flr`    | Floor collision              | ✅      | ❌      | `SwgFlr` |
| `.dds`    | DDS texture                  | ✅      | ❌      | `SwgDds` |


### File Format Details

#### Static Mesh (.msh)

- **Format**: MESH/0005 IFF
- **Contains**: Vertex positions, normals, UVs, shader groups, hardpoints, floor reference
- **Import creates**: Maya mesh with materials, locators for hardpoints
- **Export requires**: Selected mesh with proper UV mapping

#### Skeletal Mesh (.mgn)

- **Format**: SKMG/0004 IFF
- **Contains**: Vertices, normals, UVs, skin weights, blend shapes, per-shader data
- **Import creates**: Skinned mesh bound to skeleton with blend shape deformers
- **Export requires**: Mesh with skin cluster bound to joints, max 4 influences per vertex

#### Skeleton (.skt)

- **Format**: SKTM IFF
- **Contains**: Joint hierarchy, bind pose transforms
- **Import creates**: Maya joint hierarchy
- **Export requires**: Selected joint or joint hierarchy

#### Animation (.ans)

- **Format**: KFAT (uncompressed) or CKAT (compressed) IFF
- **Contains**: Per-joint rotation and translation keyframes as deltas from bind pose
- **Import creates**: Animation curves on joints
- **Export requires**: Animated joints, exports current timeline range

#### Skeletal Appearance (.sat)

- **Format**: SMAT IFF
- **Contains**: Skeleton reference, mesh LOD references, scale
- **Import creates**: Full character with skeleton and skinned meshes
- **Export requires**: Hierarchy with skeleton and mesh references

#### Portal Object (.pob)

- **Format**: PRTO IFF
- **Contains**: Cells, portals, appearance references, collision data
- **Import creates**: Hierarchical cell structure with portal geometry
- **Export requires**: Properly structured POB hierarchy (see POB Authoring)

---

## Scene Preparation

### General Guidelines

1. **Work in centimeters** — Maya's default unit matches SWG scale
2. **Use Y-up orientation** — SWG uses Y-up coordinate system
3. **Keep hierarchy clean** — Use proper naming conventions
4. **Freeze transforms** before export when appropriate
5. **Delete history** on meshes before export to avoid issues

### Preparing Static Meshes for Export

#### Mesh Requirements

```
✓ Single mesh object (combine if multiple)
✓ Triangulated faces (quads will be triangulated on export)
✓ UV coordinates on UV set "map1"
✓ Normals (hard/soft edges as needed)
✓ No n-gons (faces with more than 4 vertices)
✓ No non-manifold geometry
✓ Materials assigned (Lambert or Phong)
```

#### Hardpoints

Hardpoints are attachment points for effects, weapons, etc. Create them as locators:

```mel
// Create a hardpoint
spaceLocator -name "hp_weapon_right";
parent hp_weapon_right myMesh;

// Position and orient the locator
// The locator's transform becomes the hardpoint transform
```

**Naming convention**: Hardpoints should start with `hp_` prefix.

#### Floor Reference

If your mesh needs floor collision data:

```mel
// Add floor reference attribute to mesh transform
addAttr -ln "floorPath" -dt "string" myMeshTransform;
setAttr -type "string" myMeshTransform.floorPath "appearance/collision/my_floor.flr";
```

#### Collision Geometry

On import, a `collision` group is automatically created under the mesh root. To add collision geometry:

1. Create your collision primitives (boxes, spheres, or mesh)
2. Middle-mouse drag them into the `collision` group
3. Set the collision type attribute:

```mel
// Set collision type (box, sphere, mesh, composite)
setAttr -type "string" collision.swgCollisionType "box";
```

### Preparing Skeletal Meshes for Export

#### Skeleton Requirements

```
✓ Joint hierarchy with proper naming
✓ Bind pose at frame 0 (or specified bind pose frame)
✓ Joint orient values set correctly
✓ No scale on joints (scale = 1,1,1)
```

#### Skinning Requirements

```
✓ Smooth skin binding to skeleton
✓ Maximum 4 influences per vertex
✓ Normalized weights (sum to 1.0)
✓ No unused influences
```

#### Checking Skin Weights

```mel
// Select skinned mesh
select -r mySkinnedMesh;

// Check max influences
skinCluster -q -mi skinCluster1;

// Prune small weights
skinPercent -pruneWeights 0.01 skinCluster1;

// Normalize weights
skinCluster -e -normalizeWeights 1 skinCluster1;
```

#### Hardpoints on Skeletal Meshes

Create hardpoints as locators parented to the mesh transform:

```mel
// Create a hardpoint
spaceLocator -name "hp_weapon_right";
parent hp_weapon_right mySkinnedMeshTransform;

// Set parent joint (which joint the hardpoint follows)
addAttr -ln "swgHardpointParent" -dt "string" hp_weapon_right;
setAttr -type "string" hp_weapon_right.swgHardpointParent "r_hand";

// Mark as dynamic (morphable) hardpoint if needed
addAttr -ln "swgHardpointDynamic" -at bool -dv 0 hp_weapon_right;
```

#### Occlusion Zones

Store occlusion zone data on the mesh transform:

```mel
// Set occlusion zones (tab-separated names)
addAttr -ln "swgOcclusionZones" -dt "string" myMeshTransform;
setAttr -type "string" myMeshTransform.swgOcclusionZones "chest\tback\tshoulder_l";

// Set occlusion layer
addAttr -ln "swgOcclusionLayer" -at long myMeshTransform;
setAttr myMeshTransform.swgOcclusionLayer 1;
```

#### Blend Shapes

Maya blend shapes are automatically exported. Create them normally:

```mel
// Create blend shape deformer
blendShape -name "morphTargets" targetMesh1 targetMesh2 baseMesh;

// The exporter will capture position deltas for each target
```

### Preparing Animations for Export

#### Animation Requirements

```
✓ Animation on joints only (not transforms)
✓ Keyframes on rotate and translate channels
✓ Timeline range set correctly
✓ Bind pose established at start frame
```

#### Animation Workflow

1. **Import skeleton** or SAT first
2. **Set timeline** to your animation range
3. **Animate joints** using rotation and translation
4. **Export** — the exporter captures the current timeline range

```mel
// Set timeline range
playbackOptions -min 0 -max 60 -ast 0 -aet 60;

// Go to bind pose before animating
swgRevertToBindPose;

// ... animate ...

// Export animation
// File > Export Selection > SwgAns
```

#### Locomotion Data

To export locomotion (root motion) data, create a "master" transform above the skeleton root:

```mel
// Create master node
group -name "master" rootJoint;

// Enable locomotion export
addAttr -ln "swgLocomotion" -at bool -dv 1 master;

// Animate the master node for root motion
// The exporter will capture translation and rotation
```

#### Animation Messages

Animation messages (events) are exported from attributes on joints:

```mel
// Add a message that fires at frames 10 and 30
addAttr -ln "swgAnimMsg_footstep" -dt "string" rootJoint;
setAttr -type "string" rootJoint.swgAnimMsg_footstep "10,30";

// Add another message
addAttr -ln "swgAnimMsg_sound_whoosh" -dt "string" rootJoint;
setAttr -type "string" rootJoint.swgAnimMsg_sound_whoosh "15";
```

### Preparing Portal Objects (Buildings)

POB files require a specific hierarchy structure:

```
pobRoot (transform)
├── r0 (cell - usually exterior/entry)
│   ├── mesh (transform with external_reference attribute)
│   ├── portals (transform containing portal geometry)
│   │   └── portal_0 (mesh - quad defining portal opening)
│   ├── collision (transform with collision data)
│   └── floor0 (transform with floor reference)
├── r1 (cell)
│   ├── mesh
│   ├── portals
│   │   └── portal_0
│   └── floor0
└── ... more cells
```

#### Creating a POB from Scratch

```mel
// Source the POB authoring tools
source "pobAuthoring.mel";

// Create a template with 4 cells
createPobTemplate -n "myBuilding" -cells 4 -layoutSpacing 10;

// Layout cells in a grid
layoutPobCells -root "myBuilding" -cols 2 -dx 10 -dz 10;

// Connect cells with portals
connectPobCells -from "r0" -to "r1";
connectPobCells -from "r1" -to "r2";

// Set mesh references
// (Add external_reference attribute to each cell's mesh transform)

// Validate before export
validatePob -root "myBuilding";
```

---

## Import Workflows

### Importing via MEL Commands

#### Static Mesh

```mel
// Basic import
importLodMesh -i "appearance/mesh/object_name";

// With parent transform
importLodMesh -i "appearance/mesh/object_name" -parent "|myGroup";

// The command tries .lod, .apt, .msh extensions automatically
```

#### Skeletal Mesh

```mel
// Requires skeleton path
importSkeletalMesh -i "appearance/mesh/character_lod0" -s "appearance/skeleton/humanoid/humanoid";
```

**What gets imported**:

- Mesh geometry with skin weights
- Shader/material assignments
- **Blend Shapes**: Created as a Maya `blendShape` deformer named `swgBlendShapes`. Target meshes are hidden but connected. Access via:
  ```mel
  // List blend shape targets
  blendShape -q -t meshName;

  // Adjust a blend target weight (0-1)
  blendShape -e -w 0 0.5 swgBlendShapes;
  ```
- **Occlusion Zones**: Stored as `swgOcclusionZones` attribute on the mesh's parent transform. Zone names are logged to the Script Editor on import:
  ```mel
  // View occlusion zones
  getAttr parentTransform.swgOcclusionZones;
  // Returns tab-separated zone names like: "head\tneck\tchest\t..."
  ```
- **Hardpoints**: Created as locators parented to joints, named `hp_<name>`. Check attributes:
  ```mel
  // List hardpoints
  ls -type locator "hp_*";

  // Check hardpoint parent joint
  getAttr hp_weapon_right.swgHardpointParent;
  ```
- **DOT3 Count**: Stored as `swgDot3Count` attribute for reference during export.

#### Skeleton

```mel
importSkeleton -i "appearance/skeleton/humanoid/humanoid";
```

#### Animation

```mel
// Import animation onto existing skeleton
importAnimation -i "appearance/animation/humanoid/combat_idle.ans";
```

#### Skeletal Appearance (Full Character)

```mel
// Imports skeleton + all mesh LODs
importSat -i "appearance/mesh/appearance_data/humanoid_male/humanoid_male.sat";
```

#### Portal Object

```mel
importPob -i "appearance/building/cantina";
```

#### Shader

```mel
importShader -i "shader/defaultappearance";
```

### Importing via File Menu

1. **File > Import**
2. Select file type from "Files of type" dropdown:
  - `SWG skeletal mesh (*.mgn)`
  - `SWG static mesh (*.msh *.apt)`
  - `SWG skeleton (*.skt)`
  - `SWG animation (*.ans)`
  - `SWG skeletal appearance (*.sat)`
  - `SWG portal object (*.pob)`
  - `SWG floor (*.flr)`
  - `SWG LOD container (*.lod)`
  - `SWG skeletal LOD (*.lmg)`
3. Navigate to file and click Import

**Important**: Do NOT use "SAT_ATF" for SWG .sat files — that's Maya's ACIS solid format. Use "SwgSat" instead.

### Animation Browser

The animation browser provides a UI for browsing and importing animations:

```mel
swgAnimationBrowser;
```

Features:

- Category dropdown (top-level folders under `appearance/animation/`)
- Filter text field (substring match)
- Animation list with full paths
- Double-click or "Import selected" to apply animation

**Workflow**:

1. Import a `.sat` file first (so joints exist)
2. Open animation browser
3. Select category or type filter text
4. Click Refresh
5. Double-click animation to import

---

## Export Workflows

### Exporting Static Meshes

```mel
// Select mesh
select -r myMesh;

// Export with automatic naming
exportStaticMesh;

// Export to specific path
exportStaticMesh -path "D:/output/appearance/mesh/myMesh.msh";

// Export with custom name
exportStaticMesh -name "custom_name";
```

**What gets exported**:

- Mesh geometry (vertices, normals, UVs)
- Shader assignments (creates .sht files)
- Hardpoints (child locators)
- Floor reference (if attribute exists)
- APT redirect file

### Exporting Skeletal Meshes

```mel
// Select skinned mesh
select -r mySkinnedMesh;

// File > Export Selection
// Choose "SWG skeletal mesh (*.mgn)"
// Enter filename
```

**What gets exported**:

- Vertex positions
- Normals
- UV coordinates
- Skin weights (max 4 per vertex)
- Per-shader triangle data
- Transform (joint) names

### Exporting Skeletons

```mel
// Select root joint
select -r root_joint;

// Export
exportSkeleton;

// With specific bind pose frame
exportSkeleton -bp -10;

// To specific path
exportSkeleton -path "D:/output/appearance/skeleton/custom.skt";
```

### Exporting Animations

```mel
// Ensure timeline is set to animation range
playbackOptions -min 0 -max 60;

// Select any joint (or none - exports all joints)
// File > Export Selection
// Choose "SWG animation (*.ans)"
```

**What gets exported**:

- Rotation keyframes as quaternion deltas from bind pose
- Translation keyframes as deltas from bind pose
- Frame rate from Maya's time settings
- All joints in scene

### Exporting Skeletal Appearances

```mel
// Select SAT root transform
select -r mySatRoot;

// Export
ExportSat -path "D:/output/appearance/mesh/appearance_data/my_character.sat";
```

### Exporting Portal Objects

```mel
// Select POB root
select -r myPobRoot;

// Export
exportPob -i "appearance/building/my_building";
```

### Exporting Shaders

```mel
// Export shader with texture conversion
exportShader -i "shader/my_shader";
```

This:

1. Reads the source .sht file
2. Converts any edited TGA textures to DDS
3. Writes updated .sht with new texture paths

---

## MEL Command Reference

### Setup Commands


| Command             | Description                                |
| ------------------- | ------------------------------------------ |
| `setBaseDir "path"` | Set base directory for all imports/exports |
| `getDataRootDir`    | Returns current data root path             |


### Import Commands


| Command              | Flags                               | Description                             |
| -------------------- | ----------------------------------- | --------------------------------------- |
| `importSkeleton`     | `-i path [-parent dag]`             | Import skeleton template                |
| `importAnimation`    | `-i path`                           | Import animation onto existing skeleton |
| `importLodMesh`      | `-i path [-parent dag]`             | Import static mesh/LOD/APT              |
| `importSkeletalMesh` | `-i path -s skeleton [-parent dag]` | Import skinned mesh                     |
| `importStaticMesh`   | `-i path [-parent dag]`             | Alias for importLodMesh                 |
| `importShader`       | `-i path`                           | Import shader template                  |
| `importSat`          | `-i path`                           | Import skeletal appearance              |
| `importPob`          | `-i path`                           | Import portal object                    |
| `importStructure`    | `-i path [-flr] [-shader path]`     | Import POB + floor + shell mesh         |


### Export Commands


| Command            | Flags                       | Description                 |
| ------------------ | --------------------------- | --------------------------- |
| `exportSkeleton`   | `[-bp frame] [-path path]`  | Export selected skeleton    |
| `exportStaticMesh` | `[-path path] [-name name]` | Export selected mesh        |
| `exportPob`        | `-i path`                   | Export selected POB         |
| `exportShader`     | `-i path`                   | Export shader with textures |
| `ExportSat`        | `-path path`                | Export skeletal appearance  |


### Utility Commands


| Command               | Description                                  |
| --------------------- | -------------------------------------------- |
| `swgRevertToBindPose` | Reset skeleton to bind pose, clear animation |
| `swgAnimationBrowser` | Open animation browser UI                    |
| `swgAssetDissector`   | Open asset dissector UI                      |


### POB Authoring Commands


| Command                                    | Description                 |
| ------------------------------------------ | --------------------------- |
| `createPobTemplate -n name -cells N`       | Create empty POB structure  |
| `layoutPobCells [-cols N] [-dx F] [-dz F]` | Arrange cells in grid       |
| `addPobPortal [-preset N] [-w F] [-h F]`   | Add portal to selected cell |
| `connectPobCells -from cell -to cell`      | Create portal connection    |
| `duplicatePobCell [-stripPortals]`         | Duplicate selected cell     |
| `validatePob [-root name]`                 | Validate POB structure      |
| `reportPobPortals`                         | List portal indices         |


---

## Configuration

### Configuration File

Create `SwgMayaEditor.cfg` in the plugin directory:

```ini
[SwgMayaEditor]
; Path to NVIDIA Texture Tools for TGA→DDS conversion
nvttExporterPath = "C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools\nvtt_export.exe"

; Override default directories
appearanceWriteDir = "D:\exported\appearance\"
shaderTemplateWriteDir = "D:\exported\shader\"
textureWriteDir = "D:\exported\texture\"
skeletonTemplateWriteDir = "D:\exported\appearance\skeleton\"

; Enable verbose logging
verboseLogging = false
```

### Environment Variables


| Variable            | Description                                 |
| ------------------- | ------------------------------------------- |
| `TITAN_DATA_ROOT`   | Primary data root for imports (recommended) |
| `TITAN_EXPORT_ROOT` | Alternative data root                       |
| `DATA_ROOT`         | Fallback data root                          |
| `MAYA_PLUG_IN_PATH` | Additional plugin search paths              |


### Path Resolution

The plugin resolves paths in this order:

1. Absolute paths used as-is
2. `TITAN_DATA_ROOT` environment variable
3. `TITAN_EXPORT_ROOT` environment variable
4. `DATA_ROOT` environment variable
5. `setBaseDir` configured path

Tree paths like `appearance/mesh/object` are resolved relative to the data root.

---

## Coordinate System

### SWG vs Maya Coordinate Conversion

SWG and Maya both use Y-up coordinate systems, but with some differences:


| Axis | Maya        | SWG Game               |
| ---- | ----------- | ---------------------- |
| X    | Right (+)   | Left (+) — **negated** |
| Y    | Up (+)      | Up (+)                 |
| Z    | Forward (+) | Forward (+)            |


### Automatic Conversions

The plugin automatically handles these conversions:

**Positions/Translations**:

- X coordinate is negated on import/export

**Rotations**:

- Y and Z Euler angles are negated
- Quaternions are converted accordingly

**Normals**:

- X component is negated

### Manual Considerations

When working in Maya:

- Model facing +Z (Maya forward)
- Right side of model is +X (Maya right)
- The export will flip X for game compatibility

---

## Limitations and Known Issues

### Animation Export

- **Format**: Exports KFAT (uncompressed) only, not CKAT (compressed)
- **Keyframes**: Exports every frame in timeline range (no keyframe optimization)
- **Locomotion**: Fully supported - enable by adding `swgLocomotion` attribute to master node
- **Messages**: Fully supported - add `swgAnimMsg_<name>` attributes with comma-separated frame numbers

### Skeletal Mesh Export

- **Influences**: Maximum 4 bone influences per vertex (excess pruned)
- **Blend shapes**: Fully supported - Maya blend shapes are exported as BLTS/BLT
- **Occlusion zones**: Fully supported - stored via `swgOcclusionZones` attribute
- **DOT3 normals**: Metadata preserved via `swgDot3Count` attribute
- **Hardpoints**: Fully supported - child locators starting with `hp_` are exported

### Static Mesh Export

- **LOD**: Exports single LOD only (no automatic LOD generation)
- **Collision**: Collision group created on import - drag geometry into it for export

### Portal Objects

- **Pathfinding**: Not exported
- **Lights**: Fully supported - ambient, directional, and point lights in `lights` group
- **Floors**: Fully supported - floor references in `collision/floor0` with `external_reference` attribute

### General

- **Floor export**: Not implemented (import only)
- **DDS export**: Requires NVIDIA Texture Tools external program
- **Large files**: Very large meshes may require increased buffer sizes

---

## Troubleshooting

### Plugin Won't Load

**Error**: "The specified module could not be found"

**Solutions**:

1. Ensure Maya 2026 64-bit is installed
2. Check Visual C++ Redistributable is installed
3. Verify plugin was built for correct Maya version
4. Check MAYA_PLUG_IN_PATH if using custom location

### Import Fails - Path Not Found

**Error**: "Could not resolve file path"

**Solutions**:

1. Run `setBaseDir "D:/your/data/root"` first
2. Or set `TITAN_DATA_ROOT` environment variable
3. Ensure file exists at expected path
4. Check path uses forward slashes

### Mesh Imports Without Textures

**Cause**: Shader or texture files not found

**Solutions**:

1. Ensure `shader/` and `texture/` folders exist under data root
2. Check shader paths in the mesh file
3. Import shader manually: `importShader -i "shader/name"`

### Animation Doesn't Play Correctly

**Cause**: Skeleton mismatch or coordinate issues

**Solutions**:

1. Import the correct `.sat` file first
2. Use `swgRevertToBindPose` before importing new animation
3. Check joint names match between skeleton and animation

### Export Creates Empty or Corrupt File

**Cause**: Invalid selection or missing data

**Solutions**:

1. Ensure correct object is selected
2. Check mesh has valid geometry (no n-gons, proper UVs)
3. For skinned meshes, verify skin cluster exists
4. Check Script Editor for detailed error messages

### Skin Weights Export Incorrectly

**Cause**: Too many influences or unnormalized weights

**Solutions**:

```mel
// Limit to 4 influences
skinCluster -e -mi 4 skinCluster1;

// Prune small weights
skinPercent -pruneWeights 0.01 skinCluster1;

// Normalize
skinCluster -e -normalizeWeights 1 skinCluster1;
```

### POB Export Fails Validation

**Cause**: Invalid hierarchy or missing data

**Solutions**:

1. Run `validatePob` to see specific issues
2. Ensure all cells have required children (mesh, portals, floor0)
3. Check portal indices are paired correctly
4. Verify external_reference attributes are set

### Script Editor Shows Errors

Enable verbose logging to see detailed information:

```mel
// In Maya Script Editor, enable "Echo All Commands"
// Or check SwgMayaEditor.cfg verboseLogging = true
```

### Performance Issues with Large Scenes

**Solutions**:

1. Import meshes one at a time
2. Use File > Optimize Scene Size
3. Delete construction history before export
4. Close Animation Browser when not in use

---

## Appendix: File Format Quick Reference

### IFF Structure

All SWG files use the IFF (Interchange File Format) container:

```
FORM <size> <type>
  FORM <size> <version>
    CHUNK <size> <tag>
      <data>
    CHUNK ...
  FORM ...
```

### Common Tags


| Tag    | Description                       |
| ------ | --------------------------------- |
| `MESH` | Static mesh container             |
| `SKMG` | Skeletal mesh generator           |
| `SKTM` | Skeleton template                 |
| `KFAT` | Keyframe animation (uncompressed) |
| `CKAT` | Keyframe animation (compressed)   |
| `SMAT` | Skeletal appearance template      |
| `PRTO` | Portal object                     |
| `MLOD` | Mesh LOD container                |
| `APPR` | Appearance data                   |
| `SPS`  | Shader primitive set              |


---

*Document generated for SwgMayaEditor v1.0*
*For the latest updates, see the repository README and changelog.*