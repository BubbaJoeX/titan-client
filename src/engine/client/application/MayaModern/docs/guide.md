# SwgMayaEditor — Guide (commands and workflows)

This is the **main operational guide** for the SwgMayaEditor Maya 2026 plugin: MEL commands, translators, deploy notes, and workflows.

**Other docs in this folder**

- [README.md](README.md) — Documentation home and table of contents  
- [manual.md](manual.md) — Full editor manual (features, import/export detail, coordinate system)  
- [ans-format.md](ans-format.md) — ANS (KFAT/CKAT) IFF layout reference

---

## Quick usage

1. Build `SwgMayaEditor.mll` (see root [README.md](../README.md)), install next to bundled `.mel` scripts, then `loadPlugin SwgMayaEditor`.
2. Point Maya at game data (pick one):
  - Set `TITAN_DATA_ROOT` to your `…/compiled/game` tree **before** launch, or  
  - In Maya: `setBaseDir "D:/path/to/compiled/game/";` then `getDataRootDir;` to verify.
3. Import via **File → Import** using types `SwgSat`, `SwgAns`, etc., or use the MEL commands below.

**Example SAT import**

```mel
file -import -type "SwgSat" -ignoreVersion -ra true -mergeNamespacesOnClash false -namespace "my_creature" -pr -importFrameRate true -importTimeRange "override" "D:/data/.../appearance/creature.sat";
```

Unload the plugin before overwriting the `.mll` on Windows (file lock).

---

## Prerequisites

1. **Load the plugin**: `loadPlugin SwgMayaEditor`
2. **Set base directory** (required for most operations):
  ```mel
   setBaseDir "D:\\exported";
  ```
   This configures output directories under the given path (appearance, shader, texture, animation, skeleton, mesh, log).
3. **Path resolution**: Import commands resolve relative paths using:
  - `TITAN_DATA_ROOT`, `TITAN_EXPORT_ROOT`, or `DATA_ROOT` environment variables, or
  - The directory set by `setBaseDir` (stored as `dataRootDir`)
   Paths like `appearance/foo/bar` are resolved relative to the base. Absolute paths are used as-is.

---

## Build output and deploying (Windows)

- **Release output**: `MayaModern/build/Release/SwgMayaEditor.mll` plus `.mel` scripts copied next to it by the build.
- **Install into Maya** (`…/Maya2026/bin/plug-ins`): configure CMake with `SWG_MAYA_PLUGIN_INSTALL_DIR` pointing at that folder, then build target `**swgDeployMayaPlugins`** (elevated if under Program Files).
- **Important**: **Quit Maya** (or run `unloadPlugin SwgMayaEditor`) before deploying. While the plugin is loaded, Windows **locks** `SwgMayaEditor.mll`, so the copy may fail or appear to update nothing (e.g. **0** files / old build still running). After deploy, restart Maya and `loadPlugin SwgMayaEditor` again.

---

## Setup Commands

### setBaseDir

Configures all export/import directories under a base path. **Run this first** before importing or exporting.

```mel
setBaseDir "D:\\exported";
```

Creates and configures:

