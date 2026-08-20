# ADR-101 – Sub-Cell Hydrology Shoreline Reconstruction

## Decision
Keep the authoritative water solver cell-based, but reconstruct explicit-water coverage continuously inside the final 0.5 m presentation cells. Do not make visible water appear/disappear one whole hydrology cell at a time.

Fine visual records expose four corner water-depth samples derived from compatible neighbouring hydrology centres. The connected mesh interpolates the depth field and the shader evaluates the explicit-water threshold against it. A world-space artist-authored breakup mask perturbs only the threshold near the shoreline.

## Rationale
Increasing global mesh density would waste work in deep/flat puddle interiors and still not solve whole-cell activation. Sub-cell depth interpolation makes the shoreline continuous while retaining the performant adaptive interior mesh and preserving hydrology as the single authority for water amount/flow.
