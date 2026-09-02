# CELESTIAL07 — Stable Physical Shadow Ownership

## Symptom

At accelerated dusk/dawn the world could still alternate between markedly different
lighting states even after CELESTIAL06 made the day/night scalar monotonic.

The remaining instability was in shadow ownership, not the sky curve:

- Heritage's legacy one-direction scene light used a spherical interpolation between
  the real Sun and Moon. Around the reference sunset they are roughly 140 degrees
  apart, so the synthetic key can sweep extremely quickly while direct ownership
  transfers.
- The same synthetic key drove the 256x256 volumetric-cloud optical-depth cookie.
- The cloud cookie remained enabled down to `keyLightDirection.y > 0.01`, allowing
  near-tangent rays through the spherical cloud shell. At such low elevation the
  traced path becomes extremely long while only 15 interior samples are available,
  making small direction changes capable of producing very different masks.
- The detailed cookie was sampled in the material shader and then applied again by
  CELESTIAL04's dedicated post-opaque receiver. A cookie-validity/source change could
  therefore change direct/ambient material lighting and multiply the finished scene
  a second time.

## Fix

### Ordinary directional light / geometry shadows

The single legacy directional key no longer interpolates Sun and Moon directions.
Only one real celestial direction may own it at a time:

1. the outgoing source fades toward zero direct authority,
2. a narrow twilight bridge has zero directional-key intensity,
3. ownership changes while the key is dark,
4. the incoming physical source then fades in.

Volumetric clouds still receive independent physical Sun and Moon lighting exactly as
before; the bridge affects only the legacy one-direction material/CSM path.

### Detailed ground cloud shadows

The one cloud optical-depth cookie also follows one real body at a time.

- Sun cookie strength fades out before low-angle tangent tracing becomes unstable.
- Twilight is intentionally ambient-dominated for a short interval.
- Moon cookie strength fades in only after night authority is established and the
  Moon is safely above the horizon.
- Receiver strength is continuous and explicitly uploaded as `uShadowStrength`.
- The receiver uses the exact physical direction that generated the cookie.

There is no synthetic Sun/Moon cloud-shadow direction and no `keyLightDirection.y`
hard on/off test in the cloud receiver path.

### One detailed receiver authority

The material shader no longer samples the 256x256 detailed cloud-shadow cookie.
Materials retain the broad regional cloud transmission used for direct-light colour
and ambient response. The detailed moving optical-depth structure is applied exactly
once by CELESTIAL04's dedicated post-opaque ground receiver.

## Probe

`Build/Reports/CELESTIAL07_ShadowOwnershipProbe.csv` samples the Ivarcko reference
scene on 2026-08-24 at one-minute intervals.

The previous CELESTIAL06 synthetic key reached about 12.4 degrees of rotation per
simulated minute during the sunset handoff (about 50 degrees per real second at the
240x cycle). With CELESTIAL07, active directional-key motion follows the physical
Sun/Moon and remains about 0.25 degrees per simulated minute in the same broad
window. The Sun->Moon source change occurs while direct key intensity is zero.

The sunset cloud-shadow sequence intentionally becomes:

- 19:00: Sun cookie full
- 19:30: Sun cookie fading
- about 19:42-20:05: no detailed directional cloud cookie; twilight ambient only
- about 20:06 onward: Moon cookie fades in

The same sequence reverses continuously at dawn.

## Preserved

- CELESTIAL05 neutral low-Moon cloud/halo colour.
- CELESTIAL06 single normalized day/night scalar and early star fade.
- Independent physical Sun + Moon volumetric-cloud illumination.
- One 256x256 cloud-shadow texture and existing filter cost.
- Geometry CSM quality/settings; only the celestial direction ownership is corrected.
- LIVETRACK, tire, weather, hydrology and audio runtime behaviour.
