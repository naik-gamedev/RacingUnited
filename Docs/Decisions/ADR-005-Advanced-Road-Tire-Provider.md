# ADR-005: Native Advanced Road-Tire Provider

## Status
Accepted at Step 29G; revised by TIRE01 / ADR-035.

## Decision
Road-tire force generation is a dedicated native C++ subsystem in
`Vehicles/TireModel.*`, not an ever-growing block inside `VehicleSystem.cpp`
and not Lua simulation code.

TIRE01 makes a clean-room implementation of the publicly documented MF-Tyre
6.x/MF6.2-compatible force/moment family the default road provider. The previous
Step 29G generalized sine/arctangent curve remains available as
`legacy_generalized_road` for regression/fallback use. Heritage does not copy or
claim unpublished Siemens Simcenter Tire 2512/MF-Swift internals.

`VehicleSystem` remains responsible for wheel kinematics, suspension contact,
surface input, wheel torque integration and applying returned forces to the
chassis. `TireModel` owns provider selection and force/moment evaluation.
`TireSlipDynamics` owns transient relaxation, while motorcycle contour geometry
is isolated in `MotorcycleTireProfile`.

## Why
- Keeps the 1000 Hz vehicle loop readable and testable.
- Provides a recognized fitted-parameter vocabulary for future tire-rig data.
- Supports large-camber motorcycle tire forces without creating a separate
  unrelated motorcycle force law.
- Lets future road, motorcycle-profile, low-pressure and deformable-terrain
  providers share a stable vehicle/contact boundary.
- Prevents Lua vehicle definitions from containing high-rate solver logic.

## Current limitation
Loose gravel/dirt/grass/snow/ice still modify the road provider through surface
multipliers. That is intentionally temporary. Deformable terrain gets its own
terramechanics provider. Full MF-Swift rigid-ring/enveloping, turn-slip, fitted
TIR import, thermal/wear state and a Heritage wet-film model remain later work.
