# ADR-035: Public MF6.2-Compatible Motorcycle Tire Branch

## Status
Accepted at TIRE01 (2026-08-09).

## Context
Heritage needs a serious empirical handling tire model for cars and motorcycles,
while Simcenter Tire/MF-Tyre/MF-Swift is a commercial product whose current 2512
wet-road implementation is not publicly specified. The public TNO literature and
MF-Tyre/MF-Swift 6.2 manual document the MF6.x force/moment family, large-camber
motorcycle operation, relaxation behavior and motorcycle contour parameter
semantics.

## Decision
Heritage implements a clean-room public MF6.x/MF6.2-compatible branch in native
C++. Public coefficient names are retained for future fitted-data mapping.
Motorcycle contour geometry and transient slip state are independent modules.
Existing `advanced_road` content maps to this provider through a compatibility
seed bridge; the Step 29G generalized curve remains an explicit fallback.

Heritage does not label unpublished Siemens 2512 wet-road or MF-Swift internals as
implemented. Those capabilities may only be added from public equations or via
our own independently specified physical models.

## Consequences
- Cars, karts, trucks and future motorcycle solvers can share one empirical
  force/moment provider with different identified data.
- Motorcycle crown/contact geometry can evolve independently from force fitting.
- TIR import can be added without changing the wheel/contact API.
- Rigid-ring/enveloping, thermal, wet-film and wear state remain composable
  mechanisms rather than one untestable tire monolith.
