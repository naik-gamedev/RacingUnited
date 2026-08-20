# WATER18 — Dynamic Sponge-Brush Water Canvas

WATER18 replaces the last presentation path that could reveal the 0.50 m support lattice.
The authoritative hydrology solver and drainage basins remain unchanged; presentation is now
painted into the existing world-space clipmaps like a Substance Painter/GIMP brush canvas.

## Core rule

A support/adaptive cell is **never rasterized as its own footprint**. It may only provide
physical basin head/support data from which one or more deterministic brush dabs are emitted.
The visible footprint is determined entirely by the brush alpha.

## Brush generation

* L0/L1/L2 emit 4/3/1 dabs per relevant support respectively.
* Dab centres are deterministically jittered in world space and remain stable across camera movement.
* The first dab is biased toward the physically deepest support corner rather than the cell centre.
* Radius, angle and aspect are independently randomized from a stable hash.
* `Water_ShorelineBreakup_A8.png` is sampled several times with rotated/incommensurate UVs and is
  the actual porous sponge-brush alpha.
* At first pooling only high-valued sponge islands appear. As physical depth grows, the threshold
  falls and the same dabs fill. Overlapping dabs naturally connect into larger puddles.

## Surface safety

The brush is only a 2D presentation gate. Actual local water remains
`max(basinHead - exactRenderedSurfaceHeight, 0)`. Two vertical atlas layers plus support-height
matching prevent road paint from becoming water on a raised sidewalk/bridge.

## Performance

The compact 512/256/128 clipmaps and one-refresh-per-frame budget are retained. There is no
4096 atlas, no settled-water geometry, no tessellation, no control-volume mesh presentation and
no full-screen compute blur.
