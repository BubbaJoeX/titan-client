# x64 Migration - Phase 1 Changes

This document tracks all changes made for the x86-to-x64 migration.
The 32-bit build system is fully preserved; x64 support is additive.

## Server (/src) Changes

### Build System

`**src/CMakeLists.txt**`

- Added `BUILD_64BIT` CMake option (default OFF, preserving x86 behavior)
- When `BUILD_64BIT=ON`: omits `-m32` from CXX flags, uses `/usr/include/x86_64-linux-gnu`
- When `BUILD_64BIT=OFF` (default): identical to previous behavior (`-m32`, i386 includes)

`**src/build.xml**`

- Added `prepare_src_x64` Ant target alongside existing `prepare_src_x86`
- `prepare_src_x64` passes `-DBUILD_64BIT=ON` to CMake and omits `-m32` linker flags
- Existing `prepare_src_x86` and `build_src` targets unchanged

`**src/cmake/linux/FindJNI.cmake**`

- Added `JAVA64_HOME` environment variable support
- Added `/usr/lib64`, `/usr/local/lib64` to library search paths
- Added `amd64` architecture paths for JVM library discovery
- Added `lib/server` subdirectory searches (modern JDK layout)

### Type System Fix

`**src/engine/shared/library/sharedFoundationTypes/src/linux/FoundationTypesLinux.h**`

- Changed `uint32` from `unsigned long` to `unsigned int`
- Changed `int32` from `signed long` to `signed int`
- On Linux x64, `long` is 8 bytes (LP64); `int` is always 4 bytes
- This is a critical correctness fix: without it, `uint32` would be 8 bytes on x64

### Debug/Callstack Infrastructure

`**src/engine/shared/library/sharedDebug/src/linux/DebugHelp.h**`

- `getCallStack()`: signature widened from `uint32*` to `uintptr_t*`
- `lookupAddress()`: signature widened from `uint32` to `uintptr_t`

`**src/engine/shared/library/sharedDebug/src/linux/DebugHelp.cpp**`

- Updated implementations to match new signatures

`**src/engine/shared/library/sharedDebug/src/win32/DebugHelp.h**`

- Same signature changes as Linux

`**src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp**`

- `getCallStack()`: Added `#ifdef _M_X64` block using `RtlCaptureContext()` and
`IMAGE_FILE_MACHINE_AMD64` for x64; preserved original `__asm` block for x86
- `lookupAddress()`: uses `DWORD64` internally (already 64-bit safe)
- `reportCallStack()`: uses `uintptr_t` stack array

`**src/engine/shared/library/sharedDebug/src/shared/CallStack.h**`

- `m_callStack[]` array: `uint32` -> `uintptr_t`

`**src/engine/shared/library/sharedDebug/src/shared/CallStack.cpp**`

- Updated format strings from `%08X` with `static_cast<int>` to `%08llX` with
`static_cast<unsigned long long>` for portable address printing

`**src/engine/shared/library/sharedDebug/src/shared/CallStackCollector.cpp**`

- `m_callStack` pointer: `uint32*` -> `uintptr_t*`
- `addCallStack()` parameter: `uint32*` -> `uintptr_t*`
- Local callstack array: `uint32[]` -> `uintptr_t[]`
- Fixed `memcpy` size (was `sizeof(*newCallStack)`, now `sizeof(uintptr_t) * CALLSTACK_DEPTH`)
- Updated format strings for portable address printing

`**src/engine/shared/library/sharedFoundation/src/shared/Fatal.cpp**`

- Local callstack array: `uint32[]` -> `uintptr_t[]`
- Updated format strings for portable address printing

### Pointer Cast Fixes

`**src/engine/server/library/serverScript/src/shared/JavaLibrary.cpp**`

- All `reinterpret_cast<uint32>(pointer)` changed to `reinterpret_cast<uintptr_t>(pointer)`
- `frameAddressHigh` variable: `uint32` -> `uintptr_t`
- Prevents pointer truncation on x64

## Client (/client) Changes

### Build Infrastructure

`**client/src/build/win32/x64.props**` (NEW)

- Shared MSBuild property sheet for x64 platform configurations
- Sets `**<PlatformToolset>v143</PlatformToolset>**` (Visual Studio 2022; MSVC 14.3) for x64; all `***.vcxproj**` in the client tree use **v143** for **Win32 and x64** (legacy **v120** / VS 2013 has been retired)
- Sets output directories to `compile\x64\` (separate from `compile\win32\`)
- Defines `WIN64` preprocessor symbol
- Sets `TargetMachine` to `MachineX64` for Lib and Link
- Suppresses pointer truncation warnings (4311, 4312, 4302) during migration

`**client/add_x64_platform.ps1`** (NEW)

- PowerShell automation script to add x64 platform to all vcxproj files
- Processes the solution file (adds x64 solution/project configuration mappings)
- For each vcxproj: clones Win32 configurations to x64, removes STLport from
includes, removes `_USE_32BIT_TIME_T`, adds `WIN64` define, imports `x64.props`
- Supports `-DryRun` flag to preview changes without modifying files

### Prototype: sharedRandom

`**client/src/engine/shared/library/sharedRandom/build/win32/sharedRandom.vcxproj**`

- Added Debug|x64, Optimized|x64, Release|x64 platform configurations
- x64 configurations: no STLport, no `_USE_32BIT_TIME_T`, adds `WIN64`
- x64 output goes to `compile\x64\` instead of `compile\win32\`
- Imports `$(SolutionDir)x64.props` for x64 configurations
- All Win32 configurations completely unchanged

## How to Build

### Server x64 (Linux)

```bash
# Install 64-bit dependencies first (JDK, Oracle Instant Client, etc.)
export JAVA64_HOME=/opt/java
export ORACLE_HOME=/opt/oracle/instantclient_18_3

