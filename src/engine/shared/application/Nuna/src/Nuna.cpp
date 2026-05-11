// ======================================================================
//
// Nuna.cpp
// TRE Archive Packer/Unpacker Implementation
// Copyright (c) Titan Project
//
// ======================================================================

#include "Nuna.h"
#include "NunaCrc.h"
#include "NunaCompression.h"
#include "NunaCrypto.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <map>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace Nuna
{

bool layoutTocEntriesFromBlob(const uint8_t* blob, size_t blobLen, uint32_t numberOfFiles, std::vector<TocEntry>& out)
{
    if (numberOfFiles == 0)
    {
        out.clear();
        return true;
    }
    if (!blob || blobLen == 0)
        return false;

    const size_t entrySize = sizeof(TocEntry);
    const size_t minBytes = static_cast<size_t>(numberOfFiles) * entrySize;

    // Uniform stride: blob divides evenly (includes 32-byte padded rows, etc.)
    if (blobLen % numberOfFiles == 0)
    {
        const size_t stride = blobLen / numberOfFiles;
        if (stride >= entrySize)
        {
            out.resize(numberOfFiles);
            for (uint32_t i = 0; i < numberOfFiles; ++i)
                std::memcpy(&out[i], blob + static_cast<size_t>(i) * stride, sizeof(TocEntry));
            return true;
        }
    }

    // Trailing slack: header.sizeOfTOC may exceed numFiles * entrySize (TreeFile reads only the entry span).
    const size_t trimmedLen = blobLen - (blobLen % numberOfFiles);
    if (trimmedLen >= minBytes && trimmedLen / numberOfFiles >= entrySize)
        return layoutTocEntriesFromBlob(blob, trimmedLen, numberOfFiles, out);

    return false;
}

// ======================================================================
// Internal Helper Functions
// ======================================================================

namespace
{

// Normalize path to match client's TreeFile::fixUpFileName behavior
// - Convert to lowercase
// - Convert backslashes to forward slashes  
// - Remove leading slashes, "./" and "../"
// - Collapse consecutive slashes
std::string normalizePath(const std::string& path)
{
    std::string result;
    result.reserve(path.size());
    
    const char* f = path.c_str();
    
    // Skip leading "/" or "\"
    while (*f == '\\' || *f == '/')
        ++f;
    
    // Skip leading "./" or ".\"
    while (f[0] == '.' && (f[1] == '\\' || f[1] == '/'))
        f += 2;
    
    // Skip leading "../" or "..\"
    while (f[0] == '.' && f[1] == '.' && (f[2] == '\\' || f[2] == '/'))
        f += 3;
    
    bool previousIsSlash = false;
    
    for (; *f; ++f)
    {
        char c = *f;
        bool currentIsSlash = (c == '\\' || c == '/');
        
        if (currentIsSlash)
        {
            // Convert to forward slash and skip if previous was also a slash
            if (!previousIsSlash)
            {
                result += '/';
                previousIsSlash = true;
            }
        }
        else
        {
            // Lowercase all other characters
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            previousIsSlash = false;
        }
    }
    
    return result;
}

// Read entire file into a vector
bool readFile(const std::string& path, std::vector<uint8_t>& data)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return false;
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    data.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size))
        return false;
    
    return true;
}

// Write data to file
bool writeFile(const std::string& path, const uint8_t* data, size_t size)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
        return false;
    
    file.write(reinterpret_cast<const char*>(data), size);
    return file.good();
}

// Create directory and all parents
bool createDirectories(const std::string& path)
{
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

// Safe filename from name block (bounds + NUL); avoids Debug vector[] and raw C-string over-read.
bool tocEntryFileName(std::string* out, const TocEntry& entry, const std::vector<char>& nameBlock)
{
    if (!out)
        return false;
    if (entry.fileNameOffset < 0)
        return false;
    const size_t off = static_cast<size_t>(entry.fileNameOffset);
    if (off >= nameBlock.size())
        return false;
    const char* base = nameBlock.data() + off;
    const size_t maxLen = nameBlock.size() - off;
    const void* nulAt = memchr(base, '\0', maxLen);
    if (!nulAt)
        return false;
    out->assign(base, static_cast<const char*>(nulAt));
    return true;
}

Nuna::Result dumpCipherRegions(const std::string& inputTre, const std::string& outputDir)
{
    Nuna::Result result;
    std::ifstream inFile(inputTre, std::ios::binary);
    if (!inFile)
    {
        result.code = Nuna::ResultCode::FileNotFound;
        result.message = "Cannot open file: " + inputTre;
        return result;
    }
    if (!createDirectories(outputDir))
    {
        result.code = Nuna::ResultCode::IOError;
        result.message = "Cannot create output directory: " + outputDir;
        return result;
    }

    inFile.seekg(0, std::ios::end);
    const std::streamsize fileSize = inFile.tellg();
    inFile.seekg(0);

    std::vector<uint8_t> hdr(sizeof(Nuna::TreHeader));
    inFile.read(reinterpret_cast<char*>(hdr.data()), sizeof(Nuna::TreHeader));
    if (inFile.gcount() != static_cast<std::streamsize>(sizeof(Nuna::TreHeader)))
    {
        result.code = Nuna::ResultCode::InvalidArchive;
        result.message = "File too small for TreHeader";
        return result;
    }

    Nuna::TreHeader header{};
    std::memcpy(&header, hdr.data(), sizeof(header));
    if (!Nuna::treMagicKnown(header.token))
    {
        result.code = Nuna::ResultCode::InvalidArchive;
        result.message = "Not a recognized TRE/NUNA/LEGE archive";
        return result;
    }

    const fs::path outRoot(outputDir);
    auto writeBin = [&](const char* leaf, const uint8_t* data, size_t size) -> bool {
        const std::string path = (outRoot / leaf).string();
        return writeFile(path, data, size);
    };

    if (!writeBin("00_tre_header_36.bin", hdr.data(), hdr.size()))
    {
        result.code = Nuna::ResultCode::IOError;
        result.message = "Failed writing 00_tre_header_36.bin";
        return result;
    }

    if (Nuna::treUsesEncryptionHeader(header.token))
    {
        std::vector<uint8_t> eh(sizeof(Nuna::EncryptionHeader));
        inFile.seekg(sizeof(Nuna::TreHeader));
        inFile.read(reinterpret_cast<char*>(eh.data()), eh.size());
        if (static_cast<size_t>(inFile.gcount()) != eh.size())
        {
            result.code = Nuna::ResultCode::InvalidArchive;
            result.message = "Short read on EncryptionHeader";
            return result;
        }
        if (!writeBin("36_encryption_header_40.bin", eh.data(), eh.size()))
        {
            result.code = Nuna::ResultCode::IOError;
            result.message = "Failed writing encryption header blob";
            return result;
        }
    }

    const uint64_t tocOff = static_cast<uint64_t>(header.tocOffset);
    if (tocOff > static_cast<uint64_t>(fileSize) ||
        static_cast<uint64_t>(header.sizeOfTOC) > static_cast<uint64_t>(fileSize) - tocOff)
    {
        result.code = Nuna::ResultCode::InvalidArchive;
        result.message = "TOC region extends past end of file";
        return result;
    }

    inFile.seekg(static_cast<std::streamoff>(tocOff));
    std::vector<uint8_t> toc(static_cast<size_t>(header.sizeOfTOC));
    inFile.read(reinterpret_cast<char*>(toc.data()), toc.size());
    if (static_cast<size_t>(inFile.gcount()) != toc.size())
    {
        result.code = Nuna::ResultCode::InvalidArchive;
        result.message = "Short read on TOC ciphertext";
        return result;
    }
    if (!writeBin("toc_on_disk_cipher.bin", toc.data(), toc.size()))
    {
        result.code = Nuna::ResultCode::IOError;
        result.message = "Failed writing TOC ciphertext";
        return result;
    }

    const uint64_t nameOff = tocOff + static_cast<uint64_t>(header.sizeOfTOC);
    if (nameOff > static_cast<uint64_t>(fileSize) ||
        static_cast<uint64_t>(header.sizeOfNameBlock) > static_cast<uint64_t>(fileSize) - nameOff)
    {
        result.code = Nuna::ResultCode::InvalidArchive;
        result.message = "Name block region extends past end of file";
        return result;
    }

    inFile.seekg(static_cast<std::streamoff>(nameOff));
    std::vector<uint8_t> nb(static_cast<size_t>(header.sizeOfNameBlock));
    inFile.read(reinterpret_cast<char*>(nb.data()), nb.size());
    if (static_cast<size_t>(inFile.gcount()) != nb.size())
    {
        result.code = Nuna::ResultCode::InvalidArchive;
        result.message = "Short read on name block ciphertext";
        return result;
    }
    if (!writeBin("name_block_on_disk_cipher.bin", nb.data(), nb.size()))
    {
        result.code = Nuna::ResultCode::IOError;
        result.message = "Failed writing name block ciphertext";
        return result;
    }

    const uint64_t payloadStart = nameOff + static_cast<uint64_t>(header.sizeOfNameBlock);
    const uint64_t fsz = static_cast<uint64_t>(fileSize);
    if (payloadStart <= fsz)
    {
        const size_t tailLen = static_cast<size_t>(fsz - payloadStart);
        if (tailLen > 0)
        {
            inFile.seekg(static_cast<std::streamoff>(payloadStart));
            std::vector<uint8_t> tail(tailLen);
            inFile.read(reinterpret_cast<char*>(tail.data()), tail.size());
            if (static_cast<size_t>(inFile.gcount()) != tailLen)
            {
                result.code = Nuna::ResultCode::InvalidArchive;
                result.message = "Short read on payload tail";
                return result;
            }
            if (!writeBin("payload_tail_cipher.bin", tail.data(), tail.size()))
            {
                result.code = Nuna::ResultCode::IOError;
                result.message = "Failed writing payload tail";
                return result;
            }
        }
    }

    std::ostringstream man;
    man << "Ciphertext dump (no decryption). Same layout as TreeFile/NUNA/LEGE on-disk regions.\n"
        << "file_size_bytes: " << fileSize << "\n"
        << "tre_header: bytes [0, 36)\n";
    if (Nuna::treUsesEncryptionHeader(header.token))
        man << "encryption_header: bytes [36, 76)\n";
    man << "toc_region: bytes [" << tocOff << ", " << (tocOff + header.sizeOfTOC) << ") size=" << header.sizeOfTOC
        << "\n"
        << "name_block_region: bytes [" << nameOff << ", " << (nameOff + header.sizeOfNameBlock)
        << ") size=" << header.sizeOfNameBlock << "\n"
        << "payload_tail: bytes [" << payloadStart << ", " << fsz << ")\n"
        << "\nSet SWG_TRE_PASSWORD if you obtain the key; run \"nuna analyze\" for probes.\n";

    const std::string manifest = man.str();
    if (!writeBin("README_manifest.txt", reinterpret_cast<const uint8_t*>(manifest.data()), manifest.size()))
    {
        result.code = Nuna::ResultCode::IOError;
        result.message = "Failed writing README_manifest.txt";
        return result;
    }

    result.message = "Wrote ciphertext blobs to " + outputDir;
    return result;
}

// Get parent directory
std::string getParentPath(const std::string& path)
{
    fs::path p(path);
    return p.parent_path().string();
}

// Collect all files in a directory recursively
void collectFiles(const std::string& baseDir, 
                  const std::string& currentDir,
                  std::vector<FileEntry>& entries)
{
    for (const auto& entry : fs::recursive_directory_iterator(currentDir))
    {
        if (entry.is_regular_file())
        {
            FileEntry fe;
            fe.diskPath = entry.path().string();
            
            // Calculate relative path for archive
            fs::path relativePath = fs::relative(entry.path(), baseDir);
            fe.archivePath = normalizePath(relativePath.string());
            
            entries.push_back(fe);
        }
    }
}

// Compare entries by CRC then by name (for sorted TOC)
std::string analyzeFourCcAscii(uint32_t token)
{
    char s[5];
    std::memcpy(s, &token, 4);
    s[4] = 0;
    for (int i = 0; i < 4; ++i)
    {
        const unsigned char u = static_cast<unsigned char>(s[i]);
        if (u < 32 || u > 126)
            s[i] = '.';
    }
    return std::string(s);
}

std::string analyzeHexBytes(const uint8_t* data, size_t len)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i)
        oss << std::setw(2) << static_cast<unsigned>(data[i]) << (i + 1 < len ? " " : "");
    return oss.str();
}

