# PERF15 — Steep Water Sail Rejection

## Problem

PERF14 correctly subdivided adaptive water patches when a large fitted plane could not follow the collision contour. However, rejected adaptive blocks fell back to authoritative 0.5 m hydrology cells. Hydrology intentionally accepts upward-facing collision triangles down to `normal.y = 0.15` so runoff can remain physically connected on very steep faces. Rendering those same cells as explicit X/Z water quads can span several metres vertically and produce tall transparent "water sails" beside curbs, walls, gullies and steep terrain.

This is a topology/presentation problem, not an adaptive-LOD problem. Subdividing a near-vertical X/Z cell further without a different surface parameterization cannot make it a sensible puddle sheet.

## Decision

Separate hydrology eligibility from explicit water-geometry eligibility.

- Authoritative hydrology remains unchanged and may continue to simulate rainfall, runoff, drainage and persistence on steep collision faces.
- Explicit transparent water geometry is emitted only when the normalized surface normal is at least `cos(55 deg)` in +Y.
- Steeper faces are expected to read through the wet-surface/material path rather than a free-standing puddle card.
- Fine 0.5 m fallback cells normalize their collision normal before solving corner heights. The old minimum-normal clamp is no longer allowed to turn a near-vertical normal into a huge extrapolated Y offset.
- Adaptive parent nodes use the same explicit-water slope eligibility, so coarse and fine paths cannot disagree at a fallback boundary.
- The renderer has a final finite/bounds guard on contour corner offsets. A malformed visual record is dropped rather than stretched into the sky; the authoritative hydrology state is untouched.

## Preserved behavior

- 0.5/1/2/4/8/16 m adaptive water presentation remains active on suitable surfaces.
- PERF14 contour-error subdivision remains active around crowns, gutters, curbs and curved terrain.
- PERF13 world-space seamless material/ripple/reflection behavior is unchanged.
- Hydrology cadence remains 30/20/6/2/0.5 Hz at 0-25/25-50/50-100/100-200/>200 m.
- Water remains unrendered beyond 200 m.
- WEATHER07C6 rain calibration is unchanged.
