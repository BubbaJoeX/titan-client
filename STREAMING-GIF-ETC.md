# Streaming, Video, and Media Objects

This document describes the requirements and architecture for the Magic Painting, Magic Video Player, and Speaker Emitter systems.

---

## Table of Contents

1. [Magic Painting (Static Image/GIF)](#1-magic-painting-static-imagegif)
2. [Magic Video Player (Video Streaming)](#2-magic-video-player-video-streaming)
3. [Speaker Emitter (Audio)](#3-speaker-emitter-audio)
4. [Client Dependencies](#4-client-dependencies)
5. [Condition Flags](#5-condition-flags)
6. [AutoDelta Variables (Server-to-Client Sync)](#6-autodelta-variables-server-to-client-sync)
7. [Scripts](#7-scripts)
8. [Object Templates](#8-object-templates)
9. [Admin / God Client Setup](#9-admin--god-client-setup)

---

## 1. Magic Painting (Static Image/GIF)

Displays a remote image (PNG, JPG, GIF) on a painting object's surface.

### Condition

- `C_magicPaintingUrl` / `CONDITION_MAGIC_PAINTING_URL` (`0x20000000`)

### Required Objvars

| Objvar | Type | Required | Description |
|---|---|---|---|
| `texture.url` | `string` | **Yes** | URL to the image (PNG, JPG, GIF). Must be a direct link to the image file. |

### Optional Objvars

| Objvar | Type | Default | Values | Description |
|---|---|---|---|---|
| `texture.mode` | `string` | `IMAGE_ONLY` | `IMAGE_ONLY`, `IMAGE_ONLY_TWO_SIDED`, `DEFAULT` | Controls how the texture is applied to the object mesh. |
| `texture.displayMode` | `string` | `CUBE` | `CUBE`, `FLAT`, `DOUBLE_SIDED` | Controls the geometry used for rendering. |
| `texture.scrollH` | `string` | `0` | `0`, `0.1`, `0.25`, `0.5`, `1.0`, `-0.1`, `-0.25`, `-0.5`, `-1.0` | Horizontal scroll speed for the texture. |
| `texture.scrollV` | `string` | `0` | Same as scrollH | Vertical scroll speed for the texture. |

### How It Works

1. Set the `CONDITION_MAGIC_PAINTING_URL` condition on any `TangibleObject`.
2. Set the `texture.url` objvar to a direct image URL.
3. The server's `updateRemoteTextureUrlFromObjvars()` reads the objvars and syncs them to the client via AutoDelta variables (`m_remoteTextureUrl`, `m_remoteTextureMode`, etc.).
4. The client's `updateRemoteImageTexture()` downloads the image and applies it as a texture to the object's appearance.
5. GIF images are supported with animated frame playback via `updateGifAnimation()`.
6. The script `terminal.magic_painting_url` is auto-attached when the condition and URL are present, providing a radial menu to cycle painting mode, display mode, and scroll values.

---

## 2. Magic Video Player (Video Streaming)

Plays video from a URL (direct MP4/stream or YouTube/Vimeo via yt-dlp resolution) on a painting or tangible object.

### Condition

- `C_magicVideoPlayer` / `CONDITION_MAGIC_VIDEO_PLAYER` (`0x40000000`)

### Required Objvars

| Objvar | Type | Required | Description |
|---|---|---|---|
| `stream.url` | `string` | **Yes** | URL to the video. Supports direct URLs (`.mp4`, `.webm`, etc.) and platform URLs (YouTube, Vimeo) which are resolved via `yt-dlp`. |

### Optional Objvars

| Objvar | Type | Default | Values | Description |
|---|---|---|---|---|
| `timestamp` | `string` | `"0"` | Seconds as string | Seek position in seconds from the start of the video. |
| `stream.loop` | `string` | `"0"` | `"0"` (off), `"1"` (on) | Whether the video loops. Sets VLC `:input-repeat=65535`. |
| `stream.aspect` | `string` | `"4:3"` | `"4:3"`, `"16:9"` | Aspect ratio. The painting template is natively 4:3. Selecting 16:9 scales the object's Y axis to 0.75 to match. |

### How It Works

1. Set the `CONDITION_MAGIC_VIDEO_PLAYER` condition on any `TangibleObject`.
2. Set the `stream.url` objvar to a video URL.
3. The server's `updateRemoteVideoStreamFromObjvars()` reads the objvars and syncs them to the client via AutoDelta variables.
4. On the client:
   - `loadVlcApi()` dynamically loads `libvlc.dll` at runtime.
   - If the URL is a platform URL (contains `youtube.com`, `youtu.be`, `vimeo.com`, etc.), `yt-dlp.exe` is launched in a background thread to resolve the direct stream URL.
   - A VLC media player instance is created with the resolved URL.
   - Video frames are rendered at 640x360 (16:9) into a CPU buffer via VLC's `smem` callbacks (`videoLockCallback`, `videoUnlockCallback`, `videoDisplayCallback`).
   - Each frame is uploaded to a Direct3D texture (`TF_ARGB_8888`) and applied to the object's appearance surfaces.
   - If the object is a painting template, the texture replaces the painting's main texture. Otherwise, overlay objects are created.
5. The script `terminal.magic_video_player` is auto-attached, providing a "Video Management" radial menu.

### Play/Stop Behavior

- **Stop Video**: Clears `CONDITION_MAGIC_VIDEO_PLAYER` only. All objvars (URL, timestamp, loop, aspect) are preserved.
- **Play Video**: Re-sets `CONDITION_MAGIC_VIDEO_PLAYER`. The server re-syncs AutoDelta variables on the next `alter()` cycle, and the client resumes playback.
- **Persistence**: The condition and objvars survive server restarts and client disconnects. On reconnect, `addToWorld()` triggers `updateRemoteVideoStream()` to reinitialize playback.

### URL Resolution (yt-dlp)

For non-direct URLs (YouTube, Vimeo, etc.), the client spawns a background thread that runs:

```
yt-dlp.exe --get-url -f "best[height<=720]" --no-playlist "<url>"
```

- `yt-dlp.exe` must be present in the game client's executable directory (e.g., `exe/win32_rel/`).
- stderr is redirected to NUL to prevent warnings from corrupting the resolved URL.
- Resolution is asynchronous; the client polls via `scheduleForAlter()` until the thread completes.

---

## 3. Speaker Emitter (Audio)

A separate world object (`speaker_s01.iff`) that acts as a positional audio emitter linked to a video player. VLC handles audio output directly; the emitter controls volume based on distance and cell occlusion.

### Required Objvars

| Objvar | Type | Required | Description |
|---|---|---|---|
| `video_emitter.parent_id` | `string` | **Yes** | The NetworkId (as string) of the parent video player object this speaker is linked to. |

### How It Works

1. A speaker is spawned from the video player's radial menu ("Spawn Speaker") at the player's location.
2. The `video_emitter.parent_id` objvar is set to the video player's object ID.
3. The server detects this objvar in `updateRemoteVideoStreamFromObjvars()` and auto-attaches the `terminal.magic_video_emitter` script.
4. The client syncs `m_remoteEmitterParentId` via AutoDelta.
5. On the client, `updateVideoEmitterAudio()` runs every frame for emitter objects:
   - Looks up the parent video player by NetworkId.
   - Calculates distance from the local player to the speaker.
   - Applies volume based on distance and cell/building occlusion.

### Audio Volume Rules

| Condition | Volume |
|---|---|
| Distance <= 5m | 100% |
| Distance 5m - 32m | Linear falloff from 100% to 0% |
| Distance > 32m | 0% (muted) |
| Same cell as speaker | No additional attenuation |
| Different cell, same building | 40% of calculated volume |
| Different building or world vs interior | 5% of calculated volume |

### Radial Menu (Speaker)

The `terminal.magic_video_emitter` script provides:
- **Linked to: (id)** — Shows which video player this speaker is linked to.
- **Destroy Speaker** — Removes the speaker object (restricted to God/Master Entertainer).

---

## 4. Client Dependencies

### libVLC 3.0.22

- **Location**: `client/src/external/3rd/library/vlc-3.0.22/sdk/include/vlc/vlc.h` (header with typedefs)
- **Runtime**: `libvlc.dll` and `libvlccore.dll` must be in the game client's executable directory, along with the VLC `plugins/` folder.
- **Loading**: Dynamic via `LoadLibraryA("libvlc.dll")` and `GetProcAddress`.
- **Note**: All `libvlc_time_t` values use `__int64` (not `int64_t`) for 32-bit compilation compatibility.

### yt-dlp

- **Runtime**: `yt-dlp.exe` must be in the game client's executable directory.
- **Purpose**: Resolves platform URLs (YouTube, Vimeo, etc.) to direct stream URLs.
- **Download**: https://github.com/yt-dlp/yt-dlp/releases

### VLC Plugins Required

At minimum, the following VLC plugin categories should be present in `plugins/`:

- `access/` — Network access (HTTP, HTTPS)
- `codec/` — Video/audio decoders (H.264, AAC, etc.)
- `demux/` — Container demuxers (MP4, MKV, etc.)
- `video_output/` — Not needed (using smem callbacks)
- `audio_output/` — Required for speaker audio playback

---

## 5. Condition Flags

Defined in `TangibleObject.h` (both server and client):

| Condition | Hex Value | Purpose |
|---|---|---|
| `C_magicPaintingUrl` / `CONDITION_MAGIC_PAINTING_URL` | `0x20000000` | Enables magic painting (static image/GIF) |
| `C_magicVideoPlayer` / `CONDITION_MAGIC_VIDEO_PLAYER` | `0x40000000` | Enables magic video player (video streaming) |

---

## 6. AutoDelta Variables (Server-to-Client Sync)

Defined in `package_data.txt` as `shared_np` (non-persisted shared, synced to client):

### Magic Painting

| Variable | Type | Source Objvar |
|---|---|---|
| `m_remoteTextureUrl` | `std::string` | `texture.url` |
| `m_remoteTextureMode` | `std::string` | `texture.mode` |
| `m_remoteTextureDisplayMode` | `std::string` | `texture.displayMode` |
| `m_remoteTextureScrollH` | `std::string` | `texture.scrollH` |
| `m_remoteTextureScrollV` | `std::string` | `texture.scrollV` |

### Magic Video Player

| Variable | Type | Source Objvar |
|---|---|---|
| `m_remoteStreamUrl` | `std::string` | `stream.url` |
| `m_remoteStreamTimestamp` | `std::string` | `timestamp` |
| `m_remoteStreamLoop` | `std::string` | `stream.loop` |
| `m_remoteStreamAspect` | `std::string` | `stream.aspect` |
| `m_remoteEmitterParentId` | `std::string` | `video_emitter.parent_id` |

---

## 7. Scripts

### Auto-Attached Scripts

These scripts are automatically attached/detached by the server C++ code when the relevant conditions and objvars are present:

| Script | Trigger | Purpose |
|---|---|---|
| `terminal.magic_painting_url` | `C_magicPaintingUrl` + `texture.url` | Radial menu for painting mode, display, scroll |
| `terminal.magic_video_player` | `C_magicVideoPlayer` + `stream.url` | Radial menu for video URL, timestamp, loop, aspect, play/stop, spawn speaker |
| `terminal.magic_video_emitter` | `video_emitter.parent_id` objvar present | Radial menu for speaker info and destroy |

### Manually Attached Script

| Script | Purpose | Skill Required |
|---|---|---|
| `player.player_video_player` | Attach to any tangible object to allow Master Entertainers to convert it into a video player via radial menu | `class_entertainer_phase4_master` |

---

## 8. Object Templates

| Template | Purpose |
|---|---|
| `object/tangible/painting/painting_starmap.iff` | Recommended painting template for video players (4:3 native aspect) |
| `object/tangible/loot/misc/speaker_s01.iff` | Speaker object used as audio emitter |

---

## 9. Admin / God Client Setup

### Spawning a Video Player (God Client / Developer Command)

```
/developer spawnTelevision <url> [scale]
```

This creates a `painting_starmap.iff` with `stream.url`, `timestamp=0`, `stream.loop=1`, and `CONDITION_MAGIC_VIDEO_PLAYER` set.

### Manual Setup via Objvars

1. Create or select any `TangibleObject` (paintings work best).
2. Set objvars:
   - `stream.url` = `"https://example.com/video.mp4"`
   - `timestamp` = `"0"`
   - `stream.loop` = `"1"`
   - `stream.aspect` = `"4:3"` or `"16:9"`
3. Set condition: `CONDITION_MAGIC_VIDEO_PLAYER` (`0x40000000`)
4. The `terminal.magic_video_player` script auto-attaches, providing the radial menu.

### Spawning a Speaker

Use the "Spawn Speaker" option from the video player's radial menu, or manually:

1. Create a `speaker_s01.iff` object.
2. Set objvar: `video_emitter.parent_id` = `"<video_player_network_id>"`
3. The `terminal.magic_video_emitter` script auto-attaches.

### Client Requirements Checklist

- [ ] `libvlc.dll` in exe directory
- [ ] `libvlccore.dll` in exe directory
- [ ] VLC `plugins/` folder in exe directory
- [ ] `yt-dlp.exe` in exe directory (for YouTube/Vimeo support)