std::string analyzeCompressionLabel(uint32_t c)
{
    if (c == static_cast<uint32_t>(CompressionType::None))
        return "none (0)";
    if (c == static_cast<uint32_t>(CompressionType::Deprecated))
        return "deprecated (1)";
    if (c == static_cast<uint32_t>(CompressionType::Zlib))
        return "zlib (2)";
    return "unknown (" + std::to_string(c) + ")";
}

bool analyzeProbeTocDecrypt(std::ifstream& inFile, const TreHeader& header, EncryptionContext& encCtx)
{
    inFile.seekg(header.tocOffset);

    if (header.tocCompressor == static_cast<uint32_t>(CompressionType::Zlib))
    {
        std::vector<uint8_t> compressed(header.sizeOfTOC);
        inFile.read(reinterpret_cast<char*>(compressed.data()), header.sizeOfTOC);
        encCtx.decryptAt(compressed.data(), compressed.size(), header.tocOffset);
        std::vector<uint8_t> decompressed;
        if (!Compression::decompress(compressed.data(), compressed.size(), decompressed, 0))
            return false;
        std::vector<TocEntry> toc;
        if (!layoutTocEntriesFromBlob(decompressed.data(), decompressed.size(), header.numberOfFiles, toc))
            return false;
        if (header.numberOfFiles == 0)
            return true;
        return toc[0].length >= 0 && toc[0].offset >= 0;
    }

    std::vector<uint8_t> tocData(header.sizeOfTOC);
    inFile.read(reinterpret_cast<char*>(tocData.data()), header.sizeOfTOC);
    encCtx.decryptAt(tocData.data(), tocData.size(), header.tocOffset);
    std::vector<TocEntry> toc;
    if (!layoutTocEntriesFromBlob(tocData.data(), tocData.size(), header.numberOfFiles, toc))
        return false;
    if (header.numberOfFiles == 0)
        return true;
    return toc[0].length >= 0 && toc[0].offset >= 0;
}

