# CLOUDURP15AI – 800 km Far Cloud Horizon

## Why this exists
The cloud shader permitted a 2,000 km ray in theory, but its primary marcher only inspected at most 32 × 250 m = ~8 km of the cloud-shell segment. Near the horizon, where the ray can remain inside the cloud layer for a very long distance, most distant formations were never sampled.

## Changes
- original 32-step, 250 m maximum near-cloud march is preserved exactly for close quality;
- adds a fixed-cost 28-sample sparse far-cloud search over the remainder of the ray;
- local cloud maximum search distance is 800 km, exactly 100× the old ~8 km inspected span;
- local/world clouds now intersect a flat altitude slab (650–4300 m) instead of being clipped by an artificial spherical-Earth cloud horizon;
- world-anchored noise and the existing 1000 km regional-weather half-range continue to drive distant cloud placement;
- far samples use cheaper/coarser density evaluation so range does not scale cost linearly with distance.

## Performance intent
The horizon is 100× farther, but shader cost is not 100× higher. The extra budget is a fixed 28 cheap far samples only for rays with remaining cloud volume after the existing near pass.
