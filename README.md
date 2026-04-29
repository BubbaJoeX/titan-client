# SWG: Bubba Joe's Flavor

This repository and its sibling repositories (`src`, `dsrc`) contain internal tooling and development for **Bubba Joe's flavor of Star Wars Galaxies**. No support or assistance will be given.

Code is provided as/is and may be updated at any moment. Many features from these repositories may be missing contents from other repositories.

## Credits

- **Aconite**: Various implementation
- **Cekis**: Snapshot dumper / Stella Bellum
- **SWG Source**: All maintainers, past and present

---

## Guides and Documentation

Technical references under **`docs/`** are a copy of **`support/docs/`** at the repository root—edit whichever tree your workflow uses, and keep them in sync if both matter.

### Client root guides

- [COMPILE_GUIDE_CLIENT.md](COMPILE_GUIDE_CLIENT.md) — Build **SwgTitan** and **SwgGodClient** (Visual Studio 2013 / `v120`, MSBuild, staging under `exe/win32_rel`).
- [MODELING.md](MODELING.md) — Maya 8 and the SWG export pipeline (static/skeletal meshes, appearance templates, naming).
- [VLC.md](VLC.md) — Deploy **libVLC 3.0.22** (Win32), `plugins/`, and optional `yt-dlp.exe` next to the game exe (`exe/win32_rel`).
- [STREAMING-GIF-ETC.md](STREAMING-GIF-ETC.md) — Magic painting, magic video player, speaker emitters (architecture and checklist).
- [FEATURES.md](../FEATURES.md) — Titan-only and customized systems (monorepo index).

### Planning and platform

- [docs/Planning.md](docs/Planning.md) — Running notes (Magic Painting, God Client, x64 inventory, fixes).
- [docs/X64_MIGRATION.md](docs/X64_MIGRATION.md) — Client x64 migration, **SwgTitan** checklist, bundle script.

### Gameplay and systems

- [docs/BOUNTY_HUNTER_OVERHAUL.md](docs/BOUNTY_HUNTER_OVERHAUL.md) — Bounty hunter feature plan and touchpoints.
- [docs/CRAFTING_OVERHAUL.md](docs/CRAFTING_OVERHAUL.md) — Crafting QoL / schematic library / BOM scope.
- [docs/CITY_GAMEPLAY_LANDSCAPING_UPDATE.md](docs/CITY_GAMEPLAY_LANDSCAPING_UPDATE.md) — City terrain painting, taxation, courts, starport, expulsion (design plan).
- [docs/CITY_IMPLEMENTATION_SUMMARY.md](docs/CITY_IMPLEMENTATION_SUMMARY.md) — City implementation summary.
- [docs/adding_story_companions.md](docs/adding_story_companions.md) — Story companion datatable pipeline.
- [docs/ATMO_LANDING_POINTS.md](docs/ATMO_LANDING_POINTS.md) — Atmospheric flight landing points and docking.

### TangibleDynamics (physics on objects)

- [docs/INDEX.md](docs/INDEX.md) — Package index (start here).
- [docs/TANGIBLE_DYNAMICS_SUMMARY.md](docs/TANGIBLE_DYNAMICS_SUMMARY.md) — Executive summary.
- [docs/TANGIBLE_DYNAMICS.md](docs/TANGIBLE_DYNAMICS.md) — Full architecture.
- [docs/TANGIBLE_DYNAMICS_QUICK_REFERENCE.md](docs/TANGIBLE_DYNAMICS_QUICK_REFERENCE.md) — API quick reference.
- [docs/TANGIBLE_DYNAMICS_IMPLEMENTATION.md](docs/TANGIBLE_DYNAMICS_IMPLEMENTATION.md) — Implementation details.
- [docs/BUILD_AND_INTEGRATION.md](docs/BUILD_AND_INTEGRATION.md) — Build and integration.
- [docs/IMPLEMENTATION_FILES.md](docs/IMPLEMENTATION_FILES.md) — File list and dependency map.

### Client engine and media

- [docs/ENGINE_IMPROVEMENTS.md](docs/ENGINE_IMPROVEMENTS.md) — D3D9 instancing, occlusion queries, related optimizations.
- [docs/TEXTURE_URL_RESOLVING.md](docs/TEXTURE_URL_RESOLVING.md) — Magic painting (remote URL textures) pipeline.
- [docs/RT_CAMERA_SYSTEM.md](docs/RT_CAMERA_SYSTEM.md) — Real-time security cameras and screens.

### Data, calendar, commands

- [docs/CALENDAR_DATABASE_IMPLEMENTATION.md](docs/CALENDAR_DATABASE_IMPLEMENTATION.md) — Oracle calendar persistence.
- [docs/commands.md](docs/commands.md) — Command reference (if maintained).

### Art and interiors (Maya / buildouts)

- [docs/MAYA_POB_FROM_SCRATCH.md](docs/MAYA_POB_FROM_SCRATCH.md) — POI from scratch.
- [docs/MAYA_KITBASH_IMPORT_COMBINE.md](docs/MAYA_KITBASH_IMPORT_COMBINE.md) — Kitbash import/combine.
- [docs/INTERIOR_STRUCTURE_CREATION.md](docs/INTERIOR_STRUCTURE_CREATION.md) — Interior structure workflow.

### Project prompts and checklists

- [docs/plan-inGameCalendar.prompt.md](docs/plan-inGameCalendar.prompt.md) — In-game calendar design prompt.
- [docs/MASTER_CHECKLIST.md](docs/MASTER_CHECKLIST.md) — Master checklist.
- [docs/COMPLETION_REPORT.md](docs/COMPLETION_REPORT.md) — TangibleDynamics completion report.

---

*If you would of did it differently, why the hell are you here?*
