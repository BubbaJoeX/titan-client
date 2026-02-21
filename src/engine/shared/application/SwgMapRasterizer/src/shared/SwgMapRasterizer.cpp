// ======================================================================
//
// SwgMapRasterizer.cpp
// copyright 2026 
//
// Terrain map rasterization tool. Generates top-down planet map images
// from .trn terrain files for use in the UI (ui_planetmap.inc).
//
// Supports two modes:
//   - GPU Rendered Mode: Full client terrain rendering with shaders
//     and textures, captured via orthographic camera screenshots.
//   - CPU Colormap Mode: Uses the terrain generator to sample height
//     and color data, then applies hillshading for a 3D terrain map.
//
// ======================================================================

#include "FirstSwgMapRasterizer.h"
#include "SwgMapRasterizer.h"

// -- Engine shared includes --
#include "sharedCompression/SetupSharedCompression.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFile/SetupSharedFile.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/SetupSharedFoundation.h"
#include "sharedImage/SetupSharedImage.h"
#include "sharedImage/Image.h"
#include "sharedImage/ImageFormatList.h"
#include "sharedImage/TargaFormat.h"
#include "sharedMath/PackedRgb.h"
#include "sharedMath/SetupSharedMath.h"
#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplate.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/ObjectList.h"
#include "sharedObject/SetupSharedObject.h"
#include "sharedRandom/SetupSharedRandom.h"
#include "sharedRegex/SetupSharedRegex.h"
#include "sharedTerrain/ProceduralTerrainAppearanceTemplate.h"
#include "sharedTerrain/SetupSharedTerrain.h"
#include "sharedTerrain/TerrainGenerator.h"
#include "sharedTerrain/TerrainObject.h"
#include "sharedThread/RunThread.h"
#include "sharedThread/SetupSharedThread.h"
#include "sharedThread/ThreadHandle.h"
#include "sharedUtility/SetupSharedUtility.h"

// -- Engine client includes (for GPU rendering mode) --
#include "clientGraphics/Camera.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/ConfigClientGraphics.h"
#include "clientGraphics/ScreenShotHelper.h"
#include "clientGraphics/SetupClientGraphics.h"
#include "clientObject/ObjectListCamera.h"
#include "clientObject/SetupClientObject.h"
#include "clientTerrain/SetupClientTerrain.h"

// -- Standard library --
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <direct.h>
#include <map>


// ======================================================================
// Static member initialization
// ======================================================================

bool      SwgMapRasterizer::s_engineInitialized    = false;
bool      SwgMapRasterizer::s_graphicsInitialized  = false;
HINSTANCE SwgMapRasterizer::s_hInstance             = NULL;


//
namespace
{
	std::map<std::string, PackedRgb> s_shaderFamilyColorCache;
	// Loaded ramp images per family - avoid loading the same file millions of times per map
	std::map<std::string, Image*> s_shaderRampImageCache;

	// ======================================================================
	// Helper: Read a single pixel from an image
	// ======================================================================
	static PackedRgb readPixelFromImage(const Image* image, int x, int y)
	{
		PackedRgb result = PackedRgb::solidBlack;

		if (!image || x < 0 || x >= image->getWidth() || y < 0 || y >= image->getHeight())
			return result;

		const uint8* data = image->lockReadOnly();
		if (!data)
			return result;

		// Move to the pixel location
		data += y * image->getStride() + x * image->getBytesPerPixel();

		// Handle different pixel formats
		if (image->getPixelFormat() == Image::PF_bgr_888)
		{
			result.b = *data++;
			result.g = *data++;
			result.r = *data++;
		}
		else if (image->getPixelFormat() == Image::PF_rgb_888)
		{
			result.r = *data++;
			result.g = *data++;
			result.b = *data++;
		}
		else
		{
			// For other formats, try to extract RGB components
			result.r = *data++;
			if (image->getBytesPerPixel() > 1)
				result.g = *data++;
			if (image->getBytesPerPixel() > 2)
				result.b = *data++;
		}

		image->unlock();

		return result;
	}

	// ======================================================================
	// Get color for a shader family from the terrain file.
	// Uses color ramp image if present, otherwise terrain-defined family color.
	// childChoice (0..1) selects position along the ramp for shader variant.
	// ======================================================================
	static PackedRgb getShaderFamilyColor(
		const char* familyName,
		const PackedRgb& terrainFallback,
		float childChoice = 0.5f
	)
	{
		if (!familyName || !familyName[0])
			return terrainFallback;

		// Use cached ramp image if already loaded (avoids loading the same file per pixel)
		{
			auto rampIt = s_shaderRampImageCache.find(familyName);
			if (rampIt != s_shaderRampImageCache.end() && rampIt->second)
			{
				Image* image = rampIt->second;
				const int w = image->getWidth();
				const int h = image->getHeight();
				if (w > 0 && h > 0)
				{
					const int sampleX = std::max(0, std::min(w - 1, static_cast<int>(childChoice * (w - 1) + 0.5f)));
					const int sampleY = h / 2;
					return readPixelFromImage(image, sampleX, sampleY);
				}
			}
		}

		// Color ramps: try terrain/colorramp/<name>.tga then terrain/<name>.tga
		char rampPath[256];
		TargaFormat tgaFormat;
		Image* image = NULL;

		_snprintf(rampPath, sizeof(rampPath), "terrain/colorramp/%s.tga", familyName);
		if (!tgaFormat.loadImage(rampPath, &image))
		{
			if (image) { delete image; image = NULL; }
			_snprintf(rampPath, sizeof(rampPath), "terrain/%s.tga", familyName);
			if (!tgaFormat.loadImage(rampPath, &image))
			{
				if (image)
					delete image;
				// Use terrain-defined color from .trn file when no ramp; cache it
				s_shaderFamilyColorCache[familyName] = terrainFallback;
				return terrainFallback;
			}
		}

		// Cache the loaded ramp image so we never load it again for this run
		s_shaderRampImageCache[familyName] = image;

		// Sample the ramp: childChoice (0..1) picks position along the gradient
		const int w = image->getWidth();
		const int h = image->getHeight();
		const int sampleX = std::max(0, std::min(w - 1, static_cast<int>(childChoice * (w - 1) + 0.5f)));
		const int sampleY = h / 2;

		return readPixelFromImage(image, sampleX, sampleY);
	}

	// Call at end of render to free cached ramp images and avoid leaking / stale data
	static void clearShaderRampImageCache()
	{
		for (auto it = s_shaderRampImageCache.begin(); it != s_shaderRampImageCache.end(); ++it)
		{
			if (it->second)
				delete it->second;
		}
		s_shaderRampImageCache.clear();
	}

	// ======================================================================
	// Blend two colors with a given weight
	// ======================================================================
	static PackedRgb blendColors(const PackedRgb& base, const PackedRgb& layer, float layerWeight)
	{
		float baseWeight = 1.0f - layerWeight;
		
		PackedRgb result;
		result.r = static_cast<uint8>(base.r * baseWeight + layer.r * layerWeight);
		result.g = static_cast<uint8>(base.g * baseWeight + layer.g * layerWeight);
		result.b = static_cast<uint8>(base.b * baseWeight + layer.b * layerWeight);
		
		return result;
	}

	// ======================================================================
	// Shader Layer: represents a single shader family and its properties
	// Includes optional texture for this family (from terrain file convention)
	// ======================================================================
	struct ShaderLayer
	{
		int familyId;
		std::string familyName;
		PackedRgb familyColor;
		int priority;
		float layerWeight;
		float shaderSize;       // meters - used for texture UV tiling
		Image* familyTexture;   // loaded texture for this family, or NULL

		ShaderLayer() : familyId(0), priority(0), layerWeight(0.0f), shaderSize(2.0f), familyTexture(0) {}
	};

