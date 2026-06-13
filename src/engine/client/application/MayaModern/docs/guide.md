# SwgMayaEditor — Guide (commands and workflows)

Main operational guide for the **SwgMayaEditor** Maya **2027** plugin: MEL commands, translators, deploy, and workflows (especially static mesh export).

**Other docs in this folder**

- [README.md](README.md) — Documentation index and learning paths  
- [manual.md](manual.md) — Long-form manual (coordinates, scene prep, limitations)  
- [ans-format.md](ans-format.md) — ANS (KFAT/CKAT) IFF layout

---

## Quick usage

1. Build `SwgMayaEditor.mll` ([../README.md](../README.md)), copy `.mll` + `.mel` + `SwgMayaEditor.cfg` to your Maya plug-ins folder, then `loadPlugin SwgMayaEditor`.
2. Set **game data** via `TITAN_DATA_ROOT` in `Maya.env` (stock assets for import).
3. Set **export root** in Maya: `setBaseDir "D:/exported";` then `getDataRootDir;` to verify.
4. Import with **File → Import** (`SwgMsh`, `SwgSat`, …) or MEL import commands below.
5. For static meshes: select the **root transform**, export to `.apt` under `setBaseDir`, open `.apt` in the Viewer.

Unload the plugin before overwriting the `.mll` on Windows (file lock).

---

## Prerequisites

1. **Load the plugin**: `loadPlugin SwgMayaEditor`
2. **Game data root** (imports): set in `Maya.env` before launching Maya:
   ```
   TITAN_DATA_ROOT=D:/titan/data/sku.0/sys.client/compiled/game
   ```
3. **Export root** (writes): in Maya after load:
   ```mel
   setBaseDir "D:/exported";
   ```
   This creates/syncs `D:/exported/appearance/`, `shader/`, `texture/`, `appearance/mesh/`, etc. All published DDS and cloned `.sht` files go here during export.
4. **nvtt**: configure `nvttExporterPath` in `SwgMayaEditor.cfg` (next to the `.mll`). Without nvtt, texture publish fails and the Viewer shows placeholders or old art.
5. **Path resolution on import**: tree paths like `appearance/mesh/foo` resolve under `TITAN_DATA_ROOT`, then `TITAN_EXPORT_ROOT`, then `setBaseDir` / cfg. Absolute paths are used as-is.

---

## Static mesh export (SwgMsh)

This is the workflow for props, signs, furniture, and any **non-skinned** mesh.

### How import lays out the scene (important)

SwgMsh **import** does **not** give you one combined mesh for multi-material assets. Each **shader primitive** in the `.msh` becomes its own **mesh shape** under one **root transform** (siblings under the asset name).

Example after importing a sign:

```
thm_sign_evolve          ← select THIS for export
├── npe_sign_frame       ← mesh shape (metal frame)
├── npe_sign_panel       ← mesh shape (sign face)
└── …                    ← more shapes if more materials
```

**Export combines all mesh shapes under the selected root** back into one `.msh` with multiple shader slots. If you only export a single child shape, you get one material.

### Step-by-step: import → edit texture → export → Viewer

```mel
unloadPlugin "SwgMayaEditor";
loadPlugin "SwgMayaEditor";

// 1) Export root (writes) — not the same as TITAN_DATA_ROOT
setBaseDir "D:/exported";

// 2) Import from game data (TITAN_DATA_ROOT) or from your export tree
file -import -type "SwgMsh" -ignoreVersion -ra true -pr \
  "D:/titan/data/sku.0/sys.client/compiled/game/appearance/thm_sign_medcenter.apt";

// 3) Optional: rename asset tokens on mesh + materials + paths
select -r thm_sign_medcenter;
swgMassRenameAsset -from "npe_sign_medcenter,sign_medcenter,thm_sign_medcenter" -to "thm_sign_evolve" -renameDiskTextures;

// 4) Edit texture in GIMP, assign in Hypershade on the SIGN FACE shading group (file → Lambert.color)
// 5) Export — select ROOT transform
select -r thm_sign_evolve;
file -force -options "objExportDirectUv=0;visualHardpoints=0" -typ "SwgMsh" -pr \
  -es "D:/exported/appearance/thm_sign_evolve.apt";
```

**Viewer:** open `D:/exported/appearance/thm_sign_evolve.apt`. Keep `shader/`, `texture/`, and `appearance/` together under `setBaseDir`.

### What gets written on disk

| Materials | Shader files | Texture files (published DDS) |
|-----------|--------------|-------------------------------|
| 1 slot | `shader/<name>.sht` | `texture/<name>_d.dds` |
| 2+ slots | `shader/<name>_sg0.sht`, `_sg1.sht`, … | `texture/<name>_m0.dds`, `_m1.dds`, … |

