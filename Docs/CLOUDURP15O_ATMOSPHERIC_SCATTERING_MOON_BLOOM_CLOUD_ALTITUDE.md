# CLOUDURP15O — Atmospheric Scattering, Moon Bloom, Cloud Altitude Diversity

## Goal

Make the celestial sky feel optical rather than composited, and remove the impression that every volumetric cloud formation occupies one common horizontal slab.

## Atmospheric scattering

The visible sky shader now adds a lightweight single-scattering approximation on top of the existing astronomical sky authority:

- wavelength-selective Rayleigh scattering gives blue air its physical bias;
- humidity/rain-driven Mie scattering adds forward haze around the Sun;
- low solar elevation increases wavelength-selective extinction, naturally warming the scattered light near sunrise/sunset;
- view air mass increases toward the horizon;
- deep night retains only a tiny blue airglow floor instead of an additive daytime haze.

This is intentionally a sky/celestial scattering model, not a costly full-screen volumetric aerial-perspective pass over scene geometry. Existing astronomical horizon/zenith colours and exposure remain authoritative.

## Moon bloom

The Moon remains composited after sky tone mapping, preserving Moon.png clarity and the CLOUDURP15L atmospheric extinction path. CLOUDURP15O adds three local optical halo lobes:

- a soft near-disc shoulder;
- a modest middle halo;
- a very faint broad halo.

A small baseline represents glare/eye response even in clear air. Relative humidity, rain haze and low lunar altitude increase the halo. The effect is local to the Moon; no global bloom pass was introduced, so headlights, UI and the rest of the scene are not unintentionally smeared.

## Cloud altitude diversity

CLOUDURP15H8 already sampled low-frequency fields for cloud altitude/thickness, but the final values had been neutralized to `offset = 0` and `thickness = 1`, leaving formations in effectively the same slab.

CLOUDURP15O activates world-anchored smooth vertical variation:

- global admissible cloud shell was introduced here and is now raised by CLOUDURP15AX to about **2.64 km to 6.80 km ASL**;
- local cloud bases now vary smoothly around roughly **2.64–3.80 km ASL**, preserving the same world-anchored variation mechanism;
- local thickness now varies roughly **0.75–2.95 km**, with convective types extending higher;
- convective-looking cloud types are allowed additional vertical growth;
- neighboring cells interpolate through low-frequency noise rather than jumping between discrete heights;
- cloud-shadow density uses the same local base/top function as the visible raymarch, so shadows remain aligned with the visible cloud field.

The raymarch step count remains 32 and the maximum step remains 250 m, so this does not add another cloud pass or increase the primary sample budget.

## Preserved

- CLOUDURP15N continuous twilight and live astronomical cloud lighting.
- CLOUDURP15L2 half-strength Moon scene illumination.
- CLOUDURP15K position-stable regional cloud field.
- CAM10 free-camera UI cursor + persistent handbrake behavior.
- Existing cloud TAA, rain, radar, cloud shadows, LIVETRACK hydrology and environment IBL architecture.
