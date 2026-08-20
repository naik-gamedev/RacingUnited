# ADR-092 — Textured optical rain material

## Status
Accepted for WEATHER07B.

## Context
WEATHER07A established physical rain population, terminal velocity, rainfall
mass and deterministic world-cell identity. Its temporary WEATHER06-era visual
consumer still rendered mostly procedural pale streaks. That was useful for
physics acceptance but not a suitable final optical representation.

A rendered rain streak is not the physical geometry of an elongated raindrop.
It is an exposure-dependent image of a millimetre-scale drop moving through the
camera shutter interval. Therefore presentation may stretch a quad while the
physical drop remains spherical/oblate statistical microphysics.

## Decision
Near rain uses velocity-aligned instanced quads. Each module may provide the
following conventional assets below `Assets/Weather/Rain`:

- `RainDrop_BC.png`: coverage/opacity;
- `RainDrop_N.png`: tangent-space normal;
- `RainDrop_TN.png`: linear water thickness.

These textures define optical shape only. Drop diameter, terminal velocity,
horizontal wind coupling and world trajectory remain derived from WEATHER07A.
The vertex shader samples the same truncated exponential diameter population and
Atlas-style terminal-velocity relation used by the physical authority, then
converts velocity to apparent streak length through a bounded presentation
exposure interval.

All three authored maps are treated as linear data. They are uploaded without a
vertical image flip; the quad UV reverses V instead. This preserves the authored
normal-map channel convention while placing the rounded bulb at the falling
head. Thickness is separate from opacity so clear water can create strong optics
without becoming opaque white paint.

The fragment shader combines coverage, tangent-space curvature, water Fresnel,
thickness and the live environment cubemap. True scene refraction is deferred to
WEATHER07C, which will introduce one engine-level resolved scene-colour/depth
input for refractive materials rather than copying the framebuffer per drop.

The WEATHER06I CPU triangle tier remains a compatibility fallback only when the
optical triplet cannot be loaded. It must not overlay the textured path.

## Consequences
- Art can improve without changing precipitation physics.
- A tiny 64x128 source map can represent many physical sizes and streak lengths.
- Normal and thickness maps contribute real optical variation instead of baked
  blue/white colour.
- Missing textures fail gracefully to a subdued procedural shape.
- The module path is conventional but not Racing-United-specific.
- Scene-colour refraction, impact splashes, wet material response and windshield
  water remain later milestones.
