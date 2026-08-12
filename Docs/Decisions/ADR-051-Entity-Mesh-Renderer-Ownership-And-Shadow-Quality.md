# ADR-051 — Entity mesh renderer ownership and shadow quality

## Status
Accepted for CLEAN05 candidate validation.

## Context
`EntityMeshRenderer.cpp` had grown to roughly 3,861 lines and owned asset caching, animation/node
state, embedded GLSL, cascaded shadows and the main material draw path. This made unrelated renderer
changes collide in one translation unit and made the shadow-map resolution an unowned magic number.

## Decision
Split the implementation by stable responsibility while keeping `EntityMeshRenderer.hpp` as the
public façade. Keep embedded GLSL in `EntityMeshShaders.hpp` until Heritage has a deliberate shader
asset pipeline. Keep shared renderer-only math behind `EntityMeshRendererInternal.hpp`; it is not a
public graphics API.

Centralize shadow quality in `EntityMeshShadowConfig.hpp`. Use 3072×3072 as the current High/default
resolution, retain 2048 as Medium and scaffold 1024 Low / 4096 Ultra. Clamp allocation to the GPU's
reported maximum texture dimension. A future graphics-setting UI may select the preset; it should
not reintroduce hard-coded shadow resolution into draw code.

## Consequences
Incremental changes to cache, animation and shadows no longer require editing the main material draw
translation unit. Clean builds gain more parallelizable translation units. The higher default shadow
resolution increases shadow depth memory/fill cost relative to 2048, so future graphics settings must
allow lower presets on weak GPUs. Per-cascade resolutions require a different texture layout and are
explicitly deferred.
