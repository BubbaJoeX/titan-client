# Linking libVLC into the client runtime

The game loads **libVLC** at runtime (`LoadLibraryA("libvlc.dll")` from `clientGame`). DLLs are **not** linked at compile time beyond vendored **headers**; you must deploy matching **Windows binaries** next to `SwgTitan_r.exe`.

---

## Version and architecture

| Requirement | Value |
|-------------|--------|
| **libVLC line** | **3.0.22** — matches the SDK under `client/src/external/3rd/library/vlc-3.0.22/sdk/include/vlc/` |
| **CPU** | **32-bit (x86)** — the shipped client is **Win32**. Use the **win32** VLC package, not win64. |

Mixing 64-bit VLC DLLs with a 32-bit exe causes **`0xC000007B`** (invalid image format).

---

## What the repository contains

Under `client/src/external/3rd/library/vlc-3.0.22/` you get:

- **SDK headers** (`sdk/include/vlc/vlc.h`, etc.) for compiling `clientGame`.
- Supporting tree (e.g. Lua HTTP assets, docs).

That folder does **not** include Windows **`libvlc.dll`** / **`libvlccore.dll`** or the **`plugins`** tree. Those come from an official **VideoLAN VLC 3.0.22** Windows build.

---

## Where files must go

Put runtime pieces in the **same directory as the game executable** (the process directory used for `LoadLibrary`), typically:

```text
<repo>/exe/win32_rel/
```

If you run from another staging folder, copy VLC there instead—**paths are relative to the exe**, not to `client/`.

---

## Obtaining VLC 3.0.22 for Windows (32-bit)

1. Download a **Windows 32-bit** VLC **3.0.22** artifact from VideoLAN, for example the zip/7z **win32** bundle from the official VLC release area (see [VideoLAN releases](https://www.videolan.org/vlc/releases/)).
2. Prefer the **portable / zip** layout so you can copy files without running an installer into `Program Files`.

Unpack it to a temporary path. You should see **`libvlc.dll`**, **`libvlccore.dll`**, and a **`plugins`** directory at the root of that build (exact layout can vary slightly by package).

---

## Files and folders to copy

### Minimum (usual layout)

Copy into **`exe/win32_rel/`** (next to `SwgTitan_r.exe`):

1. **`libvlc.dll`**
2. **`libvlccore.dll`**
3. **`plugins/`** — copy the **entire** directory tree (all subfolders).

The engine expects plugin modules under `plugins/` relative to where **`libvlc.dll`** lives. At minimum, playback needs modules under categories such as:

- **`plugins/access/`** — HTTP/HTTPS access
- **`plugins/codec/`** — decoders (e.g. H.264, AAC)
- **`plugins/demux/`** — container demuxers (MP4, MKV, …)
- **`plugins/audio_output/`** — speaker / audio path

Video uses custom memory callbacks for picture output; **`plugins/video_output/`** is typically **not** required for in-world texture playback, but copying the **full** `plugins` tree avoids subtle missing-module failures.

### If playback still fails

Some official builds ship extra DLLs beside `libvlc.dll` (dependencies). If something still fails to load, copy **all** `.dll` files from the root of the official VLC **win32** package into `exe/win32_rel/` alongside `SwgTitan_r.exe`, not only the two core DLLs.

---

## Example layout after copying

```text
exe/win32_rel/
  SwgTitan_r.exe
  libvlc.dll
  libvlccore.dll
  plugins/
    access/
    audio_filter/
    audio_mixer/
    audio_output/
    codec/
    demux/
    ...
```

---

## Related: `yt-dlp.exe` (streaming URLs)

For **YouTube / Vimeo**–style URLs the client shells **`yt-dlp.exe`** from the **same executable directory**. That is separate from VLC’s DLLs.

- Download: [yt-dlp releases](https://github.com/yt-dlp/yt-dlp/releases)  
- Place **`yt-dlp.exe`** next to `SwgTitan_r.exe` (32-bit Windows build if multiple artifacts are offered).

**YouTube streaming:** YouTube is known for blocking or breaking **external** playback (anything not using their official players or embed rules). There is no stable contract for third-party tools. **It is up to you to keep Lua scripts and related streaming components up to date**—for example refreshing VLC’s Lua/plugin pieces under your deployed tree when VideoLAN ships fixes, and upgrading **`yt-dlp.exe`** when extractors change—so URL resolution and playback keep working as long as YouTube allows them.

---

## Verification

- Confirm **`libvlc.dll`** and **`libvlccore.dll`** sit next to **`SwgTitan_r.exe`**.
- Confirm **`plugins`** exists beside those DLLs with populated subfolders.
- Launch the client from that folder (working directory = exe directory is the usual setup).
- Exercise magic video / streaming features in-game; failures often log as missing DLL or missing plugin module.

---

## Licensing

VLC is licensed under **LGPL/GPL** (see **`COPYING.txt`** / **`COPYING.LIB`** in the official VLC package). Ship license texts with public distributions if your legal process requires it.

---

## See also

- [STREAMING-GIF-ETC.md](STREAMING-GIF-ETC.md) — Magic painting / magic video / speaker emitter behavior and checklist.
- [COMPILE_GUIDE_CLIENT.md](COMPILE_GUIDE_CLIENT.md) — Building the client and default `exe/win32_rel` staging.
