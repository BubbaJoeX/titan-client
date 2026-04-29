# Developer Commands Documentation

**Script:** `player_developer.java`  
**Author:** BubbaJoeX  
**Purpose:** Developer script for Titan 
**Copyright:** © SWG: Titan 2024

## Important Notes

⚠️ **Warning:** This script contains many unhandled and unchecked operations. Use at your own risk. This script also contains code cherry-picked from SWG-Source/dsrc:3.1.

📝 **Usage:** Please target yourself before running any command unless it returns a message to not to.

---

## Table of Contents

- [Server/Admin Commands](#serveradmin-commands)
- [Object/Spawn Commands](#objectspawn-commands)
- [Player/Character Commands](#playercharacter-commands)
- [Item/Equipment Commands](#itemequipment-commands)
- [Location/Travel Commands](#locationtravel-commands)
- [Script/Development Commands](#scriptdevelopment-commands)
- [UI/Effect Commands](#uieffect-commands)
- [Bonus/Configuration Commands](#bonusconfiguration-commands)
- [Collection/Quest Commands](#collectionquest-commands)
- [Housing Commands](#housing-commands)
- [Miscellaneous Commands](#miscellaneous-commands)

---

## Server/Admin Commands

### database
**Syntax:** `/developer database`  
**Description:** Opens database query window (currently commented out)

### reloadAllScripts
**Syntax:** `/developer reloadAllScripts`  
**Description:** Reloads all scripts on the server

### reloadTable
**Syntax:** `/developer reloadTable <table>`  
**Description:** Reloads a specific datatable  
**Parameters:**
- `<table>` - Name of the datatable to reload

### shutdown
**Syntax:** `/developer shutdown`  
**Description:** Initiates server shutdown (15 minutes warning). Sends Discord webhook notification.

### getConfigSetting
**Syntax:** `/developer getConfigSetting <section> <key>`  
**Description:** Retrieves a configuration setting value  
**Parameters:**
- `<section>` - Configuration section name
- `<key>` - Configuration key name

### environment
**Syntax:** `/developer environment get <key>`  
**Description:** Gets an environment variable value from the OS  
**Parameters:**
- `get` - Subcommand to retrieve value
- `<key>` - Environment variable key name

### webhook
**Syntax:** `/developer webhook <message>`  
**Description:** Sends a message to Discord webhook

### compileScripts / cs
**Syntax:** `/developer compileScripts` or `/developer cs`  
**Description:** Compiles all scripts

### compileAndReloadScript / crs
**Syntax:** `/developer compileAndReloadScript <script>` or `/developer crs <script>`  
**Description:** Compiles and reloads a specific script  
**Parameters:**
- `<script>` - Script name to compile and reload

### reload
**Syntax:** `/developer reload`  
**Description:** Reloads scripts (alias)

---

## Object/Spawn Commands

### spellObject
**Syntax:** `/developer spellObject <template> <text>`  
**Description:** Spells out text using objects placed in formation  
**Parameters:**
- `<template>` - Object template to use for letters
- `<text>` - Text to spell out

### sws / spawnWithScript
**Syntax:** `/developer sws <template> <script>` or `/developer spawnWithScript <template> <script>`  
**Description:** Spawns an object with a script attached  
**Parameters:**
- `<template>` - Object template to spawn
- `<script>` - Script to attach to the object

### replace
**Syntax:** `/developer replace <template>`  
**Description:** Replaces the target object with a new object of specified template  
**Parameters:**
- `<template>` - Template of replacement object (must end with .iff)

### clone
**Syntax:** `/developer clone`  
**Description:** Clones the target object at your location

### cloneSpawner
**Syntax:** `/developer cloneSpawner`  
**Description:** Clones the target spawner

### copy
**Syntax:** `/developer copy <subcommand> [options]`  
**Description:** Copy operations for objects  
**Subcommands:**
- `-onto` - Copies target template and spawns at your location
- `-into` - Copies target template into your inventory
- `-template copy` - Copies target's template to clipboard
- `-template paste` - Pastes template from clipboard (spawns object)
- `-template clear` - Clears template from clipboard

### swap
**Syntax:** `/developer swap`  
**Description:** Swaps positions of intended target and main target

### scale
**Syntax:** `/developer scale <float>`  
**Description:** Resizes the target object  
**Parameters:**
- `<float>` - Scale factor (e.g., 1.5 for 150% size)

### ringspawn
**Syntax:** `/developer ringspawn <creatureTemplate> <num> <radius>`  
**Description:** Spawns creatures in a ring formation  
**Parameters:**
- `<creatureTemplate>` - Creature template to spawn
- `<num>` - Number of creatures
- `<radius>` - Ring radius in meters

### ringspawninside
**Syntax:** `/developer ringspawninside <creatureTemplate> <num> <radius>`  
**Description:** Spawns creatures in a ring formation inside a building  
**Parameters:**
- `<creatureTemplate>` - Creature template to spawn
- `<num>` - Number of creatures
- `<radius>` - Ring radius in meters

### boxspawn
**Syntax:** `/developer boxspawn`  
**Description:** Spawns objects in a box formation

### createGrid
**Syntax:** `/developer createGrid <creatureTemplate> <num> <spacing> <radius>`  
**Description:** Creates a grid formation of creatures  
**Parameters:**
- `<creatureTemplate>` - Creature template
- `<num>` - Number of creatures
- `<spacing>` - Grid spacing
- `<radius>` - Grid radius

### createArch
**Syntax:** `/developer createArch <creatureTemplate> <num> <columns> <radius>`  
**Description:** Creates an arch formation of creatures  
**Parameters:**
- `<creatureTemplate>` - Creature template
- `<num>` - Number of creatures
- `<columns>` - Number of columns
- `<radius>` - Arch radius

### createBarker
**Syntax:** `/developer createBarker`  
**Description:** Opens a menu to select a template and create a barker NPC

### markObjects
**Syntax:** `/developer markObjects`  
**Description:** Marks objects in range

### getObjectsNear
**Syntax:** `/developer getObjectsNear`  
**Description:** Gets objects near your location

### templateSearch
**Syntax:** `/developer templateSearch <searchTerm>`  
**Description:** Searches for object templates by term  
**Parameters:**
- `<searchTerm>` - Search term to match in template names

### findTemplate
**Syntax:** `/developer findTemplate <template>`  
**Description:** Finds objects by template name  
**Parameters:**
- `<template>` - Template name to search for

---

## Player/Character Commands

### heal
**Syntax:** `/developer heal`  
**Description:** Heals you to full health

### setHair
**Syntax:** `/developer setHair`  
**Description:** Sets your hair to match the target's hair template (target must have hair equipped)

### exotics
**Syntax:** `/developer exotics <skillmod> <value>`  
**Description:** Applies an exotic skill modifier to target  
**Parameters:**
- `<skillmod>` - Skill modifier name
- `<value>` - Value (1-350, expertise max 35)

### uberize
**Syntax:** `/developer uberize <type>`  
**Description:** Applies maximum skillmods to target  
**Parameters:**
- `<type>` - Type: `crafting`, `combat`, or `all`

### skillmods
**Syntax:** `/developer skillmods <subcommand> <skillmod> <amount>`  
**Description:** Manages skill modifiers  
**Subcommands:**
- `add` - Adds a skill modifier
- `remove` - Removes a skill modifier  
**Parameters:**
- `<skillmod>` - Skill modifier name
- `<amount>` - Modifier value

### grantAllSkills
**Syntax:** `/developer grantAllSkills`  
**Description:** Grants all skills to target

### grantAllSchematics
**Syntax:** `/developer grantAllSchematics`  
**Description:** Grants all schematics to target

### grantAllSchematicsByGroup
**Syntax:** `/developer grantAllSchematicsByGroup <group>`  
**Description:** Grants schematics by group to target  
**Parameters:**
- `<group>` - Schematic group name

### grantAllItems
**Syntax:** `/developer grantAllItems`  
**Description:** Grants all items to target

### grantAllItemsBySearch
**Syntax:** `/developer grantAllItemsBySearch <searchTerm>`  
**Description:** Grants items matching search term to target  
**Parameters:**
- `<searchTerm>` - Search term to match item names

### seedAllSchematics
**Syntax:** `/developer seedAllSchematics`  
**Description:** Seeds all schematics to target

### seedAllSchematicsByType
**Syntax:** `/developer seedAllSchematicsByType <type>`  
**Description:** Seeds schematics by type to target  
**Parameters:**
- `<type>` - Schematic type (e.g., object/draft_schematic/[TYPE]/...)

### makePet
**Syntax:** `/developer makePet <creatureTemplate>`  
**Description:** Creates a pet from creature template and gives control device  
**Parameters:**
- `<creatureTemplate>` - Creature template name

### locomotion
**Syntax:** `/developer locomotion <state>`  
**Description:** Sets locomotion state  
**Parameters:**
- `<state>` - Locomotion state integer

### state
**Syntax:** `/developer state <toggle> <state>`  
**Description:** Sets or toggles a state  
**Subcommands:**
- `on` - Turns state on
- `off` - Turns state off  
**Parameters:**
- `<state>` - State integer

### posture
**Syntax:** `/developer posture <posture>`  
**Description:** Sets posture  
**Parameters:**
- `<posture>` - Posture integer

### toggle
**Syntax:** `/developer toggle <on|off>`  
**Description:** Toggles visibility (invisible/visible)  
**Parameters:**
- `on` - Makes you invisible
- `off` - Makes you visible

### players
**Syntax:** `/developer players`  
**Description:** Lists all players

### listAllPlayersPlanetside
**Syntax:** `/developer listAllPlayersPlanetside`  
**Description:** Lists all players planetside in a SUI window

### planetPopulation
**Syntax:** `/developer planetPopulation`  
**Description:** Shows planet population count

### findClosestPlayer
**Syntax:** `/developer findClosestPlayer [range]`  
**Description:** Finds the closest player to you  
**Parameters:**
- `[range]` - Optional search range (default if not specified)

### findPlayers
**Syntax:** `/developer findPlayers`  
**Description:** Finds players in range

### noafk
**Syntax:** `/developer noafk`  
**Description:** Prevents AFK status

### invulnerable
**Syntax:** `/developer invulnerable`  
**Description:** Makes target invulnerable

### height
**Syntax:** `/developer height <value>`  
**Description:** Sets height of target  
**Parameters:**
- `<value>` - Height value

### align
**Syntax:** `/developer align`  
**Description:** Aligns object to terrain

### animate
**Syntax:** `/developer animate <animation>`  
**Description:** Plays animation on target  
**Parameters:**
- `<animation>` - Animation name

### makeEnt
**Syntax:** `/developer makeEnt`  
**Description:** Creates an entertainer NPC

### makeUtilitySpawner
**Syntax:** `/developer makeUtilitySpawner`  
**Description:** Opens menu to create utility spawner (Medical Droid, Tactical Probe, Entertainer, Artisan)

---

## Item/Equipment Commands

### getItemList
**Syntax:** `/developer getItemList [searchParam]`  
**Description:** Lists items matching search parameter  
**Parameters:**
- `[searchParam]` - Optional search parameter

### generateComponent
**Syntax:** `/developer generateComponent <item> <sourceObjId>`  
**Description:** Generates a component item  
**Parameters:**
- `<item>` - Item template name
- `<sourceObjId>` - Source object ID for crafting

### craft
**Syntax:** `/developer craft <schematic>`  
**Description:** Crafts an item from schematic  
**Parameters:**
- `<schematic>` - Schematic template name

### socketize
**Syntax:** `/developer socketize <amount>`  
**Description:** Adds sockets to target armor/clothing/weapon  
**Parameters:**
- `<amount>` - Number of sockets to add

### editWeapon
**Syntax:** `/developer editWeapon <mod> <value>`  
**Description:** Edits weapon statistics  
**Parameters:**
- `<mod>` - Modification type: `minDamage`, `maxDamage`, `attackSpeed`, `woundChance`, `attackCost`, `accuracy`, `elementalType`, `elementalValue`, `rangeInfo`, `damageType`, `damageRadius`, `resetAllStats`
- `<value>` - Value to set

### editVehicle
**Syntax:** `/developer editVehicle <modIndex> <modValue>`  
**Description:** Edits vehicle modifications  
**Parameters:**
- `<modIndex>` - Modification index
- `<modValue>` - Modification value

### makeAugs
**Syntax:** `/developer makeAugs`  
**Description:** Creates augments

### droidSockets
**Syntax:** `/developer droidSockets`  
**Description:** Manages droid sockets

### describe
**Syntax:** `/developer describe`  
**Description:** Shows description information for target

### staticItemDetails
**Syntax:** `/developer staticItemDetails [-t (target)] | [-s (string)]`  
**Description:** Displays details about a static item  
**Parameters:**
- `-t` - Use target object
- `-s` - Use string name

### listStaticContents
**Syntax:** `/developer listStaticContents [itemName]`  
**Description:** Lists static contents, optionally filtered by item name  
**Parameters:**
- `[itemName]` - Optional item name filter

### staticItemStack
**Syntax:** `/developer staticItemStack`  
**Description:** Creates a stack of static items

### randomizeContainer
**Syntax:** `/developer randomizeContainer`  
**Description:** Randomizes contents of a container

### tagContainerContents
**Syntax:** `/developer tagContainerContents <tag>`  
**Description:** Tags all items in a container with specified tag (formats item names)  
**Parameters:**
- `<tag>` - Tag to add to item names

### renameContainerContents
**Syntax:** `/developer renameContainerContents <name>`  
**Description:** Renames all items in a container  
**Parameters:**
- `<name>` - New name for all items

### revertContainerContents
**Syntax:** `/developer revertContainerContents`  
**Description:** Resets items in container to their template paths

### lockContainer
**Syntax:** `/developer lockContainer`  
**Description:** Applies noTrade objvar and attaches item.special.nomove script

### unlockContainer
**Syntax:** `/developer unlockContainer`  
**Description:** Removes noTrade objvar and detaches item.special.nomove script

### touchContainer
**Syntax:** `/developer touchContainer`  
**Description:** Touches/refreshes container contents

### setcount
**Syntax:** `/developer setcount <amount>`  
**Description:** Sets stack count of target item  
**Parameters:**
- `<amount>` - Stack count

### setcountcontainer
**Syntax:** `/developer setcountcontainer <amount>`  
**Description:** Sets stack count for all items in container  
**Parameters:**
- `<amount>` - Stack count

### give
**Syntax:** `/developer give <flag> [options]`  
**Description:** Gives items or credits  
**Subcommands:**
- `credits add` - Adds credits (STIPEND amount)
- `credits remove <amount>` - Removes credits  
- `gear` - Gives gear

---

## Location/Travel Commands

### getCellIds
**Syntax:** `/developer getCellIds <cellId>`  
**Description:** Gets all cell IDs for a building  
**Parameters:**
- `<cellId>` - Cell object ID

### putInCell
**Syntax:** `/developer putInCell <cellId>`  
**Description:** Warps you into a cell  
**Parameters:**
- `<cellId>` - Cell object ID

### moveInCell
**Syntax:** `/developer moveInCell <cellName> <x> <y> <z>`  
**Description:** Moves you to a location within a cell  
**Parameters:**
- `<cellName>` - Cell name
- `<x>` - X coordinate
- `<y>` - Y coordinate
- `<z>` - Z coordinate

### travel
**Syntax:** `/developer travel <subcommand> [options]`  
**Description:** Manages travel points  
**Subcommands:**
- `add <cost> <name>` - Adds a travel point at your location
- `remove <name>` - Removes a travel point  
**Parameters:**
- `<cost>` - Travel cost in credits
- `<name>` - Point name (can include spaces)

### path
**Syntax:** `/developer path`  
**Description:** Creates a path to your location

### pathToTargetPlanet
**Syntax:** `/developer pathToTargetPlanet`  
**Description:** Creates a path to target planet

### distance
**Syntax:** `/developer distance`  
**Description:** Calculates distance between intended target and look-at target

### getCollision
**Syntax:** `/developer getCollision`  
**Description:** Gets collision information

### heatMap
**Syntax:** `/developer heatMap <filename>`  
**Description:** Generates a heatmap of objects  
**Parameters:**
- `<filename>` - Filename to save heatmap data

### clipboard
**Syntax:** `/developer clipboard <type>`  
**Description:** Copies information to clipboard (displayed in SUI)  
**Subcommands:**
- `location` - Copies your current location
- `scripts` - Copies scripts from target
- `objvars` - Copies objvars from target
- `template` - Copies template information

---

## Script/Development Commands

### scriptvar
**Syntax:** `/developer scriptvar`  
**Description:** Manages script variables

### messageto
**Syntax:** `/developer messageto <message> <float>`  
**Description:** Sends a message to target  
**Parameters:**
- `<message>` - Message name
- `<float>` - Delay in seconds

### messagetoparams
**Syntax:** `/developer messagetoparams <message> <params>`  
**Description:** Sends a message with parameters to target  
**Parameters:**
- `<message>` - Message name
- `<params>` - Parameters dictionary

### getCRC
**Syntax:** `/developer getCRC <string>`  
**Description:** Converts a string to CRC  
**Parameters:**
- `<string>` - String to convert

### shell
**Syntax:** `/developer shell <directory> <command>`  
**Description:** Runs a shell command on server and returns output  
**Parameters:**
- `<directory>` - Working directory
- `<command>` - Command and parameters to execute

### workbench
**Syntax:** `/developer workbench`  
**Description:** Opens workbench UI for creating objects with scripts and properties

### adminPanel
**Syntax:** `/developer adminPanel`  
**Description:** Opens admin panel

### lookupPlayer
**Syntax:** `/developer lookupPlayer`  
**Description:** Looks up player information

### getPrompts
**Syntax:** `/developer getPrompts`  
**Description:** Gets prompts from target

### getAllResponses
**Syntax:** `/developer getAllResponses`  
**Description:** Gets all responses

### getAllPrompts
**Syntax:** `/developer getAllPrompts`  
**Description:** Gets all prompts

---

## UI/Effect Commands

### playeffect
**Syntax:** `/developer playeffect <effectName>`  
**Description:** Plays client effect on yourself  
**Parameters:**
- `<effectName>` - Effect name

### playeffecttarget
**Syntax:** `/developer playeffecttarget <effectName>`  
**Description:** Plays client effect on target  
**Parameters:**
- `<effectName>` - Effect name

### playeffectloc
**Syntax:** `/developer playeffectloc <effectName>`  
**Description:** Plays client effect at your location  
**Parameters:**
- `<effectName>` - Effect name

### playeffectloctarget
**Syntax:** `/developer playeffectloctarget <effectName>`  
**Description:** Plays client effect at target's location  
**Parameters:**
- `<effectName>` - Effect name

### playeffectatloc
**Syntax:** `/developer playeffectatloc <effectName> <x> <y> <z>`  
**Description:** Plays client effect at specified coordinates  
**Parameters:**
- `<effectName>` - Effect name
- `<x>` - X coordinate
- `<y>` - Y coordinate
- `<z>` - Z coordinate

### playcefeveryone
**Syntax:** `/developer playcefeveryone <effectName>`  
**Description:** Plays client effect for everyone  
**Parameters:**
- `<effectName>` - Effect name

### playsound
**Syntax:** `/developer playsound <soundName>`  
**Description:** Plays sound for yourself  
**Parameters:**
- `<soundName>` - Sound name

### playsoundtarget
**Syntax:** `/developer playsoundtarget <soundName>`  
**Description:** Plays sound for target  
**Parameters:**
- `<soundName>` - Sound name

### playsoundloc
**Syntax:** `/developer playsoundloc <soundName>`  
**Description:** Plays sound at your location  
**Parameters:**
- `<soundName>` - Sound name

### playsoundloctarget
**Syntax:** `/developer playsoundloctarget <soundName>`  
**Description:** Plays sound at target's location  
**Parameters:**
- `<soundName>` - Sound name

### playsoundatloc
**Syntax:** `/developer playsoundatloc <soundName> <x> <y> <z>`  
**Description:** Plays sound at specified coordinates  
**Parameters:**
- `<soundName>` - Sound name
- `<x>` - X coordinate
- `<y>` - Y coordinate
- `<z>` - Z coordinate

### playsoundeveryone
**Syntax:** `/developer playsoundeveryone <soundName>`  
**Description:** Plays sound for everyone  
**Parameters:**
- `<soundName>` - Sound name

### playmusic
**Syntax:** `/developer playmusic <musicName>`  
**Description:** Plays music for yourself  
**Parameters:**
- `<musicName>` - Music name

### playmusictarget
**Syntax:** `/developer playmusictarget <musicName>`  
**Description:** Plays music for target  
**Parameters:**
- `<musicName>` - Music name

### flytext
**Syntax:** `/developer flytext <text>`  
**Description:** Displays flytext for yourself  
**Parameters:**
- `<text>` - Text to display

### flytextTarget
**Syntax:** `/developer flytextTarget <text>`  
**Description:** Displays flytext for target  
**Parameters:**
- `<text>` - Text to display

### smite
**Syntax:** `/developer smite`  
**Description:** Smites target (lightning effect)

### shazam
**Syntax:** `/developer shazam`  
**Description:** Creates dramatic effect (lightning, sound)

### rainbowBroadcast
**Syntax:** `/developer rainbowBroadcast <message>`  
**Description:** Sends rainbow-colored broadcast message  
**Parameters:**
- `<message>` - Message text

### createSoundEmitter
**Syntax:** `/developer createSoundEmitter`  
**Description:** Creates a sound emitter object

### changeLights
**Syntax:** `/developer changeLights`  
**Description:** Changes lighting

### sendWarning
**Syntax:** `/developer sendWarning <message>`  
**Description:** Sends warning message  
**Parameters:**
- `<message>` - Warning message

### notifyGalaxy
**Syntax:** `/developer notifyGalaxy <message>`  
**Description:** Sends system message to entire galaxy  
**Parameters:**
- `<message>` - Message text

### sendFeed
**Syntax:** `/developer sendFeed <message>`  
**Description:** Sends feed message  
**Parameters:**
- `<message>` - Feed message

### url
**Syntax:** `/developer url <url>`  
**Description:** Opens URL in browser  
**Parameters:**
- `<url>` - URL to open

### uitest
**Syntax:** `/developer uitest`  
**Description:** Tests UI functionality

### sliderTest
**Syntax:** `/developer sliderTest`  
**Description:** Tests slider UI component

### countdownTest
**Syntax:** `/developer countdownTest`  
**Description:** Tests countdown timer

---

## Bonus/Configuration Commands

### setbonus
**Syntax:** `/developer setbonus <type> <value>`  
**Description:** Sets server bonus multipliers  
**Parameters:**
- `<type>` - Bonus type: `heroics`, `worldboss`, `duty`, `entertainer`, `gcw`, `gcw_points`, `xp`, `battlefields`, `ship_creation`, `ship_bonus`, `junk_dealer`, `mission`, `mission_bh`, `mission_gcw`, `mission_pve`, `mission_jedi_bounty`, `cashLoot`, `creature_harvesting`
- `<value>` - Multiplier value (heroics uses float, others use integer)

**RLS Subcommands:**
- `setbonus rls maxDifferenceBelow <value>` - Max levels below player level
- `setbonus rls maxDifferenceAbove <value>` - Max levels above player level
- `setbonus rls rare <value>` - Rare chest drop chance
- `setbonus rls exceptional <value>` - Exceptional chest drop chance
- `setbonus rls legendary <value>` - Legendary chest drop chance
- `setbonus rls minDistance <value>` - Min distance for next chest
- `setbonus rls minTime <value>` - Min time between chests (in minutes)
- `setbonus rls setGroupLoot <true|false>` - Enable/disable group loot

### revertbonuses
**Syntax:** `/developer revertbonuses`  
**Description:** Resets all bonuses to default values (1x or 1.0x)

### getbonuses
**Syntax:** `/developer getbonuses`  
**Description:** Displays all current server bonus values in a SUI window

### toggleRLS
**Syntax:** `/developer toggleRLS`  
**Description:** Toggles Rare Loot System on/off

### defaultRLS
**Syntax:** `/developer defaultRLS`  
**Description:** Sets default RLS values (10/10/65/25/10)

### toggleVendorCosts
**Syntax:** `/developer toggleVendorCosts`  
**Description:** Toggles vendor costs on/off (useful for debugging)

---

## Collection/Quest Commands

### modifyCollection
**Syntax:** `/developer modifyCollection <collection> <value>`  
**Description:** Modifies collection slot value (can be negative)  
**Parameters:**
- `<collection>` - Collection name
- `<value>` - Value to add/subtract

### resetCollection
**Syntax:** `/developer resetCollection <collection>`  
**Description:** Resets all slots in a collection to 0  
**Parameters:**
- `<collection>` - Collection name

### getCollectionReward
**Syntax:** `/developer getCollectionReward <collection>`  
**Description:** Gives you the reward for completing a collection  
**Parameters:**
- `<collection>` - Collection name

### bypassCollectionTimer
**Syntax:** `/developer bypassCollectionTimer`  
**Description:** Bypasses collection timer until logout

---

## Housing Commands

### housing
**Syntax:** `/developer housing layout`  
**Description:** Shows import/export options for housing layouts  
**Subcommands:**
- `layout` - Opens housing layout import/export menu (must be inside a building)

### exportBuilding
**Syntax:** `/developer exportBuilding <filename>`  
**Description:** Exports building contents to file  
**Parameters:**
- `<filename>` - Filename for export

### housingTable
**Syntax:** `/developer housingTable`  
**Description:** Displays housing table

### decorate
**Syntax:** `/developer decorate`  
**Description:** Opens decoration interface

### decorationIncrement
**Syntax:** `/developer decorationIncrement`  
**Description:** Increments decoration placement

### restrictArea
**Syntax:** `/developer restrictArea <radius> <volumeSuffix>`  
**Description:** Creates restricted area at location  
**Parameters:**
- `<radius>` - Restriction radius
- `<volumeSuffix>` - Volume suffix identifier

### unrestrictArea
**Syntax:** `/developer unrestrictArea`  
**Description:** Removes restricted area (target the expel volume)

### persistArea
**Syntax:** `/developer persistArea`  
**Description:** Persists area settings

---

## Miscellaneous Commands

### bounty
**Syntax:** `/developer bounty <subcommand> <name>`  
**Description:** Manages Jedi bounties  
**Subcommands:**
- `set <name>` - Sets bounty on player (opens SUI)
- `clear <name>` - Clears bounty on player  
**Parameters:**
- `<name>` - Player first name

### awardBadge
**Syntax:** `/developer awardBadge <badge>`  
**Description:** Awards a badge to target  
**Parameters:**
- `<badge>` - Badge name

### sendMail
**Syntax:** `/developer sendMail <from> <subject> <body>`  
**Description:** Sends fake mail to players in range  
**Parameters:**
- `<from>` - Sender name
- `<subject>` - Mail subject
- `<body>` - Mail body text

### sendMailWaypoint
**Syntax:** `/developer sendMailWaypoint`  
**Description:** Sends mail with waypoint

### say
**Syntax:** `/developer say <message>`  
**Description:** Makes target speak a message  
**Parameters:**
- `<message>` - Message text

### objectsay
**Syntax:** `/developer objectsay <message>`  
**Description:** Makes target object say a message  
**Parameters:**
- `<message>` - Message text

### comm
**Syntax:** `/developer comm <message>`  
**Description:** Makes target speak in comm window  
**Parameters:**
- `<message>` - Message text

### commPlanet
**Syntax:** `/developer commPlanet <message>`  
**Description:** Sends comm message planetwide  
**Parameters:**
- `<message>` - Message text

### wiki
**Syntax:** `/developer wiki <search>`  
**Description:** Opens wiki page in browser  
**Parameters:**
- `<search>` - Search term

### toggleRadarMap
**Syntax:** `/developer toggleRadarMap`  
**Description:** Toggles radar and map visibility for target NPC

### setHologram
**Syntax:** `/developer setHologram <type>`  
**Description:** Sets hologram type for target NPC (0-4)  
**Parameters:**
- `<type>` - Hologram type (0-4)

### clearHologram
**Syntax:** `/developer clearHologram`  
**Description:** Resets hologram type for target NPC

### setloottable
**Syntax:** `/developer setloottable <table>`  
**Description:** Sets loot table for target  
**Parameters:**
- `<table>` - Loot table name

### setnumitems
**Syntax:** `/developer setnumitems <count>`  
**Description:** Sets number of loot items for target  
**Parameters:**
- `<count>` - Item count

### editlootarea
**Syntax:** `/developer editlootarea`  
**Description:** Edits loot area settings

### createLootableCorpse
**Syntax:** `/developer createLootableCorpse <table> <amount>`  
**Description:** Creates a lootable corpse container  
**Parameters:**
- `<table>` - Loot table name
- `<amount>` - Number of items

### createLootableCargo
**Syntax:** `/developer createLootableCargo <table> <amount>`  
**Description:** Creates a lootable cargo container  
**Parameters:**
- `<table>` - Loot table name
- `<amount>` - Number of items

### testLoot
**Syntax:** `/developer testLoot <table> <amount> <level>`  
**Description:** Tests loot generation in your inventory  
**Parameters:**
- `<table>` - Loot table name
- `<amount>` - Number of items
- `<level>` - Level for loot

### lootArea
**Syntax:** `/developer lootArea <range>`  
**Description:** Loots area within range  
**Parameters:**
- `<range>` - Loot range in meters

### createJunkCache
**Syntax:** `/developer createJunkCache <totalAmount> <minAmount> <maxAmount>`  
**Description:** Creates a container with junk items  
**Parameters:**
- `<totalAmount>` - Total number of items
- `<minAmount>` - Minimum stack per item
- `<maxAmount>` - Maximum stack per item

### junkSpawner
**Syntax:** `/developer junkSpawner`  
**Description:** Creates junk spawner

### findJunk
**Syntax:** `/developer findJunk`  
**Description:** Finds junk items

### rewardArea
**Syntax:** `/developer rewardArea <item> <count>`  
**Description:** Rewards area with items  
**Parameters:**
- `<item>` - Item template
- `<count>` - Item count

### killCredit
**Syntax:** `/developer killCredit`  
**Description:** Gives kill credit

### createCommandTriggerVolume
**Syntax:** `/developer createCommandTriggerVolume`  
**Description:** Creates command trigger volume

### pumpkin
**Syntax:** `/developer pumpkin <subcommand> [options]`  
**Description:** Creates pumpkins  
**Subcommands:**
- `single` - Creates a single pumpkin at target location
- `ring <num> <radius>` - Creates ring of pumpkins around target  
**Parameters:**
- `<num>` - Number of pumpkins
- `<radius>` - Ring radius

### findClosestPumpkin
**Syntax:** `/developer findClosestPumpkin`  
**Description:** Finds closest pumpkin

### specialPumpkin
**Syntax:** `/developer specialPumpkin`  
**Description:** Creates special pumpkin

### tree
**Syntax:** `/developer tree <subcommand> [options]`  
**Description:** Creates trees  
**Subcommands:**
- `single` - Creates a single tree at target location
- `ring <num> <radius>` - Creates ring of trees around target  
**Parameters:**
- `<num>` - Number of trees
- `<radius>` - Ring radius

### treatPalette
**Syntax:** `/developer treatPalette`  
**Description:** Creates treat palette

### rugShowcase
**Syntax:** `/developer rugShowcase`  
**Description:** Creates spiral showcase of rugs (175 rugs)

### ballgame
**Syntax:** `/developer ballgame`  
**Description:** Creates a ball in your inventory

### createTaxiToken
**Syntax:** `/developer createTaxiToken [cost]`  
**Description:** Creates a taxi location token  
**Parameters:**
- `[cost]` - Optional travel cost (default if not specified)

### createTaxi
**Syntax:** `/developer createTaxi <index> <name>`  
**Description:** Creates a taxi door object  
**Parameters:**
- `<index>` - Taxi type (1-5): 1=Tantive4, 2=USV5, 3=Neutral Zonegate, 4=Rebel Zonegate, 5=Imperial Zonegate
- `<name>` - Taxi name

### shuttleRebelDrop
**Syntax:** `/developer shuttleRebelDrop`  
**Description:** Creates rebel shuttle drop

### shuttleImperialDrop
**Syntax:** `/developer shuttleImperialDrop`  
**Description:** Creates imperial shuttle drop

### snowspeeder
**Syntax:** `/developer snowspeeder`  
**Description:** Sets up snowspeeder as pet with abilities

### meddroid
**Syntax:** `/developer meddroid`  
**Description:** Creates medical droid

### areaSpawner
**Syntax:** `/developer areaSpawner`  
**Description:** Creates area spawner

### deployable
**Syntax:** `/developer deployable create <template> <script> <stack>`  
**Description:** Creates deployable item  
**Parameters:**
- `create` - Subcommand to create deployable
- `<template>` - Item template
- `<script>` - Script to attach
- `<stack>` - Stack size

### magicSatchel
**Syntax:** `/developer magicSatchel`  
**Description:** Creates magic satchel

### createUtilityEgg
**Syntax:** `/developer createUtilityEgg`  
**Description:** Creates utility spawner egg

### resourceDatapad
**Syntax:** `/developer resourceDatapad`  
**Description:** Creates resource datapad for generating removed resources

### tcgvoucher
**Syntax:** `/developer tcgvoucher`  
**Description:** Creates TCG voucher

### makeEventToken
**Syntax:** `/developer makeEventToken`  
**Description:** Creates event token

### adventPresent
**Syntax:** `/developer adventPresent`  
**Description:** Creates advent present

### awardBirthday
**Syntax:** `/developer awardBirthday`  
**Description:** Awards birthday item

### mortar
**Syntax:** `/developer mortar`  
**Description:** Creates mortar

### createSoundEmitter
**Syntax:** `/developer createSoundEmitter`  
**Description:** Creates sound emitter object

### stopmacros
**Syntax:** `/developer stopmacros`  
**Description:** Stops macros on target

### time
**Syntax:** `/developer time`  
**Description:** Shows server time

### mapLocations
**Syntax:** `/developer mapLocations <subcommand> [options]`  
**Description:** Manages map locations  
**Subcommands:**
- `add <category> <subcategory> <name>` - Adds map location at your position
- `remove <name>` - Removes map location  
**Parameters:**
- `<category>` - Location category
- `<subcategory>` - Location subcategory
- `<name>` - Location name

### sendPrompt
**Syntax:** `/developer sendPrompt <prompt>`  
**Description:** Sends prompt to AI and gets response  
**Parameters:**
- `<prompt>` - Prompt text

### listWattos
**Syntax:** `/developer listWattos`  
**Description:** Lists all Watto NPCs in range

### gotoWatto
**Syntax:** `/developer gotoWatto`  
**Description:** Warps you to nearest Watto NPC

### gjpud
**Syntax:** `/developer gjpud <flag>`  
**Description:** Gets JSON for player user data  
**Subcommands:**
- `workbench` - Gets workbench data

### targetTest
**Syntax:** `/developer targetTest`  
**Description:** Tests targeting

### itemTable
**Syntax:** `/developer itemTable`  
**Description:** Displays item table

### prepareStaticStrings
**Syntax:** `/developer prepareStaticStrings`  
**Description:** Exports static strings to files

### exportBuilding
**Syntax:** `/developer exportBuilding <filename>`  
**Description:** Exports building contents  
**Parameters:**
- `<filename>` - Export filename

### giveItemsForDWB
**Syntax:** `/developer giveItemsForDWB`  
**Description:** Gives items for DWB (Dynamic World Boss)

### buffAllByName
**Syntax:** `/developer buffAllByName <buffName>`  
**Description:** Applies buff to all players by name  
**Parameters:**
- `<buffName>` - Buff name

### entbuff
**Syntax:** `/developer entbuff`  
**Description:** Applies entertainer buffs

### forceUVTick
**Syntax:** `/developer forceUVTick`  
**Description:** Forces UV tick update

### addCharacterSlot
**Syntax:** `/developer addCharacterSlot`  
**Description:** Adds character slot (currently commented out)

### stats
**Syntax:** `/developer stats`  
**Description:** Shows player stats (currently commented out)

### database
**Syntax:** `/developer database`  
**Description:** Opens database query window (currently commented out)

### -help
**Syntax:** `/developer -help`  
**Description:** Displays help message with some common commands

---

## Command Aliases

- `cs` = `compileScripts`
- `crs` = `compileAndReloadScript`
- `sws` = `spawnWithScript`

---

## Notes

- Most commands require you to target yourself unless otherwise specified
- Many commands log usage to the server log with "[Developer]" prefix
- Some commands are commented out or disabled in the source code
- Commands that modify server state should be used with caution
- Always test commands in a safe environment before using in production

---

**Document Generated:** Based on analysis of `player_developer.java`  
**Total Commands Documented:** 216+  
**Last Updated:** Based on current codebase state