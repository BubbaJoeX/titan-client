# Client compile guide — SwgTitan and SwgGodClient

This document describes prerequisites, solution layout, and build steps for the two primary Windows client applications on **Win32** (32-bit x86). Paths are written relative to the repository root unless noted.

---

## What you are building

| Ship name (solution) | Project file | Binary output (examples) |
|----------------------|--------------|---------------------------|
| **SwgTitan** | `client/src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` | `SwgTitan_d.exe`, `SwgTitan_o.exe`, `SwgTitan_r.exe` |
| **SwgGodClient** | `client/src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj` | `SwgGodClient_d.exe`, `SwgGodClient_o.exe`, `SwgGodClient_r.exe` |

The game exe project keeps the internal name **`SwgTitan`** via `<ProjectName>` while still living in **`SwgClient.vcxproj`**.

Intermediate and output folders:

- `client/src/compile/win32/<ProjectName>/<Configuration>/`

Suffix convention:

- **`_d`** — Debug  
- **`_o`** — Optimized  
- **`_r`** — Release (typical for deployment)

---

## Prerequisites

### Required tools (summary)

| Item | What this tree expects | Notes |
|------|------------------------|--------|
| **OS** | **Windows 10 or later** (64-bit) | Host is x64; you still build a **Win32 (x86)** game client. |
| **IDE / build** | **Visual Studio 2013–2022+** (Community, Professional, Enterprise) **or** **Build Tools for Visual Studio** (no GUI) | The solution file is **Format 12.00** and lists **Visual Studio 18** in the header; any recent VS that opens the solution is fine. |
| **C++ workload** | **Desktop development with C++** | Installs **MSBuild**, the **C++ compiler**, and the **Windows SDK** as one unit. |
| **MSVC toolset** | **`PlatformToolset v120`** (MSVC 12.0, “Visual Studio 2013” toolset) | Set on `SwgTitan` / `SwgGodClient` and `x64.props`. **Install the v120 toolset** in the Visual Studio Installer, **or** retarget all projects to a newer toolset (e.g. **v143** for VS 2022) and resolve any new errors. |
| **Target platform** | **Win32** (32-bit x86) | Requires **x86 MSVC libraries** (included with desktop C++). |
| **Windows SDK** | Whatever your workload installs (10.x / 11.x) | Native headers/libs for Win32; **DirectX 9** headers/libs are covered by **vendored** `client/src/external/3rd/library/directx9`, not only the SDK. |
| **MSBuild** | Ships with Visual Studio | CLI builds: **Developer Command Prompt for VS** or **Developer PowerShell for VS** so `msbuild` is on `PATH`. Typical path (adjust year/channel): `.\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`. |
| **Runtimes (players)** | VC++ **Redistributable** matching the toolset you ship | Developers rarely install this to compile; **players** need the redist matching the MSVC version linked into `SwgTitan_r.exe`. After retargeting to **v143**, use the **VS 2022 x86** redistributable for a Win32 Release build. |

### Visual Studio Installer — workloads and components

1. Install **Visual Studio** (recommended: **2022** current channel) or **Build Tools for Visual Studio 2022** if you only compile from the command line.
2. Select workload **Desktop development with C++**.
3. On the **Installation details** panel, ensure at minimum:
   - **MSVC v143 - VS 2022 C++ x64/x86 build tools** (or the latest offered), so you can compile **Win32** targets.
   - **Windows 10/11 SDK** (check at least one recent SDK).
4. To build **without retargeting** projects, add the legacy toolset under **Individual components** (names vary slightly by VS version), for example:
   - **MSVC v120 - VS 2013 C++ build tools**  
   or search the installer for **`v120`** / **2013**.

If **v120** is not installed, Visual Studio will prompt to **Retarget** when you open the solution; choosing **v143** upgrades `PlatformToolset` for projects you accept—coordinate with the team so everyone uses the same toolset for reproducible binaries.

### What the project files declare

- **`ToolsVersion="12.0"`** on `.vcxproj` files (MSBuild 12 / VS 2013 era); modern MSBuild still loads these.
- **`PlatformToolset`: `v120`** for Win32 Release/Debug/Optimized on the main executables (until retargeted).
- **`CharacterSet`**: `MultiByte` (not Unicode) for the game client project.

### Other tooling (optional for these exes)

- **Git** — to clone the repo; not part of the compile.
- **PowerShell 5.1+** — some repo scripts; not required to run `msbuild` on the solution.
- **CMake** — not used by `swg.sln` (engine uses `.vcxproj` / MSBuild).

### Disk space

- Reserve **several GB** under `client/src/compile` for object files and static libraries over a full solution build.

### Third-party and engine dependencies

