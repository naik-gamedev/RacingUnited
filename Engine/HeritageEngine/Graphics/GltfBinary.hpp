#pragma once

#include <filesystem>
#include <string>

#include "Mesh.hpp"

namespace heritage::graphics {

// Loads a glTF 2.0 binary asset (.glb) into Heritage Engine's shared Mesh
// representation. The importer preserves node hierarchy, skins, animation
// clips, embedded textures and PBR material data used by the renderer.
Mesh loadGlbMesh(
    const std::filesystem::path& path,
    bool normalizeToUnit);

struct GlbMetadataDocument
{
    std::vector<MeshNode> nodes;
    std::vector<int> rootNodeIndices;
    AssetMetadataMap sceneMetadata;
};

// Lightweight metadata inspection path. Parses the GLB JSON/node hierarchy and
// preserves node extras without decoding geometry or textures. This is used by
// content systems such as modular vehicle-part discovery.
bool inspectGlbMetadata(
    const std::filesystem::path& path,
    GlbMetadataDocument& document,
    std::string& errorMessage);

} // namespace heritage::graphics
