# ADR-068 — Live shadow quality and hardware comparison filtering

## Status
Accepted candidate (SHADOW03)

## Context
Heritage already owned Low/Medium/High/Ultra directional CSM resolutions, but the active renderer defaulted to High (3072) and neither the engine Settings UI nor Launcher exposed those presets. Shadow sampling used a manually compared 3x3 PCF kernel over a nearest-filtered raw depth array, so users also had no runtime choice between crisp and smoother filtered shadow edges.

## Decision
Persist `shadowQualityIndex` and `shadowFilterIndex` in per-module `VideoSettings`. Expose both in the engine Video Settings page and Launcher. The default is Ultra (4096) with Linear filtering. Low/Medium/High/Ultra map to 1024/2048/3072/4096 respectively and are clamped to `GL_MAX_TEXTURE_SIZE`.

Keep the three-by-three PCF footprint, but sample the shadow array through `sampler2DArrayShadow` with `GL_TEXTURE_COMPARE_MODE = GL_COMPARE_REF_TO_TEXTURE` and conventional CSM `GL_LEQUAL` comparison. Nearest uses hard hardware depth comparisons at each PCF tap; Linear uses hardware-filtered comparison results at each tap. Changing either setting is live: filter state changes in place and quality reallocates the existing four-layer depth texture without rebuilding the mesh/shadow programs.

## Consequences
- Racing United defaults to four 4096x4096 D32F cascades when hardware supports that resolution.
- Users can trade memory/GPU shadow raster cost against resolution without source edits.
- Linear is the default because it suppresses texel stair-stepping while preserving the existing PCF footprint.
- Nearest remains available for comparison, debugging and users who prefer a crisper result.
- SHADOW02 layered cascade fan-out and CPU submission batching are unchanged.
- The F8 overlay reports the active shadow resolution and filtering mode for verification.
