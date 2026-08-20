# WATER14H — Continuous curb-edge refinement

WATER14H fixes discontinuous fine adaptive-water patches along sidewalk and curb edges.

## Problem

WATER14G classified the 0.10 m tier from steep support normals and large normal breaks. A typical sidewalk edge defeats that test because both the road top and sidewalk top are nearly horizontal, while the vertical curb face is intentionally excluded from hydrology as non-upward-facing geometry. Fine cells therefore appeared only in isolated places where source normals happened to contain enough angular noise.

## Change

The adaptive topology prepass now also examines every shared N/E/S/W support edge. For each adjacent pair it computes the elevation delta expected from the average tangent-plane normal and compares that with the actual support-centre elevation delta. Any unexplained vertical discontinuity above the feature threshold is treated as a sharp edge and both sides naturally classify into the 0.10 m feature tier when visited.

The default feature threshold is max(2.5 cm, 1.25 × adaptive surface-fit error), which is 2.5 cm with the current configuration. This catches ordinary curb/sidewalk steps while ignoring continuous road slope and centimetre-scale collider noise.

## Preserved behavior

- WATER14F best-fit-plane coarsening remains authoritative away from features.
- WATER14G local graded transition halo remains active around fine detail.
- 40 cm seam discovery remains presentation-only and actual seam snapping remains scale-aware.
- Hydrology volume, rain accumulation, tire-water interaction, explicit-water thresholds, shoreline breakup mask, and 3–20 cm distance-adaptive collider-normal presentation offset are unchanged.
