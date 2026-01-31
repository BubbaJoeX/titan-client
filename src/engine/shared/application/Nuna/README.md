# Nuna - TitanPak Archive Tool

A standalone TitanPak archive packer/unpacker for the Titan project. Nuna provides command-line utilities to create, extract, and manage TitanPak (`.titanpak`) and legacy TRE (`.tre`) archive files.

## Features

- **Pack**: Create TitanPak archives from directories
- **Unpack**: Extract TitanPak archives to directories
- **List**: View contents of TitanPak archives
- **Validate**: Check TitanPak archive integrity
- **Auto-Encryption**: `.titanpak` files are automatically encrypted with built-in key
- **Compression**: zlib compression for efficient storage
- **Compatible**: Works with legacy SWG TRE format (version 0004/0005)

## File Extensions

| Extension | Encrypted | Description |
|-----------|-----------|-------------|
| `.titanpak` | **Yes** | TitanPak format - auto-encrypted with built-in key |
| `.tre` | No | Legacy TRE format - unencrypted, SWG compatible |

## Building

### Using CMake

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Dependencies

- C++17 compatible compiler
- zlib library (bundled)

## Usage

### Pack a Directory

Create a TitanPak archive from a directory:

```bash
# Auto-encrypted .titanpak
nuna pack ./game_data assets.titanpak

# Unencrypted legacy .tre  
nuna pack ./game_data legacy.tre
```

### Unpack an Archive

Extract files from a TitanPak archive (auto-decrypts):

```bash
nuna unpack assets.titanpak ./extracted
nuna unpack legacy.tre ./extracted
```

### List Contents

View files in a TitanPak archive (auto-decrypts):

```bash
nuna list assets.titanpak
nuna list assets.titanpak -f ".iff"
```

### Validate Archive

Check if a TitanPak archive is valid:

```bash
nuna validate assets.titanpak
```

## Command Line Options

| Option | Description |
|--------|-------------|
| `-c, --no-compress` | Disable file compression |
| `-t, --no-toc-compress` | Disable TOC/name block compression only |
| `-q, --quiet` | Suppress output |
| `-v, --verbose` | Verbose output |
| `-o, --overwrite` | Overwrite existing files when extracting |
| `-f, --filter <pattern>` | Filter files by pattern |
| `--show-offset` | Show file offsets in list output |
| `-h, --help` | Show help |

## TitanPak Format

TitanPak uses the standard TRE format with automatic encryption:

### Header (36 bytes)
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 4 | Magic ("TREE" for unencrypted, "NUNA" for encrypted) |
| 4 | 4 | Version ("0005" or "0004") |
| 8 | 4 | Number of files |
| 12 | 4 | TOC offset |
| 16 | 4 | TOC compressor |
| 20 | 4 | Size of TOC |
| 24 | 4 | Name block compressor |
| 28 | 4 | Size of name block |
| 32 | 4 | Uncompressed size of name block |

### Encryption Header (follows standard header for encrypted archives)
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 4 | Encryption version |
| 4 | 16 | Salt |
| 20 | 16 | IV |
| 36 | 4 | Flags |

## SwgClient Integration

The SwgClient automatically detects and decrypts `.titanpak` files using the same built-in encryption key. No configuration is required - simply place `.titanpak` files alongside or instead of `.tre` files.

## Security Notes

The encryption key is compiled into both the Nuna tool and SwgClient binaries. This provides:

- **Asset protection**: Prevents casual extraction of game assets
- **Seamless operation**: No password prompts or configuration needed
- **Tamper detection**: Modified files will fail to decrypt properly

**Important**: Do not share source code publicly as it contains the encryption key.

## License

Copyright (c) Titan Project. All rights reserved.
