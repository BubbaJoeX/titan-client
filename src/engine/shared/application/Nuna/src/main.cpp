// ======================================================================
//
// main.cpp
// Nuna - TitanPak Archive Packer/Unpacker Command Line Tool
// Copyright (c) Titan Project
//
// Usage:
//   nuna pack <directory> <output.titanpak> [options]
//   nuna unpack <input.titanpak> <directory> [options]
//   nuna list <input.titanpak> [options]
//   nuna validate <input.titanpak> [options]
//
// ======================================================================

#include "Nuna.h"
#include "NunaCrypto.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstring>

// ======================================================================
// Command Line Parsing
// ======================================================================


struct CommandLine
{
    std::string command;
    std::string arg1;
    std::string arg2;
    std::string password;
    std::string filter;
    bool noCompress = false;
    bool noTocCompress = false;
    bool quiet = false;
    bool verbose = false;
    bool overwrite = false;
    bool showHelp = false;
    bool showOffset = false;
};

void printUsage()
{
    std::cout << R"(
Nuna - TitanPak Archive Tool v1.0
Copyright (c) Titan Project

Usage:
  nuna pack <directory> <output.titanpak> [options]
  nuna unpack <input.titanpak> <directory> [options]
  nuna list <input.titanpak> [options]
  nuna validate <input.titanpak> [options]

Commands:
  pack       Create a TitanPak archive from a directory
  unpack     Extract a TitanPak archive to a directory  
  list       List contents of a TitanPak archive
  validate   Validate a TitanPak archive

Supported Formats:
  .titanpak  - TitanPak format (auto-encrypted with built-in key)
  .tre       - Legacy TRE format (unencrypted, SWG compatible)

Options:
  -c, --no-compress           Disable file compression
  -t, --no-toc-compress       Disable TOC/name block compression only
  -q, --quiet                 Suppress output
  -v, --verbose               Verbose output
  -o, --overwrite             Overwrite existing files when extracting
  -f, --filter <pattern>      Filter files by pattern when extracting/listing
  --show-offset               Show file offsets in list output
  -h, --help                  Show this help

Examples:
  nuna pack ./data assets.titanpak       (auto-encrypted)
  nuna unpack assets.titanpak ./extracted
  nuna list assets.titanpak
  nuna pack ./data legacy.tre            (unencrypted .tre)
  nuna list legacy.tre

)" << std::endl;
}

CommandLine parseCommandLine(int argc, char* argv[])
{
    CommandLine cmd;
    
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help")
        {
            cmd.showHelp = true;
        }
        else if (arg == "-e" || arg == "--encrypt")
        {
            if (i + 1 < argc)
            {
                cmd.password = argv[++i];
            }
        }
        else if (arg == "-d" || arg == "--decrypt")
        {
            if (i + 1 < argc)
            {
                cmd.password = argv[++i];
            }
        }
        else if (arg == "-c" || arg == "--no-compress")
        {
            cmd.noCompress = true;
        }
        else if (arg == "-t" || arg == "--no-toc-compress")
        {
            cmd.noTocCompress = true;
        }
        else if (arg == "-q" || arg == "--quiet")
        {
            cmd.quiet = true;
        }
        else if (arg == "-v" || arg == "--verbose")
        {
            cmd.verbose = true;
        }
        else if (arg == "-o" || arg == "--overwrite")
        {
            cmd.overwrite = true;
        }
        else if (arg == "-f" || arg == "--filter")
        {
            if (i + 1 < argc)
            {
                cmd.filter = argv[++i];
            }
        }
        else if (arg == "--show-offset")
        {
            cmd.showOffset = true;
        }
        else if (cmd.command.empty())
        {
            cmd.command = arg;
        }
        else if (cmd.arg1.empty())
        {
            cmd.arg1 = arg;
        }
        else if (cmd.arg2.empty())
        {
            cmd.arg2 = arg;
        }
    }
    
    return cmd;
}

// ======================================================================
// Main Entry Point
// ======================================================================