The client links a large set of static and import libraries (Miles, Bink, Qt, Mozilla xul, Direct3D 9, zlib, PCRE, libxml2, STlport on x86, etc.). **Vendored headers and prebuilt `.lib` / DLL pieces live under** `client/src/external/3rd/library/` **and related trees.** Do not remove or rename those paths without updating the `.vcxproj` files.

You do **not** need to install every SDK manually if your tree already contains the expected binaries for Win32; missing libs usually show up as **LNK1104** / **unresolved external** at link time.

### Optional: x64

This repository has ongoing **x64** work (see `support/docs/X64_MIGRATION.md` and `support/docs/Planning.md`). The **`swg.sln`** checked into this tree exposes **Debug | Win32**, **Optimized | Win32**, and **Release | Win32** only. Treat **Win32** as the supported path unless your branch adds **x64** solution platforms.

---

## Solution entry point

Open:

```text
client/src/build/win32/swg.sln
```

Both **SwgTitan** and **SwgGodClient** are first-class projects in this solution. Each has a long list of **solution-level project dependencies** (engine libs, `clientGame`, `clientGraphics`, `ui`, Mozilla, etc.). Building the exe from Visual Studio normally builds those dependencies first.

---

## Build steps (Visual Studio)

1. Launch Visual Studio and open `client/src/build/win32/swg.sln`.
2. Set **Solution Configuration** to **Release** (for shipping binaries), **Optimized**, or **Debug** as needed.
3. Set **Solution Platform** to **Win32**.
4. In **Solution Explorer**, right‑click **SwgTitan** → **Build** (or **Rebuild** after major engine changes).
5. Repeat for **SwgGodClient** when you need the God Client.

For a **clean full client stack**, use **Build Solution** once; it is slower but reduces missing static-lib errors.

---

## Build steps (MSBuild, command line)

Run from a **Developer Command Prompt for VS** (or any shell where `msbuild` resolves). Adjust the solution path if your repo root differs.

**Release game client:**

```bat
msbuild client\src\build\win32\swg.sln /t:SwgTitan /p:Configuration=Release /p:Platform=Win32 /m
```

**Release God Client:**

```bat
msbuild client\src\build\win32\swg.sln /t:SwgGodClient /p:Configuration=Release /p:Platform=Win32 /m
```

If MSBuild does not pull every dependency, build the whole solution once:

```bat
msbuild client\src\build\win32\swg.sln /p:Configuration=Release /p:Platform=Win32 /m
```

---

## Post-build copy (staging folder)

Both projects run **Post-build events** that copy the resulting exe (and, for SwgTitan / SwgGodClient Release, matching `*_r.dll` from the compile tree) into a **fixed deploy directory**:

```text
D:\titan\exe\win32_rel\
```

If your repository is **not** at `D:\titan`, either:

- Create `D:\titan` as a junction/symlink to your actual repo root, **or**
- Edit the **Post-build event** commands in `SwgClient.vcxproj` and `SwgGodClient.vcxproj` to match your paths.

Until those commands succeed or are updated, binaries still appear under `client/src/compile/win32/...` even when nothing is copied to `exe/win32_rel`.

---

## Verification

After **Release**:

- `client/src/compile/win32/SwgTitan/Release/SwgTitan_r.exe`
- `client/src/compile/win32/SwgGodClient/Release/SwgGodClient_r.exe`

If post-build is configured: copies under `D:\titan\exe\win32_rel\` (or your overridden folder).

---

## Troubleshooting

- **Unresolved externals when linking SwgTitan or SwgGodClient**  
  Rebuild the static libraries the exe depends on—especially **clientGraphics**, **clientGame**, **sharedObject**, and **swgClientUserInterface**—or run a full **Rebuild** of the solution for the same **Configuration** and **Win32** platform.

- **Wrong CRT / mixed debug-release libs**  
  Every dependent project must use the **same** configuration family (e.g. all **Release** for `SwgTitan_r.exe`).

- **Platform mismatch (`0xC000007B`) at runtime**  
  You are mixing **64-bit** DLLs with a **32-bit** exe or vice versa. Run from a layout where all DLLs next to the exe match **Win32** for these builds.

- **God Client / Qt-related link errors**  
  SwgGodClient pulls Qt and many of the same stacks as the main client; ensure Win32 Qt libs under `client/src/external/3rd/library/qt/...` match what the project expects.

---

## Related documentation

- `support/docs/X64_MIGRATION.md` — x64 migration notes (additive; Win32 remains primary for many trees).
- `support/docs/Planning.md` — toolchain and client/server planning context.
- `client/src/game/client/application/SwgCameraClient/README.md` — example of config/runtime layout next to `exe/win32_rel` (cwd and includes).
