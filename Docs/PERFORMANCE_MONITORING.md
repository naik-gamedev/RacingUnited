# PERF01 — Runtime Performance Monitor

PERF01 adds a low-overhead in-engine diagnostic overlay so new systems have to
account for the milliseconds they consume instead of relying on FPS guesses.

Press **F8** to show/hide the overlay.

## Rolling timing data

The overlay reports smoothed values for:

- FPS / complete frame time
- CPU active frame time
- housekeeping/input/audio/window work
- fixed-step physics
- module/game/environment update
- CPU render submission
- UI CPU work
- present/V-Sync wait
- asynchronous GPU frame time from OpenGL timer queries

GPU timer queries are read only after the driver reports the result available;
the monitor does not stall the CPU waiting for the GPU.

## Render counters

The current counters include:

- entity-mesh draw calls
- entity-mesh triangles
- mesh instances submitted
- debug draw calls/triangles
- loaded mesh assets
- entity count

These are deliberately named rather than pretending to be every GPU command in
the engine. Future renderer passes can add their own counters.

## Physics and vegetation counters

The monitor also reports:

- fixed world steps executed this rendered frame
- physics overload state
- vegetation species
- vegetation instances
- occupied vegetation chunks
- packed vegetation placement bytes

This makes VEG01 measurable before the first real vegetation asset is added.
When VEG02 arrives, visible cluster count, whole-plant impostor count, HLOD
count and foliage overdraw/GPU cost should be added here.
