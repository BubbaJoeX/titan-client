# Crafting system overhaul — research and checklist

This document records **codebase research** for the Titan/SWG-style crafting stack and a **delivery checklist** for the approved scope: **all QoL items (section A)**, plus **B1 (schematic favorites / library)** and **B2 (BOM planner)**.

---

## 1. Approved scope (reference)


| ID     | Theme             | Included                                                                                                                                                                                                                                                                       |
| ------ | ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **A**  | QoL (full)        | Session/resume policy, stage clarity, assembly helpers, hopper/station UX, inventory highlights, stack helpers, experiment feedback (preview / optional lock-line & undo in later sub-phase), factory preflight/summary/pause/alerts, structured errors, optional crafting log |
| **B1** | Schematic library | Favorites, filters, “craftable now” as **client hint** (server remains authoritative on assemble)                                                                                                                                                                              |
| **B2** | BOM planner       | Target quantity → aggregated components, missing vs inventory/hopper, export (chat / clipboard)                                                                                                                                                                                |


**Out of scope** for this checklist: work orders (B3+), training dummies, station synergy systems, removing experimentation RNG.

---

## 2. Architecture research (current implementation)

### 2.1 Shared definitions


| Item                                                                                                                     | Location                                                                                                                                                              |
| ------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Stages `Crafting::CraftingStage`, errors `Crafting::CraftingError`, results `Crafting::CraftingResult`, ingredient types | `src/engine/shared/library/sharedGame/src/shared/core/CraftingData.h`                                                                                                 |
| Archive / client mirror                                                                                                  | `src/engine/shared/library/sharedGame/src/shared/core/CraftingDataArchive.cpp`, `client/src/engine/shared/library/sharedGame/src/shared/core/CraftingDataArchive.cpp` |


Stages in use: `CS_none`, `CS_selectDraftSchematic`, `CS_assembly`, `CS_experiment`, `CS_customize`, `CS_finish`.

### 2.2 Server — session and slot operations


| Responsibility                                                                                                               | Primary files                                                                                                                                                                  |
| ---------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `fillSlot`, `emptySlot`, `experiment`, `goToNextCraftingStage`, `startCraftingExperiment`, crafting tool/station ids         | `src/engine/server/library/serverGame/src/shared/object/PlayerObject.cpp`, `PlayerObject.h`                                                                                    |
| Controller handling: `MessageQueueCraftFillSlot`, `MessageQueueCraftEmptySlot`, `MessageQueueCraftExperiment`, customization | `src/engine/server/library/serverGame/src/shared/controller/PlayerCreatureController.cpp`                                                                                      |
| Start/stop tool session, draft schematic request, hopper, prototype objvars                                                  | `src/engine/server/library/serverGame/src/shared/object/TangibleObject.cpp`                                                                                                    |
| Script hooks / JNI crafting APIs                                                                                             | `src/engine/server/library/serverScript/src/shared/ScriptMethodsCrafting.cpp` (and `base_class.java` crafting group in `dsrc/`)                                                |
| Manufacturing schematic logic                                                                                                | `src/engine/server/library/serverGame/src/shared/object/ManufactureSchematicObject.cpp`                                                                                        |
| Factory crafting session                                                                                                     | `src/engine/server/library/serverGame/src/shared/object/FactoryObject.cpp` (`startCraftingSession`, `endCraftingSession`, `resetCraftingSession`, `inCraftingSession`, counts) |
| Crafting-related commands / result fan-out                                                                                   | `src/engine/server/library/serverGame/src/shared/command/CommandCppFuncs.cpp`                                                                                                  |
| Creature: stop crafting on transfer/death, manufacture schematic listing, ingredient tests                                   | `src/engine/server/library/serverGame/src/shared/object/CreatureObject.cpp`                                                                                                    |
| Observation during prototype lifecycle                                                                                       | `src/engine/server/library/serverGame/src/shared/core/ObserveTracker.cpp` (`onCraftingPrototypeCreated`, `onCraftingEndCraftingSession`)                                       |


### 2.3 Network messages

Registered in `src/engine/shared/library/sharedNetworkMessages/src/shared/core/SetupSharedNetworkMessages.cpp` (and client twin):

- `MessageQueueCraftFillSlot`, `MessageQueueCraftEmptySlot`, `MessageQueueCraftExperiment`, `MessageQueueCraftCustomization`, `MessageQueueCraftRequestSession`, `MessageQueueCraftSelectSchematic`, `MessageQueueCraftIngredients`

