# SwgMayaEditor — Maya 2027 plugin

64-bit Maya 2027 plugin for SWG asset authoring. Migrated from MayaExporter (32-bit Maya 8).

**Documentation (unified):** all guides are under **[docs/](docs/README.md)**. Start with **[docs/guide.md](docs/guide.md)** for commands, data paths, and workflows, then **[docs/manual.md](docs/manual.md)** for depth.

---

## Requirements

- **Maya 2027** (64-bit)
- **Maya 2027 Devkit** at `D:\titan\lib\Maya2027\devkitBase` (`include/` + `lib/`)
- **Visual Studio Insiders** at `D:\Program Files\Microsoft Visual Studio\18\Insiders` (or VS 2022+)
- **CMake** 3.13+

---

## Build

### Windows (x64) — recommended

```powershell
cd D:\titan\client\src\engine\client\application\MayaModern
.\build-mayamodern.ps1
```

Output: `build/Release/SwgMayaEditor.mll` and companion `.mel` scripts in the same folder.

### Manual CMake

```powershell
$env:DEVKIT_LOCATION = "D:\titan\lib\Maya2027\devkitBase"
$env:MAYA_LOCATION = "C:\Program Files\Autodesk\Maya2027"
cmake -B build -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_GENERATOR_INSTANCE="D:\Program Files\Microsoft Visual Studio\18\Insiders" `
  -DDEVKIT_LOCATION=$env:DEVKIT_LOCATION `
  -DMAYA_LOCATION=$env:MAYA_LOCATION
cmake --build build --config Release
```

If `Visual Studio 18 2026` is unavailable in your CMake version, use `-G "Visual Studio 17 2022"` with the same `CMAKE_GENERATOR_INSTANCE`.

### Paths

| Variable | Default |
|----------|---------|
| `DEVKIT_LOCATION` | `D:\titan\lib\Maya2027\devkitBase` |
| `MAYA_LOCATION` | `C:\Program Files\Autodesk\Maya2027` |
| `CMAKE_GENERATOR_INSTANCE` | `D:\Program Files\Microsoft Visual Studio\18\Insiders` |

---

## Install

```powershell
.\scripts\Deploy-ToMayaPlugIns.ps1
```

Defaults to `%USERPROFILE%\Documents\maya\2027\plugins\` (or `Maya2027\bin\plug-ins` if that tree exists).

**Unload the plugin** before overwriting the `.mll` on Windows.

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
