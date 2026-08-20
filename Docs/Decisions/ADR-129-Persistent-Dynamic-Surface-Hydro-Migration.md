# ADR-129 — Persistent Dynamic Surface Hydro Migration

## Decision

Water, moisture and horizontal surface-flow presentation shall migrate to the persistent **Heritage Dynamic Surface** virtual-page system before further water rendering work is added.

The live scene renderer shall not reconstruct puddle ownership from camera-centred clipmaps, solver-cell rectangles, basin canvases, circular splats, sponge dabs, adaptive water meshes or duplicate scene geometry.

## Why

WATER14–WATER18 repeatedly exposed the same architectural error: simulation discretization or camera-relative presentation storage became visible as squares, rectangles, popping or distance-dependent puddle shapes. Increasing atlas resolution, filtering the atlas or changing the stamp shape did not remove that ownership problem.

Heritage Dynamic Surface instead gives every wettable authored surface stable world/sheet/page identity. State survives camera motion and uses normal texture mipmapping rather than regenerating a different representation at each view distance.

## DSURF03 bridge

The conserved `SurfaceHydrology` solver remains authoritative temporarily. Dynamic Surface Hydro pages sample that solver at DSURF01-authored support positions. The solver's *state* is therefore reused while its cell geometry/footprint is forbidden from presentation.

Hydro pages contain water depth, moisture, flow X and flow Z. A separate static support-height plane disambiguates overlapping sheets.

Only DSURF01-covered pages are resident. Residency follows the nearest real simulation-interest source and never a midpoint between local players.

## Consequences

- WATER15–WATER18 live renderer state is retired.
- water is shaded in the ordinary authored surface material.
- no second water geometry owner exists.
- persistent page identity is now the common route for future temperature, rubber, marbles, dirt and mud.
- final solver migration remains future work; DSURF03 is an explicit compatibility bridge, not permission to retain legacy water storage indefinitely.


## DSURF03B status

**Superseded for runtime authority by ADR-130.** The temporary mirror described above completed its purpose. `SurfaceWorld` no longer advances legacy `SurfaceHydrology` state; Heritage Dynamic Surface now owns runtime water, moisture, flow and tire-water interaction.