# Using Ant
ant prepare_src_x64
ant build_src

# Or using CMake directly
mkdir build64 && cd build64
cmake -DBUILD_64BIT=ON -DCMAKE_BUILD_TYPE=Release ../src
make -j$(nproc)
```

### Server x86 (unchanged)

```bash
ant prepare_src_x86
ant build_src
```

### Client x64 (Windows)

```powershell
# Step 1: Run the automation script to add x64 to all projects
cd D:\titan\client
.\add_x64_platform.ps1

# Step 2: OpenAL Soft (one-time; see "OpenAL Soft x64" under Phase 3)
#  Place OpenAL32.lib + OpenAL32.dll under:
#    client\src\external\3rd\library\openal-soft\lib\x64\Debug   ( /MTd — Debug|x64, Optimized|x64 )
#    client\src\external\3rd\library\openal-soft\lib\x64\Release  ( /MT  — Release|x64 )

# Step 3: Build (see checklist below — full solution may fail on Qt 3.3 tool projects)
# MSBuild path depends on VS edition; e.g. BuildTools:
#   C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe
$msb = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
# Full solution (expect Qt 3.3 / header errors on some projects; see Phase 3):
& $msb "D:\titan\client\src\build\win32\swg.sln" /p:Configuration=Release /p:Platform=x64 /m
# Or build SwgTitan only *after* dependencies exist under client\src\compile\x64\...:
& $msb "D:\titan\client\src\game\client\application\SwgClient\build\win32\SwgClient.vcxproj" /p:Configuration=Release /p:Platform=x64

# After a successful link, stage a full x64 game folder (exe + DLLs + dpvs + OpenAL + sndfile):
#   cd D:\titan\client
#   .\bundle_win64_release.ps1 -Verbose
# Output: <titan root>\exe\win64_rel\
```

### Client x86 (unchanged)

```powershell
msbuild src\build\win32\swg.sln /p:Configuration=Release /p:Platform=Win32
```

### SwgTitan x64 — compile checklist (practical)

1. **MSBuild** — Use **Visual Studio 2022** (or **Build Tools 2022**) with the **v143** toolset and a compatible **Windows 10+ SDK** (e.g. 10.0.22000+). Example: `...\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe` (path varies by edition).
2. **Static link outputs** — Most game `.lib` files are **not** checked in. They are produced under
  `client\src\compile\x64\<ProjectName>\<Configuration>\`.  
   Building `**SwgClient.vcxproj` alone** fails with `LNK1181 cannot open input file 'archive.lib'`, `blat.lib`, etc. until those projects have been built. Prefer **`msbuild swg.sln /p:...`** so dependency order is honored, *or* build the dependency projects first.
3. **OpenAL Soft** — Vendored binaries per configuration (see table in Phase 3). `**SwgClient` PostBuild** copies the matching `OpenAL32.dll` to `$(OutDir)` next to `SwgTitan_*.exe`. Regenerate: rebuild OpenAL in `openal-soft\build_x64_`* and recopy `OpenAL32.lib` / `OpenAL32.dll` into `lib\x64\Debug` or `lib\x64\Release`.
4. **OpenAL Miles shim** — `OpenALMssShim.cpp` must use the **same** `OalVoice` / `OalDig` / `OalStream` types as the public typedefs in `OpenALMssShim.h` (define them in **global** scope, not an anonymous namespace). The tree still uses `**__declspec(thread)`** for those TLS objects (works on **v143**; you may switch to `**thread_local`** if you standardize on C++11+ everywhere for the shim). The header supplies Miles-style width typedefs: `**S8` / `U8` / `S16` / `U16**` (and `S32`, `U32`, `F32`).
5. **Full solution and Qt 3.3** — A **Release|x64** build of the **entire** `swg.sln` can fail early on **Qt 3.3.4**–based projects (e.g. `SwgBoxingQt`, `SwgContentSync`) with **template/parse errors** in `qvaluelist.h` / `qtl.h` under a modern compiler. That is **separate** from the SwgTitan link line. Workarounds: exclude those projects from the active solution configuration, or port/stub them; meanwhile build the **SwgClient** dependency chain or the subset of projects you need.
6. **Remaining vendor link** (after in-tree static libs are built) — Still typically required for a full **SwgTitan** link: **PCRE** (PCRE1-style `pcre_*.lib` x64), **libxml2** x64, **Bink** x64, **ATI_Compress** (or alternative), **Mozilla/NSPR/xul** where referenced — match **CRT** (`/MT` / `/MTd`) to the project configuration. See Phase 3 table.
7. **MSB8012** — You may see a warning that `TargetPath` (`SwgTitan.exe`) does not match `Link.OutputFile` (`SwgTitan_r.exe`). The executable name is the `_r` / `_d` / `_o` form; the warning is cosmetic unless you care about the default target name in the IDE.
8. **SwgTitan `Release|x64` link order (empirical, 2026)** — On a tree where in-engine static `.lib` files under `client\src\compile\x64\...` are already built, `msbuild ... SwgClient.vcxproj /t:Rebuild` failed with: `**LNK1181: cannot open input file 'libsndfile-1.lib'**` (the import library for **libsndfile** is not in any `AdditionalLibraryDirectories` and is not vendored in-repo). Add an **x64** `libsndfile-1.lib` (and ship `libsndfile-1.dll` if linking dynamically) to a new lib directory and extend **Release|x64** library paths, or drop/conditionalize the dependency if the feature is stubbed. **After** `libsndfile-1.lib` resolves, the next **non–Windows-SDK** entries in the same link line are (among others) `**vivoxSharedWrapper_Release.lib**`, PCRE / libxml2 static `**.lib**`, `**libMozilla.lib**` — each must be a real **x64** archive matching `**/MT**`, or the link will fail with `LNK1181` / `LNK1112` / unresolved externals. (PCRE and libxml2 are wired to **v143**-built static libs; see the Phase 3 PCRE / libxml2 rows.) System entries such as `ws2_32.lib` / `winmm.lib` / `mswsock.lib` come from the Windows SDK.

## Phase 2: Client x64 Changes (Completed)

### Build System Automation

`**client/add_x64_platform.ps1**` (updated)

- Fixed `GetRelativePath` compatibility for Windows PowerShell 5.1 (uses `Uri.MakeRelativeUri`)
- Adds Debug|x64, Optimized|x64, Release|x64 to all 157 vcxproj files
- Imports `x64.props` shared property sheet for all x64 configurations
- Removes STLport from include directories in x64 ItemDefinitionGroups
- Removes `_USE_32BIT_TIME_T`, adds `WIN64` preprocessor define
- Updates output/intermediate directories from `win32` to `x64`
- Sets `TargetMachine` to `MachineX64` for Lib and Link

`**client/cleanup_x64_libs.ps1**` (new)

- Removes x86-only static libraries from x64 link dependencies:
  - STLport: `stlport_vc6_static.lib`, etc.
  - Vivox: `vivoxSharedWrapper_release.lib`, `vivoxplatform.lib`, `vivoxsdk.lib`
  - Miles Sound: `mss32.lib`
  - PCRE: `libpcre.a`

`**client/src/build/win32/swg.sln**` (updated)

- Added x64 solution configuration platforms (Debug|x64, Optimized|x64, Release|x64)
- Added per-project x64 configuration mappings

`**client/src/build/win32/x64.props**` (updated)

- Added `_WIN64` preprocessor define alongside `WIN64`

### Inline Assembly Conversion

All engine inline assembly converted to intrinsics/builtins with `#ifdef _M_X64` guards:

