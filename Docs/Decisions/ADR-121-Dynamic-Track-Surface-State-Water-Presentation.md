# ADR-121 — Dynamic Track Surface-State Water Presentation

## Status

Accepted for WATER15.

## Context

WATER14 proved that using the adaptive hydrology control-volume mesh directly as visible standing-water geometry couples unrelated concerns. Curbs, coarse/fine transitions, seam welding, T-junctions, z-fighting, LOD offsets, and render coverage all became topology problems even when the physical hydrology state itself was valid.

Public iRacing material describes Dynamic Track as water/moisture **surface state**, with puddles, drying, tire/aerodynamic movement, evaporation, drainage, surface-roughness effects, wetness shaders, reflections and ripples. The public material does not disclose iRacing's private implementation details.

## Decision

Heritage Engine will target public Dynamic Track behavior through an independently implemented **surface-state architecture**:

- `SurfaceHydrology` remains authoritative for physical water.
- Settled-water presentation will not own a separately tessellated road/terrain water mesh.
- The normal opaque pass remains authoritative for visible depth.
- WATER15B re-submits only wettable authored scene geometry with the exact same transforms and uses reversed-Z `GL_GEQUAL` so only the already-visible receiver samples shade.
- The WATER15A copied-depth/stencil fullscreen prototype is superseded and removed from the active path after live testing produced no visible water.
- Near/far layered world-space clipmaps cache hydrology for presentation and preserve stacked bridge/tunnel surfaces.
- The shoreline breakup texture is visual-only and cannot alter physical water state.
- Distance/camera LOD may change presentation sampling and refresh frequency, but never authoritative water volume or multiplayer physics.

## Rejected alternatives

### Renderer-owned adaptive water mesh

Rejected. Even with adaptive subdivision, feature-line refinement, 2:1 balancing and seam stitching, the renderer duplicated the collider surface and therefore created avoidable seam/T-junction/z-fighting failure classes.

### Single top-down world height map

Rejected. It aliases bridges, tunnels, multi-storey roads and other vertically stacked surfaces.

### Artist-reserved UV2/UV3/UV4 hydrology map

Rejected as the engine-wide authority. It adds asset-authoring requirements, can collide with existing asset UV usage, and is unnecessary for a world-space runtime cache. Authored UV data may be supported later for optional static masks, but physical hydrology identity belongs to collision/surface topology.

## Consequences

Positive:

- eliminates duplicate-water geometry and its z-fighting;
- makes visible shoreline resolution independent from road polygon size;
- preserves physical/render separation;
- naturally supports huge authored polygons and local curbs without presentation tessellation;
- provides one stable route for wetness, puddles, reflections, ripples and drying-line visuals.

Tradeoffs:

- the exact-surface water pass adds a second submission of visible wettable geometry while water is present;
- layered clipmaps consume GPU memory/bandwidth and require bounded refresh work;
- deep flood/free-surface behavior beyond road-racing puddles may eventually need a separate volumetric/free-surface subsystem, but it must remain a consumer of the same authoritative hydrology state rather than replacing it.

## Provenance rule

Documentation and UI may say "Dynamic Track-style", "public Dynamic Track parity", or "iRacing-documented behavior target". They must not say Heritage uses iRacing's exact private algorithm/source/data layout unless iRacing itself publishes those details under terms that permit implementation.

### WATER15C amendment — integrate water into the ordinary material pass

The WATER15B duplicate exact-geometry pass was an intermediate diagnostic
implementation. WATER15C supersedes it. The authoritative surface-state
clipmaps are now sampled by the existing PBR material program for tagged
wettable scene geometry. This removes the second geometry submission and also
ensures atlas lifetime/update does not depend on a secondary optical shader.


### WATER15D amendment — transmissive optics, not an opaque overlay

WATER15D keeps the WATER15C single-pass authored-surface architecture and
changes only the optical model. Shallow standing water preserves the visible
substrate, lowers material roughness, and layers dielectric Fresnel reflection
over weak depth-dependent transmission. The renderer no longer mixes shallow
puddles toward an opaque water-body colour. Only deep accumulated water gains a
small volumetric tint.

This amendment does not reintroduce alpha-blended water geometry. Transparency
is represented inside the existing material shader by transmitting the
underlying surface lighting through the water state sampled at that fragment.

### WATER15E amendment — four nested high-resolution GPU clipmaps

WATER15E supersedes the original two-level 400/640 cache. Presentation state is
now a four-level player-centred hierarchy: 4096/100 m, 2048/200 m, 1024/400 m,
and 512/2000 m with a strict 1000 m radial outer cap. The levels are stored as
mips of two `RG16F` textures (top surface and next lower vertical surface), so
adding levels does not consume additional material texture units.

The atlas is no longer CPU-rasterized and uploaded texel-by-texel. Authoritative
hydrology control volumes are GPU-rasterized into the offscreen state maps. This
internal rasterization is a cache-generation operation only: visible water still
uses the ordinary authored surface material draw, and no renderer-owned water
mesh, duplicate road pass, normal-offset shell, or water-mass authority returns.

The 50/100/200/1000 m presentation extents intentionally align with the main
simulation-interest distance bands. Visual cache refresh cadence remains
independent from physical hydrology cadence and cannot affect conserved volume,
flow, drainage, tire-water interaction, or multiplayer authority.