bool compareEntries(const FileEntry& a, const FileEntry& b)
{
    if (a.crc != b.crc)
        return a.crc < b.crc;
    
    // Case-insensitive string comparison
    std::string aLower = a.archivePath;
    std::string bLower = b.archivePath;
    std::transform(aLower.begin(), aLower.end(), aLower.begin(), 
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(bLower.begin(), bLower.end(), bLower.begin(), 
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return aLower < bLower;
}

} // anonymous namespace

// ======================================================================
// Shared TRE directory load + selective extract (internal)
// ======================================================================

namespace
{

Result loadTreDirectoryFromOpenFile(std::ifstream& inFile, const UnpackOptions& options,
                                    TreHeader& header, std::vector<TocEntry>& toc,
                                    std::vector<char>& nameBlock, bool& isEncrypted,
                                    EncryptionContext& encCtx)
{
    Result result;

    inFile.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (!treMagicKnown(header.token))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Not a valid TRE archive";
        return result;
    }

    if (!isTreHeaderSupported(header.token, header.version))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Unsupported TRE token/version";
        return result;
    }

    isEncrypted = treUsesEncryptionHeader(header.token);

    EncryptionHeader encHeader = {};
    if (isEncrypted)
    {
        inFile.read(reinterpret_cast<char*>(&encHeader), sizeof(encHeader));

        const std::string password = Crypto::resolveTrePassword(options.encryption.password);

        encCtx.initDecrypt(password, encHeader.salt, encHeader.iv);
    }

    inFile.seekg(header.tocOffset);

    std::vector<uint8_t> tocData(header.sizeOfTOC);
    inFile.read(reinterpret_cast<char*>(tocData.data()), header.sizeOfTOC);
    if (static_cast<size_t>(inFile.gcount()) != header.sizeOfTOC)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Short read on TOC (got " + std::to_string(inFile.gcount()) + ", expected " +
                         std::to_string(header.sizeOfTOC) + ")";
        return result;
    }

    if (isEncrypted)
        encCtx.decryptAt(tocData.data(), tocData.size(), header.tocOffset);

    if (header.tocCompressor == static_cast<uint32_t>(CompressionType::Zlib))
    {
        std::vector<uint8_t> decompressed;
        if (!Compression::decompress(tocData.data(), tocData.size(), decompressed, 0))
        {
            result.code = ResultCode::DecompressionError;
            result.message = "Failed to decompress TOC";
            return result;
        }
        if (!layoutTocEntriesFromBlob(decompressed.data(), decompressed.size(), header.numberOfFiles, toc))
        {
            result.code = ResultCode::InvalidArchive;
            result.message = "TOC size does not match file count (unsupported TOC row layout)";
            return result;
        }
    }
    else
    {
        if (!layoutTocEntriesFromBlob(tocData.data(), tocData.size(), header.numberOfFiles, toc))
        {
            result.code = ResultCode::InvalidArchive;
            result.message = "TOC size does not match file count (expected uncompressed TOC blob)";
            return result;
        }
    }

    const uint64_t nameBlockOffset = static_cast<uint64_t>(header.tocOffset) + header.sizeOfTOC;
    inFile.seekg(nameBlockOffset);

    std::vector<uint8_t> nameBlockData(header.sizeOfNameBlock);
    inFile.read(reinterpret_cast<char*>(nameBlockData.data()), header.sizeOfNameBlock);
    if (static_cast<size_t>(inFile.gcount()) != header.sizeOfNameBlock)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Short read on name block (got " + std::to_string(inFile.gcount()) + ", expected " +
                         std::to_string(header.sizeOfNameBlock) + ")";
        return result;
    }

    if (isEncrypted)
        encCtx.decryptAt(nameBlockData.data(), nameBlockData.size(), nameBlockOffset);

    nameBlock.resize(header.uncompSizeOfNameBlock);

    if (header.blockCompressor == static_cast<uint32_t>(CompressionType::Zlib))
    {
        std::vector<uint8_t> decompressed;
        if (!Compression::decompress(nameBlockData.data(), nameBlockData.size(),
                                     decompressed, header.uncompSizeOfNameBlock))
        {
            result.code = ResultCode::DecompressionError;
            result.message = "Failed to decompress name block";
            return result;
        }
        std::memcpy(nameBlock.data(), decompressed.data(), header.uncompSizeOfNameBlock);
    }
    else
    {
        std::memcpy(nameBlock.data(), nameBlockData.data(), header.uncompSizeOfNameBlock);
    }

    result.message = "OK";
    return result;
}

bool pathIsUnderDirectoryPrefix(const std::string& normPath, const std::string& normPrefix)
{
    if (normPrefix.empty())
        return true;
    if (normPath.size() < normPrefix.size())
        return false;
    if (normPath.compare(0, normPrefix.size(), normPrefix) != 0)
        return false;
    if (normPath.size() == normPrefix.size())
        return true;
    return normPath[normPrefix.size()] == '/';
}

Result writeEntryDataToFile(std::ifstream& inFile, bool isEncrypted, EncryptionContext& encCtx,
                            const TocEntry& entry, const std::string& outPath, bool overwrite,
                            const std::string& displayNameForErrors)
{
    Result result;
    if (entry.length == 0)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Entry is deleted (zero length): " + displayNameForErrors;
        return result;
    }

    if (!overwrite && fs::exists(outPath))
    {
        result.code = ResultCode::Success;
        result.message = "Skipped (exists)";
        return result;
    }

    std::string parentDir = getParentPath(outPath);
    if (!parentDir.empty() && !createDirectories(parentDir))
    {
        result.code = ResultCode::IOError;
        result.message = "Failed to create directory: " + parentDir;
        return result;
    }

    const size_t readSize = (entry.compressedLength > 0) ? static_cast<size_t>(entry.compressedLength)
                                                           : static_cast<size_t>(entry.length);

    std::vector<uint8_t> fileData(readSize);
    inFile.seekg(entry.offset);
    inFile.read(reinterpret_cast<char*>(fileData.data()), readSize);

    if (isEncrypted)
        encCtx.decryptAt(fileData.data(), fileData.size(), entry.offset);

    std::vector<uint8_t> outputData;
    if (entry.compressor == static_cast<int32_t>(CompressionType::Zlib))
    {
        if (!Compression::decompress(fileData.data(), fileData.size(), outputData, entry.length))
        {
            result.code = ResultCode::DecompressionError;
            result.message = "Failed to decompress: " + displayNameForErrors;
            return result;
        }
    }
    else
        outputData = std::move(fileData);

    if (!writeFile(outPath, outputData.data(), outputData.size()))
    {
        result.code = ResultCode::IOError;
        result.message = "Failed to write: " + outPath;
        return result;
    }

    result.message = "OK";
    return result;
}

Result detailExtractOne(const std::string& inputTre, const std::string& archiveInternalPath,
                        const std::string& outputFilePath, const UnpackOptions& options)
{
    Result result;

    std::ifstream inFile(inputTre, std::ios::binary);
    if (!inFile)
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Cannot open file: " + inputTre;
        return result;
    }

    TreHeader header;
    std::vector<TocEntry> toc;
    std::vector<char> nameBlock;
    bool isEncrypted = false;
    EncryptionContext encCtx;

    const Result loadRes = loadTreDirectoryFromOpenFile(inFile, options, header, toc, nameBlock, isEncrypted, encCtx);
    if (!loadRes.ok())
        return loadRes;

    const std::string want = normalizePath(archiveInternalPath);

    for (uint32_t i = 0; i < header.numberOfFiles; ++i)
    {
        const TocEntry& entry = toc[i];
        std::string fileName;
        if (!tocEntryFileName(&fileName, entry, nameBlock))
        {
            result.code = ResultCode::InvalidPassword;
            result.message = "Name block does not decode (wrong password or corrupt archive)";
            return result;
        }
        if (normalizePath(fileName) != want)
            continue;

        const Result wr = writeEntryDataToFile(inFile, isEncrypted, encCtx, entry, outputFilePath, options.overwrite,
                                                 fileName);
        if (!wr.ok())
            return wr;
        result.message = "Extracted: " + fileName;
        return result;
    }

    result.code = ResultCode::InvalidArchive;
    result.message = "Path not found in archive: " + archiveInternalPath;
    return result;
}

Result detailExtractPathPrefix(const std::string& inputTre, const std::string& archiveDirPrefix,
                               const std::string& outputRootDir, const UnpackOptions& options)
{
    Result result;

    std::ifstream inFile(inputTre, std::ios::binary);
    if (!inFile)
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Cannot open file: " + inputTre;
        return result;
    }

    TreHeader header;
    std::vector<TocEntry> toc;
    std::vector<char> nameBlock;
    bool isEncrypted = false;
    EncryptionContext encCtx;

    const Result loadRes = loadTreDirectoryFromOpenFile(inFile, options, header, toc, nameBlock, isEncrypted, encCtx);
    if (!loadRes.ok())
        return loadRes;

    if (!createDirectories(outputRootDir))
    {
        result.code = ResultCode::IOError;
        result.message = "Failed to create output directory: " + outputRootDir;
        return result;
    }

    const std::string pref = normalizePath(archiveDirPrefix);

    uint32_t extractedCount = 0;

    for (uint32_t i = 0; i < header.numberOfFiles; ++i)
    {
        const TocEntry& entry = toc[i];
        std::string fileName;
        if (!tocEntryFileName(&fileName, entry, nameBlock))
            continue;

        if (!pathIsUnderDirectoryPrefix(normalizePath(fileName), pref))
            continue;

        if (entry.length == 0)
            continue;

        if (!options.filter.empty() && fileName.find(options.filter) == std::string::npos)
            continue;

        const std::string outPath = outputRootDir + "/" + fileName;

        const Result wr =
            writeEntryDataToFile(inFile, isEncrypted, encCtx, entry, outPath, options.overwrite, fileName);
        if (!wr.ok())
            return wr;
        if (wr.message == "OK")
            ++extractedCount;
    }

    result.message = "Successfully extracted " + std::to_string(extractedCount) + " files";
    return result;
}

} // anonymous namespace

// ======================================================================
// Pack Implementation
// ======================================================================

