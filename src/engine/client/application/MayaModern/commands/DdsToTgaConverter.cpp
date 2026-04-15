#include "DdsToTgaConverter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif

namespace
{
    const unsigned long DDS_MAGIC = 0x20534444;  // "DDS "

#pragma pack(push, 1)
    struct DdsPixelFormat
    {
        unsigned long dwSize;
        unsigned long dwFlags;
        unsigned long dwFourCC;
        unsigned long dwRGBBitCount;
        unsigned long dwRBitMask;
        unsigned long dwGBitMask;
        unsigned long dwBBitMask;
        unsigned long dwABitMask;
    };

    // Must match Microsoft DDS_HEADER layout exactly (124 bytes)
    struct DdsHeader
    {
        unsigned long dwSize;
        unsigned long dwFlags;
        unsigned long dwHeight;
        unsigned long dwWidth;
        unsigned long dwPitchOrLinearSize;
        unsigned long dwDepth;
        unsigned long dwMipMapCount;
        unsigned long dwReserved1[11];
        DdsPixelFormat ddspf;
        unsigned long dwCaps;
        unsigned long dwCaps2;
        unsigned long dwCaps3;
        unsigned long dwCaps4;
        unsigned long dwReserved2;
    };

    struct DdsHeaderDxt10
    {
        unsigned long dxgiFormat;
        unsigned long resourceDimension;
        unsigned long miscFlag;
        unsigned long arraySize;
        unsigned long miscFlags2;
    };
#pragma pack(pop)

    // DXGI_FORMAT (subset). BC5 = two-channel normals (common for *_n.dds as FourCC ATI2 or DX10).
    const unsigned long DXGI_FORMAT_BC5_TYPELESS = 80;
    const unsigned long DXGI_FORMAT_BC5_UNORM = 83;
    const unsigned long DXGI_FORMAT_BC5_SNORM = 84;

    const unsigned long DDSD_PITCH = 0x00000008;
    const unsigned long DDSD_LINEARSIZE = 0x00080000;

    inline unsigned long MakeFourCC(char a, char b, char c, char d)
    {
        return (unsigned long)(unsigned char)(a) | ((unsigned long)(unsigned char)(b) << 8) |
               ((unsigned long)(unsigned char)(c) << 16) | ((unsigned long)(unsigned char)(d) << 24);
    }

    bool fileExistsAndNewerThan(const char* path, const char* otherPath)
    {
        struct stat stPath = {}, stOther = {};
        if (stat(path, &stPath) != 0 || stat(otherPath, &stOther) != 0)
            return false;
        return stPath.st_mtime >= stOther.st_mtime;
    }

    bool isDdsPath(const std::string& path)
    {
        size_t dot = path.find_last_of('.');
        if (dot == std::string::npos || dot >= path.size() - 1)
            return false;
        std::string ext = path.substr(dot + 1);
        for (size_t i = 0; i < ext.size(); ++i)
            if (ext[i] >= 'A' && ext[i] <= 'Z')
                ext[i] = static_cast<char>(ext[i] + ('a' - 'A'));
        return ext == "dds";
    }

    std::string replaceExtensionWithTga(const std::string& path)
    {
        std::string result = path;
        size_t dot = result.find_last_of('.');
        if (dot != std::string::npos && dot > result.find_last_of("/\\"))
            result.resize(dot);
        result += ".tga";
        return result;
    }

