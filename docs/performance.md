# Client Frame Rate

What limits the frame rate of `SwgTitan` and `SwgGodClient` (x64), what was changed in the October 2026 pass, the settings that matter, and how to measure.

## What was changed

| Area | Problem | Change |
|------|---------|--------|
| Frame limiter (`sharedFoundation` `Clock`) | `Clock::limitFrameRate` slept with `Sleep()`. Windows 10 2004+ rounds that up to the 15.6 ms system tick unless the process asks for a finer timer resolution, so a 144 fps limit settled near 60 fps and frame times were uneven. | `Clock::install` calls `timeBeginPeriod(1)` and creates a high-resolution waitable timer. The limiter sleeps on it until 0.3 ms before the deadline, then spins the rest. |
| Windowed presentation (`Direct3d9`) | Windowed and borderless windows presented with `D3DPRESENT_INTERVAL_ONE`. DWM already keeps windowed output tear-free, so the extra vblank wait only quantized the frame rate: a frame that missed a refresh waited for the next one. | Windowed presents are immediate, and the frame limiter paces the loop. `[Direct3d9] windowedVsync=1` restores the old behaviour; fullscreen still follows `allowTearing`. |
| Skinning (`clientSkeletalAnimation`) | Every character is skinned on the CPU. The fast path was x86 inline assembly, which is compiled out on x64, so the x64 clients ran the scalar fallback through the vertex iterator. | SSE2 intrinsics (always available on x64) for soft, hard and dot3 skinning. A soft-skinned vertex sums its weighted bone matrices once, then transforms position, normal and dot3 with the sum. Output matches the scalar path within 5e-7. In isolation it is 6.6x faster with Release flags. |
| Character LOD (`clientSkeletalAnimation`) | The LOD hysteresis added margins of 20% of the band width. The outer bands are open-ended (`FLT_MAX`), so their margin was infinite, and a character never left LOD 0 or the lowest detail level it starts at. On x64 this was hidden by forcing every character to LOD 0 at any distance (`forceHighestLod`), so distant crowds were skinned and drawn at full detail. | The margin now scales the band edges (±20%). `forceHighestLod` defaults to false on every platform. Checked with `lod_sweep.py`: one NPC is drawn at LOD 0 up to 12 m, LOD 1 at 20 m and LOD 2 at 35-60 m, and returns to LOD 0 on the way back. |
| Mouse look (`clientGraphics`) | Mouse look re-centres the OS cursor every frame with `SetCursorPos`, a slow system call (2-3% of the frame in a profile), even when the cursor is already there. | The call is skipped when `GetCursorPos` already reports the target. |
| Static meshes (`clientObject` `MeshAppearance`) | The portal-cut work for dynamic bunkers ran in `render()` and `alter()` of every mesh every frame, even with no cuts anywhere: a matrix inverse, an owner and cell walk, and map lookups. | It returns at once when the mesh has no pending cut and no building has registered cuts. |
| God client loop (`GameWidget`) | The game loop ran from a 5 ms `QTimer`, which fires on the 15.6 ms tick, behind a 16 ms minimum-frame gate. A 60 fps limit ran at about 32 fps. | A zero-length timer drives the loop, and the `Clock` limiter paces it to `[GodClient] frameRateLimit`. The inactive (half rate) and hidden (1 fps) throttles stay on the timer. |
| God client object list (`TreeBrowser`) | Every world `OBJECT_ADDED` and `OBJECT_REMOVED` rebuilt the object list. Each rebuild walked every world object, built a buildout-name string per object and diffed the list with nested linear searches. Zone loads and busy areas stalled the editor. | Additions within 250 ms are refreshed together. A removal only drops the items whose objects left the world. A refresh uses sets and compares buildout areas by pointer. |
| Compiler (`Directory.Build.targets`) | Every `Release|x64` project pinned `/Ob1`, so only functions declared `inline` were inlined. | `Release|x64` uses `/Ob2`, the setting `/O2` normally implies. |
| GPU selection (exe entry points) | On NVIDIA Optimus and AMD PowerXpress laptops, a Direct3D 9 game without the vendor exports runs on the integrated GPU. | `SwgTitan` and `SwgGodClient` export `NvOptimusEnablement` and `AmdPowerXpressRequestHighPerformance`. |

## Settings

