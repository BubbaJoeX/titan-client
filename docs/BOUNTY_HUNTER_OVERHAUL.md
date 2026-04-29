# Bounty Hunter Overhaul Plan (Template-Rebuild Safe)

This plan translates the approved BH feature set into concrete implementation steps for this codebase.

Primary constraint:

- **Do not introduce new object templates (**`.tpf`**) unless absolutely unavoidable.**
- **DB/schema changes are optional for this overhaul path** (current implementation is script/objvar/datatable-driven).
- Prefer **existing script attachments, existing item templates, existing terminal templates, existing mission types**, and datatable/script logic.
- Goal: deliver gameplay systems without forcing a global template rebuild.

---

## Implementation status (current)

Implemented in code now:

- 3.1 Investigation loop (stage/confidence APIs, init, decay, advancement).
- 3.2 Warm/cold trails (tracking output now confidence-banded).
- 3.3 Counterplay hooks (jammer/decoy states and tracking interaction).
- 3.5 Capture path (non-lethal bounty success path for bounty targets).
- 3.6 Renown progression (objvar-backed points/rank + payout modifier).
- 3.7 Regional heat (objvar-backed heat with decay + payout bonus use).
- 3.8 Unified PvE/PvP contract type scaffolding (`PVE_NPC`/`PVP_PLAYER`).
- 3.9 Group hunts (participant eligibility + split payout scaffold).
- 3.10 Player bounty terminal silhouettes (implemented previously in client UI).
- 3.11 Anti-grief safeguards (posting throttle + repeat-target payout throttles).
- 3.12 Item bounties (player-posted crafted/static item contracts with broker turn-in/pickup).

Partially implemented / scaffold only:

- [~] 3.4 Marketplace + escrow: script-level contract helper scaffolding exists, but no full terminal marketplace posting/acceptance UX and no DB-backed contract history.

Not implemented yet:

- Full DB-backed contract persistence/query flows (explicitly deferred).

Key implemented touchpoints:

- `dsrc/sku.0/sys.server/compiled/game/script/library/bounty_hunter.java`
- `dsrc/sku.0/sys.server/compiled/game/script/systems/missions/dynamic/mission_bounty.java`
- `dsrc/sku.0/sys.server/compiled/game/script/systems/missions/dynamic/mission_bounty_informant.java`
- `dsrc/sku.0/sys.server/compiled/game/script/systems/missions/dynamic/mission_bounty_target.java`
- `dsrc/sku.0/sys.server/compiled/game/script/systems/missions/dynamic/mission_bounty_item.java`
- `dsrc/sku.0/sys.server/compiled/game/script/systems/missions/dynamic/item_bounty_broker.java`
- `dsrc/sku.0/sys.server/compiled/game/script/systems/missions/base/mission_player.java`
- `dsrc/sku.0/sys.server/compiled/game/script/systems/missions/base/mission_terminal_bounty.java`
- `dsrc/sku.0/sys.server/compiled/game/script/player/base/base_player.java`
- `dsrc/sku.0/sys.server/compiled/game/script/library/pvp.java`
- `dsrc/sku.0/sys.server/compiled/game/datatables/missions/bounty/bounty_investigation.tab`
- `dsrc/sku.0/sys.server/compiled/game/datatables/missions/bounty/bounty_heat_regions.tab`

---

## 1) Existing architecture summary

Core BH systems already present:

- Jedi bounty assignment/persistence:
  - `src/game/server/application/SwgGameServer/src/shared/object/JediManagerObject.cpp`
  - `src/game/server/application/SwgGameServer/src/shared/controller/JediManagerController.cpp`
  - DB persistence via `bounty_hunter_targets` message/query/buffer paths.
- PvP bounty legality:
  - `src/engine/server/library/serverGame/src/shared/pvp/PvpRuleSetNormal.cpp`
  - `src/engine/server/library/serverGame/src/shared/pvp/PvpFactions.cpp`
- Script mission flow:
  - `dsrc/sku.0/sys.server/compiled/game/script/systems/missions/base/mission_player.java`
  - `.../mission_dynamic_base.java`
  - `.../mission_bounty.java`
  - `.../mission_bounty_informant.java`
  - `.../mission_bounty_target.java`
