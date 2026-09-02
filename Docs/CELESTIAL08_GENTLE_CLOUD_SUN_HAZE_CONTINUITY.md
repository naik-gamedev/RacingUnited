# CELESTIAL08 — Gentle Cloud Sun Attenuation + Haze Continuity

## Problem

After CELESTIAL07 stabilized Sun/Moon shadow ownership, two presentation issues remained:

1. Clouds removed far too much direct sunlight. Three receiver-side paths were stacking: broad regional weather attenuation, per-material regional cloud filtering, and the dedicated detailed ground cloud-shadow receiver.
2. Atmospheric haze could appear to switch between stronger/weaker states because broad haze/PBSKY aerosol inputs followed the camera-local regional weather sample. The regional cloud selector is intentionally spatial and can move quickly across the camera; it is not appropriate as the primary authority for the whole atmospheric air column.

## Direct-Sun attenuation

CELESTIAL08 leaves volumetric cloud density, opacity, scattering and shape untouched. Only the illumination received by the world is softened.

- Broad weather computes the previous CELESTIAL07 transmission unchanged, then blends only 10% of that loss into direct Sun intensity.
- Regional material filtering computes the previous 0.22 cloud / 0.78 rain transmission unchanged, then blends only 10% of the resulting total loss into direct light.
- Regional cloud colour filtering likewise blends only 10% of the previous tint.
- The detailed post-opaque receiver strength changes from 0.44..0.64 to 0.044..0.064.
- Its cool shadow tint is also scaled to 10%, preventing colour attenuation from remaining strong after luminance attenuation was reduced.

This means the cloud shadow pattern remains visible and spatially coherent, but the world no longer looks as though direct sunlight nearly switches off beneath ordinary clouds.

## Haze continuity

The permanent J9 aerial haze now has stable scene-climate authority:

- authored cloud cover contributes 90% of the haze cloud term; camera-local regional cloud contributes only 10%;
- authored relative humidity contributes 85%; regional humidity contributes 15%;
- local rain/cloud/humidity are low-pass filtered with a 1.35 s response before they can modulate material haze;
- elapsed-time tracking updates only once per rendered time value, so multi-camera/triple-monitor draws cannot accidentally speed the filter;
- PBSKY transmittance/multiple-scattering aerosol LUTs use scene-authored humidity/rain rather than the moving camera-local cloud cell;
- aerosol LUT refresh tolerance is tightened from 0.01 to 0.002 so authoring changes do not arrive in coarse visible steps.

The permanent haze baseline remains non-zero at all times of day. Local weather can thicken it smoothly, but cannot toggle the broad atmosphere off/on.

## Preserved

- CELESTIAL06 single day/night-cycle authority.
- CELESTIAL07 physical Sun/Moon shadow ownership and twilight bridge.
- Visible volumetric cloud density/opacity and CLOUDURP15EI temporal architecture.
- Regional weather/radar/rain authority.
- LIVETRACK22A, tire physics and Dynamic Surface runtime.

## Probe evidence

- `Build/Reports/CELESTIAL08_SunAttenuationProbe.csv` confirms representative broad-weather and regional-material sunlight loss is exactly 0.10x CELESTIAL07 wherever the old loss is nonzero.
- `Build/Reports/CELESTIAL08_HazeContinuityProbe.csv` applies a worst-case regional cloud step from 0 to 1 and shows the haze authority converging continuously through the 1.35 s filter rather than switching states.
