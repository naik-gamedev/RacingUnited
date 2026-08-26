# OPT04C — Shadow CPU Shared Frame Preparation

OPT04C attacks remaining CPU-side shadow overhead without reducing shadow
resolution, cascade count, PCSS quality or day/night behavior.

## Shared mesh preparation

Before OPT04C, a sun-shadow frame performed the same expensive instance setup
in two immediate passes. `drawShadowMaps()` acquired every mesh, rebuilt its
camera-relative model matrix, advanced/evaluated animation, applied node
overrides and resolved tire-deformation overrides. The normal material pass then
repeated those operations again.

The renderer now creates one `PreparedFrameInstance` entry per registry mesh
instance. That frame-local cache owns the mesh pointer, camera-relative model
matrix, evaluated node transforms and sparse tire visual override pointers. Both
the layered shadow pass and the visible material pass consume the same data.

This is also a correctness improvement for animated content: one render frame
now has one evaluated animation pose instead of evaluating the animation runtime
twice with the second call receiving a zero time delta.

## Shadow state submission

The shadow pass still uses the SHADOW02 layered depth-array path, but it tracks
whether face culling is already in the requested state and which VAO is already
bound. Repeated instances with the same state therefore avoid redundant driver
calls.

## Skin palette upload

The previous visible and shadow paths repacked and uploaded all 64 possible
joint matrices for every skinned range, padding unused entries with identity
matrices. The shader can only index joints belonging to the current skin, so
OPT04C uploads exactly `palette.size()` matrices. No skinning math or joint
ordering changes.

## Explicitly unchanged

- 4 cascades and existing split distances.
- Layered geometry-shader fan-out and per-range cascade masks.
- Shadow map quality presets and Ultra resolution.
- Nearest, Poisson PCF and PCSS + Poisson filtering.
- Dynamic astronomical sun/day-night cascade rebuilds.
- Asynchronous shadow GPU timing.
- Volumetric weather/cloud behavior.
- GPU Dynamic Surface water authority and `.hhyd` v15.
