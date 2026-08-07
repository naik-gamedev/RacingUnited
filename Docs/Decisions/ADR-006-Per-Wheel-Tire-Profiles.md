# ADR-006: Per-Wheel Tire Profiles

**Status:** Accepted at Step 29H.

## Decision

Every native wheel/contact unit owns its own `TireModelDescription`.

`Vehicle.SetTireModel` remains as the backward-compatible vehicle-wide setter:
it changes the vehicle default and reapplies that profile to all current wheels.
`Vehicle.SetWheelTireModel` changes only one wheel. New wheels inherit the
vehicle default at creation time.

Racing United Lua vehicle definitions reference named tire preset data. Preset
names are content-layer identifiers only; the C++ solver stores validated
numeric tire data and exposes `Vehicle.GetWheelTireModel` for authoritative
readback.

## Why

A single tire description per vehicle cannot represent staggered car tires,
different motorcycle front/rear constructions, multi-axle truck fitments,
temporary spares, mixed compounds, tire replacement, or per-corner damage.
Per-wheel ownership also gives future temperature, pressure, wear, and carcass
state a natural home.

## Guardrails

- Do not make the native solver depend on Racing United preset names.
- Do not duplicate tire solver code by vehicle category when data/configuration
  is sufficient.
- Keep `SetTireModel` behavior stable for older modules and simple prototypes.
- Production tire profiles must be clearly distinguished from generic test data.
- Future thermodynamic/wear state belongs per tire/wheel, not in one global
  vehicle table.
