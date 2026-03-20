# SwgMayaEditor How-To: Commands and Workflows

This guide explains how to use the MEL commands and file translators in the SwgMayaEditor plugin for Maya 2026.

---

## Prerequisites

1. **Load the plugin**: `loadPlugin SwgMayaEditor`
2. **Set base directory** (required for most operations):
   ```mel
   setBaseDir "D:\\exported";
   ```
   This configures output directories under the given path (appearance, shader, texture, animation, skeleton, mesh, log).

3. **Path resolution**: Import commands resolve relative paths using:
   - `TITAN_DATA_ROOT` or `TITAN_EXPORT_ROOT` environment variables, or
   - The directory set by `setBaseDir` (stored as `dataRootDir`)

   Paths like `appearance/foo/bar` are resolved relative to the base. Absolute paths are used as-is.

---

## Setup Commands

### setBaseDir

Configures all export/import directories under a base path. **Run this first** before importing or exporting.

```mel
setBaseDir "D:\\exported";
```

Creates and configures:
- `appearance\`, `shader\`, `texture\`, `animation\`, `skeleton\`, `mesh\`, `log\`
- Reference prefixes for tree paths (e.g. `appearance/`, `shader/`, `texture/`)

### getDataRootDir

Returns the currently configured data root directory.

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

| Flag | Description |
|------|-------------|
| `-i` | Input path (required). Tree path or absolute path. |
| `-parent` | Optional. DAG path of parent transform. |

### importAnimation

Imports an animation (.ans) via Maya's File > Import.

```mel
importAnimation -i "appearance/animation/humanoid/combat/dance.ans";
```

| Flag | Description |
|------|-------------|
| `-i` | Input path (required). |

### importLodMesh

Imports a LOD mesh, APT redirect, or DTLA/MLOD. Resolves .lod, .apt, or .msh automatically.

```mel
importLodMesh -i "appearance/mesh/object_lod0";
importLodMesh -i "appearance/object" -parent "|group1";
```

| Flag | Description |
|------|-------------|
| `-i` | Input path (required). Tries .lod, .apt, .msh in that order. |
| `-parent` | Optional. DAG path of parent transform. |

### importSkeletalMesh

Imports a skeletal mesh (.mgn) with skin weights, blend shapes, and hardpoints.

```mel
importSkeletalMesh -i "appearance/mesh/character_lod0" -s "appearance/skeleton/humanoid/humanoid";
importSkeletalMesh -i "character.mgn" -s "humanoid.skt" -parent "|root";
```

| Flag | Description |
|------|-------------|
| `-i` | Input mesh path (required). |
| `-s` | Skeleton path (required). |
| `-parent` | Optional. Parent transform. |

### importStaticMesh

Wrapper that calls `importLodMesh` with the given path. Use for static meshes, APT redirects, or LODs.

```mel
importStaticMesh -i "appearance/mesh/object";
importStaticMesh -i "object" -parent "|group1";
```

| Flag | Description |
|------|-------------|
| `-i` | Input path (required). |
| `-parent` | Optional. Parent transform. |

### importShader

Imports a shader template (.sht). Converts DDS textures to TGA for Maya editing.

```mel
importShader -i "shader/foo/bar";
```

| Flag | Description |
|------|-------------|
| `-i` | Input shader path (required). |

### exportShader

Exports a shader template (.sht) to `shaderTemplateWriteDir`, converting edited TGA textures in `textureWriteDir` to DDS and updating texture paths in the shader (same pipeline as `exportStaticMesh` shader pass).

```mel
setBaseDir "D:\\exported";
exportShader -i "shader/foo/bar";
exportShader -path "shader/foo/bar";
```

| Flag | Description |
|------|-------------|
| `-i` / `-path` | Shader tree path (required), e.g. `shader/foo/bar` (with or without `.sht`). |

**Prerequisites**: `setBaseDir` (or configured `shaderTemplateWriteDir` / `textureWriteDir`). Source shader is resolved like imports (data root / `TITAN_DATA_ROOT`).

### importSat

Imports a skeletal appearance template (.sat). Loads skeleton, LOD meshes, and appearance hierarchy.

```mel
importSat -i "appearance/character/sat_name";
```

| Flag | Description |
|------|-------------|
| `-i` | Input SAT path (required). |

### importPob

Imports a portal object (.pob) with cells, portals, and appearance references.

```mel
importPob -i "appearance/building/cantina";
```

| Flag | Description |
|------|-------------|
| `-i` | Input POB path (required). |

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

| Flag | Description |
|------|-------------|
| `-bp` | Bind pose frame number (default: -10). |
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

| Flag | Description |
|------|-------------|
| `-path` | Optional. Output path. Default: `appearanceWriteDir/mesh/<name>.msh` |
| `-name` | Optional. Override mesh name. |

**Selection**: Select a mesh (or its transform).

**Output**:
- `.msh` – MESH/0005 with geometry, hardpoints, shader groups
- `.apt` – Redirect to mesh (in `appearanceWriteDir`)
- `.sht` – Shaders with TGA→DDS conversion (in `shaderTemplateWriteDir`)

### exportPob

Exports the selected POB hierarchy to .pob.

```mel
select -r pobRoot;
exportPob -i "appearance/building/cantina";
```

| Flag | Description |
|------|-------------|
| `-i` | Output path (required). |

**Selection**: Select the POB root or a cell.

### Skeletal mesh, animation, and SAT export (status)

| Goal | SwgMayaEditor | Notes |
|------|----------------|--------|
| `.mgn` (SKMG) | **Import** supported; **File > Export .mgn** writer is not yet implemented | Full generator export lives in the legacy **MayaExporter** (`exportSkeletalMeshGenerator`) and depends on a large shared export stack. |
| `.ans` (keyframe anim) | **Import** supported; **File > Export .ans** writer is not yet implemented | Legacy **MayaExporter** `exportKeyframeSkeletalAnimation`. |
| `.sat` | **Import** via `importSat` | **Export** not implemented in SwgMayaEditor (legacy MayaExporter `exportSatFile` / appearance template pipeline). |

Use **exportShader** and **exportStaticMesh** for shader round-trip; for skeletal mesh/animation/SAT binary export, use the legacy exporter build or port those sources into this project (see `todo.txt`).

---

## File Translators (File > Import / Export)

Use Maya's **File > Import** or **File > Export** with these file types:

| Extension | Translator | Description |
|-----------|------------|-------------|
| .msh | msh | Static mesh. **Export**: Select mesh first, then File > Export. |
| .mgn | mgn | Skeletal mesh (SKMG). |
| .skt | skt | Skeleton template. |
| .ans | ans | Animation. |
| .flr | flr | Floor mesh. |
| .sat | sat | Skeletal appearance template. |
| .pob | pob | Portal object. |
| .dds | dds | DDS texture (converts to TGA for editing). |

**MESH Export**: Select a mesh, then File > Export and choose a .msh path. The msh translator calls `exportStaticMesh` internally.

---

## Utility Commands

### swgRevertToBindPose

Reverts the scene to bind pose. Useful before exporting skeletons or when animations are applied.

```mel
swgRevertToBindPose;
```

### swgAssetDissector

Opens the asset dissector UI (if `swgAssetDissector.mel` is available).

```mel
swgAssetDissector;
```

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

## Troubleshooting

### "No SPS form - importing APPR only"

When importing a `.msh` file without SPS (Shader Primitive Set) geometry, you may see:

```
[MshTranslator]   No SPS form - importing APPR only (hardpoints, floor)
```

**Behavior**: The importer creates the root transform with hardpoints and floor reference from the APPR form. No mesh geometry is created. This supports older meshes or files that contain only hardpoint/floor data.

---

## Path Conventions

- **Tree paths**: Use forward slashes, e.g. `appearance/mesh/object`, `shader/foo/bar`
- **Relative paths**: Resolved against data root. Prefix with `appearance/`, `shader/`, `texture/` as needed
- **Absolute paths**: Use as-is (e.g. `D:\data\object.msh`)
