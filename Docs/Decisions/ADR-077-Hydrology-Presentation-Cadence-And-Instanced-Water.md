# ADR-077 — Hydrology Presentation Cadence and Instanced Water

## Status
Accepted for JOB02.

## Context
JOB01 made the 30 Hz hydrology solver genuinely multicore. Live F8 captures then
showed that a large wet field could still spend substantial CPU time in render
submission. The renderer was collecting/aggregating visible hydrology cells,
packing tens of thousands of records and updating one dynamic VBO every rendered
frame. The water draw also used a geometry shader to expand one point record into
a quad, which is an avoidable GPU stage and can be disproportionately expensive
on low-end/integrated GPUs.

## Decision
Authoritative hydrology remains independent from presentation. Water presentation
is cached at hydrology cadence and stored in a stable FP64-origin/FP32-local frame.
The camera supplies only the current presentation-origin offset each render frame.
GPU storage is reused between updates and orphaned when refreshed. Each water
record is drawn as an instanced four-vertex triangle strip generated with
`gl_VertexID`; no water geometry shader is used.

Near presentation retains the 0.5 m hydrology resolution. Beyond 45 m, 4x4 cells
may be represented by one visual record. This is presentation LOD only and must
never alter water mass, tire sampling, drainage or runoff.

## Consequences
- Render rate no longer dictates hydrology collection/upload rate.
- Camera motion between cache refreshes does not repack the water field.
- World-stable ripple animation and environment reflection remain render-rate.
- F8 must expose water cache/CPU timings and hydrology-step timing so future work
  can distinguish simulation, CPU presentation and GPU costs.
- A future continuous/interpolated water surface may replace the visual quads
  without changing hydrology authority or this cadence separation.