	// ======================================================================
	// Sample a texture at UV (0..1) with wrap; returns RGB
	// ======================================================================
	static PackedRgb sampleTextureAtUV(const Image* image, float u, float v)
	{
		if (!image || image->getWidth() <= 0 || image->getHeight() <= 0)
			return PackedRgb::solidBlack;

		// Wrap UV to [0,1)
		u = u - floorf(u);
		v = v - floorf(v);
		if (u < 0.0f) u += 1.0f;
		if (v < 0.0f) v += 1.0f;

		const int w = image->getWidth();
		const int h = image->getHeight();
		const int x = std::max(0, std::min(w - 1, static_cast<int>(u * w)));
		const int y = std::max(0, std::min(h - 1, static_cast<int>(v * h)));

		return readPixelFromImage(image, x, y);
	}

	// ======================================================================
	// Find shader layer by family ID (for texture and shader size lookup)
	// ======================================================================
	static const ShaderLayer* getLayerForFamily(int familyId, const std::vector<ShaderLayer>& layers)
	{
		for (const auto& layer : layers)
		{
			if (layer.familyId == familyId)
				return &layer;
		}
		return 0;
	}

	// ======================================================================
	// Get terrain-defined color for a shader family (from .trn ShaderGroup)
	// ======================================================================
	static PackedRgb getTerrainShaderColor(
		int familyId,
		const std::vector<ShaderLayer>& layers,
		const PackedRgb& generatorColor
	)
	{
		for (const auto& layer : layers)
		{
			if (layer.familyId == familyId)
				return layer.familyColor;
		}
		return generatorColor;
	}
}
// ======================================================================
// Planet table: terrain file -> output name mapping
// Maps .trn files to the UI resource names used in ui_planetmap.inc
// ======================================================================

const SwgMapRasterizer::PlanetEntry SwgMapRasterizer::s_planetEntries[] =
{
	{ "terrain/corellia.trn",             "ui_map_corellia"                       },
	{ "terrain/dantooine.trn",            "ui_map_dantooine"                      },
	{ "terrain/dathomir.trn",             "ui_map_dathomir"                       },
	{ "terrain/endor.trn",                "ui_map_endor"                          },
	{ "terrain/lok.trn",                  "ui_map_lok"                            },
	{ "terrain/naboo.trn",                "ui_map_naboo"                          },
	{ "terrain/rori.trn",                 "ui_map_rori"                           },
	{ "terrain/talus.trn",                "ui_map_talus"                          },
	{ "terrain/tatooine.trn",             "ui_map_tatooine"                       },
	{ "terrain/yavin.trn",                "ui_map_yavin4"                         },
	{ "terrain/taanab.trn",               "ui_map_taanab"                         },
	{ "terrain/chandrila.trn",            "ui_map_chandrila"                      },
	{ "terrain/hoth.trn",                 "ui_map_hoth"                           },
	{ "terrain/jakku.trn",                "ui_map_jakku"                          },


	/* Only full-gusto main planets for now, as the smaller sub-planet are not guaranteed to have their terrain files available in the TREs, and may require special handling for their smaller map sizes and different shader groups. Can add these back in later if needed.
	
	{"terrain/mustafar.trn",             "ui_map_mustafar"},
	{ "terrain/kashyyyk_main.trn",        "ui_map_kashyyyk_main"                  },
	{ "terrain/kashyyyk_dead_forest.trn", "ui_map_kashyyyk_dead_forest"           },
	{ "terrain/kashyyyk_hunting.trn",     "ui_map_kashyyyk_hunting"               },
	{ "terrain/kashyyyk_rryatt_trail.trn","ui_map_kashyyyk_rryatt_trail"          },
	{ "terrain/kashyyyk_north_dungeons.trn","ui_map_kashyyyk_north_dungeons_slaver"},
	{ "terrain/kashyyyk_south_dungeons.trn","ui_map_kashyyyk_south_dungeons_bocctyyy"},*/
};

const int SwgMapRasterizer::s_numPlanetEntries = sizeof(s_planetEntries) / sizeof(s_planetEntries[0]);

// ======================================================================
// Config defaults
// ======================================================================

SwgMapRasterizer::Config::Config() :
	terrainFile(""),
	outputDir("./maps"),
	imageSize(1024),
	tilesPerSide(4),
	cameraHeight(20000.0f),
	useColormapMode(true),
	processAll(false),
	configFile("titan.cfg"),
	antiAlias(true)
{
}

// ======================================================================
// Banner and usage
// ======================================================================

void SwgMapRasterizer::printBanner()
{
	printf("========================================================\n");
	printf("  SwgMapRasterizer - Planet Map Image Generator\n");
	printf("  Generates top-down terrain map images for the UI\n");
	printf("========================================================\n\n");
}

// ----------------------------------------------------------------------

void SwgMapRasterizer::printUsage()
{
	printf("Usage: SwgMapRasterizer.exe [options]\n\n");
	printf("Options:\n");
	printf("  -terrain <file.trn>   Terrain file to rasterize\n");
	printf("  -output <dir>         Output directory (default: ./maps)\n");
	printf("  -size <pixels>        Output image size, square (default: 1024)\n");
	printf("  -tiles <n>            Tile grid divisions per side (default: 4)\n");
	printf("  -height <meters>      Camera height for GPU mode (default: 20000)\n");
	printf("  -colormap             Use CPU colormap mode (default)\n");
	printf("  -render               Use GPU rendered mode\n");
	printf("  -all                  Process all known planet terrains\n");
	printf("  -config <file.cfg>    Engine config file (default: client.cfg)\n");
	printf("  -aa                   Enable anti-aliasing (2x supersample, default)\n");
	printf("  -noaa                 Disable anti-aliasing\n");
	printf("  -help                 Show this help message\n");
	printf("\n");
	printf("\nExamples:\n");
	printf("  SwgMapRasterizer.exe -terrain terrain/tatooine.trn\n");
	printf("  SwgMapRasterizer.exe -all -size 2048\n");
	printf("  SwgMapRasterizer.exe -terrain terrain/naboo.trn -render\n");
	printf("\nKnown planets:\n");
	for (int i = 0; i < s_numPlanetEntries; ++i)
	{
		printf("  %-40s -> %s\n", s_planetEntries[i].terrainFile, s_planetEntries[i].outputName);
	}
	printf("Once generated, make a new .dds with dxtex.exe and update ui_planetmap.inc with the new filename and dimensions.\n");
	printf("Automation: Disabled");
	exit(0);
}

// ======================================================================
// Command line parsing
// ======================================================================

SwgMapRasterizer::Config SwgMapRasterizer::parseCommandLine(const char* commandLine)
{
	Config config;

	if (!commandLine || !*commandLine)
		return config;

	// Tokenize the command line
	std::vector<std::string> args;
	const char* p = commandLine;
	while (*p)
	{
		// Skip whitespace
		while (*p == ' ' || *p == '\t') ++p;
		if (!*p) break;

		// Handle quoted strings
		if (*p == '"')
		{
			++p;
			const char* start = p;
			while (*p && *p != '"') ++p;
			args.push_back(std::string(start, p));
			if (*p == '"') ++p;
		}
		else
		{
			const char* start = p;
			while (*p && *p != ' ' && *p != '\t') ++p;
			args.push_back(std::string(start, p));
		}
	}

	// Parse tokens
	for (size_t i = 0; i < args.size(); ++i)
	{
		const std::string& arg = args[i];

		if ((arg == "-terrain" || arg == "-t") && i + 1 < args.size())
		{
			config.terrainFile = args[++i];
		}
		else if ((arg == "-output" || arg == "-o") && i + 1 < args.size())
		{
			config.outputDir = args[++i];
		}
		else if ((arg == "-size" || arg == "-s") && i + 1 < args.size())
		{
			config.imageSize = atoi(args[++i].c_str());
			if (config.imageSize < 64) config.imageSize = 64;
			if (config.imageSize > 8192) config.imageSize = 8192;
		}
		else if ((arg == "-tiles") && i + 1 < args.size())
		{
			config.tilesPerSide = atoi(args[++i].c_str());
			if (config.tilesPerSide < 1) config.tilesPerSide = 1;
			if (config.tilesPerSide > 64) config.tilesPerSide = 64;
		}
		else if ((arg == "-height") && i + 1 < args.size())
		{
			config.cameraHeight = static_cast<float>(atof(args[++i].c_str()));
		}
		else if (arg == "-colormap")
		{
			config.useColormapMode = true;
		}
		else if (arg == "-render")
		{
			config.useColormapMode = false;
		}
		else if (arg == "-all")
		{
			config.processAll = true;
		}
		else if ((arg == "-config") && i + 1 < args.size())
		{
			config.configFile = args[++i];
		}
		else if (arg == "-aa")
		{
			config.antiAlias = true;
		}
		else if (arg == "-noaa")
		{
			config.antiAlias = false;
		}
		else if (arg == "-help" || arg == "-h" || arg == "--help")
		{
			printUsage();
		}
	}

	return config;
}

