# WATER12 — Staggered Triangular 3D Water Skin

## Goal
Make settled road water visibly three-dimensional without exposing the square topology of the 0.5 m hydrology simulation and without depending on unstable particle-splat coverage.

## Architecture
The simulation remains the WATER09 virtual-pipe shallow-water field. Presentation samples that field onto a separate staggered triangular lattice. The near 0–25 m lattice uses 0.24 m spacing; 25–50 m uses 0.42 m. Rows are offset by half a spacing and separated by sqrt(3)/2 of the spacing, yielding an equilateral-style triangle topology rather than quads.

At each presentation vertex Heritage bilinearly samples the source hydrology cell's four ground-support heights and four water-depth samples. Triangles are clipped against a continuous depth iso-surface, so shoreline vertices may lie inside presentation triangles. World-stable threshold noise adds only sub-millimetre contour irregularity. Triangles crossing large support-height discontinuities are rejected rather than bridging curbs/stacked surfaces.

The resulting indexed mesh is real 3D geometry positioned at support height plus physical water depth. The existing water material supplies optical response, while the vertex shader adds small world-stable geometric displacement so deeper water has actual surface motion rather than only normal-map motion.

## Ownership
- <~1 mm: terrain-conforming screen-space wet film.
- ~1 mm and deeper inside 0–50 m: staggered triangular 3D skin.
- Experimental settled parcel surface: disabled.
- Detached future spray/splash: may reuse WaterParcelRenderer later.
- Authoritative mass/flow: WATER09 hydrology.
