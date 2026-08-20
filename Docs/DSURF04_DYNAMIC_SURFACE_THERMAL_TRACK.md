# DSURF04 — Dynamic Surface thermal Track authority

**Milestone:** `DSURF04-DYNAMIC-SURFACE-THERMAL-TRACK`

## Goal

Move persistent road/surface temperature out of the global weather scalar and into the same world-anchored, surface-sheet-aware Dynamic Surface page architecture already used by Hydro.

DSURF04 deliberately keeps the first thermal authority raster at **16×16 cells per 6.25 m physical page** (~39.06 cm/cell), matching the DSURF03B Hydro authority pitch. This is a migration/authority resolution, not a visible grid: the renderer rasterizes persistent page state into the normal physical page mip chain.

## Track plane contract

The persistent Dynamic Surface Track plane is RGBA:

- **R:** authoritative `surfaceTemperatureC` — active in DSURF04.
- **G:** adhered rubber — reserved for DSURF05.
- **B:** loose-rubber areal mass — reserved for DSURF05.
- **A:** marble maturity — reserved for DSURF05.

DSURF04 writes only R. Regression coverage requires G/B/A to remain zero so the thermal migration cannot silently consume future rubber channels.

## Authority and identity

Thermal state is keyed by `VirtualPageAddress` (`chunk + sheet + page X/Z`). It therefore follows the real collision-derived surface sheet rather than camera position, a player midpoint, or a single global road temperature.

A road underneath a bridge and the bridge deck can occupy the same X/Z page while retaining independent temperatures. Static page construction chooses the exact DSURF01 surface sheet/support height and marks lower sheets as sheltered when another surface sheet is more than 0.20 m above them.

Thermal pages use the same nearest-real-interest-source cadence policy as Hydro:

- 0–50 m: 30 Hz
- 50–100 m: 20 Hz
- 100–200 m: 6 Hz
- 200–1000 m: 2 Hz
- beyond 1000 m: dormant persistent state

No midpoint between local players is created.

## Environmental evolution

Each valid Track thermal cell evolves toward a local target assembled from:

- ambient temperature;
- solar heating and cloud cover;
- local rain/wetness cooling;
- local evaporation cooling;
- material-dependent thermal time scale / areal heat capacity;
- shelter/sky exposure;
- tire slip-energy deposition.

When Hydro has the same page resident, thermal reads its exact local water depth/moisture cell directly. A weather-output wetness fallback exists only when no local Hydro page is available.

The existing explicit surface-temperature override remains exact and takes priority over simulation. This preserves the public development/test contract without reintroducing a second persistent authority.

## Tire coupling

`SurfaceWorld::localConditions()` now samples Dynamic Surface thermal state for the contacted world position/sheet. Tire thermal and grip paths therefore consume the same local surface temperature that the world evolves.

Each tire contact also submits its measured slip-dissipation power and contact area back to Dynamic Surface thermal state. DSURF04 deposits a bounded fraction of that energy into the contacted Track cell; heat does not leak into another stacked sheet merely because it shares X/Z.

## Legacy weather scalar after cutover

`SurfaceWeatherState::roadTemperatureC` / `SurfaceWeatherOutput::roadTemperatureC` remain temporarily for Lua/API compatibility and as an environmental reference/fallback. They no longer integrate their own persistent road thermal inertia.

Persistent road/surface thermal inertia belongs to Dynamic Surface Track pages. This prevents two independent road-temperature simulations from drifting apart.

## GPU persistence

The DSURF02 GPU physical page pool already owns a Track texture array. DSURF04 now rasterizes Track state alongside Hydro and uploads it into the same physical slot and mip chain.

The migration upload starts at mip2 (64×64 samples per 6.25 m page, ~9.77 cm presentation raster pitch) and is capped by the same **maximum four changed resident pages per frame** staging budget. Hydro and Track maintain separate **per-page revision stamps** so one advancing page does not make every resident page appear dirty. A dry, unchanged Hydro page therefore stops re-rasterizing/uploading while Track temperature may continue evolving independently. Hydro and Track also share a stable `VirtualPageAddress` cadence phase: pages in the slower 20/6/2 Hz bands are naturally distributed across frames instead of being synchronized by the moment they became resident.

F8 diagnostics expose Hydro/Track page uploads plus Track thermal cell count, min/average/max temperature, thermal CPU step time and tire heat-contact count. `scheduled now` is the number of Hydro authority cells whose pages actually advanced on the current update; retired legacy virtual-pipe counters are not shown as live DSURF telemetry.

## DSURF04B performance correction

The first DSURF04 live F8 capture exposed a CPU hotpath rather than a GPU limit: four presentation pages could spend seconds in CPU raster/sample/upload, while first-time thermal page construction was also very expensive. DSURF04B keeps the same physical authority and fidelity but removes the accidental global work:

- `DynamicSurfaceChunk` builds immutable triangle indices per `(sheet,page)` and per X/Z page during the DSURF01 static bake. Hydro/Track static-cell construction no longer searches the full 100 m chunk mesh for each authority texel.
- Hydro caches the immutable presentation support-height raster per page/resolution. A page does not re-run collision-triangle support queries every time water or temperature changes.
- Hydro and Track expose monotonic **per-page revisions**. The renderer no longer compares every page against global `simulationStepCount`, which previously made unrelated resident pages look stale whenever any page advanced.
- Hydro and Track have independent presentation clocks. Track.R GPU presentation is capped at **2 Hz per page** because road temperature changes slowly; this does not reduce the physics thermal cadence or tire-contact sampling.
- The resident page pool selected from the nearest real simulation-interest sources is also the adaptive CPU simulation working set. Persistent CPU state can survive eviction, but nonresident pages do not keep consuming Hydro/Track update time. No split-screen midpoint is introduced.
- Residency is refreshed before Hydro/Track advancement in `SurfaceWorld`, so the current player/vehicle working set is authoritative for that update.

A portable synthetic raster microbenchmark with 18,432 static triangles in one 100 m chunk measured the old 64×64 Hydro support path at about **250 ms/page**. DSURF04B measured about **1.0 ms for the first raster** of the same page and about **0.08 ms for a repeated raster** after support caching. These Linux numbers are diagnostic rather than a Windows FPS promise, but they directly exercise the removed all-triangles-per-texel loop.

## Regression / acceptance coverage

DSURF04 native regression protects these properties:

1. stacked road and bridge sheets remain independent thermal authorities;
2. solar exposure can heat the upper sheet while the sheltered lower sheet remains cooler;
3. tire slip heat raises only the contacted sheet temperature;
4. `SurfaceWorld::localConditions()` returns the Dynamic Surface thermal sample;
5. Track raster R contains temperature while reserved G/B/A remain untouched;
6. the old weather scalar no longer performs an independent persistent thermal relaxation.

## Next boundary

**DSURF05** migrates adhered rubber, loose-rubber mass and marble maturity into Track G/B/A, then removes the old persistent `TrackRubberState` marble spatial storage and the old persistent marble GPU cell cache after equivalent behavior/regressions are established.