Controller message types consumed on client include `CM_craftingResult`, `CM_nextCraftingStageResult`, `CM_craftingSessionEnded` — see `client/src/engine/client/library/clientGame/src/shared/controller/ClientController.cpp`.

### 2.4 Client — manager and UI


| Layer                                                        | Location                                                                                                                                                          |
| ------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Central crafting state, tool/schematic ids, message dispatch | `client/src/engine/client/library/clientUserInterface/src/shared/core/CuiCraftManager.cpp`, `CuiCraftManager.h`                                                   |
| Simulator (tests/dev)                                        | `client/src/engine/client/library/clientUserInterface/src/shared/core/CuiCraftManagerSimulator.cpp`                                                               |
| Draft schematic list / data                                  | `client/src/engine/client/library/clientGame/src/shared/core/DraftSchematicManager.cpp`, `DraftSchematicInfo.`*                                                   |
| Game UI pages                                                | `client/src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiCraft*.cpp` (`Draft`, `Assembly`, `Experiment`, `Customize`, `Summary`, `Option`, …) |
| Container provider for draft UI                              | `client/src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiContainerProviderDraft.cpp`                                                          |
| UI layout includes                                           | `client/datasources/ui/ui_craft*.inc`                                                                                                                             |


### 2.5 Content and tools

- Draft schematic XML / editor tooling under `client/src/game/server/application/SwgDraftSchematicEditor/` and `SwgSchematicXmlParser/`.
- Script-facing constants and crafting natives in `dsrc/sku.0/sys.server/compiled/game/script/base_class.java` (crafting group).

### 2.6 Design constraints discovered in code

- **Single-ingredient fill** today: controller routes one `NetworkId` per `MessageQueueCraftFillSlot`; bulk fill implies new message(s) or repeated calls with server-side batching.
- **Prototype observation**: `ObserveTracker` intentionally unobserves prototype during crafting then re-observes — UI changes must respect baseline timing.
- **Factory state**: `FactoryObject` tracks `m_craftingSchematic`, `m_craftingCount`; pause/resume requires auditing invariants against `incrementCount` / hopper consumption.
- **Schematic filtering**: tool script trigger `TRIG_REQUEST_DRAFT_SCHEMATICS` (`TangibleObject::startCraftingSession`); objvar `crafting.disableSchematicFiltering` on `CreatureObject` namespace.

---

## 3. Delivery checklist — section A (QoL)

### A.1 Session and flow

- **A.1.1** Document authoritative rules for “abandon session” (prototype disposition, hopper, tool `OBJVAR_CRAFTING_CRAFTER`, station linkage) — reference `TangibleObject::stopCraftingSession`, `PlayerObject::stopCrafting`.
- **A.1.2** Client: explicit end/confirm flow; no silent loss of items — align with `CM_craftingSessionEnded` handling in `CuiCraftManager` / `SwgCuiCraft`* pages.
- **A.1.3** Define “resume” policy (if any): same character, same tool, same server; gate on `PlayerObject::isCrafting()` and tool session objvars.
- **A.1.4** UI: persistent **stage strip** mapping `PlayerObject::m_craftingStage` / client ghost to `Crafting::CraftingStage` names.
- **A.1.5** UI: **blocker string** when `goToNextCraftingStage` cannot advance (empty slot, wrong stage, etc.) using server-provided code + args (see A.6).

### A.2 Assembly helpers

- **A.2.1** Server: optional `**fillAllSlots`** (or batched fill) building on `PlayerObject::fillSlot` validation — avoid duplicate moves / race with hopper.
- **A.2.2** Server: `**fillSlotFromHopper`** rules documented per crafting level (`Crafting::MAX_CRAFTING_LEVEL` / tool type in `CraftingData.h`).
- **A.2.3** Client: actions on `SwgCuiCraftAssembly` — wire to new or existing messages in `SetupSharedNetworkMessages.cpp`.
- **A.2.4** Client: when a slot is focused, **highlight/filter** inventory rows using draft slot metadata from `MessageQueueDraftSlots` / `CuiCraftManager` slot vector.
- **A.2.5** Client: **stack split** helper when `SimpleIngredient::count` requires partial pull from crate stack.

### A.3 Experimentation QoL

- **A.3.1** Server: extend experiment response payload (or companion message) with **display-safe** bounds / risk labels derived from `PlayerObject::experiment` state (no client-side RNG for outcomes).
- **A.3.2** Client: `SwgCuiCraftExperiment` / `SwgCuiCraftExperiment_Attrib` show preview before commit.
- **A.3.3** *(Optional sub-phase — explicit sign-off)* **Undo last experiment** or **lock attribute line**: threat model + exploit review before implementation.

