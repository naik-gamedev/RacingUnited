# ADR-138 – Regional Weather Authority, Volumetric Clouds and Cloud Shadows

## Decision

Heritage Engine uses `PrecipitationField` as the deterministic world-space authority for the broad weather envelope. A regional sample provides cloud cover, humidity, precipitation rate and storm intensity. Volumetric clouds, weather radar, local airborne rain and near-camera hydrology consume that same field.

The astronomical environment/day-night system remains authoritative for sun direction, sun colour and daylight intensity. Weather is a filtering layer on top of that system, never a replacement for it.

## GPU presentation

The renderer maintains a compact 128×128 RGBA8 regional weather map over an 80 km window. The texture stores cloud cover, normalized precipitation, humidity and storm intensity. It is rebuilt only when the 500 m-snapped world window moves. Wind advection between rebuilds is applied analytically in shaders, avoiding periodic whole-map CPU refreshes.

The existing one-third-resolution, ten-step volumetric cloud raymarch samples the regional field at each integration position. Procedural fine-scale density remains world anchored and is modulated by the regional envelope.

## Cloud shadows

Cloud shadows are evaluated as broad optical transmission rather than being rasterized into the four geometry shadow cascades. A surface point is projected toward the representative cloud layer along the current sun vector; regional cloud/rain/storm density attenuates and slightly cools direct sunlight. This transmission multiplies normal cascaded shadow visibility.

This preserves cascade resolution for geometric occluders, lets kilometer-scale cloud masses move smoothly with the weather field, and keeps cloud-shadow cost independent of the number of scene meshes.

## Radar and precipitation coherence

F10 radar reconstructs the same regional field used by visible clouds. Airborne rain and the camera-region Dynamic Surface water forcing use the local regional precipitation rate, so a radar cell passing overhead corresponds to visible cloud and rain rather than an unrelated UI pattern.

## Render-state ownership

Sky rendering receives explicit framebuffer/viewport/scissor state from the engine. The sky pass does not query live OpenGL state every frame. This follows the SHADOW06 renderer-owned-state approach and avoids driver synchronization from `glGet*`/`glIsEnabled` calls while preserving multi-view and day/night behavior.

## Consequences

This is an integrated weather architecture, not a claim of full atmospheric simulation parity with any commercial title. The current cloud raymarch uses bounded single-scattering-style presentation and broad projected cloud optical shadows. More advanced multiple scattering, 3D weather dynamics, cloud self-shadow volumes and long-lived per-region ground moisture can be layered on this authority without creating a second weather system.
