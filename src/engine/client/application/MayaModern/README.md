# SwgMayaEditor — Maya 2026 plugin

64-bit Maya 2026 plugin for SWG asset authoring. Migrated from MayaExporter (32-bit Maya 8).

**Documentation (unified):** all guides are under **[docs/](docs/README.md)**. Start with **[docs/guide.md](docs/guide.md)** for commands, data paths, and workflows, then **[docs/manual.md](docs/manual.md)** for depth.

---

## Requirements

- **Maya 2026** (64-bit) — [Download](https://www.autodesk.com/developer-network/platform-technologies/maya)
- **Maya 2026 Devkit** — separate download from Autodesk; `include/` and `lib/` must be visible to CMake (`MAYA_LOCATION` or install layout).
- **Visual Studio 2022** (17.8.3+)
- **CMake** 3.13+

---

## Build

### Windows (x64)

```batch
cd D:\titan\client\src\engine\client\application\MayaModern
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `build/Release/SwgMayaEditor.mll` and copied `.mel` scripts in the same folder.

### Maya location

FindMaya checks `MAYA_LOCATION`, then `C:\Program Files\Autodesk\Maya2026`, then `D:\Program Files\Autodesk\Maya2026`. Override:

```batch
set MAYA_LOCATION=D:\Path\To\Maya2026
cmake -B build -A x64
```

The devkit must be present where `MAYA_LOCATION` points. **VS 2022 Insiders:** set `CMAKE_GENERATOR_INSTANCE` if you need a non-default VS install.

---

## Install

Copy `SwgMayaEditor.mll` (and keep the `.mel` files beside it) to:

```
C:\Users\<user>\Documents\maya\2026\plugins\
```

Or add the `build/Release` folder to `MAYA_PLUG_IN_PATH`. **Unload the plugin** before overwriting the `.mll` on Windows.

```mel
loadPlugin SwgMayaEditor;
```

---

## POB authoring (buildings / apartments)

After `loadPlugin SwgMayaEditor`, use these for large portal graphs. Full flag details: **[docs/guide.md](docs/guide.md)**.

| Command | Purpose |
| ------- | ------- |
| `createPobTemplate` | `-n NAME -cells N [-layoutSpacing F]` — empty `r*` hierarchy; spacing on +X |
| `layoutPobCells` | `[-cols C] [-dx F] [-dz F] [-root …]` — grid cells (+X then +Z) |
| `addPobPortal` | Selection: cell or `portals`. Presets 0–4, optional `-doorHardpoint` |
| `connectPobCells` | Paired doorway: `-from rA -to rB` or select two cells |
| `duplicatePobCell` | Select `rN`; optional `-stripPortals`, `-remapPortalIndices N` |
| `reportPobPortals` | List portal indices; flag non-paired indices |
| `validatePob` | Cells, mesh/portals/collision/floor0, external refs |

**Flow:** `createPobTemplate` → optional `layoutPobCells` → mesh/floor refs → `connectPobCells` / `addPobPortal` → `validatePob` → `exportPob`.

**MEL:** `source "…/build/Release/pobAuthoring.mel"` for `swg_pob_*` helpers (placeholders, validation, export dialog). New files often need a light: `swg_pob_defaultLight` after sourcing.

---

## File type support (summary)

See [docs/manual.md](docs/manual.md) for full behavior. Quick matrix:

| Extension | Type | Import | Export |
| --------- | ---- | ------ | ------ |
| `.msh` | Static mesh | Yes | Yes |
| `.apt` | Appearance redirect | Yes | Yes |
| `.mgn` | Skeletal mesh | Yes | Yes |
| `.skt` | Skeleton | Yes | Yes |
| `.ans` | Animation | Yes | Yes |
| `.sat` | Skeletal appearance | Yes | Yes |
| `.pob` | Portal object | Yes | Yes |
| `.flr` | Floor | Yes | No |
| `.lod` / `.lmg` | LOD containers | Yes | Yes |
| `.sht` | Shader | Yes | Yes |
| `.dds` | Texture | Yes | No |

Roadmap / parity checklist: [todo.txt](todo.txt).
