# ======================================================================
# STEP-BY-STEP FIX INSTRUCTIONS
# ======================================================================

## STEP 1: Apply Jawa.cpp Changes

1. Open: `D:\titan\client\src\engine\shared\application\Jawa\src\shared\Jawa.cpp`

2. Find line 234 (after the `fileExists()` function closing brace)

3. INSERT the normalizePath() and joinPath() functions from:
   `COMPLETE_IMPLEMENTATION.cpp`

4. Find line ~582 and replace the acmMiff/cimMiff lines:
   ```cpp
   // OLD:
   const std::string acmMiff = config.basePath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "asset_customization_manager.mif";
   const std::string cimMiff = config.basePath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "customization_id_manager.mif";
   
   // NEW:
   std::string acmMiff = joinPath(joinPath(config.basePath, "customization"), "asset_customization_manager.mif");
   std::string cimMiff = joinPath(joinPath(config.basePath, "customization"), "customization_id_manager.mif");
   ```

5. Find line ~593 and replace the acmIff/cimIff lines:
   ```cpp
   // OLD:
   const std::string acmIff = config.clientPath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "asset_customization_manager.iff";
   const std::string cimIff = config.clientPath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "customization_id_manager.iff";
   
   // NEW:
   std::string acmIff = joinPath(joinPath(config.clientPath, "customization"), "asset_customization_manager.iff");
   std::string cimIff = joinPath(joinPath(config.clientPath, "customization"), "customization_id_manager.iff");
   ```

## STEP 2: Apply Perl Fixes

1. Open: `D:\titan\client\tools\perllib\VehicleCustomizationVariableGenerator.pm`

2. Find the `collectData` subroutine (around line 264)

3. Replace it with the version in: `tools/PERL_PATCH_INSTRUCTIONS.txt`

4. Open: `D:\titan\client\tools\buildAssetCustomizationManagerData.pl`

5. Find line ~1404 in `loadCustomizationIdManagerMif`

6. Replace:
   ```perl
   # OLD:
   open ($inputFile, "< " . $customizationIdManagerMifFileName) or die "Failed to open file [$customizationIdManagerMifFileName] for reading: $!";
   
   # NEW:
   my $cimPath = $customizationIdManagerMifFileName;
   $cimPath =~ s/\\/\//g;
   $cimPath =~ s/\/data\//\/dsrc\//g;
   if (!open($inputFile, "< " . $cimPath))
   {
       print "Note: CIM file [$cimPath] does not exist yet, creating new one.\n";
       return ({}, 0);
   }
   ```

## STEP 3: Rebuild Jawa

```bash
cd D:\titan\client\src\engine\shared\application\Jawa\build
cmake --build . --config Release
```

## STEP 4: Fix Your ACM.ps1 Script

**CRITICAL**: Your ACM.ps1 is passing wrong paths!

Change your script to use:
- BASE PATH: `D:/titan/dsrc/sku.0/sys.shared/compiled/game` (SOURCE - where .mif files are)
- CLIENT PATH: `D:/titan/data/sku.0/sys.client/compiled/game` (DESTINATION - where .iff files go)

See: `FIX_ACM_PS1_SCRIPT.txt` for details

## STEP 5: Run Jawa

```powershell
& "D:\titan\client\src\engine\shared\application\Jawa\build\bin\Jawa.exe" `
    --verbose `
    --perl-lib "D:/titan/client/tools/perllib" `
    --client-path "D:/titan/data/sku.0/sys.client/compiled/game" `
    "D:/titan/dsrc/sku.0/sys.shared/compiled/game"
```

## VERIFICATION

You should see output like:
```
Jawa: Starting build... [Step 1/5]
Jawa: Finished Writing ACM Lookup File... [Step 2/5]
Jawa: Finished Writing Asset Customization Info Collection... [Step 3/5]
Jawa: Finished Optimizing Lookup... [Step 4/5]
Jawa: Finished Creating ACM and CIM Miff Files... [Step 5/5]
***SUCCESS*** Finished compiling Asset Customization Manager and Customization ID Manager!
```

## COMMON ERRORS

### "failed to load vehicle variable customizations file"
- Fix: Apply VehicleCustomizationVariableGenerator.pm patch
- Or: Set environment variable `$env:SWG_DATATABLES_PATH = "D:/titan/dsrc/sku.0/sys.shared/compiled/game/"`

### "D:\titan\data\..." in error (should be dsrc)
- Fix: Your ACM.ps1 is passing the wrong base path
- Use dsrc for base path, data for client path

### Mixed separators (D:/path/\file)
- Fix: Apply Jawa.cpp normalizePath() implementation
- Or: Use forward slashes consistently in ACM.ps1

### Double backslashes (game\\customization)
- Fix: Apply joinPath() implementation
- This handles trailing/leading slashes correctly
