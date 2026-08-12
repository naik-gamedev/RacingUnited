# GFX07A — GLB UV orientation correction

Observed reference:
- Blender: correct
- Windows 3D Viewer: correct
- Heritage Engine: GLB texture mapping appeared rotated/mirrored on both U and V

Correction:
For glTF/GLB `TEXCOORD_0` only, Heritage now imports:

    U' = 1 - U
    V' = 1 - V

This is a 180-degree rotation of the UV coordinate field.

OBJ/MTL UV handling is unchanged.

Heritage recomputes tangents after GLB import, so the corrected UV orientation
also produces a matching tangent basis for normal maps.

Build with the rolling helper:
    Tools\00_BuildAndRunCurrent.cmd