| Key | Default | Notes |
|-----|---------|-------|
| `[SharedFoundation] frameRateLimit` | 144 in `SwgTitan` (the local `user.cfg` sets 60) | The cap the `Clock` limiter holds. 0 means no cap. |
| `[GodClient] frameRateLimit` | 60 in `SwgGodClient.cfg` | The god client's cap. An inactive window runs at half of it. |
| `[Direct3d9] windowedVsync` | false | Wait for vblank in windowed and borderless modes. |
| `[Direct3d9] allowTearing` | false | Immediate presents in exclusive fullscreen. |
| `[ClientGraphics] borderlessWindow` / `windowed` | borderless | DWM composes windowed output at the display refresh rate, so a window cannot show more than one frame per refresh. Exclusive fullscreen with `allowTearing=1` is the only way to go higher. |
| `[ClientSkeletalAnimation] forceHighestLod` | false | Draw every character at LOD 0 regardless of distance. |
| `[ClientSkeletalAnimation] lodManagerEnable` | true (the local `user.cfg` sets 0) | Extra LOD downgrades when many characters are on screen. |

## Remaining x64 workarounds

The x64 port turned off three more character optimizations for correctness, and each one still costs frame rate in crowds:

- The distant-character batch renderer is off, because its vertex format drops wearable UVs.
- Skinning is always soft. Small characters do not switch to hard skinning or skip skinning.
- Every-other-frame skinning is off. The code comment reports stale vertex buffer bytes and vertex spikes.

Re-enable and test them one at a time. `client_bench.py` reports how many characters were drawn at each LOD, and `lod_sweep.py` reports one character's LOD as the camera moves away and back.

## Measuring

`tools/perf` drives the development client (`SwgTitan_d`, the DevBridge build) into the offline Load Scene:

```powershell
python tools\perf\client_bench.py mylabel -s Direct3d9 allowTearing=1
python tools\perf\client_bench.py mylabel --profile 12        # also samples stacks for analyze.py
python tools\perf\analyze.py tools\perf\stacks_mylabel.pkl 60
python tools\perf\analyze.py tools\perf\stacks_mylabel.pkl 30 SoftwareBlendSkeletalShaderPrimitive::prepareToDraw
```

The benchmark loads offline Tatooine and spawns 64 dressed NPCs, eight wandering dewbacks, a cantina and two houses. For an aerial view and a street-level view it reports:

- `fps`;
- `cpu_ms_per_frame`, the main thread's CPU time per frame;
- `work_ms_per_frame`, frame time outside `Graphics::present` and the limiter, from stack samples;
- the number of characters drawn at each LOD.

Compare work and CPU times rather than fps. In a window, DWM throttles presents to its composition rate, and that rate drops when the display sleeps or the window is hidden. The driver can also spin inside `Present`. `sampler.py` is a dbghelp stack sampler that needs nothing installed; set `SWG_SYMBOL_PATH` to a folder with the matching PDB when profiling an older exe.

## Results (2026-10-24)

Measured on an i7-14650HX with an RTX 5050 Laptop GPU and 16 GB of RAM. The runs used the Debug|x64 client in the 64-NPC crowd scene, with the local `user.cfg` (60 fps cap, LOD manager off).

Share of the main thread's frame work, from stack samples within each run:

| | Old build | New build |
|---|---|---|
| Skinning (`SoftwareBlendSkeletalShaderPrimitive::prepareToDraw`) | 31% | 11% |
| Vertex loop (`fillVertexBuffer`) | 26% | 2.5% |

Characters drawn per LOD in the street view:

| | Old build | New build |
|---|---|---|
| LOD 0 | all 72 | 8 dewbacks |
| LOD 1 | none | 5 NPCs |
| LOD 2 | none | 59 NPCs |

- With LOD still forced to 0 and only the SSE2 skinning added, main-thread CPU per frame fell from 19-22 ms to 15-17 ms in comparable runs.
- The SSE2 loop on its own is 6.6x faster than the scalar loop under Release flags (`/O2`), and 2.9x faster under Debug flags.

Absolute fps on this machine depends on the display state, because DWM throttles windowed presents. It also depends on other builds running at the same time, so compare work shares or CPU time.

The Release client has no DevBridge, and the god client's active-window frame rate was not measured. Release gains are estimated from the Debug runs and the microbenchmark, not measured in game. With its window inactive, the Release god client ran at 22-31 fps, which is the intended half rate of its 60 fps cap (the old loop's half rate was about 21 fps).
