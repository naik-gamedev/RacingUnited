# VA02D — Mirrored GLB transform / winding support

Blender vehicle authoring is authoritative for the visible left/right part
orientation.

A common Blender workflow mirrors a wheel or other vehicle part with a negative
scale. glTF preserves that node transform. Negative scale reverses the handedness
of the transform and therefore reverses triangle winding.

Before VA02D, Heritage always used OpenGL's default counter-clockwise front-face
rule. Mirrored GLB nodes could therefore appear as if the wheel/rim were facing
the wrong way or as if their inside/back surfaces were visible.

VA02D computes the determinant of each final draw-range model transform:

- positive determinant -> GL_CCW
- negative determinant -> GL_CW

This is renderer-level behavior, not a Peugeot/wheel special case. It therefore
also fixes mirrored doors, suspension pieces, body panels, characters, props,
etc. in any GLB.

The authored GLB transforms remain unchanged; Heritage only corrects which
winding OpenGL considers the front face.

Build with the rolling helper:
`Tools\00_BuildAndRunCurrent.cmd`
