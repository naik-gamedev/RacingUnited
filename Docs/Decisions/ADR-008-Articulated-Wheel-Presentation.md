# ADR-008 — Articulated Wheel Presentation Follows Native Vehicle State

## Status
Accepted for Step 29J.

## Context
Whole-car OBJ imports are useful for early visual progress, but wheels embedded
in one rigid body cannot visibly follow suspension travel, Ackermann steering or
wheel angular motion. Duplicating wheel dynamics in Lua would create a second
source of truth and eventually diverge from the native solver.

## Decision
Wheel presentation is separated from vehicle simulation.

- Native `VehicleSystem` remains authoritative for suspension length, steering
  angle and wheel rotation.
- Lua visual code may read that telemetry and apply it to presentation entities.
- Visual fitment offsets/scales are explicitly non-physical and cannot alter
  tire contacts, wheel mounts or force calculations.
- Vehicle definitions may provide an independent visual asset path per wheel.
- Temporary OBJ wheel slots are allowed for iteration, but the later glTF asset
  pipeline should preserve named wheel nodes/materials in a production package.
- Debug proxy wheels and articulated mesh wheels must not be shown together by
  default.

## Consequences
A future suspension implementation may change how a wheel center moves. The
presentation layer must consume the authoritative pose/state contract rather
than reconstructing tire physics. Step 29J.1 removes the original `mount - suspensionLength` translation
approximation and consumes native `WheelState.worldCenter` directly. Future
non-linear suspension work should extend the native pose contract (for example
orientation/camber/toe) rather than recreating suspension kinematics in Lua.
