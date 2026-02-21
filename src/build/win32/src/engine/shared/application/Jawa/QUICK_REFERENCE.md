# ======================================================================
# JAWA PATH FIX - QUICK REFERENCE
# ======================================================================

## FILES CREATED FOR YOU:
✓ Jawa.h - ALREADY UPDATED (declarations added)
✓ COMPLETE_IMPLEMENTATION.cpp - Copy/paste into Jawa.cpp
✓ PERL_PATCH_INSTRUCTIONS.txt - Perl module fixes
✓ FIX_ACM_PS1_SCRIPT.txt - How to fix your PowerShell script
✓ STEP_BY_STEP_INSTRUCTIONS.md - Complete guide

## QUICK FIX (2 Minutes):

### 1. Edit Jawa.cpp (3 changes)
   - Add normalizePath() and joinPath() after line 234
   - Replace acmMiff/cimMiff lines (~582)
   - Replace acmIff/cimIff lines (~593)
   
   See: COMPLETE_IMPLEMENTATION.cpp

### 2. Edit VehicleCustomizationVariableGenerator.pm
   - Replace collectData function
   
   See: tools/PERL_PATCH_INSTRUCTIONS.txt

### 3. Edit buildAssetCustomizationManagerData.pl
   - Fix line 1404 to handle missing CIM file
   
   See: tools/PERL_PATCH_INSTRUCTIONS.txt

### 4. Rebuild Jawa
   ```bash
   cd Jawa\build
   cmake --build . --config Release
   ```

### 5. Fix YOUR ACM.ps1
   **THE KEY ISSUE**: You're passing data/ path but need dsrc/ path!
   
   ```powershell
   # WRONG (what you have now):
   $basePath = "D:\titan\data\sku.0\sys.client\..."
   
   # RIGHT (what you need):
   $basePath = "D:\titan\dsrc\sku.0\sys.shared\compiled\game"
   $clientPath = "D:\titan\data\sku.0\sys.client\compiled\game"
   ```

## PATH RULE TO REMEMBER:
```
dsrc = SOURCE (read .mif, .tab files from here)
data = DESTINATION (write .iff files to here)
```

## RUN COMMAND:
```powershell
.\Jawa.exe --verbose --perl-lib "D:/titan/client/tools/perllib" --client-path "D:/titan/data/sku.0/sys.client/compiled/game" "D:/titan/dsrc/sku.0/sys.shared/compiled/game"
```

## SUCCESS LOOKS LIKE:
```
Jawa: Starting build... [Step 1/5]
...
***SUCCESS*** Finished compiling Asset Customization Manager and Customization ID Manager!
```

## IF YOU STILL GET ERRORS:
- "D:\titan\data\..." in error → Your ACM.ps1 still has wrong base path
- "failed to load vehicle_customizations.tab" → Apply Perl patch
- "Mixed separators" → Apply Jawa.cpp normalizePath patch
- "Double backslash" → Apply joinPath patch
