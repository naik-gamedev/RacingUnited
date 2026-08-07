# ADR-015: Live Per-Wheel Suspension Tuning

Status: Accepted in Step 29N.

## Context

The nonlinear provider introduced in Step 29M was authorable through compiled
vehicle definitions, but the ordinary prototype and live tools could not apply
or read its complete parameter set. A Lua-only copy of the values would drift
from the native simulation and could not support asymmetric setups.

## Decision

`VehicleSystem` owns atomic set and readback operations for the complete
`SuspensionModelDescription` of each wheel/contact unit. Lua uses one-based
wheel indices and receives the exact native values. Invalid updates fail before
changing any field. The old `Vehicle.AddWheel` arguments remain in their
original order; nonlinear parameters are optional trailing arguments.

Racing United exposes the bridge in a focused `SUSP.` tab. Slider changes update
only the selected wheel, while explicit actions read native state, copy the tune
to all wheels, or restore the prototype definition.

## Consequences

- Front/rear and left/right suspension differences are now representable and
  testable without respawning the vehicle.
- Tools no longer pretend their local defaults are authoritative native state.
- Existing modules remain source compatible and retain linear defaults until
  they opt into the new fields.
- Linkage geometry, anti-roll coupling, thermal state, wear and damage remain
  separate future systems.
