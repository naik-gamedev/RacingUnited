# WATER15 — Dynamic Track Surface-State Water

## Scope and provenance

WATER15 replaces Heritage Engine's renderer-owned settled-water tessellation with a persistent surface-state architecture targeted at the publicly documented behavior of iRacing's Dynamic Track / Tempest rain system.

**Important provenance limit:** iRacing does not publish the source code, numerical solver, track-state data layout, texture/mesh representation, simulation resolution, update scheduling, or exact coefficients used by Dynamic Track. Heritage Engine therefore must not claim byte-for-byte, algorithm-for-algorithm, or private implementation identity. The target is **behavioral parity with every relevant behavior iRacing has publicly documented**, while Heritage keeps its own independently implemented solver and renderer.

Publicly documented target behaviors include:

- persistent water and moisture levels on the track surface;
- puddle formation and a changing wet driving line;
- water moved/cleared by tires and by aerodynamic forces;
- drying, evaporation, and track-specific drainage;
- wet grip affected by surface roughness / polish and wetness;
- tire tread channeling, water cast-off, spray, and hydroplaning;
- wetness shaders, puddles/pooling, reflections, and ripples;
- continuous visual gradation at very low water levels and clear drying lines.

## WATER15 architectural decision

Authoritative hydrology and visible water are no longer the same geometry.

`SurfaceHydrology` remains the world-owned physical authority. It owns conserved water volume, surface elevation, flow, rainfall input, infiltration, drainage, evaporation, tire clearing, and the distance-adaptive simulation cadence. Rendering may sample that state but may never create, delete, or relocate physical water merely to make a picture.

Settled-water **presentation no longer creates a separately tessellated water mesh**. The ordinary scene is rendered first. WATER15B then re-submits only `SurfaceWetnessReceiver` geometry with the exact same authored triangles and model/view/projection transform. With Heritage's reversed-Z depth buffer, `GL_GEQUAL` accepts the equal-depth road/terrain samples and rejects receiver fragments hidden by nearer cars or props. The water shader looks up the corresponding hydrology surface state and shades that exact authored surface as damp, wet, or pooled.

WATER15A briefly used a fullscreen depth/stencil reconstruction prototype. Live testing showed no visible water, so WATER15B retired that fragile dependency rather than layering more fixes onto it. No copied depth/stencil image or fullscreen reconstruction is part of the active path.

Consequences:

- no water-ring geometry;
- no water seam welding;
- no coarse/fine T-junction repair;
- no water-mesh polygon offset;
- no 3 cm -> 20 cm fake collider-normal separation;
- no independent water-road topology to z-fight with the road; the water pass uses the exact same receiver vertices and equal-depth test;
- authored road/terrain tessellation no longer determines the visible wet/dry boundary.

## Surface-state clipmaps

WATER15A uses two camera-centered world-space clipmaps as a presentation cache of the authoritative hydrology field:

| Level | Nominal texel | Resolution | Square coverage | Refresh target |
|---|---:|---:|---:|---:|
| Near | hydrology support size (normally 0.5 m) | 400 x 400 | ~200 m | 0.10 s |
| Far | 2.0 m | 640 x 640 | 1280 m | 0.50 s |

The far level therefore retains more than 500 m radial presentation coverage. The two levels overlap and blend instead of popping at a ring boundary.

Each texel currently stores up to **two vertical surface layers** as `(waterDepth0, height0, waterDepth1, height1)`. This prevents a bridge and the road underneath from becoming one top-down water field. A 5 cm layer-merge tolerance rejects ordinary curb/sidewalk height steps while still absorbing minor support-plane noise.

This clipmap is a render cache, not a second simulation. Adaptive 0.10 m .. 20 m authoritative cells are rasterized into it; changing render resolution cannot change mass, flow, tire forces, or drainage.

## Wetness / puddle shader

The WATER15B exact-surface water shader receives physical water depth directly.

- Microscopic water first changes visible reflectance/roughness-like response.
- Increasing depth continuously strengthens the optical water body and reflection.
- The optical normal transitions from following support micro-shape toward gravity-up as standing water deepens.
- Hydrology depth gradients and small world-anchored waves perturb the water normal.
- Environment-cubemap reflection is supported immediately; SSR is a later graphics-quality extension rather than a physics dependency.
- Catmull-Rom reconstruction removes rectangular texel ownership in the displayed water field.

The existing `Water_ShorelineBreakup_A8.png` asset remains active, but only as microscopic coverage breakup near the wet/dry boundary. It is explicitly forbidden from creating or deleting authoritative water mass.

