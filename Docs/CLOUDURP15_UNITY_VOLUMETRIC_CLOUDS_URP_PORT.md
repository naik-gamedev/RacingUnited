# CLOUDURP15 — UnityVolumetricCloudsURP → Heritage Engine

## Source and license

This renderer is a Heritage Engine translation of **jiaozi158/UnityVolumetricCloudsURP**, which in turn ports Unity HDRP's volumetric-cloud rendering to URP.

Upstream project: `https://github.com/jiaozi158/UnityVolumetricCloudsURP`

The upstream MIT license is retained at:

`ThirdParty/UnityVolumetricCloudsURP/LICENSE`

The source Worley and Perlin volume images used for the Heritage conversion are retained in the same ThirdParty folder for provenance.

## Rendering translation

The live Heritage implementation is in `SkyRenderer.cpp/.hpp` and translates the upstream cloud pipeline into GLSL/OpenGL rather than emulating its appearance with Heritage's former flat procedural slab.

Implemented source behavior includes:

- Earth-scale spherical cloud shell; Cloudy preset layer at 1200–3200 m.
- 128³ Worley shape volume and 32³ Perlin erosion volume with 3D mip chains.
- Cloudy density/erosion/AO LUT generation, including endpoint density zeroing and `1 - AO` storage used by the Unity setup layer.
- 32 primary samples, adaptive empty-space stepping and 250 m maximum step.
- Two light samples, dual Henyey–Greenstein phase response, powder and multiple scattering.
- Shape erosion and optional micro erosion.
- Source-style perceptual transmittance applied to the half-resolution raymarch result before upscale.
- 0.5× cloud rendering resolution with bilinear upscale by default.
- Optional source-style 7×7 bilateral upscale, including separate transmittance filtering for temporal preparation.
- Source-style temporal history built from already-composited scene RGB plus cloud transmittance, five-neighbour history clamp and camera-motion reprojection.
- Global and local volumetric-cloud semantics. Local clouds use opaque scene depth to cap ray length and therefore can appear in front of distant geometry while remaining behind nearer opaque geometry.
- Single-sample and MSAA opaque-depth handling.
- Half-resolution cloud-depth output and an optional reversed-Z merge back into Heritage scene depth.
- Optional physically based sunlight attenuation branch using the source atmosphere constants, mapped into Heritage's environment pipeline.
- Cloud shadow tracing and two 3×3 Gaussian filter passes. Heritage multiplies the resulting cloud transmission with its existing cascaded geometry shadow instead of replacing a Unity directional-light cookie.

## Cloudy preset parameter transfer

Several Unity inspector values are transformed before reaching the HLSL. Heritage stores the **shader-side** values where appropriate.

Notable values:

- Layer bottom/top: 1200 / 3200 m.
- Primary/light steps: 32 / 2.
- Authored density multiplier: 0.40; Unity uploads `0.40² × 2 = 0.32`, so Heritage raymarch density multiplier is 0.32.
- Shape factor: 0.90; shape scale: 5.
- Erosion factor: 0.80; erosion scale: 107.
- Micro-erosion Cloudy variant: shape 0.875, erosion 0.90 / scale 75, micro erosion 0.65 / scale 300.
- Authored altitude distortion: 0.25; shader-side value 0.0625.
- Authored multi-scattering: 0.50; Unity setup yields shader-side 0.525.
- Temporal accumulation: 0.95.
- Cloud shadow distance: 8000 m; shadow working texture: 256².

## Heritage-specific equivalents and extensions

Unity-specific renderer infrastructure is represented by native Heritage equivalents rather than copied literally:

- URP RenderGraph passes → Heritage framebuffer/pass ordering.
- `BeforeRenderingTransparents` → post-opaque Heritage cloud pass.
- Unity camera color/depth handles → Heritage scene color/depth captures.
- Unity directional-light cookie cloud shadows → Heritage cloud-transmission texture multiplied with existing CSM sunlight.
- Unity environment/probe ambient data → Heritage environment cubemap sampling.