Result pack(const std::string& sourceDir, 
            const std::string& outputTre, 
            const PackOptions& options)
{
    Result result;
    
    // Validate source directory
    if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir))
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Source directory does not exist: " + sourceDir;
        return result;
    }
    
    // Collect all files
    std::vector<FileEntry> entries;
    collectFiles(sourceDir, sourceDir, entries);
    
    if (entries.empty())
    {
        result.code = ResultCode::InvalidArguments;
        result.message = "No files found in source directory";
        return result;
    }
    
    // Calculate CRCs and sort entries
    for (auto& entry : entries)
    {
        entry.crc = Crc::calculate(entry.archivePath);
    }
    std::sort(entries.begin(), entries.end(), compareEntries);
    
    // Open output file
    std::ofstream outFile(outputTre, std::ios::binary);
    if (!outFile)
    {
        result.code = ResultCode::IOError;
        result.message = "Failed to create output file: " + outputTre;
        return result;
    }
    
    // Setup encryption if enabled
    EncryptionContext encCtx;
    if (options.encryption.enabled)
    {
        encCtx.initEncrypt(options.encryption.password);
    }
    
    // Write placeholder header (will update later)
    TreHeader header = {};
    header.token = options.encryption.enabled ? TAG_NUNA : TAG_TREE;
    header.version = TAG_0005;
    header.numberOfFiles = static_cast<uint32_t>(entries.size());
    
    outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // If encrypted, write encryption header
    EncryptionHeader encHeader = {};
    if (options.encryption.enabled)
    {
        encHeader.encryptionVersion = options.encryption.version;
        std::memcpy(encHeader.salt, encCtx.getSalt(), Crypto::SALT_SIZE);
        std::memcpy(encHeader.iv, encCtx.getIv(), Crypto::IV_SIZE);
        encHeader.flags = 0;
        
        outFile.write(reinterpret_cast<const char*>(&encHeader), sizeof(encHeader));
    }
    
    // Write file data
    uint64_t totalOriginal = 0;
    uint64_t totalCompressed = 0;
    
    for (size_t i = 0; i < entries.size(); ++i)
    {
        auto& entry = entries[i];
        
        if (!options.quiet)
        {
            std::cout << "[" << (i + 1) << "/" << entries.size() << "] " 
                      << entry.archivePath << std::endl;
        }
        
        // Read file
        std::vector<uint8_t> fileData;
        if (!readFile(entry.diskPath, fileData))
        {
            result.code = ResultCode::IOError;
            result.message = "Failed to read file: " + entry.diskPath;
            return result;
        }
        
        entry.length = static_cast<int32_t>(fileData.size());
        entry.offset = static_cast<int32_t>(outFile.tellp());
        totalOriginal += fileData.size();
        
        // Try compression
        std::vector<uint8_t> compressedData;
        bool useCompression = false;
        
        if (options.compressFiles && !entry.noCompress && fileData.size() > 1024)
        {
            if (Compression::compress(fileData.data(), fileData.size(), 
                                      compressedData, options.compressionLevel))
            {
                if (Compression::shouldCompress(fileData.size(), compressedData.size()))
                {
                    useCompression = true;
                }
            }
        }
        
        // Write data (compressed or not)
        const uint8_t* writeData;
        size_t writeSize;
        
        if (useCompression)
        {
            entry.compressor = static_cast<int32_t>(CompressionType::Zlib);
            entry.compressedLength = static_cast<int32_t>(compressedData.size());
            writeData = compressedData.data();
            writeSize = compressedData.size();
            
            if (options.verbose)
            {
                int ratio = 100 - static_cast<int>((compressedData.size() * 100) / fileData.size());
                std::cout << "  Compressed: " << fileData.size() << " -> " 
                          << compressedData.size() << " (" << ratio << "% saved)" << std::endl;
            }
        }
        else
        {
            entry.compressor = static_cast<int32_t>(CompressionType::None);
            entry.compressedLength = 0;
            writeData = fileData.data();
            writeSize = fileData.size();
        }
        
        // Encrypt if needed
        std::vector<uint8_t> encryptedData;
        if (options.encryption.enabled)
        {
            encryptedData.assign(writeData, writeData + writeSize);
            encCtx.encryptAt(encryptedData.data(), writeSize, entry.offset);
            writeData = encryptedData.data();
        }
        
        outFile.write(reinterpret_cast<const char*>(writeData), writeSize);
        totalCompressed += writeSize;
    }
    
    // Build and write TOC
    uint32_t tocOffset = static_cast<uint32_t>(outFile.tellp());
    
    std::vector<TocEntry> toc(entries.size());
    std::vector<char> nameBlock;
    
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        
        toc[i].crc = entry.crc;
        toc[i].length = entry.length;
        toc[i].offset = entry.offset;
        toc[i].compressor = entry.compressor;
        toc[i].compressedLength = entry.compressedLength;
        toc[i].fileNameOffset = static_cast<int32_t>(nameBlock.size());
        
        // Add filename to name block (including null terminator)
        nameBlock.insert(nameBlock.end(), 
                         entry.archivePath.begin(), 
                         entry.archivePath.end());
        nameBlock.push_back('\0');
    }
    
    // Compress TOC
    std::vector<uint8_t> tocData(toc.size() * sizeof(TocEntry));
    std::memcpy(tocData.data(), toc.data(), tocData.size());
    
    std::vector<uint8_t> compressedToc;
    uint32_t tocCompressor = static_cast<uint32_t>(CompressionType::None);
    uint32_t sizeOfToc = static_cast<uint32_t>(tocData.size());
    
    if (options.compressToc)
    {
        if (Compression::compress(tocData.data(), tocData.size(), compressedToc))
        {
            if (Compression::shouldCompress(tocData.size(), compressedToc.size()))
            {
                tocCompressor = static_cast<uint32_t>(CompressionType::Zlib);
                sizeOfToc = static_cast<uint32_t>(compressedToc.size());
                
                // Encrypt if needed
                if (options.encryption.enabled)
                {
                    encCtx.encryptAt(compressedToc.data(), compressedToc.size(), tocOffset);
                }
                
                outFile.write(reinterpret_cast<const char*>(compressedToc.data()), 
                             compressedToc.size());
            }
            else
            {
                // Encrypt if needed
                if (options.encryption.enabled)
                {
                    encCtx.encryptAt(tocData.data(), tocData.size(), tocOffset);
                }
                
                outFile.write(reinterpret_cast<const char*>(tocData.data()), 
                             tocData.size());
                sizeOfToc = static_cast<uint32_t>(tocData.size());
            }
        }
        else
        {
            if (options.encryption.enabled)
            {
                encCtx.encryptAt(tocData.data(), tocData.size(), tocOffset);
            }
            outFile.write(reinterpret_cast<const char*>(tocData.data()), tocData.size());
        }
    }
    else
    {
        if (options.encryption.enabled)
        {
            encCtx.encryptAt(tocData.data(), tocData.size(), tocOffset);
        }
        
        outFile.write(reinterpret_cast<const char*>(tocData.data()), tocData.size());
    }
    
    // Compress name block
    std::vector<uint8_t> nameBlockData(nameBlock.begin(), nameBlock.end());
    std::vector<uint8_t> compressedNameBlock;
    uint32_t blockCompressor = static_cast<uint32_t>(CompressionType::None);
    uint32_t sizeOfNameBlock = static_cast<uint32_t>(nameBlockData.size());
    uint32_t uncompSizeOfNameBlock = static_cast<uint32_t>(nameBlockData.size());
    
    uint64_t nameBlockOffset = outFile.tellp();
    
    if (options.compressToc)
    {
        if (Compression::compress(nameBlockData.data(), nameBlockData.size(), compressedNameBlock))
        {
            if (Compression::shouldCompress(nameBlockData.size(), compressedNameBlock.size()))
            {
                blockCompressor = static_cast<uint32_t>(CompressionType::Zlib);
                sizeOfNameBlock = static_cast<uint32_t>(compressedNameBlock.size());
                
                if (options.encryption.enabled)
                {
                    encCtx.encryptAt(compressedNameBlock.data(), 
                                    compressedNameBlock.size(), 
                                    nameBlockOffset);
                }
                
                outFile.write(reinterpret_cast<const char*>(compressedNameBlock.data()),
                             compressedNameBlock.size());
            }
            else
            {
                if (options.encryption.enabled)
                {
                    encCtx.encryptAt(nameBlockData.data(), nameBlockData.size(), nameBlockOffset);
                }
                
                outFile.write(reinterpret_cast<const char*>(nameBlockData.data()),
                             nameBlockData.size());
                sizeOfNameBlock = static_cast<uint32_t>(nameBlockData.size());
            }
        }
        else
        {
            if (options.encryption.enabled)
            {
                encCtx.encryptAt(nameBlockData.data(), nameBlockData.size(), nameBlockOffset);
            }
            outFile.write(reinterpret_cast<const char*>(nameBlockData.data()),
                         nameBlockData.size());
        }
    }
    else
    {
        if (options.encryption.enabled)
        {
            encCtx.encryptAt(nameBlockData.data(), nameBlockData.size(), nameBlockOffset);
        }
        
        outFile.write(reinterpret_cast<const char*>(nameBlockData.data()),
                     nameBlockData.size());
    }
    
    // Update header
    header.tocOffset = tocOffset;
    header.tocCompressor = tocCompressor;
    header.sizeOfTOC = sizeOfToc;
    header.blockCompressor = blockCompressor;
    header.sizeOfNameBlock = sizeOfNameBlock;
    header.uncompSizeOfNameBlock = uncompSizeOfNameBlock;
    
    outFile.seekp(0);
    outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    outFile.close();
    
    if (!options.quiet)
    {
        std::cout << "\nPacked " << entries.size() << " files" << std::endl;
        std::cout << "Original: " << totalOriginal << " bytes" << std::endl;
        std::cout << "Compressed: " << totalCompressed << " bytes" << std::endl;
        if (totalOriginal > 0)
        {
            int ratio = 100 - static_cast<int>((totalCompressed * 100) / totalOriginal);
            std::cout << "Compression ratio: " << ratio << "%" << std::endl;
        }
        if (options.encryption.enabled)
        {
            std::cout << "Encryption: Enabled" << std::endl;
        }
    }
    
    result.message = "Successfully packed " + std::to_string(entries.size()) + " files";
    return result;
}

