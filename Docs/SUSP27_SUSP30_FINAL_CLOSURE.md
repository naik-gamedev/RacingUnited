# SUSP27-SUSP30 final suspension closure

## SUSP27 — Physical element graph

A suspension corner is no longer represented as one spring, one damper and one aggregate damage/compliance state. `SuspensionElementGraphV3` owns up to 64 individually addressable physical elements per corner. Every structural link, ball joint, bushing, spring, damper, stop, actuator, drop link, inerter, hydraulic piston path and mount can have its own state, load, wear, leakage, permanent set and failure.

Multiple springs, dampers, helper/tender springs, air bags, actuators or other parallel/series hardware can coexist on one corner without adding another hard-coded corner layout.

## SUSP28 — Universal geometry and Jacobians

`SuspensionGeometryJacobianV3` derives element length, compression, path velocity, path acceleration and instantaneous `dCompression/dWheel` directly from physical frame transforms and authored attachment points.

The production path is analytic. Providers publish frame derivatives with respect to wheel compression and real frame velocity/acceleration. This avoids the 3x kinematic solve cost that a +/- finite difference would impose at 1 kHz. A probe-only finite-difference fallback remains solely for legacy provider migration and is forbidden from mutating provider warm-start state.

ARB, active ARB, third/heave, hydraulic cross-link and axle inerter paths are read from physical frame/element geometry. The V3 axle coordinator no longer accepts a manually supplied motion ratio or bar-arm-length shortcut.

## SUSP29 — Component damage, compliance and degraded topology

Constraint providers must report reaction loads by physical element ID. Damage and fatigue are therefore driven by the actual link/joint load rather than an aggregate corner load. Compliance/permanent-set feedback is also keyed by element ID and must be consumed before the next kinematic closure.

A broken structural constraint is removed from the constraint set. Because an exact one-DOF hardpoint solver becomes under-constrained after real component breakage, `SuspensionDegradedDynamicsV3` supplies an optional rigid-body/XPBD fallback. Intact vehicles stay on the fast exact kinematic path; damaged vehicles can transition to dynamic unsprung bodies constrained only by the surviving links/joints. Springs, dampers and actuators then apply directly at their physical attachment points rather than through a wheel motion-ratio abstraction.

## SUSP30 — Production and certification contract

`SuspensionProviderRegistryV3` requires all 12 canonical geometry authorities plus the 3 intentional aliases:

- MacPherson / Chapman alias
- double wishbone
- pushrod/rocker wishbone
- rigid/live axle / De Dion alias
- leaf live axle
- motorcycle fork/swingarm
- semi-trailing / pure trailing alias
- twist beam
- multi-link
- swing axle
- sliding pillar
- motorcycle linked/Hossack/Duolever-style front

The authoring schema exposes stable element/frame IDs for Lua, Heritage Studio, telemetry, replay and save/network state.

Portable certification covers:

- multiple simultaneous force elements on one corner
- nonlinear rocker Jacobian
- analytic Jacobian path with exactly one provider commit
- constraint-reaction load ownership
- element-specific compliance and failure
- graph-derived axle interconnections
- broken-topology degraded multibody behavior
- full provider registry completeness
- deterministic V3 state serialization round-trip
- 150-vehicle x 1000-step mixed physical-element workload
- GCC strict warnings-as-errors
- Clang strict warnings-as-errors
- ASan + UBSan

## Live integration gate

The package deliberately does not claim the merged game is complete until the actual RacingUnited checkout proves all of these hooks:

1. `VehicleSystem` calls `stepSuspensionVehicleGraphV3` exactly once for every V3-owned corner/axle.
2. All 12 canonical SUSP01-SUSP22 providers publish V3 frame transforms, analytic wheel derivatives, constraint loads and override-consumption acknowledgement.
3. Broken topology transitions to `stepSuspensionDegradedDynamicsV3` instead of falling back to a rigid/canned wheel pose.
4. Legacy scalar spring/damper/ARB forces are disabled for V3-owned corners.
5. V3 runtime state is included in save/replay/rollback/network snapshots.
6. Lua and Heritage Studio expose stable graph element IDs, frame attachments, component parameters and per-element telemetry/damage.
7. Native engine regressions include the final V3 certification and a real Heritage Engine 150-car mixed-provider benchmark.

Only after the wiring validator reports PASS should the repository milestone be labelled `SUSPENSION_DOMAIN_COMPLETE`.
