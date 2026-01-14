// ======================================================================
//
// NunaCompression.h
// TRE Archive Compression Support
// Copyright (c) Titan Project
//
// Provides zlib compression/decompression for TRE archives.
//
// ======================================================================

#ifndef NUNA_COMPRESSION_H
#define NUNA_COMPRESSION_H

#include <cstdint>
#include <vector>
#include <zlib.h>

namespace Nuna
{

// ======================================================================
// Compression Utilities
// ======================================================================

class Compression
{
public:
    // Compress data using zlib
    static bool compress(const uint8_t* input, 
                         size_t inputSize,
                         std::vector<uint8_t>& output,
                         int level = 6)
    {
        // Estimate output size (worst case is slightly larger than input)
        uLongf outputSize = compressBound(static_cast<uLong>(inputSize));
        output.resize(outputSize);
        
        int result = compress2(output.data(), 
                               &outputSize,
                               input, 
                               static_cast<uLong>(inputSize),
                               level);
        
        if (result != Z_OK)
        {
            output.clear();
            return false;
        }
        
        output.resize(outputSize);
        return true;
    }

    // Decompress data using zlib
    static bool decompress(const uint8_t* input,
                           size_t inputSize,
                           std::vector<uint8_t>& output,
                           size_t expectedSize)
    {
        output.resize(expectedSize);
        uLongf outputSize = static_cast<uLongf>(expectedSize);
        
        int result = uncompress(output.data(),
                                &outputSize,
                                input,
                                static_cast<uLong>(inputSize));
        
        if (result != Z_OK)
        {
            output.clear();
            return false;
        }
        
        output.resize(outputSize);
        return true;
    }
    
    // Decompress to buffer (caller manages memory)
    static bool decompressToBuffer(const uint8_t* input,
                                   size_t inputSize,
                                   uint8_t* output,
                                   size_t outputSize)
    {
        uLongf actualSize = static_cast<uLongf>(outputSize);
        
        int result = uncompress(output,
                                &actualSize,
                                input,
                                static_cast<uLong>(inputSize));
        
        return (result == Z_OK);
    }

    // Check if compression is beneficial (returns true if compressed is smaller)
    static bool shouldCompress(size_t originalSize, size_t compressedSize)
    {
        // Only use compression if we save at least 10% or 512 bytes
        if (compressedSize >= originalSize)
            return false;
            
        size_t savings = originalSize - compressedSize;
        if (savings < 512 && savings < originalSize / 10)
            return false;
            
        return true;
    }
};

} // namespace Nuna

#endif // NUNA_COMPRESSION_H
