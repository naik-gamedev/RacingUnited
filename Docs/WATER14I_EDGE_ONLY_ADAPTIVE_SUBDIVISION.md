# WATER14I — Edge-only adaptive subdivision

Date: 2026-08-17

## Goal

Keep minimum 0.10 m detail only where collision geometry actually requires it. A curb/sidewalk step must not turn the entire neighbouring 0.50 m support square into a dense 5x5 block of tiny authoritative cells.

## Behaviour

- Sharp N/E/S/W support-height discontinuities are stored as directional boundary hints.
- A curb/step by itself no longer creates authoritative 0.10 m hydrology cells.
- The solver cell directly beside the edge remains 0.50 m.
- Presentation creates one ~0.10 m-deep triangle strip along the marked edge.
- Behind that strip, the same patch is covered by two coarse triangles instead of extending tiny triangles deep into the road/sidewalk.
- Adaptive solver size grows outward approximately 0.50 -> 1 -> 2 -> 4 -> 8 -> 16 -> 20 m, subject to the existing plane-fit/material/error rules.
- Truly aggressive angular geometry still qualifies for authoritative 0.10 m simulation cells.
- Detected curb/step boundaries are hard topology boundaries: coarse candidates may end at them but may not merge across them.
- The 0.40 m seam search remains candidate discovery only and does not weld across a marked step edge.

## Unchanged

Hydrology volume conservation, rain accumulation, tire-water interaction, explicit-water visibility thresholds, shoreline mask, five presentation cadence rings, and the 3 cm near -> 20 cm at 500 m collider-normal presentation offset are unchanged.
