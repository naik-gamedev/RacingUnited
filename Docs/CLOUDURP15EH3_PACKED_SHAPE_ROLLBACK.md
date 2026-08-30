# CLOUDURP15EH3 — Packed Shape Rollback

Runtime visual validation showed that CLOUDURP15EH/EH2 could erase cloud occupancy entirely. The packed RGBA experiment is therefore removed from production density authority rather than tuned further.

Production cloud shape loading and density evaluation are restored exactly to the last known-visible CLOUDURP15EG implementation:

- `WorleyNoise128R.hvol` is the runtime base-shape asset again;
- visible and shadow density use the scalar de-aliased shape sampler;
- no `processPackedShapeNoise` fBm remap participates in cloud occupancy;
- CLOUDURP15EG rotated shape/erosion sampling and 256-sample vertical profile remain;
- CLOUDURP15EE four occupied substeps and full-resolution 64-step reconstruction remain.

The unused `WorleyNoise128RGBA.hvol` file may remain on disk after overlay extraction, but it is no longer referenced by the renderer.
