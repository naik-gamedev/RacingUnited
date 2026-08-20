# ADR-099 — Adaptive Contour Water Tessellation

## Status
Accepted — PERF14.

## Context
Adaptive water merging reduced presentation cost, but representing every merged node as one fitted plane allowed large transparent patches to bridge curved collision geometry. A hardware tessellation shader alone would not solve this because the GPU did not own a collision-surface height field to sample.

## Decision
Keep hydrology authoritative at 0.5 m and make the existing presentation quadtree itself the adaptive tessellator.

A candidate patch is accepted only when a conservative error bound built from authoritative source-footprint corners and inherited child residuals stays below a millimetre-scale presentation tolerance. Rejected nodes recursively expose smaller children/fallback cells. Steep surfaces cap the maximum presentation patch size as an additional visual-stability guard.

Each accepted/fallback visual record carries four collision-contour corner elevations. The GPU renders those corner supports directly while retaining two triangles for a planar merged patch. No geometry shader or tessellation shader is introduced.

## Consequences

- Flat, uniform parking lots retain 8/16 m merged water patches.
- Curved roads, gutters, hillsides and terrain breaks subdivide automatically.
- The water geometry follows collision contour rather than extrapolating one average plane.
- GPU vertex/triangle cost increases only where surface shape requires it.
- Authoritative hydrology, tire-water interaction, persistence cadence and rain mass are unchanged.
- Hardware tessellation remains an option only if a future GPU-resident collision/height representation makes it materially useful.
