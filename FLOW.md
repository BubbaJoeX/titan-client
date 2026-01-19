# SwgClient Code Flow Documentation

This document describes the complete code flow of the SwgClient application, from startup through the main game loop to shutdown.

## Table of Contents
1. [Overview](#overview)
2. [Entry Point (WinMain)](#entry-point-winmain)
3. [Main Initialization (ClientMain)](#main-initialization-clientmain)
4. [Game Loop (Game::run)](#game-loop-gamerun)
5. [Shutdown Sequence](#shutdown-sequence)
6. [Visual Flow Diagram](#visual-flow-diagram)

## Overview

The SwgClient follows a traditional game engine architecture:
- **Startup**: Initialize all subsystems in dependency order
- **Main Loop**: Update game state, process input, render graphics
- **Shutdown**: Clean up resources and save user settings

Key files:
- `src/game/client/application/SwgClient/src/win32/WinMain.cpp` - Windows entry point
- `src/game/client/application/SwgClient/src/win32/ClientMain.cpp` - Initialization and shutdown
- `src/engine/client/library/clientGame/src/shared/core/Game.cpp` - Main game loop

## Entry Point (WinMain)

**Location**: `src/game/client/application/SwgClient/src/win32/WinMain.cpp` (Lines 111-122)

The Windows `WinMain` function is the application entry point and performs initial memory setup:

### Memory Manager Configuration

1. **User-Selected Memory Target** (Lines 24-46):
   - Checks `SWGCLIENT_MEMORY_SIZE_MB` environment variable
   - Parses the value (manual atoi implementation)
   - Sets memory limit via `MemoryManager::setLimit(megabytes, false, false)`

2. **Default Memory Target** (Lines 50-68):
   - Queries system RAM using `GlobalMemoryStatusEx()`
   - Allocates 75% of available RAM, capped at 1536MB
   - Without PAE (Physical Address Extension), 2048MB is max for 32-bit process
   - SWG can crash if given all available RAM, so 1536MB limit ensures stability

3. **Delegate to ClientMain** (Line 121):
   - Calls `ClientMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow)`
   - Returns result code to operating system

## Main Initialization (ClientMain)

**Location**: `src/game/client/application/SwgClient/src/win32/ClientMain.cpp` (Lines 127-397)

The `ClientMain` function orchestrates the initialization of all game subsystems in the correct dependency order.

### Phase 1: Core Foundation (Lines 139-175)

#### Thread System (Line 139)
```cpp
SetupSharedThread::install();
```
- Initializes threading primitives used throughout the engine

#### Debug System (Line 142)
```cpp
SetupSharedDebug::install(4096);
```
- Sets up debug output with 4KB buffer
- Enables logging and assertion systems

#### Foundation System (Lines 156-175)
```cpp
SetupSharedFoundation::install(data);
```
Initializes core engine with:
- **Window Configuration**:
  - Title: "SWG: Titan" (production) or custom development name
  - Window icons (normal and small)
  - Window instance handle
- **Config File**:
  - Production: `client.cfg`
  - Debug: `client_d.cfg`
- **Clock Settings**:
  - Uses sleep for timing (`clockUsesSleep = true`)
  - Minimum frame rate: 1 FPS
  - Maximum frame rate: 144 FPS
- **Crash Reporting**: Always writes mini-dumps on crashes
- **Command Line**: Passes through for parameter processing

### Phase 2: Shared Engine Subsystems (Lines 180-301)

#### Configuration and Feature Setup (Lines 180-213)

1. **Config File Validation** (Lines 181-182):
   - Verifies config file loaded successfully
   - Fatal error if config is empty

2. **Single Instance Check** (Lines 186-191):
   - Creates named semaphore "SwgClientInstanceRunning"
   - Prevents multiple instances (configurable in non-production)
   - Shows message box and exits if another instance exists

3. **Game Feature Bits** (Lines 195-213):
   - Loads from `[Station]` section: `gameFeatures`
   - Clears bits specified in `[ClientGame]`: `gameBitsToClear`
   - Feature flags include:
     - `Base` - Core game
     - `SpaceExpansionRetail` - Jump to Lightspeed (JTL)
     - `Episode3ExpansionRetail` - Rage of the Wookiees
     - `TrialsOfObiwanRetail` - Trials of Obi-Wan
   - Automatically promotes beta/preorder flags to retail
   - Sets subscription feature bits
   - Registers external command handler

#### Core Libraries (Lines 216-267)

4. **Compression** (Lines 216-219):
   - Initializes zlib with 3 threads for concurrent access

5. **Regular Expressions** (Line 222):
   - Enables regex support for string parsing

6. **File System** (Lines 225-238):
   - Determines SKU bits based on owned expansions
   - Installs TreeFile system with appropriate SKU mask
   - Loads `misc/override.cfg` if present (Lines 99-110)

7. **Math Library** (Line 243):
   - Vector, matrix, quaternion operations

8. **Utility System** (Lines 246-249):
   - Game-specific utility functions
   - Enables file caching

9. **Random Number Generator** (Line 252):
   - Seeds with current time: `time(NULL)`

10. **Logging** (Line 254):
    - Creates "SwgClient" log file

11. **Image Loading** (Lines 257-259):
    - TGA, DDS, and other image format support

12. **Network Layer** (Lines 262-267):
    - Client-specific network configuration
    - Message factory registration
    - Game and SWG network message handlers

#### Object and Game Systems (Lines 269-301)

13. **Object System** (Lines 269-278):
    - Time-based appearance templates
    - Slot/hardpoint system (for attachments, weapons)
    - Customization data (character appearance)
    - Movement tables (animation blending)

14. **Game Core** (Lines 281-289):
    - Game scheduler for time-based events
    - Mount validation tables
    - Debug callback for bad string IDs: `CuiManager::debugBadStringIdsFunc`
    - Commodities/auction search attributes
    - Auction filter display strings

15. **Terrain System** (Lines 292-294):
    - Game-specific terrain configuration

16. **XML Parsing** (Line 297):
    - XML file loading and processing

17. **Pathfinding** (Line 300):
    - AI navigation system

### Phase 3: Client-Specific Subsystems (Lines 304-368)

#### Graphics and Audio (Lines 305-327)

18. **Audio System** (Line 305):
    - Sound effects and music playback

19. **Graphics System** (Lines 308-314):
    - Default resolution: 1024x768
    - Alpha buffer bit depth: 0 (no alpha buffer)
    - Direct3D initialization
    - Window/fullscreen mode support

20. **Splash Screen** (Lines 317-318):
    ```cpp
    SplashScreen::install();
    SplashScreen::render();
    ```
    - Displays loading screen immediately after graphics init
    - Provides visual feedback during remaining initialization

21. **Video Playback** (Line 320):
    - Bink video support for cutscenes
    - Uses Miles Sound System driver

22. **DirectInput** (Lines 323-327):
    - Keyboard and mouse input
    - Registers callbacks:
      - Screenshot: `ScreenShotHelper::screenShot` (F10 key)
      - Toggle windowed: `Graphics::toggleWindowedMode` (Alt+Enter)
      - Debug menu: `Os::requestPopupDebugMenu`
    - Lost focus handler: `DirectInput::unacquireAllDevices`

#### Graphics-Dependent Systems (Lines 329-368)

23. **Client Objects** (Lines 330-332):
    - Game-specific object types
    - Client-side object controllers

24. **Animation** (Lines 335-339):
    - Basic animation system
    - Skeletal animation with game-specific configuration

25. **Texture Renderer** (Line 342):
    - Dynamic texture generation (e.g., UI elements)

26. **Client Terrain** (Line 345):
    - Terrain rendering and collision

27. **Particle System** (Line 348):
    - Visual effects (explosions, weather, etc.)

28. **Client Game** (Lines 351-356):
    - Game-specific client logic
    - Scene management
    - Camera control
    - UI manager implementation callbacks:
      ```cpp
      CuiManager::setImplementationInstallFunctions(
          SwgCuiManager::install,
          SwgCuiManager::remove,
          SwgCuiManager::update
      );
      ```

29. **Bug Reporting** (Line 358):
    - Disabled customer service features (per README.md)

30. **IoWin (Input/Output Windows)** (Line 361):
    - Window management system

31. **SwgClientUserInterface** (Line 364):
    - SWG-specific UI components
    - HUD, inventory, chat, etc.

32. **G15 LCD Support** (Line 367):
    - Logitech G15 keyboard LCD display

### Phase 4: Main Loop Execution (Lines 370-371)

```cpp
rootInstallTimer.manualExit();
SetupSharedFoundation::callbackWithExceptionHandling(Game::run);
```

- Stops initialization timer
- Invokes `Game::run()` with exception handling wrapper
- Control remains in `Game::run()` until game exits

### Phase 5: Shutdown and Cleanup (Lines 373-395)

After `Game::run()` returns:

1. **Save Settings** (Lines 375-386):
   - UI workspace settings (window positions, states)
   - Chat window settings (filters, tabs)
   - CuiSettings (general UI preferences)
   - Chat history
   - User options (keybindings, graphics settings)
   - Local machine options

2. **Remove Core Systems** (Lines 390-391):
   - `SetupSharedFoundation::remove()` - Cleans up foundation
   - `SetupSharedThread::remove()` - Cleans up threading

3. **Release Instance Lock** (Lines 393-394):
   - Closes semaphore handle
   - Allows another instance to start

4. **Return to OS** (Line 395):
   - Returns 0 (success)

## Game Loop (Game::run)

**Location**: `src/engine/client/library/clientGame/src/shared/core/Game.cpp`

### Game::run() Entry Point (Lines 1036-1062)

The main game loop function with minimal overhead:

```cpp
void Game::run()
{
    DEBUG_REPORT_LOG_PRINT(true, ("Game::run\n"));
    
    Game::install(Game::A_client);
    
    ms_loops = 0;
    
    while (!isOver())
    {
        runGameLoopOnce(false, NULL, 0, 0);
    }
    
    delete ms_sceneCreator;
    ms_sceneCreator = 0;
    
    if (ms_cutSceneHelper)
        ms_cutSceneHelper->endCutScene(true);
}
```

#### Initialization (Lines 1040-1042)

1. **Game::install(Game::A_client)** (Lines 691-970):
   - Registers debug flags
   - Allocates crash report buffers
   - Installs game managers in order:
     - **Options**: Brightness, contrast, gamma (ground & space)
     - **Game Managers**:
       - MoodManager (emotional states)
       - ObjectAttributeManager (item stats)
       - DraftSchematicManager (crafting)
       - AuctionManager (bazaar/market)
       - PlanetMapManager (map system)
       - ResourceIconManager (resource display)
       - ClientRegionManager (areas/zones)
       - QuestJournalManager (quest tracking)
       - RoadmapManager (tutorial guidance)
       - QuestManager (quest logic)
       - CommandCppFuncs (command system)
       - ClientCommandQueue (command buffering)
       - ClientCombatPlaybackManager (combat animations)
       - CellProperty (building interiors)
     - **Network**: GameNetwork::install()
     - **CutScene**: Playback system
     - **UI**: CuiManager with asynchronous loader
   
2. **Reset Loop Counter** (Line 1042):
   - `ms_loops = 0` for tracking frame count

#### Main Loop (Lines 1046-1049)

Continues while `!isOver()`, which checks:
- `ms_done` flag (set by `Game::quit()`)
- Window close requested
- OS indicates application should exit

Each iteration calls `runGameLoopOnce(false, NULL, 0, 0)`.

#### Cleanup (Lines 1054-1061)

After loop exits:
- Deletes pending scene creator
- Ends any active cutscene

### Game Loop Iteration (runGameLoopOnce) (Lines 1066-1257)

Each frame executes these steps in order:

#### 1. Frame Timing and OS Update (Lines 1078-1097)

```cpp
Os::update();                           // Process Windows messages
VideoList::service();                   // Update video playback
const float elapsedTime = Game::getElapsedTime();
```

- **OS Update**: Processes Windows message queue (WM_PAINT, WM_KEYDOWN, etc.)
- **Video Service**: Updates playing cutscene videos
- **Elapsed Time**: Gets frame delta time from clock
- **Memory Tracking**: Updates memory statistics

#### 2. Garbage Collection (Lines 1099-1108)

Periodic cleanup to manage memory:
- **Object Templates**: `ObjectTemplate::garbageCollect()`
- **Terrain Chunks**: `TerrainObject::garbageCollect()`
- **Animations**: `AnimationStateHierarchyTemplate::garbageCollect()`

#### 3. Exit Timer Check (Lines 1112-1120)

If exit timer active:
- Check if countdown elapsed
- Call `quit()` when time expires

#### 4. Asynchronous Loading (Lines 1123-1125)

```cpp
if (AsynchronousLoader::isEnabled())
    AsynchronousLoader::processLoadRequests(0.030f);
```
- Loads assets in background (30ms per frame)
- Prevents hitches during gameplay

#### 5. Game Scheduler (Lines 1128-1137)

```cpp
if (Os::isMainWindowFocus())
{
    GameScheduler::alter(elapsedTime);
}
else
{
    GameScheduler::alterNetworkMessagingOnly(elapsedTime);
}
```

- **With Focus**: Updates all game logic (objects, combat, etc.)
- **Without Focus**: Only processes network messages
- Prevents game state updates when minimized

#### 6. Input Processing (Lines 1140-1141)

```cpp
DirectInput::update(elapsedTime);
```
- Reads keyboard and mouse state
- Triggers callbacks for hotkeys
- Builds input event queue

#### 7. Network Update (Lines 1144-1148)

```cpp
static Timer networkTimer(0.05f);  // 50ms = 20Hz
if (networkTimer.updateZero(elapsedTime))
{
    GameNetwork::update();
}
```
- Updates network at fixed 20Hz rate (every 50ms)
- Sends/receives packets
- Processes incoming messages
- Independent of frame rate

#### 8. Command and Attribute Processing (Lines 1151-1155)

```cpp
ClientCommandQueue::update(elapsedTime);
ObjectAttributeManager::update();
```
- **Command Queue**: Executes queued player commands
- **Attributes**: Updates item/creature stats display

#### 9. UI and Input Window Updates (Lines 1158-1170)

```cpp
if (Os::isMainWindowFocus())
{
    IoWinManager::update(elapsedTime);        // UI input processing
    IoWinManager::beginFrame();               // Start UI frame
    CuiManager::update(elapsedTime);          // Update UI logic
    IoWinManager::endFrame();                 // End UI frame
}
CuiManager::sendHeartbeat();                  // Keep-alive
ClientEffectManager::update(elapsedTime);     // Visual effects
```

- **IoWinManager**: Processes UI mouse/keyboard events
- **CuiManager**: Updates UI state (animations, tooltips, etc.)
- **Heartbeat**: Sent even without focus (prevents timeout)
- **Effects**: Updates particle effects, lights, sounds

#### 10. Audio Update (Lines 1197-1200)

```cpp
Object * const playerSoundObject = getPlayerSoundObject();
Audio::alter(elapsedTime, playerSoundObject);
```
- Updates 3D audio positions
- Streams music
- Processes sound cue triggers

#### 11. Texture Baking (Lines 1203-1206)

```cpp
if (TextureBaker::update(elapsedTime))
    Graphics::setStaticShaders(ShaderTemplate::getStaticShaderVector());
```
- Bakes dynamic textures (character customization)
- Updates shader list when complete

#### 12. Graphics Rendering (Lines 1209-1233)

```cpp
Graphics::update(elapsedTime);                // Prepare for frame
Graphics::beginScene();                       // Clear buffers
Graphics::setRenderWorldSetupFunction(Appearance::setupRenderWorld);

IoWinManager::draw();                         // Render all UI and 3D

Graphics::endScene();                         // Finalize frame
VideoList::drawFrameIfPlaying();              // Render video frame
Graphics::present(hwnd, rect);                // Flip buffers (present to screen)
```

**Rendering Pipeline**:
1. **Graphics Update**: Updates dynamic buffers, preps render state
2. **Begin Scene**: Clears render target and depth buffer
3. **Setup Render World**: Configures camera, fog, lighting
4. **IoWinManager Draw**: Renders in this order:
   - 3D scene (terrain, objects, characters)
   - Particle effects
   - UI elements (HUD, windows, cursor)
5. **End Scene**: Finalizes draw calls
6. **Video**: Overlays fullscreen video if playing
7. **Present**: Swaps back buffer to screen (vsync optional)

#### 13. Post-Frame Cleanup (Lines 1240-1257)

```cpp
Appearance::updateAppearanceTemplateTimeout(elapsedTime);  // Timeout unused templates
GameNetwork::getNetworkStatistics();                       // Update bandwidth stats

Clock::limitFrameRate();                                   // Sleep if ahead of target FPS
++ms_loops;                                                // Increment frame counter
```

- **Template Timeout**: Releases unused appearance data
- **Network Stats**: Calculates ping, bandwidth
- **Frame Rate Limiter**: Sleeps to maintain target FPS (144 Hz max)
- **Loop Counter**: Increments total frame count

### Timing Summary

| System | Frequency | Notes |
|--------|-----------|-------|
| OS Update | Every frame | Windows message processing |
| Garbage Collection | Every frame | Templates, terrain, animations |
| Async Loading | Every frame | 30ms budget per frame |
| Game Scheduler | Every frame | Full update when focused |
| Input | Every frame | Keyboard and mouse |
| Network | 20 Hz (50ms) | Fixed update rate |
| Commands | Every frame | Command queue processing |
| UI | Every frame | Only when focused |
| Audio | Every frame | 3D positional updates |
| Graphics | Every frame | Render at target FPS |
| Frame Limiter | Every frame | Caps at 144 FPS |

## Shutdown Sequence

### Game::remove() (Lines 974-991)

Called after main loop exits:

1. **Unregister Debug Flags** (Lines 976-978):
   - Removes client-specific debug commands

2. **Clear Crash Buffers** (Lines 980-983):
   - Deallocates crash report string buffers

3. **Cleanup Managers** (Lines 985-988):
   - PlaybackScriptManager::remove()
   - Other manager cleanup

4. **Delete Emitter** (Lines 990-991):
   - Releases message dispatch emitter

### Game::cleanupScene() (Lines 995-1002)

Cleans up active scene:

```cpp
if (ms_scene)
{
    ms_scene->quit();
    delete ms_scene;
    ms_scene = NULL;
}
```
- Calls scene's quit() method
- Deletes scene object
- Nulls pointer

### ClientMain Cleanup (Lines 373-395)

After `Game::run()` returns:

1. **Save All Settings**:
   - Workspace layout
   - Chat configuration
   - UI preferences
   - Chat history
   - User keybindings
   - Machine-specific options

2. **Remove Foundation Systems**:
   - Foundation shutdown (closes log files)
   - Thread system cleanup

3. **Release Instance Lock**:
   - Close semaphore
   - Allow new instance

4. **Return Success**:
   - Exit code 0

## Visual Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                          WinMain Entry                           │
│                                                                   │
│  1. Setup Memory Manager (user setting or 75% RAM, max 1536MB) │
│  2. Call ClientMain()                                           │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ClientMain Initialization                     │
├─────────────────────────────────────────────────────────────────┤
│ Phase 1: Core Foundation                                        │
│   • Thread System                                               │
│   • Debug System (4KB buffer)                                   │
│   • Foundation (window, config, clock)                          │
├─────────────────────────────────────────────────────────────────┤
│ Phase 2: Shared Engine Subsystems                              │
│   • Config validation & single instance check                   │
│   • Game feature bits (Base, JTL, Ep3, ToOW)                   │
│   • Compression, Regex, File System, Math                       │
│   • Utility, Random, Logging, Image                            │
│   • Network Layer & Message Handlers                           │
│   • Object System (slots, customization, movement)             │
│   • Game Core (scheduler, mounts, commodities)                 │
│   • Terrain, XML, Pathfinding                                  │
├─────────────────────────────────────────────────────────────────┤
│ Phase 3: Client-Specific Subsystems                            │
│   • Audio System                                               │
│   • Graphics System (D3D, 1024x768)                            │
│   • Splash Screen ★ (rendered immediately)                     │
│   • Video Playback (Bink)                                      │
│   • DirectInput (keyboard, mouse, hotkeys)                     │
│   • Client Objects, Animation, Skeletal Animation              │
│   • Texture Renderer, Client Terrain, Particles                │
│   • Client Game (scene, camera)                                │
│   • UI Manager (CuiManager, SwgCuiManager)                     │
│   • IoWin, SwgClientUserInterface                              │
│   • G15 LCD Support                                            │
├─────────────────────────────────────────────────────────────────┤
│ Phase 4: Main Loop                                             │
│   • Game::run() ──────────────────────────┐                    │
│                                            │                    │
└────────────────────────────────────────────┼────────────────────┘
                                             │
                                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                         Game::run()                              │
├─────────────────────────────────────────────────────────────────┤
│ Initialization:                                                 │
│   • Game::install(A_client)                                     │
│     - Register debug flags                                      │
│     - Install game managers (Mood, Auction, Quest, etc.)        │
│     - Install GameNetwork                                       │
│     - Install CutScene system                                   │
│     - Install CuiManager with async loader                      │
│   • Reset loop counter (ms_loops = 0)                           │
├─────────────────────────────────────────────────────────────────┤
│ Main Loop: while (!isOver())                                    │
│   │                                                             │
│   └─► runGameLoopOnce() ────────────────┐                      │
│                                           │                      │
└───────────────────────────────────────────┼──────────────────────┘
                                            │
                                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    runGameLoopOnce() - Each Frame                │
├─────────────────────────────────────────────────────────────────┤
│ 1. Frame Timing & OS                                            │
│    • Os::update() - Process Windows messages                    │
│    • VideoList::service() - Update cutscene video               │
│    • Calculate elapsed time                                     │
│    • Update memory statistics                                   │
├─────────────────────────────────────────────────────────────────┤
│ 2. Garbage Collection                                           │
│    • Object templates                                           │
│    • Terrain chunks                                             │
│    • Animation templates                                        │
├─────────────────────────────────────────────────────────────────┤
│ 3. Exit Timer Check                                             │
│    • Check if countdown elapsed                                 │
├─────────────────────────────────────────────────────────────────┤
│ 4. Asynchronous Loading                                         │
│    • Process load requests (30ms budget)                        │
├─────────────────────────────────────────────────────────────────┤
│ 5. Game Scheduler (60+ FPS)                                     │
│    • If focused: GameScheduler::alter() - full update           │
│    • If not focused: network messages only                      │
├─────────────────────────────────────────────────────────────────┤
│ 6. Input Processing (60+ FPS)                                   │
│    • DirectInput::update() - keyboard & mouse                   │
├─────────────────────────────────────────────────────────────────┤
│ 7. Network Update (20 Hz = every 50ms)                          │
│    • GameNetwork::update()                                      │
│    • Send/receive packets                                       │
│    • Process messages                                           │
├─────────────────────────────────────────────────────────────────┤
│ 8. Commands & Attributes                                        │
│    • ClientCommandQueue::update()                               │
│    • ObjectAttributeManager::update()                           │
├─────────────────────────────────────────────────────────────────┤
│ 9. UI & Input Windows (only if focused)                         │
│    • IoWinManager::update() - UI input                          │
│    • IoWinManager::beginFrame()                                 │
│    • CuiManager::update() - UI logic                            │
│    • IoWinManager::endFrame()                                   │
│    • CuiManager::sendHeartbeat() - always                       │
│    • ClientEffectManager::update()                              │
├─────────────────────────────────────────────────────────────────┤
│ 10. Audio Update                                                │
│     • Get player sound object                                   │
│     • Audio::alter() - 3D sound, music streaming                │
├─────────────────────────────────────────────────────────────────┤
│ 11. Texture Baking                                              │
│     • TextureBaker::update()                                    │
│     • Update static shaders if complete                         │
├─────────────────────────────────────────────────────────────────┤
│ 12. Graphics Rendering                                          │
│     • Graphics::update() - prepare frame                        │
│     • Graphics::beginScene() - clear buffers                    │
│     • Graphics::setRenderWorldSetupFunction()                   │
│     • IoWinManager::draw() ─────────────────┐                  │
│     │   ├─ 3D Scene (terrain, objects)      │                  │
│     │   ├─ Particle effects                 │                  │
│     │   └─ UI elements (HUD, windows)       │                  │
│     • Graphics::endScene() - finalize       │                  │
│     • VideoList::drawFrameIfPlaying()       │                  │
│     • Graphics::present() - flip buffers ◄──┘                  │
├─────────────────────────────────────────────────────────────────┤
│ 13. Post-Frame                                                  │
│     • Timeout unused appearance templates                       │
│     • Update network statistics                                 │
│     • Clock::limitFrameRate() - sleep if needed (144 FPS max)  │
│     • Increment loop counter                                    │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             │ (Loop continues until quit)
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Shutdown Sequence                           │
├─────────────────────────────────────────────────────────────────┤
│ Game::run() Cleanup:                                            │
│   • Delete scene creator                                        │
│   • End cutscene                                                │
├─────────────────────────────────────────────────────────────────┤
│ Game::remove():                                                 │
│   • Unregister debug flags                                      │
│   • Clear crash buffers                                         │
│   • Remove PlaybackScriptManager                                │
│   • Delete emitter                                              │
├─────────────────────────────────────────────────────────────────┤
│ ClientMain Cleanup:                                             │
│   • Save workspace settings                                     │
│   • Save chat settings                                          │
│   • Save UI settings                                            │
│   • Save chat history                                           │
│   • Save user options                                           │
│   • Save machine options                                        │
│   • SetupSharedFoundation::remove()                             │
│   • SetupSharedThread::remove()                                 │
│   • Close instance semaphore                                    │
│   • Return 0 (success)                                          │
└─────────────────────────────────────────────────────────────────┘
```

## Update Frequencies

Different systems run at different rates to balance performance and responsiveness:

| System | Rate | Interval | Notes |
|--------|------|----------|-------|
| Frame Loop | 1-144 Hz | 6.9-1000ms | Capped at 144 FPS max |
| OS Messages | Every frame | ~6.9ms @ 144Hz | Windows event queue |
| Input | Every frame | ~6.9ms @ 144Hz | Keyboard/mouse polling |
| Game Logic | Every frame | ~6.9ms @ 144Hz | Objects, combat, etc. |
| Graphics | Every frame | ~6.9ms @ 144Hz | Rendering pipeline |
| Audio | Every frame | ~6.9ms @ 144Hz | 3D sound updates |
| Network | 20 Hz | 50ms | Fixed rate, frame-independent |
| Async Loading | Every frame | 30ms budget | Background asset loading |
| Garbage Collection | Every frame | Variable | Template/terrain cleanup |

**Note**: Frame times shown at max 144 FPS. Actual frame time varies with settings and hardware.

## Key Design Patterns

### Dependency-Ordered Initialization
All subsystems initialize in dependency order:
1. Foundation (threading, debug, memory)
2. Core libraries (math, file, network)
3. Game systems (objects, terrain, audio)
4. Client-specific (graphics, UI, input)

### Separation of Concerns
- **Game::run()** - Main loop orchestration
- **runGameLoopOnce()** - Single frame execution
- **ClientMain()** - Initialization and shutdown
- **WinMain()** - Platform-specific entry

### Fixed Network Rate
Network updates at fixed 20 Hz (50ms) regardless of frame rate. This ensures consistent network behavior across different hardware.

### Asynchronous Loading
Assets load in background with 30ms per-frame budget, preventing frame rate hitches during gameplay.

### Focus-Aware Updates
Game logic pauses when window loses focus, but network continues to process messages preventing disconnects.

### Exception Handling
Main loop wrapped in exception handler (`SetupSharedFoundation::callbackWithExceptionHandling`) to capture crashes and write dump files.

## Critical Timing Sequences

### Startup
1. Memory manager configured (instant)
2. Core systems initialized (< 1 second)
3. **Splash screen shown** (first visual feedback)
4. Graphics subsystems loaded (1-2 seconds)
5. Game content loaded asynchronously (continues during play)

### Main Loop Iteration
1. OS/Input/Network (< 5ms)
2. Game logic (5-20ms, scene dependent)
3. Rendering (10-30ms, resolution/detail dependent)
4. Frame limiter (0-13ms, sleeps if ahead of target)

### Shutdown
1. Save settings (< 100ms)
2. Cleanup resources (< 500ms)
3. Close gracefully (< 1 second total)

## Configuration

Key configuration files:
- `client.cfg` or `client_d.cfg` - Main settings
- `misc/override.cfg` - Optional overrides loaded after main config
- `options.cfg` - User preferences (saved on exit)

Environment variables:
- `SWGCLIENT_MEMORY_SIZE_MB` - Override memory allocation

Command line parameters processed via `SetupSharedFoundation::Data::commandLine`.

## Related Documentation

For more information, see:
- [README.md](README.md) - Build instructions and project overview
- Source code comments in key files:
  - `WinMain.cpp` - Entry point
  - `ClientMain.cpp` - Initialization
  - `Game.cpp` - Main loop
  - Individual Setup*.cpp files - Subsystem initialization details

---

*This documentation was created based on the CSRC repository codebase. Last updated: 2026-01-19*
