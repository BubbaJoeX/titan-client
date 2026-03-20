# SwgCameraClient

Lean executable forked from **SwgHeadlessClient** for future **satellite / map capture** (ortho tiles, structure passes, etc.). Same engine setup as the headless client: minimal UI package, headless graphics via **`ClientGraphics` `headless=true`** (set in code before `SetupClientGraphics::install`, or in `client.cfg` / override), audio disabled at shutdown.

## vs full client — what you should notice

**In-world, gameplay is the same** until you turn on map capture. The fork is meant to log in and run the same **GroundScene** as SwgClient; the extra behavior is **opt-in** via config.

| Always different (this exe) | Only when `mapCaptureEnabled=true` |
|-----------------------------|-------------------------------------|
| Window title **SwgCameraClient**, log name **SwgCameraClient**, mutex **SwgCameraClientInstanceRunning** | Switches to free camera, ortho tile grid, `Graphics::screenShot` batch, GOT draw filter, fog/particle/weather suppression for captures |
| No **SplashScreen** / **VideoList** / **externalCommandHandler** (smaller `ClientMain` than SwgClient) | Writes under **`mapCaptureOutputDirectory`** (e.g. `mapcapture/`) |
| **Audio** forced off at shutdown | |

So if nothing “camera” happens, check **`[SwgCameraClient]` `mapCaptureEnabled`** in **`exe/win32_rel/swgcamera_client_base.cfg`** or **`misc/override.cfg`** — default is **`false`** so the world looks normal.

