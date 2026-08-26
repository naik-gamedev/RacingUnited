# CELESTIAL04 — Dedicated Cloud Shadow Receiver + Weak Cloud TAA

CELESTIAL04 responds to visual validation showing that repeated cloud-shadow
strength changes did not alter the scene. The detailed cloud cookie was being
threaded through the entity-material path, which is not a reliable global
receiver for every opaque scene surface.

## Single shadow authority, new receiver wiring

The existing VCLOUD01 256×256 optical-depth cookie remains the only detailed
cloud-shadow field. It still follows Heritage's continuous astronomical key
light: Sun by day, Moon by night, with continuous twilight transition.

After opaque geometry is rendered and scene depth is captured, Heritage now
runs one lightweight fullscreen receiver pass before volumetric-cloud
composition. The pass reconstructs each opaque pixel's camera-relative world
position from depth, projects it onto the camera-centred cloud-shadow receiver
plane along the same celestial-light direction, samples the existing cookie,
and multiplicatively attenuates the already-lit destination.

This makes roads, terrain, buildings, cars and any future opaque material share
the same cloud-shadow receiver without duplicating shadow textures or requiring
every material implementation to wire the cookie separately. Sky pixels are
rejected from reversed-Z depth and remain untouched.

The multiplication preserves a bounded share of sky/ambient light so cloud
shadows remain soft rather than black decals. Dense daylight cloud shadows are
stronger than Moon shadows; the remaining light is slightly cooler under dense
cloud.

## 10% cloud TAA

Cloud temporal history is set to a maximum 10% contribution (five times the prior 2% setting).
Measurements and reprojection remain active, but temporal filtering now provides
stronger shimmer stabilization while remaining heavily biased toward the current
frame, so cloud structure should stay responsive rather than smear heavily.

## Lunar appearance

Moon cloud scattering keeps the physically shaped narrow forward lobe and
higher-order interior fill introduced by CELESTIAL03, with stronger cloud-only
lunar exposure so cloud structure around the Moon can approach real bright
lunar aureole photographs without increasing ground Moon intensity.

## Ownership

- PBSKY remains atmosphere/astronomy authority.
- VCLOUD01 remains the sole cloud-density authority.
- The existing 256×256 cookie remains the sole detailed cloud-shadow field.
- Regional weather/radar/rain remain Heritage authorities.
- No second Sun/Moon shadow texture or weather system is introduced.
