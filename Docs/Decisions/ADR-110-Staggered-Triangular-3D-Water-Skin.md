# ADR-110 — Staggered Triangular 3D Water Skin

Status: Accepted for WATER12.

Settled visible water must not inherit hydrology-cell topology and must not depend on transient particle coverage. Heritage therefore renders near settled water as an independent staggered triangular presentation lattice sampling the authoritative hydrology field. Square hydrology cells remain purely simulation data.

Screen-space rendering is restricted to microscopic film. The particle-fluid experiment is retained only as future detached-water infrastructure. This deliberately prioritizes a stable, cheap, real 3D surface over a more ambitious but unstable all-particle representation.
