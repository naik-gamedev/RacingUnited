# PBSKY01 — Physically Based Sky Foundation

PBSKY01 replaces Heritage's legacy artistic sky fragment with a native OpenGL 4.6/GLSL physical atmosphere derived from the architecture and Earth preset in jiaozi158/UnityPhysicallyBasedSkyURP. The upstream project is MIT-licensed; the required notice is stored in `Docs/ThirdParty/UnityPhysicallyBasedSkyURP_NOTICE.txt`.

## Runtime ownership

- `SkyRendererPbrAtmosphere.cpp` owns physical-atmosphere program lifetime, LUT textures/FBO and update cadence.
- `SkyRendererPbrAtmosphereShaders.cpp` owns the translated physical scattering and final sky GLSL.
- `SkyRendererAtmosphereShaders.cpp` now owns only the cube vertex stage; the previous artistic fragment is retired.
- `SkyRenderer.cpp` remains the orchestration owner and preserves astronomical Sun/Moon/star-map integration.
- Existing volumetric-cloud code and cloud assets are intentionally unchanged for PBSKY01; VCLOUD01 is the separate cloud-renderer migration gate.

## Atmosphere

The Earth preset uses a 6,378.1 km planet radius, 60 km atmosphere, 8 km Rayleigh scale height, 1.2 km aerosol scale height, wavelength-dependent Rayleigh coefficients and a 20–40 km triangular ozone layer. Regional humidity and precipitation modulate aerosol loading without altering molecular-air or ozone coefficients.

Heritage uses three small HDR LUTs: a 256 x 64 transmittance cache, the upstream 32 x 32 multiple-scattering resolution and the upstream 256 x 144 sky-view resolution. Multiple scattering uses 64 directional samples with 16 integration segments; sky-view integration uses 16 non-linearly distributed segments to retain horizon detail. The sky-view table follows Heritage's moving astronomical Sun.

The transmittance LUT is a Heritage-native cache around the same physical extinction model; it replaces repeated analytical optical-depth work in the translated runtime rather than emulating Unity/URP command-buffer APIs.

## Deliberately preserved

- astronomical day/night and geographic star orientation;
- authored Moon texture and lunar direction/phase authority;
- current volumetric clouds, weather radar, precipitation, wind and cloud shadows;
- PERF05 cached shader-link validity;
- PERF06A F8 diagnostics and OPT00 asynchronous GPU timers;
- all Dynamic Surface / `.hhyd v15` behavior.

## User validation gate

Build and run through `Tools/00_BuildAndRunCurrent.cmd`. Check clear noon, low-Sun dawn/sunset, twilight and night before VCLOUD01 is layered onto the new atmosphere. This checkpoint intentionally isolates physical-sky correctness from cloud morphology changes.