// ======================================================================
// Unpack Implementation
// ======================================================================

Result unpack(const std::string& inputTre, 
              const std::string& outputDir, 
              const UnpackOptions& options)
{
    Result result;
    
    // Open input file
    std::ifstream inFile(inputTre, std::ios::binary);
    if (!inFile)
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Cannot open file: " + inputTre;
        return result;
    }
    
    TreHeader header;
    std::vector<TocEntry> toc;
    std::vector<char> nameBlock;
    bool isEncrypted = false;
    EncryptionContext encCtx;

    const Result loadRes = loadTreDirectoryFromOpenFile(inFile, options, header, toc, nameBlock, isEncrypted, encCtx);
    if (!loadRes.ok())
        return loadRes;

    // Create output directory
    if (!createDirectories(outputDir))
    {
        result.code = ResultCode::IOError;
        result.message = "Failed to create output directory";
        return result;
    }
    
    // Extract files
    uint32_t extractedCount = 0;
    
    for (uint32_t i = 0; i < header.numberOfFiles; ++i)
    {
        const TocEntry& entry = toc[i];
        std::string fileName;
        if (!tocEntryFileName(&fileName, entry, nameBlock))
        {
            result.code = ResultCode::InvalidPassword;
            result.message = "Name block does not decode (wrong password or corrupt archive)";
            return result;
        }

        // Apply filter if specified
        if (!options.filter.empty())
        {
            if (fileName.find(options.filter) == std::string::npos)
                continue;
        }

        // Skip deleted files
        if (entry.length == 0)
            continue;

        if (!options.quiet)
        {
            std::cout << "[" << (i + 1) << "/" << header.numberOfFiles << "] " 
                      << fileName << std::endl;
        }
        
        std::string outPath = outputDir + "/" + fileName;

        const Result wr =
            writeEntryDataToFile(inFile, isEncrypted, encCtx, entry, outPath, options.overwrite, fileName);
        if (!wr.ok())
            return wr;
        if (!options.overwrite && wr.message == "Skipped (exists)")
        {
            if (!options.quiet)
                std::cout << "  Skipping (exists)" << std::endl;
            continue;
        }
        ++extractedCount;
    }
    
    if (!options.quiet)
    {
        std::cout << "\nExtracted " << extractedCount << " files" << std::endl;
    }
    
    result.message = "Successfully extracted " + std::to_string(extractedCount) + " files";
    return result;
}

Result extractOne(const std::string& inputTre, const std::string& archiveInternalPath,
                  const std::string& outputFilePath, const UnpackOptions& options)
{
    return detailExtractOne(inputTre, archiveInternalPath, outputFilePath, options);
}

Result extractPathPrefix(const std::string& inputTre, const std::string& archiveDirPrefix,
                         const std::string& outputRootDir, const UnpackOptions& options)
{
    return detailExtractPathPrefix(inputTre, archiveDirPrefix, outputRootDir, options);
}

// ======================================================================
// List Implementation
// ======================================================================

Result list(const std::string& inputTre, 
            const ListOptions& options,
            std::vector<std::pair<std::string, TocEntry>>* entries)
{
    Result result;
    
    // Open input file
    std::ifstream inFile(inputTre, std::ios::binary);
    if (!inFile)
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Cannot open file: " + inputTre;
        return result;
    }
    
    // Read header
    TreHeader header;
    inFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (!treMagicKnown(header.token))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Not a valid TRE archive";
        return result;
    }

    if (!isTreHeaderSupported(header.token, header.version))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Unsupported TRE token/version";
        return result;
    }

    bool isEncrypted = treUsesEncryptionHeader(header.token);

    // Setup decryption if needed
    EncryptionContext encCtx;
    EncryptionHeader encHeader = {};

    if (isEncrypted)
    {
        inFile.read(reinterpret_cast<char*>(&encHeader), sizeof(encHeader));

        const std::string password = Crypto::resolveTrePassword(options.encryption.password);

        encCtx.initDecrypt(password, encHeader.salt, encHeader.iv);
    }

    // Read TOC
    inFile.seekg(header.tocOffset);

    std::vector<uint8_t> tocData(header.sizeOfTOC);
    inFile.read(reinterpret_cast<char*>(tocData.data()), header.sizeOfTOC);
    if (static_cast<size_t>(inFile.gcount()) != header.sizeOfTOC)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Short read on TOC (got " + std::to_string(inFile.gcount()) + ", expected " +
                         std::to_string(header.sizeOfTOC) + ")";
        return result;
    }

    if (isEncrypted)
    {
        encCtx.decryptAt(tocData.data(), tocData.size(), header.tocOffset);
    }

    std::vector<TocEntry> toc;

    if (header.tocCompressor == static_cast<uint32_t>(CompressionType::Zlib))
    {
        std::vector<uint8_t> decompressed;
        if (!Compression::decompress(tocData.data(), tocData.size(), decompressed, 0))
        {
            result.code = ResultCode::DecompressionError;
            result.message = "Failed to decompress TOC";
            return result;
        }
        if (!layoutTocEntriesFromBlob(decompressed.data(), decompressed.size(), header.numberOfFiles, toc))
        {
            result.code = ResultCode::InvalidArchive;
            result.message = "TOC size does not match file count (unsupported TOC row layout)";
            return result;
        }
    }
    else
    {
        if (!layoutTocEntriesFromBlob(tocData.data(), tocData.size(), header.numberOfFiles, toc))
        {
            result.code = ResultCode::InvalidArchive;
            result.message = "TOC size does not match file count (expected uncompressed TOC blob)";
            return result;
        }
    }

    // Read name block
    const uint64_t nameBlockOffset = static_cast<uint64_t>(header.tocOffset) + header.sizeOfTOC;
    inFile.seekg(nameBlockOffset);

    std::vector<uint8_t> nameBlockData(header.sizeOfNameBlock);
    inFile.read(reinterpret_cast<char*>(nameBlockData.data()), header.sizeOfNameBlock);
    if (static_cast<size_t>(inFile.gcount()) != header.sizeOfNameBlock)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Short read on name block (got " + std::to_string(inFile.gcount()) + ", expected " +
                         std::to_string(header.sizeOfNameBlock) + ")";
        return result;
    }

    if (isEncrypted)
    {
        encCtx.decryptAt(nameBlockData.data(), nameBlockData.size(), nameBlockOffset);
    }

    std::vector<char> nameBlock(header.uncompSizeOfNameBlock);

    if (header.blockCompressor == static_cast<uint32_t>(CompressionType::Zlib))
    {
        std::vector<uint8_t> decompressed;
        if (!Compression::decompress(nameBlockData.data(), nameBlockData.size(),
                                       decompressed, header.uncompSizeOfNameBlock))
        {
            result.code = ResultCode::DecompressionError;
            result.message = "Failed to decompress name block";
            return result;
        }
        std::memcpy(nameBlock.data(), decompressed.data(), header.uncompSizeOfNameBlock);
    }
    else
    {
        std::memcpy(nameBlock.data(), nameBlockData.data(), header.uncompSizeOfNameBlock);
    }

    // Print header info
    std::cout << "TRE Archive: " << inputTre << std::endl;
    std::cout << "Files: " << header.numberOfFiles << std::endl;
    std::cout << "Encrypted: " << (isEncrypted ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
    
    // List files
    uint32_t matchCount = 0;
    bool tocDecryptLooksWrong = false;
    for (uint32_t i = 0; i < header.numberOfFiles; ++i)
    {
        const TocEntry& entry = toc[i];
        std::string fileName;
        if (!tocEntryFileName(&fileName, entry, nameBlock))
        {
            if (!tocDecryptLooksWrong)
                std::cout << "(Cannot list paths: TOC/name offsets invalid — wrong password or corrupt archive.)\n";
            tocDecryptLooksWrong = true;
            continue;
        }

        // Apply filter if specified
        if (!options.filter.empty())
        {
            if (fileName.find(options.filter) == std::string::npos)
                continue;
        }
        
        ++matchCount;
        
        if (entries)
        {
            entries->emplace_back(fileName, entry);
        }
        
        std::cout << fileName;
        
        if (options.showSize)
        {
            std::cout << "\t" << entry.length;
        }
        
        if (options.showCompressed && entry.compressedLength > 0)
        {
            std::cout << "\t" << entry.compressedLength << " (compressed)";
        }
        
        if (options.showOffset)
        {
            std::cout << "\t@" << entry.offset;
        }
        
        std::cout << std::endl;
    }

    if (tocDecryptLooksWrong)
    {
        result.code = ResultCode::InvalidPassword;
        result.message = "TOC/name block does not decrypt cleanly (wrong password for this archive?)";
        return result;
    }

    result.message = "Listed " + std::to_string(header.numberOfFiles) + " files";
    return result;
}

