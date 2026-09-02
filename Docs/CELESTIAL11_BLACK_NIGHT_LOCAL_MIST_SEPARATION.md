# CELESTIAL11 — Black Night / Local Mist Separation

## Goal

Normal rural night should not contain a scene-wide luminous atmospheric haze. Atmospheric particles still exist, so distant geometry may lose contrast through extinction, but without a strong light source there is no bright blue/grey air-light veil.

## Changes

- Deep-night **global atmospheric air-light is now exactly zero**.
- The CELESTIAL10 continuous dawn solar-airlight curve remains unchanged: air-light begins smoothly only as astronomical dawn develops.
- Moon intensity no longer raises the global fog colour. The authored Moon halo remains local to the Moon presentation.
- Atmospheric and rain/mist **density/extinction coefficients are unchanged**. At night they therefore attenuate toward darkness rather than toward a luminous fog colour.
- This intentionally does **not** fake headlight/streetlight mist in the global fog pass. Proper visible nocturnal mist needs a low-altitude local-light volumetric scattering receiver so light cones and pools reveal only nearby moisture. That is kept as a separate rendering responsibility.

## Expected result

Clear deep night reads as dark/black with the HDR star field unobscured by a global haze veil. Wet or humid night air can still reduce distant contrast, but it does not glow by itself. Dawn reintroduces atmospheric air-light continuously through the existing CELESTIAL10 solar-scattering curve, with no new day/night switch.
