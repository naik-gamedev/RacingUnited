# WATER14G — Graded fine-detail transitions and scale-aware seams

WATER14G keeps WATER14F's planarity-driven adaptive hydrology and WATER14E's 0.40 m seam discovery radius, but fixes pathological coarse/fine transitions.

## Simulation topology

Aggressive angular support cells are classified before coarse packing. A short support-grid distance field then limits the maximum accepted coarse span near those cells. The intended progression is local rather than global: 0.10 m detail is surrounded by progressively larger 0.5/1/2/4/8 m control volumes before unrestricted coarsening can return toward 20 m. Broad flat or uniformly sloped regions remain eligible for WATER14F's large best-fit-plane cells.

## Presentation stitching

The 0.40 m value is now a candidate discovery radius only. Actual topology snapping is capped from the smaller participating cell size, and each source edge selects only the nearest coherent opposing seam line. This prevents a 0.10 m cell from ingesting several parallel boundaries that happen to lie inside the broad search window. Coarse edges still collect all fine endpoints that genuinely lie on the selected seam, so coarse/fine T-junctions remain split into shared triangle topology.

## Unchanged

Hydrology depth/flow authority, rain accumulation, tire-water state, explicit-water thresholds, triangle-only presentation, full ring budgets, shoreline breakup mask, and the 3 cm near to 20 cm at 500 m collider-normal presentation offset are unchanged.
