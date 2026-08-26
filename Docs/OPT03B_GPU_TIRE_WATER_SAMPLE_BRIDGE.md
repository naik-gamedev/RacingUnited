# OPT03B — GPU tire-water sample bridge

OPT03B removes the production tire-physics blind spot that remained after GPU Dynamic Surface became water authority.

## Runtime contract

- Tire and tire-footprint queries enqueue world-anchored water sample positions in `SurfaceWorld`.
- Requests are deduplicated to 10 cm world cells and capped before reaching graphics.
- `DynamicSurfaceGpuRuntime` samples only those active positions from the same near RGBA8 water atlas used by rendering.
- The sample compute shader mirrors the production five-tap GL_LINEAR standing-water decode, 16-value depth ladder, dry-line attenuation, and kinematic runoff reconstruction.
- Samples are dispatched after localized tire dry-line compute, so returned depth includes the same tire clearing state seen by rendering.
- Readback uses three small SSBO slots guarded by fences. `glClientWaitSync(..., 0, 0)` only polls; if a ring slot is still busy, the sample batch is dropped rather than blocking the frame.
- No full water-atlas readback, `glGetTextureSubImage`, or `glFinish` path exists.
- Physics uses the most recent sample for the corresponding 10 cm cell; before the first GPU result it keeps the existing smooth weather-film fallback.

## Ownership

`.hhyd v15` remains immutable topology. The production Dynamic Surface GPU runtime remains the only live puddle/runoff/dry-line authority. The CPU `DynamicSurfaceHydrology` implementation is still compiled only for GPU-unavailable fallback and regression safety; normal GPU-authoritative runtime does not advance it.

Track environmental thermal cooling no longer samples dormant CPU Hydro. It consumes weather wetness/film while Track temperature and tire-contact heat remain world/sheet persistent.
