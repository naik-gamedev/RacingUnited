# WATER15J — Circular Puddle Splats

WATER15J removes the final visible rectangular ownership cue from the sparse puddle atlas. The adaptive hydrology solver remains authoritative for water mass, flow, drainage, evaporation and tire clearing, but each excess-puddle source is projected into presentation as a circular world-space splat rather than as the source control-volume rectangle.

## Presentation rules

- Ordinary rain film remains the spatially smooth `SurfaceWeather` material state.
- Only water deeper than the smooth film plus 1.5 mm enters the sparse puddle atlas.
- Every excess-puddle source is rasterized through a bounding quad but the atlas fragment shader discards fragments outside `x*x + y*y <= 1`, producing an exact circle in atlas space.
- The circle radius is the source half-size plus a small capped world-space merge margin. Adjacent/nearby splats therefore overlap and form one connected union without seam welding or generated water geometry.
- The merge margin grows modestly with source size but is capped so very coarse solver cells cannot inflate arbitrarily.
- The existing `Water_ShorelineBreakup_A8` texture controls the first visible phase of a new puddle. At shallow excess depth only shoreline-mask islands are revealed; with increasing depth the reveal threshold opens until the circle/blob is completely filled and uses the normal standing-water optical path.
- Presentation depth is always clamped to physical depth. The shoreline mask can hide/erode water visually but cannot create hydrology mass.
- Exact rendered-fragment height and the two vertical surface layers remain responsible for curb/bridge separation.

## Performance

The compact WATER15I clipmaps remain 512² / 256² / 128² at 10 / 3 / 0.5 Hz, with at most one clipmap refresh per frame. Circularization happens in the existing GPU raster pass and does not add a visible mesh, a fullscreen pass, or CPU neighbour searches.
