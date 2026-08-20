# PERF21 – Safe Multiplicative Wet Film

PERF20 attempted to sample hydrology directly inside the universal PBR material fragment shader. Live testing produced catastrophic black/grey polygon corruption. PERF21 deliberately backs out that integration and restores the last known-good PBR material shader.

## Shallow film

Shallow wetness is once again a separate, deferred draw of only entities tagged `SurfaceWetnessReceiver`. Unlike PERF19, the pass does **not** compute a second lighting model, environment reflection, or replacement normal. It samples the two-layer hydrology atlas and the user-authored shoreline breakup mask, then outputs only a restrained RGB multiplier.

The pass uses `glBlendFuncSeparate(GL_ZERO, GL_SRC_COLOR, GL_ZERO, GL_ONE)`, so the already-rendered PBR RGB is multiplied by the wetness factor while destination alpha is preserved. Authored base-color, normal, roughness, metallic, AO, shadows and IBL therefore remain the original material result.

## Puddles

The PERF19 connected-puddle split is preserved. Explicit free-surface geometry remains presentation-only and starts at 6 mm near, 8 mm mid, and 12 mm far, with steep/fast runoff excluded from that geometry.

## Safety

Both the universal material program and optional wet-film program now verify `GL_LINK_STATUS`. An unlinked shader is rejected rather than used for undefined scene rendering.
