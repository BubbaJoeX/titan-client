# SwgMapRasterizer

Offline tool that builds **top-down planet map images** from procedural **`.trn`** files (same **`TerrainGenerator`** path as the game). **CPU colormap** mode samples height/color and hillshades; **GPU** mode uses the client terrain renderer.

## Resolution

| Setting | Default | Max | Notes |
|---------|---------|-----|--------|
| **`-size`** (square pixels) | **2048** | **16384** | Larger = finer meters/pixel over the planet width. |
| **`-tiles`** (grid per side) | **4** | **128** | Must divide **`-size`** evenly (tool adjusts **`-size`** down if needed). |

**4K / high resolution (colormap):** With **`-aa`** (default), **`-size 4096`** uses an **8192²** internal buffer. The tool **automatically increases `-tiles`** if needed so each terrain chunk stays around **≤1024 samples per side** (e.g. 4→**8** when you’d otherwise get 2048 px/tile). You can set **`-tiles 8`** or higher up front; higher = more tiles, smaller chunks, more `generateChunk` calls.

**Anti-aliasing** (`-aa`, default in colormap mode): uses a **2×** internal buffer. For **`-size` &gt; 8192**, colormap mode **auto-disables AA** (use **`-noaa`** to avoid the message). For very large outputs, prefer **`-noaa`** and rely on pixel count alone.

**Textures:** Terrain references often use **`.tga`** paths, but shipped assets are usually **`.dds`** (DXT or uncompressed). The tool loads **`.dds`** first (same basename), using **ATI_Compress** to decompress DXT1/DXT3/DXT5 to CPU pixels for sampling.

## Terrain shader families (colormap)

For each **`ShaderGroup`** family, the tool loads textures the **same way** for the **family** and for **each child**:

1. **Family** (e.g. grass, sand, `main_area`): treat **`familyName`** like a shader basename — open **`shader/<familyName>.sht`** when needed, scan IFF for **`texture/...`**, prefer **`.dds`**, then **`texture/<basename>.dds`** / **`.tga`** if the resolved path does not load.
2. **Children**: same rules using each child’s **`shaderTemplateName`** (often a short name or full **`shader/....sht`** path).

Per pixel, the **child** is chosen with **`ShaderGroup::createShader`** from the **`shaderMap`**. If the map is still the **default family (0)** after generation, the tool also tries **`ShaderGroup::getFamilyId(PackedRgb)`** on **`colorMap`** (when color affectors tagged family tint). **Engine notes:** **`ShaderGroup::chooseShader`** stores **family id** whenever the family exists (including **zero children**). **`TerrainGenerator::GeneratorChunkData::skipShaderSynchronize`** is set by this tool so **`synchronizeShaders`** does not run: that pass resamples the shader map on a coarser pole grid and often **replaces** stamped families with the **default** shader, which reads as flat grey in a full-resolution colormap. The diffuse image is whatever **`buildShaderLayers`** preloaded, or—**same as water**—**lazy-loaded** through **`loadTextureImageForShaderTemplateName`** (`shader/<name>.sht`, `texture/<basename>.dds`, etc.) and cached for the run. **UVs match global water tiling:** **`u = -worldX / shaderSize`**, **`v = worldZ / shaderSize`**, bilinear + wrap. When a diffuse loads, the pixel uses a **texture-forward** mix with the generator colormap (**~78%** texture) so dark game albedos are not crushed to black by **hillshading**. Hillshade uses a **higher ambient** and a **soft floor** on N·L so steep terrain stays readable.

## Water (colormap / shading)

After hillshading, the tool paints **underwater** pixels using the same **layered** surface resolution as the game (`getWaterSurfaceAt`: global baseline, **higher** local boundary tables replace it, then ribbon geometry on top).

- **Global** water table, **local** tables (rectangle/polygon boundaries and their shader fields), and **ribbon** quads / **end caps** (`getWaterSurfaceAt` on `ProceduralTerrainAppearanceTemplate`).
- **Tint:** for each water **shader** (often stored as a short name, e.g. `wter_nboo_beach`), the tool opens **`shader/<name>.sht`** when the string is not already a `.sht` path, scans IFF for **`texture/...`**, loads with **`.dds`** preferred, then on failure tries **`texture/<basename>.dds`** / **`.tga`**. Sampling is **bilinear**, UV **wrap**, same base tiling as the client: **`u = -worldX / shaderSize`**, **`v = worldZ / shaderSize`**. A **zero/small** authored shader size clamps to **2 m** where needed. If a **local/boundary** template still fails, the tool retries the **planet global** water shader (when enabled) with the same rules. **Lava** without a shader path keeps the solid orange tint so it is not replaced by the global water texture.
- **Local water tables** (polygon/rectangle) often have no per-boundary shader in the `.trn`; **`getWaterSurfaceAt`** already selects the **planet global water shader** for naming; the rasterizer now **also** reloads that global **`.dds`** when a boundary’s `.sht` does not yield a file on disk.

**Shader ramps:** along-gradient color uses the same **bilinear** path as terrain/water DDS (not a single ramp texel).

Examples:

```text
SwgMapRasterizer.exe -terrain terrain/tatooine.trn -size 4096 -tiles 8
SwgMapRasterizer.exe -terrain terrain/naboo.trn -size 16384 -noaa -colormap
```

See **`SwgMapRasterizer::printUsage`** in `src/shared/SwgMapRasterizer.cpp` for all switches.

## TerrainEditor (authoring)

For **finer in-editor chunk preview** (closer pole spacing), use **View → Map Very High Resolution (0.5m poles)** or see **`EditorTerrain::RT_very_high`** in `client/src/engine/client/application/TerrainEditor`.
