# ADR-137 — Persistent Sensor Dynamic Surface Instead of 10 m / 512 CFD

## Status

Accepted for LIVETRACK01.

## Context

The DSURF04G experiment made every resident 10 m water tile a 512 x 512 R32UI finite-volume simulation. At 64 x 48 atlas slots that alone implied a 3.0 GiB water-state texture plus a 0.75 GiB presentation texture, with substantial per-cell compute and geometry work. The result was inappropriate for the performance target of a racing game and still did not provide robust whole-session track memory through the live GPU-residency path.

The engine already contains a persistent 100 m / 64 x 64 Dynamic Surface Hydro authority that supports authored support geometry, precipitation exposure, drainage, evaporation, shallow-sheet transport, moisture, tire water clearing and tire-physics sampling.

## Decision

Promote the persistent Dynamic Surface Hydro sensor field back to the sole live water authority.

- 100 m page / 64 x 64 sensor resolution.
- 350 m bounded active radius around real simulation-interest sources.
- 2 Hz atmospheric forcing.
- Hydraulic transport subdivided to <= 0.05 s.
- Tire contacts read/write the same Hydro state sampled for grip and hydroplaning.
- Dormant page state is retained for session memory.
- GPU page pool is presentation mirror only.
- The 10 m / 512 GPU CFD prototype is runtime-retired and must not allocate or dispatch.

## Consequences

Positive:

- Removes multi-gigabyte water atlases and high-resolution CFD dispatches.
- Puddle/dry-line memory and tire physics use one authority.
- Runtime work scales with nearby track surface rather than every centimetre-scale cell in a 300 m disk.
- Existing renderer interpolation can hide sensor resolution without inventing simulation state.
- Architecture is simpler and easier to debug.

Tradeoffs:

- Fluid topology is sensor-scale, not centimetre-scale CFD.
- Very narrow drainage channels below the sensor scale are represented through authored drainage/depression parameters and visual reconstruction rather than direct fluid cells.
- Dormant state is currently session-memory state rather than serialized save data.

## Reference philosophy

The design follows the general dynamic-track/sensor-field philosophy publicly described for contemporary racing simulations; no proprietary LiveTrack implementation or code is copied.