## Curbs, sidewalks, bridges, tunnels

A curb is no longer a presentation tessellation problem. Hydrology still uses the collision surface and its connectivity/height discontinuities to decide where physical water can flow. The renderer simply shades the already-visible road/curb/sidewalk receiver at its own depth.

Likewise, stacked surfaces are represented as separate vertical layers in the clipmap instead of requiring one top-down height map. This is why WATER15 does not require the artist to reserve UV2/UV3/UV4. The cache is generated by Heritage Engine from world-space collision/hydrology state.

## What WATER15B preserves from existing physics

WATER15B deliberately does not throw away the working physical systems while replacing the failed presentation topology:

- physical rainfall accumulation;
- conserved adaptive water volume;
- unequal-cell persistent virtual-pipe transport;
- infiltration;
- authored drainage capacity;
- evaporation;
- tire-contact water clearing / redistribution / spray volume;
- tire wet-surface and hydroplaning inputs;
- nearest-interest-source hydrology cadence policy (30 / 20 / 6 / 2 / 0.5 Hz);
- multiplayer/split-screen rule that cadence uses the nearest actual simulation-interest source rather than an averaged midpoint.

## Public-parity work still required

WATER15B is the presentation architecture break, not a claim that every public iRacing Dynamic Track behavior is already complete. The remaining parity work must be implemented explicitly and validated rather than hidden behind visual tricks:

1. **Persistent moisture/saturation state distinct from standing-water depth.** Heritage currently derives much wetness response from hydrology water depth; Dynamic Track publicly distinguishes water and moisture levels. This needs an explicit long-lived surface moisture state so a tire-cleared or evaporated surface can remain damp and form a physically meaningful drying line.
2. **Vehicle aerodynamic water movement.** Tire displacement already moves water; a bounded vehicle wake must also redistribute/lift surface water without CFD-scale cost.
3. **Track-specific drain authoring.** Heritage has per-surface drainage capacity, but iRacing publicly describes drainage systems mapped to real-world locations. Heritage needs first-class drain/outlet entities/metadata rather than relying only on material capacity.
4. **Surface polish / asperity coupling.** Heritage track-rubber state already changes dry/wet grip, but road polish/asperity must become an explicit track-state input so wet grip and retained water differ between polished racing line and rougher pavement.
5. **Wet visual parity extensions.** Add SSR/refraction quality tiers and improve spray/drying-line coupling while keeping the same physical surface-state authority.

These are the next WATER15 milestones; none should reintroduce a renderer-owned settled-water mesh.

## WATER15C — integrated material water (2026-08-17)

WATER15B proved that the simulation could remain authoritative while graphics
used the authored surface, but its atlas update was accidentally gated by a
second wet-film shader. A runtime GLSL compile failure (`packed` is a reserved
GLSL token) therefore left the presentation atlas permanently at zero even while
~99% of hydrology cells were physically wet.

WATER15C removes that dependency and completes the intended presentation split:
the normal PBR road/terrain material shader samples the layered near/far
hydrology clipmaps directly. `SurfaceWetnessReceiver` is a per-entity render
contract. Dry fragments remain their normal material; wet fragments darken and
smooth; standing-water depths add environment reflection and ripple normal
perturbation. The shoreline breakup texture perturbs only visual coverage near
the microscopic wet/dry threshold.

There is no generated settled-water mesh and no duplicate scene-water draw.
The hydrology grid remains a physical state representation only; its topology is
not presentation topology.


## WATER15D — transmissive shallow-water optics (2026-08-17)

WATER15C made the hydrology visible through the ordinary scene material, but
the first optical model mixed accumulated water toward an opaque grey/blue body
colour. At distance this could resemble a patch of wet cement instead of clear
water over the authored asphalt/terrain.

WATER15D keeps the exact same authoritative hydrology and atlas architecture but
changes only presentation optics:

- millimetre- and centimetre-scale standing water keeps the underlying authored
  material visible;
- wet pavement lowers the normal PBR roughness instead of relying on a painted
  grey overlay;
- standing water uses an approximately dielectric Fresnel response (about 2.04%
  reflectance at normal incidence, increasing toward grazing angles);
- substrate transmission uses a weak Beer-Lambert-style attenuation so shallow
  water remains clear;
- only genuinely deep accumulated water receives a subtle volumetric tint;
- the existing environment map remains the reflection source and ripple normals
  perturb that reflection;
