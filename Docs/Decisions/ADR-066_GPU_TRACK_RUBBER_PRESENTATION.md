# ADR-066: GPU track-rubber presentation

Status: Accepted for TIRE16L

## Decision

Track-rubber physics remains authoritative in `TrackRubberState`; GPU presentation is a cache only.

Resting marble fields are presented through invisible 100 m x 100 m batching chunks. Each chunk owns an FP64 world origin while its compact visual records use chunk-local FP32 coordinates. Chunk boundaries never participate in marble placement, density, pile shape, maturity, opacity, aerodynamic migration, or LOD, so they must not be inferable from the image.

Each authoritative rubber cell is uploaded as one compact persistent GPU point record. A geometry shader reconstructs stable, seeded, two-triangle marble flakes. Unchanged pages are not re-uploaded. Changed cells patch only the affected page range, coalesced to one contiguous upload per dirty page per frame.

Airborne and mobile-ground rubber remains authoritative CPU simulation. Presentation uploads one compact record per bounded moving packet and expands its visible two-triangle representatives on the GPU rather than CPU-tessellating each representative.

Master LOD transition rules remain responsible for detailed-to-aggregate blending and outer visibility fading. Persistent/authoritative world state remains precise; presentation uses camera-relative or chunk-local FP32.

## Consequences

- Old/resting marble fields no longer generate thousands of CPU-side flake triangles every frame.
- CPU render submission scales mainly with visible GPU pages and changed cells, not visible individual marble representatives.
- Aerodynamic wake migration, tire sweeping, pickup, rain effects, piles, and airborne movement remain authoritative and can update the affected cached records.
- Extreme development multipliers such as 1000x may still increase GPU work substantially, but should degrade more gracefully on the CPU.
- No road-space texture baking is introduced.
