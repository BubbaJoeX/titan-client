#include "DdsToTgaConverter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <zlib.h>

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
    const unsigned long DXGI_FORMAT_BC1_UNORM = 71;
    const unsigned long DXGI_FORMAT_BC1_UNORM_SRGB = 72;
    const unsigned long DXGI_FORMAT_BC2_UNORM = 74;
    const unsigned long DXGI_FORMAT_BC2_UNORM_SRGB = 75;
    const unsigned long DXGI_FORMAT_BC3_UNORM = 77;
    const unsigned long DXGI_FORMAT_BC3_UNORM_SRGB = 78;
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

    void flattenBgraAlpha(unsigned char* bgra, int width, int height)
    {
        const int n = width * height;
        for (int i = 0; i < n; ++i)
            bgra[static_cast<size_t>(i) * 4u + 3u] = 255;
    }

    unsigned int pngBe32(unsigned int v)
    {
        return ((v & 0xff000000u) >> 24) | ((v & 0x00ff0000u) >> 8) |
               ((v & 0x0000ff00u) << 8) | ((v & 0x000000ffu) << 24);
    }

    bool writePngChunk(FILE* f, const char type[4], const unsigned char* data, size_t n)
    {
        const unsigned int lenBe = pngBe32(static_cast<unsigned int>(n));
        if (fwrite(&lenBe, 4, 1, f) != 1)
            return false;
        if (fwrite(type, 4, 1, f) != 1)
            return false;
        unsigned long crc = crc32(0L, reinterpret_cast<const Bytef*>(type), 4);
        if (n > 0)
        {
            if (fwrite(data, 1, n, f) != n)
                return false;
            crc = crc32(crc, data, static_cast<uInt>(n));
        }
        const unsigned int crcBe = pngBe32(static_cast<unsigned int>(crc & 0xfffffffful));
        return fwrite(&crcBe, 4, 1, f) == 1;
    }

    /// Top-down BGRA → 8-bit PNG. flattenAlpha writes RGB (no alpha) so VP2 cannot treat gloss as opacity.
    bool writePng(const char* path, int width, int height, const unsigned char* bgraPixels, bool flattenAlpha)
    {
        FILE* f = fopen(path, "wb");
        if (!f)
            return false;
        static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
        if (fwrite(sig, 1, 8, f) != 8)
        {
            fclose(f);
            return false;
        }

        const int bpp = flattenAlpha ? 3 : 4;
        std::vector<unsigned char> raw(static_cast<size_t>(height) * static_cast<size_t>(1 + width * bpp));
        size_t o = 0;
        for (int y = 0; y < height; ++y)
        {
            raw[o++] = 0;
            const unsigned char* row = bgraPixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u;
            for (int x = 0; x < width; ++x)
            {
                raw[o++] = row[x * 4 + 2];
                raw[o++] = row[x * 4 + 1];
                raw[o++] = row[x * 4 + 0];
                if (!flattenAlpha)
                    raw[o++] = row[x * 4 + 3];
            }
        }

        uLongf destLen = compressBound(static_cast<uLong>(raw.size()));
        std::vector<unsigned char> zbuf(static_cast<size_t>(destLen));
        if (compress(zbuf.data(), &destLen, raw.data(), static_cast<uLong>(raw.size())) != Z_OK)
        {
            fclose(f);
            return false;
        }
        zbuf.resize(static_cast<size_t>(destLen));

        unsigned char ihdr[13] = {};
        const unsigned int wBe = pngBe32(static_cast<unsigned int>(width));
        const unsigned int hBe = pngBe32(static_cast<unsigned int>(height));
        std::memcpy(ihdr + 0, &wBe, 4);
        std::memcpy(ihdr + 4, &hBe, 4);
        ihdr[8] = 8;
        ihdr[9] = flattenAlpha ? 2 : 6;
        if (!writePngChunk(f, "IHDR", ihdr, 13) ||
            !writePngChunk(f, "IDAT", zbuf.data(), zbuf.size()) ||
            !writePngChunk(f, "IEND", nullptr, 0))
        {
            fclose(f);
            return false;
        }
        fclose(f);
        return true;
    }

    bool writeTga(const char* path, int width, int height, const unsigned char* bgraPixels)
    {
        FILE* f = fopen(path, "wb");
        if (!f)
            return false;

        unsigned char header[18] = {};
        header[2] = 2;   // Uncompressed true-color
        header[12] = width & 0xFF;
        header[13] = (width >> 8) & 0xFF;
        header[14] = height & 0xFF;
        header[15] = (height >> 8) & 0xFF;
        header[16] = 32;  // 32 bpp (BGRA order in payload)
        // Bits 0–3: alpha depth = 8. Bit 5 must stay 0 (bottom-left). Maya VP2 rejects top-left TGA
        // (descriptor 0x28) and shows file.defaultColor — uniform medium gray — instead of the map.
        header[17] = 8;

        if (fwrite(header, 1, 18, f) != 18)
        {
            fclose(f);
            return false;
        }

        const int rowSize = width * 4;
        for (int y = height - 1; y >= 0; --y)
        {
            if (fwrite(bgraPixels + y * rowSize, 1, static_cast<size_t>(rowSize), f) != static_cast<size_t>(rowSize))
            {
                fclose(f);
                return false;
            }
        }

        // TGA 2.0 footer — marks file as TRUEVISION-XFILE so GIMP/Inkscape reliably load 32-bit alpha as RGBA.
        static const unsigned char kTga20Footer[26] = {
            0, 0, 0, 0,
            0, 0, 0, 0,
            'T', 'R', 'U', 'E', 'V', 'I', 'S', 'I', 'O', 'N', '-', 'X', 'F', 'I', 'L', 'E', '.', 0};
        if (fwrite(kTga20Footer, 1, sizeof(kTga20Footer), f) != sizeof(kTga20Footer))
        {
            fclose(f);
            return false;
        }
        fclose(f);
        return true;
    }
}