// ======================================================================
// Validate Implementation
// ======================================================================

Result validate(const std::string& inputTre,
                const EncryptionOptions& encryption)
{
    Result result;
    
    std::ifstream inFile(inputTre, std::ios::binary);
    if (!inFile)
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Cannot open file: " + inputTre;
        return result;
    }
    
    TreHeader header;
    inFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (!treMagicKnown(header.token))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Not a valid TRE archive (invalid magic)";
        return result;
    }

    if (!isTreHeaderSupported(header.token, header.version))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Unsupported TRE token/version";
        return result;
    }

    bool isEncrypted = treUsesEncryptionHeader(header.token);
    
    // For encrypted archives, verify we can decrypt the TOC
    if (isEncrypted)
    {
        // Read encryption header
        EncryptionHeader encHeader;
        inFile.read(reinterpret_cast<char*>(&encHeader), sizeof(encHeader));
        
        const std::string password = Crypto::resolveTrePassword(encryption.password);

        // Initialize decryption context
        EncryptionContext encCtx;
        encCtx.initDecrypt(password, encHeader.salt, encHeader.iv);
        
        // Try to read and decrypt TOC to verify password
        inFile.seekg(header.tocOffset);
        
        if (header.tocCompressor == static_cast<uint32_t>(CompressionType::Zlib))
        {
            std::vector<uint8_t> compressed(header.sizeOfTOC);
            inFile.read(reinterpret_cast<char*>(compressed.data()), header.sizeOfTOC);
            if (static_cast<size_t>(inFile.gcount()) != header.sizeOfTOC)
            {
                result.code = ResultCode::InvalidArchive;
                result.message = "Short read on TOC during validate";
                return result;
            }

            encCtx.decryptAt(compressed.data(), header.sizeOfTOC, header.tocOffset);

            std::vector<uint8_t> tocData;
            if (!Compression::decompress(compressed.data(), compressed.size(), tocData, 0))
            {
                result.code = ResultCode::InvalidPassword;
                result.message = "Failed to decrypt/decompress TOC - invalid password or corrupted archive";
                return result;
            }
            std::vector<TocEntry> tocParsed;
            if (!layoutTocEntriesFromBlob(tocData.data(), tocData.size(), header.numberOfFiles, tocParsed))
            {
                result.code = ResultCode::InvalidPassword;
                result.message = "TOC layout invalid after decrypt - wrong password or corrupted archive";
                return result;
            }
        }
        else
        {
            std::vector<uint8_t> tocData(header.sizeOfTOC);
            inFile.read(reinterpret_cast<char*>(tocData.data()), header.sizeOfTOC);
            if (static_cast<size_t>(inFile.gcount()) != header.sizeOfTOC)
            {
                result.code = ResultCode::InvalidArchive;
                result.message = "Short read on TOC during validate";
                return result;
            }

            encCtx.decryptAt(tocData.data(), tocData.size(), header.tocOffset);

            std::vector<TocEntry> tocParsed;
            if (!layoutTocEntriesFromBlob(tocData.data(), tocData.size(), header.numberOfFiles, tocParsed))
            {
                result.code = ResultCode::InvalidPassword;
                result.message = "TOC layout invalid after decrypt - wrong password or corrupted archive";
                return result;
            }
            if (header.numberOfFiles > 0 &&
                (tocParsed[0].offset < 0 || tocParsed[0].length < 0))
            {
                result.code = ResultCode::InvalidPassword;
                result.message = "TOC validation failed - invalid password or corrupted archive";
                return result;
            }
        }
        
        result.message = "Encrypted archive is valid";
    }
    else
    {
        result.message = "Archive is valid";
    }
    
    return result;
}

// ======================================================================
// GetStats Implementation
// ======================================================================

Result getStats(const std::string& inputTre,
                ArchiveStats& stats,
                const EncryptionOptions& encryption)
{
    (void)encryption;
    Result result;
    
    std::ifstream inFile(inputTre, std::ios::binary);
    if (!inFile)
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Cannot open file: " + inputTre;
        return result;
    }
    
    TreHeader header;
    inFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (!treMagicKnown(header.token))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Not a valid TRE archive";
        return result;
    }

    if (!isTreHeaderSupported(header.token, header.version))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Unsupported TRE token/version";
        return result;
    }

    stats.fileCount = header.numberOfFiles;
    stats.version = header.version;
    stats.encrypted = treUsesEncryptionHeader(header.token);
    
    result.message = "Stats retrieved";
    return result;
}

// ======================================================================
// Analyze (forensics — no original decrypt source required for metadata)
// ======================================================================

