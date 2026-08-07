# ADR-017: Authoritative Suspension Upright Pose

Status: Accepted in Step 29P.

## Context

Articulated wheel meshes previously reconstructed yaw in Lua while the tire
solver reconstructed a separate steering direction. Neither path could express
an inclined steering axis, camber gain or bump steer. Adding MacPherson,
wishbone, trailing-arm and motorcycle providers on top of those duplicated
approximations would make visual and physical wheel orientation disagree.

## Decision

Add a native `SuspensionGeometry` provider boundary with one authoritative
upright output. The first provider evaluates signed quadratic camber/toe curves
against suspension travel and rotates around an authored three-dimensional
steering axis. The resulting orthonormal basis is consumed by tire direction,
telemetry and wheel presentation.

Definitions and live APIs own independent per-contact geometry. Category names
do not select kinematics. Existing definitions default to a vertical steering
axis and zero curves, preserving their behavior.

## Consequences

- Steering, camber, toe, telemetry and wheel visuals share one native pose.
- Measured alignment curves can be fitted before full linkage hardpoints exist.
- Asymmetric and unusual vehicles do not require duplicated category solvers.
- The current curve provider does not infer hardpoint loads, scrub, track
  change, jacking, compliance or dynamic motion ratio.
- Camber thrust remains a tire-provider extension; upright orientation alone
  must not be advertised as a complete cambered-tire model.
- Future linkage providers must return the same upright contract rather than
  leaking their internal hardpoints into tire or presentation code.
