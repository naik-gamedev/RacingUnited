# WATER16 — LiveSurface drainage-basin water presentation

WATER16 retires WATER15J's circular puddle-splat presentation. The conserved adaptive hydrology solver remains authoritative for rain mass, virtual-pipe transport, drainage, evaporation, runoff and tire clearing, but its 0.10–20 m control volumes are no longer permitted to define visible puddle footprints.

## Static drainage catchments

At scene hydrology bake/load, Heritage uses the immutable 0.50 m support raster to construct deterministic downhill catchments. N/E/S/W neighbours are connected only when the measured elevation change is explained by the average support plane. A 15 cm curb therefore fails the continuity test, while a uniformly sloped or banked road remains connected.

Flat plateaus drain deterministically toward decreasing support keys so they do not fragment into one presentation basin per support cell. Each authoritative adaptive control volume is then assigned to the catchment beneath its center.

## Runtime basin state

The normal `SurfaceWeather` film remains a spatially smooth material state. WATER16 subtracts that smooth film plus a 1.5 mm local excess threshold before reducing authoritative adaptive-cell volume into presentation basins.

At 10 Hz, each basin solves one free-surface elevation from its excess volume and the sorted elevations of its support samples. This is a presentation reduction only: no solver volume is moved and the adaptive virtual-pipe field remains authoritative.

All adaptive control volumes inside one basin therefore expose the same free-surface head to graphics. Control-volume boundaries cannot become visible hydraulic discontinuities.

## Rendering

The existing authored road/terrain material draw remains the only visible geometry. Three compact camera/player-centred data clipmaps remain:

- L0 512x512 over 128 m, 10 Hz
- L1 256x256 over 512 m, 3 Hz
- L2 128x128 over 2000 m, 0.5 Hz, strict 1000 m radial cap

The clipmaps store two vertically stacked RG32F basin-head/support layers. They no longer store circular splats and no compute prefilter is performed.

The material shader computes local puddle excess as `basinHead - exact authored fragment Y`. This keeps raised sidewalks/bridges distinct without duplicate water geometry.

`Water_ShorelineBreakup_A8` is sampled in stable world space as deterministic stochastic micro-relief. At very shallow basin excess only low stochastic pockets appear. As the basin free surface rises, nearby patches naturally connect and then fill into ordinary standing water. The texture only erodes the basin excess; it cannot raise water above the solved free surface.

## Performance intent

- no 4096x4096 dynamic water maps;
- no generated visible water mesh;
- no circular or square puddle stamps;
- no adaptive-cell compute blur;
- at most one compact basin atlas refresh per rendered frame;
- zero spatial puddle-atlas work when authoritative depth never exceeds the smooth weather film plus the puddle-excess threshold.

## Known scope

WATER16 is the first LiveSurface basin implementation, not a claim of source-code identity with Madness/LiveTrack. The next refinements may add explicit basin saddle/overtopping metadata, authored drain outlets, and tire queries against the same sub-cell stochastic/local basin depth where useful.