`**client/src/engine/shared/library/sharedFoundation/src/win32/FloatingPointUnit.cpp**`

- `__asm fnstcw` / `__asm fldcw` → `_controlfp_s()` on x64

`**client/src/engine/shared/library/sharedMath/src/win32/SseMath.cpp**` (rewritten)

- All 4 functions rewritten using `_mm_*` SSE intrinsics
- `canDoSseMath()` uses `__cpuid()` intrinsic on x86, returns `true` on x64
- `rotateTranslateScale_l2p`, `rotateScale_l2p`, `skinPositionNormal_l2p`, `skinPositionNormalAdd_l2p` all use `_mm_load_ps`, `_mm_mul_ps`, `_mm_set1_ps`, `_mm_store_ps`

`**client/src/engine/shared/library/sharedMath/src/shared/Transform.cpp**`

- `sse_xf_matrix_3x4()` naked function rewritten using SSE intrinsics
- No longer uses `__declspec(naked)` or `push`/`pop` register management

`**client/src/engine/shared/library/sharedCollision/src/shared/core/CollisionUtils.cpp**`

- `__asm fld/fsqrt/fstp` → `sqrtf()` standard library call

`**client/src/engine/shared/library/sharedDebug/src/win32/ProfilerTimer.cpp**`

- `__declspec(naked) readTimeStampCounter()` → `__rdtsc()` intrinsic

`**client/src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp**`

- `getCallStack()`: x64 uses `RtlCaptureContext()` + `IMAGE_FILE_MACHINE_AMD64` + `StackWalk64`
- x86 path preserved with original `__asm` block

`**client/src/engine/shared/library/sharedFoundation/src/shared/Fatal.cpp**`

- `__asm int 3` → `__debugbreak()`

`**client/src/engine/shared/library/sharedFoundation/src/shared/Clock.cpp**`

- `__asm int 3` → `__debugbreak()`
- `timezone` (POSIX) → on `**PLATFORM_WIN32**`, `errno_t _get_timezone(long *)`; elsewhere unchanged `localtime` + `timezone` global

`**client/src/external/3rd/library/ui/src/win32/UITabbedPane.cpp**`

- `_asm nop` → `__noop`

`**client/src/engine/client/library/clientGame/src/shared/HTTPpost/VeCritsec.hpp**`

- `__asm lock bts` → `_interlockedbittestandset()` intrinsic

### Pointer-Width Fixes

`**client/src/engine/shared/library/sharedStatusWindow/src/win32/StatusWindow.cpp**`

- `GetWindowLong(hwnd, GWL_USERDATA)` → `GetWindowLongPtr(hwnd, GWLP_USERDATA)`
- `SetWindowLong(..., reinterpret_cast<LONG>(this))` → `SetWindowLongPtr(..., reinterpret_cast<LONG_PTR>(this))`

