# ADR-131 — 100 m / 64×64 / Fixed-2-Hz Dynamic Surface Tiles

**Status:** Accepted — current architecture

## Context

The sparse Dynamic Surface design evolved into a 4096×4096 logical domain per 100 m sheet, 6.25 m physical sub-pages, finer authority controls and multiple distance cadence bands. Even after DSURF04B removed obvious full-triangle rescans and global dirtiness, the live system remained too expensive for the project's performance target.

The project owner explicitly selected a simpler model: 100 m × 100 m tiles, 64×64 texture/state resolution and 2 Hz polling.

## Decision

For Hydro and Track:

1. A 100 m FP64 Dynamic Surface chunk remains the horizontal world address.
2. Every connected surface sheet in that chunk owns exactly one tile/page.
3. That tile is exactly 64×64 authoritative cells and 64×64 base GPU texels (1.5625 m/cell).
4. Every active tile advances at a fixed 2 Hz.
5. A tile is active within 1000 m of any real simulation-interest source and dormant outside that union.
6. No synthetic midpoint source is permitted for split/local players.
7. The former 4096 logical domain, 256 physical-page hierarchy, 6.25 m sub-pages and 30/20/6/2 Hz bands are retired from the live path.
8. Static surface queries use 64×64 per-tile acceleration bins, and authority state copies directly to GPU mip0.
9. Vertical surface-sheet separation, conservative Hydro transfer, local Track temperature and persistent world identity remain mandatory.

## Consequences

CPU scheduling, residency and raster work become far smaller and more predictable. GPU page dimensions also fall to 64×64, and the tile-indirection texture can be much smaller and updated only when its origin/table generation changes.

The deliberate cost is lower spatial and temporal simulation fidelity: 1.5625 m cells and 0.5 s state steps. Optical rain film, ripples, micro-wetness and breakup must therefore come from filtered/procedural material presentation rather than from centimetre-scale persistent state.

A 100 m tile boundary is still an implementation boundary, never a visible or physical seam. Multiple vertical sheets in one X/Z tile remain independent.

## Supersedes

This ADR supersedes the **live resolution/page hierarchy and Dynamic Surface cadence portions** of earlier DSURF decisions, including the corresponding runtime assumptions in ADR-128 and the earlier adaptive-cadence policy. Those records remain historical evidence for why the system evolved; their persistence, surface-sheet, no-midpoint and world-authority requirements remain applicable where not contradicted here.
