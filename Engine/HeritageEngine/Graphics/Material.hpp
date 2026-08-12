#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Core/Math/Math.hpp"

namespace heritage::graphics {

enum class TextureChannel
{
    R = 0,
    G = 1,
    B = 2,
    A = 3
};

struct EmbeddedTextureData
{
    std::string key;
    std::vector<std::uint8_t> bytes;
    std::string mimeType;
};

struct MaterialTextureReference
{
    std::filesystem::path filePath;
    std::shared_ptr<EmbeddedTextureData> embedded;
    TextureChannel channel = TextureChannel::R;

    // Wavefront/legacy image loading historically flips decoded rows for
    // OpenGL. glTF/GLB already defines the authored image/UV relationship and
    // must be uploaded without that legacy row flip.
    bool flipVerticalOnDecode = true;

    bool empty() const
    {
        return filePath.empty() && !embedded;
    }
};

struct MaterialDefinition
{
    std::string name;
    heritage::math::Vec3 baseColor{ 1.0f, 1.0f, 1.0f };
    heritage::math::Vec3 specularColor{ 0.04f, 0.04f, 0.04f };
    heritage::math::Vec3 emissiveColor{ 0.0f, 0.0f, 0.0f };

    // MTL's Ns is retained for compatibility; roughness is what the renderer
    // consumes. If Pr is absent, roughness is derived from Ns.
    float shininess = 32.0f;
    float roughness = 0.24253562f;
    float metallic = 0.0f;
    // Scalar dielectric specular weight. glTF KHR_materials_specular uses
    // this independently from the RGB specular color. Legacy MTL defaults
    // to 1.0 and continues to use Ks/map_Ks.
    float specularFactor = 1.0f;
    float opacity = 1.0f;

    std::filesystem::path sourceDirectory;
    MaterialTextureReference baseColorMap;
    MaterialTextureReference normalMap;
    MaterialTextureReference roughnessMap;
    MaterialTextureReference metallicMap;
    // RGB specular-color texture (MTL map_Ks or glTF specularColorTexture).
    MaterialTextureReference specularMap;
    // Scalar glTF KHR_materials_specular weight; specularTexture stores its
    // value in the alpha channel.
    MaterialTextureReference specularFactorMap;
    MaterialTextureReference ambientOcclusionMap;
    MaterialTextureReference emissiveMap;
    MaterialTextureReference opacityMap;
};

struct MaterialLibraryLoadResult
{
    std::unordered_map<std::string, MaterialDefinition> materials;
    std::vector<std::string> warnings;
};

// Reads the useful subset of Wavefront MTL plus common PBR extensions:
// Kd/Ks/Ke/Ns/d/Tr/Pr/Pm and map_Kd/map_Ks/map_Ke/map_d/map_Pr/map_Pm,
// normal/bump aliases and AO aliases.
//
// Returned texture paths are resolved relative to the MTL. Absolute exporter
// paths are never trusted as portable module paths; when possible their
// filename is re-based beside the MTL so copied Blender textures "just work".
MaterialLibraryLoadResult loadMaterialLibrary(
    const std::filesystem::path& materialLibraryPath);

} // namespace heritage::graphics
