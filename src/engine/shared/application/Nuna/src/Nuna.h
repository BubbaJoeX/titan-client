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
constexpr uint32_t TAG_0006 = '6' | ('0' << 8) | ('0' << 16) | ('0' << 24);  // "0006" (LE appears as "6000" after TREE/"EERT")
constexpr uint32_t TAG_NUNA = 'A' | ('N' << 8) | ('U' << 16) | ('N' << 24);  // "NUNA" (encrypted)
/// Other server / alternate encrypted tree (e.g. `.tres`) — same TreHeader + EncryptionHeader layout as NUNA; version typically NDS3.
constexpr uint32_t TAG_LEGE = 'L' | ('E' << 8) | ('G' << 16) | ('E' << 24);  // on-disk LE bytes "LEGE"
constexpr uint32_t TAG_NDS3 = 'N' | ('D' << 8) | ('S' << 16) | ('3' << 24);  // version tag "NDS3"

inline bool treMagicKnown(uint32_t token)
{
    return token == TAG_TREE || token == TAG_NUNA || token == TAG_LEGE;
}

inline bool treUsesEncryptionHeader(uint32_t token)
{
    return token == TAG_NUNA || token == TAG_LEGE;
}

inline bool isTreHeaderSupported(uint32_t token, uint32_t version)
{
    if (token == TAG_TREE || token == TAG_NUNA)
        return version == TAG_0004 || version == TAG_0005 || version == TAG_0006;
    if (token == TAG_LEGE)
        return version == TAG_NDS3;
    return false;
}

// TOC Format Constants
constexpr uint32_t TAG_TOC  = ' ' | ('C' << 8) | ('O' << 16) | ('T' << 24);  // "TOC " (space-C-O-T in little-endian)
constexpr uint32_t TAG_NTOC = 'N' | ('T' << 8) | ('O' << 16) | ('C' << 24);  // "NTOC" (encrypted titanlst)
constexpr uint32_t TAG_0001 = '1' | ('0' << 8) | ('0' << 16) | ('0' << 24);  // "0001"
/// Titanlst / SearchTOC table-of-contents file version (some community patch stacks write 0002 with a custom index blob).
constexpr uint32_t TAG_0002 = '2' | ('0' << 8) | ('0' << 16) | ('0' << 24);  // "0002"

// ======================================================================
// Compression Types
// ======================================================================

enum class CompressionType : uint32_t
{
    None = 0,
    Deprecated = 1,
    Zlib = 2
};

/// TreeFile treats any non-zero TOC / name-block / per-file compressor as zlib-wrapped payload (see
/// TreeFile::SearchTree: `isCompressed` / `if (header.blockCompressor)`). Nuna matches that for unpack.
inline bool treFieldImpliesZlibPayload(uint32_t field)
{
    return field != 0;
}

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

/// Map a raw TOC blob to `TocEntry` rows. Some SWG `.tre` archives pad each row (e.g. 32 bytes per file);
/// only the leading fields are read into `TocEntry`.
bool layoutTocEntriesFromBlob(const uint8_t* blob, size_t blobLen, uint32_t numberOfFiles, std::vector<TocEntry>& out);

/// After XOR decrypt: decode TOC bytes per `tocCompressor` hint, with opposite-path fallback (TREE v4–v6 blind recovery).
bool decodeTreTocBytes(uint32_t tocCompressorField, const uint8_t* tocBytes, size_t tocSize, uint32_t numberOfFiles,
                       std::vector<TocEntry>& outToc);

/// After XOR decrypt: decode filename blob per `blockCompressor` hint, with loose zlib and mis-tag fallbacks.
bool decodeTreNameBlock(uint32_t blockCompressorField, const uint8_t* src, size_t srcSize, uint32_t uncompSize,
                        std::vector<char>& nameBlockOut);

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

/// Options for `tryPasswordWordlist` (dictionary recovery on encrypted archives).
struct PasswordGuessOptions
{
    uint64_t maxAttempts = 0;       ///< Stop after this many tried candidates (0 = unlimited).
    uint64_t progressEvery = 50000; ///< Stderr progress every N tries (0 = no periodic progress).
    bool     quiet = false;
};

/// Options for `generateSaltDerivedGuesslist` — candidates derived from on-disk salt/IV (see `EncryptionHeader`).
struct SaltGuessGenOptions
{
    std::string seedsFile;      ///< Optional extra newline-separated tokens to combine (trimmed; # comments).
    std::string outputPath;     ///< Empty = write to stdout.
    uint64_t maxCandidates = 500000; ///< Hard cap on emitted lines (deduped).
};

/// Options for `carve-zlib` — scan arbitrary binary for embedded zlib-wrapped deflate streams (no archive TOC).
struct CarveZlibOptions
{
    size_t minInflatedBytes = 64;       ///< Ignore tiny successful inflates.
    size_t maxExtractedStreams = 0;     ///< 0 = no limit.
    bool   quiet = false;
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

/// Extract one internal path (forward slashes; matched after the same normalization as pack uses) to a single output file.
Result extractOne(const std::string& inputTre,
                  const std::string& archiveInternalPath,
                  const std::string& outputFilePath,
                  const UnpackOptions& options = UnpackOptions());

/// Extract all files under an internal directory prefix within one .tre (prefix normalized like pack; empty prefix extracts all).
Result extractPathPrefix(const std::string& inputTre,
                         const std::string& archiveDirPrefix,
                         const std::string& outputRootDir,
                         const UnpackOptions& options = UnpackOptions());

/// Print TOC row, contiguous slot size, optional next-file gap, and hex head (decrypted if NUNA/LEGE) — use when extract fails or for RE.
Result inspectEntry(const std::string& inputTre,
                    const std::string& archiveInternalPath,
                    const EncryptionOptions& encryption = EncryptionOptions());

Result list(const std::string& inputTre, 
            const ListOptions& options = ListOptions(),
            std::vector<std::pair<std::string, TocEntry>>* entries = nullptr);

Result validate(const std::string& inputTre,
                const EncryptionOptions& encryption = EncryptionOptions());

Result getStats(const std::string& inputTre,
                ArchiveStats& stats,
                const EncryptionOptions& encryption = EncryptionOptions());

/// Print header, encryption metadata, layout offsets, and optional TOC decrypt probe (for RE when crypto source is unavailable).
Result analyze(const std::string& inputTre,
               const EncryptionOptions& encryption = EncryptionOptions());

/// Copy on-disk ciphertext regions (header, encryption header, TOC, names, payload tail) without decrypting — for inspection when the password is unknown.
Result dumpCipher(const std::string& inputTre, const std::string& outputDir);

/// Forensic: scan a file for zlib CMF/FLG + deflate streams and write each successful inflate to `outputDir` as `zlib_off_0x........_out_N.bin`. Does not use the TRE TOC; works on plaintext or mixed binary (will not recover XOR ciphertext as zlib).
Result carveZlibStreams(const std::string& inputPath, const std::string& outputDir,
                        const CarveZlibOptions& opts = CarveZlibOptions());

/// Try each non-empty line of `wordlistPath` as the TitanPak/LEGE password until TOC decrypt validates (same checks as `analyze` TOC probe).
Result tryPasswordWordlist(const std::string& inputTre, const std::string& wordlistPath,
                           const PasswordGuessOptions& options = PasswordGuessOptions());

/// Emit newline-separated password candidates using salt/IV from the archive header plus common bases and separators (feeds `try-passwords`).
Result generateSaltDerivedGuesslist(const std::string& inputTre, const SaltGuessGenOptions& options = SaltGuessGenOptions());

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

