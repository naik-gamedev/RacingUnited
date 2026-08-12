# ADR-057 — World-Owned Chunked SurfaceWorld

**Status:** CLEAN10 implementation candidate  
**Date:** 2026-08-10

## Context

TIRE15 introduced persistent rut, compaction, moisture, shear-history, displaced-volume and pass-count state through `SurfaceField`. The data was conceptually shared world state, but the prototype field lived inside `VehicleSystem` and keyed cells directly from local FP32 contact coordinates.

That creates two long-term problems. First, world state should be accessible to every vehicle plus weather, presentation, persistence, networking and scene tooling rather than being privately owned by the vehicle simulator. Second, Heritage Engine's floating origin changes local FP32 coordinates while preserving one FP64 absolute world. A field keyed directly from rebased local X/Z would therefore forget or misaddress previously driven terrain after an origin shift.

The original field also used one flat unordered map with a 16,384-cell budget and found the oldest cell by scanning the complete map whenever full. That prototype is too small and too expensive for long circuits/stages, large grids and later rubber/weather evolution.

## Decision

1. `PhysicsWorld` owns one authoritative `Physics/Surfaces/SurfaceWorld`.
2. `VehicleSystem` receives/consumes `SurfaceWorld` during simulation and does not own persistent world-surface memory.
3. `SurfaceWorld` owns the local-FP32 to global-FP64 coordinate conversion. `SurfaceField` itself accepts absolute `DVec3` coordinates only.
4. Floating-origin rebases update `SurfaceWorld`'s current FP64 origin; field keys remain absolute and require no re-key/rebase pass.
5. The deformable terrain field is sparse **inside spatial chunks**. Default addressing is 0.25 m X/Z cells, 64 cells per chunk edge (16 m tiles), plus a coarse 2 m global-Y layer so stacked roads/bridges do not alias, with explicit resident-cell and resident-chunk limits.
6. Resident chunks use LRU ordering. Capacity replacement evicts bounded chunks instead of scanning the entire field for an oldest cell.
7. Chunk snapshot/restore plus an eviction callback are the persistence/streaming seam. `SurfaceField` does not choose disk formats, network replication or presentation policy.
8. The existing `Physics/SurfaceField.hpp` remains a compatibility forwarding include during migration. Its `.cpp` is no longer compiled.
9. Tire providers emit physical state deltas; they no longer own spatial address policy.
10. TIRE15C track rubber/marbles are **not** folded into deformable terrain. A dedicated `Physics/Surfaces/Rubber/TrackRubberState.*` scaffold reserves ownership for deposited rubber, loose-rubber concentration/migration and later marble/pickup behavior. Bulk state may reuse world spatial concepts, but visible marble clusters and specialized debris remain rubber/presentation responsibilities.

## Consequences

- A driven rut survives a floating-origin shift because the same absolute contact point maps to the same global cell. Vertically stacked roads at the same X/Z remain distinct through the coarse global-Y layer.
- All vehicles in one `PhysicsWorld` naturally share one driven-surface history.
- Weather, visuals, persistence and multiplayer have a stable world-owned integration point.
- Long-track memory is bounded by tile/cell budgets without O(N)-over-the-world replacement scans.
- Streaming can persist evicted chunks later without redesigning tire physics.
- Surface-field addressing uses FP64 where world scale requires it while per-contact/local physics remains FP32/vehicle-precision as before.
- Existing TIRE15 terramechanics equations and update order remain unchanged; CLEAN10 changes ownership/addressing/storage architecture.
