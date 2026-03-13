// ======================================================================
//
// DdsToTgaConverter.cpp
// Converts DDS textures to TGA for Maya import (Maya 8 does not display DDS correctly).
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "DdsToTgaConverter.h"

#include "clientGraphics/Dds.h"
#include "sharedImage/Image.h"
#include "sharedImage/TargaFormat.h"

#include "ATI_Compress.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif

// ======================================================================

namespace
{
	bool fileExistsAndNewerThan(const char *path, const char *otherPath)
	{
		struct stat stPath = {0}, stOther = {0};
		if (stat(path, &stPath) != 0 || stat(otherPath, &stOther) != 0)
			return false;
		return stPath.st_mtime >= stOther.st_mtime;
	}
	const unsigned long DDS_MAGIC = 0x20534444; // "DDS "

	bool isDdsPath(const std::string &path)
	{
		std::string::size_type dot = path.find_last_of('.');
		if (dot == std::string::npos || dot >= path.size() - 1)
			return false;
		std::string ext = path.substr(dot + 1);
		for (size_t i = 0; i < ext.size(); ++i)
			if (ext[i] >= 'A' && ext[i] <= 'Z')
				ext[i] = static_cast<char>(ext[i] + ('a' - 'A'));
		return ext == "dds";
	}

	std::string replaceExtensionWithTga(const std::string &path)
	{
		std::string result = path;
		std::string::size_type dot = result.find_last_of('.');
		if (dot != std::string::npos && dot > result.find_last_of("/\\"))
			result.resize(dot);
		result += ".tga";
		return result;
	}

	// Compute size of base mip level for DXT
	size_t getDxtBaseLevelSize(int width, int height, bool dxt1)
	{
		int blocksX = (width + 3) / 4;
		int blocksY = (height + 3) / 4;
		return static_cast<size_t>(blocksX * blocksY * (dxt1 ? 8 : 16));
	}
}

// ======================================================================

std::string DdsToTgaConverter::convertToTga(const std::string &ddsPath)
{
	if (!isDdsPath(ddsPath))
		return std::string();

	const std::string tgaPath = replaceExtensionWithTga(ddsPath);
	if (fileExistsAndNewerThan(tgaPath.c_str(), ddsPath.c_str()))
		return tgaPath;

	FILE *f = fopen(ddsPath.c_str(), "rb");
	if (!f)
		return std::string();

	unsigned long magic = 0;
	if (fread(&magic, 4, 1, f) != 1 || magic != DDS_MAGIC)
	{
		fclose(f);
		return std::string();
	}

	DDS::DDS_HEADER header;
	if (fread(&header, sizeof(header), 1, f) != 1)
	{
		fclose(f);
		return std::string();
	}

	ATI_TC_FORMAT srcFormat = ATI_TC_FORMAT_ARGB_8888;
	if (header.ddspf.dwFlags & DDS::DDS_FOURCC)
	{
		if (header.ddspf.dwFourCC == DDS::DDSPF_DXT1.dwFourCC)
			srcFormat = ATI_TC_FORMAT_DXT1;
		else if (header.ddspf.dwFourCC == DDS::DDSPF_DXT3.dwFourCC)
			srcFormat = ATI_TC_FORMAT_DXT3;
		else if (header.ddspf.dwFourCC == DDS::DDSPF_DXT5.dwFourCC)
			srcFormat = ATI_TC_FORMAT_DXT5;
		else
		{
			fclose(f);
			return std::string();
		}
	}
	else
	{
		// Uncompressed DDS - Maya might handle these; for now only convert DXT
		fclose(f);
		return std::string();
	}

	const int width = static_cast<int>(header.dwWidth);
	const int height = static_cast<int>(header.dwHeight);
	const bool dxt1 = (srcFormat == ATI_TC_FORMAT_DXT1);
	const size_t dxtSize = getDxtBaseLevelSize(width, height, dxt1);

	std::vector<ATI_TC_BYTE> dxtData(dxtSize);
	if (fread(&dxtData[0], 1, dxtSize, f) != dxtSize)
	{
		fclose(f);
		return std::string();
	}
	fclose(f);

	ATI_TC_Texture srcTex;
	memset(&srcTex, 0, sizeof(srcTex));
	srcTex.dwSize = sizeof(ATI_TC_Texture);
	srcTex.dwWidth = width;
	srcTex.dwHeight = height;
	srcTex.dwPitch = 0;
	srcTex.format = srcFormat;
	srcTex.dwDataSize = static_cast<ATI_TC_DWORD>(dxtSize);
	srcTex.pData = &dxtData[0];

	ATI_TC_Texture destTex;
	memset(&destTex, 0, sizeof(destTex));
	destTex.dwSize = sizeof(ATI_TC_Texture);
	destTex.dwWidth = width;
	destTex.dwHeight = height;
	destTex.dwPitch = width * 4;
	destTex.format = ATI_TC_FORMAT_ARGB_8888;
	destTex.dwDataSize = ATI_TC_CalculateBufferSize(&destTex);
	destTex.pData = new ATI_TC_BYTE[destTex.dwDataSize];

	ATI_TC_ERROR err = ATI_TC_ConvertTexture(&srcTex, &destTex, 0, 0, 0, 0);
	if (err != ATI_TC_OK)
	{
		delete[] destTex.pData;
		return std::string();
	}

	// ATI_TC_FORMAT_ARGB_8888: typically A in high byte, R, G, B (little-endian: BGRA in memory)
	// Image expects PF_bgra_8888 for B,G,R,A byte order
	Image image(destTex.pData, destTex.dwDataSize, width, height, 32, 4, width * 4,
		0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	image.setReadOnly();

	TargaFormat targa;
	const bool saved = targa.saveImage(image, tgaPath.c_str());

	delete[] destTex.pData;

	return saved ? tgaPath : std::string();
}
