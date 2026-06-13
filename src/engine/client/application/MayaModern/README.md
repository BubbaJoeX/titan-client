# SwgMayaEditor — Maya 2027 plugin

64-bit Maya 2027 plugin for SWG asset authoring. Migrated from legacy MayaExporter (32-bit Maya 8).

**Documentation:** [docs/README.md](docs/README.md) — start with [docs/guide.md](docs/guide.md) for commands and static mesh export.

---

## Requirements

- **Maya 2027** (64-bit)
- **Maya 2027 Devkit** at `D:\titan\lib\Maya2027\devkitBase` (`include/` + `lib/`)
- **Visual Studio 2022+** or **VS Insiders** (see `build-mayamodern.ps1`)
- **CMake** 3.13+
- **NVIDIA Texture Tools** (`nvtt_export.exe`) for static mesh / shader DDS publish

---

## Build

```powershell
cd D:\titan\client\src\engine\client\application\MayaModern
.\build-mayamodern.ps1
```

Output: `build/Release/SwgMayaEditor.mll` and companion `.mel` scripts in the same folder.

Unload the plugin in Maya before overwriting the `.mll` on Windows.

---

## Install

```powershell
.\scripts\Deploy-ToMayaPlugIns.ps1
```

Defaults to `%USERPROFILE%\Documents\maya\2027\plugins\`.

Place **`SwgMayaEditor.cfg`** next to the `.mll` (nvtt path, optional `gameDataRoot`, shader prototypes). The plugin also loads `SwgMayaEditor.cfg` from the Maya working directory if present.

```mel
loadPlugin "SwgMayaEditor";
```

### Recommended `Maya.env` (example)

```
TITAN_DATA_ROOT=D:/titan/data/sku.0/sys.client/compiled/game
TITAN_EXPORT_ROOT=D:/exported
MAYA_PLUG_IN_PATH=C:/Users/you/Documents/maya/2027/plugins
PATH=%PATH%;C:/Program Files/NVIDIA Corporation/NVIDIA Texture Tools
```

---

## POB authoring (buildings)

See [docs/guide.md](docs/guide.md) and the quick table in [docs/README.md](docs/README.md). Commands: `createPobTemplate`, `layoutPobCells`, `connectPobCells`, `addPobPortal`, `validatePob`, `exportPob`. MEL helpers: `pobAuthoring.mel`.

---

## File types (summary)

| Extension | Import | Export | Notes |
|-----------|--------|--------|-------|
| `.msh` / `.apt` | Yes | Yes | Static mesh; import creates **one mesh shape per shader primitive** |
| `.mgn` | Yes | Yes | Skeletal mesh |
| `.skt` | Yes | Yes | Skeleton |
| `.ans` | Yes | Yes | Animation |
| `.sat` | Yes | Yes | Skeletal appearance |
| `.pob` | Yes | Yes | Portal object |
| `.lod` / `.lmg` | Yes | Yes | LOD containers |
| `.sht` | Yes | Yes | Shader template |
| `.flr` | Yes | No | Floor |
| `.dds` | Yes | No | Texture (export via nvtt from TGA/PNG) |

Full matrix: [docs/manual.md](docs/manual.md). Roadmap: [docs/todo.txt](docs/todo.txt).
