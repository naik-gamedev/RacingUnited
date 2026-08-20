# WATER06 – Screen-space organic shallow wetness

WATER06 replaces the PERF19/PERF21 duplicate-geometry shallow-water overlay with a screen-space wetness resolve. The authoritative hydrology, connected puddle mesh, distance cadence and precipitation presentation remain unchanged.

## Why

Live testing showed that re-drawing tagged terrain as a second wet-film material was too fragile and could visibly wash out authored material detail. Earlier attempts to inject hydrology into the universal PBR shader were even more invasive. Shallow water also must never expose the 0.5 m hydrology grid as independent visible cards.

## Architecture

The normal entity PBR pass remains the only lighting/material pass for authored geometry. While that pass writes final scene depth, `SurfaceWetnessReceiver` entities write stencil value 1 and every other visible entity writes 0. This gives the final visible surface an inexpensive receiver mask without another geometry pass.

After the material loop, WATER06 copies the current scene depth+stencil to a dedicated single-sample depth-stencil texture. This avoids an OpenGL texture feedback loop and also resolves multisampled depth when MSAA is active. A fullscreen triangle then reconstructs the camera-relative surface position from depth using the inverse view-projection matrix. The pass runs only where stencil equals 1.

The existing 400x400, two-height-layer hydrology atlas is sampled using the reconstructed X/Z position and matched to the reconstructed surface Y. Road/bridge/tunnel layers therefore remain separated. The user-authored `Water_ShorelineBreakup_A8.png` is sampled at two world-space scales and used as a small domain warp only near wetting fronts. Saturated wet regions ignore the breakup texture so neither the texture nor the hydrology grid becomes the visible pattern.

The first WATER06 resolve is deliberately energy-conservative: it multiplies the already-correct PBR result by a restrained wet color factor. It does not re-light the surface, replace the authored normal map, or touch the universal material shader. Strong free-surface reflection remains the responsibility of connected puddle geometry once water reaches the existing 6/8/12 mm presentation thresholds.

## Performance

The wetness pass is skipped completely while the atlas contains no visible water. Stencil rejection occurs before fragment shading on non-receiver pixels. No CPU geometry reconstruction or duplicate terrain submission is required for shallow film. The depth copy is one depth/stencil blit per rendered view only while wetness is active.

## Preserved policy

- 0–25 m hydrology: 30 Hz
- 25–50 m: 20 Hz
- 50–100 m: 6 Hz
- 100–200 m: 2 Hz
- beyond 200 m: 0.5 Hz persistence, not explicit water geometry
- connected explicit puddles: 6 mm near, 8 mm mid, 12 mm far
- steep/fast shallow runoff remains hydrology-authoritative but not explicit free-surface geometry
- WEATHER07C6 rain density/range remains unchanged