Upstream's custom cloud-map path is marked WIP and currently uses hard-coded cloud coverage data. Heritage deliberately maps its existing deterministic regional weather field into that role, so Scene → Weather coverage/rain controls remain authoritative and spatial weather continues to agree with the precipitation radar/rain/hydrology systems.

## Validation performed for this archive

- The embedded GLSL stages were extracted from the C++ source and compiled **and linked** against a real headless Mesa OpenGL core context. Mesa exposes OpenGL 4.5 in the validation container, so only the `#version 460` directive was lowered to `450` for that syntax/link validation; shader logic was not rewritten.
- `SkyRenderer.cpp` passes a C++20 syntax-only translation-unit check with a generated GL declaration shim.
- The converted Heritage volume files were header/size checked:
  - Worley: `HVOL v1`, 128×128×128, R8; SHA-256 `fc4c6ea5c56383a15e7abd93309f7c6fe9925e2542d5b16c2596e3ba0fba9ec7`.
  - Perlin: `HVOL v1`, 32×32×32, R8; SHA-256 `7e4d6ad5d4fc5cb4079901d2f35fc0aceb165c27d69fddbf5fa22faac78f33cf`.

A native Windows/MSVC run is still the definitive runtime test. The supplied project archive does not contain the Windows vendor GLAD/GLFW headers used by its normal build environment, and the Linux validation container does not provide Visual Studio/PowerShell. Use `Tools/CLOUDURP15_BuildAndRun.cmd` on the normal Racing United Windows checkout; it performs the project's existing validation/build/test/run sequence.

## Intentional status notes

- Orthographic camera support is not added; the upstream project itself does not support orthographic cameras.
- Cloud shadows are integrated with Heritage CSM rather than reproducing Unity's light-cookie replacement behavior, because preserving ordinary geometry shadows is the correct engine-native equivalent.
- Heritage regional cloud-map/weather integration is intentionally more complete than upstream's current WIP custom cloud-map hook.

## CLOUDURP15A validation hotfix

The first Windows attempt reached the repository safety-net and stopped before MSVC because three validations still described superseded renderer architecture. CLOUDURP15A repairs those checks without relaxing their constraints:

- `EntityMeshRenderer.cpp` exceeded the existing CLEAN05 `<1200` orchestrator guard by 15 lines. `requestHotReloadPoll()` and `loadedAssetCount()` now live in `EntityMeshAssetCache.cpp`, reducing the root renderer to 1196 lines while keeping the original guard unchanged.
- The WEATHER12G assertion required the retired bespoke cloud FXAA/tent-alpha-blur implementation. It is replaced by a behavior-oriented CLOUDURP15 assertion covering the spherical Earth shell, source noise assets, 32-step bounded march, source temporal accumulation, bilateral/transmittance paths, local-cloud depth output and filtered cloud shadows.
- The WEATHER13H assertion required retired `referenceCloudLuma` / `ambientFromAtmosphere` strings. It now verifies that the existing WEATHER13B shared atmospheric authority remains intact and that the translated cloud renderer consumes the shared environment map and regional weather field.

These changes affect repository ownership/validation only; they do not change cloud density, lighting, temporal accumulation, weather authority or hydrology.

## CLOUDURP15D true-overcast coverage transfer

Live testing of the completed port showed that 100% Scene → Weather → Cloud Coverage still left large clear holes. The renderer had been given the camera-local regional FBM cloud value as `uCloudCover`, so the authored 1.0 control never actually reached the volumetric coverage logic as a global authoring value.

CLOUDURP15D separates the authored scene coverage from camera-local regional coverage. The regional map remains untouched for weather/radar/hydrology. For volumetric presentation only, authored coverage 0.82..1.0 smoothly fills regional cloud-map holes and expands the low-frequency Worley formation threshold up to 2.35x at exactly 1.0. Final density/extinction is not multiplied by that expansion; the result should be more and larger connected versions of the existing source-shaped clouds rather than a uniformly dark slab. The shadow tracer mirrors the same transfer.