Result analyze(const std::string& inputTre, const EncryptionOptions& encryption)
{
    Result result;

    std::ifstream inFile(inputTre, std::ios::binary | std::ios::ate);
    if (!inFile)
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Cannot open file: " + inputTre;
        return result;
    }

    const std::streamsize fileSize = inFile.tellg();
    inFile.seekg(0);

    std::vector<uint8_t> prefix(static_cast<size_t>(std::min<std::streamsize>(fileSize, 256)));
    inFile.read(reinterpret_cast<char*>(prefix.data()), prefix.size());
    const std::streamsize prefixGot = inFile.gcount();

    TreHeader header{};
    if (static_cast<size_t>(prefixGot) >= sizeof(TreHeader))
        std::memcpy(&header, prefix.data(), sizeof(TreHeader));
    else
    {
        std::cout << "File too small for TreHeader (" << prefixGot << " bytes)\n";
        std::cout << "Hex: " << analyzeHexBytes(prefix.data(), static_cast<size_t>(prefixGot)) << "\n";
        result.code = ResultCode::InvalidArchive;
        result.message = "File too small";
        return result;
    }

    std::cout << "\n=== Analyze: " << inputTre << " ===\n";
    std::cout << "File size: " << fileSize << " bytes\n";
    if (header.token == TAG_TREE)
        std::cout << "Archive kind: SWG Tree (.tre)\n";
    else if (header.token == TAG_NUNA)
        std::cout << "Archive kind: NUNA encrypted Tree (TitanPak tooling)\n";
    else if (header.token == TAG_LEGE)
        std::cout << "Archive kind: LEGE Legend encrypted tree (.tres / alternate packaging)\n";
    std::cout << "\n";

    const bool magicOk = treMagicKnown(header.token);
    std::cout << "--- TreHeader (" << sizeof(TreHeader) << " bytes @ 0) ---\n";
    std::cout << "token:           0x" << std::hex << std::uppercase << header.token << std::dec << "\n";
    std::cout << "  LE byte chars: \"" << analyzeFourCcAscii(header.token) << "\"\n";
    std::cout << "  Note:          On little-endian, SWG magic 'TREE' is stored as bytes E E R T (often misread as \"EERT\" in dumps).\n";
    std::cout << "                 Version tag '0006' reads as ASCII \"6000\" at bytes 4..7 — concatenated \"EERT6000\" is normal TREE + v0006, not a separate magic.\n";
    std::cout << "                 LEGE is stored as L E G E; paired version NDS3 is \"NDS3\" when read as four ASCII bytes.\n";
    std::cout << "  Recognized:    ";
    if (header.token == TAG_TREE)
        std::cout << "TREE (unencrypted)\n";
    else if (header.token == TAG_NUNA)
        std::cout << "NUNA (TitanPak encrypted)\n";
    else if (header.token == TAG_LEGE)
        std::cout << "LEGE (encrypted Legend / .tres-style)\n";
    else
        std::cout << "UNKNOWN — not TREE / NUNA / LEGE\n";

    std::cout << "version:         0x" << std::hex << std::uppercase << header.version << std::dec
              << "  (\"" << analyzeFourCcAscii(header.version) << "\")\n";
    const bool verOk = isTreHeaderSupported(header.token, header.version);
    std::cout << "  Supported:     "
              << (verOk ? "yes"
                          : "unknown — verify against samples")
              << " (TREE/NUNA: 0004–0006; LEGE: NDS3)\n";

    std::cout << "numberOfFiles:   " << header.numberOfFiles << "\n";
    std::cout << "tocOffset:       " << header.tocOffset << "\n";
    std::cout << "tocCompressor:   " << analyzeCompressionLabel(header.tocCompressor) << "\n";
    std::cout << "sizeOfTOC:       " << header.sizeOfTOC << "\n";
    std::cout << "blockCompressor: " << analyzeCompressionLabel(header.blockCompressor) << "\n";
    std::cout << "sizeOfNameBlock: " << header.sizeOfNameBlock << "\n";
    std::cout << "uncompSizeOfNameBlock: " << header.uncompSizeOfNameBlock << "\n";

    const uint64_t nameBlockOffset = static_cast<uint64_t>(header.tocOffset) + header.sizeOfTOC;
    std::cout << "\n--- Derived layout ---\n";
    std::cout << "Name block starts @ " << nameBlockOffset << "\n";
    if (treUsesEncryptionHeader(header.token))
        std::cout << "Encryption header @ " << sizeof(TreHeader) << " (" << sizeof(EncryptionHeader) << " bytes)\n";

    if (!magicOk || !verOk)
    {
        std::cout << "\n--- First " << std::min(prefix.size(), static_cast<size_t>(64)) << " bytes (hex) ---\n";
        std::cout << analyzeHexBytes(prefix.data(), std::min(prefix.size(), static_cast<size_t>(64))) << "\n";
        std::cout << "\nCompare fields with a known-good SWG .tre (or NUNA TitanPak if encrypted) or hex-edit sections manually.\n";
        result.message = "Analysis complete (non-standard header)";
        return result;
    }

    const bool isEncrypted = treUsesEncryptionHeader(header.token);
    if (isEncrypted)
    {
        inFile.clear();
        inFile.seekg(sizeof(TreHeader));
        EncryptionHeader encHeader{};
        inFile.read(reinterpret_cast<char*>(&encHeader), sizeof(encHeader));

        std::cout << "\n--- EncryptionHeader (" << sizeof(EncryptionHeader) << " bytes @ " << sizeof(TreHeader) << ") ---\n";
        std::cout << "encryptionVersion: " << encHeader.encryptionVersion << "\n";
        std::cout << "flags:             " << encHeader.flags << "\n";
        std::cout << "salt (hex):        " << analyzeHexBytes(encHeader.salt, sizeof(encHeader.salt)) << "\n";
        std::cout << "iv (hex):          " << analyzeHexBytes(encHeader.iv, sizeof(encHeader.iv)) << "\n";
        std::cout << "\nCipher layout: deriveKey(password, salt) -> keystream XOR; IV tweaked per file offset (see NunaCrypto.h EncryptionContext).\n";

        std::cout << "\n--- Password probe (decrypt TOC region only) ---\n";

        std::vector<std::string> passwordsToTry;
        if (!encryption.password.empty())
            passwordsToTry.push_back(encryption.password);
        passwordsToTry.push_back(Crypto::resolveTrePassword(""));

        std::vector<std::string> uniquePw;
        for (const std::string& p : passwordsToTry)
        {
            bool dup = false;
            for (const std::string& u : uniquePw)
            {
                if (u == p)
                {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                uniquePw.push_back(p);
        }

        for (size_t pi = 0; pi < uniquePw.size(); ++pi)
        {
            inFile.clear();
            inFile.seekg(sizeof(TreHeader));
            EncryptionHeader eh{};
            inFile.read(reinterpret_cast<char*>(&eh), sizeof(eh));

            EncryptionContext encCtx;
            encCtx.initDecrypt(uniquePw[pi], eh.salt, eh.iv);

            const bool ok = analyzeProbeTocDecrypt(inFile, header, encCtx);
            std::cout << "  [" << (pi + 1) << "/" << uniquePw.size() << "] ";
            if (!encryption.password.empty() && uniquePw[pi] == encryption.password)
                std::cout << "(from -d/--decrypt) ";
            else if (uniquePw[pi] == Crypto::resolveTrePassword(""))
                std::cout << "(SWG_TRE_PASSWORD env or built-in default) ";
            std::cout << "password \"" << uniquePw[pi] << "\": " << (ok ? "TOC decrypt OK" : "FAILED") << "\n";
        }

        std::cout << "\nIf all probes fail: wrong password, corrupted file, or encryption scheme differs from this Nuna build.\n";
    }
    else
    {
        std::cout << "\n(Unencrypted TREE — TOC/name blocks are plain zlib or raw per compressor fields.)\n";
    }

    std::cout << "\n--- Raw prefix (first 64 bytes, hex) ---\n";
    std::cout << analyzeHexBytes(prefix.data(), std::min(prefix.size(), static_cast<size_t>(64))) << "\n";

    result.message = "Analysis complete";
    return result;
}

Result dumpCipher(const std::string& inputTre, const std::string& outputDir)
{
    return dumpCipherRegions(inputTre, outputDir);
}

Result tryPasswordWordlist(const std::string& inputTre, const std::string& wordlistPath,
                           const PasswordGuessOptions& options)
{
    Result result;

    std::ifstream treFile(inputTre, std::ios::binary);
    if (!treFile)
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Cannot open archive: " + inputTre;
        return result;
    }

    TreHeader header{};
    treFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!treMagicKnown(header.token) || !isTreHeaderSupported(header.token, header.version))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Unsupported or invalid TRE header";
        return result;
    }

    if (!treUsesEncryptionHeader(header.token))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Archive is not encrypted (TREE only). Nothing to guess.";
        return result;
    }

    treFile.seekg(sizeof(TreHeader));
    EncryptionHeader encHeader{};
    treFile.read(reinterpret_cast<char*>(&encHeader), sizeof(encHeader));

    std::ifstream wl(wordlistPath);
    if (!wl)
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Cannot open wordlist: " + wordlistPath;
        return result;
    }

    auto trimGuessLine = [](std::string s) -> std::string {
        while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
            s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
            ++i;
        return s.substr(i);
    };

    std::string line;
    uint64_t lineNum = 0;
    uint64_t tried = 0;

    while (std::getline(wl, line))
    {
        ++lineNum;
        line = trimGuessLine(line);
        if (lineNum == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
            line.erase(0, 3);

        if (line.empty() || line[0] == '#')
            continue;

        if (options.maxAttempts != 0 && tried >= options.maxAttempts)
        {
            result.code = ResultCode::InvalidPassword;
            result.message =
                "Stopped: --max limit reached (" + std::to_string(tried) + " candidates tried, no match)";
            return result;
        }

        ++tried;

        EncryptionContext encCtx;
        encCtx.initDecrypt(line, encHeader.salt, encHeader.iv);

        treFile.clear();
        if (analyzeProbeTocDecrypt(treFile, header, encCtx))
        {
            std::cout << "\n*** MATCH - TOC decrypt validates with this candidate.\n"
                         "*** Example: nuna list \"" << inputTre << "\" -d \"<paste_password>\"\n"
                         "*** Password (exact line; may contain spaces - quote in shell):\n"
                      << line << "\n\n";
            if (!options.quiet && options.maxAttempts != 0)
                std::cerr << "[try-passwords] trial " << tried << "/" << options.maxAttempts << " (MATCH)\n";
            result.message = "Match at wordlist line " + std::to_string(lineNum) + " after " +
                             std::to_string(tried) + " tries.";
            return result;
        }

        if (!options.quiet && options.maxAttempts != 0 && options.maxAttempts <= 512 &&
            tried <= options.maxAttempts)
            std::cerr << "[try-passwords] trial " << tried << "/" << options.maxAttempts << " (no match)\n";

        if (!options.quiet && options.progressEvery != 0 && (tried % options.progressEvery) == 0)
            std::cerr << "[try-passwords] " << tried << " candidates tried...\n";
    }

    result.code = ResultCode::InvalidPassword;
    result.message = "No wordlist line matched (" + std::to_string(tried) + " candidates tried)";
    return result;
}

