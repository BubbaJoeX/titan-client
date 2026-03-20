# SwgMapRasterizer x64 Migration (Tool-Only)

This document describes what is required to convert **only** `SwgMapRasterizer` to 64-bit so it can generate very large maps (including `1m = 1px`) without 32-bit allocation limits.

## Goal

- Build `SwgMapRasterizer` as `x64` while preserving:
  - `.trn` terrain parsing
  - shader (`.sht`) resolution
  - DDS texture decode/sampling
  - CPU colormap output path
- Avoid full-client 64-bit migration scope.

## Why x64 is needed

At high sizes, internal buffers exceed practical 32-bit address space:

- `-size 8192` with `-aa` => internal `16384 x 16384`
- `heightPixels` alone is ~1.0 GiB (`float` per pixel)
- per-tile `CreateChunkBuffer` allocates multiple maps and can fail even earlier

x64 removes these virtual address constraints and greatly reduces allocation fragmentation risk.

## Scope boundaries (important)

Do this as a **tool-only** migration first:

- Keep GPU render path out of scope (CPU colormap mode is sufficient for map generation).
- Keep game runtime binaries unchanged.
- Prefer static/existing engine libraries where already available in x64.

## Required work

## 1) Add x64 project/platform configuration

Files:

- `build/win32/SwgMapRasterizer.vcxproj`
- `build/win32/SwgMapRasterizer.sln`

Tasks:

- Add `Debug|x64`, `Optimized|x64`, `Release|x64` configurations.
- Set `PlatformToolset` consistent with repo policy.
- Set output/intermediate dirs for x64 (separate from Win32).
- Carry over linker options (`/STACK`, `/HEAP`) as needed.

## 2) Resolve x64 library/link dependencies

Current project links a very large Win32-era dependency set. For x64 tool-only, reduce to what colormap mode needs:

- Keep required shared libraries for:
  - foundation/file/tree access
  - terrain generator
  - image/targa
  - compression/DDS decode helper
- Remove or conditionalize unnecessary GPU/UI/client dependencies for x64 tool build.

Practical approach:

- Create a dedicated `SwgMapRasterizer_x64` link profile in the `.vcxproj`.
- Start from minimal link set and add only missing symbols.

## 3) Verify third-party x64 availability

Must have x64-compatible binaries/headers for:

- `ATI_Compress` (or replace with alternate DDS decode path)
- any remaining external static libs in the link list

If a dependency is Win32-only:

- replace with x64 equivalent, or
- gate feature out for tool-only mode.

## 4) Pointer-size and format-safety audit

Audit code paths used by this tool for 64-bit safety:

- `%d` vs `%zu` / `%llu` for sizes
- `int` overflow in pixel/byte math
- pointer-to-int casts
- assumptions about 4-byte pointers

`SwgMapRasterizer.cpp` already uses `size_t` in several large-buffer paths; keep extending that pattern.

## 5) Memory model tuning for 1m=1px

For a `16km` map at `1m=1px`, output is `16384 x 16384`:

- RGB output ~768 MiB
- height map ~1.0 GiB
- temp/easing/downsample buffers add more

On x64, this is feasible but still heavy. Keep/extend:

- AA auto-disable logic at very large sizes
- tile auto-scaling by poles-per-tile
- runtime memory guards

## 6) Build-system routing

Ensure x64 build artifacts are discoverable by existing scripts:

- update any tool launch scripts that assume Win32 output paths
- keep Win32 binary intact for compatibility

## 7) Validation checklist

Run and verify:

- `-terrain terrain/naboo.trn -size 6144`
- `-terrain terrain/naboo.trn -size 8192 -noaa`
- `-terrain terrain/naboo.trn -size 16384 -noaa` (target 1m=1px for 16km map)

Validate:

- no allocation fatals
- shader families resolve and sample
- water pass executes
- output orientation/tiling remains correct

## Known non-goals in this migration

- making full game/client x64
- GPU-rendered mode parity
- broad refactor of terrain engine APIs

## Suggested implementation order

1. Add x64 platform/config entries
2. Minimal colormap-only link set
3. Replace/remove Win32-only deps
4. Compile and fix type-width issues
5. Run 6k/8k/16k validation cases

Once these pass, `SwgMapRasterizer` can produce full-resolution maps without 32-bit memory ceilings.

