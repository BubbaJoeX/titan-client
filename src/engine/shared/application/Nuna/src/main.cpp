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
//   nuna analyze <input.tre|.titanpak> [options]
//   nuna toc <input.titanlst> [list|unpack|validate] [options]
//
// ======================================================================

#include "Nuna.h"
#include "NunaCrypto.h"
#include "NunaToc.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>

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
    std::string treSearchPath;
    bool noCompress = false;
    bool noTocCompress = false;
    bool quiet = false;
    bool verbose = false;
    bool overwrite = false;
    bool showHelp = false;
    bool showOffset = false;
    uint64_t maxGuesses = 0;      ///< 0 = unlimited (try-passwords)
    uint64_t progressEvery = 0;   ///< 0 = use Nuna default
    std::string guessOutPath;     ///< salt-guesses --guess-out
    std::string guessSeedsPath;   ///< salt-guesses --seeds
    uint64_t guessCap = 0;        ///< 0 = default max in generator
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
  nuna analyze <input.tre|.titanpak> [options]
  nuna dump-cipher <input.tre> <output_dir>
  nuna try-passwords <input.tre> <wordlist.txt> [options]
  nuna salt-guesses <input.tre> [--guess-out <file>] [--seeds <file>] [--guess-cap N]
  nuna toc create <output.titanlst> <tre1> <tre2> ... [options]
  nuna toc list <input.titanlst> [options]
  nuna toc unpack <input.titanlst> <directory> [options]
  nuna toc validate <input.titanlst>

Commands:
  pack       Create a TitanPak archive from a directory
  unpack     Extract a TitanPak archive to a directory  
  list       List contents of a TitanPak archive
  validate   Validate a TitanPak archive
  analyze    Dump header / encryption metadata / layout (for RE without crypto source)
  dump-cipher Copy ciphertext regions to files (no password; offline inspection)
  try-passwords Dictionary attack: try each wordlist line until TOC decrypt OK (NUNA/LEGE)
  salt-guesses Build candidate passwords from archive salt/IV + bases (pipe to try-passwords)
  toc        Work with titanlst/TOC files (create, list, unpack, validate)

Supported Formats:
  .titanpak  - TitanPak format (auto-encrypted with built-in key)
  .tre       - Legacy TRE format (unencrypted, SWG compatible)
  .titanlst  - titanlst/TOC format (references multiple TRE files)

Options:
  -c, --no-compress           Disable file compression
  -t, --no-toc-compress       Disable TOC/name block compression only
  -q, --quiet                 Suppress output
  -v, --verbose               Verbose output
  -o, --overwrite             Overwrite existing files when extracting
  -f, --filter <pattern>      Filter files by pattern when extracting/listing
  -s, --search-path <path>    Path to search for TRE files (for toc unpack)
  --show-offset               Show file offsets in list output
  -d, --decrypt <password>    Password for unpack/list or TOC probe in analyze
                              If omitted, uses SWG_TRE_PASSWORD env or built-in TitanPak key
  --max <n>                   Stop try-passwords after n candidates (default: unlimited).
                              If n<=512, stderr shows trial k/n after each probe (e.g. 7/8).
  --progress-every <n>        Print stderr progress every n tries (default: 50000; 0 = off)
  --guess-out <file>          salt-guesses: write candidates to file (default: stdout)
  --seeds <file>              salt-guesses: extra newline-separated tokens to combine
  --guess-cap <n>             salt-guesses: max unique lines (default: 500000)
  -h, --help                  Show this help

