# PERF08 - Render Submission Forensics

## Purpose

PERF02 removed major accidental CPU work from the draw loop and PERF06 removed the
recurring DirectInput device enumeration hitch. PERF07 then proved that the remaining
meaningful CPU hitch was charged to the top-level Render submit bucket. PERF08 splits
that bucket without forcing GPU synchronization.

## Overlay additions

The F8 overlay now reports rolling CPU wall time for:

- module render callbacks
- entity mesh renderer
- entity debug renderer
- MSAA resolve
- post-processing / final blit
- triple-monitor span composite
- residual render CPU work

The most recent >=25 ms CPU hitch stores the exact unsmoothed version of those same
render sub-sections.

The entity mesh renderer additionally reports:

- entity mesh-instance gather time
- procedural environment cubemap update time and whether it regenerated this frame
- sky draw CPU time
- total CPU time spent in mesh instances
- the slowest mesh asset for the current frame
- VAO binds, material switches, texture binds, winding changes and skinned ranges

## Important interpretation note

These are CPU wall-clock timings around normal OpenGL calls. PERF08 deliberately does
not use glFinish. With VSync enabled, an OpenGL driver may apply back-pressure during a
draw call instead of waiting in glfwSwapBuffers. If that happens, the wait will appear
inside the mesh renderer or a particular mesh instance. That is useful evidence rather
than a measurement bug.

## Next decision

Use one F12 capture after normal driving and, if one occurs, after a reproduced hitch.
The result should tell us whether the next optimization should target scene batching,
material/state submission, environment refresh, post-processing, or driver/frame-pacing
behavior.
