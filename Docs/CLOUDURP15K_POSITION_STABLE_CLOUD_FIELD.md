# CLOUDURP15K - Position-stable regional cloud field

## Symptom

Visible volumetric clouds could appear or disappear when the camera changed world position, even when time/weather had not changed enough to justify a cloud forming or dissolving.

## Root causes

1. The volumetric cloud shader passed absolute world X/Z into `regionalWeather()`, but that helper already adds `uRegionalWeatherCameraOffsetXZ` (camera minus regional texture center). The camera position was therefore applied twice to the regional occupancy lookup. The shape noise stayed world-anchored while the regional cloud mask slid with camera motion.

2. The CPU regional-weather texture covered 2,000 km at 512x512, so one texel represents exactly 3,906.25 m. The texture center was nevertheless snapped in arbitrary 5,000 m increments. Every recenter therefore rebuilt the texture on a differently phased sampling lattice, changing bilinear interpolation across the whole visible region at the instant a 5 km boundary was crossed.

## Fix

- Volumetric-cloud and cloud-shadow regional weather lookups now use camera-relative X/Z. Absolute world X/Z remains used for the stationary 3D shape/erosion noise.
- Regional map recentering now snaps by exactly one weather texel (`diameter / resolution`) instead of 5 km. Recentered maps therefore preserve the same world-space sample lattice; overlapping samples shift by an integer texel and remain numerically continuous under GL_LINEAR sampling.

With the current 1,000 km half-range and 512 resolution, the recenter quantum is 3,906.25 m.

## Expected result

Camera translation should no longer make cloud occupancy slide, and crossing a regional-map recenter boundary should no longer make existing cloud bodies abruptly appear/disappear. Normal cloud evolution from wind advection and temporal sampling remains active.
