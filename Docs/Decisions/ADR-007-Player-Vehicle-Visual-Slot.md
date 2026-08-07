# ADR-007: Player Vehicle Visual Slot Is Separate From Native Vehicle Physics

## Status
Accepted for Step 29I.

## Decision

Racing United vehicle presentation is attached through Entity mesh components and
must remain independent from the native vehicle solver. The current player-car
slot uses:

`Modules/RacingUnited/Assets/Vehicles/Player/PlayerCar.obj`

as a stable creator-owned drop-in path. `Vehicles/Visuals.lua` applies the mesh
and visual-only offset, rotation, scale, colour, normalization, and double-sided
settings to the existing `Player Chassis` child entity.

The physics body, collision shape, suspension mounts, tire contacts, drivetrain,
and deterministic state remain unchanged when the visual mesh or its transform
changes.

## Why

- Creators can iterate on authored car geometry without recompiling C++.
- Visual alignment mistakes cannot silently alter handling.
- The renderer already detects OBJ modification times and reloads changed meshes.
- A stable slot makes asset handoff simple for non-programmers and future AI
  contributors.
- Future production vehicle definitions can reference their own visual assets
  without duplicating simulation logic.

## Current limitation

Step 29I accepts a whole-car OBJ as one rigid chassis visual. If that OBJ includes
wheels, those authored wheels do not independently steer or spin yet. Separate
wheel-mesh presentation will attach to the already-existing wheel transforms in
a later presentation step. This limitation must not be "fixed" by moving wheel
simulation into Lua.

The current Entity OBJ renderer is also a simple single-colour path. MTL/PBR
materials belong to the future production material/import pipeline.

## Update-package rule

Step 29I ships a placeholder `PlayerCar.obj` once so the slot works immediately.
After the creator replaces it with an authored model, normal incremental engine
update ZIPs should omit this file unless an asset replacement is explicitly
intended. This avoids overwriting creator content.
