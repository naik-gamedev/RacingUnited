# WATER14 – Adaptive Control-Volume Hydrology

> **WATER15 status:** the adaptive control-volume **hydrology authority remains active**, but the WATER14 renderer-owned adaptive water mesh described historically below is retired. Settled-water presentation now uses the original scene surface through the WATER15 Dynamic Track surface-state resolve. See `Docs/WATER15_DYNAMIC_TRACK_SURFACE_STATE.md`.

## Purpose
WATER14 moves adaptive sizing into the **authoritative hydrology simulation itself**. The old fixed 0.5 m water chessboard no longer owns depth, flow, pipe state, or runtime water updates.

## Static terrain support versus simulated water
Heritage still bakes a 0.5 m terrain-support raster because it is a cheap deterministic way to cache collision elevation, normal, material, rain exposure, roughness, infiltration, drainage, and depression storage. That raster is immutable support data only.

The runtime water state lives in a separate adaptive control-volume topology:

- minimum control-volume size: **0.10 m**
- maximum control-volume size: **20.0 m**
- intermediate sizes are permitted; the topology is not restricted to a distance LOD table
- flat/coherent support may merge into very large cells
- material boundaries, large normal changes, surface-plane error, and sharp support topology refuse merging
- genuinely aggressive angular/crease support may refine to 0.10 m authoritative cells
- curb/sidewalk height steps are directional feature edges: the adjacent solver cell stays 0.50 m unless independent angular criteria demand finer authority
- solver size grades outward from local feature detail approximately 0.50 -> 1 -> 2 -> 4 -> 8 -> 16 -> 20 m while plane-fit/material rules remain authoritative

Water is stored as **volume in cubic metres**, not simply as depth per fixed cell, so changing control-volume area does not change total water mass.

## Variable-face virtual pipes
Each adjacent pair of adaptive cells shares one signed virtual pipe. The pipe stores persistent volumetric flux and knows:

- shared edge width
- distance between cell centres
- hydraulic sill elevation
- signed A-to-B flux

This allows a 0.10 m cell to exchange water directly with a much larger neighbour while retaining conservative source-volume limiting.

## Scheduling
The existing multi-source simulation-interest cadence remains:

- 0–25 m: 30 Hz
- 25–50 m: 20 Hz
- 50–100 m: 6 Hz
- 100–200 m: 2 Hz
- beyond 200 m: 0.5 Hz persistence

The cadence now schedules adaptive simulation cells grouped into deterministic 20 m world chunks.

## Presentation
WATER15 supersedes the WATER14 presentation path. The adaptive control volumes remain authoritative **simulation** cells, but they are rasterized into layered near/far surface-state clipmaps and shaded on the original visible scene geometry. They are no longer converted into cadence-ring water geometry, seam-welded triangles, curb strips, or collider-normal-offset water surfaces.

The following WATER14 presentation experiments are retained only as historical documentation and regression context; none owns settled-water depth in the active renderer.

The retired experimental files are deleted from the project/build:

- `WaterContourMesher.hpp`
- `WaterParcelRenderer.hpp`
- `WaterParcelRenderer.cpp`
- `WaterSurfaceStitcher.hpp`

The build helper also deletes stale copies after ZIP overlays, because archive extraction cannot delete files already present on disk.

## Current adaptation policy
WATER14 chooses simulation topology from **static terrain/support complexity** during bake/cache reconstruction. Runtime rain/flow changes water volume and flux inside that adaptive topology but do not currently trigger continuous split/coarsen topology rebuilds every frame. This is deliberate for the first performance-oriented implementation; a later low-frequency wet-front/tire-interest adaptation pass can be added if profiling shows it is worth the topology-rebuild cost.
