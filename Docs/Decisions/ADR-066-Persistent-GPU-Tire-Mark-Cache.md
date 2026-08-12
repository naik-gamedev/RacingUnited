# ADR-066: Persistent GPU tire-mark cache

## Status
Accepted for TIRE16K.

## Context
TIRE16J proved the six-control pressure-resolved tire-mark appearance and the
20-minute history policy, but the renderer still scanned, distance-classified,
sorted, CPU-tessellated and re-uploaded every visible historical segment every
frame. A stress test reached roughly 53 ms CPU active time while the GPU frame
was roughly 18 ms, with render submission dominating the CPU cost.

The visual result must not regress. In particular, storage chunks must never
become visible as seams, chess-board placement, opacity blocks or LOD cells.
The user explicitly rejected baking old marks into a road-space coverage
texture, so this optimization remains geometry based.

## Decision
- Authoritative tire-mark history remains FP64 in `SurfacePresentation`.
- Tire-mark presentation is grouped into invisible 100 m x 100 m spatial
  chunks. A 100 m vertical layer is also used so stacked roads/bridges keep
  small local coordinates.
- A chunk owns an FP64 centre origin. Each newly-created tire-mark segment is
  converted once to chunk-local FP32 and appended to a persistent GPU page.
- GPU pages contain compact logical segment records, not pre-expanded ribbon
  triangles. Pages hold 8192 records and are append-only while active.
- Frame uploads are batched with `glBufferSubData`; frozen pages are never
  rewritten.
- A geometry shader expands each logical record into the six-control nearby
  ribbon or the uniform far strip. The GPU also evaluates the 20-minute age
  fade, 200 m LOD morph, 500 m visibility fade and per-segment range rejection.
- CPU frame work is reduced to incremental new-serial ingestion, whole-page age
  retirement, conservative 100 m chunk range culling and one draw per visible
  GPU page.
- Chunk boundaries never affect mark endpoints, tire width, pressure profile,
  intensity, age, LOD, or fade. Whole segments keep their original continuous
  FP64 endpoints; chunk-local FP32 is only a storage coordinate transform.
- The existing master LOD transition policy remains authoritative for smooth
  representation/visibility transitions.
- No road-space coverage texture baking is introduced.

## Consequences
- Old tire marks stop consuming recurring CPU tessellation, sorting and giant
  dynamic-VBO uploads.
- GPU memory grows with retained logical history, bounded by the existing one
  million segment / 20-minute presentation policy.
- Chunk-local FP32 retains far more precision than visible tire-mark geometry
  requires because local coordinates stay near a 100 m origin.
- Geometry-shader expansion increases GPU work relative to pre-baked triangles,
  but the observed bottleneck is CPU render submission and the logical record
  representation greatly reduces upload bandwidth and persistent geometry
  memory.
- If future profiling shows geometry-shader throughput becoming dominant, the
  same logical record/page format can migrate to mesh shaders or another GPU
  expansion path without changing authoritative tire-mark history.