// ======================================================================
// Engine initialization
// ======================================================================

bool SwgMapRasterizer::initializeEngine(HINSTANCE hInstance, const Config& config)
{
	s_hInstance = hInstance;

	// -- Thread --
	SetupSharedThread::install();

	// -- Debug --
	SetupSharedDebug::install(4096);

	// -- Foundation --
	if (config.useColormapMode)
	{
		// CPU colormap mode: console application, no window needed
		SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_console);
		data.configFile = config.configFile.c_str();
		SetupSharedFoundation::install(data);
	}
	else
	{
		// GPU render mode: game mode for window/D3D
		SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_game);
		data.windowName    = "SwgMapRasterizer";
		data.hInstance     = hInstance;
		data.configFile    = config.configFile.c_str();
		data.clockUsesSleep = true;
		data.frameRateLimit = 60.0f;
		SetupSharedFoundation::install(data);
	}

	if (ConfigFile::isEmpty())
	{
		printf("ERROR: Config file '%s' not found or empty.\n", config.configFile.c_str());
		printf("       The config file must contain [SharedFile] entries for TRE paths.\n");
		return false;
	}

	// -- Compression --
	{
		SetupSharedCompression::Data data;
		data.numberOfThreadsAccessingZlib = 1;
		SetupSharedCompression::install(data);
	}

	// -- Regex --
	SetupSharedRegex::install();

	// -- File system (with tree files for .trn access) --
	SetupSharedFile::install(true);

	// -- Math --
	SetupSharedMath::install();

	// -- Utility --
	{
		SetupSharedUtility::Data data;
		SetupSharedUtility::setupGameData(data);
		SetupSharedUtility::install(data);
	}

	// -- Random --
	SetupSharedRandom::install(static_cast<uint32>(time(NULL)));

	// -- Image --
	SetupSharedImage::Data data;
	SetupSharedImage::setupDefaultData(data);
	SetupSharedImage::install(data);

	// -- Object --
	{
		SetupSharedObject::Data data;
		SetupSharedObject::setupDefaultGameData(data);
		SetupSharedObject::install(data);
	}

	// -- Shared Terrain --
	{
		SetupSharedTerrain::Data data;
		SetupSharedTerrain::setupGameData(data);
		SetupSharedTerrain::install(data);
	}

	s_engineInitialized = true;

	// -- Client-side subsystems for GPU rendering mode --
	if (!config.useColormapMode)
	{
		// Graphics
		SetupClientGraphics::Data graphicsData;
		graphicsData.screenWidth  = config.imageSize;
		graphicsData.screenHeight = config.imageSize;
		graphicsData.alphaBufferBitDepth = 0;
		SetupClientGraphics::setupDefaultGameData(graphicsData);

		if (!SetupClientGraphics::install(graphicsData))
		{
			printf("WARNING: Failed to initialize graphics. Falling back to colormap mode.\n");
			return true; // Engine is initialized, just no graphics
		}

		s_graphicsInitialized = true;

		// Client Object (for ObjectListCamera)
		{
			SetupClientObject::Data data;
			SetupClientObject::setupGameData(data);
			SetupClientObject::install(data);
		}

		// Client Terrain (for rendered terrain appearance)
		SetupClientTerrain::install();
	}

	return true;
}

// ----------------------------------------------------------------------

void SwgMapRasterizer::shutdownEngine()
{
	if (s_engineInitialized)
	{
		SetupSharedFoundation::remove();
		SetupSharedThread::remove();
		s_engineInitialized = false;
		s_graphicsInitialized = false;
	}
}

// ======================================================================
// Main entry point
// ======================================================================

int SwgMapRasterizer::run(HINSTANCE hInstance, const char* commandLine)
{
	printBanner();

	Config config = parseCommandLine(commandLine);
	
	//if config is empty, show usage
	if (commandLine == nullptr || strlen(commandLine) == 0)
	{
		printUsage();
		return 1;
	}

	// Validate input
	if (!config.processAll && config.terrainFile.empty())
	{
		printf("ERROR: No terrain file specified. Use -terrain <file.trn> or -all.\n\n");
		printUsage();
		return 1;
	}

	// Ensure image size is divisible by tiles
	if (config.imageSize % config.tilesPerSide != 0)
	{
		config.imageSize = (config.imageSize / config.tilesPerSide) * config.tilesPerSide;
		printf("NOTE: Adjusted image size to %d to be divisible by tile count.\n", config.imageSize);
	}

	printf("Configuration:\n");
	printf("  Mode:       %s\n", config.useColormapMode ? "CPU Colormap" : "GPU Rendered");
	printf("  Image size: %dx%d\n", config.imageSize, config.imageSize);
	printf("  Tile grid:  %dx%d (%d tiles)\n", config.tilesPerSide, config.tilesPerSide, 
	       config.tilesPerSide * config.tilesPerSide);
	printf("  Output dir: %s\n", config.outputDir.c_str());
	printf("  Config:     %s\n", config.configFile.c_str());
	printf("\n");

	// Initialize engine
	printf("Initializing engine...\n");
	if (!initializeEngine(hInstance, config))
	{
		printf("FATAL: Engine initialization failed.\n");
		shutdownEngine();
		return 1;
	}
	printf("Engine initialized successfully.\n\n");

	// Create output directory
	_mkdir(config.outputDir.c_str());

	int successCount = 0;
	int failCount = 0;

	if (config.processAll)
	{
		// Process all known planet terrains
		printf("Processing all %d planet terrains...\n\n", s_numPlanetEntries);
		for (int i = 0; i < s_numPlanetEntries; ++i)
		{
			printf("--- [%d/%d] %s ---\n", i + 1, s_numPlanetEntries, s_planetEntries[i].terrainFile);
			if (rasterizeTerrain(config, s_planetEntries[i].terrainFile, s_planetEntries[i].outputName))
				++successCount;
			else
				++failCount;
			printf("\n");
		}
	}
	else
	{
		// Process single terrain file
		// Determine output name from terrain file
		std::string outputName;

		// Check if it matches a known planet
		for (int i = 0; i < s_numPlanetEntries; ++i)
		{
			if (config.terrainFile == s_planetEntries[i].terrainFile)
			{
				outputName = s_planetEntries[i].outputName;
				break;
			}
		}

		// Generate output name from terrain filename if not found
		if (outputName.empty())
		{
			std::string name = config.terrainFile;
			// Strip path
			size_t lastSlash = name.find_last_of("/\\");
			if (lastSlash != std::string::npos)
				name = name.substr(lastSlash + 1);
			// Strip extension
			size_t dot = name.find_last_of('.');
			if (dot != std::string::npos)
				name = name.substr(0, dot);
			outputName = "ui_map_" + name;
		}

		printf("Processing: %s -> %s\n\n", config.terrainFile.c_str(), outputName.c_str());
		if (rasterizeTerrain(config, config.terrainFile.c_str(), outputName.c_str()))
			++successCount;
		else
			++failCount;
	}

	printf("\n========================================================\n");
	printf("  Complete: %d succeeded, %d failed\n", successCount, failCount);
	printf("========================================================\n");

	shutdownEngine();
	exit(failCount > 0 ? 1 : 0);
}

// ======================================================================
// Terrain processing dispatcher
// ======================================================================
bool SwgMapRasterizer::rasterizeTerrain(const Config& config, const char* terrainFile, const char* outputName)
{
	// Check if terrain file exists in the tree file system
	if (!TreeFile::exists(terrainFile))
	{
		printf("  ERROR: Terrain file '%s' not found in tree file system.\n", terrainFile);
		printf("         Make sure the TRE files are correctly configured in %s.\n", config.configFile.c_str());
		return false;
	}

	if (config.useColormapMode || !s_graphicsInitialized)
	{
		return renderTerrainColormap(config, terrainFile, outputName);
	}
	else
	{
		return renderTerrainGPU(config, terrainFile, outputName);
	}
}