- shoreline breakup remains visual-only and cannot alter physical water depth or
  volume.

This remains a single authored surface draw. No alpha-blended water geometry,
duplicate receiver pass, adaptive water mesh, offset shell or renderer-owned
water mass is introduced. "Transparency" is therefore optical transmission of
the already-computed substrate colour through the water layer, avoiding the
sorting/depth problems of a separate transparent mesh.

## WATER15E — four-level high-resolution player clipmaps (2026-08-17)

WATER15E replaces the original 400/640 near/far presentation cache with the
requested four-level player-centred hierarchy. The resolutions are stored as
explicit mip levels so the renderer consumes only two material texture units
(top surface and next lower vertical surface) regardless of clipmap count:

| Level | Texture resolution | World square | Half-extent / nominal range | Texel pitch |
|---|---:|---:|---:|---:|
| LOD0 | 4096 x 4096 | 100 x 100 m | 50 m | 2.441 cm |
| LOD1 | 2048 x 2048 | 200 x 200 m | 100 m | 9.766 cm |
| LOD2 | 1024 x 1024 | 400 x 400 m | 200 m | 39.063 cm |
| LOD3 | 512 x 512 | 2000 x 2000 m | 1000 m | 3.906 m |

LOD3 is additionally clipped to a strict 1000 m radius in the material shader,
so the square storage does not extend visible hydrology beyond the requested
maximum presentation range. Adjacent levels overlap and cross-fade near their
clipmap borders to prevent recenter pops.

A naive CPU implementation would require rewriting a 4096 x 4096 RGBA32F atlas
and would make the requested near-field resolution unnecessarily expensive.
WATER15E therefore changes atlas construction as well as its dimensions:

- the four resolutions are one 4096 -> 2048 -> 1024 -> 512 mip hierarchy;
- each vertical surface layer is an `RG16F` mip chain (`waterDepth`,
  `surfaceHeightOffset`), so two vertical layers cost roughly half the storage
  and bandwidth of the former RGBA32F representation;
- authoritative adaptive hydrology cells are uploaded as compact point records
  and expanded to their collision-surface patches by a GPU geometry shader;
- a first offscreen pass keeps the highest surface at each texel; a second pass
  keeps the highest genuinely separate surface below it, preserving bridge /
  tunnel / stacked-road cases with the existing 5 cm layer-separation rule;
- the ordinary road/terrain PBR shader remains the only visible water draw.
  The atlas rasterizer is an offscreen data-cache build, not visible water
  geometry and does not own water mass or presentation depth.

The clipmap world extents deliberately align with the existing simulation
interest bands at 50 m, 100 m and 200 m before the 1000 m outer range. Atlas
refresh cadence is independent from authoritative hydrology cadence; the cache
may refresh more slowly than physics without changing physical water.

WATER15E materially increases *presentation sampling* precision near the player,
but it does not invent physical information finer than the authoritative
hydrology state. The next precision layer remains the planned engine-generated
high-resolution collision elevation/normal field plus hydraulic-head
reconstruction, which can make sub-cell puddle edges respond to tiny crowns,
gutters and depressions without shrinking every simulation control volume.

## WATER15F — hydraulic-head reconstruction (2026-08-17)

Live WATER15E testing exposed the difference between *texture resolution* and
*state resolution*: rasterizing an adaptive cell's water depth into a 4096² map
merely produced a high-resolution image of the solver cells. WATER15F changes
the meaning of the presentation cache rather than hiding those cells with blur.

Each vertical atlas layer now stores `(waterSurfaceHead, supportHeight)` in
`RG32F`. The authored scene fragment supplies its exact current surface height,
so visible depth is reconstructed as `max(head - fragmentHeight, 0)`. A road
puddle therefore stops naturally at a raised curb unless the physical head is
actually high enough to overtop it.

The hydrology visual gather computes presentation-only free-surface values at
cell corners. Thin films retain conformal surface coverage; accumulated standing
water transitions toward a hydraulic free surface. Neighbouring cells only
participate in corner reconstruction when their support surfaces agree within
2 cm. The material shader performs a second support-height bilateral filter on
its four texel samples, preventing road/sidewalk/bridge layers from bleeding
across ordinary texture interpolation.

The requested four-level hierarchy is unchanged. The new precision is used for
meaningful height reconstruction rather than direct depth magnification.
Standing-water normals are derived from the reconstructed head gradient, and
low-angle environment-only reflection is deliberately bounded until a proper SSR
path can provide local scene reflections.
