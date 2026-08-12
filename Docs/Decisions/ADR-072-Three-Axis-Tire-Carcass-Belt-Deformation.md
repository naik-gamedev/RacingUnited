# ADR-072 — Three-Axis Tire Carcass/Belt Deformation

**Status:** Accepted for TIRE17C2 (2026-08-11)

## Decision
Heritage tire presentation must preserve the physical distinction between radial compression, longitudinal belt shear and lateral belt/carcass shear. The bead/rim interface remains nearly anchored while deformation increases through the sidewall toward the belt/tread.

The renderer consumes VehicleSystem's authoritative world-space wheel-forward, wheel-right and contact-normal vectors. It must not infer braking/cornering directions solely from GLB local axes because mirrored wheel nodes can reverse that inference.

- Radial deformation: loaded tread footprint, adaptive 3x3 curb/step support, lower-carcass compression and bounded sidewall bulge.
- Longitudinal deformation: SWIFT-like rigid-ring displacement and wind-up under braking/drive.
- Lateral deformation: SWIFT-like rigid-ring displacement/yaw under cornering.
- Main and shadow geometry use the same positional deformation.
- Physical rigid-ring state remains authoritative; presentation gains/clamps only keep deformation readable and prevent visual bead tearing.

This is a visual/structural representation layer, not a replacement for MF6.2 force generation.
