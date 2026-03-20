# SwgMayaEditor - Maya 2026 Plugin

64-bit Maya 2026 plugin for SWG asset authoring. Migrated from MayaExporter (32-bit Maya 8).

## Requirements

- **Maya 2026** (64-bit) - [Download](https://www.autodesk.com/developer-network/platform-technologies/maya)
- **Maya 2026 Devkit** - **Separate download from Maya.** The devkit is not included in the Maya installer. Get it from the Downloads section of the [Autodesk Feedback Forum](https://feedback.autodesk.com/). Extract it and place the `include/` and `lib/` folders where FindMaya can find them (e.g. inside your Maya install, or set `MAYA_LOCATION` to the devkit root).
- **Visual Studio 2022** (17.8.3+)
- **CMake** 3.13+

## Build

### Windows (x64 required)

```batch
cd D:\titan\client\src\engine\client\application\MayaModern

# Create build directory - use VS 2022 Insiders
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build (or open build/SwgMayaEditor.sln in VS 2022 Insiders)
cmake --build build --config Release
```

The plugin will be built as `SwgMayaEditor.mll`. You can also open `build/SwgMayaEditor.sln` directly in VS 2022 Insiders.

### Maya Location

FindMaya checks these locations (in order):
- `MAYA_LOCATION` (env or CMake variable)
- `C:\Program Files\Autodesk\Maya2026`
- `D:\Program Files\Autodesk\Maya2026`

If Maya is elsewhere:

```batch
set MAYA_LOCATION=D:\Path\To\Maya2026
cmake -B build -A x64
```

**Note:** The devkit (include/, lib/) must be present. If you installed Maya separately from the devkit, extract the devkit archive so its `include/` and `lib/` folders are inside your Maya directory, or point `MAYA_LOCATION` to the devkit root.

**VS 2022 Insiders:** Use the VS 2022 generator; the .sln will open in your default VS. To force Insiders when both are installed, set `CMAKE_GENERATOR_INSTANCE` to your Insiders install path (e.g. `D:/Program Files/Microsoft Visual Studio/2022/Insiders`).

## Install

Copy `SwgMayaEditor.mll` to your Maya 2026 plugins directory:

```
C:\Users\<user>\Documents\maya\2026\plugins\
```

Or add the build output path to `MAYA_PLUG_IN_PATH` in Maya's environment.

## Features

See [todo.txt](todo.txt) for the full feature roadmap and MayaExporter parity checklist.

### Current Translators
- **mgn** - Mesh generator (.mgn)
- **msh** - Mesh (.msh)
- **skt** - Skeleton (.skt)
- **flr** - Floor (.flr)
