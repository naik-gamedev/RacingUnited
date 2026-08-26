# CLOUDURP15L2 — half moon scene illumination

The full Moon texture and CLOUDURP15L atmospheric visibility remain visually unchanged.

Moon-driven scene lighting is reduced to exactly 50% of CLOUDURP15L1:

- night directional/key moonlight uses `kMoonSceneIlluminationScale = 0.50`;
- moon-driven sky/ground ambient lift uses the same 0.50 factor;
- regional weather re-selection of the Moon key light preserves the same scale;
- visible `Moon.png`, horizon extinction, humidity/rain haze, warm horizon tint, and halo are not dimmed by this tuning change.

This keeps a full Moon clearly visible while preventing roads, terrain, buildings, and cloud lighting from reading like weak daylight.
