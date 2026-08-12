# ADR-040 — Tire Thermal / Pressure Energy State

**Status:** Accepted for TIRE07

## Context

The MF6.2 steady-state force provider already accepts inflation pressure and TIRE04 uses
pressure in finite-footprint geometry, but those values were static authoring inputs. Modern
tire behavior requires thermal history and pressure evolution without entangling proprietary
commercial Temperature & Velocity equations with Heritage's public/clean-room MF branch.

## Decision

Heritage owns tire thermal behavior as a separate native mechanism in `TireThermal.*`. The
initial implementation uses three lumped energy states: tread, carcass and contained gas.
Slip work and tire losses provide heat sources; configurable conductances exchange energy
among tread, carcass, road, ambient air and contained gas. Vehicle speed increases convective
air cooling. The mechanism advances at the normal high-rate tire timestep and also cools
airborne tires when the normal road-contact force path is absent.

Contained-gas pressure is derived from gas temperature with an ideal-gas absolute-pressure
relationship while external authoring/telemetry continues to use gauge pressure. Dynamic
pressure feeds the existing MF operating input and contact-geometry layer. Tread temperature
may scale available friction and carcass temperature may scale tire stiffness through bounded,
reference-normalized response curves. These clean-room response curves are explicit Heritage
authoring data, not claimed MF-Tyre Temperature & Velocity coefficient parity.

Tire property files may carry `[HERITAGE_THERMAL]` as a project-owned extension. FITTYP70 or
other commercial data is never silently interpreted as this extension merely because it
contains temperature/velocity content. Prototype Racing United thermal values must remain
identified as synthetic unless measured/fitted evidence replaces them.

## Consequences

- Tire behavior can evolve over a stint from actual slip/loss history instead of a static
  temperature scalar.
- Inflation pressure becomes stateful and can influence both MF pressure sensitivity and the
  physical footprint/effective-contact calculations already present.
- TIRE08 wear can consume authoritative thermal and dissipated-energy histories without
  embedding wear inside MF6.2.
- A future evidence-backed/commercial-compatible T&V response provider can replace the
  clean-room grip/stiffness mapping without rewriting the thermal energy network or wheel ABI.
- Synthetic prototype heat capacities/conductances are development data and must not be
  presented as measured historical tire properties.
