// ======================================================================
// NunaCompression.h - Compression utilities for Nuna
// Copyright (c) Titan Project
// ======================================================================

#ifndef NUNA_COMPRESSION_H
#define NUNA_COMPRESSION_H

#include <algorithm>
#include <vector>
#include <cstdint>
#include <zlib.h>

namespace Nuna
{
namespace Compression
{

// Compress data using zlib
inline bool compress(const uint8_t* data, size_t dataSize, 
                     std::vector<uint8_t>& compressed, int level = Z_DEFAULT_COMPRESSION)
{
    uLongf compSize = compressBound(static_cast<uLong>(dataSize));
    compressed.resize(compSize);
    
    int result = compress2(compressed.data(), &compSize,
                           data, static_cast<uLong>(dataSize), level);
    
    if (result != Z_OK)
        return false;
    
    compressed.resize(compSize);
    return true;
}

// Decompress zlib-wrapped DEFLATE, or raw DEFLATE (no zlib header).
// If expectedSize != 0, output length must match exactly (legacy TOC/name blocks).
// If expectedSize == 0, accept any successful inflate length (variable TOC row sizes in some SWG .tre).
inline bool decompress(const uint8_t* data, size_t dataSize,
                       std::vector<uint8_t>& decompressed, size_t expectedSize)
{
    if (!data || dataSize == 0)
        return false;

    if (expectedSize != 0)
    {
        // Try zlib uncompress() with increasingly large output buffers (some archives need > nominal TOC size).
        for (unsigned mul = 1u; mul <= 128u; mul *= 2u)
        {
            size_t bufLen = expectedSize * static_cast<size_t>(mul);
            if (bufLen / mul != expectedSize)
                break;
            if (bufLen > 128u * 1024u * 1024u)
                break;

            decompressed.resize(bufLen);
            uLongf destLen = static_cast<uLongf>(bufLen);
            const int zr = uncompress(decompressed.data(), &destLen,
                                      data, static_cast<uLong>(dataSize));
            if (zr == Z_OK)
            {
                if (destLen != expectedSize)
                    return false;
                decompressed.resize(static_cast<size_t>(destLen));
                return true;
            }
            if (zr != Z_BUF_ERROR && zr != Z_MEM_ERROR)
                break;
        }
    }
    else
    {
        // Unknown uncompressed length — grow buffer until uncompress succeeds or stream is rejected.
        for (size_t bufLen = std::max(dataSize * 4, size_t(65536));
             bufLen <= 128u * 1024u * 1024u;
             bufLen = (bufLen < size_t(1024 * 1024)) ? bufLen * 2u : bufLen + bufLen / 2u)
        {
            decompressed.resize(bufLen);
            uLongf destLen = static_cast<uLongf>(bufLen);
            const int zr = uncompress(decompressed.data(), &destLen,
                                      data, static_cast<uLong>(dataSize));
            if (zr == Z_OK)
            {
                decompressed.resize(static_cast<size_t>(destLen));
                return true;
            }
            if (zr != Z_BUF_ERROR && zr != Z_MEM_ERROR)
                break;
        }
    }

    // Raw DEFLATE (inflateInit2 windowBits -15) — pipelines that omit zlib CMF/FLG
    z_stream strm{};
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
    strm.avail_in = static_cast<uInt>(dataSize);
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if (inflateInit2(&strm, -15) != Z_OK)
        return false;

    decompressed.clear();
    decompressed.resize(std::max(expectedSize * 2, size_t(65536)));
    strm.next_out = decompressed.data();
    strm.avail_out = static_cast<uInt>(decompressed.size());

    for (;;)
    {
        const int ret = inflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_END)
            break;
        if (ret != Z_OK && ret != Z_BUF_ERROR)
        {
            inflateEnd(&strm);
            return false;
        }
        const size_t oldSz = decompressed.size();
        if (oldSz > 128u * 1024u * 1024u)
        {
            inflateEnd(&strm);
            return false;
        }
        decompressed.resize(oldSz * 2);
        strm.next_out = reinterpret_cast<Bytef*>(decompressed.data() + strm.total_out);
        strm.avail_out = static_cast<uInt>(decompressed.size() - strm.total_out);
    }

    inflateEnd(&strm);
    decompressed.resize(strm.total_out);
    if (expectedSize != 0 && strm.total_out != expectedSize)
        return false;
    return true;
}

// Check if compression is worthwhile (at least 5% savings)
inline bool shouldCompress(size_t originalSize, size_t compressedSize)
{
    if (originalSize == 0)
        return false;
    return compressedSize < (originalSize * 95 / 100);
}

} // namespace Compression
} // namespace Nuna

#endif // NUNA_COMPRESSION_H
