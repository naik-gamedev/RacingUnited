# Native Suspension Geometry

## Purpose

Step 29P separates wheel/upright kinematics from spring and damper forces.
`SuspensionGeometry` is the native provider boundary that owns the orientation
used by tire contact, telemetry and articulated wheel presentation. Lua may
author and display geometry, but it does not reconstruct steering, camber or
toe from visual transforms.

## Current curve provider

The current `linear_raycast_v1` provider keeps the existing straight
suspension-axis wheel path and evaluates signed alignment curves from wheel
compression `x` in metres:

`angle = static + gain * x + 0.5 * progression * x * abs(x)`

Camber and toe have independent static, gain and progression values. Each
contact also carries a normalized three-dimensional steering axis in chassis
local coordinates. The provider composes toe, camber and Ackermann steering,
then returns an orthonormal right-handed upright basis plus the local Euler pose
used by the entity presentation API.

Positive and negative curve signs are authoring conventions, not automatic
left/right mirroring. A definition must give each contact the values measured
for that corner. This permits asymmetric cars, oval setups and damaged geometry
later without category-specific solver code.

## Authoritative consumers

- `VehicleSystem` evaluates geometry in every high-rate wheel substep after the
  current suspension compression is known.
- The tire contact basis consumes native forward orientation, so steering-axis
  inclination and toe affect the force direction.
- `Vehicle.GetWheelUprightPose` exposes the native pose and basis.
- Articulated wheel meshes compose authored mesh facing and spin after the
  native upright pose.
- Dynamics Lab records camber and toe in captures, plots, extrema and CSV.
- `Vehicle.Set/GetWheelSuspensionGeometry` provide validated per-wheel live
  tuning and exact readback.

## Deliberate limits

Curve data can fit measured camber and bump-steer traces, but it is not a
hardpoint linkage solver. It does not yet calculate lateral/longitudinal wheel
center motion, track change, caster trail, scrub radius, kingpin offset,
dynamic motion ratio, jacking, bushing compliance or linkage loads. Camber is
authoritative pose/telemetry but the current advanced road tire does not yet
produce camber thrust.

MacPherson and double-wishbone providers come after the Workshop can author and
inspect hardpoints. Those providers must preserve this output contract so tire,
telemetry and presentation code do not depend on a particular linkage family.
