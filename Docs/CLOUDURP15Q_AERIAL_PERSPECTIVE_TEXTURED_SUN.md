# CLOUDURP15Q - scene-wide aerial perspective + textured Sun

## Goal

Embed rendered world geometry in the same atmosphere already used by the sky. Far terrain, buildings, roads, vehicles, surface presentation and weather presentation should lose contrast into atmospheric in-scattering with distance instead of remaining cut-out against the horizon.

## Global aerial perspective

The normal single-monitor render path now keeps the resolved reversed-Z depth buffer as a sampleable `GL_DEPTH32F_STENCIL8` texture. The existing final FXAA/blit fullscreen draw reconstructs camera-space distance from that depth and applies atmospheric transmittance + in-scattering in the same pass, so there is no additional color framebuffer or fullscreen draw.

The pass uses the live astronomical/environment state already resolved by `EnvironmentSystem` plus camera-local weather from the regional weather authority:

- sky horizon and zenith colours;
- Sun direction, colour and intensity;
- atmosphere thickness and sky exposure;
- relative humidity, cloud cover and precipitation;
- camera altitude.

A 120 m near-camera grace distance keeps cockpit/car/near geometry crisp. Atmospheric density falls with altitude, so the detached free camera sees less sea-level haze when climbing. Low-angle paths get a longer lower-atmosphere optical path, making distant hills and buildings merge naturally toward the horizon.

Humidity/rain increase Mie-like neutral/grey scattering. Near sunrise/sunset, forward scattering toward the real Sun adds warm orange/red in-scattering to distant geometry.

The previous per-material entity fog is suppressed while the global pass is active to avoid double fogging. Multi-monitor spanning retains the legacy material fallback because each monitor uses a different off-axis projection.

## Textured astronomical Sun

`Modules/RacingUnited/Assets/Scenes/Sun.png` is now loaded by `SkyRenderer` and used as a finite solar disc rather than the old power-function yellow core.

The disc is approximately the real angular size (~0.57 degrees diameter). The same astronomical `sunColor`, `sunIntensity`, atmospheric solar transmission and cloud occlusion remain authoritative.

At high elevation the disc is intentionally overexposed, so it still reads as a brilliant Sun. At low elevation atmospheric extinction reduces the core and the warm astronomical colour becomes stronger, allowing the texture silhouette/disc shape to be seen more clearly during sunrise and sunset.

## Existing systems preserved

CLOUDURP15P emissive star micro-bloom, CLOUDURP15O sky scattering/Moon bloom/cloud altitude variation, CLOUDURP15N twilight continuity and cloud light colour, CAM10 camera/UI behavior, and LIVETRACK hydrology remain intact.
