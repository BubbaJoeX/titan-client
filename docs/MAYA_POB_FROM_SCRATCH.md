# Authoring a New `.pob` From Scratch (SwgMayaEditor / Maya, File Dialog Workflow)

This guide assumes you work mainly through **File → Import** / **File → Export Selection** and the **Outliner**, not MEL commands. For the **full game pipeline** (`.ilf`, `.tpf`, footprints), see [INTERIOR_STRUCTURE_CREATION.md](./INTERIOR_STRUCTURE_CREATION.md).

Technical reference (translators, env vars, command equivalents): `client/src/engine/client/application/MayaModern/how-to.md`.

---

## One-time setup

1. **Load the plug-in**
  **Windows → Settings/Preferences → Plug-in Manager** → enable **SwgMayaEditor** (and **Auto load** if you want it every session).
2. **Point Maya at your game data tree**
  Imports resolve companions (`.apt` → `.msh`, shaders, textures) from a **root folder** that contains `appearance/`, `shader/`, `texture/`, etc.  
  - Easiest without Script Editor: set a Windows **user or system environment variable** `**TITAN_DATA_ROOT`** (or `**TITAN_EXPORT_ROOT`**) to that root, then restart Maya.  
  - Alternatively, one line in **Script Editor** is `setBaseDir "D:/path/to/root";` — only if you are willing to use that once.

---

## File types in the dialog

In **File → Import** or **File → Export Selection**, open **Files of type** and pick the entry that matches the extension (SwgMayaEditor registers short patterns like `*.pob`, `*.msh *.apt` — the exact label depends on your Maya version).


| You need                 | Extension(s)   | Use                                                                       |
| ------------------------ | -------------- | ------------------------------------------------------------------------- |
| Portal / building layout | `.pob`         | Import or export as SWG portal object (**SwgPob** / `*.pob` in the list). |
| Static appearance / mesh | `.msh`, `.apt` | SWG static mesh (`*.msh *.apt`). `.apt` is often a redirect to a `.msh`.  |


**Import:** **File → Import** → choose type → pick the file on disk.

**Export POB:** Select the **POB root** transform (the parent of `r0`, `r1`, …), then **File → Export Selection** → type `**.pob`** / SWG portal object → choose path and save.

---

## What must exist in the scene (DAG)

The exporter expects a **root** transform whose **direct children** are cells named `**r0`**, `**r1`**, …

Under each `**rN**`:


| Child           | Purpose                                                                                                                     |
| --------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `**mesh**`      | String attribute `**external_reference**` on the transform = tree path the client loads (e.g. `appearance/my_room.apt`).    |
| `**portals**`   | Holds portal door transforms (meshes with portal metadata).                                                                 |
| `**collision**` | Contains `**floor0**` (and optionally more). `**floor0**` also has `**external_reference**` for floor/collision appearance. |


**Viewport-only** placeholder cubes/planes under `mesh` / `floor0` are optional; the game uses the **string paths**, not those meshes.

Portals use attributes such as `**buildingPortalIndex`**, `**portalClockwise`**, `**portalTargetCell**` on the portal transform (visible in Attribute Editor if the plug-in added them). `**portalTargetCell**` = **-1** is typical for doors that do not connect to another cell.

---

## Practical “from scratch” path using only import + editing

1. **Start from a real or minimal `.pob`**
  **File → Import** a `.pob` that is close to what you need (even a small test building from your data). That creates a valid `**r0` / `mesh` / `portals` / `collision` / `floor0`** hierarchy.
2. **Add more cells**
  In the **Outliner**, **duplicate** the `r0` group (or its parent chain), rename duplicates to `**r1`**, `**r2`**, … under the same POB root. Reposition with **Move** tool.
3. **Portals**
  - **Duplicate** an existing portal transform under `**portals`** from the imported file, move/rotate it, then in **Attribute Editor** adjust `**buildingPortalIndex`**, `**portalClockwise`**, `**portalTargetCell**` as needed.  
  - Or use **Script Editor** once for `**addPobPortal`** / `**connectPobCells`** if you need the plug-in to generate the quad and attrs (optional).
4. **Appearance paths**
  Select `**mesh`** or `**floor0`** → **Attribute Editor** → set `**external_reference`** to your tree path (e.g. `appearance/my_room.apt`).
5. **Export**
  Select the **POB root** → **File → Export Selection** → `**.pob`** → save where your pipeline expects (under `appearance/…` in your export or data tree).
6. **Round-trip check**
  **File → Import** your new `.pob` into an empty scene and confirm cells and paths.

---

## Optional: MEL presets (Script Editor)

If you later use **Script Editor**, `pobSingleCellPreset.mel` and `pobAuthoring.mel` can generate layouts faster; they are not required for a file-dialog-only workflow.

---

## Common pitfalls

- **Missing `external_reference`**: The `.pob` can still save, but the client has nothing to load for that cell.  
- **Wrong “Files of type”**: For SWG `**.sat`**, do not pick Maya’s ACIS SAT importer — use the SwgMayaEditor `**.sat`** entry (see kitbash doc).  
- **Data root**: If companions fail to load, fix `**TITAN_DATA_ROOT`** / folder layout so `appearance/`, `shader/`, `texture/` resolve.

---

## See also

- [MAYA_KITBASH_IMPORT_COMBINE.md](./MAYA_KITBASH_IMPORT_COMBINE.md) — mixing `.sat`, `.msh`, `.apt` via the file dialog.  
- [INTERIOR_STRUCTURE_CREATION.md](./INTERIOR_STRUCTURE_CREATION.md) — `.ilf`, templates, footprints.  
- `client/src/engine/client/application/MayaModern/how-to.md` — full translator ids and troubleshooting.

