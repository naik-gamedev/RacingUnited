# PBSKY01A — Physical Sky Migration Validator Hotfix

PBSKY01 moved visible atmospheric scattering from the retired CLOUDURP15 artistic sky shader into the Heritage-native UnityPhysicallyBasedSkyURP-derived LUT pipeline. Four historical CLOUDURP15 validation predicates still searched only the retired shader owner and therefore rejected the migration before compilation.

PBSKY01A updates those safety checks to inspect `SkyRendererPbrAtmosphereShaders.cpp` and validate the physical equivalents. It also restores two night-sky presentation details that were unintentionally lost during migration: peak-only micro-bloom for the HDR BC6H celestial map and post-tone-map compositing of the authored Moon texture. Star and Moon extinction remain owned by the new physical atmosphere.

Existing volumetric clouds, water/hydrology, physics, shadow quality, PERF05 link-status caching and PERF06A diagnostics are unchanged.
