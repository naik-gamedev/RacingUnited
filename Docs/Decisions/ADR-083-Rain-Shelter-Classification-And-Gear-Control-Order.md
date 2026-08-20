# ADR-083 — Rain Shelter Classification and Gear Control Order

## Status
Accepted for WEATHER06G.

## Rain presentation
WEATHER06F used a broad nearby-surface test to decide whether the camera was
under precipitation cover. On steep LiDAR terrain, ordinary uphill ground could
be above camera eye height within that radius and suppress the entire rain pass.
Shelter classification is now almost columnar at the camera X/Z and requires a
distinct surface 1.20–20 m above the eye. The visible rain remains world-space;
no camera-attached streak texture is reintroduced.

## Input presentation
The Gears settings category is a driver-facing physical control sequence rather
than an alphabetic action list. Its order is Shift Up, Shift Down, Clutch,
Neutral, Reverse, Gear 1 through Gear 24. Clutch is bindable but has no factory
default.
