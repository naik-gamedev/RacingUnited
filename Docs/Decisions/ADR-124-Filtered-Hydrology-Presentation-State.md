# ADR-124 — Filter adaptive hydrology once at presentation cadence

## Decision

Keep adaptive hydrology as the sole water authority, but never expose raw control-volume ownership or hydraulic-head derivatives directly to the final material shader. Raster raw state into compact clipmaps, then run a support-aware depth prefilter at clipmap refresh cadence. Preserve the center support elevation and reject neighbours across meaningful elevation discontinuities such as curbs.

The final material shader derives local depth from filtered hydraulic head minus the exact authored fragment elevation. Standing-water normals are presentation normals and must not be derived from discontinuous solver-cell head derivatives.

## Consequences

- Adaptive solver cells are not visible as rectangular reflection bands.
- Curb/sidewalk separation is preserved by support-height filtering and exact receiver depth.
- Continuity work is amortized at 15/5/1 Hz rather than repeated for every rendered fragment.
- No generated visible water geometry is reintroduced.
