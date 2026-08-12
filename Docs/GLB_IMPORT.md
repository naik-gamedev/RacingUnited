# GLB static mesh import (GFX2)

Heritage Engine now accepts **glTF 2.0 Binary (`.glb`)** files in Entity mesh components, alongside the existing OBJ/MTL path.

## What works now

- Static mesh geometry from the default glTF scene.
- Node transforms are flattened at load time.
- Multiple primitives and multiple materials in one `.glb`.
- Embedded textures stored inside the `.glb` container.
- External texture files referenced by the glTF, as long as they stay inside the module `Assets` tree.
- PBR-style inputs:
  - base color / diffuse
  - normal
  - roughness
  - metallic
  - ambient occlusion
  - emissive
- Hot reload when the `.glb` file changes on disk.
- Hot reload for external texture files referenced by the glTF.

## Current scope / limitations

This first pass is intentionally focused on the static-mesh rendering path.

Not part of this milestone yet:

- skeletal animation / skinning
- glTF animation playback
- morph targets
- cameras / lights imported from the asset
- sparse accessors
- primitive modes other than triangles

## Notes on PBR mapping

- `baseColorTexture` is used as the base color map.
- `metallicRoughnessTexture` is supported by sampling the glTF-standard channels:
  - **G** = roughness
  - **B** = metallic
- `occlusionTexture` uses the red channel.
- `baseColorFactor` alpha contributes to opacity.

## Usage

Any existing `Entity.SetMesh(...)` or equivalent mesh component path that points to a module-relative mesh file can now point to either:

- `Assets/.../MyMesh.obj`
- `Assets/.../MyMesh.glb`

No special flag is required.

## Recommended Blender export

For the cleanest single-file asset pipeline:

1. Export as **glTF Binary (.glb)**.
2. Keep the default scene clean.
3. Apply transforms if desired.
4. Pack or embed textures so the `.glb` stays self-contained.
5. Use UVs and normal maps as usual.

This gives you one container file that can carry geometry, materials and textures together.
