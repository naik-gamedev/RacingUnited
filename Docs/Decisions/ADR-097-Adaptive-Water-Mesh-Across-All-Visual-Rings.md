# ADR-097 - Adaptive Water Mesh Across All Visual Rings

**Status:** Accepted for PERF12.

## Context

PERF11 separated authoritative hydrology cadence from water presentation cadence and introduced adaptive 2/4/8/16 m patches in the 100–200 m ring. Live debug visualization showed that the 0–100 m field still remained a dense 0.5 m checkerboard on broad planar parking-lot geometry. That preserved information the renderer did not need and prevented the intended plane-based optimization from helping the most commonly visible area.

## Decision

Use the adaptive planar collector for every explicit-water cadence ring from 0–200 m.

The authoritative hydrology grid remains 0.5 m. Presentation uses a dense local merge hierarchy and recursively permits 1, 2, 4, 8 and 16 m patches. The 0–25 m and 25–50 m regions begin at 1 m and cap merges at 8 m. The 50–100 m and 100–200 m regions begin at 2 m and cap at 16 m, while retaining 0.5 m fallback wherever a coarse block cannot be represented safely. Any region that fails completeness, wet topology, material or support-plane compatibility falls back toward the underlying 0.5 m records.

Adjacent presentation slices are cross-faded per fragment. A separate 100 m fine/coarse LOD class is removed because all slices now use the same representation policy. The 50–100 m 6 Hz cadence region is divided into two independently phased presentation caches and the 100–200 m 2 Hz region into four, spreading adaptive collection/upload work across frames without changing any point's authoritative cadence.

Depth and flow variation are presentation criteria, not geometry-authority criteria. Small rain-film gradients are tolerated so a flat support plane can actually collapse; meaningful discontinuities still prevent merging.

## Consequences

- Flat parking lots, broad roads and similar planar wet surfaces can collapse to a handful of large GPU records even near the player.
- Puddle edges, curbs, drains, sharp cambers and mixed materials retain fine fallback geometry.
- Debug presentation visibly exposes the same patch sizes the normal water pass consumes.
- Authoritative hydrology cadence, cell size, mass transport, tire coupling and multiplayer interest-source rules are unchanged.
- Adaptive collection remains CPU work and should continue to be profiled. PERF12 uses dense local arrays instead of a hash-heavy hierarchy and phases mid/far radial slices to reduce individual refresh spikes. A future GPU/state-atlas presentation path may replace repeated CPU aggregation if collection itself remains a dominant hitch source.
