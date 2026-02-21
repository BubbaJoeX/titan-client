# ======================================================================
# JAWA ACM BUILD TOOL - WINDOWS/LINUX PATH FIXES SUMMARY
# ======================================================================

## ISSUE DESCRIPTION

When running Jawa on Windows, the following errors occur:
1. "failed to load vehicle variable customizations file" - VehicleCustomizationVariableGenerator.pm uses hardcoded Linux paths
2. "Failed to open file [D:\titan\data\sku.0\sys.client\...]" - Path contains /data/ instead of /dsrc/
3. Double backslashes in paths (game\\customization) - PATH_SEPARATOR concatenation issue

## PATH RULES

| File Type | Directory |
|-----------|-----------|
| .mif (source) | dsrc/sku.0/sys.shared/compiled/game/ or dsrc/sku.0/sys.client/compiled/game/ |
| .iff (compiled) | data/sku.0/sys.shared/compiled/game/ or data/sku.0/sys.client/compiled/game/ |
| .tab (datatables) | dsrc/sku.0/sys.shared/compiled/game/datatables/ |

## FILES TO MODIFY

### 1. D:\titan\client\tools\perllib\VehicleCustomizationVariableGenerator.pm

**Location:** Lines 264-288 (collectData function)

**Problem:** Hardcoded paths `//depot/swg/...` and `//home/swg/swg-main/build/`

**Fix:** Auto-detect root directory from script location

See: tools/PERL_PATCH_INSTRUCTIONS.txt

### 2. D:\titan\client\tools\buildAssetCustomizationManagerData.pl

**Location:** Line 1404 (loadCustomizationIdManagerMif function)

**Problem:** Script tries to read CIM .mif file and dies if not found

**Fix:** Handle missing file gracefully (return empty data for first-time builds)

See: tools/PERL_PATCH_INSTRUCTIONS.txt

### 3. D:\titan\client\src\engine\shared\application\Jawa\src\shared\Jawa.cpp

**Location:** Multiple path concatenations

**Problem:** Using PATH_SEPARATOR causes double backslashes on Windows

**Fix:** Add normalizePath() and joinPath() helper functions

See: src/engine/shared/application/Jawa/JAWA_CPP_PATCH.txt

### 4. D:\titan\client\src\engine\shared\application\Jawa\src\shared\Jawa.h

**Add declarations:**
```cpp
static std::string normalizePath(const std::string &path);
static std::string joinPath(const std::string &base, const std::string &sub);
```

## QUICK FIX (Without Code Changes)

If you want to test without modifying code, you can set environment variables:

```powershell
$env:SWG_DATATABLES_PATH = "D:/titan/dsrc/sku.0/sys.shared/compiled/game/"
```

Then run Jawa with the correct paths:
```powershell
.\Jawa.exe --verbose D:/titan/dsrc/sku.0/sys.shared/compiled/game/
```

## REBUILD JAWA

After applying the patches:
```bash
cd D:\titan\client\src\engine\shared\application\Jawa\build
cmake --build . --config Release
```

## VERIFY

Run with --verbose to see actual paths being used:
```bash
.\Jawa.exe --verbose D:/titan/dsrc/sku.0/sys.shared/compiled/game/
```
