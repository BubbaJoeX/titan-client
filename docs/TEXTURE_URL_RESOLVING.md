# TEXTURE_URL_RESOLVING — Magic Painting (Remote Texture URL) Feature

## Overview

The Magic Painting system allows any in-world `TangibleObject` to dynamically load and render a remote image (PNG, JPEG, GIF) from a URL. The image is fetched at runtime via WinHTTP, decoded via Windows Imaging Component (WIC), and rendered as a Direct3D texture. The feature is condition-driven — no new templates are required. Setting `CONDITION_MAGIC_PAINTING_URL` on any existing object, along with the `texture.url` objvar, converts it into a magic painting.

---

## Architecture

```
Server (src/)                          Client (client/)
┌──────────────────────┐              ┌──────────────────────────────┐
│ TangibleObject.cpp   │   baselines  │ TangibleObject.cpp           │
│  - reads objvars     │──────────────│  - receives AutoDeltaVars    │
│  - sets AutoDelta    │   (shared_np)│  - fetches image via WinHTTP │
│    variables         │              │  - decodes via WIC           │
│  - attaches script   │              │  - creates overlay objects   │
│    magic_painting_url│              │  - handles GIF animation     │
└──────────────────────┘              │  - applies texture scroll    │
                                      └──────────────────────────────┘
```

**Data flow:**
1. Server reads objvars (`texture.url`, `texture.mode`, `texture.displayMode`, `texture.scrollH`, `texture.scrollV`)
2. Server writes values to `AutoDeltaVariable<std::string>` shared variables (`shared_np`)
3. Client receives values via baseline/delta packets
4. Client-side callbacks fire, setting a `dirty` flag
5. On next `alter()`, client fetches the image (async thread), decodes it, creates/updates overlay objects

---

## Object Variables (Objvars)

| Objvar | Type | Values | Description |
|--------|------|--------|-------------|
| `texture.url` | string | Any HTTP/HTTPS URL | Remote image URL (PNG, JPEG, GIF) |
| `texture.mode` | string | `"DEFAULT"`, `"IMAGE_ONLY"` | DEFAULT keeps parent visible; IMAGE_ONLY hides parent appearance |
| `texture.displayMode` | string | `"CUBE"`, `"FLAT"`, `"DOUBLE_SIDED"` | Geometry mode for the image overlay |
| `texture.scrollH` | string | `"0"`, `"0.1"`, `"0.25"`, `"0.5"`, `"1"`, `"-0.1"`, etc. | Horizontal texture scroll speed |
| `texture.scrollV` | string | `"0"`, `"0.1"`, `"0.25"`, `"0.5"`, `"1"`, `"-0.1"`, etc. | Vertical texture scroll speed |

---

## Condition

`C_magicPaintingUrl` — Added to `TangibleObject::Conditions` enum. When this condition is set on an object, the magic painting system activates. The condition gates all processing in both `alter()` and the various callbacks.

---

## Display Modes

- **CUBE** — Image rendered on the default overlay geometry (box-like), positioned 2m above the parent object.
- **FLAT** — Image rendered on a flat upright plane, standing vertically above the parent.
- **DOUBLE_SIDED** — Two flat planes back-to-back, showing the image from both front and back.

If the parent template name contains `"painting"`, the image replaces the main painting surface texture instead of spawning an overlay above it.

---

## Files Changed

### Server-Side C++ (`src/`)

| File | Change |
|------|--------|
| `src/engine/server/library/serverGame/src/shared/object/TangibleObject.h` | Added `C_magicPaintingUrl` condition constant. Added `AutoDeltaVariable<std::string>` members: `m_remoteTextureUrl`, `m_remoteTextureMode`, `m_remoteTextureDisplayMode`, `m_remoteTextureScrollH`, `m_remoteTextureScrollV`. |
| `src/engine/server/library/serverGame/src/shared/object/TangibleObject.cpp` | Added `updateRemoteTextureUrlFromObjvars()` — reads all `texture.*` objvars, provides defaults, updates AutoDelta variables. Attaches/detaches `terminal.magic_painting_url` script based on condition. Logging with `[Titan]` prefix. |
| `src/engine/server/library/codegen/package_data.txt` | Added `m_remoteTextureUrl`, `m_remoteTextureMode`, `m_remoteTextureDisplayMode`, `m_remoteTextureScrollH`, `m_remoteTextureScrollV` as `shared_np` `std::string` entries for `TangibleObject`. This is the **source of truth** for `Packager.cpp` generation. |
| `src/engine/server/library/serverGame/src/shared/generated/Packager.cpp` | **Generated file** — registers the new AutoDelta variables in the serialization system. Rebuilt from `package_data.txt`. |

### Client-Side C++ (`client/`)