// ======================================================================
// CPU Colormap Mode
// ======================================================================
//
// Path conventions (verified):
//   Color ramps: terrain/colorramp/<name>.tga or terrain/<name>.tga
//   texture/*                 - textures (.dds preferred, .tga fallback)
//   shader/*.sht              - shader templates (names used to resolve texture/<basename>.dds)
//
// ======================================================================
// Build shader layer information from the terrain generator
// ======================================================================
static std::vector<ShaderLayer> buildShaderLayers(const TerrainGenerator* generator)
{
	std::vector<ShaderLayer> layers;

	if (!generator)
		return layers;

	const ShaderGroup& shaderGroup = generator->getShaderGroup();
	int numFamilies = shaderGroup.getNumberOfFamilies();

	printf("  Found %d shader families:\n", numFamilies);

	// Enumerate all shader families in the generator
	for (int familyIndex = 0; familyIndex < numFamilies; ++familyIndex)
	{
		int familyId = shaderGroup.getFamilyId(familyIndex);
		const char* familyName = shaderGroup.getFamilyName(familyId);
		const PackedRgb& familyColor = shaderGroup.getFamilyColor(familyId);
		float shaderSize = shaderGroup.getFamilyShaderSize(familyId);
		int numChildren = shaderGroup.getFamilyNumberOfChildren(familyId);

		printf("    [%d] %s (id=%d, color=%d,%d,%d, size=%.1f m, children=%d)\n",
			familyIndex, familyName, familyId, 
			familyColor.r, familyColor.g, familyColor.b,
			shaderSize, numChildren);

		ShaderLayer layer;
		layer.familyId = familyId;
		layer.familyName = familyName ? familyName : "";
		layer.familyColor = familyColor;
		layer.priority = familyIndex;
		layer.layerWeight = 0.3f;
		layer.shaderSize = (shaderSize > 0.1f) ? shaderSize : 2.0f;  // avoid zero
		layer.familyTexture = 0;

		// Adjust weight based on shader size for visual prominence
		if (shaderSize > 100.0f)
			layer.layerWeight = 0.4f;
		else if (shaderSize < 10.0f)
			layer.layerWeight = 0.2f;

		// Textures: texture/* (.dds preferred; .tga fallback since we only have TargaFormat loader)
		const char* name = layer.familyName.c_str();
		char path[256];
		TargaFormat tgaFormat;
		Image* tex = 0;

		// 1) texture/<familyname>.dds (if DDS loader registered)
		_snprintf(path, sizeof(path), "texture/%s.dds", name);
		tex = ImageFormatList::loadImage(path);
		if (tex)
		{
			layer.familyTexture = tex;
			printf("      -> texture: %s\n", path);
		}
		else
		{
			// 2) texture/<familyname>.tga
			_snprintf(path, sizeof(path), "texture/%s.tga", name);
			if (tgaFormat.loadImage(path, &tex))
			{
				layer.familyTexture = tex;
				printf("      -> texture: %s\n", path);
			}
			else if (tex)
				delete tex;
		}
		if (!layer.familyTexture && numChildren > 0)
		{
			tex = 0;
			// 3) From shader path shader/*.sht -> texture/<basename>.dds then .tga
			ShaderGroup::FamilyChildData child = shaderGroup.getFamilyChild(familyId, 0);
			if (child.shaderTemplateName && child.shaderTemplateName[0])
			{
				std::string base(child.shaderTemplateName);
				size_t slash = base.find_last_of("/\\");
				if (slash != std::string::npos)
					base = base.substr(slash + 1);
				size_t dot = base.find_last_of('.');
				if (dot != std::string::npos)
					base = base.substr(0, dot);
				if (!base.empty())
				{
					_snprintf(path, sizeof(path), "texture/%s.dds", base.c_str());
					tex = ImageFormatList::loadImage(path);
					if (tex)
					{
						layer.familyTexture = tex;
						printf("      -> texture: %s (from shader %s)\n", path, child.shaderTemplateName);
					}
					else
					{
						_snprintf(path, sizeof(path), "texture/%s.tga", base.c_str());
						if (tgaFormat.loadImage(path, &tex))
						{
							layer.familyTexture = tex;
							printf("      -> texture: %s (from shader %s)\n", path, child.shaderTemplateName);
						}
						else if (tex)
							delete tex;
					}
				}
			}
		}

		layers.push_back(layer);
	}

	printf("  Shader family enumeration complete.\n\n");

	return layers;
}

