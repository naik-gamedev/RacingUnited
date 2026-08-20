# PERF16A — Drivable Connected Water

PERF16 proved the connected indexed topology but made the wrong classification decision: every microscopically wet cell was eligible for explicit connected geometry. Heavy rain can wet almost the entire 0–200 m hydrology field, turning the renderer into a hundreds-of-thousands-of-patches CPU mesh builder.

PERF16A restores the intended architecture: **hydrology cells are the authority; wet film is a material; explicit geometry is for accumulated water.**

## Explicit-free-surface thresholds

- 0–50 m: 1.5 mm
- 50–100 m: 2.0 mm
- 100–200 m: 3.0 mm

These are presentation thresholds only. They never delete or quantize water mass. A 0.8 mm film still exists physically, moves, drains, wets tires and affects surface state; it simply does not become a transparent connected mesh.

The threshold is applied before adaptive-grid construction, not after collection, so shallow cells avoid the expensive adaptive and stitch passes entirely.

## Warm-up pacing

Eight invalid water rings previously initialized together. PERF16A initializes at most one previously-invalid connected-water ring per rendered frame. Existing valid rings retain their normal 30/20/6/2 Hz presentation cadence. This avoids a large one-frame startup rebuild without changing simulation.

## Visual consequence

Immediately after rainfall starts, the road should first become darker/glossier through wet-material shading. Explicit connected puddle geometry appears only where water actually accumulates. This is both cheaper and more physically believable than covering the whole wet road in a transparent water sheet.
