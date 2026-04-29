# Titan Project Planning

## TRANSIENT MEMORY

### x64 Migration — Current State (Last updated: 2026-02-26)

**Scope**: Full `/src` (server), plus `SwgTitan` and `SwgGodClient` from `/client`. Other client tools deferred.

**Core Constraint**: Preserve 32-bit build system. All x64 changes are additive; x86 paths untouched.

#### Completed Work

**Server (Phase 1) — DONE**
- `src/CMakeLists.txt`: `BUILD_64BIT` option added (default OFF). Controls `-m32` flags and include paths.
- `src/build.xml`: `prepare_src_x64` Ant target added alongside `prepare_src_x86`.
- `src/cmake/linux/FindJNI.cmake`: x64 JVM discovery paths added.
- `FoundationTypesLinux.h`: `uint32`/`int32` changed from `unsigned long`/`signed long` to `unsigned int`/`signed int` (LP64 fix).
- Callstack infrastructure widened: `uint32` → `uintptr_t` in `DebugHelp`, `CallStack`, `CallStackCollector`, `Fatal.cpp`.
- `JavaLibrary.cpp`: All `reinterpret_cast<uint32>(ptr)` → `reinterpret_cast<uintptr_t>(ptr)`.
- `OciQueryImplementation.cpp`: Investigated, no change needed (`sizeof(long)` with `SQLT_INT` is correct on x64).

**Client (Phase 2) — DONE**
- `add_x64_platform.ps1`: Ran on all 157 vcxproj files. Uses `Uri.MakeRelativeUri` (PS 5.1 compatible).
- `cleanup_x64_libs.ps1`: Stripped x86-only libs (STLport, Vivox, Miles, PCRE) from 20 executable x64 configs.
- `swg.sln`: x64 solution configurations added.
- `x64.props`: Shared property sheet with `WIN64`, `_WIN64`, `MachineX64`, output to `compile\x64\`.
- Inline assembly: 9 engine files converted to intrinsics (`_mm_*`, `__rdtsc`, `__debugbreak`, `_controlfp_s`, `_interlockedbittestandset`, `sqrtf`). All guarded with `#ifdef _M_X64` / `#else`.
- Pointer-width: `GetWindowLongPtr`/`SetWindowLongPtr` in `StatusWindow.cpp`, `Direct3d9.cpp`. `uintptr_t`/`intptr_t`/`INT_PTR`/`ULONG_PTR` in `MemoryManager.cpp`, `Os.cpp`, `PathSearch.cpp`.
- STLport compat: `StlForwardDeclaration.h` and `UIStlFwd.h` dual-pathed for x64 (standard lib) vs x86 (STLport). `hash_compat.h` maps `std::hash_map` → `std::unordered_map` on x64. Included from `FirstSharedFoundation.h` PCH.
- Vivox: x86-only libs removed from x64 link deps. DLL-loading wrapper handles absence gracefully. No code changes.

#### Remaining Work — Blockers for x64 Build

**Server**
- Install 64-bit Oracle Instant Client + JDK on game server (`/home/swg/swg-main`).
- Build x64 server, validate 32-bit client interop.

**Client — Third-Party Libraries (BLOCKING)**
These must be rebuilt/replaced before x64 SwgTitan or SwgGodClient can link:
- **Qt 3.3.4**: Rebuild from source for x64. Required by SwgGodClient only.
- **DPVS**: Rebuild from source. Has extensive x86 inline asm in `dpvsX86.cpp`, `dpvsMath.cpp`, `dpvsOcclusionBuffer_*.cpp`, `dpvsBitMath.hpp`, `dpvsFiller.hpp`. Needs intrinsics conversion.
- **PCRE 4.1**: Rebuild from source (currently `.a` static archive, x86 only).
- **libxml2**: Rebuild from source for x64.
- **Mozilla/NSPR**: Rebuild from source for x64.
- **DirectX 9**: Obtain x64 libs from Windows SDK or DirectX SDK.
- **Miles Sound 7.2e**: Replace with OpenAL Soft (open-source, x64-native). Requires adapter in `clientAudio`.
- **Bink Video**: Replace with FFmpeg libavcodec/libavformat. Requires video decoder adapter.

**Client — Lower Priority Code Fixes**
- Sort key functions (`StaticShader::getShaderTemplateSortKey`, `ShaderEffect::getShaderImplementationSortKey`, etc.) return `int` from pointer — truncation on x64 (cosmetic, not crash).
- `PathSearch` marks system stores pointers in `int` array — needs `intptr_t` redesign.
- Remaining `GetWindowLong` calls in deferred tools (Viewer, UiBuilder, TextureBuilder, ShaderBuilder, TerrainEditor, MayaExporter).
- Debug logging format strings in `LeakFinder.h`, `CuiMediator.cpp`, `EditableAnimationState.cpp`, `RenderWorld.cpp` still use `%08X` for pointers.

#### Key Architecture Notes

