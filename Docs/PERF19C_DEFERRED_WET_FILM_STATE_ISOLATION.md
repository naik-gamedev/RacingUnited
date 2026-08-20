# PERF19C – Deferred Wet-Film State Isolation

PERF19/19B showed catastrophic base-scene corruption even after moving the overlay to dedicated texture units. PERF19C removes the wet-film shader switch from the normal material/instance loop entirely.

## Changes
- Normal entity/material rendering completes for every instance before wet-film submission begins.
- `drawWetFilmPass` runs as one explicit second pass over `SurfaceWetnessReceiver` geometry.
- The pass binds the wetness atlas, breakup mask and environment map once, on texture units 12/13/14.
- GL program, VAO, active texture, texture bindings, depth function/write mask, blend factors, cull enable, depth-test enable and front-face state are snapshotted and restored.
- A dry-atlas fast gate skips the entire pass when no hydrology texel has visible wetness.
- Hydrology, puddle eligibility and PERF19 6/8/12 mm thresholds are unchanged.

This is intentionally an ownership/state fix rather than another change to the water look.
