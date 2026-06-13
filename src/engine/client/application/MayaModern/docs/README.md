# SwgMayaEditor documentation

Markdown for the **SwgMayaEditor** Maya 2027 plugin lives in this folder. Read in order the first time; jump to a section when you already know the basics.

---

## Start here (pick one path)

| You want to… | Read this |
|--------------|-----------|
| Build, install, load the plugin | [../README.md](../README.md) |
| **Export a static mesh / sign / prop with textures** | [guide.md — Static mesh export](guide.md#static-mesh-export-swgmsh) |
| Day-to-day MEL commands and translators | [guide.md](guide.md) |
| Deep import/export behavior, coordinates, limitations | [manual.md](manual.md) |
| POB (building) commands at a glance | [../README.md#pob-authoring-buildings--apartments](../README.md#pob-authoring-buildings--apartments) |
| ANS animation IFF layout (technical) | [ans-format.md](ans-format.md) |
| Parity checklist / roadmap | [todo.txt](todo.txt) |

---

## Two different folders (do not confuse them)

| Purpose | How to set | Example |
|---------|----------|---------|
| **Game data** (read stock `.msh`, `.sht`, `.dds` on import) | `TITAN_DATA_ROOT` in `Maya.env` **before** Maya starts | `D:/titan/data/sku.0/sys.client/compiled/game` |
| **Export output** (write new `.msh`, `.apt`, published `.dds`, cloned `.sht`) | `setBaseDir` in Maya | `D:/exported` |

`setBaseDir` configures **write** paths (`appearance/`, `shader/`, `texture/` under that root). Imports still resolve prototypes and stock textures from `TITAN_DATA_ROOT` when needed.

---

## Static mesh export in 30 seconds

```mel
loadPlugin "SwgMayaEditor";
setBaseDir "D:/exported";
select -r myAssetRoot;   // transform root, not a single mesh shape
file -force -options "objExportDirectUv=0;visualHardpoints=0" -typ "SwgMsh" -pr -es "D:/exported/appearance/myAsset.apt";
```

Open **`D:/exported/appearance/myAsset.apt`** in the SWG Viewer (not the `.msh` alone). Copy **`shader/`**, **`texture/`**, and **`appearance/`** together — same tree as `setBaseDir`.

---

## Document index

| File | Contents |
|------|----------|
| [../README.md](../README.md) | Build (CMake / `build-mayamodern.ps1`), install, file-type matrix |
| [guide.md](guide.md) | Commands, `setBaseDir`, static mesh textures/materials, translators, troubleshooting |
| [manual.md](manual.md) | Long-form manual: scene prep, import/export detail, coordinates |
| [ans-format.md](ans-format.md) | KFAT/CKAT chunk reference |
| [todo.txt](todo.txt) | Implementation status and parity gaps |

Companion **MEL** (copied next to `SwgMayaEditor.mll` on build): `swgStaticMeshExport.mel`, `swgMshExportOptions.mel`, `pobAuthoring.mel`, etc.
