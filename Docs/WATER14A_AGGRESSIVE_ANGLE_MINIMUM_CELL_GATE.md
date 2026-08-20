# WATER14A – Aggressive-Angle Minimum Cell Gate

WATER14A keeps the authoritative adaptive 0.10–20 m control-volume hydrology introduced by WATER14, but changes when the most expensive 0.10 m tier is allowed.

## Policy

The solver may subdivide one immutable 0.50 m terrain-support sample into 0.10 m authoritative water cells only when either:

- the absolute support slope is at least **55 degrees**, or
- a neighboring support normal differs by at least **30 degrees**.

Material changes, normal differences below 30 degrees, ordinary road camber, broad hillsides, crowns/gutters and surface-fit residuals may still prevent larger cells from merging, but they now fall back to **0.50 m** instead of automatically creating twenty-five 0.10 m cells.

The large-to-small adaptive merge hierarchy is unchanged, so coherent areas may still use intermediate sizes up to 20 m. Water volume and variable-face virtual-pipe transport remain authoritative and conservative.

The two angular thresholds are explicit `SurfaceHydrologyDescription` settings (`adaptiveMinimumCellSlopeDegrees` and `adaptiveMinimumCellNormalBreakDegrees`) so they can be calibrated later without changing topology code.
