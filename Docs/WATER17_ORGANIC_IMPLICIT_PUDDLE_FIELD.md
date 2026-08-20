# WATER17 — Organic Implicit Puddle Field

WATER17 removes the final route by which hydrology grids could become visible puddle silhouettes.

## Authority versus presentation

Authoritative water mass remains in `SurfaceHydrology` adaptive control volumes.  Their 0.10–20 m topology, cadence and virtual-pipe flow are simulation concerns only.

The graphics path is parameterized from the immutable 0.50 m collision-support raster.  It never submits adaptive solver leaves as presentation primitives.

## Continuous organic field

Each support sample contributes a compact, overlapping Wendland C2 radial-basis support to the low-cadence wetness atlas.  The support is domain-warped in stable world space.  Its boundary combines harmonic radial deformation with `Water_ShorelineBreakup_A8` sampled at rotated, incommensurate scales.

Adjacent support kernels overlap beyond the support-cell diagonal, so internal support-cell boundaries are completely covered.  Only the outer union boundary can become visible, and that boundary is an implicit curved/noisy contour rather than a square edge.

The ordinary material shader performs an invalid-safe four-corner bilateral reconstruction instead of hardware filtering across the atlas invalid sentinel.  It then applies a second world-space domain warp and multi-scale shoreline micro-relief.  The final shoreline is the zero-level set of a continuous scalar field.

## Basin continuity

Static drainage catchments remain separated by real support discontinuities such as curbs.  Continuous neighbouring catchments are linked by a lowest-saddle spill graph.  At presentation refresh, a bounded six-iteration graph-Laplacian relaxation removes millimetre-scale artificial head seams once water reaches the connecting saddle.  The relaxation is limited to ±12 mm from the basin's physical presentation head and never changes authoritative water volume.

## Curbs and stacked surfaces

Organic kernels may overlap laterally, but vertical-layer matching remains strict.  A road kernel cannot become a puddle on a raised sidewalk because the exact rendered fragment height and the two-layer support-height test reject the mismatch.  Bridges/tunnels remain separate layers.

## Performance

The compact WATER15G/WATER16 clipmaps remain:

- 512² / 64 m at 10 Hz
- 256² / 256 m at 3 Hz
- 128² / 2000 m at 0.5 Hz
- strict 1000 m radial presentation cap
- at most one atlas refresh per frame
- zero atlas refresh work for ordinary uniform rain film without local puddle excess

There is still no visible water mesh, adaptive presentation tessellation, duplicate scene pass, or 4096² dynamic atlas.