int main(int argc, char* argv[])
{
    CommandLine cmd = parseCommandLine(argc, argv);
    
    if (cmd.showHelp || argc < 2)
    {
        printUsage();
        return 0;
    }
    
    Nuna::Result result;
    
    // Pack command
    if (cmd.command == "pack" || cmd.command == "p")
    {
        if (cmd.arg1.empty() || cmd.arg2.empty())
        {
            std::cerr << "Error: pack requires <directory> and <output.titanpak>" << std::endl;
            return 1;
        }
        
        Nuna::PackOptions options;
        options.compressFiles = !cmd.noCompress;
        options.compressToc = !cmd.noTocCompress;
        options.quiet = cmd.quiet;
        options.verbose = cmd.verbose;
        
        // Auto-enable encryption for .titanpak files using hardcoded password
        // Use .tre extension for unencrypted legacy archives
        std::string outputFile = cmd.arg2;
        const std::string titanpakExt = ".titanpak";
        bool isTitanPak = false;
        
        if (outputFile.length() >= titanpakExt.length())
        {
            std::string ext = outputFile.substr(outputFile.length() - titanpakExt.length());
            isTitanPak = (ext == titanpakExt);
        }
        
        if (isTitanPak)
        {
            // Auto-encrypt .titanpak files with hardcoded password
            options.encryption.enabled = true;
            options.encryption.password = Nuna::Crypto::getTitanPakPassword();
        }
        
        result = Nuna::pack(cmd.arg1, cmd.arg2, options);
    }
    // Unpack command
    else if (cmd.command == "unpack" || cmd.command == "u" || cmd.command == "extract" || cmd.command == "x")
    {
        if (cmd.arg1.empty() || cmd.arg2.empty())
        {
            std::cerr << "Error: unpack requires <input.titanpak> and <directory>" << std::endl;
            return 1;
        }
        
        Nuna::UnpackOptions options;
        options.overwrite = cmd.overwrite;
        options.quiet = cmd.quiet;
        options.verbose = cmd.verbose;
        options.filter = cmd.filter;
        
        // Always enable encryption with hardcoded password for encrypted archives
        // The unpack function will detect if the archive is actually encrypted
        options.encryption.enabled = true;
        options.encryption.password = cmd.password.empty() ? 
            Nuna::Crypto::getTitanPakPassword() : cmd.password;
        
        result = Nuna::unpack(cmd.arg1, cmd.arg2, options);
    }
    // List command
    else if (cmd.command == "list" || cmd.command == "l")
    {
        if (cmd.arg1.empty())
        {
            std::cerr << "Error: list requires <input.titanpak>" << std::endl;
            return 1;
        }
        
        Nuna::ListOptions options;
        options.showOffset = cmd.showOffset;
        options.filter = cmd.filter;
        
        // Always enable encryption with hardcoded password for encrypted archives
        options.encryption.enabled = true;
        options.encryption.password = cmd.password.empty() ? 
            Nuna::Crypto::getTitanPakPassword() : cmd.password;
        
        result = Nuna::list(cmd.arg1, options);
    }
    // Validate command
    else if (cmd.command == "validate" || cmd.command == "v")
    {
        if (cmd.arg1.empty())
        {
            std::cerr << "Error: validate requires <input.tre>" << std::endl;
            return 1;
        }
        
        Nuna::EncryptionOptions encryption;
        if (!cmd.password.empty())
        {
            encryption.enabled = true;
            encryption.password = cmd.password;
        }
        
        result = Nuna::validate(cmd.arg1, encryption);
        
        if (result.ok())
        {
            std::cout << "Archive is valid" << std::endl;
        }
    }
    else
    {
        std::cerr << "Error: Unknown command '" << cmd.command << "'" << std::endl;
        printUsage();
        return 1;
    }
    
    // Handle result
    if (!result.ok())
    {
        std::cerr << "Error: " << result.message << std::endl;
        return static_cast<int>(result.code);
    }
    
    if (!cmd.quiet && !result.message.empty())
    {
        std::cout << result.message << std::endl;
    }
    
    return 0;
}
