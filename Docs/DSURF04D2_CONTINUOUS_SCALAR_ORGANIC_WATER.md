# DSURF04D2 — Continuous Scalar Organic Water

## Status

Current Dynamic Surface water-presentation candidate. DSURF04C physics remains unchanged: one 100 m × 100 m surface-sheet tile, one 64 × 64 Hydro/Track authority raster (1.5625 m/cell), fixed 2 Hz active polling.

DSURF04D2 supersedes the **terrain-cut presentation method** introduced by DSURF04D. Live testing showed that subtracting exact rendered triangle height from a coarse support-derived water head can stamp slope bands and triangle topology into the image.

## Decision

Hydro.R is treated as a **continuous scalar depth field**. The renderer uses hardware linear filtering of the authoritative 64 × 64 mip0 values and never converts support-height error into visible water depth.

Support height now has one presentation responsibility only: **select the correct vertical surface sheet** (road, bridge, tunnel, sidewalk, stacked deck). After that selection, visible depth comes from Hydro.R.

To prevent an axis-aligned bilinear zero contour from exposing the 1.5625 m lattice, Hydro UV is displaced by a bounded deterministic world-space domain warp derived from `Water_ShorelineBreakup_A8`. The warp is less than half a simulation texel and fades to zero at 100 m tile borders so tile-neighbour blending remains authoritative there.

The final shallow contour receives millimetre-scale world-stable optical relief and derivative anti-aliasing. Relief fades out rapidly with standing-water depth and cannot create isolated water in unrelated dry regions.

## Explicitly forbidden live presentation path

The shader must not compute visible depth from:

`coarseSupportY + HydroDepth - exactRenderedTriangleY`

That method is retained only in historical DSURF04D documentation because live testing disproved it.

## Performance contract

- 100 m × 100 m tiles remain unchanged.
- 64 × 64 authority/texture remains unchanged.
- fixed 2 Hz authority remains unchanged.
- mip0-only upload remains unchanged.
- no second high-resolution simulation texture is added.
- no generated water geometry, clipmap, canvas, splat or marching-square mesh is reintroduced.

## Acceptance gates

1. No triangle-shaped or slope-band water artifacts from receiver mesh topology.
2. No 1.5625 m squares/stair steps in the visible shoreline.
3. No straight 100 m ownership seams.
4. Bridges/roads/tunnels remain sheet-isolated.
5. Water stays world-anchored while the camera moves.
6. Hydro/thermal regressions and Windows build safety gates remain green.

If this scalar reconstruction still exposes the 64 × 64 lattice in live testing, the next escalation is a dedicated high-resolution **presentation-only** scalar reconstruction texture; authority resolution and 2 Hz physics do not change.
