# ADR-046: Compacted Snow and Hard Ice Surface Interaction

## Status
Accepted — TIRE13 current candidate.

## Context
Hard ice and compacted snow cannot be represented credibly by a single constant friction value, but they are also not the same regime as deep deformable snow. Heritage already owns an MF6.2/SWIFT-like pneumatic tire stack, adaptive 2D footprint, thermal state and 16x3 spatial tread history.

## Decision
Add `TireWinterSurfaceInteraction.*` as a clean-room hard-winter surface provider around one MF6.2 evaluation per tire.

Hard ice uses separate tire/surface mechanisms for local surface temperature, contact slip speed, surface wetness/interface melt film, winter-compound effectiveness, siping and optional stud count/protrusion. Stud traction is a bounded mechanical contribution, not a generic direct grip switch.

Compacted snow remains a load-bearing surface. It adds tread-block/sipe interlock and a 16x3 packed-snow tread state. Packed snow rotates with the material-fixed tread cells and is shed by rotation/speed/slip/self-cleaning. Deep snow sinkage, bulldozing and rut formation are explicitly deferred to TIRE15 terramechanics.

Until the scene `SurfaceField` exists, static-scene winter contact receives an explicit -5 C compatibility surface temperature. The provider API is temperature-dependent now so this bridge can later be replaced without rewriting the tire solver.

Prototype Peugeot coefficients remain synthetic development placeholders and must not be presented as measured Pirelli winter-tire data.

## Consequences
- Snow and ice remain distinct footprint fractions.
- One MF6.2 evaluation per tire remains the default performance contract.
- Winter tire traits become authorable/fittable data rather than surface-specific magic grip numbers.
- Deep snow remains structurally separate from compacted snow.
