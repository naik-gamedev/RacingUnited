# WATER14C — 3 cm Collider-Normal Water Presentation Offset

WATER14C preserves WATER14B's adaptive shared-boundary water mesh and changes only the **presentation position of explicit water that already qualifies to render**.

## Requested behavior

Every visible explicit-water vertex is translated **0.030 m away from its collision support in the direction of the collider-derived surface normal**.

This is not a world-Y lift. On a sloped collider the 3 cm displacement contains X/Z components exactly as the surface normal does.

## Stitching behavior

- Each adaptive cell contributes its normalized collider-derived normal to shared seam vertices.
- A welded seam vertex uses the normalized average of all compatible contributing normals.
- The shared base position is built exactly as WATER14B did, then the common vertex is translated by 3 cm along that averaged normal.
- Interior/center vertices use their owning adaptive cell's normalized collider-derived normal.
- The same normal is uploaded with the water vertex so rendering no longer replaces the collider-derived normal with hard-coded `(0,1,0)`.

This preserves one crack-resistant shared seam while keeping the offset normal-aware.

## Explicitly unchanged

- No water appearance/depth threshold is changed.
- No shallow-film vs explicit-water classification is changed.
- Hydrology depth, volume, rain input, flow and virtual-pipe simulation are unchanged.
- Tire-water physics remains on the authoritative collider-derived hydrology state.
- WATER14A adaptive 0.10–20 m simulation sizing is unchanged.
- WATER14B 0.20 m topology-aware neighbour discovery and seam welding are unchanged.
- Reversed-Z and the existing water polygon depth bias remain enabled.

The 3 cm displacement exists only in the visible explicit-water geometry.