### A.4 Factory QoL

- **A.4.1** Preflight: compute total inputs for N runs using `ManufactureSchematicObject` + crate counts (shared routine callable from UI request).
- **A.4.2** Surface **failure reason** strings (insufficient ingredient, not in crafting session, `getCount()` constraints in `FactoryObject`).
- **A.4.3** **Low-stock alert**: threshold storage (objvar vs player setting) + cooldown to prevent chat spam.
- **A.4.4** **Pause / resume**: design state machine against `FactoryObject::inCraftingSession`, `resetCraftingSession`, `incrementCount`; add tests for mid-run disconnect.

### A.5 Feedback and support

- **A.5.1** Map every `Crafting::CraftingError` in `CraftingData.h` to a **stable code** + `StringId` with optional parameters (slot index, template name).
- **A.5.2** Client: replace generic failures in `CuiCraftManager::receiveCraftingResult` paths with mapped messages.
- **A.5.3** *(Optional)* **Crafting log**: ring buffer in client vs persisted server audit — pick one; document PII/trade impact.

---

## 4. Delivery checklist — B1 (schematic library / favorites)

- **B1.1** Persistence: store favorite draft schematic ids (template crc and/or `NetworkId` strategy) on `PlayerObject` or character objvars — migration for existing characters.
- **B1.2** UI: star/unstar on `SwgCuiCraftDraft` (or draft list provider); sync on login.
- **B1.3** Filters: favorites-only, recent (client session or persisted), by `Crafting::CraftingType` bitmask — align with `CreatureObject::getManufactureSchematics` categorization.
- **B1.4** “**Craftable now**”: client-side heuristic from inventory snapshot + `DraftSchematicInfo`; **must** still fail safely via existing `fillSlot` / `startCraftingExperiment` checks.
- **B1.5** Ensure favorites interact correctly with script filtering (`TRIG_REQUEST_DRAFT_SCHEMATICS`) and `OBJVAR_DISABLE_SCHEMATIC_FILTER` behavior on `CreatureObject`.

---

## 5. Delivery checklist — B2 (BOM planner)

- **B2.1** Input: select schematic + **target output quantity** (respect stack limits and factory batch sizes if integrated).
- **B2.2** Server or shared lib: **aggregate ingredients** from `ServerDraftSchematicObjectTemplate` / active manufacture schematic slots — must match `PlayerObject::startCraftingExperiment` expectations for optional slots.
- **B2.3** Compare aggregated BOM to **inventory + tool hopper** contents (`CreatureObject` ingredient tests, tool container).
- **B2.4** UI panel: table of {resource/component, required, have, missing}.
- **B2.5** Export: single system message or clipboard string (platform-specific) — no false precision on substitute resources.
- **B2.6** Automated test: fixed schematic + known inventory → BOM match and missing list correctness.

---

## 6. Cross-cutting engineering tasks

- **X.1** Update `docs/INDEX.md` with a link to this document (if project index is maintained).
- **X.2** String tables: new `StringId` entries for errors, planner, factory alerts (location per project convention).
- **X.3** Config flags: optional `ConfigClientGame` / `ConfigServerGame` toggles for bulk fill and BOM to allow staged rollout.
- **X.4** Performance: profile inventory scan for “craftable now” / BOM on large inventories; throttle or cache.
- **X.5** Multiplayer: verify `ObserveTracker` crafting prototype path unchanged for other observers when BOM only reads client inventory.

---

## 7. Test plan (minimum)

- **T.1** Full happy path: select draft → assemble → experiment → customize → finish; verify `ObserveTracker` prototype visibility.
- **T.2** Each `Crafting::CraftingError` path returns correct new user-visible string.
- **T.3** Bulk fill: partial inventory, wrong resource class, damaged component, stacked loot (`CE_stackedLoot`).
- **T.4** Factory: preflight success/fail; mid-session `resetCraftingSession` / logout.
- **T.5** B1: favorites persist across logout; filtered list matches server on assemble.
- **T.6** B2: BOM totals match actual consumption for 1 and N outputs; optional slots toggled.

---

## 8. Sign-off


| Area            | Owner | Date | Notes |
| --------------- | ----- | ---- | ----- |
| Design          |       |      |       |
| Server          |       |      |       |
| Client UI       |       |      |       |
| Content/scripts |       |      |       |
| QA              |       |      |       |


---

*Last updated: research pass against Titan tree (server `PlayerObject`/`TangibleObject`/`FactoryObject`, client `CuiCraftManager` + `SwgCuiCraft`*, shared `CraftingData.h` and craft network messages).*