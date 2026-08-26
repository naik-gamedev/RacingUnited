# OPT02B — Hydrology Validation Ownership Fix

OPT02 extracted precipitation/shelter column queries from the old monolithic `SurfaceHydrology.cpp` into `SurfaceHydrologyCover.cpp`. A WEATHER06H architecture check still searched `SurfaceHydrologyTiles.cpp` for `SurfaceHydrology::hasPrecipitationCoverAbove()`, causing a false validation failure before the engine could launch.

OPT02B changes the validator to follow the new ownership. No runtime behavior changes.
