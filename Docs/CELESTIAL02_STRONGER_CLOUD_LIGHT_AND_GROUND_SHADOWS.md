# CELESTIAL02 — Stronger Cloud Interior Light and Ground Shadows

CELESTIAL02 is a visual-response pass on top of CELESTIAL01.  It does not add
another weather, cloud-density, astronomy, or shadow authority.

## Moon-lit cloud interiors

The VCLOUD01 raymarch already evaluates two multiple-scattering octaves.  User
validation showed that this is still too dark inside optically thick clouds under
Moon light.  CELESTIAL02 keeps the existing directional Moon transport and adds a
low-energy broad higher-order approximation that grows with local droplet density.
It remains multiplied by the real astronomical Moon radiance and PBSKY
transmittance at resolve time, so it is not a self-emissive cloud hack.

The goal is a readable cool lunar fill through the cloud body while preserving
directional rims, extinction, phase response and dark storm structure.

## Ground cloud shadows

The same single 256x256 VCLOUD01 optical-depth cookie still follows Heritage's
continuous Sun/Moon key direction.  Its coarse 16-segment trace now converts
transmittance with a 2x optical-depth response (`T^2`).  This makes broken-cloud
shadows visibly remove direct sunlight/moonlight while leaving sky/IBL ambient
lighting untouched, so cloud shadows become stronger without becoming black
decals.

CELESTIAL01 also normalized regional cloud transmission to preserve its spectral
shape; that accidentally discarded its magnitude whenever the detailed cookie was
valid.  CELESTIAL02 fixes the combination: the detailed cookie may darken the
direct key light, but may never make an already overcast regional sample brighter.

## Preserved architecture

- one `EnvironmentSystem` astronomical authority,
- one PBSKY atmosphere,
- one VCLOUD density/extinction field,
- one regional weather/radar/rain authority,
- one moving ground cloud-shadow cookie,
- no additional shadow decals,
- no change to water, physics or `.hhyd v15`.
