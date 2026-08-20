# DSURF04B — Dynamic Surface performance hotpath

**Milestone:** `DSURF04B-DYNAMIC-SURFACE-PERFORMANCE-HOTPATH`

DSURF04B is a performance correction to DSURF04, not a reduction in Hydro or thermal physics fidelity. It keeps the 16×16 authority state per 6.25 m surface-sheet page and the same nearest-real-source 30/20/6/2 Hz physics cadence.

## What was wrong

The DSURF04 live capture was CPU-bound while the GPU was essentially idle. The largest error was presentation support generation: every 64×64 texel searched every static triangle in its 100 m chunk. Global simulation-step counters also caused unrelated pages to be re-rasterized after any Hydro/Track update. Static authority-page construction repeated the same broad triangle search, especially visible as a large thermal startup cost.

## Hotpath contract

- Static geometry is indexed once into page-local triangle buckets.
- Immutable support height is cached after its first presentation raster.
- Hydro/Track renderer dirtiness is page-local through monotonic revisions.
- Track.R presentation is capped at 2 Hz/page; authoritative physics thermal state remains 30/20/6/2 Hz according to nearest source distance.
- Only currently resident nearest-real-source pages consume Hydro/Track simulation CPU. Evicted CPU page state may persist for later reuse but is dormant.
- No player midpoint, camera-centred simulation square, generated water mesh, or legacy WATER15-18 state is reintroduced.

## Fairness and starvation

The renderer still stages at most four changed pages per frame. Candidates are ordered by oldest refresh first and then distance so continuously changing pages closest to the camera cannot permanently starve other changed resident pages. Hydro and Track have independent clocks.

## Diagnostic benchmark

A portable 18,432-triangle synthetic 100 m chunk benchmark measured the previous 64×64 Hydro/support raster at ~250 ms per page. With page indexing and support caching, the first raster measured ~1.0 ms and a repeated raster ~0.08 ms. This benchmark isolates the corrected CPU algorithm; live Windows F8 remains the acceptance authority.

## Acceptance

The next live F8 capture should show the Hydro/Track `CPU raster/sample/upload` figure collapsing from the seconds range. Thermal first-population cost should also fall sharply because static cell construction is page-local. If steady-state Dynamic Surface CPU remains material after warm-up, optimize that measured path next rather than lowering authority fidelity blindly.
