# GLB skeletal animation support (GFX3)

Heritage Engine now extends the GLB importer from static meshes to **skinned / skeletal animated glTF 2.0 binary (`.glb`) assets**.

## What this milestone adds

- Static `.glb` support remains intact.
- Node hierarchy import is preserved.
- glTF skins are imported.
- Up to 4 joint influences per vertex are imported from `JOINTS_0` and `WEIGHTS_0`.
- Inverse bind matrices are imported.
- Joint palettes are built at runtime and uploaded to the mesh shader.
- Skinned meshes are deformed in the vertex shader.
- glTF animation channels are imported for:
  - translation
  - rotation
  - scale
- Animation playback loops automatically.
- Existing OBJ support remains unchanged.

## Current scope / limitations

This is the first practical animation pass and is intentionally conservative.

Current limitations:

- The renderer currently auto-plays the **first animation clip** found in the GLB.
- There is not yet a gameplay/UI API for selecting or blending clips.
- Interpolation support currently covers:
  - `LINEAR`
  - `STEP`
- `CUBICSPLINE` is not implemented yet.
- Maximum joint palette sent to the shader per draw range is **128 joints**.
- The implementation targets the normal Blender-to-GLB workflow first.

## Practical usage

If your entity mesh path points to a module-relative `.glb`, and that GLB contains:

- a skinned mesh
- a bone hierarchy
- one or more animation clips

then the engine will render it and automatically loop the first clip.

No new script-side flag is required yet.

## Blender export guidance

Recommended export settings for first tests:

1. Export as **glTF Binary (.glb)**.
2. Include **Armature** and **Mesh**.
3. Keep **Animations** enabled.
4. Keep **Skinning** enabled.
5. Prefer ordinary keyframed TRS animation on bones.
6. Keep textures embedded or safely referenced inside the module `Assets` tree.

## Good first test asset

A very small sanity-test asset is best:

- one character or prop
- one armature
- one idle loop
- modest bone count

Examples:

- a waving stickman
- a rotating/skinned robot arm
- a simple driver dummy with an idle pose loop

That makes it easier to confirm the whole path before trying a complex full character.
