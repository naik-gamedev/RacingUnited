# ADR-103 – PBR-Integrated Tagged Surface Wetness

## Status
Accepted. Supersedes ADR-102 only for the shallow-film rendering mechanism.

## Context
A separate wet-film overlay successfully avoided free-standing shallow-water cards, but it re-lit the surface from geometric normals. Live rain testing showed the overlay visually replacing normal-map detail with broad, smooth reflection streaks. Multiple state-isolation fixes also made the duplicate-pass architecture unnecessarily fragile.

## Decision
Keep hydrology authoritative and keep explicit connected geometry only for accumulated puddles. Render shallow film/runoff as a material response in the existing PBR shader, gated by the `SurfaceWetnessReceiver` tag.

The shader samples the camera-near two-layer hydrology atlas and user breakup mask, then modifies the already-resolved material base colour and roughness. The authored tangent-space normal map remains the lighting normal and is never replaced by a wetness-specific normal.

## Consequences
- No duplicate shallow-water geometry or overlay shader.
- No extra blend/depth state transition for wet film.
- Dry and wet surfaces preserve identical authored texture detail.
- Wet reflections naturally inherit the material normal map because the ordinary PBR environment path sees the reduced roughness.
- Wetness remains opt-in, presentation-only, and cannot affect vehicles/buildings unless explicitly tagged.