`<name>` is the **export filename** (e.g. `thm_sign_evolve`), not the old medcenter material names.

Script Editor should show **one line per material slot**, for example:

```
[ExportStaticMesh] Combining 3 mesh shape(s) under "thm_sign_evolve" -> 3 material slot(s)
[ExportStaticMesh] slot 2 SG "…SG": hypershade D:/…/evolve.tga -> texture/thm_sign_evolve_m2.dds
[ShaderExporter] Published texture/thm_sign_evolve_m2.dds from …
```

If you only see **one** published texture, you exported a single shape or only one material has geometry.

### Where each material slot gets its image (priority)

For each shading group, export resolves the diffuse **in this order**:

1. **Hypershade** — connected `file` / `aiImage` on the surface shader (matches Maya viewport). **This wins.**
2. **Drop-in** in `textureWriteDir` (`setBaseDir`/texture/) — `<name>_m0.tga`, `_m1.tga`, or for slot 0 also `_d.tga` / bare `<name>.tga`.
3. **`swgTexturePath`** on the shadingEngine — only if a source file can be **published** to DDS. Stale tree strings like `texture/npe_sign_medcenter_d.dds` are **not** passed through without baking.

**Common mistake:** an old `.tga` left in `D:/exported/texture/thm_sign_evolve.tga` from before your GIMP edit. Previously drop-ins beat Hypershade; **now Hypershade wins** — but clear stale files if you are unsure.

### UV and winding (Viewer parity)

- **Winding:** automatic (`StaticMeshViewportSpace`); do not use legacy triangle-flip flags.
- **UV in the `.msh` file:** always **legacy 1−V** for the SWG Viewer (same as stock game assets).
- **`objExportDirectUv=0`** in the export dialog: recommended default; affects Maya **re-import** interpretation, not Viewer encoding.
- **`objExportDirectUv=1`:** only when round-tripping old shells in Maya; export still writes legacy 1−V for the Viewer.

### Renaming imported assets (`swgMassRenameAsset`)

Renames tokens in node names, `swgShaderPath`, `swgTexturePath`, `fileTextureName`, and other string attrs on the selection hierarchy + shading networks.

```mel
select -r myAssetRoot;
swgMassRenameAsset -from "npe_sign_medcenter,sign_medcenter" -to "thm_sign_evolve" -renameDiskTextures;
```

| Flag | Meaning |
|------|---------|
| `-from` | One token or comma-separated list (`npe_sign_medcenter,sign_medcenter`) |
| `-to` | New token |
| `-renameDiskTextures` | Rename files in `textureWriteDir` that contain any `-from` token |
| `-keepSwgTexturePath` | Do not auto-clear `swgTexturePath` that still references an old token after replace |
| `-dryRun` | Log only |

MEL wrapper (does not shadow the command name): `swgMassRenameSelectedAsset "from" "to" 1` — third argument `1` = rename disk textures.

Namespaces on materials (`thm_sign_medcenter:fooSG`) do not change export naming; export uses the **output file basename**. Rename still updates string paths inside attrs.

### Validate before export

```mel
select -r myAssetRoot;
swgPrepareStaticMeshExport;   // lists shading groups, swgShaderPath, UV warnings
```

Source `swgStaticMeshExport.mel` for `swgStaticMeshValidateSelection`, effect/transparency helpers, and combine-export shortcuts.

### Shader clone behavior (brief)

On export, each slot gets a **new** `.sht` cloned from:

- `swgShaderPath` on the shadingEngine (prototype layout / effect), if the file exists under game data or export tree, else
- `shader/defaultshader.sht` or `shaderPrototypeSht` in cfg.

The published diffuse DDS is bound to the prototype’s **primary diffuse TXM slot** (MRNC on multi-slot env/spec shaders). Effect overrides: `soe_effectName` on the surface shader; transparency: TGA/PNG triggers alpha-blend clone path.

---

## Build output and deploying (Windows)

- **Release output**: `MayaModern/build/Release/SwgMayaEditor.mll` plus `.mel` scripts copied next to it by the build.
- **Install into Maya** (`…/maya/2027/plugins/`): `scripts/Deploy-ToMayaPlugIns.ps1` or CMake target `swgDeployMayaPlugins` with `SWG_MAYA_PLUGIN_INSTALL_DIR`.
- **Quit Maya** (or `unloadPlugin SwgMayaEditor`) before copying the `.mll`. Reload after deploy.

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

Exports the selected mesh hierarchy to `.msh` + `.apt`. Publishes textures (nvtt → DDS) and clones `.sht` per material slot.

```mel
select -r myAssetRoot;   // root transform — combines all child mesh shapes
exportStaticMesh;
exportStaticMesh -path "D:/exported/appearance/myAsset.apt";
```

