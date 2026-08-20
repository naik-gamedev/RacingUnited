# WATER12A – Four-Band Triangular Water Skin

WATER12A keeps the authoritative WATER09 virtual-pipe hydrology, but expands the visible 3D water skin from the original 0–50 m test region out to 500 m. The visible surface remains an independent staggered triangular lattice, not hydrology-cell quads and not parcel splats.

Presentation spacing bands:

- 0–50 m: 0.10 m
- 50–100 m: 0.50 m
- 100–200 m: 1.0 m
- 200–500 m: 2.0 m

The collector feeds every explicit-water ring from full-resolution 0.5 m hydrology, including dry neighbours, so shoreline clipping remains continuous. Microscopic film stays screen-space. Beyond 500 m, hydrology persists but no explicit water surface geometry is emitted.