`**client/src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp**`

- `SetWindowLong(ms_window, GWL_STYLE, ...)` → `SetWindowLongPtr(ms_window, GWL_STYLE, static_cast<LONG_PTR>(...))`

`**client/src/engine/shared/library/sharedMemoryManager/src/shared/MemoryManager.cpp**`

- `reinterpret_cast<int>(next) - reinterpret_cast<int>(this)` → `reinterpret_cast<intptr_t>` for pointer arithmetic
- `int const memory = reinterpret_cast<int>(...)` → `uintptr_t const memory = reinterpret_cast<uintptr_t>(...)`
- Format strings updated: `%08X` → `%08llX` with `static_cast<unsigned long long>`

`**client/src/engine/shared/library/sharedFoundation/src/win32/Os.cpp**`

- `reinterpret_cast<int>(ShellExecute(...))` → `reinterpret_cast<INT_PTR>(...)`
- `RaiseException(..., reinterpret_cast<DWORD *>(&info))` → `reinterpret_cast<ULONG_PTR const *>(&info)`

`**client/src/engine/shared/library/sharedPathfinding/src/shared/PathSearch.cpp**`

- `(int)((void*)searchNode)` → `static_cast<int>(reinterpret_cast<intptr_t>(searchNode))`

### STLport Compatibility Layer

`**client/src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h**` (updated)

- x64 path: includes standard headers directly, maps `stdhash_map` → `std::unordered_map`, `stdhash_set` → `std::unordered_set`
- x86 path: preserved original STLport forward declarations

`**client/src/engine/shared/library/sharedFoundation/src/shared/hash_compat.h**` (new)

- Provides `std::hash_map` and `std::hash_set` as thin wrappers around `std::unordered_map`/`std::unordered_set`
- Only active on x64 (`#ifdef _M_X64`)
- Included from `FirstSharedFoundation.h` PCH for x64 builds

`**client/src/engine/shared/library/sharedFoundation/src/shared/FirstSharedFoundation.h**` (updated)

- Conditionally includes `hash_compat.h` for x64 builds

`**client/src/external/3rd/library/ui/src/win32/UIStlFwd.h**` (updated)

- x64 path: includes standard headers, maps `ui_stdhash_map` → `std::unordered_map`
- x86 path: preserved original STLport `stl/_config.h` include and forward declarations

### Vivox / proximity voice (deprecated)

- **Product:** Voice chat is **disabled**; `SWG_NO_VIVOX` is defined when building `vivoxSharedWrapper_x64.vcxproj`. `Vivox.cpp` compiles a **stub** that never loads `vivoxsdk.dll` (`sLoadVivoxDLL` always fails; `ProcessEvents` is a no-op when the DLL handle is null).
- **Link:** SwgClient **drops** `vivoxplatform.lib` / `vivoxsdk.lib` and vendor `vivox\lib` search paths; the game still links `**vivoxSharedWrapper_*.lib**` so `SwgVivox` template symbols resolve, but no vendor voice binaries are required at runtime.
- **Win32:** Older configs may still list legacy Vivox import libs; trim them when touching those lines (same stub applies if `SWG_NO_VIVOX` is added to a Win32 wrapper build).

### Direct3D 9 / D3DX (x64)

- Legacy D3DX link names (`d3dx.lib`, `d3dx9.lib`, …) are not used for x64; **PreLink** builds `directx9_x64_stubs.vcxproj` → `d3dx9_x64_stubs.lib` (minimal stub implementations for symbols the codebase still references).
- `dsetup.lib` dropped for x64 SwgClient where not available from installed SDKs.

### SOE VideoCapture stack (deprecated on x64)

- **x64 dev builds:** `VideoCapture::` / `SingleUse::` are **no-ops** in `SwgVideoCapture.h`; `SwgVideoCapture.cpp` compiles the legacy implementation only for `!_WIN64`.
- **x64 executables:** SOE videocapture **libs and lib paths removed** from SwgClient, Viewer, TextureBuilder, TerrainEditor (x64 configs only). Win32 still links the old stack.
- **clientGraphics:** `videocapture` include path removed from x64 compile configs (headers only needed for the x86 implementation).

---

## Phase 3: SwgTitan x64 link (current status)

**As of this update:** engine libraries through **Tier 4** (e.g. `clientGraphics`) build for **Debug|x64**. `**clientAudio` Release|x64** builds with **OpenAL Soft** when the shim and vendor libs are in place. A **narrow** `msbuild` of `**SwgClient.vcxproj**` **Release|x64** can **link** and produce `**SwgTitan_r.exe**` when all in-tree static `.lib` outputs exist under `client\src\compile\x64\...` and **x64** vendor import libs (PCRE, libxml2, OpenAL, libsndfile, etc.) match the project **CRT** (`/MT` / `/MTd`). The **full** `swg.sln` **Release|x64** run may still **fail** on **Qt 3.3** / **MFC** tool projects; use a subset build or exclude those from the active configuration. Remaining **optional** link/runtime gaps include **Bink**, **ATI_Compress**, **Mozilla** stacks where still referenced, per the table below.

### SwgTitan Debug|x64 — resolved vs open


