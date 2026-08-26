# CELESTIAL01 — Sun/Moon Cloud Lighting and Ground Cloud Shadows

CELESTIAL01 extends the PBSKY01 + VCLOUD01 atmospheric renderer without adding
another weather or day/night authority.

## Authority

- `EnvironmentSystem` remains the sole astronomical Sun/Moon/date/time authority.
- PBSKY01 remains the physical atmosphere/transmittance authority.
- VCLOUD01 remains the volumetric cloud density/extinction authority.
- Heritage regional weather remains the cloud/radar/rain field authority.

## Volumetric cloud illumination

The cloud raymarch now treats the Sun and Moon as two independent directional
radiance sources. Each source:

1. uses its own astronomical direction and radiance,
2. traverses the same VCLOUD01 density/extinction field,
3. evaluates the same dual-HG phase, powder and multiple-scattering transport,
4. is attenuated by the PBSKY physical atmosphere LUT.

This allows warm solar and cool lunar contributions to coexist naturally during
twilight rather than synthesizing a single cloud-light direction.

## Ground cloud shadows

Heritage still uses one direct scene key light for the ordinary material/shadow
path. Its direction is the continuous great-circle Sun/Moon key already resolved
by `EnvironmentSystem`.

The 256x256 cloud optical-depth cookie now follows that exact key direction:

- day: Sun-directed cloud shadows,
- night: Moon-directed cloud shadows,
- dawn/dusk: continuous transition with no binary ownership switch.

The cookie is generated from the same VCLOUD01 density field visible in the sky,
so it is not a separate painted shadow mask. The material shader combines the
detailed cookie with regional cloud transmission. Removing warm/cool direct light
also lets the remaining sky/environment contribution shift scene colour naturally
under cloud cover.

## Performance

CELESTIAL01 does not add a second ground cloud-shadow texture or a second shadow
trace. The existing cookie follows the current celestial key. The only additional
raymarch work is Moon light transport in cloud pixels when Moon scene radiance is
non-negligible.

PERF05's cached shader link-status state remains mandatory; no per-frame
`GL_LINK_STATUS` queries are reintroduced.
