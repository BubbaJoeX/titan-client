# Linking libVLC into the client runtime

The game loads **libVLC** at runtime from a supported external runtime layout. DLLs are **not** linked at compile time beyond vendored headers, and the repository/staging scripts do not download or redistribute VLC binaries.

---

## Version and architecture

| Requirement | Value |
|-------------|--------|
| **libVLC line** | **3.0.22** — matches the SDK under `client/src/external/3rd/library/vlc-3.0.22/sdk/include/vlc/` |
| **CPU** | Must match the executable: use **win64/x64** VLC for the x64 client and **win32/x86** VLC for the legacy client. |

Mixing architectures fails with `ERROR_BAD_EXE_FORMAT`; the client log now identifies this explicitly.

---

## What the repository contains

Under `client/src/external/3rd/library/vlc-3.0.22/` you get:

- **SDK headers** (`sdk/include/vlc/vlc.h`, etc.) for compiling `clientGame`.
- Supporting tree (e.g. Lua HTTP assets, docs).

That folder does **not** include Windows **`libvlc.dll`** / **`libvlccore.dll`** or the **`plugins`** tree. Those come from an official **VideoLAN VLC 3.0.22** Windows build.

---

## Where files must go

The loader checks these locations in order:

1. Directory named by the `SWG_VLC_RUNTIME` environment variable.
2. `<client-exe-directory>\vlc\`
3. `<client-exe-directory>\runtime\vlc\`
4. The client executable directory (legacy layout).

`libvlc.dll`, `libvlccore.dll`, their matching dependency DLLs, and the complete `plugins\` directory must come from the same architecture/versioned distribution. The loader uses the DLL's directory for dependent-DLL resolution and passes its `plugins\` path explicitly to libVLC.

---

## Obtaining VLC 3.0.22 for Windows

1. Obtain an approved VLC **3.0.22** portable artifact from VideoLAN (see [VideoLAN releases](https://www.videolan.org/vlc/releases/)) matching the client architecture.
2. Prefer the **portable / zip** layout so you can copy files without running an installer into `Program Files`.

Unpack it to a temporary path. You should see **`libvlc.dll`**, **`libvlccore.dll`**, and a **`plugins`** directory at the root of that build (exact layout can vary slightly by package).

---

## Files and folders to copy

### Minimum (usual layout)

Copy into one supported runtime directory, preferably `<client-root>\vlc\`:

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

Some official builds ship extra DLLs beside `libvlc.dll` (dependencies). If loading still fails, keep all root DLLs from the approved matching-architecture package together in the selected VLC runtime directory.

---

## Example layout after copying

```text
<client-root>/
  SwgClient_r.exe
  vlc/
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
- Place a compatible approved **`yt-dlp.exe`** next to the client executable or on `PATH`.

**YouTube streaming:** YouTube is known for blocking or breaking **external** playback (anything not using their official players or embed rules). There is no stable contract for third-party tools. **It is up to you to keep Lua scripts and related streaming components up to date**—for example refreshing VLC’s Lua/plugin pieces under your deployed tree when VideoLAN ships fixes, and upgrading **`yt-dlp.exe`** when extractors change—so URL resolution and playback keep working as long as YouTube allows them.

---

## Verification

- Confirm the runtime architecture matches the executable (`dumpbin /headers libvlc.dll`).
- Confirm **`libvlc.dll`** and **`libvlccore.dll`** share the selected runtime directory.
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
