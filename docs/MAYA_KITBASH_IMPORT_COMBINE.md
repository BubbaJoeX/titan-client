# Kitbashing in Maya: Import, Pose, and Combine (File Dialog Workflow)

This guide assumes you use **File → Import** / **File → Export Selection** and standard Maya modeling menus (**Mesh**, **Outliner**, **Attribute Editor**), not MEL. Command equivalents live in `client/src/engine/client/application/MayaModern/how-to.md`.

---

## One-time setup

1. **Plug-in Manager** → load **SwgMayaEditor**.
2. **Data root**: Set Windows environment variable `**TITAN_DATA_ROOT`** (or `**TITAN_EXPORT_ROOT`**) to the folder that contains `**appearance/`**, `**shader/**`, `**texture/**`, then restart Maya — so imports find redirects and shaders without typing `**setBaseDir**`.

---

## Choosing the right importer in File → Import

In **Files of type**, pick the SwgMayaEditor entry for your extension. Do **not** use Maya’s **SAT_ATF** / ACIS importer for Star Wars Galaxies `**.sat`** files — those are **skeletal appearance templates** and use the SWG `**.sat`** type (e.g. **SwgSat** / `*.sat`).


| Asset                                              | Extension(s)   | File → Import                                           |
| -------------------------------------------------- | -------------- | ------------------------------------------------------- |
| Skeletal appearance (rig + meshes + hardpoints)    | `.sat`         | SWG `.sat` (not ACIS)                                   |
| Static mesh or APT redirect                        | `.msh`, `.apt` | SWG static mesh (`*.msh` / `*.apt` pattern in the list) |
| Skeletal mesh (skin)                               | `.mgn`         | SWG `*.mgn`                                             |
| Portal / building                                  | `.pob`         | SWG `*.pob`                                             |
| Floor                                              | `.flr`         | SWG `*.flr`                                             |
| Shader (for editing; DDS may convert for viewport) | `.dds`         | SWG `*.dds` (see plug-in docs)                          |


Import each kitbash piece **from disk**; use Maya’s import options (e.g. **namespace**, **group**) if names clash.

---

## Pattern A: Character (`.sat`) + prop (`.msh` / `.apt`)

1. **File → Import** the `**.sat`**. You get joints, meshes, and **hardpoint** transforms.
2. **File → Import** the prop (`.msh` or `.apt`).
3. In the **Outliner**, **parent** the prop’s top transform under the correct **hardpoint** (or hand joint).
4. **Pose** with **Rotate** on joints; use **Modify → Freeze Transformations** on the prop if needed.
5. **Materials**: If the prop is flat gray, your `**shader/`** / `**texture/`** tree under the data root may be wrong, or the mesh needs a material assigned after shader files resolve.

**Export note:** SwgMayaEditor does **not** export a new `**.sat`** from Maya; use this workflow for layout, stills, or to prepare **static** geometry for `**.msh`** export (Pattern B).

---

## Pattern B: Several static pieces → one mesh (kitbash chunk)

1. **File → Import** each `.msh` / `.apt` (or duplicate pieces inside Maya).
2. **Move / rotate** pieces into position; optional **group** under one transform.
3. **Select all polygon meshes** to merge → **Mesh → Combine** (Maya built-in).
4. Clean up **UVs** and **materials** on the combined mesh (multiple shaders may need reassignment).
5. Assign materials, then **File → Export Selection** → choose SWG `**.msh`** / `*.msh *.apt` → save.

The plug-in also offers `**swgPrepareStaticMeshExport -combine -fixUvSet`** (Script Editor) for **polyUnite** plus UV-set repair; if you stay file-dialog-only, **Mesh → Combine** is the parallel step, but you may need to fix `**map1`** / current UV set yourself before export.

---

## Pattern C: SAT + props without combining

Parent props under joints or groups; do **not** use **Combine** if you need separate materials, LOD-style parts, or a rig you can still pose.

---

## Pattern D: POB references your kitbash

1. Export your kitbash as `**.msh` / `.apt`** (Pattern B) into your `**appearance/`** tree.
2. Author or import a `**.pob`** ([MAYA_POB_FROM_SCRATCH.md](./MAYA_POB_FROM_SCRATCH.md)).
3. On `**rN|mesh**`, set `**external_reference**` in **Attribute Editor** to `**appearance/your_kitbash.apt`** (or the path your tree uses).

---

## Export static mesh from the file dialog

Select the **mesh transform** (or the combined mesh), then **File → Export Selection** → **Files of type** → SWG static mesh (`*.msh` / `*.apt`) → pick output path. The plug-in runs the same path as `**exportStaticMesh`** internally.

Ensure the mesh has **materials / shading engines** assigned or export may fail (see how-to troubleshooting).

---

## Limitations

- `**.sat` / `.mgn` / `.ans` binary export** is not the focus of SwgMayaEditor; plan kitbash **delivery** around `**.msh`** unless you use a legacy exporter.  
- `**.dds` → TGA** and shader round-trips may still need the paths and cfg described in **how-to.md**.

---

## See also

- [MAYA_POB_FROM_SCRATCH.md](./MAYA_POB_FROM_SCRATCH.md)  
- [INTERIOR_STRUCTURE_CREATION.md](./INTERIOR_STRUCTURE_CREATION.md)  
- `client/src/engine/client/application/MayaModern/how-to.md`

