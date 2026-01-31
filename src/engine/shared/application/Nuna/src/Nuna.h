// ======================================================================
//
// Nuna.h
// TRE/titanlst Archive Packer/Unpacker Tool
// Copyright (c) Titan Project
//
// A standalone archive utility supporting:
// - Packing directories into TRE archives
// - Unpacking TRE archives to directories
// - Unpacking titanlst files (extracts from referenced TRE files)
// - Listing archive contents
// - Optional encryption for secure TRE files
//
// ======================================================================

#ifndef NUNA_H
#define NUNA_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <fstream>

namespace Nuna
{

// ======================================================================
// TRE Format Constants
// ======================================================================

constexpr uint32_t TAG_TREE = 'E' | ('E' << 8) | ('R' << 16) | ('T' << 24);  // "TREE"
constexpr uint32_t TAG_0005 = '5' | ('0' << 8) | ('0' << 16) | ('0' << 24);  // "0005"
constexpr uint32_t TAG_0004 = '4' | ('0' << 8) | ('0' << 16) | ('0' << 24);  // "0004"
constexpr uint32_t TAG_NUNA = 'A' | ('N' << 8) | ('U' << 16) | ('N' << 24);  // "NUNA" (encrypted)

// TOC Format Constants
constexpr uint32_t TAG_TOC  = ' ' | ('C' << 8) | ('O' << 16) | ('T' << 24);  // "TOC " (space-C-O-T in little-endian)
constexpr uint32_t TAG_NTOC = 'N' | ('T' << 8) | ('O' << 16) | ('C' << 24);  // "NTOC" (encrypted titanlst)
constexpr uint32_t TAG_0001 = '1' | ('0' << 8) | ('0' << 16) | ('0' << 24);  // "0001"

// ======================================================================
// Compression Types
// ======================================================================

enum class CompressionType : uint32_t
{
    None = 0,
    Deprecated = 1,
    Zlib = 2
};

// ======================================================================
// TRE Header Structure (36 bytes)
// ======================================================================

#pragma pack(push, 1)
struct TreHeader
{
    uint32_t token;
    uint32_t version;
    uint32_t numberOfFiles;
    uint32_t tocOffset;
    uint32_t tocCompressor;
    uint32_t sizeOfTOC;
    uint32_t blockCompressor;
    uint32_t sizeOfNameBlock;
    uint32_t uncompSizeOfNameBlock;
};

// ======================================================================
// TRE Table of Contents Entry (24 bytes)
// ======================================================================

struct TocEntry
{
    uint32_t crc;
    int32_t  length;
    int32_t  offset;
    int32_t  compressor;
    int32_t  compressedLength;
    int32_t  fileNameOffset;
};

// ======================================================================
// TOC File Header (32 bytes)
// ======================================================================

struct TocFileHeader
{
    uint32_t token;                    // 'TOC '
    uint32_t version;                  // '0001'
    uint8_t  tocCompressor;
    uint8_t  fileNameBlockCompressor;
    uint8_t  unused1;
    uint8_t  unused2;
    uint32_t numberOfFiles;
    uint32_t sizeOfTOC;
    uint32_t sizeOfNameBlock;
    uint32_t uncompSizeOfNameBlock;
    uint32_t numberOfTreeFiles;
    uint32_t sizeOfTreeFileNameBlock;
};

// ======================================================================
// TOC Table of Contents Entry (20 bytes)
// ======================================================================

struct TocFileEntry
{
    uint8_t  compressor;
    uint8_t  unused;
    uint16_t treeFileIndex;
    uint32_t crc;
    uint32_t fileNameOffset;
    uint32_t offset;
    uint32_t length;
    uint32_t compressedLength;
};

struct EncryptionHeader
{
    uint32_t encryptionVersion;
    uint8_t  salt[16];
    uint8_t  iv[16];
    uint32_t flags;
};
#pragma pack(pop)

// ======================================================================
// File Entry (for building archives)
// ======================================================================

struct FileEntry
{
    std::string diskPath;
    std::string archivePath;
    int32_t     offset = 0;
    int32_t     length = 0;
    int32_t     compressor = 0;
    int32_t     compressedLength = 0;
    uint32_t    crc = 0;
    bool        deleted = false;
    bool        noCompress = false;
};

// ======================================================================
// Archive Statistics
// ======================================================================

struct ArchiveStats
{
    uint32_t fileCount = 0;
    uint64_t totalUncompressed = 0;
    uint64_t totalCompressed = 0;
    uint32_t version = 0;
    bool     encrypted = false;
};

// ======================================================================
// Options Structures
// ======================================================================

struct EncryptionOptions
{
    bool        enabled = false;
    std::string password;
    uint32_t    version = 1;
};

struct PackOptions
{
    bool               compressToc = true;
    bool               compressFiles = true;
    int                compressionLevel = 6;
    bool               quiet = false;
    bool               verbose = false;
    EncryptionOptions  encryption;
};

struct UnpackOptions
{
    bool               overwrite = false;
    bool               quiet = false;
    bool               verbose = false;
    std::string        filter;
    std::string        treSearchPath;   // For TOC: path to search for TRE files
    EncryptionOptions  encryption;
};

struct ListOptions
{
    bool               showSize = true;
    bool               showCompressed = true;
    bool               showOffset = false;
    std::string        filter;
    EncryptionOptions  encryption;
};

// ======================================================================
// Result/Error Handling
// ======================================================================

enum class ResultCode
{
    Success = 0,
    FileNotFound,
    InvalidArchive,
    CompressionError,
    DecompressionError,
    IOError,
    EncryptionError,
    DecryptionError,
    InvalidPassword,
    InvalidArguments,
    OutOfMemory,
    TreFileNotFound
};

struct Result
{
    ResultCode  code = ResultCode::Success;
    std::string message;
    
    bool ok() const { return code == ResultCode::Success; }
    operator bool() const { return ok(); }
};

// ======================================================================
// TRE API Functions
// ======================================================================

Result pack(const std::string& sourceDir, 
            const std::string& outputTre, 
            const PackOptions& options = PackOptions());

Result unpack(const std::string& inputTre, 
              const std::string& outputDir, 
              const UnpackOptions& options = UnpackOptions());

Result list(const std::string& inputTre, 
            const ListOptions& options = ListOptions(),
            std::vector<std::pair<std::string, TocEntry>>* entries = nullptr);

Result validate(const std::string& inputTre,
                const EncryptionOptions& encryption = EncryptionOptions());

Result getStats(const std::string& inputTre,
                ArchiveStats& stats,
                const EncryptionOptions& encryption = EncryptionOptions());

// ======================================================================
// TOC API Functions
// ======================================================================

Result unpackToc(const std::string& inputToc,
                 const std::string& outputDir,
                 const UnpackOptions& options = UnpackOptions());

Result listToc(const std::string& inputToc,
               const ListOptions& options = ListOptions());

Result validateToc(const std::string& inputToc);

} // namespace Nuna

#endif // NUNA_H

