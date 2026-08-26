> **SUPERSEDED BY CLOUDURP15E6.** The implementation described below is retained only as milestone history and is no longer present in the renderer.

# CLOUDURP15E4 — Adaptive high-strength cloud TAA

CLOUDURP15E4 corrects the temporal-history sampling path and applies the requested
three cloud TAA regimes.

## Corrected reprojection sampling

The prior cloud history sampler explicitly forced `GL_NEAREST`. Temporal reprojection
usually lands between history texels, so nearest sampling quantized the lookup back to
a single previous texel and preserved much of the salt-and-pepper appearance. Cloud
history now uses `GL_LINEAR`, allowing sub-pixel reprojected samples to interpolate
properly. The history textures remain full-resolution RGBA16F ping-pong targets.

## Requested adaptive strengths

The temporal shader uses a spatial/temporal stochastic-noise classifier and three
persistence regimes. Severe one-pixel grain can be classified from spatial isolation
even when it happens to remain stable across consecutive frames; temporal flicker is
still used as additional evidence:

- stable cloud structure: **20% history**;
- mild detected dithering: **60% history**;
- strong detected dithering: requested **5000% TAA intensity**.

A blend weight above 100% is mathematically invalid for `mix()` and would extrapolate
HDR values. Therefore 5000% is represented as a **50:1 history/current persistence
ratio**, which is `50 / 51 = 98.0392%` history and `1.9608%` current frame. This gives
severe stochastic pixels approximately fifty times as much temporal persistence as
the new sample without producing unstable negative/overbright extrapolation.

## Motion protection

The existing coherent-neighbourhood reactive test remains. Actual appearing,
disappearing or coherently moving cloud structure can reject old history so the very
strong dither regime does not simply drag stale silhouettes across the sky. A small
reprojection-velocity retention factor further reduces history during fast motion.

## Unchanged

- Volumetric density, lighting and cloud morphology are unchanged.
- The cloud raymarch step budget is unchanged.
- CELESTIAL04 Sun/Moon cloud-shadow generation and receivers are unchanged.
- The weather/radar/rain authority is unchanged.
