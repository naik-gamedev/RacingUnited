# CLOUDURP15J8 - geographic calendar + real celestial sky

## Runtime star texture

`Modules/RacingUnited/Assets/Scenes/Sky/Scene_NightSky.ktx2`

The runtime asset is a 4096x2048 KTX2 containing one BC6H unsigned-float
2D level. Heritage uploads the BC6H blocks directly with
`GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT`; there is no CPU decompression and no
mipmap chain. Sampling is bilinear and the horizontal coordinate repeats.

The map is treated as a celestial-coordinate plate-carree map: 0h right
ascension is centered and right ascension increases to the left. The image is
fixed in celestial space. Heritage rotates the local observer basis according
to geographic latitude/longitude and local sidereal time.

## Scene geographic metadata

Canonical GLB extras / Blender Custom Properties:

- `heritage.latitude_deg` - required for authored geography
- `heritage.longitude_deg` - required for authored geography
- `heritage.elevation_m` - optional, defaults to 0 m
- `heritage.timezone` - optional civil-time interpretation, e.g.
  `Europe/Ljubljana` or `Asia/Tokyo`
- `heritage.utc_offset_minutes` - optional explicit override

Metadata may be exported on the active glTF scene itself. For Blender workflows
where scene extras are not preserved, put the same Custom Properties on a
root-level Empty named `Heritage_SceneMetadata`. Heritage also accepts these
keys on any node as a compatibility fallback.

Racing United currently initializes Ivarcko Jezero as its module fallback:
latitude 46.50619924 N, longitude 14.97089803 E, elevation 643 m,
`Europe/Ljubljana`. A loaded Scene_*.glb with authored metadata overrides it.

## Calendar / astronomy authority

The engine owns one Gregorian scene calendar and local clock. Time-scale
advancement crosses midnight by advancing the calendar date. The astronomy
path computes:

1. local civil time -> UTC using the scene time-zone/offset
2. Gregorian date/time -> Julian date
3. Julian date -> Greenwich mean sidereal time
4. longitude -> local sidereal time
5. local sidereal time + latitude -> celestial/world orientation
6. compact solar and lunar ephemerides -> local Sun/Moon directions

The visible Moon remains intentionally full-phase per Racing United's current
art direction; only its sky position moves astronomically.

## Lua API

- `Environment.GetDate()`
- `Environment.SetDate(year, month, day)`
- `Environment.GetLocation()`
- `Environment.SetLocation(latitude, longitude, elevation, timezone [, utcOffsetMinutes])`
- `Environment.ApplySceneMetadata("Scenes/Scene_Whatever.glb")`

The SCENE -> ENVIRONMENT panel exposes the date, local time, time scale and
loaded geographic metadata.

## Removed path

The old procedural `hash13` point-star field is removed. There is no longer a
grid of native-resolution white pseudo-star pixels.
