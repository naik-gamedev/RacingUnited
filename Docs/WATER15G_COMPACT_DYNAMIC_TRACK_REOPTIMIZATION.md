# WATER15G – Compact Dynamic Track Reoptimization

## Why WATER15F was retired

The 4096/2048/1024/512 presentation experiment consumed hundreds of MiB of GPU storage, repeatedly rasterized large offscreen targets, and made each wet receiver fragment walk every clipmap level with layer-aware four-tap reconstruction. It magnified the adaptive solver's rectangular ownership without creating new physical detail and caused catastrophic frame time on the reference scene.

WATER15G keeps the useful architecture: conserved hydrology remains authoritative, water presentation stays inside the authored road/terrain PBR draw, and no visible water mesh exists. Only the presentation cache and optical policy are replaced.

## Compact clipmaps

- L0: 1024x1024 over 128x128 m, 12.5 cm/texel, 15 Hz.
- L1: 512x512 over 512x512 m, 1.0 m/texel, 5 Hz.
- L2: 256x256 over 2000x2000 m, 7.8125 m/texel, 1 Hz, strict 1000 m radial visibility cap.
- Two RG32F vertical surface layers remain for bridge/tunnel separation.
- Approximate color-state storage is ~22 MiB rather than the ~340 MiB WATER15F RG32F color chains.

The material shader samples one clipmap almost everywhere. Two levels are sampled only in narrow L0/L1 and L1/L2 transition bands. The compact state texture keeps mip filtering enabled (`GL_LINEAR_MIPMAP_NEAREST`) so explicit `textureLod` calls actually address L1/L2 instead of accidentally reading L0.

## Refresh policy

Adaptive topology-count changes no longer bypass presentation cadence. A changing solver may update at its authoritative rate while the atlas refreshes at its own 15/5/1 Hz policy. Recenter thresholds are 8/32/128 m. Once a dry atlas has no visible water, advancing hydrology time does not rebuild it until water returns.

## Visual policy

The shader still derives physical local depth from reconstructed hydraulic head minus exact authored fragment height. Near/mid support matching is tightened so curb-height surfaces are not casually borrowed.

Shallow rain is treated as wet material first and its optical strength is driven directly by authoritative physical depth. The shoreline breakup texture is only allowed to erode the visible edge of shallow standing water; it can never increase apparent depth above the physical depth. This prevents a few hundredths of a millimetre of rain from being amplified into bright square puddles. Strong standing-water Fresnel, flattened free-surface normals, and pooled-water optics begin only after several millimetres of actual accumulation.

## CPU presentation simplification

Hydrology corner reconstruction now uses directly connected virtual-pipe neighbours only. The WATER15F second-hop graph walk was presentation-only and multiplied work for every corner and clipmap refresh. Authoritative flow, volume, drainage, evaporation and tire interaction are unchanged.

Additional frame-budget rule
- At most one clipmap rebuild is submitted per frame; a rotating cursor prevents synchronized refresh spikes.
- Sub-2.2 mm film uses a two-sample hardware-filtered state lookup; exact four-corner bilateral reconstruction is reserved for pooled near/mid water.