- BH utility script:
  - `dsrc/sku.0/sys.server/compiled/game/script/library/bounty_hunter.java`
- Death/bounty accounting hooks:
  - `dsrc/sku.0/sys.server/compiled/game/script/library/pclib.java`
  - `dsrc/sku.0/sys.server/compiled/game/script/library/pvp.java`

---

## 2) Non-negotiable implementation rules

- **Template-safe first**:
  - Reuse existing:
    - BH terminals (`terminal_bounty*`, droid terminal),
    - droid/item templates already in data,
    - mission object templates and script attach points.
  - Add new behavior through:
    - script objvars,
    - datatables,
    - server script handlers/libraries,
    - existing mission/controller messages where possible.
- **No new client protocol unless necessary**:
  - Prefer mission description/system message updates for first release.
- **Server authoritative for all anti-abuse and rewards**.

---

## 3) Feature mapping and concrete touchpoints

## 3.1 Investigation loop (cold -> warm -> confirmed)

### Goal

Make player hunts require intel gathering before exact lock.

### Implementation touchpoints

- `script/library/bounty_hunter.java`
  - Add mission-intel API helpers:
    - `getInvestigationStage()`
    - `advanceInvestigationStage()`
    - `getInvestigationConfidence()`
    - `applyInvestigationDecay()`
- `script/systems/missions/dynamic/mission_bounty_informant.java`
  - Return clue quality based on stage/confidence, not static text only.
- `script/systems/missions/base/mission_dynamic_base.java`
  - Initialize investigation objvars on mission creation.
- Datatable additions (no templates):
  - `datatables/missions/bounty/bounty_investigation.tab`
    - clue type, confidence delta, cooldown, region granularity.

### Data model (objvars, no schema yet)

- `bh.invest.stage` (int)
- `bh.invest.confidence` (float/int)
- `bh.invest.lastUpdate` (int timestamp)
- `bh.invest.lastClueType` (string)

---

## 3.2 Warm/cold trails

### Goal

Tracking quality degrades over time and improves with activity/clues.

### Implementation touchpoints

- `script/library/bounty_hunter.java`
  - Integrate confidence decay + stale trail penalty.
- Existing droid scripts:
  - `.../bounty_probe_droid.java`
  - `.../bounty_probot.java`
  - `.../bounty_seeker.java`
  - change output from binary location to confidence-based granularity.

### Output model

- Cold: planet/city hint only.
- Warm: district/zone hint.
- Confirmed: precise waypoint.

---

## 3.3 Target counterplay gadgets (jammer/decoy)

### Goal

Allow hunted players to delay BH certainty without nullifying hunts.

### Template-safe approach

- Reuse existing consumable templates where possible.
- Apply behavior via script item handlers and buff/debuff states.

### Implementation touchpoints

- `script/library/bounty_hunter.java`
  - `applyTrackerJam(target, duration)`
  - `applyDecoySignal(target, duration)`
- player/item scripts under existing consumable handler framework.
- Optional messaging through existing system message flow.

---

## 3.4 Contract marketplace with escrow

### Goal

Player-created contracts with secured payout lifecycle.

### Phase 1 (template-safe, low UI)

- Use existing bounty terminal interaction + SUI flow.
- Add backend contract records and escrow accounting.

### Current status

- Script-level scaffolding exists in `bounty_hunter.java` (contract id/state helpers).
- Full marketplace UI flow and durable DB contract lifecycle are still pending.

### Likely touchpoints

- Java:
  - `base_player.java` (contract posting entry)
  - `bounty_hunter.java` (accept/complete/cancel contract APIs)
- DB (new schema required):
  - `bounty_contracts`
  - `bounty_contract_participants` (optional for group phase)
  - package methods in persister/loader pattern.

---

## 3.5 Capture mechanics (non-lethal completion path)

### Goal

Support capture bonus without removing kill completion.

### Implementation touchpoints

- `pclib.java`
  - keep death path, add capture resolver hook.
- `bounty_hunter.java`
  - `winBountyMissionCapture()` and payout multipliers.
