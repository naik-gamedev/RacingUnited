# ADR-096 - Adaptive cadence-aligned water presentation

**Status:** Accepted for PERF11.

## Context

The authoritative hydrology grid must retain 0.5 m spatial detail for pooling, drainage, tire interaction and persistent weather state, but explicit rendering does not need one quad per physical cell at every distance. Profiling also showed that a monolithic presentation cache caused distant water to be rescanned and reuploaded at the near-field cadence.

## Decision

Water presentation is split into persistent cadence-aligned rings corresponding to 30 Hz (0–25 m), 20 Hz (25–50 m), 6 Hz (50–100 m) and 2 Hz (100–200 m) hydrology regions. Explicit water is not rendered beyond 200 m, although authoritative hydrology continues at 0.5 Hz background persistence.

The first 100 m retains 0.5 m presentation records. The 100–200 m region starts from 2 m blocks and may recursively merge to 4, 8 and 16 m only when a conservative error test confirms complete topology, material compatibility, near-planarity, similar normals, sufficiently uniform depth and sufficiently uniform flow. Failed candidates retain finer source cells.

Fine/coarse records overlap around 100 m and the water shader computes cross-fade weight from current camera-relative distance. Cadence-ring boundaries are also blended. Presentation refresh phases are offset so low-frequency rings do not intentionally synchronize into one CPU spike.

## Consequences

- Flat, uniformly wet parking lots can collapse to very few large water patches.
- Puddle shorelines, curbs, drains, mixed surfaces and complex water gradients preserve local detail.
- Distant water collection/upload work is no longer performed at 30 Hz solely because near water advanced.
- Four water rings may require several very cheap instanced draw calls instead of one monolithic call; this trades a small fixed submission cost for much lower and better distributed CPU collection/upload work.
- Physics resolution, tire interaction and multi-source hydrology authority are unchanged.
