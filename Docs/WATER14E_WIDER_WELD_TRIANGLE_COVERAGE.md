# WATER14E - Wider Weld, Shared Triangles, Coverage Budgets

WATER14E keeps the WATER14C/D collider-normal presentation offset unchanged and addresses the remaining disconnected visible-water patches.

## Changes

- Seam neighbour discovery radius is widened from 0.20 m to 0.40 m.
- The radius is discovery-only. Candidates must still be on opposite boundaries, overlap along the seam, use the same presentation layer, and have compatible water-surface elevation.
- Adaptive control-volume rectangles are no longer rendered with a private center vertex and a center fan. Their canonical shared boundary polygon is triangulated directly.
- Adjacent patches therefore reuse the same boundary vertices and the visible water skin is triangle-only.
- Degenerate projected triangles are rejected and reversed triangles have their winding repaired.
- The five existing water-ring record ceilings are raised from 24k/24k/30k/36k/48k to 800k each. The supplied F8 screenshot reports 719,831 total simulation cells, so no individual ring on this current map can be truncated by the renderer budget. The previous caps were silently selecting only the nearest subset of eligible wet cells when a band overflowed, which explains why the F8 overlay could report ~719k wet simulation cells while only ~81k were represented by the water surface.

## Unchanged

- Authoritative adaptive hydrology and rain accumulation.
- Explicit-water depth/visibility thresholds.
- 0.10 m aggressive-angle simulation gate and adaptive simulation sizing.
- WATER14C/D collider-normal offsets: 3 cm near, smoothly reaching 20 cm at 500 m.
- Tire-water physics.
- Reversed-Z and water polygon depth bias.

## Performance note

The larger ring ceilings prioritize coverage/correctness. The direct-boundary triangulation removes one vertex and two triangles per ordinary unsplit source patch relative to the previous center-fan representation, partially offsetting the extra coverage. If a future large track exceeds these budgets, the next step should be coverage-preserving far presentation compaction/chunking rather than dropping wet cells by nearest-distance truncation.
