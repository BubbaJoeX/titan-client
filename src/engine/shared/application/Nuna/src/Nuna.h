// ======================================================================
//
// Nuna.h
// TRE Archive Packer/Unpacker Tool
// Copyright (c) Titan Project
//
// A standalone TRE archive utility supporting:
// - Packing directories into TRE archives
// - Unpacking TRE archives to directories
// - Listing TRE archive contents
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

// Tags are stored as big-endian (characters in reading order)
// 'TREE' in file = bytes 0x54 0x52 0x45 0x45 = "TREE"
// When read as uint32_t on little-endian: 0x45455254
constexpr uint32_t TAG_TREE = 'E' | ('E' << 8) | ('R' << 16) | ('T' << 24);  // "TREE" as read from file
constexpr uint32_t TAG_0005 = '5' | ('0' << 8) | ('0' << 16) | ('0' << 24);  // "0005" as read from file
constexpr uint32_t TAG_0004 = '4' | ('0' << 8) | ('0' << 16) | ('0' << 24);  // "0004" as read from file

// Encrypted TRE magic (NUNA)
constexpr uint32_t TAG_NUNA = 'A' | ('N' << 8) | ('U' << 16) | ('N' << 24);  // "NUNA" as read from file

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
    uint32_t token;                    // 'TREE' or 'NUNA' for encrypted
    uint32_t version;                  // '0005' or '0004'
    uint32_t numberOfFiles;            // Number of files in archive
    uint32_t tocOffset;                // Offset to table of contents
    uint32_t tocCompressor;            // Compressor used for TOC
    uint32_t sizeOfTOC;                // Size of compressed TOC
    uint32_t blockCompressor;          // Compressor used for name block
    uint32_t sizeOfNameBlock;          // Size of compressed name block
    uint32_t uncompSizeOfNameBlock;    // Uncompressed size of name block
};

// ======================================================================
// Table of Contents Entry (24 bytes)
// ======================================================================

struct TocEntry
{
    uint32_t crc;                      // CRC32 of filename (for fast lookup)
    int32_t  length;                   // Uncompressed file length
    int32_t  offset;                   // Offset in archive
    int32_t  compressor;               // Compression type used
    int32_t  compressedLength;         // Compressed length (0 if uncompressed)
    int32_t  fileNameOffset;           // Offset into name block
};

// ======================================================================
// Encrypted TRE Header Extension (follows standard header)
// ======================================================================

struct EncryptionHeader
{
    uint32_t encryptionVersion;        // Encryption version (1 = XOR, 2 = future AES)
    uint8_t  salt[16];                 // Salt for key derivation
    uint8_t  iv[16];                   // Initialization vector
    uint32_t flags;                    // Encryption flags
};
#pragma pack(pop)

// ======================================================================
// File Entry (for building archives)
// ======================================================================

struct FileEntry
{
    std::string diskPath;              // Path on disk
    std::string archivePath;           // Path in archive
    int32_t     offset = 0;
    int32_t     length = 0;
    int32_t     compressor = 0;
    int32_t     compressedLength = 0;
    uint32_t    crc = 0;
    bool        deleted = false;
    bool        noCompress = false;
};

// ======================================================================
// TRE Archive Statistics
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
// Encryption Options
// ======================================================================

struct EncryptionOptions
{
    bool        enabled = false;
    std::string password;
    uint32_t    version = 1;           // 1 = XOR-based, 2 = reserved for AES
};

// ======================================================================
// Pack Options
// ======================================================================

struct PackOptions
{
    bool               compressToc = true;
    bool               compressFiles = true;
    int                compressionLevel = 6;
    bool               quiet = false;
    bool               verbose = false;
    EncryptionOptions  encryption;
};

// ======================================================================
// Unpack Options
// ======================================================================

struct UnpackOptions
{
    bool               overwrite = false;
    bool               quiet = false;
    bool               verbose = false;
    std::string        filter;          // Optional filename filter
    EncryptionOptions  encryption;
};

// ======================================================================
// List Options
// ======================================================================

struct ListOptions
{
    bool               showSize = true;
    bool               showCompressed = true;
    bool               showOffset = false;
    std::string        filter;           // Optional filename filter
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
    OutOfMemory
};

struct Result
{
    ResultCode  code = ResultCode::Success;
    std::string message;
    
    bool ok() const { return code == ResultCode::Success; }
    operator bool() const { return ok(); }
};

// ======================================================================
// Main API Functions
// ======================================================================

// Pack a directory into a TRE archive
Result pack(const std::string& sourceDir, 
            const std::string& outputTre, 
            const PackOptions& options = PackOptions());

// Unpack a TRE archive to a directory
Result unpack(const std::string& inputTre, 
              const std::string& outputDir, 
              const UnpackOptions& options = UnpackOptions());

// List contents of a TRE archive
Result list(const std::string& inputTre, 
            const ListOptions& options = ListOptions(),
            std::vector<std::pair<std::string, TocEntry>>* entries = nullptr);

// Validate a TRE archive
Result validate(const std::string& inputTre,
                const EncryptionOptions& encryption = EncryptionOptions());

// Get archive statistics
Result getStats(const std::string& inputTre,
                ArchiveStats& stats,
                const EncryptionOptions& encryption = EncryptionOptions());

} // namespace Nuna

#endif // NUNA_H
