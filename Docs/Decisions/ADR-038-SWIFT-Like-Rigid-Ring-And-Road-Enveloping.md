# ADR-038 — SWIFT-like rigid-ring structural dynamics and road enveloping

**Status:** Accepted — TIRE05
**Date:** 2026-08-09

## Context

MF6.2 supplies the steady-state/semi-empirical tire force and moment law, but short road
wavelengths and belt/carcass dynamics require structural state and finite-footprint road
processing. Public MF-Swift literature describes a rigid belt/ring connected to the rim by
compliant/damped modes and an enveloping stage for obstacles/rough roads. Heritage also
needs this fidelity to remain scalable toward large vehicle grids.

## Decision

Heritage implements the public architecture independently as composable mechanisms:

- `TireRoadEnveloping` converts finite-footprint road samples into an effective local road
  excitation. TIRE05 begins with a front/rear tandem-cam-inspired longitudinal row.
- `TireRigidRing` owns structural belt/ring state. TIRE05 activates longitudinal, lateral
  and radial translation; yaw/wind-up parameters are preserved for later rotational DOFs.
- MF6.2 remains the force/moment provider. Structural velocities modify contact slip
  kinematics; structural radial displacement/velocity couples road excitation toward the
  wheel/suspension path.
- The local center-contact plane is removed before enveloping so a smooth road grade is not
  interpreted as tire roughness.
- Structural state runs at the high-rate vehicle step (normally 1000 Hz). Auxiliary road
  envelope queries default to 250 Hz and are cached between structural steps.
- Public MF-Swift-style `.tir` vocabulary is imported where documented. Synthetic prototype
  parameters are clearly labelled and never represented as measured historical tire data.

## Non-goals

TIRE05 does not claim numerical parity with proprietary Simcenter Tire/MF-Swift releases.
It does not yet activate rotational yaw/wind-up ring DOFs, full multi-row 3D enveloping,
thermal/pressure/wear state, or proprietary 2512 wet-road behavior.

## Consequences

Road obstacles can excite an actual tire structural state before reaching suspension and
MF slip calculations. The architecture can grow toward higher fidelity without making the
core tire law monolithic, while road-query cost is independently rate-limited for future
large-grid performance work.


## TIRE06 extension

TIRE06 keeps this separation but expands the enveloping provider to a bounded adaptive 2D
footprint and activates the ring's yaw/wind-up rotational modes. The current baseline performs
one MF6.2 evaluation per tire, using footprint-aggregated surface multipliers; it does not
perform one Magic Formula evaluation per road sample. Smooth/homogeneous contact uses a slower
coarse sampling cadence while detected discontinuities trigger full-grid refinement and the
faster envelope cadence. This preserves an explicit fidelity/performance boundary for large
vehicle counts. Detailed policy is recorded in ADR-039.
