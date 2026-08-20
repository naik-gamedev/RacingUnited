# ADR-132 — Organic Terrain-Cut Dynamic Surface Water

## Status

Superseded by ADR-133 / DSURF04D2 after live testing. The terrain-cut method is retained here for provenance only; DSURF04C physical authority remains unchanged.

## Context

A 100 m / 64 × 64 / 2 Hz Dynamic Surface authority is cheap enough for the current performance target, but exposing its 1.5625 m depth samples directly in the material shader can still reveal squares, stair steps or triangle-like regions. Raising the physical simulation resolution would trade away the performance win while still failing the architectural requirement that solver topology be invisible.

Earlier WATER-era implicit/canvas/clipmap approaches are retired and must not be resurrected.

## Decision

1. The 64 × 64 Hydro raster is **low-frequency physical authority**, never visible geometry.
2. The renderer reconstructs local water-surface elevation from support height plus authoritative Hydro depth.
3. The exact authored fragment height is subtracted from that water elevation to form visible local depth.
4. A bounded authority envelope prevents coarse support interpolation from inventing significant water mass.
5. World-stable multi-frequency micro-relief perturbs only the final shallow contour by millimetres.
6. Analytical derivative-based coverage smooths the implicit zero-depth contour.
7. Live Dynamic Surface presentation samples mip0 only. CPU-generated 32→1 presentation mips and whole-array live mip regeneration are removed from the active path.
8. There is no generated water mesh, marching-square polygon, simulation-cell quad, camera clipmap, basin canvas or splat ownership in the renderer.

## Consequences

- Physical resolution and visible shoreline resolution are intentionally decoupled.
- The visible contour can follow sub-cell authored road geometry without a high-resolution fluid solver.
- Water remains persistent and world/sheet anchored.
- Coarse state changes may still occur at 2 Hz; temporal interpolation is independent and may be added after spatial acceptance.
- Tile-boundary continuity becomes an explicit presentation acceptance gate rather than something hidden by mip averaging.
