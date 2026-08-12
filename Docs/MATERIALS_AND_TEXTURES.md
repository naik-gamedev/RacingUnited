# Materials and Texture Maps

## Heritage Engine graphics foundation: GFX1

Heritage Engine entity OBJ meshes can now consume Wavefront UVs, MTL material
assignments and module-local texture maps. This is a renderer capability, not a
physics milestone, so it intentionally does not replace or renumber the active
Step 29Q terrain-contact work.

## Authoring contract

- Meshes are still attached with the existing `Entity.SetMesh` API.
- OBJ `vt` texture coordinates are imported and kept per vertex.
- `mtllib` and `usemtl` are supported, including multiple material draw ranges
  inside one OBJ.
- MTL files must be referenced from the OBJ with a safe relative path.
- Texture files must live under the active module's `Assets` directory.
- Relative texture paths in the MTL are resolved relative to that MTL file.
- Workstation-specific absolute texture paths are not allowed to escape the
  module. If an MTL contains an absolute Blender/exporter path, Heritage first
  tries a file with the same filename beside the MTL; otherwise the texture is
  rejected with a one-time warning.
- WIC-backed image decoding supports ordinary Windows image formats such as PNG,
  JPEG, BMP and TIFF. DDS/KTX are not part of GFX1.

The Racing United vehicle authoring convention remains unchanged: Blender
X=right, Y=forward/back, Z=up, one unit = one metre, with Blender OBJ conversion
performed at the import boundary.

## Material inputs

The renderer consumes these MTL/PBR fields when present:

| Purpose | MTL / extension |
| --- | --- |
| Base colour | `Kd`, `map_Kd` |
| Specular | `Ks`, `map_Ks` |
| Emissive | `Ke`, `map_Ke` |
| Shininess fallback | `Ns` |
| Roughness | `Pr`, `map_Pr`, `map_roughness` |
| Metallic | `Pm`, `map_Pm`, `map_metallic` |
| Normal | `norm`, `normal`, `map_normal`; `map_Bump`/`bump` only when the filename clearly identifies a normal map |
| Ambient occlusion | `map_Ka`, `map_AO`, `map_ao` |
| Opacity | `d`, `Tr`, `map_d` |

For compatibility with some older Blender-era exports, a `map_d` whose filename
clearly contains `Albedo`, `Diffuse`, `BaseColor` or `Base_Color` is treated as a
base-colour map instead of opacity. Likewise, `map_Ns` is accepted as roughness
only when its filename clearly says `Roughness`/`Rough`.

## Colour space

Base-colour and emissive textures are uploaded as sRGB. OpenGL converts them to
linear values before lighting. Normal, roughness, metallic, AO and opacity maps stay linear. RGB specular-color
maps are color data and are decoded from sRGB; scalar glTF specular-strength maps
remain linear. Final display gamma remains controlled by the existing
video gamma setting.

## Normal maps

OBJ UVs are used to build a tangent frame in native C++. Tangent-space normal
maps therefore work on authored meshes without Lua-side tangent data. Meshes
without usable UVs continue to render with their geometric normals and material
constants; texture maps are disabled for those meshes rather than sampling one
arbitrary texel everywhere.

## Filtering

Material textures generate mipmaps and use the existing Video > Texture Filter
setting:

- Nearest
- Bilinear
- Trilinear
- Anisotropic x2
- Anisotropic x4
- Anisotropic x8
- Anisotropic x16

Anisotropic levels are clamped to what the active OpenGL driver reports.

## Hot reload and caching

OBJ geometry is cached as before. Referenced MTL files are tracked as source
dependencies, so editing an MTL can reload the mesh/material description without
touching the OBJ timestamp. Texture files have an independent cache and reload
when their timestamp changes.

## Current limitations

GFX1 deliberately stays narrow:

- OBJ/MTL is the current production bridge; glTF and a native Heritage material
  asset format can come later.
- Transparent materials use ordinary alpha blending but are not globally sorted
  back-to-front yet.
- Height/parallax/displacement maps are not interpreted as normal maps.
- Environment reflections, IBL and a full energy-conserving PBR lighting model
  are later rendering work. Roughness/metallic/specular inputs already have
  distinct, useful effects in the current lightweight directional-light shader.
- Texture streaming/compression/virtual texturing are not part of this step.

## Portable Blender workflow

For the least surprising current workflow:

1. UV unwrap the mesh in Blender.
2. Export OBJ + MTL into a folder under the module `Assets` tree.
3. Copy referenced PNG/JPEG textures into the same folder or a relative
   subfolder such as `Textures`.
4. Ensure the MTL uses relative texture paths where possible.
5. Keep the OBJ `mtllib` line relative, for example `mtllib PlayerWheel.mtl`.
6. Rebuild with `Tools\BuildAndRunTextureMaps.cmd`.

The engine will still refuse any material texture that resolves outside the
active module's Assets directory.


## GFX5 additions

The GLB path now also supports `KHR_materials_specular` and standard glTF
`COLOR_0` vertex colors. Heritage deliberately permits a diffuse-style
base-color texture with subtle baked shading while normal/roughness/metallic/
specular PBR inputs remain active. Greyscale `COLOR_0` is therefore a supported
way to carry baked vertex AO/cavity shading. See
`Docs/GLB_SPECULAR_VERTEX_COLORS.md`.


## GFX6 environment response

The same material channels now feed a GGX/Cook-Torrance direct-light model and
a procedural cubemap environment. Roughness controls reflection blur, metallic
controls whether the base color participates in metallic reflection, specular
controls dielectric reflectance, and AO attenuates environment lighting. Broad
vertex-color AO and fine AO textures can be used together. See
`Docs/ENVIRONMENT_IBL.md`.
