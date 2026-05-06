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
    if (blobLen % numberOfFiles != 0)
        return false;
    const size_t stride = blobLen / numberOfFiles;
    if (stride < sizeof(TocEntry))
        return false;
    out.resize(numberOfFiles);
    for (uint32_t i = 0; i < numberOfFiles; ++i)
        std::memcpy(&out[i], blob + static_cast<size_t>(i) * stride, sizeof(TocEntry));
    return true;
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
    
    // Read header
    TreHeader header;
    inFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (header.token != TAG_TREE && header.token != TAG_NUNA)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Not a valid TRE archive";
        return result;
    }
    
    
    bool isEncrypted = (header.token == TAG_NUNA);
    
    // Setup decryption if needed
    EncryptionContext encCtx;
    EncryptionHeader encHeader = {};
    
    if (isEncrypted)
    {
        inFile.read(reinterpret_cast<char*>(&encHeader), sizeof(encHeader));
        
        // Use provided password or fall back to default
        std::string password = options.encryption.password;
        if (password.empty())
        {
            password = TITANPAK_PASSWORD;
        }
        
        encCtx.initDecrypt(password, encHeader.salt, encHeader.iv);
    }
    
    if (header.version != TAG_0005 && header.version != TAG_0004 && header.version != TAG_0006)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Unsupported TRE version";
        return result;
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
        std::string fileName = &nameBlock[entry.fileNameOffset];
        
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
        
        // Create output path
        std::string outPath = outputDir + "/" + fileName;
        std::string parentDir = getParentPath(outPath);
        
        if (!parentDir.empty() && !createDirectories(parentDir))
        {
            result.code = ResultCode::IOError;
            result.message = "Failed to create directory: " + parentDir;
            return result;
        }
        
        // Check if file exists
        if (!options.overwrite && fs::exists(outPath))
        {
            if (!options.quiet)
            {
                std::cout << "  Skipping (exists)" << std::endl;
            }
            continue;
        }
        
        // Read file data
        size_t readSize = (entry.compressedLength > 0) ? 
                          static_cast<size_t>(entry.compressedLength) : 
                          static_cast<size_t>(entry.length);
        
        std::vector<uint8_t> fileData(readSize);
        inFile.seekg(entry.offset);
        inFile.read(reinterpret_cast<char*>(fileData.data()), readSize);
        
        // Decrypt if needed
        if (isEncrypted)
        {
            encCtx.decryptAt(fileData.data(), fileData.size(), entry.offset);
        }
        
        // Decompress if needed
        std::vector<uint8_t> outputData;
        
        if (entry.compressor == static_cast<int32_t>(CompressionType::Zlib))
        {
            if (!Compression::decompress(fileData.data(), fileData.size(),
                                         outputData, entry.length))
            {
                result.code = ResultCode::DecompressionError;
                result.message = "Failed to decompress: " + fileName;
                return result;
            }
        }
        else
        {
            outputData = std::move(fileData);
        }
        
        // Write file
        if (!writeFile(outPath, outputData.data(), outputData.size()))
        {
            result.code = ResultCode::IOError;
            result.message = "Failed to write: " + outPath;
            return result;
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
    
    if (header.token != TAG_TREE && header.token != TAG_NUNA)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Not a valid TRE archive";
        return result;
    }
    
    bool isEncrypted = (header.token == TAG_NUNA);
    
    // Setup decryption if needed
    EncryptionContext encCtx;
    EncryptionHeader encHeader = {};
    
    if (isEncrypted)
    {
        inFile.read(reinterpret_cast<char*>(&encHeader), sizeof(encHeader));
        
        // Use provided password or fall back to default
        std::string password = options.encryption.password;
        if (password.empty())
        {
            password = TITANPAK_PASSWORD;
        }
        
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
    for (uint32_t i = 0; i < header.numberOfFiles; ++i)
    {
        const TocEntry& entry = toc[i];
        std::string fileName = &nameBlock[entry.fileNameOffset];
        
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
    
    if (header.token != TAG_TREE && header.token != TAG_NUNA)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Not a valid TRE archive (invalid magic)";
        return result;
    }
    
    if (header.version != TAG_0005 && header.version != TAG_0004 && header.version != TAG_0006)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Unsupported TRE version";
        return result;
    }
    
    bool isEncrypted = (header.token == TAG_NUNA);
    
    // For encrypted archives, verify we can decrypt the TOC
    if (isEncrypted)
    {
        // Read encryption header
        EncryptionHeader encHeader;
        inFile.read(reinterpret_cast<char*>(&encHeader), sizeof(encHeader));
        
        // Use provided password or fall back to default
        std::string password = encryption.password;
        if (password.empty())
        {
            password = TITANPAK_PASSWORD;
        }
        
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
    
    if (header.token != TAG_TREE && header.token != TAG_NUNA)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Not a valid TRE archive";
        return result;
    }
    
    stats.fileCount = header.numberOfFiles;
    stats.version = header.version;
    stats.encrypted = (header.token == TAG_NUNA);
    
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
    std::cout << "\n";

    const bool magicOk = (header.token == TAG_TREE || header.token == TAG_NUNA);
    std::cout << "--- TreHeader (" << sizeof(TreHeader) << " bytes @ 0) ---\n";
    std::cout << "token:           0x" << std::hex << std::uppercase << header.token << std::dec << "\n";
    std::cout << "  LE byte chars: \"" << analyzeFourCcAscii(header.token) << "\"\n";
    std::cout << "  Note:          On little-endian, SWG magic 'TREE' is stored as bytes E E R T (often misread as \"EERT\" in dumps).\n";
    std::cout << "                 Version tag '0006' reads as ASCII \"6000\" at bytes 4..7 — concatenated \"EERT6000\" is normal TREE + v0006, not a separate magic.\n";
    std::cout << "  Recognized:    ";
    if (header.token == TAG_TREE)
        std::cout << "TREE (unencrypted)\n";
    else if (header.token == TAG_NUNA)
        std::cout << "NUNA (TitanPak encrypted)\n";
    else
        std::cout << "UNKNOWN — not standard TREE/NUNA\n";

    std::cout << "version:         0x" << std::hex << std::uppercase << header.version << std::dec
              << "  (\"" << analyzeFourCcAscii(header.version) << "\")\n";
    const bool verOk = (header.version == TAG_0005 || header.version == TAG_0004 || header.version == TAG_0006);
    std::cout << "  Supported:     " << (verOk ? "yes (0004 / 0005 / 0006)" : "unknown — verify against samples") << "\n";

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
    if (header.token == TAG_NUNA)
        std::cout << "Encryption header @ " << sizeof(TreHeader) << " (" << sizeof(EncryptionHeader) << " bytes)\n";

    if (!magicOk || !verOk)
    {
        std::cout << "\n--- First " << std::min(prefix.size(), static_cast<size_t>(64)) << " bytes (hex) ---\n";
        std::cout << analyzeHexBytes(prefix.data(), std::min(prefix.size(), static_cast<size_t>(64))) << "\n";
        std::cout << "\nCompare fields with a known-good SWG .tre (or NUNA TitanPak if encrypted) or hex-edit sections manually.\n";
        result.message = "Analysis complete (non-standard header)";
        return result;
    }

    const bool isEncrypted = (header.token == TAG_NUNA);
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
        passwordsToTry.emplace_back(TITANPAK_PASSWORD);

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
            else if (uniquePw[pi] == TITANPAK_PASSWORD)
                std::cout << "(Nuna built-in default) ";
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

} // namespace Nuna
