# CLOUDURP15H6B — startup weather module split

H6A correctly restored generic `SurfaceWorld` weather-disabled defaults, but placed Racing United's startup-weather activation directly in `Main.lua`, violating the existing small-coordinator architecture guard.

H6B moves that behavior into `Runtime/StartupWeather.lua`; `Main.lua` only includes it. The game still enables its authored 20% startup weather on load while engine/test worlds remain dry by default.