    std::string getBaseNameWithExt(const std::string& path)
    {
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash != std::string::npos)
            return path.substr(lastSlash + 1);
        return path;
    }

    std::string getBaseNameNoExt(const std::string& path)
    {
        std::string base = getBaseNameWithExt(path);
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos)
            base.resize(dot);
        return base;
    }

    size_t getDxtBaseLevelSize(int width, int height, bool dxt1)
    {
        int blocksX = (width + 3) / 4;
        int blocksY = (height + 3) / 4;
        return static_cast<size_t>(blocksX * blocksY * (dxt1 ? 8 : 16));
    }

    void unpackRgb565(unsigned short packed, unsigned char& r, unsigned char& g, unsigned char& b)
    {
        r = static_cast<unsigned char>((packed >> 11) * 255 / 31);
        g = static_cast<unsigned char>(((packed >> 5) & 0x3F) * 255 / 63);
        b = static_cast<unsigned char>((packed & 0x1F) * 255 / 31);
    }

    /// Expand a masked DDS field to 8-bit (handles A8R8G8B8, A8B8G8R8, X8R8G8B8, etc.).
    unsigned char expandMaskTo8(unsigned long pixel, unsigned long mask)
    {
        if (!mask)
            return 0;
        int rshift = 0;
        for (unsigned long t = mask; t && (t & 1u) == 0; t >>= 1)
            ++rshift;
        unsigned bits = 0;
        for (unsigned long t = mask >> rshift; t; t >>= 1)
            ++bits;
        unsigned long v = (pixel & mask) >> rshift;
        if (bits == 0)
            return 0;
        if (bits >= 8)
            return static_cast<unsigned char>(v & 0xFF);
        const unsigned long maxVal = (1u << bits) - 1;
        return static_cast<unsigned char>((v * 255 + maxVal / 2) / maxVal);
    }

    void unpackDdsPixelToBgra(
        unsigned long pixel,
        unsigned long rMask,
        unsigned long gMask,
        unsigned long bMask,
        unsigned long aMask,
        unsigned char* bgra)
    {
        bgra[2] = expandMaskTo8(pixel, rMask);
        bgra[1] = expandMaskTo8(pixel, gMask);
        bgra[0] = expandMaskTo8(pixel, bMask);
        bgra[3] = aMask ? expandMaskTo8(pixel, aMask) : 255;
    }

    void decompressDxt1Block(const unsigned char* block, unsigned char* outPixels, int outStride)
    {
        unsigned short c0 = block[0] | (block[1] << 8);
        unsigned short c1 = block[2] | (block[3] << 8);
        unsigned char r0, g0, b0, r1, g1, b1;
        unpackRgb565(c0, r0, g0, b0);
        unpackRgb565(c1, r1, g1, b1);

        unsigned int indices = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);

        unsigned char r[4], g[4], b[4], a[4];
        r[0] = r0; g[0] = g0; b[0] = b0; a[0] = 255;
        r[1] = r1; g[1] = g1; b[1] = b1; a[1] = 255;
        if (c0 > c1)
        {
            r[2] = static_cast<unsigned char>((2 * r0 + r1) / 3);
            g[2] = static_cast<unsigned char>((2 * g0 + g1) / 3);
            b[2] = static_cast<unsigned char>((2 * b0 + b1) / 3);
            a[2] = 255;
            r[3] = static_cast<unsigned char>((r0 + 2 * r1) / 3);
            g[3] = static_cast<unsigned char>((g0 + 2 * g1) / 3);
            b[3] = static_cast<unsigned char>((b0 + 2 * b1) / 3);
            a[3] = 255;
        }
        else
        {
            r[2] = static_cast<unsigned char>((r0 + r1) / 2);
            g[2] = static_cast<unsigned char>((g0 + g1) / 2);
            b[2] = static_cast<unsigned char>((b0 + b1) / 2);
            a[2] = 255;
            r[3] = 0; g[3] = 0; b[3] = 0; a[3] = 0;
        }

        for (int y = 0; y < 4; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                int idx = (indices >> (2 * (y * 4 + x))) & 3;
                int outIdx = y * outStride + x * 4;
                outPixels[outIdx + 0] = b[idx];
                outPixels[outIdx + 1] = g[idx];
                outPixels[outIdx + 2] = r[idx];
                outPixels[outIdx + 3] = a[idx];
            }
        }
    }

    void decompressDxt5AlphaBlock(const unsigned char* block, unsigned char* outAlpha)
    {
        unsigned char a0 = block[0];
        unsigned char a1 = block[1];
        unsigned long long indices = block[2] | (static_cast<unsigned long long>(block[3]) << 8) |
            (static_cast<unsigned long long>(block[4]) << 16) | (static_cast<unsigned long long>(block[5]) << 24) |
            (static_cast<unsigned long long>(block[6]) << 32) | (static_cast<unsigned long long>(block[7]) << 40);

        unsigned char a[8];
        a[0] = a0;
        a[1] = a1;
        if (a0 > a1)
        {
            a[2] = static_cast<unsigned char>((6 * a0 + 1 * a1) / 7);
            a[3] = static_cast<unsigned char>((5 * a0 + 2 * a1) / 7);
            a[4] = static_cast<unsigned char>((4 * a0 + 3 * a1) / 7);
            a[5] = static_cast<unsigned char>((3 * a0 + 4 * a1) / 7);
            a[6] = static_cast<unsigned char>((2 * a0 + 5 * a1) / 7);
            a[7] = static_cast<unsigned char>((1 * a0 + 6 * a1) / 7);
        }
        else
        {
            a[2] = static_cast<unsigned char>((4 * a0 + 1 * a1) / 5);
            a[3] = static_cast<unsigned char>((3 * a0 + 2 * a1) / 5);
            a[4] = static_cast<unsigned char>((2 * a0 + 3 * a1) / 5);
            a[5] = static_cast<unsigned char>((1 * a0 + 4 * a1) / 5);
            a[6] = 0;
            a[7] = 255;
        }

        for (int i = 0; i < 16; ++i)
            outAlpha[i] = a[(indices >> (3 * i)) & 7];
    }

    void decompressDxt5Block(const unsigned char* block, unsigned char* outPixels, int outStride)
    {
        unsigned char alpha[16];
        decompressDxt5AlphaBlock(block, alpha);
        decompressDxt1Block(block + 8, outPixels, outStride);

        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                outPixels[y * outStride + x * 4 + 3] = alpha[y * 4 + x];
    }

    /// BC5 / ATI2: two DXT5-style alpha blocks = R and G (tangent normal). Output BGRA, A=255 for GIMP/Maya.
    void decompressBc5Block(const unsigned char* block, unsigned char* outPixels, int outStride)
    {
        unsigned char red[16];
        unsigned char green[16];
        decompressDxt5AlphaBlock(block, red);
        decompressDxt5AlphaBlock(block + 8, green);
        for (int y = 0; y < 4; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                const int idx = y * 4 + x;
                const float nx = red[idx] * (1.0f / 255.0f) * 2.0f - 1.0f;
                const float ny = green[idx] * (1.0f / 255.0f) * 2.0f - 1.0f;
                const float sq = 1.0f - nx * nx - ny * ny;
                const float nz = sq > 0.f ? std::sqrt(sq) : 0.f;
                const int o = y * outStride + x * 4;
                outPixels[o + 0] = static_cast<unsigned char>(std::min(255.f, (nz * 0.5f + 0.5f) * 255.f + 0.5f));
                outPixels[o + 1] = static_cast<unsigned char>(std::min(255.f, (ny * 0.5f + 0.5f) * 255.f + 0.5f));
                outPixels[o + 2] = static_cast<unsigned char>(std::min(255.f, (nx * 0.5f + 0.5f) * 255.f + 0.5f));
                outPixels[o + 3] = 255;
            }
        }
    }

    void decompressDxt3Block(const unsigned char* block, unsigned char* outPixels, int outStride)
    {
        decompressDxt1Block(block + 8, outPixels, outStride);
        for (int i = 0; i < 16; ++i)
        {
            unsigned char a4 = (i & 1) ? (block[i / 2] >> 4) : (block[i / 2] & 0x0F);
            outPixels[i * 4 + 3] = static_cast<unsigned char>(a4 | (a4 << 4));
        }
    }

    bool writeTga(const char* path, int width, int height, const unsigned char* bgraPixels)
    {
        FILE* f = fopen(path, "wb");
        if (!f)
            return false;

        unsigned char header[18] = {};
        header[2] = 2;   // Uncompressed RGB
        header[12] = width & 0xFF;
        header[13] = (width >> 8) & 0xFF;
        header[14] = height & 0xFF;
        header[15] = (height >> 8) & 0xFF;
        header[16] = 32;  // 32 bpp
        // 8 alpha attribute bits (0-3) + top-left origin (bit 5): matches prior MayaModern behavior and most viewers.
        header[17] = 40;

        if (fwrite(header, 1, 18, f) != 18)
        {
            fclose(f);
            return false;
        }

        const int rowSize = width * 4;
        for (int y = 0; y < height; ++y)
        {
            if (fwrite(bgraPixels + y * rowSize, 1, static_cast<size_t>(rowSize), f) != static_cast<size_t>(rowSize))
            {
                fclose(f);
                return false;
            }
        }

        fclose(f);
        return true;
    }
}

