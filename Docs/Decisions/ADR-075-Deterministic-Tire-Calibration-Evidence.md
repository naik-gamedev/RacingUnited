# ADR-075 — Deterministic Tire Calibration Evidence

**Status:** Accepted for TIRE18A-E baseline (2026-08-14)

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

TIRE18 now has a stable native evidence foundation and CSV contract, an installed-tire A/B Lab,
stateful transient/thermal/wear/failure scenarios, provenance-labelled acceptance-envelope checks
and an experimental bounded 3x3 contact tier. Commercial fidelity still depends on lawful measured
datasets; the existence of a plot or synthetic envelope does not prove accuracy. Large-grid work is
algorithmically bounded but still requires an actual 150-car scene profile.
