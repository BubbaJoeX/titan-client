# ANS (KFAT/CKAT) Format Analysis

## Overview

ANS files are SWG animation files in IFF format. Two variants exist:
- **KFAT** (Keyframe Animation Template): Uncompressed, version 0003
- **CKAT** (Compressed Keyframe Animation Template): Compressed, version 0001

## File Structure

### KFAT (0003)
```
KFAT
  0003
    INFO     : fps, frameCount, transformCount, rotationChannelCount, staticRotationCount,
               translationChannelCount, staticTranslationCount (all int32)
    XFRM     : Transform info
      XFIN   : per transform (name, hasAnimatedRotation, rotationChannelIndex, translationMask,
               translationChannelIndexX/Y/Z) - uint32 for indices
    AROT     : Animated rotations
      QCHN   : per channel - keyCount(int32), then per key: frame(int32), quat(w,x,y,z floats)
    SROT     : Static rotations - per entry: quat(w,x,y,z)
    ATRN     : Animated translations
      CHNL   : per channel - keyCount(int32), then per key: frame(int32), value(float)
    STRN     : Static translations - per entry: float
```

### CKAT (0001)
```
CKAT
  0001
    INFO     : fps, frameCount(int16), transformCount(int16), rotationChannelCount(int16),
               staticRotationCount(int16), translationChannelCount(int16), staticTranslationCount(int16)
    XFRM     : Transform info
      XFIN   : per transform - name, hasAnimatedRotation(int8), rotationChannelIndex(int16),
               translationMask(uint8), translationChannelIndexX/Y/Z(int16)
    AROT     : Animated rotations (optional)
      QCHN   : per channel - keyCount(int16), xFormat, yFormat, zFormat(uint8),
               then per key: frame(int16), compressedQuat(uint32)
    SROT     : Static rotations (optional) - per entry: xFormat, yFormat, zFormat, compressedQuat
    ATRN     : Animated translations (optional)
      CHNL   : per channel - keyCount(int16), then per key: frame(int16), value(float)
    STRN     : Static translations (optional)
    MSGS     : Animation messages (optional)
    LOCT     : Locomotion translation (optional)
    QCHN     : Locomotion rotation (optional, at form end)
```

## Translation Mask (SATCCF)

- `0x08` (MASK_X): X translation is animated
- `0x10` (MASK_Y): Y translation is animated  
- `0x20` (MASK_Z): Z translation is animated

When mask bit is SET = animated (use channel). When CLEAR = static (use static value if index valid).

## Rotation/Translation Semantics

- **Delta from bind pose**: ANS stores deltas. Final = delta * bindPose (rotation), Final = bindPose + delta (translation)
- **Quaternion**: Engine format (w,x,y,z). Maya MQuaternion (x,y,z,w). Export: delta = current * conj(bind)
- **Translation X**: Engine negates X; import applies -delta for X

## Channel Indexing

- Each transform (joint) has: rotationChannelIndex, translationChannelIndexX/Y/Z
- Indices point into flat channel arrays (AROT channels 0..N-1, ATRN channels 0..M-1)
- Index -1 or invalid = no data for that axis
- Channels are per-axis: one CHNL = one axis (X or Y or Z) for one joint

## Common Issues

1. **Joint name matching**: ANS uses "root__skeleton" format. Scene may have "namespace:root__skeleton". Stripping namespace required.
2. **Multiple instances**: Scene may have kaadu:kaadu and kaadu:kaadu1. Joint map keeps first match; only one skeleton gets animated.
3. **Empty Graph Editor**: Select the actual joint (e.g. root__kaadu), not the parent group, to see curves.
4. **Only vertical movement**: If only translateY animates, check: translationMask has MASK_X/MASK_Z for root, rotation has hasAnimatedRotations=true, joint names match.
