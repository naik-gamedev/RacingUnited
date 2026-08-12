# ADR-048 — Shallow Granular Gravel and Hard Dirt

**Status:** Accepted / TIRE14 candidate  
**Date:** 2026-08-10

## Context

Rally gravel and hard dirt are not well represented by assigning a lower friction coefficient to an
otherwise rigid asphalt contact. A shallow loose layer can penetrate, shear, compact and be pushed
sideways while a harder substrate still supports the pneumatic tire. Heritage therefore needs a
surface interaction regime between hard-surface MF6.2 and the fully deformable terramechanics planned
for TIRE15.

The tire/surface authoring contract from ADR-047 also requires tire properties and ground properties
to remain separate. Tread geometry/compound traits belong to the tire dataset; loose-layer depth,
density, cohesion, friction angle, moisture and persistent terrain state belong to SurfaceMaterial /
SurfaceField.

## Decision

Add `Vehicles/Tires/TireShallowGranularInteraction.*` as a narrow clean-room provider for Gravel and
Dirt contacts with a load-bearing base.

The normal high-fidelity path remains:

`surface -> footprint/enveloping -> structural tire -> transient/contact state -> one MF6.2 evaluation`

TIRE14 then composes shallow-granular terrain reaction around that base tire response. It does **not**
replace MF6.2 with a gravel magic coefficient, and it does not evaluate MF once per footprint sample.

### Current mechanisms

1. **Shallow pressure/sinkage.** Contact load and TIRE04 footprint area produce contact pressure. A
   bounded reduced-order power-law pressure/sinkage relation estimates penetration into the loose
   layer, capped by both authored layer depth and a tire-provider safety bound.
2. **Available granular shear.** Effective cohesion and internal-friction angle form a
   Mohr-Coulomb-style maximum shear-stress capacity under the contact pressure.
3. **Shear-displacement mobilization.** Longitudinal slip ratio and lateral slip angle create
   contact-length-scaled shear displacements. An exponential Janosi/Hanamoto-style mobilization law
   grows the usable terrain reaction progressively rather than switching traction on instantly.
4. **Tread coupling.** Tread aggressiveness, biting-edge density, open void, remaining tread depth and
   explicit granular coupling determine how much available terrain shear the tire can mobilize.
5. **Lateral bulldozing.** Sinkage plus lateral slip produce a bounded passive-wedge reaction using a
   clean-room earth-pressure approximation. This is a physical extra lateral mechanism, not a larger
   MF friction coefficient.
6. **Longitudinal plowing/compaction.** A sunk tire consumes energy by compacting/displacing the loose
   layer. This is applied as resistive longitudinal drag with explicit compaction-power telemetry.
7. **Partial footprint support.** TIRE06 adaptive footprint Gravel/Dirt fractions blend the provider
   continuously when only part of the tire is on loose material.
8. **Physical support datum.** Calculated sinkage moves the road support/contact datum downward in both
   massless and unsprung-mass paths, so the wheel can physically settle into the loose layer rather
   than showing sinkage only in telemetry.

## Authoring boundary

The new `.tir` section `[HERITAGE_SHALLOW_GRANULAR]` contains **tire/tread traits only**, including
aggressiveness, edge density, open void, shear/bulldozing/plowing coupling and worn-tread response.
The prototype Peugeot values are synthetic development placeholders and are not measured Pirelli or
rally-tire data.

TIRE14 currently uses explicit synthetic Gravel/Dirt material presets inside the provider as a
compatibility bridge because SurfaceMaterial/SurfaceField does not yet own the necessary material
state. These presets are not part of the tire dataset and must migrate to scene surface assets when
that infrastructure is implemented.

## Deliberate limits

TIRE14 does not claim full terramechanics or permanent terrain deformation. It does not yet store:

- rut depth or permanent height change;
- multipass compaction memory;
- loose-material redistribution;
- moisture evolution;
- persistent shear history;
- deep sinkage through mud, sand, deep snow or soft soil.

Those belong to TIRE15, where a dynamic SurfaceField can own spatial terrain state and a dedicated
terramechanics provider can use full surface parameters such as pressure-sinkage coefficients and
persistent compaction/shear history.

The passive-wedge bulldozing and reduced pressure-sinkage formulas are Heritage clean-room
approximations. No proprietary commercial implementation is copied or claimed.

## Performance contract

- One MF6.2 evaluation per tire remains the normal high-fidelity path.
- TIRE14 consumes existing adaptive footprint fractions instead of adding dense granular contact
  meshes.
- Tire/structural state remains compatible with the 1000 Hz high-rate vehicle cadence.
- Persistent terrain queries/state are deferred to TIRE15 and must receive explicit fidelity/LOD
  tiers for large race fields.

## Consequences

Rally gravel and hard dirt can now produce traction from more than a lowered rubber friction curve:
there is shallow penetration, progressive granular shear, lateral material reaction and plowing loss.
An aggressive/deep rally tread can exploit the same ground more effectively than a worn road tread.
The architecture also leaves the ground side ready to become authored/dynamic independently of the
tire when SurfaceMaterial/SurfaceField arrives.
