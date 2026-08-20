# ADR-135 — GPU Dynamic Surface Authority Cutover

**Status:** Accepted for DSURF04F live validation

## Context

The DSURF04C/D2 64x64-per-100m CPU Hydro authority was inexpensive but repeatedly exposed its 1.5625 m spatial lattice in standing-water boundaries. Presentation-only warps could improve appearance but preserved a fundamental split between physical water and the state seen by the renderer.

The desired contract is centimetre-scale Water/Snow/Mud state where the exact fields advanced by simulation are also the fields rendered, with bounded camera-distance LOD and zero high-resolution work outside 300 m.

## Decision

Adopt a camera-centred GPU-compute Dynamic Surface authority:

- LOD0: 8192x8192 x1 100m tile at 2 Hz;
- LOD1: 4096x4096 x8 first-ring tiles at 1 Hz;
- LOD2: 1024x1024 x16 second-ring tiles at 0.5 Hz;
- no high-resolution Water/Snow/Mud rendering or compute beyond the 300 m visual horizon.

Use:

- Water `R32UI`: nonlinear 16-bit depth + signed 8-bit X velocity + signed 8-bit Z velocity;
- Snow `R16UI`: nonlinear 12-bit depth + 4-bit compaction;
- Mud `R8UI`: persistent rut/depression state.

Each state uses initialized ping-pong images and frame-staggered 128-row compute work. The renderer samples those published GPU images directly. The CPU 64x64 Hydro solver becomes startup/regression fallback and must not advance after GPU authority activation.

Tire contacts are world-anchored, CPU-aggregated, then applied by bounded GPU footprint compute to Water/Snow/Mud authority. Static Dynamic Surface support/sheet pages remain read-only geometric input.

## Consequences

### Positive

- simulation and rendering share one dynamic authority per phenomenon;
- LOD0 water has ~1.22 cm physical cells rather than a 1.5625 m presentation lattice;
- flow velocity is persistent rather than reconstructed solely for graphics;
- Snow and Mud gain the same high-resolution contact-deformation path;
- bulk cell evolution stays on the GPU;
- frame work is staggered instead of issuing one monolithic cadence spike;
- far optical detail is deliberately reduced/faded to keep LOD transitions inconspicuous.

### Costs / accepted first-cut limitations

- the complete three-state ping-pong envelope is about 2.9 GiB before other GPU resources;
- DSURF04F1 migrates overlapping world tiles across the 8192/4096/1024 rings on a 100m centre-tile rebase; only newly entering tiles start empty;
- immediate CPU tire-force wet-depth queries use weather-film fallback until a tiny asynchronous GPU contact-sample bridge is added;
- static support height is still supplied by the existing sheet-aware Dynamic Surface bake/page system.

These limitations do not justify retaining a second live CPU water solver.

## Supersedes

For live Water/Snow/Mud authority, this supersedes the DSURF04E stress-gate-only decision in ADR-134 and supersedes DSURF04C/D2 as the normal running water authority whenever GPU initialization succeeds. DSURF04D2 remains only a failure/startup fallback.

## DSURF04F8 amendment — exact geometry at authority resolution

The accepted first-cut statement that static support height may be supplied by the legacy 64x64 Dynamic Surface support pages is superseded.

When high-resolution GPU authority is ready, Water/Snow/Mud compute does **not** sample any 64x64 support or hydrology texture. The active 5x5 100m world window uploads the authored static collision triangles plus their hydrology parameters to GPU SSBOs. A 256x256-per-tile bin table contains candidate triangle indices only; it is acceleration metadata, not a sampled surface-state raster. Every 8192/4096/1024 authority invocation evaluates the exact authored triangle plane at that invocation's world X/Z coordinate before performing precipitation, drainage, flow, snow, or mud work.

The renderer continues to sample the exact published WaterState textures evolved by compute. Once this exact-geometry authority is ready, the legacy 64x64 Hydro/support GPU page pool is dormant fallback infrastructure and is neither synchronized nor sampled by live GPU WaterState physics/rendering.

This preserves the single-authority requirement: high-resolution WaterState is both the simulated water field and the rendered water field; no 64x64 surface raster participates in the live high-resolution water solve.


## DSURF04F9 amendment — conservative finite-volume water transport

The live centimetre WaterState keeps its R32UI depth/velocity representation and 8192/4096/1024 LOD ownership, but the prototype neighbour head-exchange transport is superseded. GPU WaterState now advances conserved depth and X/Z momentum through physical cell faces using hydrostatic reconstruction and a Rusanov flux, plus a positivity-preserving donor limiter and semi-implicit roughness friction. Rendering continues to sample the same published WaterState. Camera height is forbidden from selecting the physical receiver; authored persistent surface-sheet identity is used instead.

## DSURF04F10 amendment — all visible LOD tiles advance concurrently

The 300 m authority window is a simultaneously live set, not a queue of world tiles. A compute row work unit therefore spans all texture-array layers in its LOD using the dispatch Z dimension: 1 layer at LOD0, 8 at LOD1 and 16 at LOD2. This preserves 8192²@2 Hz, 4096²@1 Hz and 1024²@0.5 Hz while preventing complete neighboring world tiles from being serialized behind one another. Moving the camera centre changes LOD assignment only; a tile already inside the visible 300 m window must already have accumulated weather and evolved its surface state before promotion to LOD0.


## Supersession

ADR-136 supersedes the 100 m simulation-resolution LOD layout while retaining this ADR's core GPU-authority decision. Water remains GPU authoritative and directly rendered, but resident physics now uses fixed 10 m / 512² tiles at every distance.