| Flag | Description |
| ---- | ----------- |
| `-path` | Output path; `.apt` basename sets export `<name>` for shaders/textures |
| `-objExportDirectUv` | Maya re-import UV semantics only (Viewer always gets legacy 1−V in file) |

**Selection:** Root **transform** of the asset (recommended), or a single mesh shape (single material only).

**Hardpoints:** Child transforms under the mesh root, or `swgAddStaticMeshHardpoint -n gun`. Preview cubes: tag shape with `swgExcludeFromStaticMeshExport=1`.

**Output (under `setBaseDir`):**

- `appearance/mesh/<name>.msh` — combined geometry, one SPS slot per material  
- `appearance/<name>.apt` — redirect for Viewer  
- `shader/<name>.sht` or `shader/<name>_sgN.sht`  
- `texture/<name>_d.dds` or `texture/<name>_mN.dds`

See [Static mesh export (SwgMsh)](guide.md#static-mesh-export-swgmsh) for textures, renaming, and Viewer checks.

### OBJ / Wavefront `.mtl`

Maya’s OBJ importer wires `.mtl` into file textures. `exportStaticMesh` walks those networks and publishes DDS + `.sht` like any other static mesh.

Diffuse images go **straight to DDS** via nvtt (no `.tga` on disk unless `textureMirrorSourceBesideDds=1` in cfg).

**Optional** `swgApplyWavefrontMtl` — force `swgShaderPath` / `swgTexturePath` from a `.mtl` on disk when Maya’s paths are broken after moving files.

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

Reverts skeleton to bind pose; clears animation keys (skips hardpoint locators).

```mel
swgRevertToBindPose;
```

### swgMassRenameAsset

Mass-replace tokens on the selected hierarchy: node names, `swgShaderPath`, `swgTexturePath`, `fileTextureName`, etc. See [Static mesh export — Renaming](guide.md#static-mesh-export-swgmsh).

```mel
swgMassRenameAsset -from "npe_sign_medcenter,sign_medcenter" -to "thm_sign_evolve" -renameDiskTextures;
```

### swgPrepareStaticMeshExport

Validates selection for static mesh export (shading groups, `swgShaderPath`, UVs). Optional `-fixUvSet`, `-combine`.

```mel
swgPrepareStaticMeshExport;
```

### swgAddStaticMeshHardpoint

Adds a hardpoint locator (+ optional preview cube excluded from export).

```mel
select -r myMeshRoot;
swgAddStaticMeshHardpoint -n gun;
```

### swgApplyWavefrontMtl

Optional: parse a `.mtl` and set `swgShaderPath` / `swgTexturePath` on matching shading groups.

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

Create `SwgMayaEditor.cfg` in the **plugin directory** (next to `.mll`) or Maya cwd. Loaded at plugin init (plugin dir if cwd file missing).

```ini
[SwgMayaEditor]
nvttExporterPath = "C:/Program Files/NVIDIA Corporation/NVIDIA Texture Tools/nvtt_export.exe"
gameDataRoot = "D:/titan/data/sku.0/sys.client/compiled/game"
shaderPrototypeSht = "shader/defaultshader.sht"
shaderPrototypeHueableSht = "shader/defaultshader_hueable.sht"
shaderPrototypeTransparentSht = ""
textureMirrorSourceBesideDds = 0
verboseLogging = false
```

`setBaseDir` overrides write dirs (`appearanceWriteDir`, `shaderTemplateWriteDir`, `textureWriteDir`, …) at runtime.

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

### Viewer shows wrong texture or old medcenter art

1. Confirm you opened the **`.apt`** from `setBaseDir`, not an old copy elsewhere.
2. Script Editor: count **`[ExportStaticMesh] slot N`** lines — must match material count. Each should say **`hypershade`** with your GIMP file path if you assigned in Hypershade.
3. Check `getAttr <faceSG>.swgTexturePath` — if it still says `texture/npe_sign_medcenter…`, run `swgMassRenameAsset` or clear it; export no longer passes stale paths without publishing.
4. Delete stale drop-ins in `D:/exported/texture/` (`thm_sign_evolve.tga` from an old export) if you rely on drop-in workflow.
5. Verify `shader/<name>_sgN.sht` and `texture/<name>_mN.dds` exist for multi-material assets.

### Viewer missing sign face / only frame exports

Import creates **multiple mesh shapes**. Select the **root transform** before export. Log should say `Combining N mesh shape(s)`.

### "Shader rebuild failed" / export aborted

- Run `setBaseDir` before export.  
- Ensure `shader/defaultshader.sht` exists under game data or export `shader/`.  
- Read `[ShaderExporter]` lines in Script Editor (nvtt path, prototype `.sht` missing).

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