Examples:
  nuna pack ./data assets.titanpak       (auto-encrypted)
  nuna unpack assets.titanpak ./extracted
  nuna list assets.titanpak
  nuna pack ./data legacy.tre            (unencrypted .tre)
  nuna list legacy.tre
  nuna toc create custom.titanlst bottom.tre top.tre
  nuna toc list default.titanlst
  nuna toc unpack default.titanlst ./extracted -s ./tre_files
  nuna analyze mystery.titanpak
  nuna analyze mystery.titanpak -d guess_password

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
        else if (arg == "-s" || arg == "--search-path")
        {
            if (i + 1 < argc)
            {
                cmd.treSearchPath = argv[++i];
            }
        }
        else if (arg == "--show-offset")
        {
            cmd.showOffset = true;
        }
        else if (arg == "--max" || arg == "--max-guesses")
        {
            if (i + 1 < argc)
                cmd.maxGuesses = std::strtoull(argv[++i], nullptr, 10);
        }
        else if (arg == "--progress-every")
        {
            if (i + 1 < argc)
                cmd.progressEvery = std::strtoull(argv[++i], nullptr, 10);
        }
        else if (arg == "--guess-out")
        {
            if (i + 1 < argc)
                cmd.guessOutPath = argv[++i];
        }
        else if (arg == "--seeds")
        {
            if (i + 1 < argc)
                cmd.guessSeedsPath = argv[++i];
        }
        else if (arg == "--guess-cap")
        {
            if (i + 1 < argc)
                cmd.guessCap = std::strtoull(argv[++i], nullptr, 10);
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
        options.encryption.password = Nuna::Crypto::resolveTrePassword(cmd.password);

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
        options.encryption.password = Nuna::Crypto::resolveTrePassword(cmd.password);

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
        encryption.password = cmd.password;

        result = Nuna::validate(cmd.arg1, encryption);
        
        if (result.ok())
        {
            std::cout << "Archive is valid" << std::endl;
        }
    }
    else if (cmd.command == "analyze" || cmd.command == "info")
    {
        if (cmd.arg1.empty())
        {
            std::cerr << "Error: analyze requires <input.tre|.titanpak>" << std::endl;
            return 1;
        }

        Nuna::EncryptionOptions encryption;
        encryption.password = cmd.password;

        result = Nuna::analyze(cmd.arg1, encryption);
    }
    else if (cmd.command == "dump-cipher" || cmd.command == "dumpcipher")
    {
        if (cmd.arg1.empty() || cmd.arg2.empty())
        {
            std::cerr << "Error: dump-cipher requires <input.tre> <output_directory>" << std::endl;
            return 1;
        }

        result = Nuna::dumpCipher(cmd.arg1, cmd.arg2);
    }
    else if (cmd.command == "try-passwords" || cmd.command == "guess-password" || cmd.command == "try-password")
    {
        if (cmd.arg1.empty() || cmd.arg2.empty())
        {
            std::cerr << "Error: try-passwords requires <input.tre> <wordlist.txt>" << std::endl;
            std::cerr << "  Options: --max N  --progress-every N  -q" << std::endl;
            return 1;
        }

        Nuna::PasswordGuessOptions guessOpts;
        guessOpts.maxAttempts = cmd.maxGuesses;
        if (cmd.progressEvery != 0)
            guessOpts.progressEvery = cmd.progressEvery;
        guessOpts.quiet = cmd.quiet;

        result = Nuna::tryPasswordWordlist(cmd.arg1, cmd.arg2, guessOpts);
    }
    else if (cmd.command == "salt-guesses" || cmd.command == "salt-guess")
    {
        if (cmd.arg1.empty())
        {
            std::cerr << "Error: salt-guesses requires <input.tre>" << std::endl;
            std::cerr << "  Optional: --guess-out <file>  --seeds <extra_tokens.txt>  --guess-cap N" << std::endl;
            return 1;
        }

        Nuna::SaltGuessGenOptions genOpts;
        genOpts.outputPath = cmd.guessOutPath;
        genOpts.seedsFile = cmd.guessSeedsPath;
        if (cmd.guessCap != 0)
            genOpts.maxCandidates = cmd.guessCap;

        result = Nuna::generateSaltDerivedGuesslist(cmd.arg1, genOpts);
    }
    // TOC/titanlst commands
    else if (cmd.command == "toc" || cmd.command == "titanlst")
    {
        if (cmd.arg1.empty())
        {
            std::cerr << "Error: toc requires a subcommand (create, list, unpack, validate)" << std::endl;
            return 1;
        }
        
        std::string subCommand = cmd.arg1;
        
        if (subCommand == "create" || subCommand == "c")
        {
            if (cmd.arg2.empty())
            {
                std::cerr << "Error: toc create requires <output.titanlst> <tre1> <tre2> ..." << std::endl;
                return 1;
            }
            
            std::string outputFile = cmd.arg2;
            std::vector<std::string> treFiles;
            
            // Collect all TRE files from remaining args
            for (int i = 1; i < argc; ++i)
            {
                std::string arg = argv[i];
                if (arg == cmd.command) continue;
                if (arg == subCommand) continue;
                if (arg == outputFile) continue;
                if (arg[0] == '-') continue;
                treFiles.push_back(arg);
            }
            
            if (treFiles.empty())
            {
                std::cerr << "Error: toc create requires at least one TRE file" << std::endl;
                return 1;
            }
            
            Nuna::PackOptions options;
            options.compressToc = !cmd.noTocCompress;
            options.quiet = cmd.quiet;
            options.verbose = cmd.verbose;
            
            result = Nuna::createTitanlst(outputFile, treFiles, options);
        }
        else if (subCommand == "list" || subCommand == "l")
        {
            if (cmd.arg2.empty())
            {
                std::cerr << "Error: toc list requires <input.titanlst>" << std::endl;
                return 1;
            }
            
            Nuna::ListOptions options;
            options.showOffset = cmd.showOffset;
            options.filter = cmd.filter;
            
            result = Nuna::listTitanlst(cmd.arg2, options);
        }
        else if (subCommand == "unpack" || subCommand == "u" || subCommand == "extract" || subCommand == "x")
        {
            if (cmd.arg2.empty())
            {
                std::cerr << "Error: toc unpack requires <input.titanlst> <directory>" << std::endl;
                return 1;
            }
            
            // Find output directory from remaining args
            std::string inputFile = cmd.arg2;
            std::string outputDir;
            
            // Scan for output directory (first non-option after arg2)
            for (int i = 1; i < argc; ++i)
            {
                std::string arg = argv[i];
                if (arg == cmd.command) continue;
                if (arg == subCommand) continue;
                if (arg == inputFile) continue;
                if (arg[0] == '-') continue;
                outputDir = arg;
                break;
            }
            
            if (outputDir.empty())
            {
                std::cerr << "Error: toc unpack requires <input.titanlst> <directory>" << std::endl;
                return 1;
            }
            
            Nuna::UnpackOptions options;
            options.overwrite = cmd.overwrite;
            options.quiet = cmd.quiet;
            options.verbose = cmd.verbose;
            options.filter = cmd.filter;
            options.treSearchPath = cmd.treSearchPath;
            
            result = Nuna::unpackTitanlst(inputFile, outputDir, options);
        }
        else if (subCommand == "validate" || subCommand == "v")
        {
            if (cmd.arg2.empty())
            {
                std::cerr << "Error: toc validate requires <input.titanlst>" << std::endl;
                return 1;
            }
            
            result = Nuna::validateTitanlst(cmd.arg2);
            
            if (result.ok())
            {
                std::cout << "titanlst file is valid" << std::endl;
            }
        }
        else
        {
            std::cerr << "Error: Unknown toc subcommand '" << subCommand << "'" << std::endl;
            std::cerr << "Valid subcommands: list, unpack, validate" << std::endl;
            return 1;
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
