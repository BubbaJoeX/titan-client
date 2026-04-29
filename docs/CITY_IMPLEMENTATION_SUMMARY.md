# City Gameplay & Landscaping Update - Implementation Summary

## Files Created/Modified

### New Scripts (dsrc/)

1. **`script/systems/city/city_expulsion_handler.java`** - NEW
   - Handles player expulsion from city
   - 120 second grace period before attackable
   - TEF application when militia attacks
   - Building entry restrictions
   - Heartbeat to check if player left city bounds

2. **`script/systems/city/city_judge_election.java`** - NEW
   - Judge election management
   - Candidate registration
   - Vote casting and counting
   - Election finalization

3. **`script/systems/city/city_court.java`** - NEW
   - Eviction status tracking
   - Appeal management
   - Grace period expiration handling

### Modified Scripts (dsrc/)

4. **`script/library/city.java`** - MODIFIED
   - Updated `RANK_MAX` from 4 to 6
   - Added `CP_JUDGE` citizen permission flag
   - Added `SF_INDUSTRIAL_STARPORT` structure flag
   - Added expulsion system functions:
     - `beginExpulsion()`, `isExpelledFromCity()`, `canMilitiaAttackExpelled()`
     - `hasCityExpulsionTEF()`, `applyExpulsionTEF()`, `clearExpulsion()`
   - Added judge system functions:
     - `isJudge()`, `addJudge()`, `removeJudge()`, `getCityJudges()`
     - `getMaxJudgeCount()`
   - Added eviction system functions:
     - `beginEviction()`, `appealEviction()`, `handleJudgeDecision()`
   - Added extended tax functions:
     - `getStarshipLandingTax()`, `setStarshipLandingTax()`
     - `getCraftingTax()`, `setCraftingTax()`
     - `getVendorLicenseFee()`, `setVendorLicenseFee()`
     - `getStructurePlacementFee()`, `setStructurePlacementFee()`
   - Added `collectStarshipLandingTax()` and `ejectShipFromCity()`

5. **`script/terminal/terminal_city.java`** - MODIFIED
   - Added menu items for:
     - Expel Player (SERVER_MENU19)
     - Begin Eviction (SERVER_MENU20)
     - Extended Taxes (SERVER_MENU21)
     - Start Judge Election (SERVER_MENU22)
     - Review Appeals (SERVER_MENU23)
     - Register as Judge Candidate (SERVER_MENU25)
     - Vote for Judge (SERVER_MENU26)
     - Eviction Status (SERVER_MENU27)
     - Appeal Eviction (SERVER_MENU28)
   - Added UI helper methods and handlers

6. **`script/space/atmo/atmo_landing_point.java`** - MODIFIED
   - Integrated city landing tax collection
   - Ships ejected from city if cannot pay tax

### Data Files

7. **`datatables/city/city_rank.tab`** - MODIFIED
   - Added Rank 6 (Metropolis): 600m radius, 55 population

8. **`database/updates/city_update_001.sql`** - NEW
   - SQL schema for new tables:
     - `city_terrain_regions`
     - `city_bulldoze`
     - `city_evictions`
     - `city_judges`
     - `city_judge_elections`
     - `city_judge_votes`
     - `city_extended_taxes`

9. **`string/city_update_strings.tab`** - NEW
   - String table entries for all new features

---

## Features Implemented

### Phase 1: Foundation ✅
- [x] Add Rank 6 to city_rank datatable
- [x] Update RANK_MAX constant
- [x] Add new permission flags (CP_JUDGE)
- [x] Add structure flag for Industrial Starport

### Phase 2: Core Systems ✅
- [x] City Expulsion Warning System
  - [x] 120 second grace period
  - [x] Militia can attack after grace expires
  - [x] TEF restrictions on building/terminal access
  - [x] Automatic clear when leaving city bounds
- [x] Enhanced Taxation
  - [x] Crafting tax (0-10%)
  - [x] Vendor license fee (0-5000 credits/week)
  - [x] Structure placement fee (0-25000 credits)
  - [x] Starship landing tax (0-50000 credits)
- [x] Judge Election System
  - [x] 3-day election duration
  - [x] Candidate registration
  - [x] Vote casting
  - [x] Automatic finalization

### Phase 3: Major Features ✅
- [x] Eviction/Court System
  - [x] Mayor can initiate eviction
  - [x] 7-day grace period
  - [x] Appeal filing
  - [x] Judge review and decision
- [x] Starship Landing Tax Integration
  - [x] Tax collected on landing
  - [x] Ship ejected if cannot pay

### Phase 4: Terrain Systems (Planned)
- [ ] Radius Terrain Painting
- [ ] Road Terrain Painting
- [ ] City Bulldoze System

---

## Installation Instructions

1. **Database Updates**
   Run the SQL script to create new tables:
   ```sql
   @dsrc/sku.0/sys.server/compiled/game/database/updates/city_update_001.sql
   ```

2. **String Table Updates**
   Add entries from `string/city_update_strings.tab` to `city/city.stf`

3. **Compile Scripts**
   ```bash
   ant compile_java
   ```

4. **Rebuild Datatable**
   Rebuild `city_rank.iff` from the updated `.tab` file

---

## Testing Checklist

### Expulsion System
- [ ] Mayor/militia can expel players
- [ ] 120 second grace period works
- [ ] Player becomes attackable after grace
- [ ] TEF prevents building entry
- [ ] TEF prevents terminal usage
- [ ] Leaving city clears expulsion

### Judge Elections
- [ ] Mayor can start election
- [ ] Citizens can register as candidates
- [ ] Citizens can vote
- [ ] Election finalizes after 3 days
- [ ] Winners become judges

### Eviction System
- [ ] Mayor can initiate eviction
- [ ] Citizen receives mail notification
- [ ] Appeal can be filed
- [ ] Judges can review and decide
- [ ] Grace period enforced

### Extended Taxes
- [ ] All tax types configurable
- [ ] Landing tax collected
- [ ] Ships ejected if cannot pay
- [ ] Treasury receives funds

