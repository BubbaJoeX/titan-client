[CmdletBinding()]
param(
    [switch]$InvalidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host @"
Dynamic bunker — zero-setup usage
=================================

In-game (preferred):
  1. Enter any POB building (stand in a cell)
  2. Type:  /dynamicBunker
  3. Pick a room + snap socket in the floorplan UI → Assign

No terminal, scripts, or objvars are required for that path.

Optional terminal (radial convenience only):
  attachScript(obj, "terminal.terminal_dynamic_bunker");
  — still auto-detects sockets; objvars are optional.

Room catalog (server runtime):
  DynamicBunkerRoomCatalog scans building/installation templates → unique .pob
  → one catalog entry per cell/portal
  roomId = dyn|<donorPob>|<cellIndex>|<portalIndex>

Deploy:
  - Rebuild GameServer (serverGame + serverScript) for /dynamicBunker
  - Compile command_table.tab → command_table.iff
  - Redeploy Java scripts if using the terminal path
  - Client with floorplan UI + DynamicBunkerMessages
"@

if ($InvalidateOnly) {
    Write-Host "Cache invalidation: DynamicBunkerRoomCatalog::invalidateCache() or restart GameServer."
}