std::string DdsToTgaConverter::convertToTga(const std::string& ddsPath, const std::string& outputDir, bool flattenAlpha)
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
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
    {
        std::cerr << "[DdsToTga] Rejecting DDS with invalid size " << width << "x" << height << ": " << ddsPath << "\n";
        fclose(f);
        return std::string();
    }
    const int outStride = width * 4;
    std::vector<unsigned char> outPixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

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
            if (dxgiFormat == DXGI_FORMAT_BC1_UNORM || dxgiFormat == DXGI_FORMAT_BC1_UNORM_SRGB)
                format = 1;
            else if (dxgiFormat == DXGI_FORMAT_BC2_UNORM || dxgiFormat == DXGI_FORMAT_BC2_UNORM_SRGB)
                format = 2;
            else if (dxgiFormat == DXGI_FORMAT_BC3_UNORM || dxgiFormat == DXGI_FORMAT_BC3_UNORM_SRGB)
                format = 3;
            else if (dxgiFormat == DXGI_FORMAT_BC5_TYPELESS || dxgiFormat == DXGI_FORMAT_BC5_UNORM ||
                dxgiFormat == DXGI_FORMAT_BC5_SNORM)
                format = 5;
        }
    }
    else if (header.ddspf.dwRGBBitCount == 32)
    {
        // Mixed-in game DDS often omit DDPF_RGB (0x40) even when masks are valid A8R8G8B8.
        unsigned long rMask = header.ddspf.dwRBitMask;
        unsigned long gMask = header.ddspf.dwGBitMask;
        unsigned long bMask = header.ddspf.dwBBitMask;
        if (!rMask && !gMask && !bMask)
        {
            rMask = 0x00FF0000UL;
            gMask = 0x0000FF00UL;
            bMask = 0x000000FFUL;
            header.ddspf.dwRBitMask = rMask;
            header.ddspf.dwGBitMask = gMask;
            header.ddspf.dwBBitMask = bMask;
            if (!header.ddspf.dwABitMask)
                header.ddspf.dwABitMask = 0xFF000000UL;
        }
        if (rMask && gMask && bMask)
            format = 4;
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
        // Some writers set DDPF_ALPHAPIXELS but leave dwABitMask zero; preserve A8 in high byte for A8R8G8B8.
        unsigned long aMask = header.ddspf.dwABitMask;
        const bool hasAlphaPixelsFlag = (header.ddspf.dwFlags & 0x1) != 0;
        if (aMask == 0 && header.ddspf.dwRGBBitCount == 32 && hasAlphaPixelsFlag && rMask == 0x00FF0000UL &&
            gMask == 0x0000FF00UL && bMask == 0x000000FFUL)
            aMask = 0xFF000000UL;

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

    if (flattenAlpha)
        flattenBgraAlpha(outPixels.data(), width, height);

    std::string pngPath = tgaPath;
    {
        const size_t dot = pngPath.find_last_of('.');
        if (dot != std::string::npos && dot > pngPath.find_last_of("/\\"))
            pngPath = pngPath.substr(0, dot) + ".png";
        else
            pngPath += ".png";
    }
    if (writePng(pngPath.c_str(), width, height, outPixels.data(), flattenAlpha))
    {
        std::cerr << "[DdsToTga] Maya PNG: " << width << "x" << height << " -> " << pngPath
            << (flattenAlpha ? " (opaque MAIN)" : "") << "\n";
        return pngPath;
    }

    if (!writeTga(tgaPath.c_str(), width, height, outPixels.data()))
    {
        std::cerr << "[DdsToTga] Failed to write TGA: " << tgaPath << "\n";
        return std::string();
    }

    std::cerr << "[DdsToTga] Converted OK: " << width << "x" << height << " -> " << tgaPath << "\n";

    return tgaPath;
}

