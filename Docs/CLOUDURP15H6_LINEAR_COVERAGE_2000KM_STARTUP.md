# CLOUDURP15H6 — linear coverage, 2000 km weather field, startup clouds

Authored cloud coverage is now treated as an approximately linear fraction of the regional sky using a calibrated coherent-noise percentile selector. Sparse selected regions receive enough local coverage to form visible volumetric bodies, so 1% is not numerically erased by density squaring. The startup SurfaceWeather state is enabled by default, making the scene's default 20% cover visible immediately rather than only after the UI writes weather once.

The cloud ray cap is 2000 km and the GPU regional weather map spans 2000 km (±1000 km) at 256². The spherical Earth/cloud-shell intersection remains physically authoritative, so actual visible tangent distance is still constrained by geometry rather than only by the cap.
