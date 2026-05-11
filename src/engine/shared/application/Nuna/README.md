# Nuna - TitanPak Archive Tool

A standalone TitanPak archive packer/unpacker for the Titan project. Nuna provides command-line utilities to create, extract, and manage TitanPak (`.titanpak`) and legacy TRE (`.tre`) archive files.

## Features

- **Pack**: Create TitanPak archives from directories
- **Unpack**: Extract TitanPak archives to directories
- **List**: View contents of TitanPak archives
- **Validate**: Check TitanPak archive integrity
- **Auto-Encryption**: `.titanpak` files are automatically encrypted with built-in key
- **Compression**: zlib compression for efficient storage
- **Compatible**: Works with legacy SWG TRE format (versions **0004 / 0005 / 0006**)

## File Extensions

Extension is only a **convention**. What matters is the **header magic** (`TREE` vs `NUNA` vs `LEGE`) and **version** — run `nuna analyze <file>` if unsure.

| Extension | Encrypted | Description |
|-----------|-----------|-------------|
| `.titanpak` | Usually **yes** (`NUNA`) | Pack defaults to encrypted TitanPak (same crypto as LEGE below). |
| `.tre` | **Often no** (`TREE` + `0004`–`0006`) | Classic SWG tree — unencrypted. Same extension has been used for other payloads; use **analyze** to see magic. |
| `.tres` / Legend `.tre` | **Yes** (`LEGE` + **NDS3**) | Legend encrypted tree — **same** `EncryptionHeader`, `deriveKey`, and password plumbing as **NUNA** `.titanpak` in engine/Nuna (`TitanPakCrypto` / `SWG_TRE_PASSWORD`). The writer may still choose a **custom** password at pack time; it is not a different cipher family. |

**`.titanlst`** references trees by path; referenced trees can be `TREE`, `NUNA`, or `LEGE` — encryption follows each embedded tree file’s magic, not the list extension.

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

### Analyze archive (reverse-engineering / unknown crypto)

When you **do not have** the original decryption source or password, use **`analyze`** to print everything readable **without** secrets:

- Full **TreHeader** fields (magic, version, file count, TOC/name offsets, compressors)
- For **NUNA** archives: **encryption header** (version, flags, **salt** and **IV** as hex — these are stored in the clear)
- **Derived layout** (where the name block starts)
- Optional **TOC decrypt probe**: tries your `-d` password first (if any), then Nuna’s built-in default — confirms whether the archive matches **this** tool’s scheme (`NunaCrypto.h`)

```bash
nuna analyze sample.titanpak
nuna analyze sample.titanpak -d your_guess
```

**Note:** On little-endian machines the SWG **TREE** magic is stored as the byte sequence often misread as **“EERT”** in hex editors — that is normal for unencrypted `.tre` files, not a separate format name.

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
| `-d, --decrypt <password>` | Password for unpack/list or `analyze` TOC probe |
| `-h, --help` | Show help |

## TitanPak Format

TitanPak uses the standard TRE format with automatic encryption:

### Header (36 bytes)
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 4 | Magic ("TREE" for unencrypted, "NUNA" for encrypted) |
| 4 | 4 | Version tag: **0004**, **0005**, or **0006** (same TOC layout). On LE, **0006** appears as the ASCII characters `6000` after `EERT`, so the first 8 bytes can read as **`EERT6000`** — that is still standard **TREE** + **0006**, not a custom magic. |
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
