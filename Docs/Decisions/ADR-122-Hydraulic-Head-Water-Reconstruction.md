# ADR-122 — Hydraulic-Head Water Reconstruction on Authored Surfaces

## Status

Accepted for WATER15F.

## Context

WATER15E proved that increasing the presentation cache to 4096×4096 does not
create information when the cache stores adaptive control-volume **water depth**
directly. The renderer simply magnified solver ownership: half-metre and larger
control volumes became visible as square wet/puddle patches, and a permissive
vertical match could let nearby road state shade a raised sidewalk.

The authoritative hydrology solver is still valid and must remain the only owner
of water mass. The failure is presentation reconstruction, not conservation.

## Decision

The Dynamic Track-style presentation cache stores a reconstructed water
free-surface / hydraulic-head elevation together with the supporting collider
elevation for each of the two retained vertical surface layers:

- `R = waterSurfaceHeight - clipmapHeightOrigin`
- `G = supportSurfaceHeight - clipmapHeightOrigin`

The ordinary authored road/terrain material derives local visible depth from:

`localDepth = max(waterSurfaceHeight - exactFragmentSurfaceHeight, 0)`

No generated visible water geometry is introduced.

### Corner reconstruction

`SurfaceHydrology::collectVisualCells()` now emits a presentation-only
`cornerWaterSurfaceElevationM` field. It is derived from authoritative cell
volume/depth but is not fed back into physics.

- microscopic/thin film follows the supporting collider plane;
- standing water progressively approaches hydraulic head;
- compatible neighbouring cells are blended at shared corners so adaptive
  control-volume boundaries do not become visible square puddle borders;
- neighbours whose support elevations differ by more than 2 cm are not blended,
  preventing curb/sidewalk steps from being smoothed into one water surface.

### Fragment reconstruction

The material shader performs layer-aware bilateral interpolation. Spatial
bilinear weights are rejected/attenuated when neighbouring atlas samples belong
to a vertically different support surface. This prevents a road fragment from
borrowing a sidewalk or bridge sample merely because it lies in the same X/Z
filter footprint.

### Precision

The requested clipmap hierarchy remains:

- LOD0: 4096×4096 / 100×100 m
- LOD1: 2048×2048 / 200×200 m
- LOD2: 1024×1024 / 400×400 m
- LOD3: 512×512 / 2000×2000 m, with a strict 1000 m radial presentation cap

State maps move from `RG16F` to `RG32F`. Height precision is more important than
saving roughly half the state-map bandwidth because centimetre-class curb
separation and millimetre water depths must coexist. The offscreen top-surface
selection depth attachment is reduced to `DEPTH_COMPONENT16`; it is a temporary
selection buffer, not stored physical water data.

### Optical correction

Standing-water normals are derived from derivatives of the reconstructed
hydraulic-head field, then perturbed by ripples. Thin film continues to follow
the authored surface normal.

Until Heritage has screen-space reflections, environment-only Fresnel is capped
at a bounded reflection weight so a grey rainy cubemap does not make low-angle
puddles look like opaque cement. The authored substrate remains visibly
transmitted through shallow water.

## Consequences

Positive:

- adaptive hydrology control-volume squares are no longer the visible puddle
  primitive;
- a curb clips road water by its real scene height rather than by special render
  tessellation;
- large authored polygons remain valid because the wet/dry boundary is resolved
  in the shader;
- bridges/tunnels remain supported by the two-layer vertical state cache;
- physical water volume, flow, drainage, evaporation and tire interaction are
  unchanged.

Tradeoffs:

- two RG32F clipmap chains consume more VRAM/bandwidth than WATER15E RG16F;
- the presentation still cannot invent true centimetre-scale *physical* flow
  state inside a coarse authoritative cell; it reconstructs a plausible local
  free surface from conserved cell state and exact scene elevation;
- SSR remains desirable for convincing low-angle local reflections.

## Rejected alternatives

- **Blur the depth atlas:** hides squares while leaking across curbs and destroys
  sharp hydraulic barriers.
- **Increase adaptive simulation density everywhere:** wastes CPU/physics work
  and re-couples rendering quality to solver tessellation.
- **Return to generated water geometry:** reintroduces z-fighting, seam welding,
  T-junctions and coarse/fine topology failures already retired by WATER15.