**Troubleshooting (enabled but no tiles / no top-down view):** Output goes to **`mapCaptureOutputDirectory`** relative to the **process current working directory** (often `exe\win32_rel`, but Visual Studio / shortcuts may use another cwd — search the disk for `mapcapture\`). Rebuild **`clientGame`** after changing map capture code. In **`SwgCameraClient`**.log look for **`SwgCameraMapCapture: batch started`**; a line about **`Graphics::screenShot failed`** usually means window/desktop capture restrictions or a bad path.

**Config gotcha:** `ConfigFile::getKeyBool("SwgCameraClient", …)` used to **create a fake `[SwgCameraClient]` section** with `mapCaptureEnabled=false` when the real section was missing (e.g. bad cwd so `.include` failed). Map capture now reads **`[SwgCameraClient]` only via `getSection()`** (no lazy section) and **also** honors **`[ClientGame] mapCaptureEnabled`** — `exe/win32_rel/client.cfg` appends that so the switch still turns on if the include path is wrong. **`Game::isSpace()`** is no longer used to block capture (space zones still use `GroundScene` in this engine).

## Build (Visual Studio)

1. **`swg.sln`** lists **SwgCameraClient** with **project dependencies** on **clientGame**, **clientGraphics**, and **sharedObject** so they build first. The **SwgCameraClient** `.vcxproj` also has **ProjectReferences** (build order only; libs stay explicit in `AdditionalDependencies`).
2. Set **Win32** and **Debug** / **Release** to match your other game binaries.
3. Place **`swg.ico`** next to `src/win32/SwgCameraClient.rc` (same icon as **SwgClient** — copy from your existing client tree if it is not in source control).
4. Build outputs go under `src/compile/win32/SwgCameraClient/<Config>/` as `SwgCameraClient_d.exe` / `_r.exe` / `_o.exe` (per configuration).

**Linker unresolved externals** (`Camera::setPerspectiveProjection`, `ObjectList::setObjectRenderSkipPredicate`, `ConfigClientGraphics::getHeadless`, etc.): those symbols live in **clientGraphics** / **sharedObject** / **clientGame** static libs. **Rebuild** those three projects (or the whole solution) **before** linking **SwgCameraClient** so `client\src\compile\win32\...\*.lib` match current sources. Stale `clientGraphics.lib` or `sharedObject.lib` is the usual cause.

After pulling engine changes, **rebuild `clientGraphics`** (or the whole solution) so `ConfigClientGraphics.cpp` picks up the `ClientGraphics` / `headless` handling used at `SetupClientGraphics::install` time.

## Config layout (`exe/win32_rel/`)

- **`client.cfg`** — entry point only; it **`.include`s** `swgcamera_client_base.cfg` (relative paths). Do not replace it with a monolithic paste; edit **`swgcamera_client_base.cfg`** for shared defaults or **`misc/override.cfg`** for local overrides. Run the exe with **cwd = `exe\win32_rel`** (same folder as `titan.cfg` and `tres\`) so `tres/...` and `../../data/...` resolve. See **`exe/win32_rel/README_swgcamera.txt`**.
- **Login vs full SwgTitan client:** the base config does **not** include **`login.cfg`**. If the full client logs in but SwgCameraClient does not, add **`loginServerAddress0` / `loginServerPort0` / `autoConnectToLoginServer`** (and credentials) via **`misc/override.cfg`**, uncomment **`.include "login.cfg"`** in **`client.cfg`**, or use **`-loginUser` / `-loginPassword`**. The engine also runs **`GameScheduler`** when **`ClientGraphics` headless** is set so login is not blocked when the window is not foreground.

## Auto-login (command line)

After `client.cfg` is loaded, optional flags are parsed and written into the **`ClientGame`** section as **`loginClientID`** and **`loginClientPassword`** (same keys as `ConfigClientGame` / login UI):

```text
SwgCameraClient_r.exe -loginUser myaccount -loginPassword "secret"
```

Use quotes if a value contains spaces.

**Alternative (engine-wide):** anything after `--` is already merged into `ConfigFile` by `SetupSharedFoundation` (see `ConfigFile::loadFromCommandLine`), e.g.:

```text
SwgClient.exe -- -s ClientGame loginClientID=user loginClientPassword=pass
```

## Instance mutex

Uses semaphore name **`SwgCameraClientInstanceRunning`** so you can run this beside **SwgClient** when `SwgClient/allowMultipleInstances` allows it.

## Map capture (ortho tiles + GOT filter + environment)

Implemented in **clientGame** as `SwgCameraMapCapture`, driven from `GroundScene` (free camera, parallel projection during world draw, `Graphics::screenShot` at end of `GroundScene::draw`). While a batch runs, **FreeCamera** input is ignored so the view stays stable. Object culling uses `ObjectList` with a GOT predicate (`SharedObjectTemplate` + `GameObjectTypes`).

Add a **`[SwgCameraClient]`** section to your config (e.g. `client.cfg` or `--` merged keys):

| Key | Default | Purpose |
|-----|---------|---------|
| `mapCaptureEnabled` | `false` | Master switch. When `true`, starts a capture batch once the player exists and is in-world (ground scene only). |
| `mapCaptureUseFixedWorldBounds` | `true` | If `true`, tiles cover **`mapCaptureWorldMinX`…`MaxX`** × **`MinZ`…`MaxZ`** in a **zigzag** (row 0 left→right, row 1 right→left, …) starting at top-left **(minX, maxZ)**. If `false`, uses a square grid around the **player** (`mapCaptureGridRadius`). |
| `mapCaptureWorldMinX` / `MaxX` / `MinZ` / `MaxZ` | `-8000` / `8000` / `-8000` / `8000` | World rectangle for fixed mode (horizontal plane **X/Z**). |
| `mapCaptureTileWorldSize` | `512` | World units covered along the horizontal axis of each tile (ortho frustum matches viewport aspect). |
| `mapCaptureCameraHeight` | `700` | Camera altitude (`Y`) above the origin plane for the free camera. |
| `mapCaptureGridRadius` | `0` | **Fixed mode off only:** integer radius in tiles around the player’s tile when the batch starts. |
| `mapCaptureSettleFrames` | `2` | Frames to wait after moving to each tile before calling `Graphics::screenShot`. |
| `mapCaptureOutputDirectory` | `mapcapture` | Output folder (under cwd). **Fixed mode:** `tile_<index>_r<row>_c<col>.*` ; **player grid:** `tile_<originX>_<originZ>_<dx>_<dz>.*` (extension from `ClientGraphics`). |
| `mapCaptureFilterCreatures` | `true` | Skip drawing non–player-creature mobs (masked `GOT_creature`, excluding `GOT_creature_character`). |
| `mapCaptureFilterLairs` | `true` | Skip `GOT_lair`. |
| `mapCaptureFilterNpcLike` | `true` | Skip `GOT_vendor` and `GOT_creature_character` (includes player avatars). |
| `mapCaptureSuppressFog` | `true` | Disable terrain fog for the batch (`GroundEnvironment::setEnableFog`). |
| `mapCaptureSuppressParticles` | `true` | Disable particles (`ParticleManager::setParticlesEnabled`). |
| `mapCaptureSuppressWeather` | `true` | Zero wind, pause environment time progression (`WeatherManager` + `GroundEnvironment::setPaused`). |

**Streaming:** `WorldSnapshot` still follows the **player**. Ortho tiles far from the avatar may show sparse/empty terrain unless you move the character (e.g. admin tools) or extend the client to sync the player to each tile (not implemented here).

**Engine support:** `Camera::setPerspectiveProjection()` restores perspective after orthographic capture. **`ObjectList`** exposes `setObjectRenderSkipPredicate` / `clearObjectRenderSkipPredicate` for the GOT filter without pulling clientGame into sharedObject.

### Tile aspect ratio (square world coverage)

Ortho half-extents are derived from the **render target** width/height (`vh/vw`). For each tile to cover a **square** patch of world **X/Z** with side `mapCaptureTileWorldSize`, the viewport must be **1:1** — set **`[SharedFoundation]` `screenWidth` = `screenHeight`** (e.g. **1024×1024**). A 4:3 or 16:9 window stretches one world axis vs the other.

While a capture batch runs, **`CuiManager::setRenderSuppressed(true)`** skips the full UI render pass so the first frame’s screenshot (and all tiles) have **no HUD/widgets**.