bool SwgMapRasterizer::renderTerrainColormap(const Config& config, const char* terrainFile, const char* outputName)
{
	printf("  Loading terrain: %s\n", terrainFile);

	// Load the terrain template through the appearance system
	const AppearanceTemplate* at = AppearanceTemplateList::fetch(terrainFile);
	if (!at)
	{
		printf("  ERROR: Failed to load terrain template '%s'.\n", terrainFile);
		return false;
	}

	const ProceduralTerrainAppearanceTemplate* terrainTemplate = 
		dynamic_cast<const ProceduralTerrainAppearanceTemplate*>(at);

	if (!terrainTemplate)
	{
		printf("  ERROR: '%s' is not a procedural terrain file.\n", terrainFile);
		AppearanceTemplateList::release(at);
		return false;
	}

	// Get terrain properties
	const float mapWidth  = terrainTemplate->getMapWidthInMeters();
	const bool legacyMode = terrainTemplate->getLegacyMode();
	const int originOffset = terrainTemplate->getChunkOriginOffset();
	const int upperPad     = terrainTemplate->getChunkUpperPad();

	const TerrainGenerator* generator = terrainTemplate->getTerrainGenerator();
	const ShaderGroup& shaderGroup = generator->getShaderGroup();
	if (!generator)
	{
		printf("  ERROR: Terrain has no generator.\n");
		AppearanceTemplateList::release(at);
		return false;
	}

	printf("  Map width: %.0f meters (%.0f x %.0f km)\n", mapWidth, mapWidth / 1000.0f, mapWidth / 1000.0f);
	printf("  Legacy mode: %s\n", legacyMode ? "yes" : "no");
	printf("  Chunk padding: origin=%d, upper=%d\n", originOffset, upperPad);

	// Build shader layer information from the generator
	printf("\n  Building shader layer information...\n");
	std::vector<ShaderLayer> shaderLayers = buildShaderLayers(generator);
	printf("\n");

	const int imageSize = config.imageSize;
	const int internalSize = config.antiAlias ? (2 * imageSize) : imageSize;
	const int tilesPerSide = config.tilesPerSide;
	const int pixelsPerTile = internalSize / tilesPerSide;
	const float metersPerPixel = mapWidth / static_cast<float>(internalSize);

	printf("  Output: %dx%d pixels (%.2f meters/pixel)%s\n",
		imageSize, imageSize,
		mapWidth / static_cast<float>(imageSize),
		config.antiAlias ? " (2x supersample anti-aliasing)" : "");
	printf("  Generating terrain data in %dx%d tiles (4 quadrants in parallel)...\n", tilesPerSide, tilesPerSide);
	fflush(stdout);

	// Allocate output buffers at internal resolution
	uint8* colorPixels = new uint8[internalSize * internalSize * 3];
	float* heightPixels = new float[internalSize * internalSize];
	memset(colorPixels, 0, internalSize * internalSize * 3);
	memset(heightPixels, 0, internalSize * internalSize * sizeof(float));

	const int totalTiles = tilesPerSide * tilesPerSide;
	const int midT = tilesPerSide / 2;

	// Context for quadrant workers: shared state + per-thread min/max
	struct ColormapQuadrantContext
	{
		TerrainGenerator const* generator;
		std::vector<ShaderLayer> const& shaderLayers;
		uint8* colorPixels;
		float* heightPixels;
		int tilesPerSide;
		int pixelsPerTile;
		int internalSize;
		int originOffset;
		int upperPad;
		bool legacyMode;
		float mapWidth;
		float metersPerPixel;
		float threadMin[4];
		float threadMax[4];

		ColormapQuadrantContext(
			TerrainGenerator const* gen,
			std::vector<ShaderLayer> const& layers,
			uint8* colors,
			float* heights,
			int tiles, int pixelsPerTile_, int internal,
			int origin, int upper, bool legacy,
			float mapW, float metersPerPixel_
		) : generator(gen), shaderLayers(layers), colorPixels(colors), heightPixels(heights),
		    tilesPerSide(tiles), pixelsPerTile(pixelsPerTile_), internalSize(internal),
		    originOffset(origin), upperPad(upper), legacyMode(legacy),
		    mapWidth(mapW), metersPerPixel(metersPerPixel_)
		{
			for (int i = 0; i < 4; ++i) { threadMin[i] = 1e10f; threadMax[i] = -1e10f; }
		}

		void processQuadrant(int q)
		{
			int const midT = tilesPerSide / 2;
			int txLo, txHi, tzLo, tzHi;
			if (q == 0)      { txLo = 0; txHi = midT; tzLo = 0; tzHi = midT; }
			else if (q == 1) { txLo = midT; txHi = tilesPerSide; tzLo = 0; tzHi = midT; }
			else if (q == 2) { txLo = 0; txHi = midT; tzLo = midT; tzHi = tilesPerSide; }
			else             { txLo = midT; txHi = tilesPerSide; tzLo = midT; tzHi = tilesPerSide; }

			float& myMin = threadMin[q];
			float& myMax = threadMax[q];

			for (int tz = tzLo; tz < tzHi; ++tz)
			{
				for (int tx = txLo; tx < txHi; ++tx)
				{
					const float tileWorldMinX = -mapWidth / 2.0f + tx * (mapWidth / tilesPerSide);
					const float tileWorldMinZ = -mapWidth / 2.0f + tz * (mapWidth / tilesPerSide);

					const int totalPoles = pixelsPerTile + originOffset + upperPad;

					TerrainGenerator::CreateChunkBuffer* buffer =
						new TerrainGenerator::CreateChunkBuffer();
					memset(buffer, 0, sizeof(TerrainGenerator::CreateChunkBuffer));
					buffer->allocate(totalPoles);

					TerrainGenerator::GeneratorChunkData chunkData(legacyMode);
					chunkData.originOffset        = originOffset;
					chunkData.numberOfPoles       = totalPoles;
					chunkData.upperPad            = upperPad;
					chunkData.distanceBetweenPoles = metersPerPixel;
					chunkData.start               = Vector(
						tileWorldMinX - originOffset * metersPerPixel,
						0.0f,
						tileWorldMinZ - originOffset * metersPerPixel
					);

					chunkData.heightMap                    = &buffer->heightMap;
					chunkData.colorMap                     = &buffer->colorMap;
					chunkData.shaderMap                    = &buffer->shaderMap;
					chunkData.floraStaticCollidableMap      = &buffer->floraStaticCollidableMap;
					chunkData.floraStaticNonCollidableMap   = &buffer->floraStaticNonCollidableMap;
					chunkData.floraDynamicNearMap           = &buffer->floraDynamicNearMap;
					chunkData.floraDynamicFarMap            = &buffer->floraDynamicFarMap;
					chunkData.environmentMap                = &buffer->environmentMap;
					chunkData.vertexPositionMap             = &buffer->vertexPositionMap;
					chunkData.vertexNormalMap               = &buffer->vertexNormalMap;
					chunkData.excludeMap                    = &buffer->excludeMap;
					chunkData.passableMap                   = &buffer->passableMap;

					chunkData.shaderGroup       = &generator->getShaderGroup();
					chunkData.floraGroup        = &generator->getFloraGroup();
					chunkData.radialGroup       = &generator->getRadialGroup();
					chunkData.environmentGroup  = &generator->getEnvironmentGroup();
					chunkData.fractalGroup      = &generator->getFractalGroup();
					chunkData.bitmapGroup       = &generator->getBitmapGroup();

					generator->generateChunk(chunkData);

					for (int z = 0; z < pixelsPerTile; ++z)
					{
						for (int x = 0; x < pixelsPerTile; ++x)
						{
							const int srcX = x + originOffset;
							const int srcZ = z + originOffset;

							const int dstX = tx * pixelsPerTile + x;
							const int dstZ = tz * pixelsPerTile + z;

							const int imgX = dstX;
							const int imgY = (internalSize - 1) - dstZ;

							if (imgX < 0 || imgX >= internalSize || imgY < 0 || imgY >= internalSize)
								continue;

							const PackedRgb generatorColor =
								buffer->colorMap.getData(srcX, srcZ);

							const ShaderGroup::Info sgi =
								buffer->shaderMap.getData(srcX, srcZ);

							PackedRgb color = generatorColor;

							if (sgi.getFamilyId() > 0 && chunkData.shaderGroup)
							{
								const char* familyName = chunkData.shaderGroup->getFamilyName(sgi.getFamilyId());
								if (familyName && familyName[0])
								{
									const ShaderLayer* layer = getLayerForFamily(sgi.getFamilyId(), shaderLayers);
									PackedRgb shaderColor;

									if (layer && layer->familyTexture && layer->shaderSize > 0.0f)
									{
										float worldX = chunkData.start.x + srcX * chunkData.distanceBetweenPoles;
										float worldZ = chunkData.start.z + srcZ * chunkData.distanceBetweenPoles;
										float u = worldX / layer->shaderSize;
										float v = worldZ / layer->shaderSize;
										shaderColor = sampleTextureAtUV(layer->familyTexture, u, v);
									}
									else
									{
										PackedRgb terrainColor = getTerrainShaderColor(
											sgi.getFamilyId(), shaderLayers, generatorColor);
										float childChoice = sgi.getChildChoice();
										shaderColor = getShaderFamilyColor(
											familyName, terrainColor, childChoice);
									}

									color = blendColors(generatorColor, shaderColor, 0.85f);
								}
							}

							const float height =
								buffer->heightMap.getData(srcX, srcZ);

							const int pixelIndex = imgY * internalSize + imgX;
							const int colorOffset = pixelIndex * 3;

							colorPixels[colorOffset + 0] = color.r;
							colorPixels[colorOffset + 1] = color.g;
							colorPixels[colorOffset + 2] = color.b;

							heightPixels[pixelIndex] = height;

							if (height < myMin) myMin = height;
							if (height > myMax) myMax = height;
						}
					}
					delete buffer;
				}
			}
		}
	};

	ColormapQuadrantContext ctx(
		generator, shaderLayers, colorPixels, heightPixels,
		tilesPerSide, pixelsPerTile, internalSize,
		originOffset, upperPad, legacyMode,
		mapWidth, metersPerPixel
	);

	typedef TypedThreadHandle<MemberFunctionThreadOne<ColormapQuadrantContext, int> > QuadrantHandle;
	QuadrantHandle h0 = runThread(ctx, &ColormapQuadrantContext::processQuadrant, 0);
	QuadrantHandle h1 = runThread(ctx, &ColormapQuadrantContext::processQuadrant, 1);
	QuadrantHandle h2 = runThread(ctx, &ColormapQuadrantContext::processQuadrant, 2);
	QuadrantHandle h3 = runThread(ctx, &ColormapQuadrantContext::processQuadrant, 3);

	h0->wait();
	h1->wait();
	h2->wait();
	h3->wait();

	float minHeight = ctx.threadMin[0];
	float maxHeight = ctx.threadMax[0];
	for (int q = 1; q < 4; ++q)
	{
		if (ctx.threadMin[q] < minHeight) minHeight = ctx.threadMin[q];
		if (ctx.threadMax[q] > maxHeight) maxHeight = ctx.threadMax[q];
	}

	printf("  [100%%] Generated %d tiles. Height range: %.1f to %.1f m\n", totalTiles, minHeight, maxHeight);

	// Apply hillshading for 3D terrain effect
	printf("  Applying hillshading...\n");
	fflush(stdout);
	applyHillshading(heightPixels, colorPixels, internalSize, internalSize, mapWidth);
	printf("  Hillshading done.\n");
	fflush(stdout);

	// Apply water rendering
	if (terrainTemplate->getUseGlobalWaterTable())
	{
		const float waterHeight = terrainTemplate->getGlobalWaterTableHeight();
		printf("  Rendering water (global water table at %.1f meters)...\n", waterHeight);

		for (int y = 0; y < internalSize; ++y)
		{
			for (int x = 0; x < internalSize; ++x)
			{
				const int idx = y * internalSize + x;
				const float h = heightPixels[idx];

				if (h <= waterHeight)
				{
					const int offset = idx * 3;
					const float depth = waterHeight - h;
					const float waterFactor = std::min(depth / 30.0f, 0.85f);

					// Water color: deep blue, default until shading implementation is added
					const float waterR = 15.0f;
					const float waterG = 50.0f;
					const float waterB = 110.0f;

					// Acid color: deep blue, default until shading implementation is added
					const float acidR = 60.0f;
					const float acidG = 120.0f;
					const float acidB = 30.0f;

					//Lava color - bright orange, default until shading implementation is added
					const float lavaR = 200.0f;
					const float lavaG = 80.0f;
					const float lavaB = 20.0f;

					//Using TerrainGeneratorWaterType to determine if we should render water, acid, or lava based on the terrain's water type
					
					//init vector with water color by default, then override it if it's acid or lava

					Vector waterColor(waterR, waterG, waterB);
					Vector w_postion(x, waterHeight, y);

					const TerrainGeneratorWaterType waterType = terrainTemplate->getWaterType(w_postion);
					if (waterType == TerrainGeneratorWaterType::TGWT_lava)
					{
						waterColor = Vector(lavaR, lavaG, lavaB);
					}
					else if (waterType == TerrainGeneratorWaterType::TGWT_water)
					{
						waterColor = Vector(waterR, waterG, waterB);
					}

					colorPixels[offset + 0] = static_cast<uint8>(colorPixels[offset + 0] * (1.0f - waterFactor) + waterColor.x * waterFactor);
					colorPixels[offset + 1] = static_cast<uint8>(colorPixels[offset + 1] * (1.0f - waterFactor) + waterColor.y * waterFactor);
					colorPixels[offset + 2] = static_cast<uint8>(colorPixels[offset + 2] * (1.0f - waterFactor) + waterColor.z * waterFactor);
				}
			}
		}
	}

	// Optionally downsample 2x for anti-aliased output
	uint8* savePixels = colorPixels;
	int saveWidth = internalSize;
	int saveHeight = internalSize;
	std::vector<uint8> downsampled;
	if (config.antiAlias)
	{
		downsampled.resize(imageSize * imageSize * 3);
		downsample2xRGB(colorPixels, internalSize, internalSize, &downsampled[0], imageSize, imageSize);
		savePixels = &downsampled[0];
		saveWidth = imageSize;
		saveHeight = imageSize;
		printf("  Downsampled 2x for anti-aliasing.\n");
		fflush(stdout);
	}

	// Flip vertically to match the expected orientation in the UI
	const int rowBytes = saveWidth * 3;
	std::vector<uint8> flippedPixels(saveWidth * saveHeight * 3);
	for (int y = 0; y < saveHeight; ++y)
	{
		const int srcOffset = y * rowBytes;
		const int dstOffset = (saveHeight - 1 - y) * rowBytes;
		memcpy(&flippedPixels[dstOffset], &savePixels[srcOffset], rowBytes);
	}
	printf("  Flipped image vertically for correct orientation.\n");
	fflush(stdout);

	// Save output image
	char outputPath[512];
	_snprintf(outputPath, sizeof(outputPath), "%s/%s.tga", config.outputDir.c_str(), outputName);

	printf("  Saving %s ...\n", outputPath);
	fflush(stdout);
	const bool saved = saveTGA(&flippedPixels[0], saveWidth, saveHeight, 3, outputPath);

	// Cleanup: release family textures and cached ramp images
	for (size_t i = 0; i < shaderLayers.size(); ++i)
	{
		if (shaderLayers[i].familyTexture)
		{
			delete shaderLayers[i].familyTexture;
			shaderLayers[i].familyTexture = 0;
		}
	}
	clearShaderRampImageCache();

	delete[] colorPixels;
	delete[] heightPixels;
	AppearanceTemplateList::release(at);

	if (saved)
		printf("  SUCCESS: %s (%dx%d)\n", outputPath, imageSize, imageSize);
	else
		printf("  ERROR: Failed to save %s\n", outputPath);

	return saved;
}

