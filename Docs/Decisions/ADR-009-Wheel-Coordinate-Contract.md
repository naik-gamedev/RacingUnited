# ADR-009 — Wheel Coordinate and Presentation Contract

## Status
Accepted for Step 29J.1.

## Context
The first articulated-wheel test proved that four independent meshes could read
native suspension, Ackermann steering and wheel spin, but temporary 2.10 m track
and 2.60 m wheelbase values made the wheels float outside the imported Peugeot.
All four instances also used the same mesh-facing convention. Reconstructing a
wheel center in Lua would become increasingly fragile once camber, toe and
non-linear suspension geometry are added.

## Decision
- Native `WheelState.worldCenter` is authoritative for rendered wheel position.
- Lua must not derive wheel translation from suspension length when an
  authoritative native center is available.
- Heritage Engine native vehicle simulation currently uses +X right, +Y up, +Z forward. Racing United creator-authored content uses Blender X right, Y forward, Z up per ADR-010; importers convert at the boundary.
- Temporary OBJ wheel assets are authored with their origin exactly at the wheel
  center and local X along the axle. The visible outer face points toward +X.
- A vehicle definition may independently specify wheel mesh asset, face yaw and
  visual spin sign per corner. Shared front/rear/all-corner assets remain valid.
- The 29J.1 prototype uses 2003 Peugeot 206 RC reference geometry: 2.442 m
  wheelbase, 1.437 m front track, 1.428 m rear track, 205/40 ZR17 tires and a
  derived unloaded tire radius of 0.2979 m. These values improve the prototype
  geometry but do not turn its generic drivetrain/tire curves into final Peugeot
  simulation data.
- Future camber, toe, steering-axis and suspension orientation must come from a
  native per-wheel pose/orientation contract, not Lua-side guesses.

## Consequences
Visual wheels remain attached to the same authoritative simulation that creates
tire contacts. Staggered wheels, asymmetric vehicles, motorcycles, trucks and
left/right-specific wheel meshes can be represented without changing the core
solver. The later glTF vehicle pipeline should preserve the native wheel-state contract while honoring ADR-010 Blender authoring coordinates
and replace temporary OBJ-facing offsets with authored node transforms.
