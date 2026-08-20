# DSURF04G — 10 m / 512 Fixed-Resolution Dynamic Surface

## Current boundary

DSURF04G supersedes the DSURF04F9/F10 100 m simulation-resolution LOD layout. Water simulation resolution no longer changes with camera distance. Every resident Water tile uses the same physical grid and the road/terrain material samples that exact authority.

## World and WaterState layout

- World/surface-sheet ownership unit: **10 m x 10 m**.
- Water authority per resident tile: **512 x 512 R32UI**.
- Physical spacing: **10 / 512 = 0.01953125 m = 1.953125 cm/cell**.
- WaterState packs nonlinear water depth, signed X/Z velocity and N/E/S/W hard-edge topology into one 32-bit word.
- A 64 x 48 atlas provides 3072 resident 10 m tile slots. Slot ownership is world-addressed and survives camera-tile changes.
- Snow uses the same tile address space as R16UI and Mud as R8UI, but both remain lazy allocated so a normal wet asphalt scene does not pay their full VRAM cost.

The atlas is an allocation strategy, not a different simulation grid. Every resident tile is always 512 x 512.

## Simulation cadence

Distance changes scheduling only:

| Distance | Physical grid | Target cadence |
|---|---:|---:|
| 0–100 m | 512² / 10 m | 2 Hz |
| 100–150 m | 512² / 10 m | 1.5 Hz |
| 150–220 m | 512² / 10 m | 1 Hz |
| 220–300 m | 512² / 10 m | 0.5 Hz |

Cadence is recalculated as interest sources move. A tile never changes its physical cell size merely because it crosses a distance boundary. Tile phases are staggered and a bounded per-frame scheduler exposes due/backlog counts on F8.

## High-speed invisible prewarm

The 300 m limit is the **visual Dynamic Surface horizon**, not a hard simulation wake-up wall. The authority adds a narrow forward prewarm corridor based on motion: approximately five seconds of travel, capped at 600 m beyond the normal 300 m simulation radius and about 15 m half-width. Prewarm state is simulated but not made visible solely because it exists.

This is specifically intended to prevent a very fast vehicle from reaching an unsimulated wet road before the environmental state is ready. The ordinary 300 m region remains simulated around the interest source even when outside the current camera frustum, so rapidly rotating the camera reveals already-live water rather than waking it on view.

## Exact geometry and curb topology

A 1.953 cm water grid is not relied upon to solve curbs by resolution alone. Every authority cell evaluates exact authored collision triangles at its own world X/Z. Geometry bins are acceleration metadata only.

For every WaterState cell the solver derives N/E/S/W hard-edge flags. A face becomes a hydraulic barrier when the neighboring support is invalid, belongs to another surface sheet or contains a significant support discontinuity. The four flags are stored in the same R32UI WaterState word as the water state.

The finite-volume solver rejects flow through a hard edge until the free surface physically overtops the higher support. The material shader reads the same barrier bits and refuses to presentation-filter across them. Therefore smoothing inside a puddle must not smear road water up a sidewalk or across a wall/bridge sheet.

## Water transport

Water remains the hydrostatic-reconstruction Rusanov finite-volume solver introduced by DSURF04F9:

- conserved depth and X/Z momentum;
- face-based finite-volume flux;
- positivity/donor limiting;
- authored roughness/friction;
- rain, infiltration, drainage and evaporation source terms;
- high-rate tire contacts merged into bounded GPU disturbance events.

The CPU 64² Hydro path remains only startup/regression fallback and stops advancing after the GPU WaterState authority is ready.

## Rendering

Rendering remains derived exclusively from the **same 512² R32UI WaterState authority**. There is no 8192/4096/1024 simulation-resolution stack and no second pretty-water simulation.

DSURF04G2 adds a presentation-only **R8 cache at the same 512² tile resolution**. Whenever a WaterState tile publishes, a GPU compute pass decodes the exact R32UI authority and performs a 3x3 Gaussian reconstruction. Every contribution must have an open path through the WaterState N/E/S/W curb/surface-sheet barrier bits. The R8 cache is then sampled with fixed-function `GL_LINEAR` filtering.

Near a curb/sheet discontinuity or a 10 m atlas-tile border, the material shader rejects the hardware-filtered cache and falls back to the exact barrier-aware R32UI lookup. This keeps puddle interiors smooth while preserving a razor-sharp sidewalk edge. The cache is never read by hydrology compute and cannot become physical authority. Optical detail still falls with distance and water contribution fades between roughly 250 and 300 m.

## Memory boundary

A 512² R32UI tile is 1 MiB. The 3072-slot Water atlas is therefore approximately 3 GiB plus a single 1 MiB scratch tile. DSURF04G2 additionally allocates one R8 presentation atlas over the same slots (approximately 768 MiB) so final material sampling can use hardware linear filtering without touching the packed integer authority. This is intentionally aggressive and F8 reports actual allocation and GPU timing. Snow/Mud are lazy so their possible full-atlas costs are not paid unless those phenomena become active. If this budget proves too high on target hardware, residency/page-pool changes must preserve the fixed 512² physical resolution; falling back to simulation-resolution LOD is not the preferred answer.


## DSURF04G3: tile-invisible world-space presentation

The 10 m partition is an ownership/storage detail, not a visible rendering boundary. The R8 presentation cache continues to be generated from the exact 512² R32UI WaterState with barrier-aware 3x3 filtering, but final material reconstruction now treats the atlas as physical storage only. Within tile interiors fixed-function `GL_LINEAR` remains the cheap path. Within a 16-cell (~31.25 cm) seam band, the shader resolves samples through the logical world-tile indirection table, so interpolation crosses into the actual neighbouring 10 m tile rather than the physically adjacent atlas slot. A symmetric world-space bridge removes cadence/publication discontinuities across otherwise open tile boundaries.

The same WaterState N/E/S/W barrier bits veto seam blending at curbs, walls and surface-sheet discontinuities. Therefore ordinary asphalt tile seams are mathematically blended away, while a real curb remains a hard presentation and flow boundary. This is presentation-only; no filtered value feeds WaterState physics.
