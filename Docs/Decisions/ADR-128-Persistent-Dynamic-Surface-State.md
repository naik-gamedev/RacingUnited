# ADR-128: Persistent Dynamic Surface State

Status: Accepted  
Date: 2026-08-17

## Context

Heritage Engine accumulated separate systems for water/hydrology presentation, track wetness, rubber, loose marbles and surface particles. WATER14-WATER18 demonstrated a recurring failure mode: when adaptive solver cells, clipmap texels or camera-relative splats own visible puddle silhouettes, rectangular/triangular discretization leaks into the image and can flicker as representation LODs refresh.

The project also needs one coherent surface state for tire physics, rain, drainage, temperature, rubber, loose marbles, dirt/mud and vehicle aerodynamic interaction.

## Decision

Adopt a world-owned `DynamicSurfaceSystem` using persistent 100 m x 100 m global chunks and engine-generated surface sheets. Each sheet exposes a logical 4096 x 4096 domain (2.44 cm/texel) backed by engine-managed sparse 256 x 256 physical pages and coarser persistent fallback levels.

Dynamic state is stored in typed GPU-friendly planes rather than one overloaded RGBA8 texture:

- Hydro RGBA16F: water depth/amount, moisture, flow X, flow Z.
- Track RGBA16F: temperature, adhered rubber, loose-rubber/marble mass, marble maturity.
- Contamination initially RGBA8: dirt, mud, loose debris and reserved contamination.

Static scene bake provides surface height/microtopography, normals, materials, drains, barriers and surface-sheet identity.

The ordinary scene material shader samples this persistent state. The visible water shape is derived from physical water state plus high-resolution static surface microtopography and shoreline breakup; solver cells never become visible geometry or visible ownership masks.

Tires and aerodynamics submit bounded events/impulses to Dynamic Surface. GPU compute passes consume those events at distance-adaptive cadence. High-frequency ripples remain procedural rendering driven by wind and water flow.

Persistent loose-rubber/marble state moves into Dynamic Surface. Nearby individual 3D marbles are reconstructed presentation representatives. The old persistent marble-cell state and GPU marble-cell cache are removed after migration.

## Consequences

### Positive

- stable Substance-Painter-like world-anchored dynamic paint;
- no camera-relative puddle shape regeneration;
- normal mipmaps preserve the same puddle shape with distance;
- one authoritative state shared by physics and rendering;
- natural coupling among water, heat, rubber, marbles, dirt and aerodynamics;
- sparse pages prevent full 4096² allocation for every world chunk;
- supports bridge/road overlap through independent surface sheets.

### Costs

- requires static surface-sheet bake and page-table infrastructure;
- migration touches hydrology, track rubber, tire surface interaction, renderer and validation;
- persistence/network synchronization must operate on page state/events;
- legacy WATER14-WATER18 and marble code must be retired carefully in staged builds rather than deleted before replacement paths compile.

## Superseded runtime decisions

ADR-096 through ADR-127 remain historical evidence but no longer define the target water-presentation architecture where they conflict with this ADR. ADR-063 Dynamic Track rubber/marbles is superseded for persistent state ownership; its physical concepts are migrated into Dynamic Surface. Tire-mark ADRs remain active.

