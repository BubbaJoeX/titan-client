# Adding new story companions

Story companions are **datatable-driven NPC pets** (SWTOR-style roster): one combat pet out at a time, control device on the **datapad**, shared rules with normal NPC pets (`pet_lib` / `callable`).

Primary code: `script.library.companion_lib`  
Primary data: `datatables/companion/story_companions.iff` (source `.tab` under `dsrc/.../datatables/companion/story_companions.tab`)

---

## 1. Add a row to `story_companions`

| Column | Purpose |
|--------|---------|
| **companion_id** | Stable string key (e.g. `companion_my_hero`). Used in code and objvars (`companion.storyId`). |
| **companion_name** | Display / default name. |
| **creature_name** | Row in **`datatables/mob/creatures.iff`** — defines the spawned mob template, stats baseline, and species. |
| **level** | `0` = effective level follows the owner (capped like other pets). Positive = cap level from table vs player. |
| **role** | `tank`, `healer`, `dps` (or `heal`) — default **combat stance** (`companion.stance` on PCD/pet). |
| **companion_weapon** | Optional token for weapon setup; can be empty to keep creature defaults. |
| **companion_favorite_gifts** | Optional comma-separated item names for gift/influence systems. |
| **companion_speed** | Movement scale hint (default `1.2` in table). |
| **companion_pet_bar_abilities** | **Non–human-skeleton** companions only: comma-separated **`beast_specials`** `ability_name` entries (e.g. `bm_slash_2`). **Human-shaped** companions use the humanoid pet bar (weapon toggle + taught player commands); this column is ignored for them at runtime. |
| **grant_message** | System message when granted via `grantStoryCompanionToDatapad` (can be empty). |

After editing the `.tab`, rebuild the **`.iff`** as your pipeline requires.

---

## 2. Creature choice: humanoid vs non-humanoid

- **Human skeleton** (`ai_lib.isHumanSkeleton(pet)` — see `datatables/ai/species.iff` **Skeleton** = `human`):  
  - Pet bar: **melee/ranged toggle** + **three “taught” slots** (player commands from the datapad **Train Companion Abilities** menu).  
  - **Faction** on summon: aligned to owner via `applyStoryCompanionFactionFromOwner`.  
  - Teach abilities from **`getCommandListingForPlayer`** (must exist in `command_table` with a **scriptHook**).

- **Other creatures** (e.g. beasts, aliens without human skeleton):  
  - Pet bar abilities come from **`companion_pet_bar_abilities`** and must validate against **`beast_specials`**.

Pick **`creature_name`** accordingly.

---

## 3. Granting the companion to a player

Runtime API:

```java
obj_id cd = companion_lib.grantStoryCompanionToDatapad(player, "companion_id");
```

- Creates/spools the pet, builds the **control device** on the datapad, sets **`companion.storyId`**, default stance, **`companion.taughtAbilities`**, attaches **`systems.companion.companion_story_pcd`**, stores the pet, ensures **`ai.pet_master`** on the player.  
- Fails if the player already owns that `companion_id`, pet storage limits block, or creation fails.

Call this from **quests**, **conversation** scripts, or **GM tools**.

---

## 4. Hire flow (world NPC → join roster)

For a **world recruiter** that talks to the player:

1. Implement a **conversation** script (see `conversation.companion_greeata`).
2. Wire the companion in **`companion_lib.resolveHireConversationScript(String storyCompanionId)`** — return your `conversation.*` script name (e.g. `"conversation.companion_my_npc"`).
3. Optionally handle **`applyMakeHireableToNpc`** / **`prepareHireConversationNpc`** for designer `/developer` setup (strip AI, attach only hire dialog). Greeata also has **`companion_lib.GREEATA_WORLD_MOBILE_TEMPLATE`** for a safe client mesh.

In the hire branch, call:

- `companion_lib.playerOwnsStoryCompanion(player, STORY_ID)` to avoid duplicates  
- `companion_lib.grantStoryCompanionToDatapad(player, STORY_ID)`  
- Optional: `companion_lib.modifyInfluence(player, STORY_ID, delta)`

**`resolveHireConversationScript`** must return **non-null** for `applyMakeHireableToNpc` to succeed for that id.

---

## 5. Control device radial (same for all story companions)

Script: **`systems.companion.companion_story_pcd`**

- **Companion Combat Role** — Tank / Healer / Damage (updates `companion.stance`).  
- **Train Companion Abilities** — assign player commands to pet bar slots 1–3 (humanoid bar).  
- **Clear Taught Ability Slot** — clear one slot.

Requires **`companion.storyId`** on the PCD.

---

## 6. Code touchpoints when adding a brand-new id

| Task | Where |
|------|--------|
| New hire conversation | New `conversation/companion_*.java`; register in **`companion_lib.resolveHireConversationScript`** |
| Special world template / name | **`applyMakeHireableToNpc`** or hire script (pattern: Greeata) |
| Influence / gifts | **`companion_lib`** influence objvars and gift tables as needed |

---

## 7. Quick checklist

1. Add **`creature_name`** to creatures table if it does not exist.  
2. Add **`story_companions`** row with **`companion_id`** and columns above.  
3. Rebuild **story_companions.iff**.  
4. Grant via script: **`grantStoryCompanionToDatapad`**, or implement hire + **`resolveHireConversationScript`**.  
5. Test: datapad PCD, call out pet, combat role radial, (humanoid) train/clear abilities, non-humanoid beast specials from table.

---

## 8. Reference IDs in repo

Example rows: `companion_demo_bodyguard`, `companion_demo_scoundrel`, `companion_demo_medic`, `companion_greeata` in `story_companions.tab`.  
Reference conversation: `conversation/companion_greeata.java`.
