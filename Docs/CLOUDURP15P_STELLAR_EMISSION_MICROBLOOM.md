# CLOUDURP15P - emissive star field + micro-bloom

## Goal

The HDR celestial map should read as actual self-luminous stars rather than a flat night-sky texture, without turning the renderer into a heavy bloom pipeline or making the Milky Way fuzzy.

## Existing emissive ownership

The star map was already added after the astronomical sky exposure multiplication. CLOUDURP15P makes that intent explicit and preserves it: star radiance is independent of Sun, Moon, ground and ordinary scene lighting. Atmospheric daylight visibility and horizon extinction still attenuate the stars naturally.

## Bright-star response

Only compact high-luminance texels receive an additional 7.5% emissive peak response. The broad Milky Way/background remains essentially unchanged.

## Micro-bloom

A four-tap cardinal halo samples only the immediate angular neighbourhood. The sampling radius uses both the KTX2 texel size and `fwidth(starUv)`, keeping the apparent halo around roughly 1-2 screen pixels when the star map resolution changes.

Bloom source energy is capped before spreading, so a very hot HDR star can retain a brilliant core without creating a large fantasy halo. The final spread coefficient is intentionally only 0.026.

This is not a full-scene post-process bloom pass. Headlights, reflections, UI and ordinary bright geometry are unaffected. Moon bloom remains its independent atmospheric optical treatment from CLOUDURP15O.

## Performance

The extra four star-map samples execute only while the astronomical star field is visible and only for sky directions above the horizon. No new framebuffer, blur target or postprocess pass is allocated.
