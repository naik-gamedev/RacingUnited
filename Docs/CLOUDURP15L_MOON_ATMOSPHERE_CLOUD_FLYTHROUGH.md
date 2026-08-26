# CLOUDURP15L — Moon Atmosphere + True Cloud Flythrough

## Moon: atmospheric visibility instead of a pasted disc

The Moon texture remains composited after the sky tone-map so its source detail does not collapse into a dark blurred HDR spot, but the display pass now applies atmospheric visibility before blending it into the sky.

The live moon-disc path now includes:

- altitude-dependent optical air mass;
- stronger extinction near the horizon;
- humidity and precipitation increasing atmospheric attenuation;
- a small warm shift as the Moon approaches the horizon;
- a low-energy haze halo whose size/strength grows with murky air and long horizon paths;
- visual visibility tied to the astronomical `moonIntensity`, so the texture is no longer an unconditional full-strength sprite whenever its direction is above the horizon.

Volumetric clouds are still rendered after the sky. Their actual spatial transmittance therefore remains the local Moon occluder; a cloud body can veil or hide the Moon without replacing that with a global cloud-cover scalar.

## Detached free-camera travel gear

CAM08 changes only the detached world-space free camera's Fast multiplier:

- normal detached speed: **8 m/s**;
- Shift / `Camera Fast`: **50x = 400 m/s**;
- Ctrl / `Camera Slow`: remains **0.25x**;
- vehicle-local authoring fly mode retains its old **4x** Fast multiplier for precision.

At 400 m/s the translated Cloudy layer at 1.2–3.2 km can be reached in a few seconds instead of nearly a minute.

## True flythrough semantics

The translated Unity cloud renderer already contained both global and local cloud semantics, but Heritage's runtime parameter defaulted to global clouds. In that mode the cloud-shell ray origin is pinned to sea level, so changing camera altitude cannot actually put the observer inside the 1.2–3.2 km shell.

CLOUDURP15L makes local/world-space cloud semantics the default. The ray origin now follows the actual camera global X/Y/Z position and opaque scene depth bounds local cloud integration.

A second issue was the existing near-camera density fade: it intentionally removed cloud density for roughly the first 400–1400 m of a ray. That is useful from the ground but would create a huge invisible bubble around a camera inside the clouds. When the camera is physically within the cloud altitude band, CLOUDURP15L switches to a small 12–90 m comfort fade instead. This preserves near-plane stability while allowing the camera to visibly enter, travel through, and leave the volume.

## Expected test

1. Toggle detached free camera.
2. Hold Shift + E to climb rapidly.
3. Cloud base is approximately **1200 m** and cloud top approximately **3200 m** above the engine's sea-level reference.
4. Inside the layer, cloud density should remain around the camera instead of the layer staying visually pinned below you or opening a kilometre-scale clear bubble.
5. At night, inspect the Moon high in the sky and near the horizon under clear/humid/rainy weather. The low Moon should become progressively softer, dimmer and warmer, with a subtle atmospheric halo.
