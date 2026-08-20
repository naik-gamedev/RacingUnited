# WATER08 - Continuous Near Puddle Contour

WATER08 removes the visible rectangular hydrology footprint from standing water in the 0-50 m presentation range. The authoritative 0.5 m hydrology remains unchanged.

## Near-field presentation

The two 0-50 m cadence rings collect full-resolution hydrology samples including dry neighbours. `WaterContourMesher.hpp` treats water depth as a scalar field and clips upward-facing triangles against the standing-water iso-threshold. Edge vertices are interpolated between wet and dry samples, so the shoreline moves through a cell instead of switching a whole square on/off.

A deterministic world-space millimetre-scale threshold perturbation breaks residual lattice regularity without camera swimming. The existing user-authored shoreline breakup texture remains a finer fragment-stage detail. Standing-water geometry uses an upward free-surface normal and suppresses tiny cell-to-cell solver flow before wave-direction normalization, preventing the physics lattice from reappearing as lighting/ripple facets.

## Distance split

- 0-25 m: continuous scalar contour, refreshed on the existing 30 Hz presentation cadence.
- 25-50 m: continuous scalar contour, existing 20 Hz cadence.
- 50-100 m: existing adaptive basin mesh, 6 Hz.
- 100-200 m: existing adaptive basin mesh, 2 Hz.
- Beyond 200 m: no explicit puddle geometry; hydrology persists at background cadence.

This is intentionally a presentation change only. Hydrology mass, flow, surface wetness, tire-water interaction and multiplayer-authoritative state remain in the existing solver. Once the visual contour is accepted, the same extraction algorithm can be migrated to an OpenGL 4.6 compute/indirect path without changing hydrology semantics.
