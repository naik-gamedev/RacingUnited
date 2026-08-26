# CLOUDURP15H6A — module-owned startup weather

H6's startup-cloud fix incorrectly changed the engine-wide `SurfaceWeatherDescription` default to enabled. H6A restores the generic engine default to disabled and explicitly enables Racing United's own current/default weather state at module startup. This preserves startup 20% clouds while keeping test worlds and other games free to opt into weather explicitly.
