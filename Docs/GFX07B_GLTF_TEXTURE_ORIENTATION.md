# GFX07B — glTF authored UV/image orientation

The GFX07A UV 180-degree transform was an experiment and was too aggressive.

Correct Heritage policy is now:

## glTF / GLB
- `TEXCOORD_0` is imported exactly as authored.
- Embedded/external glTF image rows are uploaded exactly as decoded.
- No U mirror.
- No V mirror.
- No hidden 180-degree UV rotation.

This matches the principle used for precision vehicle geometry: the source asset
is authoritative. Blender and conforming glTF viewers are the reference.

## OBJ / MTL
The existing legacy vertical image-row flip remains enabled for Wavefront
materials so this patch does not change existing OBJ content.

The texture cache key includes the orientation mode, so the same image can be
used by legacy and glTF paths without sharing an incorrectly oriented cached
OpenGL texture.

Normal-map tangents continue to be generated from the unmodified GLB UVs.