| File | Change |
|------|--------|
| `client/src/engine/client/library/clientGame/src/shared/object/TangibleObject.h` | Added `Messages` structs and `Callbacks` typedefs for all five remote texture variables. Added `AutoDeltaVariableCallback` members. Declared `updateGifAnimation()`. |
| `client/src/engine/client/library/clientGame/src/shared/object/TangibleObject.cpp` | **Core implementation.** Contains: `RemoteImageRuntimeData` struct (texture, overlay objects, GIF frames, dirty/settled flags, cached state), `updateRemoteImageTexture()` (main update loop with dirty-flag optimization), `clearRemoteImageTexture()`, `updateGifAnimation()`, `decodeImageBytesToTexture()` (WIC decoding), `decodeGifFrames()` (multi-frame GIF via WIC), `ensureRemoteImageOverlayObjects()`, `createOverlayObject()`, `removeOverlayObject()`, `applyPictureOnlyPresentation()`, `applyCachedRuntimeTextureToSurfaces()`, `applyTextureScrollToAppearance()`, async WinHTTP fetch thread, COM initialization helper. |

### Appearance/Shader System (`client/`)

| File | Change |
|------|--------|
| `client/src/engine/shared/library/sharedObject/src/shared/appearance/Appearance.h` | Added `virtual void setTextureScroll(Tag, float, float)` to base class. |
| `client/src/engine/shared/library/sharedObject/src/shared/appearance/Appearance.cpp` | Empty base implementation of `setTextureScroll`. |
| `client/src/engine/client/library/clientObject/src/shared/appearance/MeshAppearance.h` | Added `setTextureScroll` override. |
| `client/src/engine/client/library/clientObject/src/shared/appearance/MeshAppearance.cpp` | Implements `setTextureScroll` — delegates to `ShaderPrimitiveSet`. |
| `client/src/engine/client/library/clientObject/src/shared/appearance/DetailAppearance.h` | Added `setTextureScroll` override. |
| `client/src/engine/client/library/clientObject/src/shared/appearance/DetailAppearance.cpp` | Implements `setTextureScroll` — delegates to current detail level. |
| `client/src/engine/client/library/clientObject/src/shared/appearance/ComponentAppearance.h` | Added `setTextureScroll` override. |
| `client/src/engine/client/library/clientObject/src/shared/appearance/ComponentAppearance.cpp` | Implements `setTextureScroll` — iterates child objects. |
| `client/src/engine/client/library/clientGraphics/src/shared/ShaderPrimitiveSet.h` | Changed `setTextureScroll` signature from `StaticShaderTemplate::TextureScroll` struct to raw floats (`u1, v1, u2, v2`). Removed `#include "StaticShaderTemplate.h"` from header. |
| `client/src/engine/client/library/clientGraphics/src/shared/ShaderPrimitiveSet.cpp` | Updated `setTextureScroll` implementations to construct `TextureScroll` struct internally from float args. |
| `client/src/engine/client/library/clientGraphics/src/shared/StaticShader.cpp` | Suppressed "Shader has no texture scrolls to set" warning — now silently returns when `m_textureScrollMap` is null (benign case for `defaultappearance.apt`). |

### Server-Side Appearance (build sync)

| File | Change |
|------|--------|
| `src/engine/shared/library/sharedObject/src/shared/appearance/Appearance.h` | Added `virtual void setTextureScroll(Tag, float, float)` (mirrors client). |
| `src/engine/shared/library/sharedObject/src/shared/appearance/Appearance.cpp` | Empty base implementation (mirrors client). |

### Java Scripts (`dsrc/`)

| File | Change |
|------|--------|
| `dsrc/sku.0/sys.server/compiled/game/script/terminal/magic_painting_url.java` | Radial menu script. Provides menus for: Set Mode (DEFAULT/IMAGE_ONLY), Set Display Mode (CUBE/FLAT/DOUBLE_SIDED), Set Scroll H, Set Scroll V. Uses `SERVER_MENU14`–`SERVER_MENU18` IDs. Cycles through predefined scroll values. |
| `dsrc/sku.0/sys.server/compiled/game/script/developer/bubbajoe/player_developer.java` | Added `/developer magicPaintingUrl <url>` command. Sets `CONDITION_MAGIC_PAINTING_URL`, `texture.url`, `texture.mode=IMAGE_ONLY`, `texture.displayMode=CUBE`, `texture.scrollH=0`, `texture.scrollV=0` on the targeted object. |
| `dsrc/sku.0/sys.server/compiled/game/script/base_class.java` | Defines `CONDITION_MAGIC_PAINTING_URL` constant for Java scripts. |

### God Client (`client/src/game/client/application/SwgGodClient/`)