bool SwgMapRasterizer::renderTerrainShading(const Config& config, const char* terrainFile, const char* outputName)
{
	printf("  Loading terrain: %s\n", terrainFile);

	// Load the terrain template through the appearance system
	const AppearanceTemplate* at = AppearanceTemplateList::fetch(terrainFile);
	if (!at)
	{
		printf("  ERROR: Failed to load terrain template '%s'.\n", terrainFile);
		return false;
	}

	const ProceduralTerrainAppearanceTemplate* terrainTemplate =
		dynamic_cast<const ProceduralTerrainAppearanceTemplate*>(at);

	if (!terrainTemplate)
	{
		printf("  ERROR: '%s' is not a procedural terrain file.\n", terrainFile);
		AppearanceTemplateList::release(at);
		return false;
	}

	// Get terrain properties
	const float mapWidth = terrainTemplate->getMapWidthInMeters();
	const bool legacyMode = terrainTemplate->getLegacyMode();
	const int originOffset = terrainTemplate->getChunkOriginOffset();
	const int upperPad = terrainTemplate->getChunkUpperPad();

	const TerrainGenerator* generator = terrainTemplate->getTerrainGenerator();
	if (!generator)
	{
		printf("  ERROR: Terrain has no generator.\n");
		AppearanceTemplateList::release(at);
		return false;
	}

	printf("  Map width: %.0f meters (%.0f x %.0f km)\n", mapWidth, mapWidth / 1000.0f, mapWidth / 1000.0f);
	printf("  Legacy mode: %s\n", legacyMode ? "yes" : "no");
	printf("  Chunk padding: origin=%d, upper=%d\n", originOffset, upperPad);

	const int imageSize = config.imageSize;
	const int tilesPerSide = config.tilesPerSide;
	const int pixelsPerTile = imageSize / tilesPerSide;
	const float metersPerPixel = mapWidth / static_cast<float>(imageSize);

	printf("  Output: %dx%d pixels (%.2f meters/pixel)\n", imageSize, imageSize, metersPerPixel);
	printf("  Generating terrain data in %dx%d tiles...\n", tilesPerSide, tilesPerSide);

	// Allocate output buffers
	uint8* colorPixels = new uint8[imageSize * imageSize * 3];
	float* heightPixels = new float[imageSize * imageSize];
	memset(colorPixels, 0, imageSize * imageSize * 3);
	memset(heightPixels, 0, imageSize * imageSize * sizeof(float));

	float minHeight = 1e10f;
	float maxHeight = -1e10f;

	// Process each tile
	int totalTiles = tilesPerSide * tilesPerSide;
	int tilesDone = 0;

	for (int tz = 0; tz < tilesPerSide; ++tz)
	{
		for (int tx = 0; tx < tilesPerSide; ++tx)
		{
			++tilesDone;
			int pctShading = (totalTiles > 0) ? (tilesDone * 100 / totalTiles) : 0;
			printf("\r  [%3d%%] Tile %d/%d (%d,%d)  ", pctShading, tilesDone, totalTiles, tx, tz);
			fflush(stdout);

			// World-space origin of this tile
			const float tileWorldMinX = -mapWidth / 2.0f + tx * (mapWidth / tilesPerSide);
			const float tileWorldMinZ = -mapWidth / 2.0f + tz * (mapWidth / tilesPerSide);

			// Set up chunk generation request
			const int totalPoles = pixelsPerTile + originOffset + upperPad;

			TerrainGenerator::CreateChunkBuffer* buffer =
				new TerrainGenerator::CreateChunkBuffer();
			memset(buffer, 0, sizeof(TerrainGenerator::CreateChunkBuffer));
			buffer->allocate(totalPoles);

			TerrainGenerator::GeneratorChunkData chunkData(legacyMode);
			chunkData.originOffset = originOffset;
			chunkData.numberOfPoles = totalPoles;
			chunkData.upperPad = upperPad;
			chunkData.distanceBetweenPoles = metersPerPixel;
			chunkData.start = Vector(
				tileWorldMinX - originOffset * metersPerPixel,
				0.0f,
				tileWorldMinZ - originOffset * metersPerPixel
			);

			// Connect map pointers
			chunkData.heightMap = &buffer->heightMap;
			chunkData.colorMap = &buffer->colorMap;
			chunkData.shaderMap = &buffer->shaderMap;
			chunkData.floraStaticCollidableMap = &buffer->floraStaticCollidableMap;
			chunkData.floraStaticNonCollidableMap = &buffer->floraStaticNonCollidableMap;
			chunkData.floraDynamicNearMap = &buffer->floraDynamicNearMap;
			chunkData.floraDynamicFarMap = &buffer->floraDynamicFarMap;
			chunkData.environmentMap = &buffer->environmentMap;
			chunkData.vertexPositionMap = &buffer->vertexPositionMap;
			chunkData.vertexNormalMap = &buffer->vertexNormalMap;
			chunkData.excludeMap = &buffer->excludeMap;
			chunkData.passableMap = &buffer->passableMap;

			// Set group pointers from the generator
			chunkData.shaderGroup = &generator->getShaderGroup();
			chunkData.floraGroup = &generator->getFloraGroup();
			chunkData.radialGroup = &generator->getRadialGroup();
			chunkData.environmentGroup = &generator->getEnvironmentGroup();
			chunkData.fractalGroup = &generator->getFractalGroup();
			chunkData.bitmapGroup = &generator->getBitmapGroup();

			// Generate terrain chunk data
			generator->generateChunk(chunkData);

			// Copy height and color data to output image
			for (int z = 0; z < pixelsPerTile; ++z)
			{
				for (int x = 0; x < pixelsPerTile; ++x)
				{
					const int srcX = x + originOffset;
					const int srcZ = z + originOffset;

					// Destination pixel in the full image
					const int dstX = tx * pixelsPerTile + x;
					const int dstZ = tz * pixelsPerTile + z;

					// Image Y is inverted: top of image = north = max Z
					const int imgX = dstX;
					const int imgY = (imageSize - 1) - dstZ;

					if (imgX < 0 || imgX >= imageSize || imgY < 0 || imgY >= imageSize)
						continue;

					const PackedRgb color = buffer->colorMap.getData(srcX, srcZ);
					const float height = buffer->heightMap.getData(srcX, srcZ);
					const auto shader = buffer->shaderMap.getData(srcX, srcZ);
					//get shader to color pixel

					const int pixelIndex = imgY * imageSize + imgX;
					const int colorOffset = pixelIndex * 3;

					colorPixels[colorOffset + 0] = color.r;
					colorPixels[colorOffset + 1] = color.g;
					colorPixels[colorOffset + 2] = color.b;

					heightPixels[pixelIndex] = height;

					if (height < minHeight) minHeight = height;
					if (height > maxHeight) maxHeight = height;
				}
			}
		}
	}

	printf("\r  Generated %d tiles. Height range: %.1f to %.1f meters\n", totalTiles, minHeight, maxHeight);

	// Apply hillshading for 3D terrain effect
	printf("  Applying hillshading...\n");
	applyHillshading(heightPixels, colorPixels, imageSize, imageSize, mapWidth);

	// Apply water rendering
	if (terrainTemplate->getUseGlobalWaterTable())
	{
		const float waterHeight = terrainTemplate->getGlobalWaterTableHeight();
		printf("  Rendering water (global water table at %.1f meters)...\n", waterHeight);

		for (int y = 0; y < imageSize; ++y)
		{
			for (int x = 0; x < imageSize; ++x)
			{
				const int idx = y * imageSize + x;
				const float h = heightPixels[idx];

				if (h <= waterHeight)
				{
					const int offset = idx * 3;
					const float depth = waterHeight - h;
					const float waterFactor = std::min(depth / 30.0f, 0.85f);

					// Water color: deep blue
					const float waterR = 15.0f;
					const float waterG = 50.0f;
					const float waterB = 110.0f;

					colorPixels[offset + 0] = static_cast<uint8>(colorPixels[offset + 0] * (1.0f - waterFactor) + waterR * waterFactor);
					colorPixels[offset + 1] = static_cast<uint8>(colorPixels[offset + 1] * (1.0f - waterFactor) + waterG * waterFactor);
					colorPixels[offset + 2] = static_cast<uint8>(colorPixels[offset + 2] * (1.0f - waterFactor) + waterB * waterFactor);
				}
			}
		}
	}


	// Save output image
	char outputPath[512];
	_snprintf(outputPath, sizeof(outputPath), "%s/%s.tga", config.outputDir.c_str(), outputName);

	printf("  Saving: %s\n", outputPath);
	const bool saved = saveTGA(colorPixels, imageSize, imageSize, 3, outputPath);

	// Cleanup
	delete[] colorPixels;
	delete[] heightPixels;
	AppearanceTemplateList::release(at);

	if (saved)
		printf("  SUCCESS: %s (%dx%d)\n", outputPath, imageSize, imageSize);
	else
		printf("  ERROR: Failed to save %s\n", outputPath);

	return saved;
}


