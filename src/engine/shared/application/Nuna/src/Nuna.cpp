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

namespace fs = std::filesystem;

namespace Nuna
{

// ======================================================================
// Internal Helper Functions
// ======================================================================

namespace
{

// Normalize path separators to forward slashes
std::string normalizePath(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    
    // Remove leading slash if present
    if (!result.empty() && result[0] == '/')
        result = result.substr(1);
    
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
        
        if (!options.encryption.enabled || options.encryption.password.empty())
        {
            result.code = ResultCode::InvalidPassword;
            result.message = "Archive is encrypted, password required";
            return result;
        }
        
        encCtx.initDecrypt(options.encryption.password, encHeader.salt, encHeader.iv);
    }
    
    if (header.version != TAG_0005 && header.version != TAG_0004)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Unsupported TRE version";
        return result;
    }
    
    // Read TOC
    inFile.seekg(header.tocOffset);
    
    std::vector<uint8_t> tocData(header.sizeOfTOC);
    inFile.read(reinterpret_cast<char*>(tocData.data()), header.sizeOfTOC);
    
    if (isEncrypted)
    {
        encCtx.decryptAt(tocData.data(), tocData.size(), header.tocOffset);
    }
    
    std::vector<TocEntry> toc(header.numberOfFiles);
    size_t tocSize = header.numberOfFiles * sizeof(TocEntry);
    
    if (header.tocCompressor == static_cast<uint32_t>(CompressionType::Zlib))
    {
        std::vector<uint8_t> decompressed;
        if (!Compression::decompress(tocData.data(), tocData.size(), decompressed, tocSize))
        {
            result.code = ResultCode::DecompressionError;
            result.message = "Failed to decompress TOC";
            return result;
        }
        std::memcpy(toc.data(), decompressed.data(), tocSize);
    }
    else
    {
        std::memcpy(toc.data(), tocData.data(), tocSize);
    }
    
    // Read name block
    uint64_t nameBlockOffset = static_cast<uint64_t>(header.tocOffset) + header.sizeOfTOC;
    inFile.seekg(nameBlockOffset);
    
    std::vector<uint8_t> nameBlockData(header.sizeOfNameBlock);
    inFile.read(reinterpret_cast<char*>(nameBlockData.data()), header.sizeOfNameBlock);
    
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
        
        if (!options.encryption.enabled || options.encryption.password.empty())
        {
            result.code = ResultCode::InvalidPassword;
            result.message = "Archive is encrypted, password required";
            return result;
        }
        
        encCtx.initDecrypt(options.encryption.password, encHeader.salt, encHeader.iv);
    }
    
    // Read TOC
    inFile.seekg(header.tocOffset);
    
    std::vector<uint8_t> tocData(header.sizeOfTOC);
    inFile.read(reinterpret_cast<char*>(tocData.data()), header.sizeOfTOC);
    
    if (isEncrypted)
    {
        encCtx.decryptAt(tocData.data(), tocData.size(), header.tocOffset);
    }
    
    std::vector<TocEntry> toc(header.numberOfFiles);
    size_t tocSize = header.numberOfFiles * sizeof(TocEntry);
    
    if (header.tocCompressor == static_cast<uint32_t>(CompressionType::Zlib))
    {
        std::vector<uint8_t> decompressed;
        if (!Compression::decompress(tocData.data(), tocData.size(), decompressed, tocSize))
        {
            result.code = ResultCode::DecompressionError;
            result.message = "Failed to decompress TOC";
            return result;
        }
        std::memcpy(toc.data(), decompressed.data(), tocSize);
    }
    else
    {
        std::memcpy(toc.data(), tocData.data(), tocSize);
    }
    
    // Read name block
    uint64_t nameBlockOffset = static_cast<uint64_t>(header.tocOffset) + header.sizeOfTOC;
    inFile.seekg(nameBlockOffset);
    
    std::vector<uint8_t> nameBlockData(header.sizeOfNameBlock);
    inFile.read(reinterpret_cast<char*>(nameBlockData.data()), header.sizeOfNameBlock);
    
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
    
    if (header.version != TAG_0005 && header.version != TAG_0004)
    {
        result.code = ResultCode::InvalidArchive;
        result.message = "Unsupported TRE version";
        return result;
    }
    
    if (header.token == TAG_NUNA && (!encryption.enabled || encryption.password.empty()))
    {
        result.code = ResultCode::InvalidPassword;
        result.message = "Archive is encrypted, password required for validation";
        return result;
    }
    
    result.message = "Archive is valid";
    return result;
}

// ======================================================================
// GetStats Implementation
// ======================================================================

Result getStats(const std::string& inputTre,
                ArchiveStats& stats,
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
        result.message = "Not a valid TRE archive";
        return result;
    }
    
    stats.fileCount = header.numberOfFiles;
    stats.version = header.version;
    stats.encrypted = (header.token == TAG_NUNA);
    
    result.message = "Stats retrieved";
    return result;
}

} // namespace Nuna
