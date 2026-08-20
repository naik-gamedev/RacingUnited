# DSURF04C — 100 m / 64×64 / 2 Hz Dynamic Surface Tiles

## Status

Implemented as the current Hydro + Track architecture. This milestone supersedes the live resolution/cadence hierarchy introduced by DSURF00–04B while retaining their persistent world/surface-sheet authority and physical state semantics.

## User-directed performance contract

Dynamic Surface is deliberately coarse and predictable:

- one FP64 world chunk is **100 m × 100 m**;
- each connected surface sheet in that chunk owns **one** Dynamic Surface tile;
- each tile is exactly **64 × 64** authoritative Hydro/Track cells and GPU texels;
- spatial pitch is **1.5625 m/cell**;
- every active tile polls at **2 Hz**;
- a tile is active when its chunk AABB is within **1000 m** of any real local simulation-interest source;
- outside that bound the tile remains persistent but dormant;
- multiple local players activate the union of their real nearby tiles; no midpoint source is synthesized.

The previous 4096×4096 logical domain, 256×256 physical pages, 6.25 m sub-pages and 30/20/6/2 Hz distance bands are not part of the DSURF04C live path.

## Authority and layering

Hydro still owns standing water, moisture, flow, rainfall, infiltration, mapped drainage, evaporation and tire clearing/spray. Track.R still owns local surface temperature; Track G/B/A remain reserved for the DSURF05 rubber/marbles migration.

A 100 m XZ tile is not automatically one vertical surface. DSURF01 connected surface-sheet IDs remain authoritative, so a bridge deck, the road below it, a raised sidewalk and a tunnel can each own an independent 64×64 tile at the same X/Z chunk coordinate. Support-height sampling and cross-chunk sheet links preserve that separation.

## CPU hotpath

Static collision triangles are binned into the tile's 64×64 state cells at scene/static-surface construction time. Hydro and thermal static-cell construction query those bins rather than rescanning the whole 100 m chunk per cell.

The authority raster is already the desired GPU base raster. Hydro, Track and support data therefore copy directly at 64×64 mip0; no 16→64 presentation raster and no 4096-domain addressing remain. Coarser texture mips are generated only for presentation filtering.

The camera-local world-tile indirection table is 64×64 and is rebuilt/uploaded only when its 100 m world origin changes or the persistent page-table generation changes, rather than every frame.

## Temporal behavior

Active Hydro and Track tiles use one fixed 0.5 s update interval. This intentionally removes distance-cadence bookkeeping and slow-to-fast catch-up transitions. Tire physics remains free to run at its higher fixed timestep: it samples the latest tile state and accumulates bounded contact inputs until the next surface update.

Rendering should hide the coarse physics lattice through ordinary texture filtering and procedural/material optical detail. No shader or geometry may reveal 100 m tile seams.

## Persistence and flow

Tile dormancy is not state deletion. CPU Hydro/Track state stays associated with the persistent world/sheet address and resumes when the tile becomes relevant again. Conservative shallow-sheet transfer across 100 m chunk boundaries remains supported through DSURF01 cross-chunk sheet links.

## Acceptance gates

- Native DSURF01 bake/sheet regressions remain green.
- DSURF02 persistence/page-pool regressions remain green with one page per sheet/chunk.
- DSURF03 Hydro conservation, rain-cover and tire-clearing regressions remain green.
- DSURF04 thermal sheet separation and tire-heating regressions remain green.
- Split local sources do not activate the midpoint when it lies outside both real 1000 m interest regions.
- F8 reports fixed 2 Hz active tiles and no 30/20/6 Hz Dynamic Surface bands.
- Windows live profiling is the final performance gate; this milestone is specifically intended to eliminate the prior fine-page scheduling/raster overhead rather than preserve that hierarchy at lower quality.

## DSURF04C2 boundary-cell coverage
A 64×64 cell represents its full 1.5625 m square area. Static Hydro/Track support first tests the cheap cell-centre path, then uses projected triangle/cell intersection only for boundary cells. Narrow authored surfaces therefore remain represented without increasing tile resolution or cadence.
