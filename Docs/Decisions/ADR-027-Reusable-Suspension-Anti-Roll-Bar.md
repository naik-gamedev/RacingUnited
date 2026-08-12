# ADR-027: Reusable Suspension Anti-Roll Bar

**Status:** Accepted  
**Date:** 2026-08-09

## Decision

Heritage Engine models an anti-roll bar as an independent reusable suspension
coupling mechanism. It does not belong inside MacPherson, trailing-arm,
double-wishbone, live-axle or any other kinematics provider.

A bar explicitly couples two contact units and owns:

- torsional stiffness and damping;
- left/right lever-arm lengths;
- left/right link motion ratios;
- an optional wheel-force limit; and
- enabled state.

The mechanism converts each side's suspension compression and compression
velocity into bar-arm angle/rate, evaluates elastic plus damping torque from the
left/right twist difference, then returns equal-and-opposite wheel-side forces.
This lets the same mechanism be reused by unrelated suspension layouts.

VehicleDefinitionV2 carries anti-roll bars as top-level components with stable
contact-unit references. The native compiler resolves those references and the
loader creates the corresponding runtime bars. Lua also exposes live set/get and
telemetry APIs for creator tooling and handwritten vehicle definitions.

## Determinism and high-rate ordering

The vehicle loop evaluates all bars from one synchronized pre-wheel snapshot of
wheel compression/velocity before solving the individual wheels. This prevents
left/right results from depending on wheel iteration order. At the current
1000 Hz high-rate vehicle loop, this means the coupling state is effectively one
high-rate substep behind the wheel currently being solved (about 1 ms).

A future two-pass/current-state wheel solve may remove that lag if measured
fidelity or profiling justifies the added complexity. The ordering must remain
explicit and deterministic.

## Racing United starting data

The current Peugeot-oriented prototype uses deliberately low-confidence front
and rear anti-roll-bar estimates. Their provenance is `estimated` and confidence
is 0.20. They are useful simulation starting points, not claimed Peugeot factory
rates. Better measured, documented or asset-authored data may replace them
without changing the mechanism.

## Limits

SUS04 does not yet infer physical bar diameter/material/shape from geometry and
does not model rubber-bushing or drop-link compliance as separate elastic
components. Those can be added later without changing the independent-coupling
contract.
