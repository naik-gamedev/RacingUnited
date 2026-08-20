# DSURF01 - Heritage Dynamic Surface Static Scene Bake

Status: implemented foundation milestone
Date: 2026-08-17

## Purpose

DSURF01 converts the authoritative static collision scene into persistent Heritage Dynamic Surface spatial metadata. This is the first live step after DSURF00's world/chunk/page contracts.

The bake is independent of camera position and floating-origin rebases. It does not allocate the logical 4096 x 4096 dynamic textures yet; DSURF02 provides the persistent GPU page pool.

## Bake pipeline

1. Accept upward-facing wettable collision triangles (`normal.y >= 0.15`).
2. Convert their current local FP32 vertices to global FP64 using `SurfaceWorld::globalOrigin()`.
3. Clip triangles exactly against persistent 100 m x 100 m world chunk boundaries. A source triangle crossing a chunk boundary becomes local patch triangles in each touched chunk instead of being owned by an arbitrary camera region.
4. Build connected surface sheets from shared **3D** manifold edges. Connectivity is geometric rather than X/Z-only, so a bridge and the road below remain independent even when they overlap perfectly in plan view.
5. Preserve material, authored wetness, infiltration/permeability, drainage capacity, flow roughness, depression storage and authored static surface temperature on every patch.
6. Generate stable per-patch and per-sheet microtopography seeds from immutable scene/chunk identity.
7. Classify open/step/crease sheet boundaries as hard static barrier segments.
8. Match artificial 100 m chunk cuts in full 3D and replace them with explicit cross-chunk surface-sheet links. Unmatched chunk cuts become world/scene boundaries.
9. Generate deterministic drain-region records wherever collision metadata has non-zero engineered drainage capacity.
10. Cache the complete immutable bake as `.hdsurf` under the engine settings `DynamicSurface` cache directory.

## Important topology rules

### Bridge / road overlap

Surface connectivity is never inferred from X/Z alone. If two surfaces are five metres apart vertically they receive separate sheet IDs and later independent dynamic texture pages.

### Curbs / steps

A 15 cm road-to-sidewalk step is not joined merely because the two top surfaces meet in X/Z. The rejected vertical curb face breaks the upward-facing manifold, leaving a hard transport boundary. DSURF03 can later implement overtopping explicitly from physical water head rather than allowing texture bleed.

### Chunk seams

The 100 m chunk grid is storage only. A road crossing a chunk boundary receives a `StaticSurfaceSheetLink`, not a wall. Chunk boundaries therefore cannot become hydrology barriers or visible texture seams.

## Cache contents

The deterministic DSURF01 cache stores only immutable scene-derived metadata:

- chunk addresses;
- clipped surface patch triangles;
- connected sheet IDs and bounds;
- normals and material masks;
- hydrology/permeability/drain metadata;
- hard barrier segments;
- cross-chunk sheet links;
- deterministic microtopography seeds.

No rain water, rubber, marbles, dirt, mud or temperature evolution is persisted by this bake. Those are dynamic page state introduced/migrated in DSURF02+.

## Regression protection

The native regression now verifies:

- a road crossing x=100 m becomes two chunk-local sheets linked across the chunk seam;
- a bridge exactly above a road remains a separate surface sheet;
- a 15 cm sidewalk step remains disconnected and owns a hard boundary;
- authored drainage produces static drain metadata;
- a second load restores the same topology and microtopography seed from `.hdsurf` cache.