- `mission_bounty_target.java`
  - update NPC target rules to support capture-ready state where desired.

### Template-safe approach

- Use existing incapacitation/restraint states and mission objvars.
- Do not introduce new target templates for v1.

### Current status

- Implemented for dynamic bounty targets through mission script hooks.
- `mission_bounty_target.java` now routes incapacitation to bounty success (capture flag).

---

## 3.6 BH progression / renown

### Goal

Dedicated BH identity and unlock track.

### Implementation touchpoints

- `bounty_hunter.java`
  - `addRenown(player, reason, amount)`
  - rank thresholds and rewards.
- reward hooks:
  - `mission_bounty.java` success path
  - Jedi/player hunt completion path.

### Storage

- v1: player objvars (template-safe, no DB migration).
- v2: optional DB mirror for analytics/reporting.

---

## 3.7 Regional heat / jurisdiction

### Goal

Location-based legal pressure and payout variation.

### Implementation touchpoints

- `mission_player.java` and/or `mission_dynamic_base.java`
  - heat-aware mission generation weighting.
- `bounty_hunter.java`
  - heat increment/decay routines.
- datatable:
  - `datatables/missions/bounty/bounty_heat_regions.tab`

### Storage (v1)

- world/region objvars or server memory cache with periodic persist.

### Current status

- Implemented as objvar-backed heat values with time-based decay.
- Payout bonus integration is active; mission-generation weighting is not yet extended.

---

## 3.8 Unified PvE/PvP contracts

### Goal

Single contract abstraction with different execution backends.

### Implementation touchpoints

- `mission_dynamic_base.java`
  - introduce `contractType` field:
    - `PVE_NPC`
    - `PVP_PLAYER`
- `bounty_hunter.java`
  - shared reward/timeout/completion adapters.
- keep JediManagerObject ownership logic for PVP targets in place.

### Current status

- Implemented as contract-type scaffolding and mission-level contract tagging.
- Not yet a full shared lifecycle service with complete terminal UX.

---

## 3.9 Group hunts

### Goal

Optional cooperative contracts.

### Implementation touchpoints

- `bounty_hunter.java`
  - participant registration and distance/activity checks.
- mission scripts:
  - success fan-out and split payouts.
- use existing group systems; avoid new group templates.

---

## 3.10 Player bounty terminal silhouettes (replace rotating icon)

### Goal

Display **silhouettes** on the Player Bounty mission terminal UI instead of the current rotating icon treatment.

### Template-safe approach

- Reuse existing UI pages/widgets and icon assets where possible.
- Prefer UI/script/data changes only; do not add new object templates.

### Implementation touchpoints

- Client UI:
  - `client/src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiMissionBrowser.cpp`
  - `client/src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiMissionDetails.cpp`
  - `client/src/engine/client/library/clientGame/src/shared/object/ClientMissionObject.cpp`
- Mission payload/filtering:
  - `sharedNetworkMessages/.../MessageQueueMissionListResponse.*` (existing bounty-terminal flag paths)
- Server scripts (if needed to expose silhouette metadata by target class/risk):
  - `dsrc/.../script/systems/missions/base/mission_player.java`
  - `dsrc/.../script/systems/missions/base/mission_dynamic_base.java`

### Notes

- For PvP player bounties, silhouette variants can be tied to risk tier/mission difficulty, not identity-revealing portraits.
- Keep explicit identity reveal gated behind existing investigation/tracking progression.

---

## 3.11 Anti-grief safeguards

### Goal

Prevent farming/collusion and protect low-level players.

### Touchpoints

- `pvp.java`:
  - strengthen repeat-kill diminishing logic.
- `bounty_hunter.java`:
  - cooldown windows, same-target throttles, contract abuse checks.
- `base_player.java`:
  - posting constraints and min eligibility.
- logging:
  - add contract lifecycle audit messages for CS review.

---

## 3.12 Item bounties (crafted/static item contracts)

### Goal

Allow any player to post a bounty on a specific crafted/static item object, let BH players accept that contract, and complete it by retrieving the exact item and turning it in through a broker NPC.

### Implementation touchpoints

