# PERF10 - Frame Pacing and Surface Submission

## Why this pass exists

A rain-on performance capture showed roughly 62 FPS with only about 6 ms of GPU
work, while the CPU-active frame was near 16 ms and the render-submit bucket was
about 11 ms. The latest captured hitch was dominated by render submission rather
than physics. That makes render-thread pacing and avoidable submission work the
next useful target; lowering rain density or water simulation fidelity would not
address the measured bottleneck.

## Hydrology was inspected, not re-staggered

JOB03 already implements the requested distance cadence (30/20/6/2 Hz through
1000 m, plus 0.5 Hz background persistence) and deterministically phase-staggers
non-near spatial chunks. Each due chunk advances by its real elapsed physical time.
PERF10 therefore leaves hydrology cadence/authority unchanged instead of layering a
second scheduler on top of an existing solution.

## Surface-presentation allocation removal

`SurfacePresentationRenderer::draw()` previously constructed a local track vertex
vector and called `reserve(240000)` every rendered view. A track vertex is seven
floats (28 bytes), so this could request and release about 6.7 MB (~6.4 MiB) of transient heap
capacity every frame even when little or no debug track geometry was visible.

PERF10 moves transient presentation staging to persistent renderer-owned scratch
vectors. Track, particle, resting-marble and moving-rubber staging now retain their
capacity between frames and release it only when the renderer shuts down.

## OpenGL uniform metadata caching

Tire-mark, marble, moving-rubber, track and particle programs previously performed
many `glGetUniformLocation()` string lookups from render paths. Uniform locations are
program metadata and do not vary frame to frame, so PERF10 resolves them once after
shader program creation and reuses the cached locations during draw submission.
Water already followed this policy and is unchanged.

## F8 render-forensics expansion

The top-level Render submit timer now has explicit child timings for:

- module rendering;
- entity mesh rendering;
- surface presentation;
- weather presentation;
- debug rendering;
- framebuffer/viewport/clear setup;
- MSAA resolve;
- post processing/final blit;
- triple-monitor span composite;
- residual render time.

The same breakdown is captured unsmoothed for the latest CPU-active hitch. The hitch
threshold is 20 ms so the diagnostic catches smaller spikes relevant to 0.1% lows.
The overlay also prints VSync and the selected FPS cap, plus a computed
`Mesh submit residual/driver` value after subtracting named mesh-renderer CPU scopes.

## How to interpret the next capture

If Surface or Weather is large, optimize that concrete renderer. If framebuffer,
post or residual is large, inspect that path directly. If `Mesh submit
residual/driver` remains several milliseconds while the GPU frame is much shorter
and VSync is enabled, OpenGL driver back-pressure may be blocking inside draw calls
rather than inside `glfwSwapBuffers`; the wall-time scope is intentionally allowed to
show that wait. A VSync-off diagnostic run can distinguish pacing wait from actual
CPU mesh work without permanently changing the user's normal settings.

## Preserved contracts

- Rain remains 10k candidates at 0-2 m, 100k at 2-10 m, 10k at 10-100 m, with the
  distant water-curtain shader from 100-1000 m.
- Hydrology remains authoritative and distance-adaptive at the established JOB03
  cadences.
- No `glFinish` or synchronous GPU readback was introduced for profiling.
- The optimization changes presentation CPU lifetime/diagnostics only; it does not
  alter tire, water, weather or vehicle physics behavior.
