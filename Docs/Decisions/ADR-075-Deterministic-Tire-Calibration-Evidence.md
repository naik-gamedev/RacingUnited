# ADR-075 — Deterministic Tire Calibration Evidence

**Status:** Accepted; calibration evidence policy retained under TIRE46 freeze (updated 2026-08-31)

## Context

Heritage has accumulated a broad tire stack: MF6.2-style forces and moments, relaxation, rigid-ring
and contact geometry, thermal/pressure state, spatial wear, contamination, water, winter and granular
surface providers. Isolated regressions prove bounded mechanisms, but they do not provide reusable
force curves or a stable comparison artifact when coefficients and equations change.

Subjective driving and screenshots remain necessary for vehicle feel and presentation, but they are
not sufficient calibration evidence. Conversely, a laboratory must not treat every aesthetically flat
curve as a bug: a dataset may honestly contain zero or unidentified coefficients for a mechanism.

## Decision

Add a native, deterministic, reduced-order steady-state tire sweep runner.

1. A sweep owns a baseline `TireContactInput`, one required primary axis and one optional secondary
   axis.
2. Axes use canonical SI units and explicit bounds/sample counts.
3. The runner invokes the same `evaluateAdvancedRoadTire` authority used by vehicle simulation.
4. Sample order is deterministic and all force/moment/stiffness/trail outputs are retained.
5. A standard suite covers pure longitudinal, pure lateral, combined slip, load, pressure, camber and
   turn-slip sensitivity.
6. CSV export preserves SI values and adds labelled degree/PSI convenience columns.
7. Tests require finite deterministic output and structural invariants, but do not require a nonzero
   sensitivity when the selected dataset does not identify it.

## Invariants

- The laboratory never owns a second tire-force implementation.
- UI/Lua presentation may select, display and export sweeps but may not alter their equations.
- Estimated/seeded and fitted/imported tire data remain visibly distinguishable.
- Fitted validity envelopes constrain standard sweeps; damage behaviour is not extrapolated from MF
  equations below their pressure range.
- One sweep is bounded to 262,144 evaluations.
- A material tire-model change must keep the standard evidence suite finite and deterministic.

## Rejected alternatives

- Hand-authored Lua curves: they would drift from native physics.
- Screenshots as the only regression artifact: they do not expose forces, moments or provenance.
- Requiring every curve to change: this would encourage fabricated sensitivity.
- Proprietary tire-dataset cloning: Heritage consumes lawful measured/fitted data with provenance or
  explicitly labelled estimates.
- Running unrestricted sweeps over arbitrary values: bounded ranges protect tools and CI from
  accidental work explosions and meaningless extrapolation.

## Consequences

TIRE18 remains the stable native evidence foundation and CSV contract used under the TIRE46 tire
freeze. The production tire solver now uses bounded `Distributed3x3` contact universally; the
150-car / 600-tire tire-only benchmark exercises the full distributed workload. Historical commercial
tire fidelity is improved through the strongest lawful evidence available: measured/fitted data when
it exists, otherwise explicitly labelled engineering estimates reconstructed from dimensions,
construction era, period specifications/reviews, comparable tires, vehicle behavior and physical
bounds. Measured data is not a completion gate and must never be fabricated. The existence of a plot
or synthetic envelope still does not prove accuracy. Full 150-car scene performance remains a
whole-engine profiling task.
