# PERF13 - Seamless Water Material Recovery

## Goal

PERF12 solved adaptive water geometry cost, but the live water presentation still had two visual risks: the actual reflective material was too easy to lose at rain-film depths, and neighbouring adaptive quads could expose their construction through transparent overlap or different local procedural coordinates.

PERF13 restores the dedicated visible water material on top of the PERF12 adaptive mesh without changing authoritative hydrology.

## Geometry continuity

Hydrology/adaptive patches are axis-aligned in world X/Z. The water vertex shader now preserves that exact footprint. Each corner uses the patch's world-X/Z half extent and solves its Y coordinate from the fitted support plane normal.

This replaces the old tangent-space square expansion. The old path also multiplied half size by `0.505`; that deliberate 1% overlap was tolerable for opaque debug geometry but is incorrect for alpha-blended water because overlapping neighbouring patches accumulate color/alpha and reveal seams. PERF13 uses the exact `0.5 * patchSize` footprint.

Consequences:

- a planar sloped road receives a sloped water surface;
- adjacent 0.5/1/2/4/8/16 m patches meet on the same X/Z boundaries;
- adaptive LOD no longer rotates or enlarges the presentation footprint;
- transparent overlap is no longer used as a crack-hiding mechanism.

## One continuous optical field

Procedural water detail is world anchored. Each record stores a bounded FP32 phase derived from its global X/Z centre. The vertex shader adds the actual world-X/Z corner offset, so the fragment shader sees the same procedural coordinates regardless of which adaptive patch contains a pixel.

Flow waves, broad ripples, roughness variation and rain-impact rings therefore do not restart at card boundaries. Patch size changes geometry cost only; it does not rescale or restart the material.

## Thin film versus pooled water

The material deliberately avoids a blue-card look.

- Micrometre-scale rain film quickly becomes readable as a neutral dark/glossy overlay while remaining translucent enough to preserve the authored road/terrain color underneath.
- Small depth differences are saturated over a broad thin-film range so neighbouring hydrology cells do not become a depth-colored checkerboard.
- Millimetre-scale accumulation progressively increases reflection strength and opacity.
- Deeper standing water receives lower roughness, stronger Fresnel response and rain-impact normal disturbance.
- The optical normal trends toward gravity-up as standing-water depth increases. Geometry remains conservative and surface-following until a later connected free-surface/puddle reconstruction system is introduced.

No external texture is required for PERF13. Ripple/flow/impact detail is procedural and render-rate animated. Artist-authored seamless ripple normal maps remain an optional later polish layer, not a requirement for continuity.

## Performance and authority

PERF13 does not change:

- 0.5 m authoritative hydrology cells;
- 30/20/6/2/0.5 Hz distance cadence;
- PERF12 adaptive 0.5/1/2/4/8/16 m presentation policy;
- phased 50-100 m and 100-200 m presentation caches;
- 200 m explicit-water draw limit;
- WEATHER07C6 rain density/range calibration.

The water path remains one instanced four-vertex strip per presentation patch with no water geometry shader and no per-frame texture uploads.

## Future polish

After the material is proven live, the next art-oriented upgrades may include two seamless static normal maps (fine and broad ripples) blended/panned in world space. They should supplement, not replace, the world-space continuity contract established here. Screen-space/planar reflection of nearby dynamic objects is also a separate future reflection tier.

## Opacity-correct cadence/LOD handoffs

Adjacent presentation caches overlap around their radial boundaries. With ordinary
straight-alpha blending, multiplying each duplicate layer's alpha by complementary
LOD weights does not reproduce the opacity of one full layer; the overlap can form
a visible annular seam. PERF13 gives all internal water-ring boundaries a common
3 m transition width and weights **transmittance** instead:

`weightedAlpha = 1 - (1 - baseAlpha)^weight`

For two complementary weights, the combined transmittance exactly equals the
single-layer target. The final 197 m ring centre plus the same 3 m fade reaches
zero explicit-water opacity at 200 m.
