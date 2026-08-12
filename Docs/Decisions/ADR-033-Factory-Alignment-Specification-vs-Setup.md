# ADR-033: Factory Alignment Specification vs Vehicle Setup

## Status
Accepted for ALIGN01.

## Decision
Factory alignment evidence is stored separately from the player's/current vehicle
setup. A factory MIN/MAX specification is a reference envelope, not a gameplay
clamp and not automatically a nominal target.

When a source provides MIN/MAX values but no standard/nominal value, Heritage may
use the midpoint as an explicitly labelled workshop default. The midpoint must
retain provenance stating that it was derived from the supplied range rather than
presented as a factory target.

Alignment remains per corner. Front and rear left/right linking is a UI/setup
convenience only and can be disabled for oval, race, historical, damaged or other
asymmetric vehicles.

## Human-facing sign convention

- Camber: negative = tire top inward; positive = tire top outward.
- Toe: positive = toe-in; negative = toe-out.
- Caster: positive = rearward steering-axis inclination in side view.

## Precision and ranges

The setup UI provides both a slider and a visible exact numeric editor. Normal
sliders use practical ranges and 0.01 degree increments. Exact entry preserves
finer typed values (for example `0.82` or `0.8237`) within the engine-safe range.
An advanced slider mode exposes the broader native limits.

Factory ranges are displayed as IN SPEC / CUSTOM OUTSIDE SPEC but never prevent a
valid native setup such as large positive/negative camber.

## Peugeot 206 RC evidence used by ALIGN01

The project author supplied a table containing these ranges:

- front total toe: -0.20 to -0.03 deg;
- front per-wheel toe: -0.10 to -0.02 deg;
- front camber: -0.50 to +0.50 deg;
- rear total toe: +0.43 to +0.60 deg;
- rear per-wheel toe: +0.22 to +0.30 deg;
- rear camber: -1.50 to -0.50 deg;
- caster: +2.70 to +3.70 deg;
- steering-axis inclination: +9.20 to +10.20 deg.

The supplied table did not contain a populated standard/nominal column. ALIGN01
therefore uses midpoint workshop defaults while preserving the full ranges.