- **Network protocol**: `Archive` serialization uses fixed 4-byte widths. 32-bit client can talk to 64-bit server without changes.
- **Build toolchain**: Visual Studio 2022 (**v143**) for Windows client. Server uses GCC on Linux.
- **STLport 4.5.3**: x86 builds still use it. x64 builds use MSVC standard library. Compatibility layer in `hash_compat.h` and `StlForwardDeclaration.h`.
- **Conditional compilation**: x64 code guarded by `_M_X64` (MSVC) or `BUILD_64BIT` (CMake). x86 code preserved in `#else` blocks.
- **DPVS inline asm**: ~20 blocks across 6 files. Most complex third-party asm to convert. Includes CPU detection (CPUID), SSE/MMX ops, fixed-point math, bit scan (BSR/BSF), cache filling.
- **ATL/MFC inline asm**: In `atlmfc/` — has COM thunking and message routing asm. VS2013 should have x64 ATL/MFC built-in, so these source files may not be needed.

#### File Inventory

| Category | Files Modified | Key Files |
|----------|---------------|-----------|
| Server build | 3 | `CMakeLists.txt`, `build.xml`, `FindJNI.cmake` |
| Server types | 1 | `FoundationTypesLinux.h` |
| Server debug | 7 | `DebugHelp.h/.cpp` (linux+win32), `CallStack.h/.cpp`, `CallStackCollector.cpp`, `Fatal.cpp` |
| Server script | 1 | `JavaLibrary.cpp` |
| Client build | 157+ | All vcxproj files, `swg.sln`, `x64.props` |
| Client asm | 9 | `SseMath.cpp`, `Transform.cpp`, `FloatingPointUnit.cpp`, `ProfilerTimer.cpp`, `DebugHelp.cpp`, `CollisionUtils.cpp`, `Fatal.cpp`, `Clock.cpp`, `VeCritsec.hpp`, `UITabbedPane.cpp` |
| Client pointers | 5 | `StatusWindow.cpp`, `Direct3d9.cpp`, `MemoryManager.cpp`, `Os.cpp`, `PathSearch.cpp` |
| Client STLport | 4 | `StlForwardDeclaration.h`, `hash_compat.h` (new), `FirstSharedFoundation.h`, `UIStlFwd.h` |
| Scripts | 2 | `add_x64_platform.ps1`, `cleanup_x64_libs.ps1` |
| Documentation | 1 | `X64_MIGRATION.md` |

#### MCP Server Access

- `user-ssh-mcp-swg-gameserver`: Game server at `/home/swg/swg-main` (Linux, server builds)
- `user-ssh-mcp-swg-gameserver-db`: Database server access (Oracle, sqlplus)

#### Build Commands

```bash
# Server x64 (Linux)
ant prepare_src_x64 && ant build_src

# Server x86 (unchanged)
ant prepare_src_x86 && ant build_src
```

```powershell
# Client x64 (Windows) — after third-party libs are ready
msbuild src\build\win32\swg.sln /p:Configuration=Release /p:Platform=x64

# Client x86 (unchanged)
msbuild src\build\win32\swg.sln /p:Configuration=Release /p:Platform=Win32
```

#### Related Transcripts
- [x64 Migration Planning](85e0a28a-238f-4e18-8955-7206f480c946) — Full conversation history for the x64 migration research, planning, and execution.

---


# Recents Changes (master)

### Magic Painting URL Feature — Current State

**Status**: Functional but with known issues from previous session.

**What it does**: Allows spawning any in-game object, setting `CONDITION_MAGIC_PAINTING_URL` condition and a `magic_painting_url.url` objvar, which causes the client to download and render a remote image (PNG, JPEG, GIF) on/above the object.

**Key files**:
- Server: Script command in `player_developer` command table
- Client: `TangibleObject.cpp` handles rendering via `MagicPaintingUrlManager` or inline logic
- Shared: Network messages for objvar sync

**Display modes**: Cube (default), flat upright plane, double-sided upright plane. Controlled by `magic_painting_url.mode` objvar.

**Known state**: Images render successfully. GIF support, texture scrolling, and painting-template detection were being implemented. See `TEXTURE_URL_RESOLVING` documentation and previous transcript for details.

---

### God Client (SwgGodClient) — Known Issues

- **File Control Tree crash**: Crashes when file control server is not online. Crash guard was added but may need further hardening. Crash in `qt-mt334.dll`.
- **Template Editor crash**: Opening Template Editor or right-clicking `.iff` and hitting "edit" causes crash.
- **Spacebar in chat**: Previously fixed — spacebar not working in god client chat input.
- **Large directory loading**: Warning about sending more data than remote can receive. Chunked tree view was being implemented.

---

### Sidequest Fixes Applied (Previous Sessions)

- Suppressed `Sphere has negative radius` warning when object scale changes.
- Suppressed `Object already has a far update volume` warning for magic painting objects.
- Created `DiscordWebhook.java` script class (was missing, causing `NoClassDefFoundError`).
- Added `[Titan]` prefix to `DEBUG_LOG_PRINT` and `REPORT_LOG_PRINT` macros globally (via `Report::printfTitan`).
- Fixed Archive read error crash — null/empty values in baselines handled gracefully.
- God client: Added "Set Condition" context menu for tangible objects with all player-facing conditions.
- God client: Added sub-context for `CONDITION_MAGIC_PAINTING_URL` to set URL.
- Updated `common.cfg` to load all `sys.shared`, `sys.client`, `sys.server` files for tools.
- Created build/deploy scripts for SwgTitan and SwgGodClient.
- Synced `/src` shared files with `/client` shared files.