std::string DdsToTgaConverter::mayaSafeImagePath(const std::string& imagePath, const std::string& outputDir, bool flattenAlpha)
{
    if (imagePath.empty())
        return std::string();

    std::string path = imagePath;
    for (char& c : path)
        if (c == '\\')
            c = '/';

    const auto endsWith = [](const std::string& s, const char* ext) -> bool {
        const size_t n = std::strlen(ext);
        if (s.size() < n)
            return false;
        for (size_t i = 0; i < n; ++i)
        {
            char a = s[s.size() - n + i];
            char b = ext[i];
            if (a >= 'A' && a <= 'Z')
                a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z')
                b = static_cast<char>(b + ('a' - 'A'));
            if (a != b)
                return false;
        }
        return true;
    };
    if (isDdsPath(path))
        return convertToTga(path, outputDir, flattenAlpha);

    if (!endsWith(path, ".tga"))
        return path;

    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
        return path;

    unsigned char header[18] = {};
    if (fread(header, 1, 18, f) != 18)
    {
        fclose(f);
        return path;
    }
    const int idLen = header[0];
    const int imgType = header[2];
    const int width = header[12] | (header[13] << 8);
    const int height = header[14] | (header[15] << 8);
    const int bpp = header[16];
    const unsigned char desc = header[17];
    const bool topLeft = (desc & 0x20) != 0;
    if (imgType != 2 || (bpp != 24 && bpp != 32) || width <= 0 || height <= 0)
    {
        fclose(f);
        return path;
    }
    // 0x08 32-bit game TGAs are VP2-readable but DXT5 gloss lives in alpha (body max ~61).
    // Returning them as-is makes Lambert/VP2 treat the character as transparent.
    if (!topLeft && !flattenAlpha)
    {
        fclose(f);
        return path;
    }
    if (idLen > 0)
        fseek(f, idLen, SEEK_CUR);

    const int srcStride = width * (bpp / 8);
    std::vector<unsigned char> src(static_cast<size_t>(srcStride) * static_cast<size_t>(height));
    if (fread(src.data(), 1, src.size(), f) != src.size())
    {
        fclose(f);
        return path;
    }
    fclose(f);

    std::vector<unsigned char> bgra(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    for (int y = 0; y < height; ++y)
    {
        const int srcY = topLeft ? y : (height - 1 - y);
        const unsigned char* row = src.data() + static_cast<size_t>(srcY) * static_cast<size_t>(srcStride);
        unsigned char* dst = bgra.data() + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u;
        for (int x = 0; x < width; ++x)
        {
            if (bpp == 32)
            {
                dst[x * 4 + 0] = row[x * 4 + 0];
                dst[x * 4 + 1] = row[x * 4 + 1];
                dst[x * 4 + 2] = row[x * 4 + 2];
                dst[x * 4 + 3] = flattenAlpha ? 255 : row[x * 4 + 3];
            }
            else
            {
                dst[x * 4 + 0] = row[x * 3 + 0];
                dst[x * 4 + 1] = row[x * 3 + 1];
                dst[x * 4 + 2] = row[x * 3 + 2];
                dst[x * 4 + 3] = 255;
            }
        }
    }

    std::string outPath;
    if (!outputDir.empty())
    {
        outPath = outputDir;
        if (outPath.back() != '/' && outPath.back() != '\\')
            outPath += '/';
        outPath += getBaseNameNoExt(path) + ".png";
    }
    else
    {
        outPath = path;
        const size_t dot = outPath.find_last_of('.');
        if (dot != std::string::npos && dot > outPath.find_last_of("/\\"))
            outPath.resize(dot);
        outPath += "_maya.png";
    }

    {
        std::string a = outPath;
        std::string b = path;
        for (char& c : a)
            if (c == '\\') c = '/';
        for (char& c : b)
            if (c == '\\') c = '/';
        for (char& c : a)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        for (char& c : b)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        if (a == b)
        {
            const size_t dot = outPath.find_last_of('.');
            if (dot != std::string::npos && dot > outPath.find_last_of("/\\"))
                outPath = outPath.substr(0, dot) + "_maya.png";
            else
                outPath += "_maya.png";
        }
    }

    if (writePng(outPath.c_str(), width, height, bgra.data(), flattenAlpha))
    {
        std::cerr << "[DdsToTga] Maya-safe albedo PNG: " << path << " -> " << outPath
            << (flattenAlpha ? " (opaque MAIN)" : "") << "\n";
        return outPath;
    }
    std::cerr << "[DdsToTga] Failed to write PNG for Maya: " << path << "\n";
    return path;
}
