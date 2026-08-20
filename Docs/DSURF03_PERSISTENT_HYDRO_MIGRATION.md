# DSURF03 — Persistent Hydro Migration

## Status
Implemented as the first live-state migration into **Heritage Dynamic Surface**.

DSURF03 removes the WATER15–WATER18 camera-relative puddle/canvas presentation path from the live `EntityMeshRenderer`. The existing conserved `SurfaceHydrology` solver remains the temporary physics authority during this migration milestone, but its water/moisture/flow result is mirrored into persistent Dynamic Surface Hydro pages and the ordinary authored scene material reads those pages directly.

## Non-negotiable architecture

- Dynamic Surface identity is `(100 m chunk, surface sheet, 6.25 m physical page)` and is independent of the camera.
- The logical surface domain remains 4096×4096 samples per 100 m chunk (~2.44 cm logical pitch).
- Physical storage remains sparse 256×256 pages with a conventional mip chain.
- Only pages that DSURF01 proved contain actual collision surface geometry are candidates for Hydro residency.
- Multiple local simulation-interest sources are evaluated independently. No synthetic midpoint between players allocates or updates water.
- The ordinary road/terrain/material draw samples Dynamic Surface state. There is no settled-water mesh, duplicate scene draw, camera-relative puddle atlas, circular splat field, sponge-brush canvas, or presentation quadtree.

## Hydro page contract

`Hydro RGBA16F`

- R — standing-water depth in metres
- G — surface moisture/wetness [0,1]
- B — surface flow velocity X in m/s
- A — surface flow velocity Z in m/s

`Support R32F`

- immutable authored/collider support elevation for the resident surface sheet
- used to distinguish road / bridge / sidewalk layers that overlap in X/Z
- invalid texels use a large sentinel and cannot receive water in the material shader

DSURF03 migrates at physical page mip 2: 64×64 samples per 6.25 m page, approximately 9.77 cm/sample. This is intentionally a staged migration resolution, not the final logical 2.44 cm Dynamic Surface solver resolution.

## Residency and cadence

`DynamicSurfaceSystem::refreshHydroResidency()` gathers only `coveredPages()` from DSURF01 static surface chunks. Candidates are distance-sorted against the nearest *real* simulation-interest source and only the bounded page-pool capacity is made resident.

Hydro uploads obey the existing distance cadence through `requestedUpdateHz()` and a hard renderer migration budget of at most four changed pages per frame. Page generation is tracked so recycled physical slots are always repopulated before use.

## Rendering path

The material shader maps each receiver fragment into a 6.25 m Dynamic Surface page. A small integer indirection texture only resolves the persistent physical page slot; it does not contain water state and therefore cannot own the puddle shape.

For overlapping surfaces, up to four resident sheet slots may exist at one X/Z page coordinate. The shader samples the support-height page and selects the layer whose authored support elevation matches the exact rendered receiver Y.

Water optics continue to use the existing shoreline texture only as sub-grid visual micro-relief. It never creates authoritative water mass and never determines Dynamic Surface residency.

## Temporary bridge to the legacy solver

For DSURF03 only, Hydro page texels are populated by sampling the existing conserved `SurfaceHydrology` authority at the exact DSURF01 support position. This retains rain accumulation, virtual-pipe transport, drains, evaporation and tire clearing while the state representation is migrated safely.

The important boundary is now enforced: the legacy adaptive solver may provide a numerical state sample, but its cell footprint is never rasterized or used as presentation geometry.

Future Dynamic Surface milestones move the authority itself into the persistent surface pages and then delete the legacy storage paths.

## Diagnostics

F8 reports:

- Dynamic Surface resident / capacity / dirty pages
- committed GPU MiB
- page-table generation and uploads
- initialized physical pages and mip work
- Hydro page uploads this frame
- CPU cost of static-support raster, hydrology sampling and Hydro upload

No WATER15–WATER18 camera-atlas counters are retained in `EntityMeshRendererStats`.

## Acceptance checks

- road crossing a 100 m chunk boundary retains explicit surface continuity
- bridge and road below remain independent sheets
- 15 cm curb remains a hard surface-sheet boundary
- Hydro residency uses real covered pages only
- two local interest sources allocate around each player, not between them
- ordinary material shader samples persistent Hydro/support page arrays
- no live renderer reference to WATER18 brush/circle/clipmap collectors
- no generated visible water geometry
