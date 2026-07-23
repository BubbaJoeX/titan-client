[CmdletBinding()]
param(
    [switch]$InvalidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

<#
.SYNOPSIS
  Dynamic bunker rooms are discovered at runtime by the GameServer.

  DynamicBunkerRoomCatalog scans the object template CRC table for building/
  installation shared templates, collects unique portalLayoutFilename (.pob)
  paths, then expands each POB into one catalog entry per cell/portal:

      roomId = dyn|<donorPob>|<cellIndex>|<portalIndex>

  No per-room datatable is required. The floorplan UI receives the live catalog
  when openDynamicBunkerFloorplan runs.

  This script only documents that workflow (and optional cache notes).
#>

Write-Host @"
Dynamic bunker room catalog is RUNTIME-DISCOVERED (no datatable seed needed).

Server class: DynamicBunkerRoomCatalog
  1) ObjectTemplateList::getAllTemplateNamesFromCrcStringTable()
  2) Filter object/building/**/shared_* and object/installation/**/shared_*
  3) Read SharedObjectTemplate::getPortalLayoutFilename()
  4) PortalPropertyTemplateList::fetch(pob) -> emit room per cell/portal
  5) Cache until DynamicBunkerRoomCatalog::invalidateCache()

roomId encoding (used by Assign):
  dyn|appearance/some_bunker.pob|1|0

Floorplan open path:
  terminal.terminal_dynamic_bunker -> openDynamicBunkerFloorplan()
  -> DynamicBunker::openFloorplan() -> DynamicBunkerRoomCatalog::buildCatalog()

Optional legacy tab (ignored by server catalog now):
  dsrc/.../datatables/building/dynamic_bunker_rooms.tab
"@

if ($InvalidateOnly) {
    Write-Host "Cache invalidation is a server API call (DynamicBunkerRoomCatalog::invalidateCache); restart GameServer or add a god command to flush."
}
