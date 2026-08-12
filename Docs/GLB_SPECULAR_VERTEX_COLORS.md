# GLB specular + vertex colors (GFX5)

GFX5 extends the Heritage Engine GLB material path with explicit glTF specular
support and standard `COLOR_0` vertex colors.

## GLB specular

Heritage now reads the glTF 2.0 extension:

`KHR_materials_specular`

Supported inputs:

- `specularFactor`
- `specularTexture` — glTF alpha-channel specular strength
- `specularColorFactor`
- `specularColorTexture` — RGB specular color

These extend the existing metallic/roughness workflow; they do not replace base
color, normal, roughness, metallic, AO or emissive.

Heritage's current lighting shader is still intentionally lightweight rather
than a final IBL renderer, but the GLB material data is now preserved and used
instead of being discarded.

## Diffuse / base color together with PBR

Heritage does **not** require a philosophically pure albedo texture. The normal
PBR path still uses the authored base-color texture together with normal,
roughness, metallic, specular, AO and other channels.

That means an artist may deliberately use a diffuse-style base-color texture
with subtle baked ambient/cavity shading and still use the PBR response on top.
The engine will not strip or forbid that shading.

The cleanest label for the authoring slot is therefore:

**Diffuse / Base Color**

A physically strict asset can put pure albedo there; a Racing United asset may
choose tasteful baked shading when that gives the desired look.

## Vertex colors

GLB `COLOR_0` is now imported for static and skinned meshes.

Supported accessor forms include the normal glTF forms:

- RGB (`VEC3`)
- RGBA (`VEC4`)
- floating point
- normalized integer color data such as 8-bit or 16-bit vertex colors

Vertex color RGB multiplies the material's Diffuse / Base Color in linear
lighting space. Vertex alpha multiplies material opacity.

This makes greyscale vertex colors useful for baked ambient-occlusion/cavity
shading. For example:

- white vertex = no darkening
- 0.8 grey = mild baked occlusion
- 0.4 grey = strong baked occlusion

This is deliberately compatible with the common workflow of baking AO into a
vertex color layer and using it to modulate the visible base material.

## Blender workflow

For vertex AO/color:

1. Create a Color Attribute on the mesh.
2. Paint or bake the desired vertex colors / AO into it.
3. Export as glTF Binary (`.glb`) with vertex colors included.
4. Heritage imports the first glTF color set as `COLOR_0`.

For specular:

1. Author the material in Blender.
2. Export to `.glb` using material settings that produce
   `KHR_materials_specular` when explicit specular control is required.
3. Heritage reads the factor/color fields and their textures.

## Current boundary

GFX5 adds the asset/material data path. Environment reflections and IBL remain a
later renderer milestone; those will make roughness, metallic and specular much
more visually expressive, especially for vehicle paint, glass and metal.
