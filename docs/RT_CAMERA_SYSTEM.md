# Real-Time Camera System Implementation

## Overview

The RT Camera System enables real-time security/surveillance cameras in SWG: Titan. Players can place cameras in the world and view their feeds on linked screens.

## Components

### Server-Side Scripts

#### `item/rt_camera.java`
Camera object script that:
- Manages camera linkage to screens
- Controls camera activation state
- Stores camera settings (FOV, name)
- Provides radial menu for configuration

**Objvars:**
- `rt_camera.linkedScreen` - obj_id of linked screen
- `rt_camera.owner` - obj_id of owner player
- `rt_camera.isActive` - boolean, whether camera is streaming
- `rt_camera.fov` - float, field of view (30-120 degrees)
- `rt_camera.name` - string, custom camera name

#### `item/rt_screen.java`
Screen object script that:
- Displays camera feed
- Manages camera linkage
- Controls resolution settings
- Provides viewing interface

**Objvars:**
- `rt_screen.linkedCamera` - obj_id of linked camera
- `rt_screen.owner` - obj_id of owner
- `rt_screen.isDisplaying` - boolean
- `rt_screen.resolution` - int (256 or 512)
- `rt_screen.name` - string, custom screen name

### Client-Side Components

#### `RtCameraManager.h/.cpp`
Central manager for RT camera feeds:
- Tracks active camera feeds (max 4)
- Manages render target textures
- Handles camera transform updates
- Renders camera views to textures

**Key Functions:**
- `registerFeed()` - Create new camera feed
- `unregisterFeed()` - Remove camera feed
- `updateCameraTransform()` - Update camera position
- `renderFeeds()` - Render all active feeds
- `getScreenTexture()` - Get texture for screen display

### Developer Commands

Use `/developer` command:
- `spawnRtCamera [name]` - Spawn RT Camera at location
- `spawnRtScreen [name]` - Spawn RT Screen at location

## Workflow

1. **Setup:**
   - Spawn or obtain RT Camera and RT Screen objects
   - Place camera where you want to view from
   - Place screen where you want to see the feed

2. **Linking:**
   - Use radial menu on camera → "Link to Screen"
   - Use radial menu on screen → "Link to Camera"
   - Camera and screen are now paired

3. **Activation:**
   - Use radial menu on camera → "Activate"
   - Camera starts streaming

4. **Viewing:**
   - Approach the screen
   - Use radial menu → "View Camera Feed"
   - Screen displays real-time camera view

## Technical Details

### Render Pipeline
```
Camera Object Transform
    ↓
RtCameraManager configures RenderWorldCamera
    ↓
Exclusion list updated (all RT screens + camera object)
    ↓
Scene rendered via RenderWorld::drawScene() to D3D9 RenderTarget
    ↓
Texture applied to screen object material
```

### Full Scene Rendering
The RT Camera system uses proper `RenderWorldCamera` for rendering:
- Handles DPVS visibility culling
- Proper lighting and shadows
- All render passes (environment, objects, effects)
- Configurable FOV per camera

### Recursion Protection
To prevent infinite loops when cameras view screens:
- All registered RT Screen objects are added to exclusion list
- Camera object itself is excluded from its own render
- Global recursion guard prevents nested `renderFeeds()` calls
- `RenderWorld::recursivelyDisableDpvsObjectsForThisRender()` used for exclusion

### Performance Limits
- Maximum 4 active cameras
- Resolution: 256x256 or 512x512
- Update rate: ~20 FPS (50ms intervals), max 60 FPS
- Only renders when screen is within 100 meters of player
- Only renders when screen is visible

### Security
- Server validates camera/screen permissions
- Owner must activate camera
- Linkage validated on both ends
- Client cannot arbitrarily create feeds

## File Locations

### Server Scripts
- `dsrc/sku.0/sys.server/compiled/game/script/item/rt_camera.java`
- `dsrc/sku.0/sys.server/compiled/game/script/item/rt_screen.java`

### Client Code
- `client/src/engine/client/library/clientGame/src/shared/core/RtCameraManager.h`
- `client/src/engine/client/library/clientGame/src/shared/core/RtCameraManager.cpp`

### Build Integration
- `client/src/engine/client/library/clientGame/build/win32/clientGame.vcxproj`
- `client/src/engine/client/library/clientGame/src/shared/core/SetupClientGame.cpp`

## Future Enhancements

1. ~~**Custom Object Templates**~~ ✅ IMPLEMENTED
   - Created dedicated `rt_camera.iff` and `rt_screen.iff` templates
   - Templates in `object/tangible/device/`
   - Scripts auto-attached via template definition

2. **Screen Material Shader**
   - Create `shader/rt_screen.sht` for proper texture display
   - Support dynamic texture binding

3. ~~**Full Scene Rendering**~~ ✅ IMPLEMENTED
   - Uses proper `RenderWorldCamera` for full scene rendering
   - Recursion protection via exclusion lists
   - All RT screens excluded from RT camera renders

4. **Player Permissions**
   - City/guild based camera access
   - Public/private feed settings

5. **Network Optimization**
   - Only send camera updates when viewing
   - Distance-based resolution scaling

