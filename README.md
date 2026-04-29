# SWG: Bubba Joe's Flavor

This repository and its sibling repositories (`src`, `dsrc`) contain internal tooling and development for **Bubba Joe's flavor of Star Wars Galaxies**. No support or assistance will be given.

Code is provided as/is and may be updated at any moment. Many features from these repositories may be missing contents from other repositories.

## Credits

- **Aconite**: Various implementation
- **Cekis**: Snapshot dumper / Stella Bellum
- **SWG Source**: All maintainers, past and present

---

## Documentation

First-party guides and READMEs in this tree (paths relative to `client/`).

### Top level

- [COMPILE_GUIDE_CLIENT.md](COMPILE_GUIDE_CLIENT.md) — Build **SwgTitan** and **SwgGodClient** (Visual Studio 2013 / `v120`, MSBuild, staging under `exe/win32_rel`).
- [STREAMING-GIF-ETC.md](STREAMING-GIF-ETC.md) — Magic painting, magic video player, speaker emitters, and client media dependencies.
- [MODELING.md](MODELING.md) — Maya 8 and the SWG export pipeline (static/skeletal meshes, appearance templates, naming).

### `tools/`

- [buildACM_README.md](tools/buildACM_README.md) — `buildACM.pl`: ACM / CIM IFF generation (Perl, cross-platform).
- [buildACM_SUMMARY.md](tools/buildACM_SUMMARY.md) — Files produced by ACM tooling and quick usage.
- [PERL_FILES_STATUS.md](tools/PERL_FILES_STATUS.md) — Perl path handling and Perforce removal (reference status).

### Component READMEs

- [SwgCameraClient](src/game/client/application/SwgCameraClient/README.md) — Map capture / camera client, config next to `exe/win32_rel`.
- [SwgMapRasterizer](src/engine/shared/application/SwgMapRasterizer/README.md) — Map rasterizer; [X64_MIGRATION.md](src/engine/shared/application/SwgMapRasterizer/X64_MIGRATION.md) for that tool.
- [TerrainEditor](src/engine/client/application/TerrainEditor/README.md) — Terrain editor.
- [MayaModern](src/engine/client/application/MayaModern/README.md) — Maya plugin and tooling; more under `MayaModern/docs/`.
- [AcmBuildTool](src/build/win32/src/engine/shared/application/AcmBuildTool/README.md) — ACM build tool (Visual Studio project).

### Repository-wide (outside `client/`)

- [support/docs/Planning.md](../support/docs/Planning.md) — Project planning and toolchain notes.
- [support/docs/X64_MIGRATION.md](../support/docs/X64_MIGRATION.md) — Client x64 migration details.

---

*If you would of did it differently, why the hell are you here?*
