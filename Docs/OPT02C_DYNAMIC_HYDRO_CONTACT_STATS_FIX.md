# OPT02C — Dynamic Hydro Contact Statistics Fix

OPT02 successfully moved the tire-fleet regression from the retired WATER14 adaptive solver to the current Dynamic Surface Hydro authority. That migration exposed two stale assumptions and one real CPU fallback performance bug.

## Performance bug

`DynamicSurfaceHydrology::locateMutable()` calls `ensurePage()` for every tire-water contact. Before OPT02C, `ensurePage()` unconditionally called `refreshPageStats()`, which rescanned all 65,536 texels in a 256×256 Hydro authority page even when the page already existed. `applyTireContact()` then performed additional full-page stat scans after water changes.

For the 150-car / 600-tire, 50 ms benchmark this meant 30,000 tire contacts could trigger roughly billions of diagnostic cell visits. The Windows OPT02B regression measured about 13.19 seconds for the wet case.

OPT02C makes page-stat ownership explicit:

- existing pages are no longer rescanned merely because a tire contact locates them;
- each Hydro page maintains a 16-bin water-level histogram;
- tire-contact water changes update wet-cell count, water volume and maximum depth in O(1) work plus a 16-bin maximum search;
- full 256×256 scans remain only at coherent bulk mutations such as page creation, environment/flow steps, uniform lab initialization and reset.

This does not change water quantization, tire clearing, redistribution, flow, rainfall, `.hhyd v15`, or GPU runtime authority.

## Regression alignment

The old fleet regression still expected the retired WATER14 adaptive control-volume count (`<1000`) and at least one 30 Hz hydrology step in a 50 ms run. Current Dynamic Surface Hydro uses one 256×256 authority page per 100 m chunk and a 2 Hz environmental cadence. The synthetic 160 m road spans two chunks, therefore 131,072 authority texels are expected, while a 50 ms tire workload intentionally executes zero environmental flow steps. Dynamic Surface cadence/flow remains tested in the dedicated Dynamic Surface regressions.

## Portable verification

The complete 125-translation-unit `HeritagePhysicsTests` source set was built under Linux with Clang C++20 and executed successfully. The wet fleet workload dropped from the pathological multi-second behavior to approximately the same order of magnitude as the dry tire workload in the portable debug-style build, and all native regressions passed.
