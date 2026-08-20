# PERF16 — Connected Water Surface v1

## Goal

Replace visible independent hydrology/adaptive water cards with a connected presentation surface while preserving the authoritative 0.5 m hydrology solver, distance-adaptive cadence, contour-aware adaptive source leaves, and the PERF13 world-space water material.

## Architecture

Hydrology cells remain authoritative for water mass, depth and flow. `collectVisualCellsBand()` still produces adaptive 0.5/1/2/4/8/16 m source leaves so flat/uniform regions remain cheap. PERF16 no longer uploads those leaves as instanced quads.

For every cadence ring, `SurfacePresentationRenderer` now reconstructs a conforming indexed surface:

1. Source leaves are projected onto the authoritative 0.5 m X/Z lattice.
2. Their vertical hydrology layer is retained so road/bridge/tunnel surfaces at the same X/Z can never be stitched together.
3. All source-leaf corners are registered.
4. A coarse edge is split wherever a finer neighbour contributes a corner on that edge. This removes adaptive T-junctions.
5. Vertices on matching X/Z/layer boundaries are shared by every incident triangle.
6. Shared vertices average collision-support height, normal, water depth, flow velocity and hard-surface optical weight. Finer leaves receive more authority at coarse/fine boundaries.
7. Each adaptive leaf triangulates to a local centre and its stitched boundary. The centre is presentation-local; its boundary vertices are globally shared within that cadence ring.

The result is one connected indexed mesh per cadence ring rather than a set of overlapping alpha cards.

## Height and hydraulic interpolation

Visible surface elevation is reconstructed from:

`ground support elevation + simulated water depth`

with only a 1.5 mm minimum anti-z-fight lift for microscopic film. This replaces the historical fixed 6 mm card lift. Where hydrology has reached a common hydraulic head in a depression, neighbouring reconstructed vertices therefore tend toward the same free-surface elevation even though the ground beneath them differs.

Shared-vertex depth and flow are continuously interpolated by the rasterizer across triangles. The water fragment shader still adds render-rate world-space procedural ripples and rain impacts, so the low-frequency physical flow field does not need dense visual triangles merely to hide interpolation.

## Shorelines

A shared vertex tracks which of the four surrounding X/Z quadrants contain explicit wet geometry. If all four are covered it is an interior water vertex. If not, it is a real presentation boundary and its depth is tapered toward the terrain before upload. This softens the visual shoreline instead of terminating full-depth water at a square hydrology-cell edge.

This is intentionally a conservative first boundary reconstruction. A future marching-squares contour pass can move the shoreline itself between cell centres; PERF16 first removes the more damaging disconnected-card topology.

## Steep runoff

PERF15 remains in force. Hydrology can simulate runoff on steep collision faces, but explicit transparent water geometry is rejected above 55 degrees from horizontal. Those surfaces remain candidates for wet-material/runoff shading rather than free-standing puddle sheets.

## LOD and cadence

The existing policy is unchanged:

- 0–25 m: 30 Hz hydrology/presentation source cadence
- 25–50 m: 20 Hz
- 50–100 m: 6 Hz
- 100–200 m: 2 Hz
- beyond 200 m: 0.5 Hz hydrology persistence, no explicit water geometry

Adaptive source leaves remain 0.5/1/2/4/8/16 m depending on contour/depth/flow error. PERF16 stitches those leaves; it does not disable the adaptive savings.

## Shader interpolation

The vertex shader now consumes explicit connected mesh vertices. Per-vertex normal, water depth, X/Z flow and hard-surface optical response are interpolated continuously by ordinary triangle rasterization. Procedural detail uses world-stable X/Z coordinates, so ripple phase does not restart at triangle, source-leaf or cadence-ring boundaries.

## Diagnostics

F8 now distinguishes:

- adaptive source patch count,
- connected GPU vertex count,
- connected GPU triangle count,
- existing 1/2/4/8/16 m source-leaf distribution,
- water collect/pack/upload/draw timings.

## Known next visual refinements

PERF16 is deliberately the topology pivot, not the final water art pass. Live testing should determine whether the next priority is:

- marching-squares/sub-cell shoreline extraction,
- one-ring/C1 flow smoothing for very large triangles,
- connected-puddle free-surface relaxation for deeper pools,
- screen-space adaptive triangulation,
- or fine/broad ripple normal textures.