- `appearance\`, `shader\`, `texture\`, `animation\`, `skeleton\`, `mesh\`, `log\`, `exported\`
- Reference prefixes for tree paths (e.g. `appearance/`, `shader/`, `texture/`)

### getDataRootDir

Returns the **same** base directory import uses: `TITAN_DATA_ROOT`, else `TITAN_EXPORT_ROOT`, else `DATA_ROOT`, else `setBaseDir` / cfg (`dataRootDir`, then `appearanceWriteDir` parent). Prefer `**TITAN_DATA_ROOT`** for consistency with the rest of the toolchain.

```mel
getDataRootDir;
```

---

## Import Commands

### importSkeleton

Imports a skeleton template (.skt).

```mel
importSkeleton -i "appearance/skeleton/humanoid/humanoid";
importSkeleton -i "path/to/skeleton.skt" -parent "|group1";
```


| Flag      | Description                                        |
| --------- | -------------------------------------------------- |
| `-i`      | Input path (required). Tree path or absolute path. |
| `-parent` | Optional. DAG path of parent transform.            |


### importAnimation

Imports an animation (.ans) via Maya's File > Import.

```mel
importAnimation -i "appearance/animation/humanoid/combat/dance.ans";
```


| Flag | Description            |
| ---- | ---------------------- |
| `-i` | Input path (required). |


### importLodMesh

Imports a LOD mesh, APT redirect, or DTLA/MLOD. Resolves .lod, .apt, or .msh automatically.

```mel
importLodMesh -i "appearance/mesh/object_lod0";
importLodMesh -i "appearance/object" -parent "|group1";
```


| Flag      | Description                                                  |
| --------- | ------------------------------------------------------------ |
| `-i`      | Input path (required). Tries .lod, .apt, .msh in that order. |
| `-parent` | Optional. DAG path of parent transform.                      |


### importSkeletalMesh

Imports a skeletal mesh (.mgn) with skin weights, blend shapes, and hardpoints.

```mel
importSkeletalMesh -i "appearance/mesh/character_lod0" -s "appearance/skeleton/humanoid/humanoid";
importSkeletalMesh -i "character.mgn" -s "humanoid.skt" -parent "|root";
```


| Flag      | Description                 |
| --------- | --------------------------- |
| `-i`      | Input mesh path (required). |
| `-s`      | Skeleton path (required).   |
| `-parent` | Optional. Parent transform. |


### importStaticMesh

Wrapper that calls `importLodMesh` with the given path. Use for static meshes, APT redirects, or LODs.

```mel
importStaticMesh -i "appearance/mesh/object";
importStaticMesh -i "object" -parent "|group1";
```


| Flag      | Description                 |
| --------- | --------------------------- |
| `-i`      | Input path (required).      |
| `-parent` | Optional. Parent transform. |


### importShader

Imports a shader template (.sht). Converts DDS textures to TGA for Maya editing.

```mel
importShader -i "shader/foo/bar";
```


| Flag | Description                   |
| ---- | ----------------------------- |
| `-i` | Input shader path (required). |


### exportShader

Exports a shader template (.sht) to `shaderTemplateWriteDir`, converting edited TGA textures in `textureWriteDir` to DDS and updating texture paths in the shader (same pipeline as `exportStaticMesh` shader pass).

```mel
setBaseDir "D:\\exported";
exportShader -i "shader/foo/bar";
exportShader -path "shader/foo/bar";
```


| Flag           | Description                                                                  |
| -------------- | ---------------------------------------------------------------------------- |
| `-i` / `-path` | Shader tree path (required), e.g. `shader/foo/bar` (with or without `.sht`). |


**Prerequisites**: `setBaseDir` (or configured `shaderTemplateWriteDir` / `textureWriteDir`). Source shader is resolved like imports (data root / `TITAN_DATA_ROOT`).

### importSat

Imports a skeletal appearance template (.sat). Loads skeleton, LOD meshes, and appearance hierarchy.

```mel
importSat -i "appearance/character/sat_name";
```


| Flag | Description                |
| ---- | -------------------------- |
| `-i` | Input SAT path (required). |


### importPob

Imports a portal object (.pob) with cells, portals, and appearance references.

```mel
importPob -i "appearance/building/cantina";
```


| Flag | Description                |
| ---- | -------------------------- |
| `-i` | Input POB path (required). |


### importStructure

One-shot import for a structure basename: resolves `basename.pob`, optional `basename.flr`, shell mesh at `basename.msh` or `appearance/mesh/<basename>.(msh|apt|lod)`, and optionally a shader.

```mel
importStructure -i "appearance/building/cantina";
importStructure -i "appearance/building/cantina" -flr;
importStructure -i "appearance/building/cantina" -shader "shader/building/cantina_ext";
```


| Flag             | Description                                                                                                       |
| ---------------- | ----------------------------------------------------------------------------------------------------------------- |
| `-i`             | Tree path without extension (required). `appearance/` is prepended if missing.                                    |
| `-flr`           | Import standalone `.flr` even when a POB was loaded (default: skip FLR if POB exists, to avoid duplicate floors). |
| `-shader` / `-s` | Run `importShader` on this path after geometry.                                                                   |


Requires `setBaseDir` / data root the same as other import commands.

---

## Export Commands

### exportSkeleton

Exports the selected joint or skeleton hierarchy to .skt.

```mel
select -r joint1;
exportSkeleton;