| File | Change |
|------|--------|
| `SwgGodClient/src/GameWidget.cpp` | Added right-click context menu: `Set Condition ->` submenu listing all player-facing conditions. If `CONDITION_MAGIC_PAINTING_URL` is selected, prompts for image URL via `QInputDialog`. Fixed spacebar not working in chat input — now checks `CuiManager::getKeyboardInputActive()` before consuming `Key_Space`. |
| `SwgGodClient/src/MainFrame.cpp` | Template Editor creation changed to lazy (created on first use, not at startup). |
| `SwgGodClient/src/MainFrame.h` | Added `friend ActionsObjectTemplate` for Template Editor access. |
| `SwgGodClient/src/ActionsFileControl.cpp` | Template Editor opening now lazily creates the editor and calls `raise()`. |
| `SwgGodClient/src/ActionsObjectTemplate.cpp` | Fixed broken `onServerView`/`onServerEdit`/`onClientView`/`onClientEdit` — now open the Template Editor with the selected path instead of calling `doEditFile` with an empty string. |
| `SwgGodClient/src/TemplateEditorWindow.h` | New file — Template Editor dialog header. |
| `SwgGodClient/src/TemplateEditorWindow.cpp` | New file — Full template editor with server/shared/string panes, appearance preview, compile support. |

### Network Robustness

| File | Change |
|------|--------|
| `client/src/engine/shared/library/sharedNetwork/src/shared/networkhandler.cpp` | Added try-catch around `Archive::ReadException` in `NetworkHandler::dispatch`. Malformed packets are now dropped with a warning instead of crashing the client. |

### Logging

| File | Change |
|------|--------|
| `client/src/engine/shared/library/sharedDebug/src/shared/Report.cpp` | Modified `Report::vprintf` to prepend `"[Titan] "` to all log output via direct buffer manipulation. |
| `src/engine/shared/library/sharedDebug/src/shared/Report.cpp` | Same `[Titan]` prefix change (server-side mirror). |

### Bug Fixes (Side Quests)

| File | Change |
|------|--------|
| `client/src/engine/shared/library/sharedMath/src/shared/Sphere.h` | Clamps negative radius to 0.0f in constructors (suppresses "Sphere has negative radius!" warning on scaled objects). |
| `src/engine/shared/library/sharedMath/src/shared/Sphere.h` | Same negative radius fix (server-side mirror). |
| `src/engine/server/library/serverGame/src/shared/object/ServerObject.cpp` | Downgraded "Object already has a far update volume" from `WARNING_STRICT_FATAL` to `DEBUG_REPORT_LOG`. |
| `dsrc/sku.0/sys.server/compiled/game/script/DiscordWebhook.java` | New file — proper `DiscordWebhook` script class (fixes `NoClassDefFoundError`). |
| `dsrc/sku.0/sys.server/compiled/game/script/theme_park/nym/nym_elevator_down.java` | Added `!exists(building)` guard to `getCellId` call (fixes `JavaLibrary::getCellId passed invalid object`). |

---

## Key Implementation Details

### Image Fetching (WinHTTP)
- Async thread via `_beginthreadex`
- `WinHttpOpen` / `WinHttpConnect` / `WinHttpOpenRequest` / `WinHttpSendRequest` / `WinHttpReceiveResponse`
- Raw bytes stored in `std::vector<unsigned char>` on the runtime data
- `InterlockedExchange` flags for thread-safe ready signaling

### Image Decoding (WIC)
- `IWICImagingFactory` / `IWICBitmapDecoder` / `IWICFormatConverter`
- Converts to `GUID_WICPixelFormat32bppBGRA` for Direct3D compatibility
- GIF: `GetFrameCount()` for multi-frame, per-frame metadata for delays
- COM initialized once via `ensureComInitialized()` helper

### Performance Optimization
- **Dirty flag system**: `RemoteImageRuntimeData::dirty` and `settled` flags
- Callbacks set `dirty=true`, `settled=false` on value changes
- `updateRemoteImageTexture()` early-exits when `settled && !dirty && !fetchInProgress`
- Expensive parsing (string ops, `atof`, `_stricmp`) only runs when `dirty`
- `isPaintingTemplate` resolved once and cached
- `updateGifAnimation()` directly sets texture on active appearance (avoids redundant surface scan)
- Overlay recreation only on display mode change (`forceRecreate` flag)

### Object Lifecycle
- Overlays created as child `Object` instances attached to the parent `TangibleObject`
- `removeOverlayObject()` handles detach → removeFromWorld → delete safely
- `clearRemoteImageTexture()` called on `removeFromWorld()` and destructor
- GIF frame textures tracked separately; `runtimeData.texture` set to null before `clearGifFrames()` to prevent double-free
- `scheduleForAlter()` keeps the object updating during async fetch and GIF playback

### Persistence
All settings stored as objvars on the server and synchronized via `AutoDeltaVariable<std::string>` (`shared_np`). On object load, the server reads objvars and populates the shared variables, which the client receives and uses to restore the full rendering state.

---

## Developer Usage

```
/developer magicPaintingUrl https://example.com/image.png
```
Target an object, run the command. Sets condition + all objvars. The object immediately begins rendering the remote image.

Radial menu on the object provides:
- **Set Mode** — Toggle DEFAULT / IMAGE_ONLY
- **Set Display** — Cycle CUBE → FLAT → DOUBLE_SIDED
- **Set Scroll H** — Cycle through scroll values
- **Set Scroll V** — Cycle through scroll values

God Client right-click → **Set Condition** → **Magic Painting URL** → Enter URL dialog.
