# ADR-136 — 10 m / 512 Fixed-Resolution Dynamic Surface

**Status:** Accepted candidate for live validation

## Context

The DSURF04F 100 m layout changed physical WaterState resolution with distance (8192²/4096²/1024²). Live testing showed that resolution migration and coarse/fine presentation made authority behavior difficult to reason about, while curb/sidewalk correctness must remain sharp even when presentation is filtered. High-speed free-roam driving also requires water to be ready before it enters the visible 300 m range.

## Decision

1. Partition live high-resolution Dynamic Surface ownership into 10 m x 10 m world/surface-sheet tiles.
2. Every resident Water tile uses one fixed 512 x 512 R32UI authority (~1.953 cm/cell). Simulation resolution never changes with distance.
3. Distance controls cadence only: 2 Hz to 100 m, 1.5 Hz to 150 m, 1 Hz to 220 m, 0.5 Hz to 300 m.
4. Keep simulation alive independent of the camera frustum. Rendering is naturally limited to drawn geometry and its Dynamic Surface contribution fades toward 300 m.
5. Add a motion-predicted invisible prewarm corridor beyond 300 m, approximately five seconds of travel and capped at 600 m extra, so high-speed vehicles do not outrun environmental state.
6. Evaluate exact authored collision triangles at every 512² authority cell. Acceleration bins are lookup metadata only.
7. Pack N/E/S/W hydraulic barrier bits in the same WaterState word used by physics and rendering. Sheet discontinuities/curbs/walls block finite-volume face flux until physical overtopping, and presentation filtering must reject the same edges.
8. Keep the finite-volume WaterState as the single live water authority. The material shader samples that same atlas. CPU 64² Hydro/support remains startup/regression fallback only.
9. Snow R16UI and Mud R8UI share the 10 m tile coordinate system but allocate lazily.

## Consequences

- Approaching a tile no longer refines or invents its physical water state.
- Rapid camera rotation shows already-simulated tiles; view direction affects rendering, not hydrology ownership.
- Curb fidelity is a topology rule rather than merely a texture-resolution gamble.
- The Water atlas can consume roughly 3 GiB at the current 3072-slot stress configuration. F8 telemetry is mandatory before deciding whether residency should be made more sparse.
- Presentation filtering remains allowed, but it is derived from the same WaterState and must preserve barrier topology.
- Large-world dormant weather memory remains a follow-up; prewarm currently seeds newly resident tiles from the bounded background weather accumulator rather than a final per-tile environmental ledger.
