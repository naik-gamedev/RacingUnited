# ADR-074 — Single-Authority Flexible-Ring Tire Presentation

**Status:** Accepted for TIRE41 (2026-08-12)

## Context

The previous visual tire implementation accumulated separate flat-road,
contact-plane, probe-grid, collider-triangle, rigid-ring, sidewall-bulge and
carcass-profile displacements. A vertex could be moved by eleven statements in
the visible shader and nine in the shadow shader. Their masks and clamps could
not preserve one continuous carcass, producing a rectangular lower flap and
localized dents during curb contact.

Published real-time tire work points to a structural model instead of layered
silhouette corrections. iRacing describes a physically based carcass supplying
vertical, lateral, longitudinal and torsional stiffness plus contact-patch
dimensions and pressure. Studio 397's rFactor 2 TGM documentation describes a
node-based tire and contact-pressure distribution. Live for Speed describes a
high-resolution carcass, belt and air-pressure model whose behaviour is reduced
to a real-time approximation. Flexible-ring literature couples radial and
tangential belt deformation on an elastic foundation.

References:

- https://www.iracing.com/physics-modeling-ntm-v7-info-plus/
- https://www.studio-397.com/wp-content/uploads/2016/12/rF2_TGM_TyreTool_QuickStart_V3.pdf
- https://www.lfs.net/report-dec2010
- https://aaltodoc4.aalto.fi/items/75b234f3-b535-4354-b9a1-a35e43726edd
- https://saemobilus.sae.org/articles/a-3d-semi-empirical-road-transient-tire-model-2010-01-1916

These sources inform the architecture; they do not expose enough proprietary
implementation detail to copy any commercial simulator.

## Decision

Tire mesh presentation has exactly one deformation authority:
`TireFlexibleRingField`.

1. A bounded 21-by-13 collision lattice samples distributed road contact around
   the lower carcass. Duplicate diagonal hits on the ordinary support plane are
   rejected.
2. Native deflection, patch dimensions, load, inflation pressure, authored
   vertical construction stiffness, non-radial rigid-ring modes and flat-spot
   state enter the same solver. Road-envelope radial height remains a support
   physics state and is not reapplied as a whole-belt visual translation.
3. A cyclic 24-by-13 elastic-foundation solve distributes contact and section
   displacement through the complete belt. Circumferential and lateral coupling
   replace independent vertex dents. Near-incompressible section response moves
   displaced volume toward the sidewalls, with asymmetric obstacle contact
   favouring the unsupported side.
4. The solver publishes one final forward/down/lateral metre-domain field. The
   shader only interpolates that field, attaches it continuously from bead to
   tread, and adds it once. The shadow shader follows the same rule.

The model is deterministic and reduced-order. It is not an unrestricted soft
body and does not turn render vertices into physics bodies.

## Removed authorities

- scalar contact-plane flattening and hard-plane clamps;
- shader-generated pneumatic bulges;
- independent probe-grid vertex dents;
- GPU collider-triangle displacement;
- the separate `TireCarcassProfile` provider;
- the old `Entity.SetMeshNodeTireDeformation` Lua API;
- the 3-by-3 visual support and 64-triangle visual collider caches.

Tire force laws, road enveloping and the native rigid-ring transient model remain
physics systems. They provide state to the flexible-ring field but do not mutate
the rendered mesh separately.

## Invariants

- One visible-shader position addition and one equivalent shadow addition.
- No render-time contact plane, probe, collider or bulge displacement.
- Cyclic circumferential continuity at the first/last station.
- Bead attachment approaches zero at the rim; belt response is continuous.
- Pressure changes structural compliance and sidewall displacement.
- Pressure-derived hoop tension and construction stiffness retain the unloaded
  crown and enforce a pressure-dependent bead/rim-clearance envelope.
- Contact-side indentation and free-side expansion remain bounded by section and
  sidewall dimensions.
- Source mesh, rim, brake and collision geometry remain unchanged.

## Consequences

There is now one place to improve the physical approximation and one final field
to inspect. Commercial-grade calibration and validation still require measured
tire data, high-speed reference footage and construction-specific parameters;
the architectural stacking defect is removed rather than hidden by more masks.
