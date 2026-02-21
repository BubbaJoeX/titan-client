# buildACM - Files Created
## New Files

| File | Description |
|------|-------------|
| `tools/buildACM.pl` | Main Perl script - handles entire ACM/CIM build process |
| `tools/buildACM.bat` | Windows batch file wrapper |
| `tools/buildACM.sh` | Linux shell script wrapper |
| `tools/buildACM_README.md` | Documentation and usage guide |

## Usage

### Windows
```cmd
cd D:\titan\client\tools
buildACM.bat
```

Or directly:
```cmd
perl buildACM.pl --verbose
```

### Linux
```bash
cd /home/swg/swg-main/client/tools
chmod +x buildACM.sh
./buildACM.sh
```

Or directly:
```bash
perl buildACM.pl --verbose
```

## Features

- ✅ Auto-detects Windows (D:/titan/) and Linux (/home/swg/) paths
- ✅ No Perforce dependency
- ✅ Single script handles all 6 build steps
- ✅ Proper path normalization (forward slashes everywhere)
- ✅ Verbose and debug output options
- ✅ Clean build option
- ✅ Comprehensive error handling
- ✅ Creates output directories if needed

## Build Steps (All Automated)

1. Gather asset files (.apt, .cmp, .lmg, .lod, .lsb, .mgn, .msh, .sat, .pob, .sht, .trt)
2. Create lookup table
3. Collect customization info (calls collectAssetCustomizationInfo.pl)
4. Optimize data (calls buildAssetCustomizationManagerData.pl -r)
5. Build MIF files (calls buildAssetCustomizationManagerData.pl)
6. Compile to IFF (calls Miff compiler)
7. Cleanup temporary files

## Output

- `data/sku.0/sys.client/compiled/game/customization/asset_customization_manager.iff`
- `data/sku.0/sys.client/compiled/game/customization/customization_id_manager.iff`
