# buildACM - Files Created
## New Files

| File | Description |
|------|-------------|
| `tools/buildACM.pl` | Main Perl script: full or `--fast` / `--custinfo` pipeline; temps next to `buildACM.pl` |
| `tools/buildACM_new.pl` | Deprecated; forwards to `buildACM.pl` |
| `tools/buildACM.bat` | Windows wrapper; use `BUILDACM_NO_PAUSE=1` to skip `pause` |
| `tools/buildACM.sh` | Linux shell script wrapper |
| `tools/buildACM_README.md` | Documentation and usage guide |
| `tools/perllib/CollectAcmInfo.pm` | Shared asset scan / raw customization data (used by `buildACM.pl` in-process and by `collectAssetCustomizationInfo.pl`) |

Paths are relative to the **tools tree**: often `<root>/tools` (e.g. `/home/swg/swg-main/tools` on Linux) or `<root>/client/tools` on some Windows layouts (e.g. Titan).

## Usage

### Windows (example: Titan — scripts under `client\tools`)
```cmd
cd D:\titan\client\tools
buildACM.bat
```

Or directly:
```cmd
perl buildACM.pl --verbose
```

Fast MIF/IFF only (reuse optimized `.dat` next to `buildACM.pl`):
```cmd
perl buildACM.pl --verbose --fast
```

### Linux (example: `swg-main/tools`)
```bash
cd /home/swg/swg-main/tools
chmod +x buildACM.sh
./buildACM.sh
```

Or directly:
```bash
perl buildACM.pl --verbose
```

## Features

- ✅ Auto-detects repo root from `<root>/tools` or `<root>/client/tools`
- ✅ No Perforce dependency
- ✅ Full pipeline (5 user-visible steps) or **fast** path (4 steps) with `--fast` / `--custinfo`
- ✅ Intermediates always in the tools directory containing `buildACM.pl` (not shell CWD)
- ✅ Proper path normalization (forward slashes everywhere)
- ✅ Verbose and debug output options
- ✅ Clean build option; `--keep-temp` to retain intermediates
- ✅ Comprehensive error handling
- ✅ Creates output directories if needed

## Build Steps (All Automated)

**Full:** gather → lookup → collect → **optimize + MIF (one `buildAssetCustomizationManagerData.pl`)** → IFF → cleanup.

**Fast:** gather → lookup → MIF-only from cached `.dat` → IFF → cleanup.

1. Gather asset files (.apt, .cmp, .lmg, .lod, .lsb, .mgn, .msh, .sat, .pob, .sht, .trt)
2. Create lookup table (`acm_lookup.dat` next to `buildACM.pl`)
3. *(Optional in fast mode)* Collect customization info (in-process via `CollectAcmInfo`, or standalone `collectAssetCustomizationInfo.pl`)
4. **Optimize + MIF** in one Perl run, or MIF-only from cache in fast mode
5. Compile to IFF (Miff)
6. Cleanup temporary files (preserves cached optimized input used with `--fast`)

## Output

- `data/sku.0/sys.client/compiled/game/customization/asset_customization_manager.iff`
- `data/sku.0/sys.client/compiled/game/customization/customization_id_manager.iff`
