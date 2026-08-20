# ADR-104 – Screen-space depth-reconstructed shallow wetness

## Status
Accepted for WATER06 prototype validation.

## Context

Hydrology is authoritative on a 0.5 m field, but shallow rain film must not inherit that field's visible topology. Duplicate authored-geometry wet-film passes proved fragile in live OpenGL state/material integration, while direct hydrology sampling inside the universal PBR shader caused unacceptable scene-rendering regressions. Connected free-surface geometry is appropriate only after water actually pools.

## Decision

Render shallow wetness as an isolated screen-space resolve after the normal PBR entity pass and before connected water/weather presentation.

The normal PBR pass writes a stencil ownership bit: `SurfaceWetnessReceiver` geometry writes 1 and other visible entity geometry writes 0. The active scene depth+stencil is copied/resolved into a dedicated sampleable `GL_DEPTH32F_STENCIL8` texture. A fullscreen pass reconstructs camera-relative visible-surface position from depth and inverse view-projection, samples the existing two-layer hydrology atlas at that position, and runs only where the scene stencil equals 1.

The user-authored shoreline breakup texture is used as a small world-space domain warp near the wetting front. Saturated wet areas become uniform so the texture itself never becomes a visible decal pattern. The first resolve only multiplies the existing PBR color and does not calculate replacement lighting, normals, or reflections.

Connected explicit water remains responsible for pooled free-surface appearance at the existing presentation depth thresholds.

## Consequences

- No duplicate shallow-water geometry exists to reveal 0.5 m cells, quads, or detached runoff ribbons.
- The universal material shader stays hydrology-free.
- Cars and other foreground entities are protected by the visible-surface stencil ownership mask.
- Road/bridge/tunnel separation remains controlled by hydrology atlas height layers.
- MSAA depth can be resolved by the depth/stencil blit without adding a second shader variant.
- The pass introduces one depth/stencil blit and one stencil-limited fullscreen draw per active wet view.
- Future wet-surface reflection can be added as a separate, testable screen-space stage rather than modifying the base PBR shader.