// ======================================================================
// GPU Rendered Mode
// ======================================================================


bool SwgMapRasterizer::renderTerrainGPU(const Config& config, const char* terrainFile, const char* outputName)
{
	if (!s_graphicsInitialized)
	{
		printf("  ERROR: Graphics not initialized. Cannot use GPU rendered mode.\n");
		printf("         Falling back to colormap mode.\n");
		return renderTerrainColormap(config, terrainFile, outputName);
	}

	printf("\n");
	printf("  ====== GPU Rendered Terrain Mode ======\n");
	printf("  WARNING: GPU rendering is experimental. Using CPU colormap mode instead.\n");
	printf("  (GPU rendering requires full D3D context setup and is not yet stable)\n\n");
	
	// GPU rendering is still experimental - fall back to proven CPU colormap mode
	return renderTerrainColormap(config, terrainFile, outputName);
}

// ======================================================================
// GPU Tile Rendering (Experimental - Currently Disabled)
// 
// This function renders individual tiles using GPU-accelerated terrain
// rendering. It's kept for future use when full D3D context setup can be
// properly integrated. For now, the CPU colormap mode is the stable option.
// ======================================================================
/*
bool SwgMapRasterizer::renderTile(
	ObjectListCamera* camera,
	TerrainObject* terrain,
	float mapWidth,
	int tileX,
	int tileZ,
	int tilesPerSide,
	float cameraHeight,
	int tilePixelSize,
	const char* outputPath
)
{
	const float tileWidth = mapWidth / tilesPerSide;
	const float halfTile  = tileWidth / 2.0f;

	// Center of this tile in world space
	// Map coordinates: X and Z are horizontal, Y is vertical
	// Tile (0,0) is at map corner (-mapWidth/2, -mapWidth/2)
	const float centerX = -mapWidth / 2.0f + (tileX + 0.5f) * tileWidth;
	const float centerZ = -mapWidth / 2.0f + (tileZ + 0.5f) * tileWidth;

	printf("\n    Setting up camera for tile (%d,%d) at world (%.1f, %.1f)\n", 
		   tileX, tileZ, centerX, centerZ);

	// ======================================================================
	// Set camera transform: position high above terrain, looking straight down
	// ======================================================================
	Transform cameraTransform;
	
	// Camera coordinate frame:
	//   K (forward)  = world -Y (down, towards ground)
	//   J (up)       = world +Z (north)
	//   I (right)    = world +X (east)
	// This creates a top-down orthographic view
	cameraTransform.setLocalFrameKJ_p(
		Vector(0.0f, -1.0f, 0.0f),   // K = look down (world -Y)
		Vector(0.0f,  0.0f, 1.0f)    // J = north is up (world +Z)
	);
	
	// Position camera high above the tile center
	cameraTransform.setPosition_p(Vector(centerX, cameraHeight, centerZ));
	camera->setTransform_o2p(cameraTransform);

	printf("    Camera transform set: pos=(%.1f, %.1f, %.1f)\n", 
		   centerX, cameraHeight, centerZ);

	// ======================================================================
	// Set orthographic (parallel) projection
	// ======================================================================
	// Covers the tile area in world space:
	// X range: -halfTile to +halfTile (east-west)
	// Z range: -halfTile to +halfTile (north-south)
	// Near/far planes: handle terrain height variation
	
	camera->setNearPlane(1.0f);
	camera->setFarPlane(cameraHeight + 5000.0f);
	
	// Orthographic projection bounds (left, right, top, bottom)
	// In camera space, these map to world X and Z
	camera->setParallelProjection(-halfTile, halfTile, halfTile, -halfTile);

	printf("    Orthographic projection: bounds=[%.1f,%.1f] x [%.1f,%.1f]\n",
		   -halfTile, halfTile, halfTile, -halfTile);

	// ======================================================================
	// Ensure terrain chunks are generated before rendering
	// ======================================================================
	printf("    Pre-loading terrain chunks...\n");
	for (int warmup = 0; warmup < 10; ++warmup)
	{
		// Call alter to trigger terrain generation
		terrain->alter(0.05f);
		
		// Allow async operations and message processing
		Os::update();
	}

	// ======================================================================
	// Render frames to ensure stable output
	// ======================================================================
	printf("    Rendering frames...\n");
	for (int frame = 0; frame < 3; ++frame)
	{
		// Clear and render the scene
		camera->renderScene();
		
		// Allow time for rendering to complete
		Os::update();
	}

	// ======================================================================
	// Capture the final rendered frame as TGA image
	// ======================================================================
	printf("    Capturing screenshot to %s\n", outputPath);
	
	bool success = Graphics::screenShot(outputPath);
	
	if (success)
	{
		printf("    ✓ Tile (%d,%d) rendered successfully\n", tileX, tileZ);
	}
	else
	{
		printf("    ✗ Failed to capture screenshot for tile (%d,%d)\n", tileX, tileZ);
	}
	
	return success;
}
*/
// End of experimental GPU tile rendering