namespace {

void hexLower16(const uint8_t* p, size_t n, std::string& out)
{
    static const char* digits = "0123456789abcdef";
    out.resize(n * 2);
    for (size_t i = 0; i < n; ++i)
    {
        out[i * 2] = digits[(p[i] >> 4) & 0xF];
        out[i * 2 + 1] = digits[p[i] & 0xF];
    }
}

void hexUpper16(const uint8_t* p, size_t n, std::string& out)
{
    static const char* digits = "0123456789ABCDEF";
    out.resize(n * 2);
    for (size_t i = 0; i < n; ++i)
    {
        out[i * 2] = digits[(p[i] >> 4) & 0xF];
        out[i * 2 + 1] = digits[p[i] & 0xF];
    }
}

bool pushUnique(std::unordered_set<std::string>& seen, std::vector<std::string>& lines, const std::string& s,
                uint64_t maxLines)
{
    if (s.empty() || lines.size() >= maxLines)
        return false;
    if (!seen.insert(s).second)
        return false;
    lines.push_back(s);
    return true;
}

void combineBaseSalt(std::unordered_set<std::string>& seen, std::vector<std::string>& lines, uint64_t maxLines,
                     const std::string& base, const std::string& saltHex, const std::string& ivHex)
{
    static const char* seps[] = {"", "_", "-", ":"};
    const std::string s8 = saltHex.size() >= 8 ? saltHex.substr(0, 8) : saltHex;
    const std::string i8 = ivHex.size() >= 8 ? ivHex.substr(0, 8) : ivHex;

    pushUnique(seen, lines, base, maxLines);

    for (const char* sep : seps)
    {
        std::string sepStr(sep);
        pushUnique(seen, lines, base + sepStr + saltHex, maxLines);
        pushUnique(seen, lines, saltHex + sepStr + base, maxLines);
        pushUnique(seen, lines, base + sepStr + ivHex, maxLines);
        pushUnique(seen, lines, ivHex + sepStr + base, maxLines);
        pushUnique(seen, lines, base + sepStr + s8, maxLines);
        pushUnique(seen, lines, base + sepStr + i8, maxLines);
        pushUnique(seen, lines, s8 + sepStr + base, maxLines);
        pushUnique(seen, lines, i8 + sepStr + base, maxLines);
        pushUnique(seen, lines, base + sepStr + saltHex + sepStr + ivHex, maxLines);
    }
}

} // namespace

Result generateSaltDerivedGuesslist(const std::string& inputTre, const SaltGuessGenOptions& options)
{
    Result result;

    std::ifstream treFile(inputTre, std::ios::binary);
    if (!treFile)
    {
        result.code = ResultCode::FileNotFound;
        result.message = "Cannot open archive: " + inputTre;
        return result;
    }

    TreHeader header{};
    treFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!treMagicKnown(header.token) || !isTreHeaderSupported(header.token, header.version))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Unsupported or invalid TRE header";
        return result;
    }

    if (!treUsesEncryptionHeader(header.token))
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Archive has no EncryptionHeader (unencrypted TREE). Nothing to derive from salt.";
        return result;
    }

    treFile.seekg(sizeof(TreHeader));
    EncryptionHeader enc{};
    treFile.read(reinterpret_cast<char*>(&enc), sizeof(enc));

    std::string saltHex;
    std::string ivHex;
    std::string saltHexU;
    std::string ivHexU;
    hexLower16(enc.salt, Crypto::SALT_SIZE, saltHex);
    hexLower16(enc.iv, Crypto::IV_SIZE, ivHex);
    hexUpper16(enc.salt, Crypto::SALT_SIZE, saltHexU);
    hexUpper16(enc.iv, Crypto::IV_SIZE, ivHexU);

    const uint64_t maxLines = options.maxCandidates == 0 ? 500000 : options.maxCandidates;

    std::unordered_set<std::string> seen;
    std::vector<std::string> lines;
    lines.reserve(static_cast<size_t>(std::min<uint64_t>(maxLines, 100000)));

    auto addRaw = [&](const std::string& s) { pushUnique(seen, lines, s, maxLines); };

    // Raw header material (pipelines sometimes paste hex salt/iv alone or concatenated).
    addRaw(saltHex);
    addRaw(ivHex);
    addRaw(saltHexU);
    addRaw(ivHexU);
    addRaw(saltHex + ivHex);
    addRaw(ivHex + saltHex);
    addRaw(saltHexU + ivHexU);

    std::vector<std::string> bases;
    bases.emplace_back(Crypto::resolveTrePassword(""));
    bases.emplace_back(std::string(Crypto::getTitanPakPassword()));

    static const char* stock[] = {"titan",          "Titan",      "TitanPak", "titanpak", "SWG",       "swg",
                                  "LEGE",           "lege",       "Legend",   "legend",   "NDS3",      "nds3",
                                  "password",       "Password",   "admin",    "secret",   "key",       "pack",
                                  "april",          "April",      "2024",     "2025",     "tre",       "tres"};
    for (const char* s : stock)
        bases.emplace_back(s);

    if (!options.seedsFile.empty())
    {
        std::ifstream sf(options.seedsFile);
        if (!sf)
        {
            result.code = ResultCode::FileNotFound;
            result.message = "Cannot open seeds file: " + options.seedsFile;
            return result;
        }
        std::string ln;
        while (std::getline(sf, ln))
        {
            while (!ln.empty() && (ln.back() == '\r' || ln.back() == ' ' || ln.back() == '\t'))
                ln.pop_back();
            size_t i = 0;
            while (i < ln.size() && (ln[i] == ' ' || ln[i] == '\t'))
                ++i;
            ln = ln.substr(i);
            if (ln.empty() || ln[0] == '#')
                continue;
            bases.push_back(ln);
        }
    }

    // Dedupe bases cheaply
    {
        std::unordered_set<std::string> baseSeen;
        std::vector<std::string> uniqBases;
        for (const auto& b : bases)
        {
            if (b.empty())
                continue;
            if (baseSeen.insert(b).second)
                uniqBases.push_back(b);
        }
        bases = std::move(uniqBases);
    }

    for (const auto& base : bases)
    {
        if (lines.size() >= maxLines)
            break;
        combineBaseSalt(seen, lines, maxLines, base, saltHex, ivHex);
    }

    std::ostream* os = &std::cout;
    std::ofstream fileOut;
    if (!options.outputPath.empty())
    {
        fileOut.open(options.outputPath, std::ios::binary);
        if (!fileOut)
        {
            result.code = ResultCode::IOError;
            result.message = "Cannot write: " + options.outputPath;
            return result;
        }
        os = &fileOut;
    }

    for (const auto& s : lines)
        *os << s << '\n';

    result.message =
        "Emitted " + std::to_string(lines.size()) + " unique candidates (cap " + std::to_string(maxLines) + ").";
    if (lines.size() >= maxLines)
        result.message += " Hit cap - increase --guess-cap or narrow seeds.";
    return result;
}

} // namespace Nuna
