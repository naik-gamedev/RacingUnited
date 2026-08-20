# ADR-080 — OpenGL Procedural Rain Wetting Presentation

## Status
Accepted for WATER05A.

## Context
WATER04 proved that pushing hydrology wetness into the general entity material renderer was too invasive: a live build produced catastrophic scene rendering corruption. WATER04C recovered the known-good JOB03 render path by isolating hydrology from the active general entity shader. The remaining JOB03 water pass was stable but visually exposed hydrology-cell construction and made thin rain film read as translucent blue water cards rather than a road becoming wet.

The desired rain-onset appearance is physically recognizable: isolated raindrops first create small dark circular wet marks, those marks spread and overlap, and sustained rain turns the material into a continuous darker, glossier wet surface. Standing water should become more reflective than the thin wet film.

## Decision
WATER05A implements the first safe visual step entirely inside the existing dedicated OpenGL `SurfacePresentationRenderer` water pass. The general entity shader remains untouched. The hydrology field remains authoritative for water depth, flow and persistence.

The OpenGL/GLSL presentation uses world-anchored deterministic procedural rain candidates. A 3x3 neighbourhood around a jittered 18 cm candidate grid is evaluated per fragment so circular wetting fronts cross hydrology-cell boundaries. Each candidate has a deterministic activation threshold. As authoritative film depth grows, existing circles expand and additional circles activate; near saturation the remaining microscopic dry gaps are bridged so the surface becomes coherently wet instead of retaining a permanent polka-dot pattern.

Active precipitation is supplied as a cached shader uniform from `SurfaceWorld::weather()`. It only drives recent visual impact/ripple activity; it does not add water. Water mass and wetness authority remain in `SurfaceHydrology`. The pattern never uses camera position, so camera motion cannot drag rain marks across the world.

Thin wet film is rendered as a dark neutral overlay with restrained environment reflection, allowing the actual road/terrain albedo to remain visible beneath. Deeper accumulated water progressively transitions to stronger Fresnel/environment reflection and rain-impact crests. Water geometry remains surface-following. Individual hydrology cards must never independently rotate toward world-up; coherent flat free surfaces are reserved for a later connected puddle reconstruction pass.

## OpenGL constraints
- Windows uses the existing GLSL 4.60 core path; the non-Windows validation path remains GLSL 3.30 core compatible.
- No geometry shader is added to water; records remain instanced four-vertex triangle strips.
- No per-raindrop CPU objects, allocations or render-rate hydrology uploads are introduced.
- The existing JOB02 VBO cache and hydrology-cadence upload policy remain intact.
- The rain-rate uniform location is resolved once at shader initialization rather than queried by string every frame.

## Consequences
The first rain should now read as scattered expanding dark circles that progressively join into a continuous wet material. Standing water retains a separate reflective look. This does not yet solve connected puddle boundaries: WATER05B should reconstruct coherent standing-water surfaces from hydrology data instead of exposing one geometric card per cell.
