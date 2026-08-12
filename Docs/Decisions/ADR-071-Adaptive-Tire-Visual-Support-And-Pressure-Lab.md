# ADR-071 — Adaptive Tire Visual Support And Pressure Lab

**Status:** Accepted for TIRE17C1 (2026-08-11)

## Decision

- Tire physics keeps the adaptive TIRE06 road-envelope policy: the cheap footprint pattern is used on smooth support and a refined 3x3 lattice is queried when height/support/material/wetness discontinuities justify refinement.
- When that refined 3x3 lattice is already available, its support-height residual field is retained for presentation. No additional collision rays are added for visual deformation.
- The tire vertex/shadow shaders bilinearly reconstruct that local support field and allow only the road-facing tread/contact region to conform to a bounded curb/step profile. Smooth road keeps the previous single support-plane path.
- Physical rigid-ring state remains authoritative. Presentation clamps extreme transient belt translations so a road tire cannot visually tear away from its bead/rim after a sharp impact.
- The Tire Lab exposes fitted cold inflation pressure inside the common validity range of the fitted tires. The control edits cold/reference pressure; live gas pressure remains thermal-state driven.
- True zero-pressure collapse/puncture is not faked by this Lab control. It requires the later tire-damage/failure model.
- Prototype pneumatic vertical stiffness uses a conservative low-confidence pressure coupling around the authored nominal stiffness until measured pressure-vs-stiffness data is available. The authored stiffness remains the nominal-pressure datum.

## Rationale

The prior renderer reduced a refined multi-sample physical footprint to one infinite plane. A tire could therefore physically detect a curb while visually remaining a flat pancake. Retaining already-computed support residuals removes that mismatch without increasing collision-query cost.

Inflation pressure must also be testable without editing tire files. Pressure changes already affect force/contact/thermal/wet mechanisms, and TIRE17C1 also lets it influence the pneumatic share of vertical support so low-pressure testing produces a coherent footprint/deflection response.
