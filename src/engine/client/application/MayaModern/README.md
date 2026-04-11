# SwgMayaEditor - Maya 2026 Plugin

64-bit Maya 2026 plugin for SWG asset authoring. Migrated from MayaExporter (32-bit Maya 8).

## Requirements

- **Maya 2026** (64-bit) - [Download](https://www.autodesk.com/developer-network/platform-technologies/maya)
- **Maya 2026 Devkit** - **Separate download from Maya.** The devkit is not included in the Maya installer. Get it from the Downloads section of the [Autodesk Feedback Forum](https://feedback.autodesk.com/). Extract it and place the `include/` and `lib/` folders where FindMaya can find them (e.g. inside your Maya install, or set `MAYA_LOCATION` to the devkit root).
- **Visual Studio 2022** (17.8.3+)
- **CMake** 3.13+

## Build

### Windows (x64 required)

```batch
cd D:\titan\client\src\engine\client\application\MayaModern

# Create build directory - use VS 2022 Insiders
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build (or open build/SwgMayaEditor.sln in VS 2022 Insiders)
cmake --build build --config Release
```

The plugin will be built as `SwgMayaEditor.mll`. You can also open `build/SwgMayaEditor.sln` directly in VS 2022 Insiders.

### Maya Location

FindMaya checks these locations (in order):
- `MAYA_LOCATION` (env or CMake variable)
- `C:\Program Files\Autodesk\Maya2026`
- `D:\Program Files\Autodesk\Maya2026`

If Maya is elsewhere:

```batch
set MAYA_LOCATION=D:\Path\To\Maya2026
cmake -B build -A x64
```

**Note:** The devkit (include/, lib/) must be present. If you installed Maya separately from the devkit, extract the devkit archive so its `include/` and `lib/` folders are inside your Maya directory, or point `MAYA_LOCATION` to the devkit root.

**VS 2022 Insiders:** Use the VS 2022 generator; the .sln will open in your default VS. To force Insiders when both are installed, set `CMAKE_GENERATOR_INSTANCE` to your Insiders install path (e.g. `D:/Program Files/Microsoft Visual Studio/2022/Insiders`).

## POB authoring (buildings / apartments)

After loading the plugin, use these for large portal graphs (complexes, corridors, many units).

| Command | Purpose |
|--------|---------|
| **`createPobTemplate`** | ` -n NAME -cells N [-layoutSpacing F]` — empty `r*` hierarchy; spacing spreads cells on **+X** in the viewport. |
| **`layoutPobCells`** | `[-cols C] [-dx F] [-dz F] [-root …]` — repositions existing `r*` cells in a grid (row-major: +X then +Z). |
| **`addPobPortal`** | Selection: cell or `portals`. `[-preset 0–4] [-w/-h] [-index] [-clockwise] [-target] [-doorHardpoint] [-doorStyle]`. **Presets:** 0 default 2×2.5, 1 wide, 2 narrow, 3 tall, 4 garage. |
| **`connectPobCells`** | One step for a **paired doorway:** `-from rA -to rB` (names under root) **or** select **two** cells (from = CCW 0 side, to = CW 1 side). Sets shared index + targets. |
| **`duplicatePobCell`** | Select **`rN`** (direct child of POB root). `[-stripPortals]` empty portals on copy; `[-remapPortalIndices N]` add **N** to every portal index under the copy. |
| **`reportPobPortals`** | Lists portal indices and paths; flags indices that are not a pair of two. |
| **`validatePob`** | Cells: `mesh` / `portals` / `collision` / `floor0`; warns on empty `external_reference`; repeats portal-pair audit. |

**Typical flow:** `createPobTemplate` → optional `layoutPobCells` → assign mesh/floor refs → `connectPobCells` or `addPobPortal` for doors → `validatePob` → `exportPob`.

**MEL:** `source "…/build/Release/pobAuthoring.mel"` (path next to the `.mll`) for `swg_pob_*` shortcuts.

**Black / empty viewport:** new Maya files often have no lights — run `swg_pob_defaultLight` after sourcing the MEL. Then `swg_pob_apartmentPreset` builds a 24-cell grid named `apt_wing_a` with **placeholder** room cubes and floor planes sized to your `dx`/`dz` bays (ignored by `exportPob`; real art uses `external_reference`). Or call `swg_pob_fromScratch` with your own counts. For a template built with `createPobTemplate` only, run `swg_pob_addTemplatePlaceholders "root" 11.76 3.0 9.8 12 10` (example) after `layoutPobCells`. Use `swg_pob_connectChain "r0,r1,r2"` for a corridor spine between consecutive cells.

**More MEL (after `source pobAuthoring.mel`):**

| Proc | What it does |
|------|----------------|
| `swg_pob_addTemplatePlaceholders "apt_wing_a" 11.76 3.0 9.8 12 10` | Room `polyCube` (W,H,D) under each `mesh` + floor `polyPlane` (W,D) under `floor0`; args = inset room, ceiling height, full floor footprint. |
| `swg_pob_setAllMeshRefs "apt_wing_a" "appearance/your.lod"` | Same mesh ref on every `r*\|mesh`. |
| `swg_pob_setAllFloorRefs "apt_wing_a" "appearance/collision/your_floor"` | Same floor `.flr` path on every `floor0`. |
| `swg_pob_setCellRefs "apt_wing_a" 5 "appearance/unit.msh" "appearance/collision/floor"` | One cell’s mesh + floor. |
| `swg_pob_connectRing "r0,r1,r2,r3"` | Chain plus last→first (courtyard / loop). |
| `swg_pob_validateFull "apt_wing_a"` | `validatePob` + `reportPobPortals` with `-root`. |
| `swg_pob_validateFullSelection` | Same, inferring root from selection. |
| `swg_pob_exportDialog` | File dialog → `exportPob`. |
| `swg_pob_layoutColumnY "apt_wing_a" 4.0` | Stack cells on local **Y** (tower preview). |
| `swg_pob_selectPortalsGroup "apt_wing_a" 3` | Select `r3\|portals` for `addPobPortal`. |
| `swg_pob_selectAllMeshes "apt_wing_a"` | Multi-select all cell meshes. |
| `swg_pob_printCellPaths "apt_wing_a"` | Print `root\|rN` paths to Script Editor. |
| `swg_pob_clearAllPortals "apt_wing_a"` | Deletes all portal transforms under every cell (reset). |

## Install

Copy `SwgMayaEditor.mll` to your Maya 2026 plugins directory:

```
C:\Users\<user>\Documents\maya\2026\plugins\
```

Or add the build output path to `MAYA_PLUG_IN_PATH` in Maya's environment.

## Features

See [todo.txt](todo.txt) for the full feature roadmap and MayaExporter parity checklist.

### Current Translators
- **mgn** - Mesh generator (.mgn)
- **msh** - Mesh (.msh)
- **skt** - Skeleton (.skt)
- **flr** - Floor (.flr)
