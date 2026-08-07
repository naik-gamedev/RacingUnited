# ADR-005: Native Advanced Road-Tire Provider

## Status
Accepted at Step 29G.

## Decision
Road-tire force generation is a dedicated native C++ subsystem in
`Vehicles/TireModel.*`, not an ever-growing block inside `VehicleSystem.cpp`
and not Lua simulation code.

The Step 29G provider uses an original generalized sine/arctangent pure-slip
curve whose small-slip stiffness, peak friction, shape, curvature, load
sensitivity, relaxation and combined-slip envelope are explicit tire data.
It is not a copy of Siemens MF-Tyre/MF-Swift or another proprietary tire
implementation.

`VehicleSystem` remains responsible for wheel kinematics, suspension contact,
surface input, wheel torque integration and applying the returned forces to the
chassis. `TireModel` receives an already-relaxed contact state and returns tire
forces/moments plus telemetry.

## Why
- Keeps the 1000 Hz vehicle loop readable and testable.
- Gives future AI/human contributors one obvious place for tire-force laws.
- Lets future motorcycle, low-pressure ATV and deformable-terrain providers
  share the same stable contact boundary.
- Prevents Lua vehicle definitions from containing high-rate solver logic.

## Current limitation
Loose gravel/dirt/grass/snow/ice still modify this road provider through
surface multipliers. That is intentionally temporary. Deformable terrain gets
its own terramechanics provider later rather than being represented only by a
friction coefficient.
