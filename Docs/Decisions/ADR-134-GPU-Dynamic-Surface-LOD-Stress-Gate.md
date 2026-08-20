# ADR-134 — GPU Dynamic Surface LOD Stress Gate Before Authority Cutover

Status: Accepted candidate for live profiling

## Context

The DSURF04D2 64×64 / 100 m CPU authority can be made visually organic, but its coarse physical lattice does not contain enough centimetre-scale state to guarantee that simulation and rendering are literally the same field. The proposed replacement is a GPU-owned field with a maximum 8192² resolution per 100 m tile and distance-reduced 4096² / 1024² LODs.

The target development GPU is a GTX 1660 Ti 6 GB. A full Water/Snow/Mud ping-pong stack at the proposed ring counts is large enough that committing the project to it without live timing would be irresponsible.

## Decision

Implement the exact proposed texture sizes, formats, ring counts and cadences first as a **shadow compute workload** while retaining DSURF04D2 as the visible/physics authority.

The stress gate uses:

- LOD0: 8192² × 1 tile at 2 Hz;
- LOD1: 4096² × 8 tiles at 1 Hz;
- LOD2: 1024² × 16 tiles at 0.5 Hz;
- Water `R32UI` ping-pong;
- Snow `R16UI` ping-pong;
- Mud `R8UI` ping-pong;
- 128-row frame-distributed work units;
- asynchronous OpenGL timer queries.

No work exists beyond the 300 m proposed presentation horizon.

## Consequences

- We obtain real GPU/CPU/VRAM evidence on the user's hardware before deleting a working authority path.
- The workload can be deliberately harsher than production residency because Water/Snow/Mud may all allocate together for the gate.
- If Snow or Mud allocation fails, Water profiling is preserved and F8 exposes the degraded allocation rather than preventing launch.
- This milestone must not be described as GPU authority yet. It is a performance gate.
- The next authority milestone must reuse the accepted GPU resources/scheduler rather than creating a second unrelated implementation.
