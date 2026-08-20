# DSURF03B — Heritage Dynamic Surface Hydro Authority

## Milestone

DSURF03B completes the water-state authority cutover begun in DSURF03A. Runtime water, moisture and horizontal flow now belong to **Heritage Dynamic Surface** persistent surface pages rather than the legacy `SurfaceHydrology` state store.

This milestone is intentionally an authority migration, not the final optical-water milestone. The renderer still has later work scheduled under DSURF08 for mature organic puddle presentation, reflections and ripple quality.

## Persistent authority

Dynamic hydro state is keyed by `VirtualPageAddress`, therefore by the DSURF01 world chunk, surface sheet and page. It is not keyed to the camera, a camera clipmap, a puddle stamp, an adaptive solver rectangle or generated water geometry.

The static hierarchy remains:

- 100 m × 100 m FP64 world chunks;
- logical 4096 × 4096 surface domain per sheet (~2.44 cm logical texel pitch);
- 256 × 256 sparse physical pages, each covering 6.25 m × 6.25 m;
- independent surface-sheet identity for overlapping road/bridge/tunnel/sidewalk geometry.

The first DSURF03B physical hydro authority uses **16 × 16 controls per 6.25 m page**, or approximately **39.0625 cm** between control samples. This is deliberately conservative for the authority cutover. It must not be confused with the 2.44 cm logical page address space or with final visual detail.

Each authority control stores:

- water depth;
- persistent surface moisture;
- horizontal water velocity X;
- horizontal water velocity Z.

Its static support stores the exact DSURF01 sheet elevation plus infiltration, drainage, depression-storage and flow-roughness metadata.

## Runtime physics now owned here

`DynamicSurfaceHydrology` now advances:

- precipitation accumulation;
- surface infiltration;
- material and mapped drainage;
- evaporation;
- moisture accumulation/drying;
- conservative four-neighbour shallow-sheet transport using hydraulic head and a Manning-style discharge relation;
- cross-page transport across valid DSURF01 sheet links;
- tire water removal/clearing;
- forward redistribution;
- friction evaporation and spray accounting.

The update cadence follows the nearest real simulation-interest source:

- 0–50 m: 30 Hz;
- 50–100 m: 20 Hz;
- 100–200 m: 6 Hz;
- 200–1000 m: 2 Hz;
- beyond 1000 m: no active step in this milestone.

There is no midpoint source between local players.

## Shelter and stacked surfaces

Precipitation exposure is derived from the DSURF01 static surface sheets. When another sheet is present sufficiently above a support sample, the lower sheet is sheltered. A bridge therefore receives rain while the road directly underneath can remain dry. The two surfaces never become the same hydro page merely because they overlap in world X/Z.

The same sheet identity is used by local queries and renderer support-height matching so a road puddle cannot be reinterpreted as sidewalk or bridge water.

## Presentation bridge

The renderer no longer samples legacy hydrology cells. For a resident Dynamic Surface page it requests a persistent hydro raster from the new authority.

The current presentation upload is 64 × 64 samples per 6.25 m page (mip 2), approximately 9.77 cm per uploaded sample. State is interpolated from the authority controls while support elevation is reconstructed from the exact DSURF01 authored surface triangles. This prevents the retired adaptive `SurfaceHydrology` cell footprint from being rasterized into the material.

The ordinary authored road/terrain material remains the only visible geometry owner. No water mesh, duplicate water pass, camera puddle atlas, circular splat field or sponge-brush clipmap is reintroduced.

## Legacy `SurfaceHydrology` status

`SurfaceWorld::advancePresentation()` no longer advances legacy `SurfaceHydrology`.

Runtime tire-water interaction and local surface conditions read/write Heritage Dynamic Surface hydro authority. The old object remains temporarily because weather/rain-cover code still uses its static precipitation-cover index and because historical standalone tests/benchmarks still compile against it. Those are compatibility responsibilities, not dynamic water-state authority.

Removing those remaining static compatibility responsibilities is a later cleanup gate; DSURF10 must physically remove superseded storage/code once all consumers have migrated.

## Acceptance gates

DSURF03B requires:

1. Dynamic Surface authority advances under rain without legacy state advancement.
2. A bridge over a lower road receives rain while the sheltered lower sheet stays dry.
3. Tire contact removes water from the Dynamic Surface page authority.
4. Renderer page rasterization returns water/moisture/flow plus exact support data from the same persistent page identity.
5. Build projects track the new authority source in both engine and native regression targets.
6. WATER15–18 presentation architecture remains absent from the live renderer.
7. Capped rainfall and full-receiver flow cannot silently destroy or over-account water mass.

## Next milestones

- **DSURF04:** migrate thermal track state into the Dynamic Surface Track plane.
- **DSURF05:** migrate adhered rubber, loose rubber/marble density and maturity, then delete the old persistent `TrackRubberState` marble storage and old marble GPU-cell cache.
