# DSURF04D — Organic Water Reconstruction

## Status

Historical DSURF04D candidate, **superseded by DSURF04D2** after live testing exposed terrain/triangle imprinting. It **does not change** the DSURF04C physical authority contract: one 100 m × 100 m surface-sheet tile, one 64 × 64 Hydro/Track state raster (1.5625 m/cell), and fixed 2 Hz active-tile polling.

The historical DSURF04D attempt existed for one reason: **the coarse physical lattice must never become the visible puddle shape.**

## Presentation contract

The renderer no longer interprets Hydro.R as “draw this much water directly on this fragment.” Instead it treats the coarse Hydro/support samples as a low-frequency estimate of the local free-water elevation:

`waterSurfaceY = filteredSupportY + filteredHydroDepth`

For every wettable fragment of the actual authored road/terrain mesh, the shader then reconstructs:

`terrainCutDepth = max(waterSurfaceY - exactFragmentSurfaceY, 0)`

The visible shoreline is therefore the continuous intersection of a water-height field with the **actual rendered receiver height**, not the perimeter of a 1.5625 m simulation cell, collision triangle, virtual page or 100 m tile.

The reconstructed depth is clamped to a small envelope around the authoritative Hydro depth so coarse support interpolation cannot fabricate large bodies of water on steep geometry. A dry Hydro sample remains dry.

At the outer half-texel of a 100 m tile, the shader samples the neighbouring world tile through the same sheet/support-height resolver and bilinearly cross-fades water head/depth/moisture. This removes texture-array clamp seams at 100 m ownership boundaries without introducing guard texels or changing the requested 64 × 64 texture size. At tile corners the diagonal neighbour participates as well.

## Organic sub-cell edge

`Water_ShorelineBreakup_A8.png` remains presentation-only. It now contributes several rotated, incommensurate world-space frequencies including sub-metre components. The result perturbs only millimetres of unresolved road micro-relief near the shoreline.

The mask:

- cannot create authoritative water mass;
- cannot move with the camera;
- cannot override surface-sheet separation;
- fades out as water becomes deeper;
- is analytically anti-aliased with fragment derivatives.

This intentionally provides irregular fingers, islands and local breakup without allocating a second high-resolution fluid simulation.

## No presentation mip hierarchy

The live water shader samples only the authoritative 64 × 64 mip0 tile. DSURF04D removes the CPU 64→32→16→8→4→2→1 Hydro/support/Track downsample-and-upload loop from `EntityMeshSurfaceWetness.cpp`.

This serves both visual and performance goals:

- distant mips can no longer expose averaged square/cell regions as shoreline ownership;
- each changed Hydro tile requires one Hydro upload plus one support upload at mip0;
- each changed Track tile requires one Track upload at mip0;
- new GPU slots no longer trigger whole-array mip generation for the live Dynamic Surface path;
- ordinary linear filtering of mip0 supplies the low-frequency authority interpolation, while the exact receiver mesh and shoreline micro-relief define the visible contour.

The backing texture arrays still reserve the historical mip storage accounted by the DSURF02 pool budget. Those levels are not sampled by DSURF04D and may be physically removed in a later storage-only cleanup after live validation.

## What remains authoritative

Nothing in this milestone changes Hydro mass, flow, drainage, moisture, tire clearing/spray, Track temperature, surface-sheet identity, persistence, residency or cadence. Tire physics still reads the 64 × 64 / 2 Hz Dynamic Surface authority. Only presentation reconstruction changes.

## Acceptance gates

1. No visible water boundary should align with 64 × 64 cell edges merely because the simulation cell ends there.
2. No visible boundary should align with a 100 m tile edge merely because ownership changes there; if a seam is observed, it is a DSURF04D follow-up bug.
3. Raised road detail, camber and depressions should cut the puddle at fragment resolution.
4. Bridges/roads/tunnels remain vertically isolated by DSURF01 sheet/support selection.
5. Shoreline breakup remains world-anchored while driving or moving the camera.
6. F8 should report direct mip0 uploads and no live CPU mip ladder.
7. The DSURF01→04 native regression chain remains green.

## Deliberately deferred

Render-time interpolation between consecutive 2 Hz authority snapshots is a separate temporal problem. DSURF04D first removes the spatial grid from the image. If 2 Hz state stepping is perceptible after the spatial result is accepted, a previous/current GPU state blend can be added without changing Hydro resolution or cadence.