std::string DdsToTgaConverter::convertToTga(const std::string& ddsPath, const std::string& outputDir)
{
    if (!isDdsPath(ddsPath))
    {
        std::cerr << "[DdsToTga] Not a DDS path: " << ddsPath << "\n";
        return std::string();
    }

    std::string tgaPath;
    if (!outputDir.empty())
    {
        std::string baseName = getBaseNameNoExt(ddsPath) + ".tga";
        tgaPath = outputDir;
        if (tgaPath.back() != '/' && tgaPath.back() != '\\')
            tgaPath += '/';
        tgaPath += baseName;
    }
    else
    {
        tgaPath = replaceExtensionWithTga(ddsPath);
    }
    // Always reconvert (no cache) - ensures correct output after converter fixes
    std::cerr << "[DdsToTga] Converting: " << ddsPath << " -> " << tgaPath << "\n";

    FILE* f = fopen(ddsPath.c_str(), "rb");
    if (!f)
    {
        std::cerr << "[DdsToTga] Failed to open DDS: " << ddsPath << "\n";
        return std::string();
    }

    unsigned long magic = 0;
    if (fread(&magic, 4, 1, f) != 1 || magic != DDS_MAGIC)
    {
        fclose(f);
        return std::string();
    }

    DdsHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1)
    {
        fclose(f);
        return std::string();
    }

    unsigned long dxgiFormat = 0;
    if ((header.ddspf.dwFlags & 4) && header.ddspf.dwFourCC == MakeFourCC('D', 'X', '1', '0'))
    {
        DdsHeaderDxt10 d10;
        if (fread(&d10, sizeof(d10), 1, f) != 1)
        {
            std::cerr << "[DdsToTga] Failed to read DX10 header: " << ddsPath << "\n";
            fclose(f);
            return std::string();
        }
        dxgiFormat = d10.dxgiFormat;
        if (d10.arraySize > 1)
            std::cerr << "[DdsToTga] Warning: texture array size " << d10.arraySize << " - decoding first slice only.\n";
    }

    const int width = static_cast<int>(header.dwWidth);
    const int height = static_cast<int>(header.dwHeight);
    const int outStride = width * 4;
    std::vector<unsigned char> outPixels(static_cast<size_t>(width * height * 4));

    // 0=unsupported, 1=DXT1, 2=DXT3/BC2, 3=DXT5/BC3, 4=uncompressed RGBA, 5=BC5/ATI2
    int format = 0;
    if (header.ddspf.dwFlags & 4)  // DDS_FOURCC
    {
        if (header.ddspf.dwFourCC == MakeFourCC('D', 'X', 'T', '1'))
            format = 1;
        else if (header.ddspf.dwFourCC == MakeFourCC('D', 'X', 'T', '2') ||
                 header.ddspf.dwFourCC == MakeFourCC('D', 'X', 'T', '3'))
            format = 2;
        else if (header.ddspf.dwFourCC == MakeFourCC('D', 'X', 'T', '4') ||
                 header.ddspf.dwFourCC == MakeFourCC('D', 'X', 'T', '5'))
            format = 3;
        else if (header.ddspf.dwFourCC == MakeFourCC('A', 'T', 'I', '2'))
            format = 5;
        else if (header.ddspf.dwFourCC == MakeFourCC('D', 'X', '1', '0'))
        {
            if (dxgiFormat == DXGI_FORMAT_BC5_TYPELESS || dxgiFormat == DXGI_FORMAT_BC5_UNORM ||
                dxgiFormat == DXGI_FORMAT_BC5_SNORM)
                format = 5;
        }
    }
    else if (header.ddspf.dwRGBBitCount == 32)
    {
        //32-bit packed: require RGB masks (DDPF_RGB and/or DDPF_ALPHAPIXELS).
        const bool hasRgbFlag = (header.ddspf.dwFlags & 0x40) != 0;
        const bool hasAlphaFlag = (header.ddspf.dwFlags & 0x1) != 0;
        if (hasRgbFlag || hasAlphaFlag)
        {
            const unsigned long rMask = header.ddspf.dwRBitMask;
            const unsigned long gMask = header.ddspf.dwGBitMask;
            const unsigned long bMask = header.ddspf.dwBBitMask;
            if (rMask && gMask && bMask)
                format = 4;
        }
    }

    if (format == 0)
    {
        if (dxgiFormat)
            std::cerr << "[DdsToTga] Unsupported DDS (DX10 DXGI format " << dxgiFormat << "): " << ddsPath << "\n";
        else
            std::cerr << "[DdsToTga] Unsupported DDS format: " << ddsPath << "\n";
        fclose(f);
        return std::string();
    }

    const char* formatName = (format == 1)   ? "DXT1"
                             : (format == 2) ? "DXT3"
                             : (format == 3) ? "DXT5"
                             : (format == 4) ? "RGBA"
                             : (format == 5) ? "BC5/ATI2"
                                             : "?";
    std::cerr << "[DdsToTga] Format: " << formatName << ", " << width << "x" << height << "\n";

    if (format == 4)
    {
        const unsigned long rMask = header.ddspf.dwRBitMask;
        const unsigned long gMask = header.ddspf.dwGBitMask;
        const unsigned long bMask = header.ddspf.dwBBitMask;
        const unsigned long aMask = header.ddspf.dwABitMask;

        const size_t rowBytes = static_cast<size_t>(width) * 4;
        unsigned long pitch = static_cast<unsigned long>(rowBytes);
        if ((header.dwFlags & DDSD_PITCH) && header.dwPitchOrLinearSize)
            pitch = header.dwPitchOrLinearSize;
        else if ((header.dwFlags & DDSD_LINEARSIZE) && header.dwPitchOrLinearSize && height > 0)
        {
            const unsigned long ls = header.dwPitchOrLinearSize;
            if (ls >= rowBytes && ls % rowBytes == 0)
                pitch = ls / static_cast<unsigned long>(height);
        }
        else if (header.dwPitchOrLinearSize >= rowBytes)
            pitch = header.dwPitchOrLinearSize;

        if (pitch < rowBytes)
        {
            std::cerr << "[DdsToTga] Invalid pitch for uncompressed DDS: " << ddsPath << "\n";
            fclose(f);
            return std::string();
        }
        std::vector<unsigned char> rowBuf(pitch);
        for (int y = 0; y < height; ++y)
        {
            if (fread(rowBuf.data(), 1, pitch, f) != pitch)
            {
                fclose(f);
                return std::string();
            }
            unsigned char* dstRow = outPixels.data() + static_cast<size_t>(y) * rowBytes;
            for (int x = 0; x < width; ++x)
            {
                unsigned long pixel = 0;
                std::memcpy(&pixel, rowBuf.data() + static_cast<size_t>(x) * 4, 4);
                unpackDdsPixelToBgra(pixel, rMask, gMask, bMask, aMask, dstRow + static_cast<size_t>(x) * 4);
            }
        }
        fclose(f);
    }
    else
    {
        const bool dxt1 = (format == 1);
        const size_t dxtSize = getDxtBaseLevelSize(width, height, dxt1);
        std::vector<unsigned char> dxtData(dxtSize);
        if (fread(dxtData.data(), 1, dxtSize, f) != dxtSize)
        {
            fclose(f);
            return std::string();
        }
        fclose(f);

        const int blocksX = (width + 3) / 4;
        const int blocksY = (height + 3) / 4;

        for (int by = 0; by < blocksY; ++by)
        {
            for (int bx = 0; bx < blocksX; ++bx)
            {
                const size_t bytesPerBlock = dxt1 ? 8u : 16u;
                const size_t blockOffset = (by * blocksX + bx) * bytesPerBlock;
                int outX = bx * 4;
                int outY = by * 4;

                unsigned char blockPixels[64];
                if (dxt1)
                    decompressDxt1Block(dxtData.data() + blockOffset, blockPixels, 16);
                else if (format == 2)
                    decompressDxt3Block(dxtData.data() + blockOffset, blockPixels, 16);
                else if (format == 5)
                    decompressBc5Block(dxtData.data() + blockOffset, blockPixels, 16);
                else
                    decompressDxt5Block(dxtData.data() + blockOffset, blockPixels, 16);

                for (int py = 0; py < 4 && (outY + py) < height; ++py)
                {
                    for (int px = 0; px < 4 && (outX + px) < width; ++px)
                    {
                        int dstIdx = (outY + py) * outStride + (outX + px) * 4;
                        int srcIdx = py * 16 + px * 4;
                        outPixels[static_cast<size_t>(dstIdx + 0)] = blockPixels[srcIdx + 0];
                        outPixels[static_cast<size_t>(dstIdx + 1)] = blockPixels[srcIdx + 1];
                        outPixels[static_cast<size_t>(dstIdx + 2)] = blockPixels[srcIdx + 2];
                        outPixels[static_cast<size_t>(dstIdx + 3)] = blockPixels[srcIdx + 3];
                    }
                }
            }
        }
    }

    {
        const std::string base = getBaseNameNoExt(ddsPath);
        if (base.find("_n") != std::string::npos && (format == 3 || format == 4))
            std::cerr << "[DdsToTga] Note: \"" << base << "\" looks like a normal map. In GIMP, DXT5/uncompressed alpha is often data (e.g. Y), not "
                         "opacity; checkerboard means low alpha values, not missing RGB.\n";
    }

    if (!writeTga(tgaPath.c_str(), width, height, outPixels.data()))
    {
        std::cerr << "[DdsToTga] Failed to write TGA: " << tgaPath << "\n";
        return std::string();
    }

    std::cerr << "[DdsToTga] Converted OK: " << width << "x" << height << " -> " << tgaPath << "\n";

    return tgaPath;
}