| Area                                      | Status                                    | Notes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| ----------------------------------------- | ----------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| STLport                                   | Done                                      | Bypassed for x64; `hash_compat` / standard library                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| D3DX / old DX SDK libs                    | Done                                      | `d3dx9_x64_stubs.lib` + system `d3d9.lib`, etc.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| Vivox wrapper                             | Done                                      | Stub SDK (`SWG_NO_VIVOX`); no `vivoxsdk.dll`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| SOE VideoCapture                          | Done                                      | Stub API + drop vendor libs on x64                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Miles / OpenAL (`Mss32.lib` → OpenAL)** | **Done (x64)**                            | **Win32** still uses Miles (`mss.h`, `Mss32.lib`). **x64 `clientAudio**` defines `SWG_USE_OPENAL` and compiles `OpenALMssShim.cpp` instead of linking Miles: a compatibility layer implements the `AIL_*` surface used by `Audio.cpp` / `SoundObject3d.cpp` on **OpenAL Soft**. **SwgClient** x64 links `**OpenAL32.lib**` and searches `**openal-soft/lib/x64/Debug**` (Debug and Optimized x64, `/MTd`) and `**openal-soft/lib/x64/Release**` (Release x64, `/MT`). Vendor **Khronos headers** live under `openal-soft/include/AL/`. **OpenAL Soft** is built with CMake into e.g. `openal-soft/build_x64_Debug/Debug` and `build_x64_Release/Release`; copy `**OpenAL32.lib**` + `**OpenAL32.dll**` into the `lib\x64\...` folders (and keep **CRT** matching the game). `**SwgClient` PostBuild** copies the right `OpenAL32.dll` to `$(OutDir)`. **CMake upstream** clone: `openal-soft/upstream/`; build trees `build_x64_*` are local-only (good candidates for `.gitignore`). **Limitations (shim):** PCM WAV **8- or 16-bit mono/stereo** only (no IMA ADPCM / MPEG in this path); EAX-style `**AIL_set_room_type` / reverb** are stored for queries but **not** mapped to EFX; occlusion/obstruction approximate **gain** only; coordinate system matches Miles call sites (verify 3D parity in-game). |
| **PCRE (`pcre_compile`, …)**              | **Wired (`.lib` built in-tree, VS 2022)** | **PCRE 8.45** (PCRE1): CMake with `PCRE_STATIC` + **CRT** matching the game (`/MT` / `/MTd`). **SwgClient** x64 Debug and Optimized link `pcred.lib` + `pcreposixd.lib`; Release x64 links `pcre.lib` + `pcreposix.lib` from `client/src/external/3rd/library/pcre/lib/x64/` (Debug or Release subfolders). **sharedRegex** includes from `pcre/include` (headers also under `pcre/include/pcre/` for `#include "pcre/pcre.h"`). **Win32** still uses `pcre/4.1/win32/lib` for *link* until those configs are pointed at the same static libs.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **libxml2 (`xmlNewDoc`, …)**              | **Wired (`.lib` built in-tree, VS 2022)** | **libxml2 2.12.x** static: CMake `BUILD_SHARED_LIBS=OFF`, optional modules off to match legacy; `libxml2s.lib` / `libxml2sd.lib` under `client/src/external/3rd/library/libxml2/lib/x64/` (Release vs Debug). **sharedXml** x64 includes: `libxml2/include`. **SwgClient** x64 links the static import lib names above (replaces `libxml2-win32-*.lib`). **[txwizard/libxml2_x64_and_ARM](https://github.com/txwizard/libxml2_x64_and_ARM)** is useful as a *reference* (VC projects, multi-arch); this tree uses **static `/MT**` vendored builds, not that repo’s `/MD` link line as-is.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| **Logitech LCD (`lgLcd*.lib`)**           | **Done (removed)**                        | `lcdui.lib` / `lgLcd.lib` and lcdui lib paths dropped from SwgClient; G15 path compiled out (`USE_LCD` undefined in `SwgCuiG15Lcd.cpp`)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| DPVS                                      | **Wired to client deploy**                | x64 `dpvs.dll` is linked by name (not `*_r.dll`). `**SwgClient` Release|x64 PostBuild** copies `compile\x64\dpvs\<Config>\dpvs.dll` to `**$(OutDir)**` and `**exe\win64_rel\`** alongside `**bundle_win64_release.ps1`**. If `dpvs.dll` is missing next to the x64 exe, the loader may resolve a **wrong-arch** copy from `PATH` → **`0xC000007B`**.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Qt 3.3 / 4.1                              | Open                                      | Large; affects tools and embedded UI paths                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Bink                                      | Open                                      | `binkw32.lib` name implies 32-bit; RAD provides per-arch builds under license                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| ATI_Compress / legacy texture tools       | Open                                      | Vendor binary or replace with Compressonator / GPU path                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| Mozilla / NSPR / xul                      | Watch                                     | Verify all linked archives are x64 (LNK1112)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |


**Next practical order for link closure:** (1) ~~**Miles**~~ **OpenAL Soft x64** — done as above; (2) ~~**PCRE** + **libxml2~~** — CMake (VS 2022, **/MT** / **/MTd**): outputs under `pcre/build_x64_*/(Release|Debug)/` and `libxml2/build_x64_*/...`, copied to `pcre/lib/x64/...` and `libxml2/lib/x64/...` (re-run CMake `install` is optional; headers including `libxml2/include/libxml/xmlversion.h` are refreshed from the build tree as needed). **SwgGodClient** x64 `Debug` / `Optimized` / `Release` link lines updated the same way as **SwgClient**; (3) **Bink** / **ATI_Compress** / **Mozilla** as needed; (4) **Qt 3.3** tool projects: exclude from solution or fix headers if full **swg.sln** x64 is required; (5) run **shared** / engine **/m** build so `compile\x64\*.lib` exists before `SwgClient` / **SwgGodClient** link.

### x64 client — deployment, first-run, and startup (progress)


| Topic                                 | What changed / how to use                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| ------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Deploy folder**                     | `**exe\win64_rel\`** is the x64 layout (parallel to **`exe\win32_rel\`** for 32-bit). **Do not** run the x64 exe from a folder full of **win32** `*_r.dll` or you may get `**0xC000007B`** (invalid image format — usually **x64 exe + 32-bit DLL** in the app directory or a bad `PATH` match).                                                                                                                                                                                                                                                                                                                                                                                 |
| **Bundle script**                     | `**client\bundle_win64_release.ps1`** — stages `**SwgTitan_r.exe**`, all `compile\x64\**\<Configuration>\*_r.dll`, `**dpvs\<Configuration>\dpvs.dll**`, x64 **OpenAL32.dll** and **sndfile.dll**, optional **SwgGodClient_r.exe**. Params: `**-TitanRoot**`, `**-Configuration**` (Release / Debug / Optimized), `**-IncludeGodClient**`. See script header for examples.                                                                                                                                                                                                                                                                                                        |
| `**dpvs.dll` in PostBuild**           | The recursive copy `*_r.dll` does **not** include `**dpvs.dll**`. **SwgClient** **Release|x64** PostBuild explicitly copies `**compile\x64\dpvs\$(Configuration)\dpvs.dll**` to the exe output and `**win64_rel**`. (Paths in the vcxproj may be rooted at `**D:\titan\**`; override staging with the bundle script on other drive letters.)                                                                                                                                                                                                                                                                                                                                     |
| **Registry (standard user)**          | `**RegistryKey::install**` used `**AF_READ` only** for `**HKEY_LOCAL_MACHINE\Software\Sony Online Entertainment\Default**`, which fails with `**ERROR_ACCESS_DENIED` (5)** on a normal (non-elevated) first run, and **KEY_READ** alone is insufficient to **create** a new subkey. **Fix:** create HKLM with `**KEY_READ                                                                                                                                                                                                                                                                                                                                                        |
| **Clock / startup stall**             | `**D_game` defaults** set `**clockUsesRecalibrationThread = true`**, which spawns a **Clock** worker during `**SetupSharedFoundation::install`** (before the main window). If startup appears to **hang** after **MemoryBlockManager** logging, **SwgClient** sets `**data.clockUsesRecalibrationThread = false`** so the recalibration thread is off for the game client.                                                                                                                                                                                                                                                                                                       |
| **Startup tracing**                   | **Unconditional** lines prefixed `**[Titan] startup:`** are emitted with `**OutputDebugStringA**` from `**MemoryBlockManager.cpp**`, `**PersistentCrcString.cpp**`, and `**SetupSharedFoundation.cpp**` (visible in **DebugView** / Visual Studio **Output**). Optional `**[SharedFoundation] logStartupMilestones=1**` uses **ODS only** (not `REPORT_LOG`, which could block or re-enter during early init). **Stall after `RegistryKey`:** the next work is optional milestones, then `**PersistentCrcString::install**` (five `**MemoryBlockManager**` allocations) → `**CrcLowerString::install**` → `**WatchedByList::install**`, then `**Os::install` / `CreateWindow**`. |
| **PDB / link**                        | `**LNK1285` corrupt PDB** — delete the stale `**…_r.pdb**` in the project output folder and **rebuild** the affected project.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| **AutoDelta warnings**                | `**C4458` (`owner` hides class member`)** in **`AutoDeltaSet`/`AutoDeltaMap`/`AutoDeltaVector`/`AutoDeltaPackedMap`**: callback parameters renamed (e.g. to **`objectPtr`**) to avoid shadowing **`AutoDeltaVariableBase::owner`**.                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| **Resources (no MFC in Build Tools)** | `**SwgClient.rc`**: `**afxres.h**` → `**winres.h**` / Win32 `TEXTINCLUDE` so the client resources build with **Build Tools** without the **MFC** optional component.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |


### `swg.sln` **Releasex64** — build log reference (2026-04-21)

A full parallel build of `client\src\build\win32\swg.sln` with `**/p:Configuration=Release` `/p:Platform=x64` `/m`** was captured to `**client\msbuild_release_x64.log**`. The run **fails**; it is still a useful “what breaks first” map for `**SwgTitan_r.exe`**, which is *not* blocked by the whole solution being green.


| Category                      | Symptom                                                          | Example projects (non-exhaustive)                                                                                                                                 | Mitigation for `SwgTitan_r.exe`                                                                                                                                                                                            |
| ----------------------------- | ---------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **MFC not installed**         | `C1083` `afxwin.h`, or `**MSB8041*`*: MFC required               | `TreeFileRspBuilder`, `DataLintRspBuilder`, `SwgClientSetup`, `SwgDraftSchematicEditor`, …                                                                        | Install **MFC for v143 (x64 + x86)** from Visual Studio Installer (**Individual components**), or use **full VS** with the **Desktop development with C++** workload + MFC, or do **not** build those tool/setup projects. |
| **Qt 3.3 + modern MSVC**      | `C2061` / `C2447` / `C2805` in `qtl.h`, `qvaluelist.h`, `qmap.h` | `SwgContentSync`, `SwgBoxingQt`, (depends) `swgClientQtWidgets`                                                                                                   | **Unload** or **exclude** from the solution configuration, or port tools to Qt 5/6/ImGui; the game **SwgClient** path may not need these if the dependency graph allows.                                                   |
| **POSIX time API on Windows** | `C2065` `'timezone': undeclared identifier` in `Clock.cpp`       | `sharedFoundation`                                                                                                                                                | In this tree, `**Clock.cpp`** uses `**_get_timezone( long* )**` when `**PLATFORM_WIN32**` is set; the POSIX `timezone` path remains for other platforms.                                                                   |
| **Other tool / 3rd-party**    | Various `C` / `LNK` per project                                  | `Direct3d9*`, `BugTool`, `CrashReporter`, `clientUserInterface`, `dpvs`, `libMozilla`, `SwgFileControl`, `LocalizationTool`, `Turf`, `CommonAPI`, `ui`, `Base`, … | Build **only** the dependency chain to `**SwgClient.vcxproj`**, or fix projects as needed.                                                                                                                                 |


**Practical path to `SwgTitan_r.exe`:** (1) **Tooling** — v143 + **Windows 10+ SDK**; add **MFC** if you build MFC projects. (2) **Narrow the graph** — prefer `**msbuild` `SwgClient.vcxproj`** (and **ProjectReferences** / known deps) or a **custom solution** with Qt/MFC tools removed, instead of a **full** `swg.sln` **Releasex64** until those stacks are fixed. (3) **Libraries** — ensure everything under `client\src\compile\x64\...` and vendor `**AdditionalLibraryDirectories`** is **x64** and **CRT-consistent** (`/MT` for Release) per the checklist above. (4) **Output** — `**Link.OutputFile`** for **Releasex64** is the `**…_r.exe`** name (e.g. `SwgTitan_r.exe`); confirm `**OutDir**` under `client\src\compile\x64\SwgClient\Release\` (or as overridden in the project).

### Dear ImGui (Qt replacement path)

- **Upstream:** [Dear ImGui](https://github.com/ocornut/imgui) `**master`** is vendored under `client/src/external/3rd/library/imgui/` (MIT).
- **Build:** `client/src/external/3rd/library/imgui/build/win32/imgui.vcxproj` produces a static lib (`imgui_d` / `imgui_o` / `imgui`) with **Win32 + `imgui_impl_win32` + `imgui_impl_dx9`** sources; output under `client/src/compile/{win32,x64}/imgui/<Config>/`. Add this project to the solution and link it from tools or the game when you start porting UI.
- **Scope:** Replacing Qt for the full client is **not** done here; ImGui is the supported direction for new **debug/tooling** UI and incremental migration.

### Server (unchanged roadmap)

- Install 64-bit Oracle Instant Client and JDK on staging environment
- Build and test x64 server
- Validate 32-bit client to 64-bit server network interop

### Client — code hygiene (lower priority)

- Sort keys / IDs still derived from pointers as `int` (ordering quirks on x64)
- PathSearch and similar stores that still use `int` for pointer-sized values
- Residual `%08X` logging for pointers; stray `GetWindowLong` in some tools

### Client — testing (after link succeeds)

- Run x64 SwgTitan end-to-end
- Test against x64 server
- Verify rendering, audio, networking, scripting

---

## Alternatives research (vendor gaps)

This section summarizes **realistic options** for each open dependency—not full designs.

### Audio: Miles (Win32) vs OpenAL Soft (x64)

- **Win32 (unchanged):** Miles Sound System — `FirstClientAudio.h` includes `mss.h`; link `**Mss32.lib`** and ship vendor `**mss32.dll**` / redist as before.
- **x64 (current tree):** `**SWG_USE_OPENAL`** is set only on **x64** configurations of `**clientAudio`**. `FirstClientAudio.h` includes `**OpenALMssShim.h**` instead of `mss.h`. Implementation: `**clientAudio/src/win32/OpenALMssShim.cpp**` (OpenAL device/context, buffer upload, 3D listener/sources, loop/EOS polling via `AIL_serve`). **Streaming** loads the whole stream file through the existing Miles file callbacks into memory and decodes as WAV (same PCM limits as samples). **SwgClient** x64 replaces `**Mss32.lib`** with `**OpenAL32.lib**` and adds `**openal-soft/lib/x64/Debug**` or `**.../Release**` to library directories by configuration. Shim types `OalVoice` / `OalDig` / `OalStream` must be defined in **global** scope in the `.cpp` to match the header typedefs; the implementation currently uses `**__declspec(thread)`** for TLS (optional migration to `**thread_local**` on **v143**).
- **Vendor build:** Clone or download **[OpenAL Soft](https://github.com/kcat/openal-soft)**, configure CMake for **x64** static CRT to match the client (`/MT` for Release, `/MTd` for Debug/Optimized), build the `**OpenAL`** target, copy `**OpenAL32.lib**` and `**OpenAL32.dll**` into `openal-soft/lib/x64/Release` and/or `.../lib/x64/Debug` (see `build_x64_Release/Release` vs `build_x64_Debug/Debug` outputs). Deploy the DLL next to the exe (SwgClient PostBuild does this) or on `PATH`. Headers in-repo live under `openal-soft/include/AL/`.
- **Optional / legacy:** A licensed **Miles x64** SDK could still replace the shim later by dropping `SWG_USE_OPENAL` and restoring `Mss64` link lines; the Win32 path remains the reference Miles integration.
- **FFmpeg:** Useful for **decode** expansion (e.g. ADPCM/MP3) inside the shim or a future rewrite; not required for the baseline PCM WAV path.

### Video: Bink

- **RAD Bink:** Same pattern as Miles—prefer **x64 SDK + `binkw64.dll`** if licensed.
- **FFmpeg / libav:** Can decode **BIK** in many builds; replace `BinkVideo` usage with a small adapter (legal/build implications: FFmpeg LGPL/GPL depending on configuration).

### Regex: PCRE

- **Build PCRE 8.x for x64** with MSVC (`/MT` or `/MD` matching the game) and point x64 configs at the new `.lib`.
- **vcpkg:** `pcre` / `pcre2` packages produce x64 triplet libs (watch **PCRE1 vs PCRE2** API; SWG code appears to use **PCRE1**-style `pcre_compile` / `pcre_exec`).
- **Long-term:** Migrate call sites to `**std::regex`** only where semantics and performance are acceptable (not always 1:1 with PCRE).

### XML: libxml2

- **Upstream:** **[GNOME libxml2](https://github.com/GNOME/libxml2)** builds with CMake (`BUILD_SHARED_LIBS`, `LIBXML2_WITH_ZLIB`, etc.). Produce x64 `libxml2.lib` (or the chosen import lib name) and point x64 **sharedXml** / consumer projects at a consistent **include** tree plus **AdditionalLibraryDirectories**.
- **Official or vcpkg x64** `libxml2.lib` (and consistent `LIBXML_STATIC` / DLL defines) wired into x64 **AdditionalLibraryDirectories** / dependency names.
- Avoid continuing to link a file literally named `libxml2-win32-release.lib` for x64 unless that artifact is rebuilt as 64-bit.

### TinyXML (in-tree)

- **MSBuild:** `client/src/external/3rd/library/tinyxml/tinyxml.vcxproj` and `tinyxml_stl.vcxproj` are in `swg.sln` next to zlib. Outputs match legacy link names (`tinyxmld.lib`, `tinyxmld_STL.lib`) under `client/src/compile/<win32|x64>/tinyxml/<Configuration>/` and `.../tinyxml_stl/...`.
- **SwgGodClient** library paths use those `compile\` locations for Debug and Release (Win32 and x64).
- Build example: `msbuild tinyxml.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:SolutionDir=<path\to\client\src\build\win32\>` (same pattern as zlib when importing `x64.props`).

### libjpeg (prebuilt / CMake)

- The tree ships **headers** under `external/3rd/library/libjpeg/include`; **objects are not vendored**. For x64, **SwgGodClient** expects `libjpeg.lib` in `external/3rd/library/libjpeg/lib/x64/` (Win32 uses `lib\`).
- Build **libjpeg-turbo** or IJG libjpeg with CMake for **x64** `/MT` or `/MTd` to match consumers, then copy the static import library as `libjpeg.lib` into `lib/x64/` (and refresh headers if needed).

### Logitech G15 / LCD (`lcdui`) — deprecated

- **Current tree:** SwgClient no longer links `**lcdui.lib`** / `**lgLcd.lib**`; `SwgCuiG15Lcd.cpp` leaves `**USE_LCD` undefined** so all `#ifdef USE_LCD` bodies are excluded. Source under `lcdui_src/` is retained only as reference.
- **If resurrecting hardware LCD:** restore link lines and define `USE_LCD` again (not recommended for new work).

### Qt 3 / 4 (tools and some client UI)

- **Rebuild Qt 3.3.x for x64:** Possible but painful (old toolchain expectations).
- **Port tools to Qt 5/6** or **Dear ImGui** for internal/editor tools; keep game client on existing UI until a larger UI migration is scoped.
- **Dear ImGui** (vendored, see above) is the preferred direction for **debug/inspector** surfaces and gradual replacement of Qt in tools; replacing the full in-game Qt-driven UI remains a large separate effort.

### Texture compression: ATI_Compress / legacy

- **AMD Compressonator** (open-source) or **DirectCompute** / BC encoders for offline or runtime paths.
- Keep legacy **ATI_Compress** only if an x64 binary is obtained.

### Summary table


| Component         | Lowest friction                              | Heavier but open-source                          |
| ----------------- | -------------------------------------------- | ------------------------------------------------ |
| Miles / x64 audio | OpenAL Soft + `OpenALMssShim` (done for x64) | Miles x64 SDK + `mss64.dll` if reverting to RAD  |
| Bink              | x64 Bink SDK                                 | FFmpeg decoder adapter                           |
| PCRE              | Build PCRE1 x64 or vcpkg                     | Gradual `std::regex` where safe                  |
| libxml2           | vcpkg / official x64 build                   | (Rare) tinyxml2 only if schema fits              |
| lgLcd             | (deprecated; removed from SwgClient link)    | —                                                |
| Qt (tools)        | —                                            | Qt 5/6 or Dear ImGui (`library/imgui`) for tools |
| D3DX              | Done (stubs)                                 | Migrate call sites to DirectXMath / shaders      |


