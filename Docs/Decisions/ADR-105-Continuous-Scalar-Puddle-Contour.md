# ADR-105: Continuous scalar-field puddle contour

## Decision

Near standing water is no longer presented by rectangular adaptive hydrology leaves. For 0-50 m, Heritage samples the authoritative hydrology field at full 0.5 m resolution and extracts the visible free-surface boundary as a continuous piecewise-linear iso-contour. Farther standing water retains adaptive basin presentation until the contour path is proven and moved GPU-side.

## Rationale

Hydrology cells are an efficient simulation discretization but are not an acceptable visible primitive. Shader alpha breakup can perturb a boundary, but it cannot hide a rectangular support mesh when the whole adaptive leaf is above threshold. Extracting the iso-boundary converts cell data into a continuous shoreline while preserving the solver.

## Consequences

Near presentation collects dry neighbour samples, so CPU collection/packing cost may rise. Existing cadence caches bound this work to 30/20 Hz rather than render rate. If the look is accepted, compute-shader contour extraction and indirect drawing are the intended optimization path.