- `script/library/bounty_hunter.java`
  - item bounty registry/state machine (`OPEN`, `ACCEPTED`, `READY_FOR_PICKUP`, `PICKED_UP`)
  - target validation (crafted/static requirement, no-trade/shared rejection, unique static rejection)
  - mission creation for BH acceptance
  - turn-in/pickup settlement + notifications
- `script/systems/missions/base/mission_terminal_bounty.java`
  - mission-board style posting/acceptance entry points:
    - `Post Item Bounty`
    - `Browse Item Bounties` (BH-only acceptance)
- `script/systems/missions/dynamic/mission_bounty_item.java`
  - accepted mission behavior + staged clue messaging (crafted/static profile and class hints)
- `script/systems/missions/dynamic/item_bounty_broker.java`
  - server-side broker NPC conversation flow for:
    - hunter turn-in
    - creator pickup
- Client UI:
  - `client/src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiMissionDetails.cpp`
  - bounty details preserve tangible/weapon/armor appearance silhouettes for item-bounty visuals instead of forcing humanoid anonymized silhouettes.

### Notes

- Broker script attachment required on intended NPCs:
  - `systems.missions.dynamic.item_bounty_broker`
- DB persistence layer is not required for current implementation.

---

## 4) Milestone plan

## M1: Script/datatable-only gameplay depth (no DB schema)

- Investigation stages + confidence model.
- Warm/cold tracking output from informants + droids.
- Basic counterplay jammer/decoy.
- Renown via objvars.
- Player bounty terminal silhouette UI swap.
- Anti-grief pass (script-level).
- Item bounty posting/acceptance/turn-in/pickup flow.

Status: **Implemented**

Acceptance:

- Hunt quality visibly changes by intel state.
- Targets can delay but not invalidate hunts.
- No template rebuild required.

## M2: Capture + unified contract behavior

- Add capture completion path and rewards.
- Introduce unified contract type abstraction in mission scripts.
- Add group hunt scaffolding.

Status: **Implemented (script-level/scaffold)**  
Notes:

- Capture completion path implemented.
- Contract-type and group payout scaffolding implemented.
- Further polishing and balancing still expected.

Acceptance:

- Both kill and capture can complete contract (where configured).
- PvE/PvP share common contract lifecycle events.

## M3: Marketplace + escrow + regional heat

- DB schema and package additions for contracts.
- Terminal/SUI posting and accepting flows.
- Region heat effects and payout scaling.

Status: **Partially implemented**

- Region heat effects and payout scaling: implemented (objvar/datatable/script path).
- DB schema/package additions for contracts: deferred.
- Full marketplace posting/acceptance UX: pending.

Acceptance:

- Contracts settle with escrow guarantees.
- Region heat influences mission availability/payout.

---

## 5) Build/rebuild impact

Expected for M1/M2:

- Script compile + datatable updates only.
- **No new `.tpf`**, no asset/template rebuild.

Expected for M3:

- DB schema/package migration required.
- Still template-safe if we keep using existing terminals/items.

Current actual impact:

- Script + datatable updates only.
- No DB migration required for current implemented state.
- No template rebuild required.

---

## 6) Testing checklist

- BH mission acquisition from existing terminals still works.
- Jedi hunter assignment cap and cleanup remain correct.
- PVP legality checks (`hasBounty` + mission target) unchanged for baseline.
- Investigation confidence decays and recovers as designed.
- Counterplay effects expire correctly and cannot stack-exploit.
- Player bounty mission terminal shows silhouettes (no rotating icon) and does not leak target identity prematurely.
- Capture path does not break legacy death completion.
- Escrow always resolves credits correctly (success/fail/timeout/cancel).
- Anti-grief: repeated collusion attempts do not pay out normally.
- Item bounty: invalid targets (no-trade/shared, unique static, non-crafted/non-static) are rejected.
- Item bounty: BH can accept, receives clues, turn-in to broker succeeds, creator pickup returns the exact item object.

---

## 7) Open design decisions

- Capture reward delta vs kill reward.
- Max concurrent contracts per hunter by rank.
- Whether regional heat is global per shard or per planet.
- Group split model: equal split vs contribution weighted.

