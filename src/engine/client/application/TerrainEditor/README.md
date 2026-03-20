# TerrainEditor

MFC tool for authoring **procedural terrain** (`.trn` and related data). It drives the same **`TerrainGenerator`** stack as the game client.

## Resolution = pole spacing (not pole count)

Per-chunk preview uses a fixed number of **poles** along each axis: **`16 + originOffset + upperPad`** (defaults: `originOffset = 1`, `upperPad = 2` → **19** poles). What changes with the map **resolution** is **how many meters lie between adjacent poles** and the **chunk width in world space**:

| Mode | Chunk width | Distance between poles |
|------|-------------|-------------------------|
| **Very High** | 8 m | 0.5 m |
| **High** | 16 m | 1 m |
| **Medium** | 32 m | 2 m |
| **Low** | 64 m | 4 m |

**Very High** is available from the main menu: **View → Map Very High Resolution (0.5m poles)**. Toolbar buttons remain Low / Medium / High only (no fourth icon strip).

Implementation: `EditorTerrain::setResolutionType` in `src/win32/EditorTerrain.cpp`. Default after construction is **Medium** (`setResolutionType(RT_medium)`).

Use **Very High** or **High** when you need **dense** editor sampling for painting, rivers, or debugging generator output. Zoom in the map view still scales `distanceBetweenPoles` and chunk width by zoom (`createChunk`).

## Generator

Chunk builds call **`TerrainGenerator::generateChunk`** with **`TerrainGenerator::GeneratorChunkData`** populated from the document’s **`TerrainGenerator`** (shaders, fractals, flora, etc.). Saving your work updates the **same procedural assets** the client loads.

## Full-planet 2D map images

For a **large top-down image** of an entire planet from a `.trn` (UI maps, etc.), use **`SwgMapRasterizer`** — see `client/src/engine/shared/application/SwgMapRasterizer/README.md`. It supports **`-size`** up to **16384** and a wider **`-tiles`** range than before.
