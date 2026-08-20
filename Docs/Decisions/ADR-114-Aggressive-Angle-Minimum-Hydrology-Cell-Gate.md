# ADR-114: Aggressive-Angle Minimum Hydrology Cell Gate

## Status
Accepted for WATER14A.

## Decision
Reserve the 0.10 m authoritative adaptive hydrology tier for aggressive angular geometry. Default gates are 55 degrees absolute surface slope or 30 degrees local normal discontinuity.

## Consequences
Ordinary non-mergeable support remains 0.50 m, drastically reducing tiny-cell proliferation on normal roads and broad hillsides. Material or surface-fit discontinuities still block inappropriate coarse merging but do not independently trigger the 0.10 m tier. The 0.10–20 m control-volume architecture and conservative unequal-face virtual pipes remain unchanged.
