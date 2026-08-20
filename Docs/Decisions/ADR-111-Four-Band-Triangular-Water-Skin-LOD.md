# ADR-111: Four-Band Triangular Water Skin LOD

## Status
Accepted

## Decision
Use one continuous 3D settled-water presentation strategy from 0–500 m based on a staggered triangular lattice sampled from the authoritative shallow-water field.

## Presentation bands
- 0–50 m: 0.10 m spacing
- 50–100 m: 0.50 m spacing
- 100–200 m: 1.0 m spacing
- 200–500 m: 2.0 m spacing

## Rationale
The user preferred the WATER12 result but requested visibly different water element sizes by distance. The triangular skin remains independent from the square hydrology topology, while the distance bands reduce geometric density farther from the camera without falling back to chessboard quads.
