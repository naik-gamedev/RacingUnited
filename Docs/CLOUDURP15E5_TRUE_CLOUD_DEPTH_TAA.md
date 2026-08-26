> **SUPERSEDED BY CLOUDURP15E6.** The implementation described below is retained only as milestone history and is no longer present in the renderer.

# CLOUDURP15E5 — True cloud-depth temporal resolve

CLOUDURP15E5 repairs the Heritage volumetric-cloud temporal resolve rather than only changing blend constants.

- The temporal pass reprojects from the raymarch mean cloud depth (`m_cloudRaymarchDepthTexture`) instead of opaque scene depth / far-plane depth.
- Stochastic ray integration uses a render-frame index, independent of simulation clock speed or pause state.
- Current and history use matching five-tap low-frequency neighbourhoods for reactive rejection, so single-pixel stochastic disagreement is accumulated rather than rejected.
- History variance clipping is broad sigma clipping instead of current-frame min/max clipping.
- Requested regimes remain 20% stable, 60% mild dither and 50:1 (98.0392%) history for strong dither.
- Ping-pong full-resolution RGBA16F history and GL_LINEAR history sampling remain intact.
