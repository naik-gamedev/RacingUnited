# CLOUDURP15E8 — Stronger selective temporal denoiser

CLOUDURP15E8 keeps the existing single UnityVolumetricCloudsURP/HDRP-derived full-resolution temporal resolve. It does **not** add a second TAA system, spatial blur pass, or cloud-only post filter.

The user-requested denoiser increase is implemented by strengthening temporal accumulation inside the existing resolve:

- coherent cloud structure: history baseline `0.95 -> 0.97`;
- stochastic partial-volume samples: selective history target `0.985 -> 0.992`;
- strongest noisy samples: `0.9975 -> 0.9985`;
- the selective classifier begins slightly earlier (`0.075..0.36` mild, `0.40..0.82` strong).

The same current-frame five-pixel RGB AABB clamps reprojected history, clear-sky neighbourhoods still bypass accumulation, and camera velocity still reduces history strength. The change therefore reduces visible volumetric grain while preserving the existing anti-ghosting authority and without increasing raymarch cost.
