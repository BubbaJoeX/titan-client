// ======================================================================
//
// SwgMapRasterizer.h
// copyright 2026 
//
// Tool that renders terrain tiles from a top-down orthographic view
// and captures images for use as planet map textures in the UI
// (ui_planetmap.inc). Supports both GPU-rendered mode (full shader
// rendering) and CPU colormap mode (terrain generator sampling).
//
// ======================================================================

#ifndef INCLUDED_SwgMapRasterizer_H
#define INCLUDED_SwgMapRasterizer_H

// ======================================================================

#include <string>
#include <vector>
#include <windows.h>

// ======================================================================

class ObjectListCamera;
class ObjectList;
class TerrainObject;
class ProceduralTerrainAppearanceTemplate;
class TerrainGenerator;
class Image;

// ======================================================================

class SwgMapRasterizer
{
public:

	// ------------------------------------------------------------------
	// Configuration for a rasterization run
	// ------------------------------------------------------------------
	struct Config
	{
		std::string terrainFile;       // e.g. "terrain/tatooine.trn"
		std::string outputDir;         // output directory for images
		int         imageSize;         // output image size in pixels (square)
		int         tilesPerSide;      // number of tiles per side (total = tilesPerSide^2)
		float       cameraHeight;      // camera Y position above terrain
		bool        useColormapMode;   // true = CPU colormap, false = GPU render
		bool        processAll;        // process all known planet terrains
		std::string configFile;        // engine config file path
		bool        antiAlias;        // true = 2x supersample then downsample for sharper edges

		Config();
	};

	// ------------------------------------------------------------------
	// Planet terrain entry for batch processing
	// ------------------------------------------------------------------
	struct PlanetEntry
	{
		const char* terrainFile;       // .trn filename
		const char* outputName;        // output filename base (e.g. "ui_map_tatooine")
	};

	// ------------------------------------------------------------------
	// Main entry point
	// ------------------------------------------------------------------
	static int  run(HINSTANCE hInstance, const char* commandLine);

	// ------------------------------------------------------------------
	// Processing
	// ------------------------------------------------------------------
	static bool rasterizeTerrain(const Config& config, const char* terrainFile, const char* outputName);

private:

	// GPU-rendered mode
	static bool renderTerrainGPU(const Config& config, const char* terrainFile, const char* outputName);
	static bool renderTile(ObjectListCamera* camera, TerrainObject* terrain, 
	                        float mapWidth, int tileX, int tileZ, int tilesPerSide,
	                        float cameraHeight, int tilePixelSize, const char* outputPath);

	// CPU colormap mode
	static bool renderTerrainColormap(const Config& config, const char* terrainFile, const char* outputName);
	bool renderTerrainShading(const Config& config, const char* terrainFile, const char* outputName);
	static bool generateColormapTile(const TerrainGenerator* generator,
	                                  float mapWidth, int imageSize,
	                                  int tileX, int tileZ, int tilesPerSide,
	                                  uint8* imageBuffer, bool legacyMode);

	// Utility
	static Config parseCommandLine(const char* commandLine);
	static void   printUsage();
	static void   printBanner();
	static bool   initializeEngine(HINSTANCE hInstance, const Config& config);
	static void   shutdownEngine();
	static bool   saveTGA(const uint8* pixels, int width, int height, int channels, const char* filename);
	static void   applyHillshading(const float* heightMap, uint8* colorPixels, int width, int height, float mapWidth);
	// Downsample 2x (4 pixels -> 1) for anti-aliased output; src is 2*outWidth x 2*outHeight, RGB
	static void   downsample2xRGB(const uint8* src, int srcWidth, int srcHeight, uint8* out, int outWidth, int outHeight);

	// Planet list for batch processing
	static const PlanetEntry s_planetEntries[];
	static const int         s_numPlanetEntries;

	// State
	static bool              s_engineInitialized;
	static bool              s_graphicsInitialized;
	static HINSTANCE         s_hInstance;

private:

	SwgMapRasterizer();
	SwgMapRasterizer(const SwgMapRasterizer&);
	SwgMapRasterizer& operator=(const SwgMapRasterizer&);
};

// ======================================================================

#endif
