# ADR-130 — Heritage Dynamic Surface Owns Runtime Hydrology

## Status

Accepted — DSURF03B.

## Decision

Runtime surface water, moisture and horizontal flow are authoritative state of Heritage Dynamic Surface persistent pages. `SurfaceWorld` must not advance legacy `SurfaceHydrology` dynamic state once DSURF03B is active.

The persistent key is DSURF01 `VirtualPageAddress` (world chunk + surface sheet + page). Camera-relative clipmaps, generated water geometry, adaptive-cell footprints, puddle splats and renderer-owned water mass are forbidden as authority.

## Rationale

The WATER14–WATER18 line repeatedly coupled physical discretization to visible puddle topology, producing squares, triangles, distance-dependent regeneration, popping and severe performance cost. DSURF00–02 established stable world/surface identity and sparse persistent GPU pages. DSURF03A proved presentation could consume those pages. DSURF03B completes the cutover by moving the actual dynamic water state and tire interactions into the same persistent surface system.

This also establishes the pattern used by later temperature, rubber, marbles, dirt and mud migrations: physics and rendering address the same world-anchored surface state but rendering never owns the physical quantity.

## Consequences

- rain, infiltration, drainage, evaporation and shallow flow update Dynamic Surface hydro authority;
- tire clearing/redistribution/spray update the same authority;
- bridge/road/tunnel separation is based on DSURF01 surface sheets and support heights;
- renderer uploads are a view of persistent authority, not a second water state;
- the first authority resolution is ~39.06 cm and may be refined later without changing persistent addressing;
- legacy `SurfaceHydrology` may remain temporarily only for static precipitation-cover compatibility and historical tests, never as runtime water state;
- future DSURF10 cleanup must delete the superseded legacy dynamic water store once its final compatibility consumers migrate.
