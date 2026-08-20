# DSURF04E — GPU LOD Stress Gate

## Status

Live performance candidate only. DSURF04D2 remains the user-visible and tire/SurfaceWorld water authority during this milestone.

DSURF04E exists to answer one question with the user's real GTX 1660 Ti: **can the proposed centimetre-scale GPU Dynamic Surface LOD envelope be scheduled without unacceptable frame time or VRAM pressure?** Authority cutover is intentionally deferred until that hardware gate is measured.

## Exact workload under test

World addressing remains based on 100 m × 100 m surface tiles. The proposed camera-distance envelope is represented by three rings:

| LOD | Resolution per 100 m tile | Approx. cell size | Ring layers | Publication cadence |
|---|---:|---:|---:|---:|
| LOD0 | 8192 × 8192 | 1.2207 cm | 1 | 2 Hz |
| LOD1 | 4096 × 4096 | 2.4414 cm | 8 | 1 Hz |
| LOD2 | 1024 × 1024 | 9.7656 cm | 16 | 0.5 Hz |
| beyond | none | — | 0 | 0 Hz |

The 1/8/16 layers model the current 100 m tile, its first neighbour ring and the next outer ring. No DSURF04E compute work is provisioned beyond the 300 m design horizon.

## Formats under test

- `WaterState`: `R32UI`, packed as 16-bit depth + signed 8-bit X velocity + signed 8-bit Z velocity.
- `SnowState`: `R16UI`; production packing target is snow depth + compaction.
- `MudState`: `R8UI`; production target is persistent rut/depression depth.

All three use ping-pong storage because neighbourhood simulation must read a coherent old state while writing a new state.

The full requested stack is approximately 2912 MiB if every allocation succeeds. Water alone is approximately 1664 MiB. Snow and Mud allocations are allowed to degrade independently if the GPU cannot reserve the whole worst-case stack; the renderer must still launch and F8 must report which states were actually allocated.

This number is the **dynamic-state envelope only**. Future static support/topography metadata is intentionally excluded from this first gate because its exact format/resolution has not yet been accepted; the Water kernel uses representative neighbour-state reads rather than claiming final terrain-coupled flow.

## Scheduling

A publication cadence does **not** mean one enormous dispatch at 2/1/0.5 Hz.

Each texture layer is split into 128-row work units. Work units are phase-distributed across render frames so the average amount of compute remains approximately constant:

- LOD0: 64 work units per publication × 2 Hz.
- LOD1: 32 work units/layer × 8 layers × 1 Hz.
- LOD2: 8 work units/layer × 16 layers × 0.5 Hz.

A whole destination state is published only after all work units for that LOD have been written. The source texture remains unchanged during that cycle, giving deterministic Jacobi-style neighbour reads and avoiding in-place race/checkerboard artefacts.

Catch-up is bounded so a debugger pause or hitch cannot dump an arbitrarily large compute backlog into one frame.

## Compute content

The Water stress shader performs representative production-class operations:

- unpack 16-bit depth + 8/8-bit velocity;
- four neighbour reads;
- local depth gradient;
- bounded velocity evolution;
- rainfall accumulation;
- local redistribution;
- repack and store.

This is **not yet claimed as the final shallow-water solver**. It is deliberately representative enough to expose memory bandwidth/ALU cost before the actual GPU authority solver is accepted.

Snow and Mud run lighter update kernels because their eventual physics is not a fluid solve.

## F8 acceptance telemetry

F8 reports:

- committed DSURF04E MiB;
- Water/Snow/Mud allocation success independently;
- CPU dispatch time;
- asynchronous GPU compute time (no `glFinish`);
- dispatch count and cells processed this frame;
- LOD0/1/2 Water dispatch counts;
- published-cycle count.

The first live gate is purely empirical. If the user's 1660 Ti handles the workload comfortably, the next milestone promotes these resources to the actual Dynamic Surface authority/render source and retires the old 64×64 water authority. If it does not, resolution/ring width/state packing is adjusted before destructive migration.
