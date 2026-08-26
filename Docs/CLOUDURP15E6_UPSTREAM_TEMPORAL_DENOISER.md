# CLOUDURP15E6 — Upstream temporal denoiser restoration

CLOUDURP15E6 removes the experimental Heritage cloud-TAA stack introduced after VCLOUD01 and restores the temporal denoising architecture used by `jiaozi158/UnityVolumetricCloudsURP` (itself HDRP-derived).

## Authoritative temporal path

1. Raymarch the current cloud lighting/transmittance at the cloud render resolution.
2. Upscale the cloud result to full resolution and compose it over the staged current scene colour. Alpha stores current cloud transmittance.
3. Reproject the previous resolved camera-colour history using opaque scene depth for local clouds and the far plane for global clouds.
4. Build the upstream 5-pixel current-frame RGB box (center + four cardinal neighbours).
5. Point-sample and clamp the reprojected previous RGB into that current box.
6. Use the upstream default temporal accumulation factor `0.95`, reduced by screen-space camera velocity.
7. Write the resolved RGB directly; keep current transmittance in alpha as the clear-cloud mask for the next frame.
8. Ping-pong the resolved full-resolution history.

The direct shader output is mathematically equivalent to the source shader returning previous RGB plus blend intensity and then using `SrcAlpha / OneMinusSrcAlpha` over current camera colour.

## Removed legacy experiments

The 20% stable / 60% mild / 50:1 strong classifier, stochastic-noise classification, low-frequency reactive rejection, sigma clipping, cloud-depth temporal reprojection, and GL_LINEAR history override are no longer part of the cloud TAA path.

History is point-clamped, matching the source implementation. The raymarch also restores the upstream integration-jitter semantics: one random scalar per pixel/frame, a raw 0..1 initial distance offset, and that same scalar only on the first relative step rather than multiplying the initial offset by the entire ray step.

CELESTIAL04 cloud-shadow receiver behavior is not merged into temporal history; it is applied after the resolved camera colour is restored so moving ground shadows cannot ghost through cloud TAA.
