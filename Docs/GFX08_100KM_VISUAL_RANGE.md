# GFX08 — 100 km visual range

Heritage Engine now uses a 100,000 metre default far clip plane for both normal
and triple-monitor/off-axis projection paths.

## Depth precision

A conventional 0.10 m near plane combined with a 100 km far plane is a poor fit
for a normal 24-bit forward-Z buffer. GFX08 therefore also changes the scene
depth convention to reversed-Z and upgrades the engine-owned scene depth
attachments to 32-bit floating-point depth + stencil.

- Near clip: 0.10 m
- Far clip: 100,000 m
- Depth clear value: 0
- Depth comparison: `GL_GREATER`
- Scene depth format: `GL_DEPTH32F_STENCIL8`
- Sky renders at reversed-Z far depth

This is a projection/visibility foundation only. It does **not** force every
asset inside 100 km to render at full detail forever. Future scene streaming,
LOD, vegetation impostors, occlusion and distance culling can reduce cost while
keeping the same 100 km horizon.
