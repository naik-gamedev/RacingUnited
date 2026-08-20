# PERF19D – Material Uniform Recovery

PERF19A's ownership refactor accidentally replaced the normal `EntityMeshRenderer`
material-program uniform/sampler initialization with `initializeWetFilmResources()`.
That left the base renderer's cached uniform table and sampler bindings at default
values, causing the entire scene to render grey/undefined regardless of later
wet-film GL-state hotfixes.

PERF19D restores the complete known-good material shader initialization first,
including material maps, environment/shadow samplers, transforms, lighting,
skinning and tire-visual uniforms. Optional PERF19 wet-film resources are then
initialized afterward.

The root renderer remains below the CLEAN05 1200-line orchestration guard.
A static contract now requires the normal material sampler initialization to
remain present alongside the optional wet-film initializer.
