# ADR-093 — Scientific Raindrop Scale and Sub-pixel Optical Rendering

## Status
Accepted for WEATHER07B5.

## Context
The WEATHER07B4 diagnostic deliberately enlarged authored rain quads to two
metres to prove that the supplied coverage, normal and thickness textures were
actually reaching rasterization. That test succeeded and therefore must not
remain in normal presentation.

Liquid raindrops are millimetre-scale. Heritage's WEATHER07A physical authority
already samples a Marshall–Palmer-style drop-size population and uses the
Atlas–Srivastava–Sekhon terminal-velocity relation. Presentation must preserve
those physical diameters rather than turning a rendered representative into a
centimetre- or metre-wide object merely to survive rasterization.

A visible camera streak is also not the physical length of the liquid drop. It
is the image-space integration of a moving millimetre-scale drop during a finite
camera exposure interval.

## Decision
Normal liquid-rain representatives use equivalent-volume diameters between
0.20 mm and 6.00 mm. This conservative ordinary-rain range includes drizzle and
large rain while avoiding turning rare unstable giant drops into the default.

The optical streak path is

`terminal velocity × effective exposure time`

with an effective exposure between approximately 1/180 s in lighter rain and
1/90 s in heavy storm presentation. Changing exposure changes only the rendered
motion trace; it never changes physical fall speed or rainfall mass.

Millimetre-scale rain is usually narrower than one display pixel. Heritage
therefore expands only the *raster support* to approximately 1.25 pixels in
width and 1.10 pixels in streak length when necessary. The fragment alpha is
multiplied by the original-physical-area / expanded-raster-area ratio. This is
analytic anti-aliasing: the liquid diameter remains physical while the GPU is
given enough coverage to rasterize its sub-pixel optical contribution.

The supplied RainDrop_BC, RainDrop_N and RainDrop_TN textures are an optical
kernel. Their teardrop silhouette must not be interpreted as the literal
aerodynamic shape of the falling liquid drop. Real small raindrops are close to
spherical and larger drops become increasingly oblate/deformed.

The proven world-space dynamic-VBO rain path is retained as the compatibility
consumer while the instanced path remains available where its GLSL program is
executable. Both paths must use the same physical diameter/exposure policy.

## Scientific basis
- Marshall, J. S. and Palmer, W. McK. (1948), *The Distribution of Raindrops
  with Size*, Journal of Meteorology 5, 165–166.
- Atlas, D., Srivastava, R. C. and Sekhon, R. S. (1973), *Doppler Radar
  Characteristics of Precipitation at Vertical Incidence*, Reviews of
  Geophysics 11, 1–35.
- Beard, K. V. and Chuang, C. (1987), *A New Model for the Equilibrium Shape
  of Raindrops*, Journal of the Atmospheric Sciences 44, 1509–1524.
- Laboratory/field breakup literature shows ordinary large drops become
  increasingly unstable in roughly the 5–8 mm class; Heritage therefore keeps
  the normal population capped at 6 mm until a dedicated giant-drop/breakup
  model is introduced.

## Consequences
- No normal rain geometry is metres wide.
- Rain retains realistic physical size even at 4K.
- Sub-pixel rain remains visible without lying about physical diameter.
- Motion streaks respond to real terminal velocity and optical exposure.
- Later WEATHER07C refraction can reuse the same physical-width/coverage model.
