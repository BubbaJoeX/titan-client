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
#include "sharedFile/Iff.h"
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
#include "sharedTerrain/ShaderGroup.h"
#include "sharedTerrain/SetupSharedTerrain.h"
#include "sharedTerrain/TerrainGenerator.h"
#include "sharedTerrain/TerrainObject.h"
#include "sharedThread/SetupSharedThread.h"
#include "sharedUtility/SetupSharedUtility.h"
#include "sharedSynchronization/RecursiveMutex.h"
#include "sharedSynchronization/Guard.h"

// -- Engine client includes (for GPU rendering mode) --
#include "clientGraphics/Camera.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/ConfigClientGraphics.h"
#include "clientGraphics/ScreenShotHelper.h"
#include "clientGraphics/SetupClientGraphics.h"
#include "clientObject/ObjectListCamera.h"
#include "clientObject/SetupClientObject.h"
#include "clientTerrain/SetupClientTerrain.h"
#include "clientGraphics/Dds.h"
#include "ATI_Compress.h"
#include "fileInterface/AbstractFile.h"

// -- Standard library --
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <ctime>
#include <direct.h>
#include <map>
#include <string>
#include <vector>


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
	// Image::lockReadOnly is not thread-safe; quadrants sample shared ramp/texture Images in parallel.
	// RecursiveMutex (not std::mutex): VS2013 + STLport breaks on <mutex>/<type_traits>.
	RecursiveMutex s_shaderImageSamplingMutex;
	// Per-run cache for terrain diffuse loaded during rasterization (not owned by ShaderLayer preload)
	RecursiveMutex s_terrainLazyShaderTexturesMutex;
	std::map<std::string, Image*> s_terrainLazyShaderTextures;

	// One-time debug dump for the first sampled texel so we can verify pixel format
	// and raw byte channel ordering from Image/DDS loaders.
	static bool s_debugDumpDiffuseSampleOnce = true;

	// ======================================================================
	// DDS loading (SetupSharedImage only registers TargaFormat; game assets are .dds)
	// ======================================================================
	static bool matchesDdsPixelFormat(DDS::DDS_PIXELFORMAT const &a, DDS::DDS_PIXELFORMAT const &b)
	{
		return memcmp(&a, &b, sizeof(a)) == 0;
	}

	static size_t dxtBaseMipSize(int width, int height, bool dxt1)
	{
		int const bx = (width + 3) / 4;
		int const by = (height + 3) / 4;
		return static_cast<size_t>(bx * by * (dxt1 ? 8 : 16));
	}

	static Image* imageFromArgbRows(uint8* argbOwned, int w, int hgt)
	{
		static bool s_debugAtiOrderDumped = false;
		Image* img = new Image();
		// Match MayaExporter/DdsToTgaConverter: ATI_TC_FORMAT_ARGB_8888 is written to memory as BGRA
		// on little-endian, and Image expects PF_bgra_8888 with these channel masks.
		img->setDimensions(w, hgt, 32, 4);
		img->setPixelInformation(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
		uint8* dst = img->lock();
		int const stride = img->getStride();
		for (int y = 0; y < hgt; ++y)
		{
			uint8* rowOut = dst + y * stride;
			uint8 const* rowIn = argbOwned + y * w * 4;
			for (int x = 0; x < w; ++x)
			{
				rowOut[x * 4 + 0] = rowIn[x * 4 + 0]; // B
				rowOut[x * 4 + 1] = rowIn[x * 4 + 1]; // G
				rowOut[x * 4 + 2] = rowIn[x * 4 + 2]; // R
				rowOut[x * 4 + 3] = rowIn[x * 4 + 3]; // A

				if (!s_debugAtiOrderDumped && x == 0 && y == 0)
				{
					s_debugAtiOrderDumped = true;
					uint8 const b0 = rowIn[0];
					uint8 const g0 = rowIn[1];
					uint8 const r0 = rowIn[2];
					uint8 const a0 = rowIn[3];
					uint8 const outB = rowOut[0];
					uint8 const outG = rowOut[1];
					uint8 const outR = rowOut[2];
					uint8 const outA = rowOut[3];
					printf("  DEBUG ATI order: raw argbOwned[x=0,y=0]=[%u,%u,%u,%u] -> wrote outRGB=[%u,%u,%u] (Image PF_bgr_888)\n",
						b0, g0, r0, a0, outR, outG, outB);
					printf("  DEBUG ATI order: wrote outA=%u (Image now PF_bgra_8888)\n", outA);
				}
			}
		}
		img->unlock();
		img->setReadOnly();
		delete[] argbOwned;
		return img;
	}

	static Image* loadDdsImageFromPath(char const *filename)
	{
		AbstractFile* file = TreeFile::open(filename, AbstractFile::PriorityData, true);
		if (!file)
			return 0;

		unsigned long magic = 0;
		if (file->read(&magic, 4) != 4 || magic != DDS::MakeFourCC('D', 'D', 'S', ' '))
		{
			delete file;
			return 0;
		}

		DDS::DDS_HEADER header;
		if (file->read(&header, sizeof(header)) != static_cast<int>(sizeof(header)))
		{
			delete file;
			return 0;
		}

		if (header.dwComplexFlags & DDS::DDS_COMPLEX_FLAGS_CUBEMAP)
		{
			delete file;
			return 0;
		}

		int const w = static_cast<int>(header.dwWidth);
		int const hgt = static_cast<int>(header.dwHeight);
		if (w <= 0 || hgt <= 0)
		{
			delete file;
			return 0;
		}

		if (header.ddspf.dwFlags & DDS::DDS_FOURCC)
		{
			ATI_TC_FORMAT srcFmt = ATI_TC_FORMAT_ARGB_8888;
			bool dxt1 = false;
			if (header.ddspf.dwFourCC == DDS::DDSPF_DXT1.dwFourCC)
			{
				srcFmt = ATI_TC_FORMAT_DXT1;
				dxt1 = true;
			}
			else if (header.ddspf.dwFourCC == DDS::DDSPF_DXT3.dwFourCC)
				srcFmt = ATI_TC_FORMAT_DXT3;
			else if (header.ddspf.dwFourCC == DDS::DDSPF_DXT5.dwFourCC)
				srcFmt = ATI_TC_FORMAT_DXT5;
			else
			{
				delete file;
				return 0;
			}

			size_t const dxtSize = dxtBaseMipSize(w, hgt, dxt1);
			std::vector<ATI_TC_BYTE> dxtData(dxtSize);
			if (file->read(&dxtData[0], static_cast<int>(dxtSize)) != static_cast<int>(dxtSize))
			{
				delete file;
				return 0;
			}
			delete file;

			ATI_TC_Texture srcTex;
			memset(&srcTex, 0, sizeof(srcTex));
			srcTex.dwSize = sizeof(ATI_TC_Texture);
			srcTex.dwWidth = static_cast<ATI_TC_DWORD>(w);
			srcTex.dwHeight = static_cast<ATI_TC_DWORD>(hgt);
			srcTex.dwPitch = 0;
			srcTex.format = srcFmt;
			srcTex.dwDataSize = static_cast<ATI_TC_DWORD>(dxtSize);
			srcTex.pData = &dxtData[0];

			ATI_TC_Texture destTex;
			memset(&destTex, 0, sizeof(destTex));
			destTex.dwSize = sizeof(ATI_TC_Texture);
			destTex.dwWidth = srcTex.dwWidth;
			destTex.dwHeight = srcTex.dwHeight;
			destTex.dwPitch = static_cast<ATI_TC_DWORD>(w * 4);
			destTex.format = ATI_TC_FORMAT_ARGB_8888;
			destTex.dwDataSize = ATI_TC_CalculateBufferSize(&destTex);
			destTex.pData = new ATI_TC_BYTE[destTex.dwDataSize];

			ATI_TC_ERROR const err = ATI_TC_ConvertTexture(&srcTex, &destTex, 0, 0, 0, 0);
			if (err != ATI_TC_OK)
			{
				delete[] destTex.pData;
				return 0;
			}

			return imageFromArgbRows(reinterpret_cast<uint8*>(destTex.pData), w, hgt);
		}

		if (matchesDdsPixelFormat(header.ddspf, DDS::DDSPF_A8R8G8B8))
		{
			int pitch = 0;
			if (header.dwHeaderFlags & DDS::DDS_HEADER_FLAGS_PITCH)
				pitch = static_cast<int>(header.dwPitchOrLinearSize);
			else
				pitch = w * 4;

			Image* img = new Image();
			img->setDimensions(w, hgt, 24, 3);
			img->setPixelInformation(0x00ff0000, 0x0000ff00, 0x000000ff, 0);
			uint8* dst = img->lock();
			int const outStride = img->getStride();
			std::vector<uint8> row(static_cast<size_t>(pitch));
			for (int y = 0; y < hgt; ++y)
			{
				if (file->read(&row[0], pitch) != pitch)
				{
					img->unlock();
					delete img;
					delete file;
					return 0;
				}
				uint8* rowOut = dst + y * outStride;
				for (int x = 0; x < w; ++x)
				{
					// DDS::DDSPF_A8R8G8B8 pixel order is 0xAARRGGBB in a 32-bit value.
					// DDS pixel data is little-endian byte order, so file bytes are: B, G, R, A.
					// Our Image uses PF_bgr_888 memory layout (offset0=blue), so copy B,G,R directly.
					rowOut[x * 3 + 0] = row[x * 4 + 0]; // B
					rowOut[x * 3 + 1] = row[x * 4 + 1]; // G
					rowOut[x * 3 + 2] = row[x * 4 + 2]; // R
				}
			}
			img->unlock();
			img->setReadOnly();
			delete file;
			return img;
		}

		if (matchesDdsPixelFormat(header.ddspf, DDS::DDSPF_R8G8B8))
		{
			int pitch = 0;
			if (header.dwHeaderFlags & DDS::DDS_HEADER_FLAGS_PITCH)
				pitch = static_cast<int>(header.dwPitchOrLinearSize);
			else
				pitch = ((w * 3) + 3) & ~3;

			Image* img = new Image();
			img->setDimensions(w, hgt, 24, 3);
			img->setPixelInformation(0x00ff0000, 0x0000ff00, 0x000000ff, 0);
			uint8* dst = img->lock();
			int const outStride = img->getStride();
			std::vector<uint8> row(static_cast<size_t>(pitch));
			for (int y = 0; y < hgt; ++y)
			{
				if (file->read(&row[0], pitch) != pitch)
				{
					img->unlock();
					delete img;
					delete file;
					return 0;
				}
				uint8* rowOut = dst + y * outStride;
				for (int x = 0; x < w; ++x)
				{
					// DDS::DDSPF_R8G8B8 pixel order is 0x00RRGGBB in a 32-bit value.
					// DDS pixel data is little-endian byte order, so file bytes are: B, G, R.
					// Our Image uses PF_bgr_888 memory layout, so copy B,G,R directly.
					rowOut[x * 3 + 0] = row[x * 3 + 0]; // B
					rowOut[x * 3 + 1] = row[x * 3 + 1]; // G
					rowOut[x * 3 + 2] = row[x * 3 + 2]; // R
				}
			}
			img->unlock();
			img->setReadOnly();
			delete file;
			return img;
		}

		delete file;
		return 0;
	}

	static void swapTgaExtensionToDds(char const *pathIn, char* pathOut, size_t pathOutLen)
	{
		if (!pathIn || !pathOut || pathOutLen == 0)
			return;
		strncpy(pathOut, pathIn, pathOutLen);
		pathOut[pathOutLen - 1] = '\0';
		size_t const n = strlen(pathOut);
		if (n >= 4 && _stricmp(pathOut + n - 4, ".tga") == 0)
			strcpy(pathOut + n - 4, ".dds");
	}

	/** Prefer .dds (try path with .tga replaced first), then Targa for .tga, else ImageFormatList. */
	static Image* loadTextureImageForMap(char const *path)
	{
		if (!path || !path[0])
			return 0;

		char ddsPath[512];
		swapTgaExtensionToDds(path, ddsPath, sizeof(ddsPath));
		if (_stricmp(path, ddsPath) != 0)
		{
			Image* img = loadDdsImageFromPath(ddsPath);
			if (img)
				return img;
		}

		Image* img = loadDdsImageFromPath(path);
		if (img)
			return img;

		size_t const plen = strlen(path);
		if (plen >= 4 && _stricmp(path + plen - 4, ".tga") == 0)
		{
			TargaFormat tga;
			if (tga.loadImage(path, &img))
				return img;
			return 0;
		}

		img = ImageFormatList::loadImage(path);
		if (img)
			return img;

		TargaFormat tga;
		if (tga.loadImage(path, &img))
			return img;
		return 0;
	}

	// ======================================================================
	// Helper: Read a single pixel from an image
	// ======================================================================
	static PackedRgb readPixelFromImage(const Image* image, int x, int y)
	{
		PackedRgb result = PackedRgb::solidBlack;

		if (!image || x < 0 || x >= image->getWidth() || y < 0 || y >= image->getHeight())
			return result;

		Guard const guard(s_shaderImageSamplingMutex);

		const uint8* data = image->lockReadOnly();
		Image::UnlockGuard unlockGuard(image);

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

		return result;
	}

	// Defined below; used by ramp sampling and helpers above it in this file.
	static PackedRgb sampleTextureAtUV(Image const *image, float u, float v);

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

		Guard const guard(s_shaderImageSamplingMutex);

		// Use cached ramp image if already loaded (avoids loading the same file per pixel)
		{
			auto rampIt = s_shaderRampImageCache.find(familyName);
			if (rampIt != s_shaderRampImageCache.end() && rampIt->second)
			{
				Image* image = rampIt->second;
				if (image->getWidth() > 0 && image->getHeight() > 0)
				{
					float const u = std::max(0.f, std::min(1.f, childChoice));
					return sampleTextureAtUV(image, u, 0.5f);
				}
			}
		}

		// Color ramps: terrain/colorramp/<name> then terrain/<name> (.dds preferred; see loadTextureImageForMap)
		char rampPath[256];
		Image* image = NULL;

		_snprintf(rampPath, sizeof(rampPath), "terrain/colorramp/%s.tga", familyName);
		image = loadTextureImageForMap(rampPath);
		if (!image)
		{
			_snprintf(rampPath, sizeof(rampPath), "terrain/%s.tga", familyName);
			image = loadTextureImageForMap(rampPath);
		}
		if (!image)
		{
			s_shaderFamilyColorCache[familyName] = terrainFallback;
			return terrainFallback;
		}

		// Cache the loaded ramp image so we never load it again for this run
		s_shaderRampImageCache[familyName] = image;

		// Bilinear sample along the ramp (same as terrain/water DDS path; avoids single-texel choice)
		float const u = std::max(0.f, std::min(1.f, childChoice));
		return sampleTextureAtUV(image, u, 0.5f);
	}

	// Call at end of render to free cached ramp images and avoid leaking / stale data
	static void clearShaderRampImageCache()
	{
		Guard const guard(s_shaderImageSamplingMutex);
		for (auto it = s_shaderRampImageCache.begin(); it != s_shaderRampImageCache.end(); ++it)
		{
			if (it->second)
				delete it->second;
		}
		s_shaderRampImageCache.clear();
	}

	// ======================================================================
	// Edge-aware pixel easing (softens harsh color transitions)
	// ======================================================================
	static inline float smoothStep01(float x)
	{
		x = std::max(0.0f, std::min(1.0f, x));
		return x * x * (3.0f - 2.0f * x);
	}

	static void applyPixelEasing(uint8* colorPixels, int width, int height, float strength)
	{
		if (!colorPixels || width < 3 || height < 3 || strength <= 0.0f)
			return;

		const size_t totalBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 3u;
		std::vector<uint8> src(totalBytes);
		memcpy(&src[0], colorPixels, totalBytes);

		// Only ease where local contrast is high (hard borders). Keep interiors crisp.
		const float contrastLow = 0.08f;
		const float contrastHigh = 0.24f;

		for (int y = 1; y < height - 1; ++y)
		{
			for (int x = 1; x < width - 1; ++x)
			{
				int sumR = 0;
				int sumG = 0;
				int sumB = 0;

				for (int oy = -1; oy <= 1; ++oy)
				{
					for (int ox = -1; ox <= 1; ++ox)
					{
						const int ni = ((y + oy) * width + (x + ox)) * 3;
						sumR += src[ni + 0];
						sumG += src[ni + 1];
						sumB += src[ni + 2];
					}
				}

				const int i = (y * width + x) * 3;
				const float r0 = static_cast<float>(src[i + 0]);
				const float g0 = static_cast<float>(src[i + 1]);
				const float b0 = static_cast<float>(src[i + 2]);

				const float avgR = sumR / 9.0f;
				const float avgG = sumG / 9.0f;
				const float avgB = sumB / 9.0f;

				const float contrast =
					(std::fabs(r0 - avgR) + std::fabs(g0 - avgG) + std::fabs(b0 - avgB)) / (3.0f * 255.0f);

				const float t = (contrast - contrastLow) / (contrastHigh - contrastLow);
				const float ease = strength * smoothStep01(t);

				const float r = r0 + (avgR - r0) * ease;
				const float g = g0 + (avgG - g0) * ease;
				const float b = b0 + (avgB - b0) * ease;

				colorPixels[i + 0] = static_cast<uint8>(std::max(0.0f, std::min(255.0f, r)));
				colorPixels[i + 1] = static_cast<uint8>(std::max(0.0f, std::min(255.0f, g)));
				colorPixels[i + 2] = static_cast<uint8>(std::max(0.0f, std::min(255.0f, b)));
			}
		}
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

	// Apply sampled shader texture as scaled detail over an authored family tint.
	// This keeps terrain color identity while ensuring UV-scaled shader detail is visible.
	static PackedRgb applyTintedShaderDetail(const PackedRgb& familyTint, const PackedRgb& shaderSample)
	{
		const float lum =
			(0.299f * shaderSample.r + 0.587f * shaderSample.g + 0.114f * shaderSample.b) / 255.0f;

		// Detail multiplier from texture luminance; centered near 1.0 with room for dark/light relief.
		const float detailMul = 0.70f + 0.60f * lum; // ~0.70 .. 1.30 (brighter, less crush)

		PackedRgb detailed;
		detailed.r = static_cast<uint8>(std::max(0.0f, std::min(255.0f, familyTint.r * detailMul)));
		detailed.g = static_cast<uint8>(std::max(0.0f, std::min(255.0f, familyTint.g * detailMul)));
		detailed.b = static_cast<uint8>(std::max(0.0f, std::min(255.0f, familyTint.b * detailMul)));

		// Keep a little sampled chroma (if present), but avoid washing out authored family tint.
		return blendColors(detailed, shaderSample, 0.16f);
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
		Image* familyTexture;   // fallback: texture/<familyName> or when family has no children
		// One entry per ShaderGroup child index (same order as getFamilyChild(id, idx))
		std::vector<Image*> childTextures;

		ShaderLayer() : familyId(0), priority(0), layerWeight(0.0f), shaderSize(2.0f), familyTexture(0) {}
	};

	static int wrapTextureCoord(int c, int dim)
	{
		int m = c % dim;
		if (m < 0)
			m += dim;
		return m;
	}

	// Decode one pixel from a locked image buffer (no lock)
	static void decodeRgbFromLockedRow(Image const *image, uint8 const *p, PackedRgb &out)
	{
		Image::PixelFormat const pf = image->getPixelFormat();
		int const bpp = image->getBytesPerPixel();

		// Explicitly handle the Image pixel formats used by the game's DDS/TGA loaders.
		// Some loaders keep 4-channel formats (ARGB/BGRA), and the previous fallback treated alpha as R.
		switch (pf)
		{
		case Image::PF_bgr_888:
		case Image::PF_bgra_8888:
			// memory: B,G,R,(A)
			out.b = p[0];
			out.g = p[1];
			out.r = p[2];
			break;

		case Image::PF_rgb_888:
		case Image::PF_rgba_8888:
			// memory: R,G,B,(A)
			out.r = p[0];
			out.g = p[1];
			out.b = p[2];
			break;

		case Image::PF_argb_8888:
			// memory: A,R,G,B
			out.r = (bpp >= 4) ? p[1] : p[0];
			out.g = (bpp >= 4) ? p[2] : p[0];
			out.b = (bpp >= 4) ? p[3] : p[0];
			break;

		case Image::PF_w_8:
			// 8-bit greyscale bitmap
			out.r = out.g = out.b = p[0];
			break;

		default:
			// Best-effort fallback: interpret first 3 bytes as RGB.
			out.r = p[0];
			out.g = (bpp > 1) ? p[1] : p[0];
			out.b = (bpp > 2) ? p[2] : p[0];
			break;
		}
	}

	// ======================================================================
	// Sample a texture at UV with wrap + bilinear (one lock)
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

		float const fx = u * static_cast<float>(w) - 0.5f;
		float const fy = v * static_cast<float>(h) - 0.5f;
		int x0 = static_cast<int>(floorf(fx));
		int y0 = static_cast<int>(floorf(fy));
		float const tx = fx - static_cast<float>(x0);
		float const ty = fy - static_cast<float>(y0);

		int const xa = wrapTextureCoord(x0, w);
		int const xb = wrapTextureCoord(x0 + 1, w);
		int const ya = wrapTextureCoord(y0, h);
		int const yb = wrapTextureCoord(y0 + 1, h);

		Guard const guard(s_shaderImageSamplingMutex);
		uint8 const *const base = image->lockReadOnly();
		Image::UnlockGuard unlockGuard(image);
		int const stride = image->getStride();
		int const bpp = image->getBytesPerPixel();

		PackedRgb c00, c10, c01, c11;
		uint8 const* p00 = base + ya * stride + xa * bpp;
		if (s_debugDumpDiffuseSampleOnce)
		{
			s_debugDumpDiffuseSampleOnce = false;
			int const pf = static_cast<int>(image->getPixelFormat());
			uint8 const raw0 = p00[0];
			uint8 const raw1 = (bpp > 1) ? p00[1] : 0;
			uint8 const raw2 = (bpp > 2) ? p00[2] : 0;
			uint8 const raw3 = (bpp > 3) ? p00[3] : 0;
			printf("  DEBUG diffuse sample: texPf=%d bpp=%d stride=%d tex=%dx%d sampleUV=(%.4f,%.4f) xy=(%d,%d) raw=[%u,%u,%u,%u]\n",
				pf, bpp, stride, image->getWidth(), image->getHeight(), u, v, xa, ya, raw0, raw1, raw2, raw3);
		}

		decodeRgbFromLockedRow(image, p00, c00);
		decodeRgbFromLockedRow(image, base + ya * stride + xb * bpp, c10);
		decodeRgbFromLockedRow(image, base + yb * stride + xa * bpp, c01);
		decodeRgbFromLockedRow(image, base + yb * stride + xb * bpp, c11);

		PackedRgb r;
		r.r = static_cast<uint8>((1.f - tx) * (1.f - ty) * c00.r + tx * (1.f - ty) * c10.r + (1.f - tx) * ty * c01.r + tx * ty * c11.r + 0.5f);
		r.g = static_cast<uint8>((1.f - tx) * (1.f - ty) * c00.g + tx * (1.f - ty) * c10.g + (1.f - tx) * ty * c01.g + tx * ty * c11.g + 0.5f);
		r.b = static_cast<uint8>((1.f - tx) * (1.f - ty) * c00.b + tx * (1.f - ty) * c10.b + (1.f - tx) * ty * c01.b + tx * ty * c11.b + 0.5f);
		return r;
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
	// Map CPU sampling: .sht IFF -> first texture path, else texture/<basename>.tga
	// (Used for terrain shader families and water.)
	// ======================================================================

	static bool shaderTemplateToBasename(char const *shaderTemplate, char *baseOut, size_t baseOutLen)
	{
		if (!shaderTemplate || !shaderTemplate[0] || !baseOut || baseOutLen < 2)
			return false;
		std::string base(shaderTemplate);
		size_t slash = base.find_last_of("/\\");
		if (slash != std::string::npos)
			base = base.substr(slash + 1);
		size_t dot = base.find_last_of('.');
		if (dot != std::string::npos)
			base = base.substr(0, dot);
		if (base.empty())
			return false;
		_snprintf(baseOut, baseOutLen, "%s", base.c_str());
		baseOut[baseOutLen - 1] = '\0';
		return true;
	}

	static bool shaderTemplateToDefaultTextureTgaPath(char const* shaderTemplate, char* outPath, size_t outLen)
	{
		char base[256];
		if (!shaderTemplateToBasename(shaderTemplate, base, sizeof(base)))
			return false;
		_snprintf(outPath, outLen, "texture/%s.tga", base);
		outPath[outLen - 1] = '\0';
		return true;
	}

	static bool pathLooksLikeGameTextureAsset(char const *p)
	{
		if (!p || !p[0])
			return false;
		if (_strnicmp(p, "effect/", 7) == 0 || _strnicmp(p, "shader/", 7) == 0)
			return false;
		if (_strnicmp(p, "texture/", 8) == 0)
			return true;
		size_t const n = strlen(p);
		if (n >= 4 && (_stricmp(p + n - 4, ".dds") == 0 || _stricmp(p + n - 4, ".tga") == 0))
			return true;
		return false;
	}

	static bool scanIffForFirstTextureAssetName(Iff &iff, char *out, size_t outLen)
	{
		while (!iff.atEndOfForm())
		{
			if (iff.isCurrentForm())
			{
				iff.enterForm();
				bool const found = scanIffForFirstTextureAssetName(iff, out, outLen);
				iff.exitForm();
				if (found)
					return true;
				continue;
			}
			if (iff.isCurrentChunk())
			{
				if (iff.getCurrentName() == TAG_NAME)
				{
					iff.enterChunk(TAG_NAME);
					char buf[512];
					iff.read_string(buf, sizeof(buf));
					iff.exitChunk(TAG_NAME);
					if (pathLooksLikeGameTextureAsset(buf))
					{
						strncpy(out, buf, outLen);
						out[outLen - 1] = '\0';
						return true;
					}
				}
				else
				{
					iff.enterChunk();
					iff.exitChunk();
				}
				continue;
			}
			break;
		}
		return false;
	}

	static bool resolveShaderTemplateToTextureLoadPath(char const *shaderTemplate, char *outPath, size_t outLen)
	{
		if (!shaderTemplate || !shaderTemplate[0] || !outPath || outLen < 16)
			return false;

		if (pathLooksLikeGameTextureAsset(shaderTemplate))
		{
			strncpy(outPath, shaderTemplate, outLen);
			outPath[outLen - 1] = '\0';
			return true;
		}

		size_t const sl = strlen(shaderTemplate);
		if (sl >= 4 && _stricmp(shaderTemplate + sl - 4, ".sht") == 0)
		{
			Iff iff;
			if (iff.open(shaderTemplate, true) && scanIffForFirstTextureAssetName(iff, outPath, outLen))
				return true;
		}

		// Data often stores a bare shader name (e.g. wter_nboo_beach); game asset is shader/<name>.sht
		{
			char base[256];
			char shtPath[512];
			if (shaderTemplateToBasename(shaderTemplate, base, sizeof(base)))
			{
				_snprintf(shtPath, sizeof(shtPath), "shader/%s.sht", base);
				shtPath[sizeof(shtPath) - 1] = '\0';
				if (_stricmp(shaderTemplate, shtPath) != 0)
				{
					Iff iff;
					if (iff.open(shtPath, true) && scanIffForFirstTextureAssetName(iff, outPath, outLen))
						return true;
				}
			}
		}

		return shaderTemplateToDefaultTextureTgaPath(shaderTemplate, outPath, outLen);
	}

	// Shader family name, child shaderTemplateName, or water template: resolve path, load; if missing try texture/<basename>.dds then .tga.
	static Image* loadTextureImageForShaderTemplateName(char const *shaderTemplateName)
	{
		if (!shaderTemplateName || !shaderTemplateName[0])
			return 0;

		char path[512];
		if (resolveShaderTemplateToTextureLoadPath(shaderTemplateName, path, sizeof(path)))
		{
			Image* const img = loadTextureImageForMap(path);
			if (img)
				return img;
		}

		char base[256];
		if (!shaderTemplateToBasename(shaderTemplateName, base, sizeof(base)))
			return 0;

		_snprintf(path, sizeof(path), "texture/%s.dds", base);
		Image* img = loadTextureImageForMap(path);
		if (img)
			return img;

		_snprintf(path, sizeof(path), "texture/%s.tga", base);
		return loadTextureImageForMap(path);
	}

	static void clearTerrainLazyShaderTextures()
	{
		Guard const g(s_terrainLazyShaderTexturesMutex);
		for (std::map<std::string, Image*>::iterator it = s_terrainLazyShaderTextures.begin();
		     it != s_terrainLazyShaderTextures.end(); ++it)
		{
			if (it->second)
				delete it->second;
		}
		s_terrainLazyShaderTextures.clear();
	}

	// Same loading chain as loadTextureImageForShaderTemplateName / water; only for lazy loads (cache owns Image*).
	static Image* getOrLoadTerrainShaderTextureLazy(char const *shaderKey)
	{
		if (!shaderKey || !shaderKey[0])
			return 0;
		std::string const key(shaderKey);
		Guard const g(s_terrainLazyShaderTexturesMutex);
		std::map<std::string, Image*>::iterator const it = s_terrainLazyShaderTextures.find(key);
		if (it != s_terrainLazyShaderTextures.end())
			return it->second;
		Image* const img = loadTextureImageForShaderTemplateName(shaderKey);
		s_terrainLazyShaderTextures[key] = img;
		return img;
	}

	// Pick diffuse image: preloaded child/family from buildShaderLayers, else lazy-load by child template or family name (water-style resolution).
	static Image const *resolveTerrainDiffuseMapTexture(
		ShaderLayer const *layer,
		ShaderGroup const *shaderGroup,
		ShaderGroup::Info const &sgi,
		int familyId)
	{
		if (!layer || !shaderGroup || familyId <= 0)
			return 0;

		int const nChildren = shaderGroup->getFamilyNumberOfChildren(familyId);
		if (nChildren > 0)
		{
			int const childIdx = shaderGroup->createShader(sgi);
			if (childIdx >= 0 && childIdx < nChildren)
			{
				if (childIdx < static_cast<int>(layer->childTextures.size()))
				{
					Image *const pre = layer->childTextures[static_cast<size_t>(childIdx)];
					if (pre)
						return pre;
				}
				ShaderGroup::FamilyChildData const child = shaderGroup->getFamilyChild(familyId, childIdx);
				if (child.shaderTemplateName && child.shaderTemplateName[0])
					return getOrLoadTerrainShaderTextureLazy(child.shaderTemplateName);
			}
		}

		if (layer->familyTexture)
			return layer->familyTexture;

		if (!layer->familyName.empty())
			return getOrLoadTerrainShaderTextureLazy(layer->familyName.c_str());

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
			layer.shaderSize = (shaderSize > 0.1f) ? shaderSize : 2.0f;
			layer.familyTexture = 0;
			layer.childTextures.clear();

			if (shaderSize > 100.0f)
				layer.layerWeight = 0.4f;
			else if (shaderSize < 10.0f)
				layer.layerWeight = 0.2f;

			// Same resolution as child shaders and water: shader/<family>.sht IFF → texture,
			// then texture/<basename>.dds / .tga (family names are usually the shader basename).
			if (!layer.familyName.empty())
			{
				layer.familyTexture = loadTextureImageForShaderTemplateName(layer.familyName.c_str());
				if (layer.familyTexture)
					printf("      -> family texture loaded: %s (shader/<name>.sht or texture/<name>.dds/.tga)\n", layer.familyName.c_str());
			}

			if (numChildren > 0)
			{
				layer.childTextures.resize(static_cast<size_t>(numChildren), static_cast<Image*>(0));
				for (int ci = 0; ci < numChildren; ++ci)
				{
					ShaderGroup::FamilyChildData const child = shaderGroup.getFamilyChild(familyId, ci);
					if (!child.shaderTemplateName || !child.shaderTemplateName[0])
						continue;
					Image* const img = loadTextureImageForShaderTemplateName(child.shaderTemplateName);
					if (img)
					{
						layer.childTextures[static_cast<size_t>(ci)] = img;
						printf("      -> child[%d] texture loaded (shader %s; .sht path or texture/<basename>.dds/.tga)\n", ci, child.shaderTemplateName);
					}
				}
			}

			layers.push_back(layer);
		}

		printf("  Shader family enumeration complete.\n\n");

		return layers;
	}

	// ======================================================================
	// Planet-map water: same heights/types as game (global + boundary + ribbon),
	// tint using water shader's texture/ DDS when available.
	// ======================================================================

	static void mapPixelToWorldXZ(int ix, int iy, int w, int h, float mapWidth, float& worldX, float& worldZ)
	{
		const float half = mapWidth * 0.5f;
		const float fx = (static_cast<float>(ix) + 0.5f) / static_cast<float>(w);
		const float fz = (static_cast<float>(h - 1 - iy) + 0.5f) / static_cast<float>(h);
		worldX = -half + fx * mapWidth;
		worldZ = -half + fz * mapWidth;
	}

	// Cached water texture + UV scale (must match texture we actually loaded; global fallback uses planet shader size)
	struct PlanetWaterTextureEntry
	{
		Image* image;
		float  shaderSizeForUv;

		PlanetWaterTextureEntry() : image(0), shaderSizeForUv(2.f) {}
	};

	// Resolve .sht -> texture path, prefer DDS via loadTextureImageForMap. Local/boundary shaders that fail
	// still get the planet global water texture when enabled (same logic as getWaterSurfaceAt shader name).
	static void resolveAndLoadPlanetWaterTexture(
		ProceduralTerrainAppearanceTemplate const* terrainTemplate,
		char const* shaderTemplateName,
		float shaderSize,
		PlanetWaterTextureEntry& outEntry)
	{
		outEntry.image = 0;
		outEntry.shaderSizeForUv = (shaderSize > 1e-4f) ? shaderSize : 2.f;

		if (shaderTemplateName && shaderTemplateName[0])
			outEntry.image = loadTextureImageForShaderTemplateName(shaderTemplateName);

		if (!outEntry.image && terrainTemplate && terrainTemplate->getUseGlobalWaterTable())
		{
			char const* const gname = terrainTemplate->getGlobalWaterTableShaderTemplateName();
			if (gname && gname[0])
			{
				outEntry.image = loadTextureImageForShaderTemplateName(gname);
				if (outEntry.image)
				{
					float const gs = terrainTemplate->getGlobalWaterTableShaderSize();
					outEntry.shaderSizeForUv = (gs > 1e-4f) ? gs : 2.f;
				}
			}
		}
	}

	static void applyWaterToPlanetMap(
		ProceduralTerrainAppearanceTemplate const* terrainTemplate,
		float mapWidth,
		int pixelWidth,
		int pixelHeight,
		float const* heightPixels,
		uint8* colorPixels)
	{
		if (!terrainTemplate || !terrainTemplate->hasAnyWaterSurface() || !heightPixels || !colorPixels
			|| pixelWidth < 1 || pixelHeight < 1)
			return;

		double const megapixels = (static_cast<double>(pixelWidth) * static_cast<double>(pixelHeight)) / 1000000.0;
		printf("  Water pass (CPU): scanning %d x %d (~%.2f M pixels).\n", pixelWidth, pixelHeight, megapixels);
		printf("    Each pixel: world position -> getWaterSurfaceAt (global + boundary tables + ribbons); if terrain height <= surface, tint with DDS/bilinear.\n");
		printf("    Large maps are slow: one getWaterSurfaceAt + possible tint per pixel, single-threaded. Progress updates every few rows — not frozen.\n");
		fflush(stdout);

		// Layering matches ProceduralTerrainAppearanceTemplate::getWaterSurfaceAt (global baseline,
		// higher local tables override, ribbon/ribbon-cap last). We only tint pixels below winning surface.
		std::map<std::string, PlanetWaterTextureEntry> waterTexByShader;

		long long pixelsProcessed = 0;
		long long surfaceHits = 0;       // getWaterSurfaceAt returned true
		long long underwaterTinted = 0; // actually blended water/lava

		clock_t const clockStart = clock();
		int const reportEveryRows = std::max(1, pixelHeight / 64);

		for (int iy = 0; iy < pixelHeight; ++iy)
		{
			if (iy % reportEveryRows == 0 || iy == pixelHeight - 1)
			{
				int const pct = (pixelHeight > 0) ? ((iy + 1) * 100 / pixelHeight) : 100;
				printf("\r  Water: %3d%%  row %6d / %d  |  surfaceOK %lld  tinted %lld   ",
					pct, iy + 1, pixelHeight,
					static_cast<long long>(surfaceHits),
					static_cast<long long>(underwaterTinted));
				fflush(stdout);
			}

			for (int ix = 0; ix < pixelWidth; ++ix)
			{
				++pixelsProcessed;
				const int idx = iy * pixelWidth + ix;
				const float h = heightPixels[idx];

				float worldX = 0.f;
				float worldZ = 0.f;
				mapPixelToWorldXZ(ix, iy, pixelWidth, pixelHeight, mapWidth, worldX, worldZ);
				Vector const pos(worldX, 0.f, worldZ);

				float wh = 0.f;
				TerrainGeneratorWaterType wtype = TGWT_invalid;
				char const* shaderTemplateName = 0;
				float shaderSize = 2.f;

				if (!terrainTemplate->getWaterSurfaceAt(pos, wh, wtype, shaderTemplateName, shaderSize))
					continue;
				++surfaceHits;
				if (h > wh)
					continue;

				const int offset = idx * 3;
				const float depth = wh - h;
				const float waterFactor = std::min(depth / 30.0f, 0.85f);

				PackedRgb waterColor(15, 50, 110);
				if (wtype == TGWT_lava)
					waterColor = PackedRgb(200, 80, 20);

				// Sample planet/global water DDS (bilinear). Lava without a shader template keeps solid tint
				// (matches prior behavior; avoids painting lava with the ocean global texture).
				const bool tryWaterTex =
					!(wtype == TGWT_lava && (!shaderTemplateName || !shaderTemplateName[0]));
				if (tryWaterTex)
				{
					std::string const key(shaderTemplateName && shaderTemplateName[0] ? shaderTemplateName : "__default_water__");
					PlanetWaterTextureEntry& entry = waterTexByShader[key];
					if (!entry.image)
					{
						printf("\n    Loading water texture cache entry \"%s\" (shader template: %s, size %.2f m)...\n",
							key.c_str(),
							(shaderTemplateName && shaderTemplateName[0]) ? shaderTemplateName : "(none / global fallback path)",
							shaderSize);
						fflush(stdout);
						resolveAndLoadPlanetWaterTexture(terrainTemplate, shaderTemplateName, shaderSize, entry);
						if (entry.image)
						{
							printf("    -> OK: %d x %d, UV tile %.2f m (bilinear/wrap)\n",
								entry.image->getWidth(), entry.image->getHeight(), entry.shaderSizeForUv);
						}
						else
							printf("    -> No file resolved; using solid water tint for this key.\n");
						fflush(stdout);
					}
					// Match ClientGlobalWaterManager2::LocalShaderPrimitive::_fillGraphicsBuffersFFP base UVs
					if (entry.image)
						waterColor = sampleTextureAtUV(entry.image, (-worldX) / entry.shaderSizeForUv, worldZ / entry.shaderSizeForUv);
				}

				colorPixels[offset + 0] = static_cast<uint8>(colorPixels[offset + 0] * (1.0f - waterFactor) + waterColor.r * waterFactor);
				colorPixels[offset + 1] = static_cast<uint8>(colorPixels[offset + 1] * (1.0f - waterFactor) + waterColor.g * waterFactor);
				colorPixels[offset + 2] = static_cast<uint8>(colorPixels[offset + 2] * (1.0f - waterFactor) + waterColor.b * waterFactor);
				++underwaterTinted;
			}
		}

		double const secs = (clock() - clockStart) / static_cast<double>(CLOCKS_PER_SEC);
		int texCached = 0;
		for (std::map<std::string, PlanetWaterTextureEntry>::iterator it = waterTexByShader.begin(); it != waterTexByShader.end(); ++it)
		{
			if (it->second.image)
				++texCached;
		}

		printf("\n  Water pass finished in %.2f s: %lld tinted pixels (terrain at/below water surface), %lld px where getWaterSurfaceAt applied, %lld px scanned, %d cached textures.\n",
			secs,
			static_cast<long long>(underwaterTinted),
			static_cast<long long>(surfaceHits),
			static_cast<long long>(pixelsProcessed),
			texCached);

		for (std::map<std::string, PlanetWaterTextureEntry>::iterator it = waterTexByShader.begin(); it != waterTexByShader.end(); ++it)
			if (it->second.image)
				delete it->second.image;
		waterTexByShader.clear();
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
	imageSize(2048),
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
	printf("  \n");
	printf("  SWG: Source");
	printf("========================================================\n\n");
}

