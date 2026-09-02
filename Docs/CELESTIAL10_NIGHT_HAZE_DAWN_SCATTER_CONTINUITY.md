# CELESTIAL10 — Night Haze + Dawn Scatter Continuity

## Symptom

CELESTIAL09 still left two visible atmosphere problems: deep-night aerial haze was too luminous, and dawn contained a short interval where atmospheric haze disappeared before the dawn haze returned.

## Root cause

The dawn gap was a real ownership mismatch. The PBSKY presentation shader could reach almost full physical-sky authority while the sky-view LUT was still being generated with `lighting.sunIntensity`. That value is intentionally clamped to zero below the geometric horizon because it is the direct surface-light value. As a result Heritage blended away from its authored twilight fallback toward an effectively unlit physical LUT, then restored atmospheric scattering only when direct sunrise began.

## Changes

- Deep-night visible air-light is reduced from 0.34 to 0.034: exactly one tenth of CELESTIAL09.
- Moon-lit haze lift is reduced from 0.16 to 0.016. Atmospheric extinction/density remains intact.
- Rain/mist fog colour is scaled by the same illumination term, preventing weather fog from reintroducing a bright nocturnal veil.
- PBSKY sky-view generation receives a separate `atmosphereSunIntensity` derived continuously from the shared day/night cycle. The upper atmosphere can therefore scatter twilight before direct ground sunlight exists.
- Physical-sky authority now fades from 0.035 to 0.220 of the shared cycle instead of 0.015 to 0.145, keeping the continuous authored twilight fallback visible until PBSKY has meaningful scattering.
- No new day/night state machine or threshold authority is introduced. The CELESTIAL06 shared solar-cycle scalar remains authoritative.

## Expected result

Night keeps real distance extinction but loses the milky luminous veil, making the star field substantially easier to see. Dawn and dusk remain continuously hazy/scattered instead of passing through a brief haze-free/black-atmosphere notch.