// ======================================================================
// TGA file writer
// ======================================================================

bool SwgMapRasterizer::saveTGA(const uint8* pixels, int width, int height, int channels, const char* filename)
{
	// Ensure output directory exists
	std::string path(filename);
	size_t lastSlash = path.find_last_of("/\\");
	if (lastSlash != std::string::npos)
	{
		std::string dir = path.substr(0, lastSlash);
		_mkdir(dir.c_str());
	}

	FILE* fp = fopen(filename, "wb");
	if (!fp)
	{
		printf("  ERROR: Cannot open file for writing: %s\n", filename);
		return false;
	}

	// TGA header
	uint8 header[18];
	memset(header, 0, sizeof(header));
	header[2]  = 2;                              // Uncompressed RGB
	header[12] = static_cast<uint8>(width & 0xFF);
	header[13] = static_cast<uint8>((width >> 8) & 0xFF);
	header[14] = static_cast<uint8>(height & 0xFF);
	header[15] = static_cast<uint8>((height >> 8) & 0xFF);
	header[16] = static_cast<uint8>(channels * 8); // Bits per pixel (24 or 32)
	header[17] = (channels == 4) ? 8 : 0;        // Image descriptor (alpha bits)

	fwrite(header, 1, sizeof(header), fp);

	// TGA stores pixels bottom-to-top, in BGR order
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const int srcOffset = (y * width + x) * channels;
			uint8 bgr[4];

			if (channels >= 3)
			{
				bgr[0] = pixels[srcOffset + 2]; // B
				bgr[1] = pixels[srcOffset + 1]; // G
				bgr[2] = pixels[srcOffset + 0]; // R
				if (channels == 4)
					bgr[3] = pixels[srcOffset + 3]; // A
			}
			else
			{
				bgr[0] = bgr[1] = bgr[2] = pixels[srcOffset];
			}

			fwrite(bgr, 1, channels, fp);
		}
	}

	// TGA footer
	const char tgaFooter[] = "\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0";
	fwrite(tgaFooter, 1, 26, fp);

	fclose(fp);
	return true;
}

// ======================================================================
// Downsample 2x: average each 2x2 block (src 2*outW x 2*outH -> out outW x outH, RGB)
// ======================================================================

void SwgMapRasterizer::downsample2xRGB(
	const uint8* src,
	int srcWidth,
	int srcHeight,
	uint8* out,
	int outWidth,
	int outHeight
)
{
	if (!src || !out || srcWidth != 2 * outWidth || srcHeight != 2 * outHeight)
		return;

	for (int y = 0; y < outHeight; ++y)
	{
		for (int x = 0; x < outWidth; ++x)
		{
			const int sx = 2 * x;
			const int sy = 2 * y;
			const int i00 = (sy     * srcWidth + sx) * 3;
			const int i10 = (sy     * srcWidth + sx + 1) * 3;
			const int i01 = ((sy+1) * srcWidth + sx) * 3;
			const int i11 = ((sy+1) * srcWidth + sx + 1) * 3;

			for (int c = 0; c < 3; ++c)
			{
				unsigned sum = src[i00 + c] + src[i10 + c] + src[i01 + c] + src[i11 + c];
				out[(y * outWidth + x) * 3 + c] = static_cast<uint8>((sum + 2) / 4);
			}
		}
	}
}

// ======================================================================
// Hillshading: applies directional lighting based on terrain normals
// to give the colormap a 3D appearance
// ======================================================================

void SwgMapRasterizer::applyHillshading(
	const float* heightMap,
	uint8* colorPixels,
	int width,
	int height,
	float mapWidth
)
{
	if (!heightMap || !colorPixels || width < 3 || height < 3)
		return;

	const float metersPerPixel = mapWidth / static_cast<float>(width);

	// Light direction (sun from upper-left, slightly elevated)
	// Normalized direction vector: (-1, -1, 2) normalized
	const float lightLen = sqrtf(1.0f + 1.0f + 4.0f);
	const float lightX = -1.0f / lightLen;
	const float lightZ = -1.0f / lightLen;
	const float lightY =  2.0f / lightLen;

	// Ambient + diffuse lighting parameters
	const float ambient  = 0.35f;
	const float diffuse  = 0.65f;

	for (int y = 1; y < height - 1; ++y)
	{
		for (int x = 1; x < width - 1; ++x)
		{
			// Calculate surface normal using central differences
			const float hL = heightMap[y * width + (x - 1)];
			const float hR = heightMap[y * width + (x + 1)];
			const float hD = heightMap[(y + 1) * width + x]; // down in image = south
			const float hU = heightMap[(y - 1) * width + x]; // up in image = north

			// Surface normal: cross product of tangent vectors
			// dX = (2*metersPerPixel, 0, hR - hL)
			// dZ = (0, 2*metersPerPixel, hU - hD)
			// Normal = dX cross dZ = (-2m*(hR-hL), -2m*(hU-hD), 4m^2)
			// But in our image coords, we need to be careful:
			// Image Y up = north, so hU (y-1) is north

			const float scale = 2.0f * metersPerPixel;
			float nx = -(hR - hL) / scale;
			float nz = -(hU - hD) / scale;
			float ny = 1.0f;

			// Normalize
			const float nLen = sqrtf(nx * nx + ny * ny + nz * nz);
			if (nLen > 0.0001f)
			{
				nx /= nLen;
				ny /= nLen;
				nz /= nLen;
			}

			// Dot product with light direction
			float dot = nx * lightX + ny * lightY + nz * lightZ;
			dot = std::max(0.0f, std::min(1.0f, dot));

			// Compute lighting factor
			float lighting = ambient + diffuse * dot;
			lighting = std::max(0.0f, std::min(1.5f, lighting)); // Allow slight over-brightening

			// Apply to color pixels
			const int offset = (y * width + x) * 3;
			for (int c = 0; c < 3; ++c)
			{
				float val = colorPixels[offset + c] * lighting;
				colorPixels[offset + c] = static_cast<uint8>(std::max(0.0f, std::min(255.0f, val)));
			}
		}
	}
}

// ======================================================================