// ----------------------------------------------------------------------

void SwgMapRasterizer::printUsage()
{
	printf("Usage: SwgMapRasterizer.exe [options]\n\n");
	printf("Options:\n");
	printf("  -terrain <file.trn>   Terrain file to rasterize\n");
	printf("  -output <dir>         Output directory (default: ./maps)\n");
	printf("  -size <pixels>        Output image size, square (default: 2048, max: 16384)\n");
	printf("  -tiles <n>            Tile grid divisions per side (default: 4, max: 128); colormap auto-raises\n");
	printf("                        tiles if needed so internal resolution stays ~1024 px/tile (4K + AA safe).\n");
	printf("  -height <meters>      Camera height for GPU mode (default: 20000)\n");
	printf("  -colormap             Use CPU colormap mode (default)\n");
	printf("  -render               Use GPU rendered mode\n");
	printf("  -all                  Process all known planet terrains\n");
	printf("  -config <file.cfg>    Engine config file (default: client.cfg)\n");
	printf("  -aa                   Enable anti-aliasing (2x supersample, default)\n");
	printf("  -noaa                 Disable anti-aliasing (required for -size above 8192 in colormap mode)\n");
	printf("  -help                 Show this help message\n");
	printf("\n");
	printf("\nExamples:\n");
	printf("  SwgMapRasterizer.exe -terrain terrain/tatooine.trn\n");
	printf("  SwgMapRasterizer.exe -all -size 4096 -tiles 8\n");
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
			if (config.imageSize > 16384) config.imageSize = 16384;
		}
		else if ((arg == "-tiles") && i + 1 < args.size())
		{
			config.tilesPerSide = atoi(args[++i].c_str());
			if (config.tilesPerSide < 1) config.tilesPerSide = 1;
			if (config.tilesPerSide > 128) config.tilesPerSide = 128;
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

	// Output size policy: generated map dimensions must be divisible by 1024.
	if (config.imageSize % 1024 != 0)
	{
		int adjusted = (config.imageSize / 1024) * 1024; // round down
		if (adjusted < 1024)
			adjusted = 1024;
		printf("NOTE: Adjusted image size %d -> %d to satisfy 1024 divisibility requirement.\n",
			config.imageSize, adjusted);
		config.imageSize = adjusted;
	}

	// CPU colormap + AA uses a 2x internal buffer; 8K output => 16K internal, which is too large
	// for a 32-bit process (height map alone ~1GB).
	if (config.useColormapMode && config.antiAlias && config.imageSize >= 8192)
	{
		printf("NOTE: CPU colormap anti-aliasing is disabled for image size %d (>= 8192). Use -noaa to silence this.\n",
			config.imageSize);
		config.antiAlias = false;
	}

	// Additional memory guard for 32-bit: estimate core internal buffers
	// (color RGB + height float + easing temp RGB) and disable AA when too large.
	if (config.useColormapMode && config.antiAlias)
	{
		const unsigned long long internal = static_cast<unsigned long long>(config.imageSize) * 2ull;
		const unsigned long long pixels = internal * internal;
		const unsigned long long estimatedBytes = pixels * 10ull; // 3 + 4 + 3
		const unsigned long long maxSafeBytes = 1200ull * 1024ull * 1024ull; // ~1.2 GB working set
		if (estimatedBytes > maxSafeBytes)
		{
			printf("NOTE: CPU colormap anti-aliasing auto-disabled for memory headroom (estimated %.2f GB internal buffers).\n",
				static_cast<double>(estimatedBytes) / (1024.0 * 1024.0 * 1024.0));
			config.antiAlias = false;
		}
	}

	// Ensure image size is divisible by tiles
	if (config.imageSize % config.tilesPerSide != 0)
	{
		config.imageSize = (config.imageSize / config.tilesPerSide) * config.tilesPerSide;
		printf("NOTE: Adjusted image size to %d to be divisible by tile count.\n", config.imageSize);
	}

	// Colormap: keep each chunk <= ~512 samples/side (plus terrain padding).
	// CreateChunkBuffer allocates many maps; 1024² poles per tile can exceed 32-bit memory headroom.
	// Auto-scale tiles upward to keep per-tile allocations safe.
	if (config.useColormapMode)
	{
		const int internalForTiles = config.antiAlias ? (2 * config.imageSize) : config.imageSize;
		const int ppt = internalForTiles / config.tilesPerSide;
		if (ppt > 512)
		{
			int newTiles = config.tilesPerSide;
			const int minTiles = (internalForTiles + 511) / 512;
			for (int t = std::max(config.tilesPerSide, minTiles); t <= internalForTiles; ++t)
			{
				if (internalForTiles % t != 0)
					continue;
				if (internalForTiles / t > 512)
					continue;
				newTiles = t;
				break;
			}
			if (newTiles > config.tilesPerSide)
			{
				printf("NOTE: Increased -tiles %d -> %d for colormap stability (internal %d px, max ~512 px/tile).\n",
					config.tilesPerSide, newTiles, internalForTiles);
				config.tilesPerSide = newTiles;
			}
		}
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
// Textures and color ramps: loadTextureImageForMap() resolves paths ending in .tga by
// loading the same basename with .dds first (DXT1/3/5 via ATI_Compress; uncompressed RGB).
//
// ======================================================================

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
	int tilesPerSide = config.tilesPerSide;
	int pixelsPerTile = internalSize / tilesPerSide;
	{
		// Runtime memory guard: CreateChunkBuffer allocates many full-size pole maps.
		// Keep total poles per side bounded to avoid 32-bit allocation failures.
		const int maxTotalPolesPerSide = 1024;
		const int maxPixelsPerTileSafe = std::max(1, maxTotalPolesPerSide - originOffset - upperPad);
		if (pixelsPerTile > maxPixelsPerTileSafe)
		{
			int newTiles = (internalSize + maxPixelsPerTileSafe - 1) / maxPixelsPerTileSafe;
			while (newTiles <= internalSize && (internalSize % newTiles) != 0)
				++newTiles;
			if (newTiles > internalSize)
				newTiles = internalSize;
			if (newTiles > tilesPerSide)
			{
				printf("NOTE: Increased -tiles %d -> %d at runtime to keep chunk poles <= %d (safe memory cap).\n",
					tilesPerSide, newTiles, maxTotalPolesPerSide);
				tilesPerSide = newTiles;
				pixelsPerTile = internalSize / tilesPerSide;
			}
		}
	}
	const float metersPerPixel = mapWidth / static_cast<float>(internalSize);

	printf("  Output: %dx%d pixels (%.2f meters/pixel)%s\n",
		imageSize, imageSize,
		mapWidth / static_cast<float>(imageSize),
		config.antiAlias ? " (2x supersample anti-aliasing)" : "");
	printf("  Generating terrain data in %dx%d tiles (4 quadrants on main thread; avoids 1MB worker stack overflow)...\n", tilesPerSide, tilesPerSide);
	printf("  Shader map: skipping synchronizeShaders (per-pole stamps preserved for diffuse sampling).\n");
	fflush(stdout);

	// Allocate output buffers at internal resolution (size_t: avoid int overflow for multi‑MB buffers)
	const size_t internalPixels =
		static_cast<size_t>(internalSize) * static_cast<size_t>(internalSize);
	uint8* colorPixels = new uint8[internalPixels * 3];
	float* heightPixels = new float[internalPixels];
	memset(colorPixels, 0, internalPixels * 3);
	memset(heightPixels, 0, internalPixels * sizeof(float));

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

		// Debug stats: how many pixels actually get a family and a diffuse texture.
		// Helps distinguish "family id not resolved" vs "texture load/sampling failure".
		unsigned long long pixelsWithResolvedFamily;
		unsigned long long pixelsResolvedViaColorMap;
		unsigned long long pixelsDiffuseTexLoaded;
		unsigned long long pixelsDiffuseTexMissing;

		// Additional debug: sampled diffuse texels may be grayscale if DDS decoding / pixel
		// format interpretation is wrong.
		unsigned long long pixelsDiffuseShaderColorSampled;
		unsigned long long pixelsDiffuseShaderColorNearGray;
		unsigned long long sumDiffuseShaderR;
		unsigned long long sumDiffuseShaderG;
		unsigned long long sumDiffuseShaderB;

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

			pixelsWithResolvedFamily = 0;
			pixelsResolvedViaColorMap = 0;
			pixelsDiffuseTexLoaded = 0;
			pixelsDiffuseTexMissing = 0;
			pixelsDiffuseShaderColorSampled = 0;
			pixelsDiffuseShaderColorNearGray = 0;
			sumDiffuseShaderR = 0;
			sumDiffuseShaderG = 0;
			sumDiffuseShaderB = 0;
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

					chunkData.skipShaderSynchronize = true;
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

							// Bitmap / color affectors write generatorColor (family tint). Shader map often stays default
							// (family id 0) because ShaderGroup::chooseShader(int) only sets family when children > 0.
							// getFamilyId(PackedRgb) matches the color map to a family regardless — same data artists use.
							if (chunkData.shaderGroup)
							{
								ShaderGroup::Info sgiEffective = sgi;
								int familyId = sgi.getFamilyId();
								bool derivedFromColorMap = false;
								if (familyId <= 0)
								{
									familyId = chunkData.shaderGroup->getFamilyId(generatorColor);
									if (familyId > 0)
									{
										sgiEffective.setFamilyId(familyId);
										derivedFromColorMap = true;
									}
								}

								if (familyId > 0)
								{
									const char* familyName = chunkData.shaderGroup->getFamilyName(familyId);
									if (familyName && familyName[0])
									{
										++pixelsWithResolvedFamily;
										if (derivedFromColorMap)
											++pixelsResolvedViaColorMap;

										const ShaderLayer* layer = getLayerForFamily(familyId, shaderLayers);
										PackedRgb shaderColor;
										float const authoredTileSize = (layer && layer->shaderSize > 1e-4f) ? layer->shaderSize : 2.f;
										// Map-scale UV filtering: if authored tiling is finer than pixel footprint,
										// raise effective tile size to reduce over-repetition / moire at map resolution.
										float const minTileSizeForMap = std::max(2.0f, metersPerPixel * 1.0f);
										float const tileSize = std::max(authoredTileSize, minTileSizeForMap);

										Image const *mapTex = resolveTerrainDiffuseMapTexture(layer, chunkData.shaderGroup, sgiEffective, familyId);
										if (layer && mapTex)
										{
											float worldX = chunkData.start.x + srcX * chunkData.distanceBetweenPoles;
											float worldZ = chunkData.start.z + srcZ * chunkData.distanceBetweenPoles;
											// Match planet water UV convention (ClientGlobalWaterManager2): u = -worldX / size, v = worldZ / size
											float const u = (-worldX) / tileSize;
											// Terrain textures need V to track the image's "north-up" orientation consistently.
											// This effectively applies the missing Z-axis sign convention.
											float const v = (-worldZ) / tileSize;
											shaderColor = sampleTextureAtUV(mapTex, u, v);

											++pixelsDiffuseShaderColorSampled;
											// Count how often the sampled RGB is effectively grayscale.
											int const dr = static_cast<int>(shaderColor.r) - static_cast<int>(shaderColor.g);
											int const dg = static_cast<int>(shaderColor.g) - static_cast<int>(shaderColor.b);
											int const diff = (dr < 0 ? -dr : dr) + (dg < 0 ? -dg : dg);
											if (diff < 6) // ignore small quantization noise
												++pixelsDiffuseShaderColorNearGray;
											sumDiffuseShaderR += shaderColor.r;
											sumDiffuseShaderG += shaderColor.g;
											sumDiffuseShaderB += shaderColor.b;

											PackedRgb const baseTint = layer ? layer->familyColor : generatorColor;
											color = applyTintedShaderDetail(baseTint, shaderColor);

											++pixelsDiffuseTexLoaded;
										}
										else
										{
											PackedRgb terrainColor = getTerrainShaderColor(familyId, shaderLayers, generatorColor);
											float childChoice = sgiEffective.getChildChoice();
											shaderColor = getShaderFamilyColor(familyName, terrainColor, childChoice);
											// When we don't have a diffuse DDS, rely on the authored family color/ramp.
											color = shaderColor;

											++pixelsDiffuseTexMissing;
										}
									}
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

	for (int q = 0; q < 4; ++q)
		ctx.processQuadrant(q);

	float minHeight = ctx.threadMin[0];
	float maxHeight = ctx.threadMax[0];
	for (int q = 1; q < 4; ++q)
	{
		if (ctx.threadMin[q] < minHeight) minHeight = ctx.threadMin[q];
		if (ctx.threadMax[q] > maxHeight) maxHeight = ctx.threadMax[q];
	}

	printf("  [100%%] Generated %d tiles. Height range: %.1f to %.1f m\n", totalTiles, minHeight, maxHeight);

	// Print shader sampling stats after terrain generation, before hillshading.
	{
		unsigned long long const totalInternalPixels =
			static_cast<unsigned long long>(internalSize) * static_cast<unsigned long long>(internalSize);
		printf("  Shader sampling stats: familyPixels=%llu (via colorMap=%llu), diffuseLoaded=%llu, diffuseMissing=%llu (internalPixels=%llu)\n",
			ctx.pixelsWithResolvedFamily,
			ctx.pixelsResolvedViaColorMap,
			ctx.pixelsDiffuseTexLoaded,
			ctx.pixelsDiffuseTexMissing,
			totalInternalPixels);

		if (ctx.pixelsDiffuseShaderColorSampled > 0)
		{
			double const avgR = static_cast<double>(ctx.sumDiffuseShaderR) / static_cast<double>(ctx.pixelsDiffuseShaderColorSampled);
			double const avgG = static_cast<double>(ctx.sumDiffuseShaderG) / static_cast<double>(ctx.pixelsDiffuseShaderColorSampled);
			double const avgB = static_cast<double>(ctx.sumDiffuseShaderB) / static_cast<double>(ctx.pixelsDiffuseShaderColorSampled);
			printf("  Diffuse texel RGB: avg(%.1f,%.1f,%.1f) nearGray=%llu/%llu\n",
				avgR, avgG, avgB,
				ctx.pixelsDiffuseShaderColorNearGray,
				ctx.pixelsDiffuseShaderColorSampled);
		}
	}

	printf("  Applying edge-aware pixel easing...\n");
	fflush(stdout);
	applyPixelEasing(colorPixels, internalSize, internalSize, 0.24f);

	// Apply hillshading for 3D terrain effect
	printf("  Applying hillshading...\n");
	fflush(stdout);
	applyHillshading(heightPixels, colorPixels, internalSize, internalSize, mapWidth);
	printf("  Hillshading done.\n");
	fflush(stdout);

	applyWaterToPlanetMap(terrainTemplate, mapWidth, internalSize, internalSize, heightPixels, colorPixels);

	// Optionally downsample 2x for anti-aliased output
	uint8* savePixels = colorPixels;
	int saveWidth = internalSize;
	int saveHeight = internalSize;
	std::vector<uint8> downsampled;
	if (config.antiAlias)
	{
		downsampled.resize(static_cast<size_t>(imageSize) * static_cast<size_t>(imageSize) * 3);
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
		for (size_t j = 0; j < shaderLayers[i].childTextures.size(); ++j)
		{
			if (shaderLayers[i].childTextures[j])
			{
				delete shaderLayers[i].childTextures[j];
				shaderLayers[i].childTextures[j] = 0;
			}
		}
		shaderLayers[i].childTextures.clear();
	}
	clearTerrainLazyShaderTextures();
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

	printf("\n  Building shader layer information...\n");
	std::vector<ShaderLayer> shaderLayers = buildShaderLayers(generator);
	printf("\n");

	const int imageSize = config.imageSize;
	int tilesPerSide = config.tilesPerSide;
	int pixelsPerTile = imageSize / tilesPerSide;
	{
		// Runtime memory guard for non-quadrant shading path as well.
		const int maxTotalPolesPerSide = 1024;
		const int maxPixelsPerTileSafe = std::max(1, maxTotalPolesPerSide - originOffset - upperPad);
		if (pixelsPerTile > maxPixelsPerTileSafe)
		{
			int newTiles = (imageSize + maxPixelsPerTileSafe - 1) / maxPixelsPerTileSafe;
			while (newTiles <= imageSize && (imageSize % newTiles) != 0)
				++newTiles;
			if (newTiles > imageSize)
				newTiles = imageSize;
			if (newTiles > tilesPerSide)
			{
				printf("NOTE: Increased -tiles %d -> %d at runtime to keep chunk poles <= %d (safe memory cap).\n",
					tilesPerSide, newTiles, maxTotalPolesPerSide);
				tilesPerSide = newTiles;
				pixelsPerTile = imageSize / tilesPerSide;
			}
		}
	}
	const float metersPerPixel = mapWidth / static_cast<float>(imageSize);

	printf("  Output: %dx%d pixels (%.2f meters/pixel)\n", imageSize, imageSize, metersPerPixel);
	printf("  Generating terrain data in %dx%d tiles (shader family textures + color ramps)...\n", tilesPerSide, tilesPerSide);
	printf("  Shader map: skipping synchronizeShaders (per-pole stamps preserved for diffuse sampling).\n");

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

			chunkData.skipShaderSynchronize = true;
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

					const PackedRgb generatorColor = buffer->colorMap.getData(srcX, srcZ);
					const ShaderGroup::Info sgi = buffer->shaderMap.getData(srcX, srcZ);
					PackedRgb color = generatorColor;

					// Same family resolution as renderTerrainColormap (quadrant path): shaderMap + colorMap fallback.
					if (chunkData.shaderGroup)
					{
						ShaderGroup::Info sgiEffective = sgi;
						int familyId = sgi.getFamilyId();
						if (familyId <= 0)
						{
							familyId = chunkData.shaderGroup->getFamilyId(generatorColor);
							if (familyId > 0)
								sgiEffective.setFamilyId(familyId);
						}

						if (familyId > 0)
						{
							const char* familyName = chunkData.shaderGroup->getFamilyName(familyId);
							if (familyName && familyName[0])
							{
								const ShaderLayer* layer = getLayerForFamily(familyId, shaderLayers);
								PackedRgb shaderColor;
								float const authoredTileSize = (layer && layer->shaderSize > 1e-4f) ? layer->shaderSize : 2.f;
								// Map-scale UV filtering: if authored tiling is finer than pixel footprint,
								// raise effective tile size to reduce over-repetition / moire at map resolution.
								float const minTileSizeForMap = std::max(2.0f, metersPerPixel * 1.0f);
								float const tileSize = std::max(authoredTileSize, minTileSizeForMap);

								Image const *mapTex = resolveTerrainDiffuseMapTexture(layer, chunkData.shaderGroup, sgiEffective, familyId);
								if (layer && mapTex)
								{
									float const worldX = chunkData.start.x + srcX * chunkData.distanceBetweenPoles;
									float const worldZ = chunkData.start.z + srcZ * chunkData.distanceBetweenPoles;
									float const u = (-worldX) / tileSize;
									// Terrain textures need V to track the image's "north-up" orientation consistently.
									// This effectively applies the missing Z-axis sign convention.
									float const v = (-worldZ) / tileSize;
									shaderColor = sampleTextureAtUV(mapTex, u, v);
									PackedRgb const baseTint = layer ? layer->familyColor : generatorColor;
									color = applyTintedShaderDetail(baseTint, shaderColor);
								}
								else
								{
									PackedRgb const terrainColor = getTerrainShaderColor(familyId, shaderLayers, generatorColor);
									float const childChoice = sgiEffective.getChildChoice();
									shaderColor = getShaderFamilyColor(familyName, terrainColor, childChoice);
									color = shaderColor;
								}
							}
						}
					}

					const float height = buffer->heightMap.getData(srcX, srcZ);

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

	printf("  Applying edge-aware pixel easing...\n");
	applyPixelEasing(colorPixels, imageSize, imageSize, 0.24f);

	// Apply hillshading for 3D terrain effect
	printf("  Applying hillshading...\n");
	applyHillshading(heightPixels, colorPixels, imageSize, imageSize, mapWidth);

	applyWaterToPlanetMap(terrainTemplate, mapWidth, imageSize, imageSize, heightPixels, colorPixels);

	// Save output image
	char outputPath[512];
	_snprintf(outputPath, sizeof(outputPath), "%s/%s.tga", config.outputDir.c_str(), outputName);

	printf("  Saving: %s\n", outputPath);
	const bool saved = saveTGA(colorPixels, imageSize, imageSize, 3, outputPath);

	for (size_t i = 0; i < shaderLayers.size(); ++i)
	{
		if (shaderLayers[i].familyTexture)
		{
			delete shaderLayers[i].familyTexture;
			shaderLayers[i].familyTexture = 0;
		}
		for (size_t j = 0; j < shaderLayers[i].childTextures.size(); ++j)
		{
			if (shaderLayers[i].childTextures[j])
			{
				delete shaderLayers[i].childTextures[j];
				shaderLayers[i].childTextures[j] = 0;
			}
		}
		shaderLayers[i].childTextures.clear();
	}
	clearTerrainLazyShaderTextures();
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

	// Ambient-heavy so planet maps stay readable after multiply (diffuse albedo is often ~0.2–0.5).
	const float ambient  = 0.52f;
	const float diffuse  = 0.48f;

	for (int y = 1; y < height - 1; ++y)
	{
		for (int x = 1; x < width - 1; ++x)
		{
			// Calculate surface normal using central differences
			const float hC = heightMap[y * width + x];
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

			// Dot product with light direction (soft floor so steep/shadowed slopes are not crushed to black)
			float dot = nx * lightX + ny * lightY + nz * lightZ;
			dot = std::max(0.0f, std::min(1.0f, dot));
			dot = 0.22f + 0.78f * dot;

			// Compute lighting factor
			float lighting = ambient + diffuse * dot;

			// Elevation-aware depth term: below 0m should read lower (e.g. Theed lake retreat basin),
			// not as bright as surrounding high terrain under the same slope lighting.
			{
				const float depthMeters = std::max(0.0f, -hC);
				// Full effect reached by ~200m below 0; tuned to keep shallow negatives readable.
				const float depthT = std::min(1.0f, depthMeters / 200.0f);
				const float depthDarken = 1.0f - 0.35f * depthT;
				lighting *= depthDarken;
			}

			lighting = std::max(0.0f, std::min(1.45f, lighting));

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
