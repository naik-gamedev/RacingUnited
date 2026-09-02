# CELESTIAL09 — Dawn IBL Continuity + Night Air-Light

## Problems

After CELESTIAL08, two presentation issues remained:

1. The night-to-morning transition could still appear to switch even though the astronomical/day-night scalar was continuous. The procedural IBL cubemap was refreshed one face per frame and then atomically swapped once all six faces were complete. At accelerated time, that makes material ambient/reflections jump from a night snapshot to a newer dawn snapshot in one rendered frame.
2. J9 aerial haze was visually too strong at night. CELESTIAL08 retained a dedicated deep-night density bonus even though visible air-light should decrease when solar illumination disappears. The star plate was consequently less legible than intended for the rural reference scene.

## Dawn IBL continuity

The two existing 128x128 RGB16F cubemaps are retained; no third texture or synchronous six-face rebuild is introduced.

- After a staged cubemap refresh completes, the old active cubemap remains intact in the staging texture.
- The new cubemap becomes current, but materials cross-fade old -> new over 0.18 s of wall time.
- The staging texture is not reused for another refresh until that blend completes, so it can never contain partially rewritten faces while sampled as the previous IBL.
- Entity materials cross-fade both diffuse and roughness-aware specular IBL.
- Volumetric-cloud environment ambient uses the same old/new cubemap blend; its existing 82% live-frame sky authority remains intact.
- Forced refreshes bypass the blend and rebuild immediately, preserving deterministic reset/hot-reload semantics.

The result is continuous dawn/dusk ambient and reflection transport without weakening CLEAN05/OPT04 renderer ownership or reintroducing a synchronous cubemap pulse.

## Night haze / stars

Atmospheric particles still exist after sunset, so extinction is preserved. What is reduced is visible scattered air-light:

- the artificial `deepNight` density bonus is removed;
- deep-night haze colour/air-light falls to 34% of daytime authority, with at most a small Moon-dependent lift;
- daylight/twilight smoothly restore full visible air-light through the same CELESTIAL06 day-night scalar;
- PBSKY star extinction remains physically atmosphere-driven, but the HDR star plate receives a restrained 1.22x deep-night visibility boost that fades away through morning twilight.

This keeps night atmosphere present without making it look like a luminous daytime veil and preserves the rural-sky intent without returning to the earlier fantastically bright night.

## Preserved

- CELESTIAL06 single solar-cycle authority.
- CELESTIAL07 physical Sun/Moon shadow ownership and zero-strength handoff bridge.
- CELESTIAL08 0.10x cloud Sun attenuation and stable scene-climate atmosphere inputs.
- CLOUDURP15EI temporal architecture and volumetric density/shape.
- LIVETRACK22A, Dynamic Surface and tire physics.