exportSkeleton -bp -10;
exportSkeleton -path "D:\\exported\\appearance\\skeleton\\custom.skt";
```


| Flag    | Description                                                            |
| ------- | ---------------------------------------------------------------------- |
| `-bp`   | Bind pose frame number (default: -10).                                 |
| `-path` | Optional. Full output path. Otherwise uses `skeletonTemplateWriteDir`. |


**Selection**: Select the root joint or any joint in the skeleton.

### exportStaticMesh

Exports the selected mesh to .msh and optionally .apt. Also exports shaders (TGA→DDS) and creates an APT redirect.

```mel
select -r mesh1;
exportStaticMesh;

exportStaticMesh -path "D:\\exported\\appearance\\mesh\\object.msh";
exportStaticMesh -name "custom_name";
```


| Flag    | Description                                                          |
| ------- | -------------------------------------------------------------------- |
| `-path` | Optional. Output path. Default: `appearanceWriteDir/mesh/<name>.msh` |
| `-name` | Optional. Override mesh name.                                        |


**Selection**: Select a mesh (or its transform).

**Hardpoints**: Add child transforms under the **mesh parent** (same level as the mesh shape’s transform). Each transform except `floor_component` and the vehicle authoring group `hardpoints` is written as HPNT (name, position, rotation only—no extra mesh geometry). To place a hardpoint with a **0.5 m viewport cube** that is **not** baked into the `.msh`, select the static mesh and run `swgAddStaticMeshHardpoint -n my_hp` (creates `hp_my_hp` and a cube shape tagged `swgExcludeFromStaticMeshExport`). You can tag any preview-only mesh shape with that attribute so `exportStaticMesh` / `swgPrepareStaticMeshExport` / `swgReformatMesh` ignore it when resolving export geometry. Imported `.msh` hardpoints get the same preview cube when rebuilt in the scene.

**Output**:

- `.msh` – MESH/0005 with geometry, hardpoints, shader groups
- APPR/FLOR – If the mesh transform has a child `floor_component` with string attribute `floorPath` (from `.msh` import), that path is written into the floor reference chunk on export.
- `.apt` – Redirect to mesh (in `appearanceWriteDir`)
- `.sht` – Shaders with image→DDS conversion via **nvtt** (in `shaderTemplateWriteDir`)

### OBJ / Wavefront `.mtl` (auto → DDS + `.sht`)

Maya’s OBJ importer loads the companion `**.mtl`** and builds shading networks **file texture → surface shader → shading group**. You do **not** need a separate step for that. `**exportStaticMesh`** walks those networks (including common intermediate nodes like `**bump2d**`) to find the diffuse image, **writes `texture/<mesh>_d.dds`** (and per-slot names when multimaterial), **clones the prototype `.sht`**, etc.

**Important — no intermediate `.tga` on disk:** The plug-in converts **PNG / JPG / TGA / …** straight to `**.dds`** using `**nvtt_export.exe**` (NVIDIA Texture Tools). Nothing creates a sibling `.tga` unless you enable `**textureMirrorSourceBesideDds=1**` in **SwgMayaEditor.cfg**, which copies the **original** diffuse file beside the `.dds` as `**texture/<name>_src.png`** (or `.jpg`, etc.) for debugging. Configure `**nvttExporterPath**` to your installed `nvtt_export.exe`; if it is missing or fails, Script Editor lines `**[TgaToDds]**` / `**[ShaderExporter]**` explain the failure (the viewer then falls back to placeholder art). If the diffuse on disk is already `**.dds**`, it is **copied** to the published name.

Configure `**setBaseDir`** so `**textureWriteDir**` / `**shaderTemplateWriteDir**` are set; keep **shader/defaultshader.sht** (or `**shaderPrototypeSht`**) available as elsewhere in this guide.

**Optional** `**swgApplyWavefrontMtl`** — only if you need to **force** `**swgShaderPath`** / `**swgTexturePath**` from a `.mtl` on disk (e.g. broken file paths after moving files). Normal OBJ→export flow uses Maya’s networks only.

### exportPob

Exports the selected POB hierarchy to .pob.

```mel
select -r pobRoot;
exportPob -i "appearance/building/cantina";
```


| Flag | Description             |
| ---- | ----------------------- |
| `-i` | Output path (required). |


**Selection**: Select the POB root or a cell.

### Skeletal mesh, animation, LOD, and SAT export (status)


| Goal                   | SwgMayaEditor                                                   | Notes                                                                                                                                                                                                                                                        |
| ---------------------- | --------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `.mgn` (SKMG)          | **Import** and **Export** supported via File > Export Selection | Select a skinned mesh bound to a skeleton. Exports skin weights, UVs, normals, per-shader data. Optional `swgShipBundlePaths` (spacecraft) or `swgVehicleBundlePaths` (ground vehicles) on the mesh parent copies extra IFFs next to the `.mgn` (see below). |
| `.lsb` (LSAT)          | **Import** and **Export** supported via File > Import / Export  | Lightsaber template: **per-field** attrs; **import also loads the hilt `.apt`/mesh** from **Hilt appearance path** (BASE) under the LSB root via `importLodMesh`. Legacy string attrs still work on export if present.                                       |
| `.ans` (keyframe anim) | **Import** and **Export** supported via File > Export Selection | Exports KFAT (uncompressed) format. Captures delta rotations/translations from bind pose.                                                                                                                                                                    |
| `.lod` (MLOD)          | **Import** and **Export** supported                             | Select transform with `swgLodChildren` attribute (from import) to re-export LOD container.                                                                                                                                                                   |
| `.lmg` (MLOD)          | **Import** and **Export** supported                             | Select transform with `swgLmgChildren` attribute (from import) to re-export LMG container.                                                                                                                                                                   |
| `.sat`                 | **Import** via `importSat`; **Export** via `ExportSat` command  | Export writes skeleton reference and mesh LOD references from scene hierarchy.                                                                                                                                                                               |


All file types now support round-trip editing. Use **File > Export Selection** with the appropriate file type filter.

### MGN export: optional bundle IFFs (ships vs ground vehicles)

On the **parent transform** of the skinned mesh (the transform above the mesh shape), add **one or both** string attributes, depending on asset type:

- `**swgShipBundlePaths`** — **Spacecraft / starships** (cockpit and other ship IFFs the client references). Same semantics as before.
- `**swgVehicleBundlePaths`** — **Ground vehicles** (speeders, walkers, etc.); separate authoring path from ships. `**swgMakeVehicle`** creates this attribute on `**swgVehicle_geo**`.

Each attribute holds one or more **data-root-relative tree paths** (or absolute paths), separated by **semicolons** or **newlines**. After a successful `.mgn` write, the plug-in resolves each path like other imports (`setBaseDir` / `TITAN_DATA_ROOT`, etc.) and **copies** each file into the **same folder as the exported `.mgn`**, using the file’s basename. The same sources are also copied into `**<dataRoot>/exported/**` (see below). Missing sources produce a script editor warning; copy failures are warned but do not roll back the `.mgn`.

**Export staging (`exported/`)**: `setBaseDir` creates an `**exported`** folder under your base path. For any configured **data root** (`getDataRootDir` / `TITAN_DATA_ROOT` / `setBaseDir`), successful `**.mgn`**, bundle IFF, and `**.lsb`** writes are **mirrored** into `**<dataRoot>\\exported\\`** with the same basename so client-relevant files collect in one place alongside your chosen export path.

### Authoring: `swgMakeShip`, `swgMakeVehicle`, and `swgMakeLightsaber`

- `**swgMakeShip`** — **Spacecraft** rig: `**|…|swgShip_geo`** (string attribute `**swgShipBundlePaths**`), `**|…|swgShip_skeleton|swgShip_root**`, empty geo group. Parent your skinned hull under `**swgShip_geo**`, bind to the skeleton, fill bundle paths, then export `**.mgn**` with **SwgMgn**. Optional: `swgMakeShip -n myShip` (alphanumeric / underscore root name).
- `**swgMakeVehicle`** — **Ground vehicle** rig (not a ship): `**|…|swgVehicle_geo`** (`**swgVehicleBundlePaths**`), `**|…|swgVehicle_skeleton|swgVehicle_root**`, `**seat_0` … `seat_{n-1}**` under `**swgVehicle_root**`, empty `**hardpoints**` under the root, and `**swgVehicleSeatCount**` on the root. Use **one seat** for a single occupant; **two or more** for multi-passenger (`**seat_0`** = primary / driver). Flags: `swgMakeVehicle -n myVehicle -seats 4` (seats **1–16**). Export `**.mgn`** like any skinned mesh; bundle copying uses `**swgVehicleBundlePaths**`.
- `**swgMakeLightsaber`** — Creates a **capped cylinder** (0.1 m diameter, 0.6 m height, Y-up) and installs the **granular** `swgLsb`* attribute set (categories in the Attribute Editor: **LSB / Core**, **LGHT Flicker**, **BLAD blade N**). Optional: `swgMakeLightsaber -n myLsb`. Fill **Hilt appearance path** and blade **Shader** fields before export; export IFF matches the client LSAT layout.

### `swgVehicleToolkit` — Vehicle suite UI

Run `**swgVehicleToolkit`** (with **SwgMayaEditor** loaded) to open **SWG Vehicle Toolkit**:

- **Create vehicle rig** — runs `swgMakeVehicle` with the **root name** and **seat count** from the window (presets for 1, 2, or 4 seats). Spacecraft use `**swgMakeShip`** separately (not from this window).

Requires `**getDataRootDir**` / `setBaseDir` for the data-root line (same as other toolkits).

### `swgLightsaberToolkit` — Lightsaber suite UI

Run `**swgLightsaberToolkit**` (with **SwgMayaEditor** loaded) to open **SWG Lightsaber Toolkit**:

- **Create lightsaber base** — same as `swgMakeLightsaber`.
- **Import .lsb** — file dialog + `file -import -type "SwgLsb"` (hilt APT is imported automatically as above).
- **View blade / Hide blade / Refresh** — builds a **preview cylinder** child `swgLsb_bladePreview` under the LSB transform, sized from `**swgLsbBlade0Length`** and `**swgLsbBlade0Width**` (select the LSB root or any descendant). Hide toggles visibility only.

Requires `**getDataRootDir**` / `setBaseDir` so hilt and APT paths resolve.

---

## File Translators (File > Import / Export)

Use Maya's **File > Import** or **File > Export** with these file types. The **Files of type** list shows the **filter** labels; MEL `file -import -type` / `file -export -type` must use the **short type id** (Maya matches the registered translator name, not the dialog text).

**Do not use `SAT_ATF` for SWG `.sat` files.** In Maya, `SAT_ATF` is the **ACIS solid** SAT importer. SWG skeletal appearance templates are imported with type `**SwgSat`** (this plugin).


| Extension   | MEL `-type` (register name) | Files of type label (`filter()`)  | Import | Export |
| ----------- | --------------------------- | --------------------------------- | ------ | ------ |
| .mgn        | `SwgMgn`                    | SWG skeletal mesh (*.mgn)         | Yes    | Yes    |
| .lsb        | `SwgLsb`                    | SWG lightsaber appearance (*.lsb) | Yes    | Yes    |
| .msh / .apt | `SwgMsh`                    | SWG static mesh (*.msh *.apt)     | Yes    | Yes    |
| .skt        | `SwgSkt`                    | SWG skeleton (*.skt)              | Yes    | Yes    |
| .ans        | `SwgAns`                    | SWG animation (*.ans)             | Yes    | Yes    |
| .flr        | `SwgFlr`                    | SWG floor (*.flr)                 | Yes    | No     |
| .sat        | `SwgSat`                    | SWG skeletal appearance (*.sat)   | Yes    | Yes    |
| .pob        | `SwgPob`                    | SWG portal object (*.pob)         | Yes    | Yes    |
| .lod        | `SwgLod`                    | SWG LOD container (*.lod)         | Yes    | Yes    |
| .lmg        | `SwgLmg`                    | SWG skeletal LOD (*.lmg)          | Yes    | Yes    |
| .dds        | `SwgDds`                    | SWG DDS texture (*.dds)           | Yes    | No     |


Constants live in `translators/SwgTranslatorNames.h`: `swg_translator::kType`* for scripts, `swg_translator::kFilter`* for the dialog strings.

Example SAT import:

```mel
file -import -type "SwgSat" -ignoreVersion -ra true -mergeNamespacesOnClash false -namespace "lom" "D:/path/to/4lom.sat";
```

**MESH export**: Select a mesh, then File > Export and choose a .msh path. The static-mesh translator calls `exportStaticMesh` internally.

---

## Utility Commands

### swgRevertToBindPose

Reverts the scene to bind pose. Useful before exporting skeletons or when animations are applied.

```mel
swgRevertToBindPose;
```

### swgReformatMesh

Isolates the **selected** polygon mesh(es) for a clean static-mesh workflow: deletes **every other polygon mesh** in the scene, then **groups** the remaining top-level mesh roots under a new transform (default `**swgStaticMesh`**). Shading on kept meshes is preserved; **unused** shading nodes are pruned via Hypershade’s delete-unused step. Does **not** remove cameras, lights, or non-mesh DAG nodes—only polygon meshes not in the selection.

```mel
swgReformatMesh;
swgReformatMesh -root myStaticRoot;
```

### swgAssetDissector

Opens the asset dissector UI (if `swgAssetDissector.mel` is available).

```mel
swgAssetDissector;
```

### swgAnimationBrowser

Opens a UI with **Categories** (top-level folders under `appearance/animation`), a **list** of matching `.ans` paths, **Filter** + **Refresh**, and **Import selected**. Picking a category fills the filter and refreshes the list; the filter is a case-insensitive substring on `appearance/animation/…`. **Double-click** a list row or use **Import selected** to run `importAnimation -i "<path>"`. Uses the same **data root** as other imports (`TITAN_DATA_ROOT` / `TITAN_EXPORT_ROOT` / `DATA_ROOT` or `setBaseDir`). Import a matching `**.sat`** first so joints line up.

```mel
swgAnimationBrowser;
```

### swgLightsaberToolkit

Opens **SWG Lightsaber Toolkit**: create saber base, import `.lsb`, and blade preview (details under **Authoring** above).

```mel
swgLightsaberToolkit;
```

### Statueize (`swgStatueize.mel`) — optional look-dev

Apply preset SWG textures to selected meshes; originals stored on the transform as `swgOriginalShadingGroups`.

```mel
source "/path/to/SwgMayaEditor/build/Release/swgStatueize.mel";
swgStatueizeUI;
```

Textures resolve under `getDataRootDir` / `TITAN_DATA_ROOT` in `texture/`, `texture/tatooine/`, `texture/thm_tatt_detail/`. If `source` fails after an update, restart Maya (MEL procedure cache).

### Skeletal mesh, SAT, and ANS (behavior summary)

- **SAT / `importSkeletalMesh`:** Resolves `.lmg` → `.mgn`, builds geometry and `skinCluster`. MGN transform names (XFNM) must match scene joints; matching is case-insensitive for skinning.
- **Blend targets:** Names are stored on the mesh parent (e.g. `swgBlendTargets`) for export reference; live blendShape creation on import is disabled for stability.
- **Occlusion zones:** Listed in the Script Editor and stored in custom attributes (see [manual.md](manual.md)).
- **ANS import:** Before a new import, keys are cleared and bind pose restored on joints **except** hardpoints (`hold_`* names or `swgHardpointParent` attribute) so attachment nodes stay aligned with the hand.

---

## Configuration

Create `SwgMayaEditor.cfg` in the plugin directory or Maya config path. Example:

```ini
[SwgMayaEditor]
; Path to nvtt_export.exe for TGA→DDS conversion on shader export
nvttExporterPath = "D:\\Program Files\\NVIDIA Corporation\\NVIDIA Texture Tools\\nvtt_export.exe"

; Override directories (optional; setBaseDir sets these by default)
; appearanceWriteDir = "D:\\exported\\appearance\\"
; shaderTemplateWriteDir = "D:\\exported\\shader\\"
; textureWriteDir = "D:\\exported\\texture\\"
; skeletonTemplateWriteDir = "D:\\exported\\appearance\\skeleton\\"

; Enable verbose logging
verboseLogging = false
```

---

## Related documentation (repository `docs/`)

From the Titan repo root, shared Maya articles live under `docs/`. Relative to this file (`MayaModern/docs/guide.md`):

- [MAYA_POB_FROM_SCRATCH.md](../../../../../../docs/MAYA_POB_FROM_SCRATCH.md) — new `.pob` authoring in Maya.
- [MAYA_KITBASH_IMPORT_COMBINE.md](../../../../../../docs/MAYA_KITBASH_IMPORT_COMBINE.md) — import SAT/APT/MSH, combine, pose, export static meshes.

---

## Typical Workflows

### Import and edit a static mesh

```mel
loadPlugin SwgMayaEditor;
setBaseDir "D:\\exported";
importLodMesh -i "appearance/mesh/object_lod0";
// Edit mesh in Maya...
select -r object_lod0;
exportStaticMesh;
```

### Import and export a skeleton

```mel
setBaseDir "D:\\exported";
importSkeleton -i "appearance/skeleton/humanoid/humanoid";
// Edit joints...
select -r humanoid;
exportSkeleton -bp -10;
```

### Import a POB and export changes

```mel
setBaseDir "D:\\exported";
importPob -i "appearance/building/cantina";
// Edit cells, portals, appearances...
select -r r0;  // POB root
exportPob -i "appearance/building/cantina_modified";
```

### Round-trip shader editing

```mel
importShader -i "shader/foo/bar";
// Edit textures in Hypershade (TGA in textureWriteDir)...
exportShader -i "shader/foo/bar";  // or rely on exportStaticMesh to export referenced shaders
importLodMesh -i "appearance/mesh/object";  // Uses shader
select -r object;
exportStaticMesh;
```

---

## IFF format versions (client parity)

Authoritative notes for which **FORM** versions the game client loads live in:

`MayaModern/translators/SwgIffFormatVersions.h`

The `**.msh`** translator (`MshTranslator`) follows `**MeshAppearanceTemplate`**, `**AppearanceTemplate`**, and `**ShaderPrimitiveSetTemplate**` for MESH / APPR / SPS versions.

---

## Troubleshooting

### "No SPS form - importing APPR only"

When importing a `.msh` file without SPS (Shader Primitive Set) geometry, you may see:

```
[MshTranslator]   No SPS form - importing APPR only (hardpoints, floor)
```

**Behavior**: The importer creates the root transform with hardpoints and floor reference from the APPR form. No mesh geometry is created. This supports older meshes or files that contain only hardpoint/floor data.

### `.msh` import fails (Script Editor / stderr)

1. **Run `setBaseDir`** (or set `TITAN_DATA_ROOT` / `TITAN_EXPORT_ROOT`) so paths in the file and companion `.apt` redirects resolve on disk.
2. Prefer `**importLodMesh -i "appearance/mesh/your_basename"**` (no extension) so the tool can pick `.lod` / `.apt` / `.msh` in a supported order. Opening a raw `.msh` via **File → Import** still works but follows the same IFF parser rules.
3. If you see `**Cannot build mesh: missing vertex/index data`**, the MESH/0005 → SPS block in that asset does not match what this plug-in expects (e.g. no index buffer, or an unsupported primitive layout). Check `**[MshTranslator]`** lines in stderr / the Script Editor history.
4. If a sibling `**.apt**` exists, the importer follows the redirect; ensure the redirected file is present and readable.

### `.mgn` looked gray / no textures

**Shader assignment**: After import, the plug-in resolves each per-shader **template name** from the `.mgn`, runs `**importShader`** for the matching `**.sht`** under your data root, then assigns that shading group to the mesh faces (same idea as `.msh`). If the `.sht` or textures cannot be resolved, you get a **default green** material — fix `**setBaseDir`**, tree layout (`shader/`, `texture/`), and paths inside the shader template.

**UVs**: Texture coordinates are read from **PSDT → TCSF → TCSD** when present.

**TRTS**: Optional **texture renderer template** bindings in `.mgn` / SKMG are parsed (FORM TRTS / CHUNK TRT) and written to the import root transform as `**swgTrtBindings`** (tab-separated lines: template name, shader index, texture tag hex). Shader assignment still comes from per-shader template names and `**importShader`**.

---

## Path Conventions

- **Tree paths**: Use forward slashes, e.g. `appearance/mesh/object`, `shader/foo/bar`
- **Relative paths**: Resolved against data root. Prefix with `appearance/`, `shader/`, `texture/` as needed
- **Absolute paths**: Use as-is (e.g. `D:\data\object.msh`)

