#include "../GltfBinary.hpp"
#include "../GltfSceneData.hpp"
#include "GltfInternal.hpp"

#include <iostream>
#include <unordered_set>

namespace heritage::graphics {
using namespace gltf_internal;

bool extractGlbStaticCollisionScene(
    const std::filesystem::path& path,
    GlbStaticCollisionScene& scene,
    std::string& errorMessage)
{
    scene = {};
    errorMessage.clear();

    const std::filesystem::path assetPath = path;
    JsonValue root;
    std::vector<std::uint8_t> binaryChunk;
    if (!parseGlb(assetPath, root, binaryChunk, errorMessage))
        return false;

    Mesh metadataMesh;
    buildNodeHierarchy(root, metadataMesh);
    scene.nodes.reserve(metadataMesh.nodes.size());
    for (const MeshNode& node : metadataMesh.nodes)
    {
        GlbSceneNodeInfo info;
        info.name = node.name;
        info.parentIndex = node.parentIndex;
        info.metadata = node.metadata;
        scene.nodes.push_back(std::move(info));
    }

    std::vector<int> roots;
    const JsonValue* scenes = root.find("scenes");
    const int defaultSceneIndex = root.find("scene")
        ? root.find("scene")->asInt(0)
        : 0;
    if (scenes && scenes->isArray()
        && defaultSceneIndex >= 0
        && static_cast<std::size_t>(defaultSceneIndex) < scenes->arrayValue.size())
    {
        roots = readIntArray(
            scenes->arrayValue[static_cast<std::size_t>(defaultSceneIndex)].find("nodes"));
    }
    else
    {
        for (std::size_t index = 0; index < metadataMesh.nodes.size(); ++index)
        {
            if (metadataMesh.nodes[index].parentIndex < 0)
                roots.push_back(static_cast<int>(index));
        }
    }

    std::vector<char> visited(metadataMesh.nodes.size(), 0);
    std::string extractionWarning;
    for (int rootIndex : roots)
    {
        extractCollisionNodeRecursive(
            root,
            binaryChunk,
            rootIndex,
            identityMatrix(),
            false,
            metadataMesh.nodes,
            scene,
            visited,
            extractionWarning);
    }

    if (scene.triangles.empty())
    {
        errorMessage =
            "GLB contains no marked static collision triangles. In Blender, name a collision object '*_Collision' "
            "or add Custom Properties heritage.role='collision_mesh' and heritage.collision_type='static_triangle_mesh'.";
        return false;
    }

    if (!extractionWarning.empty())
        errorMessage = "GLB collision extraction warning: " + extractionWarning;
    else
        errorMessage.clear();
    return true;
}

bool inspectGlbMetadata(
    const std::filesystem::path& assetPath,
    GlbMetadataDocument& document,
    std::string& errorMessage)
{
    document = {};
    JsonValue root;
    std::vector<std::uint8_t> binaryChunk;
    if (!parseGlb(assetPath, root, binaryChunk, errorMessage))
        return false;

    Mesh metadataMesh;
    buildNodeHierarchy(root, metadataMesh);

    const JsonValue* scenes = root.find("scenes");
    const int defaultSceneIndex = root.find("scene")
        ? root.find("scene")->asInt(0)
        : 0;
    if (scenes && scenes->isArray()
        && defaultSceneIndex >= 0
        && static_cast<std::size_t>(defaultSceneIndex) < scenes->arrayValue.size())
    {
        metadataMesh.rootNodeIndices = readIntArray(
            scenes->arrayValue[static_cast<std::size_t>(defaultSceneIndex)].find("nodes"));
    }
    else
    {
        for (std::size_t i = 0; i < metadataMesh.nodes.size(); ++i)
        {
            if (metadataMesh.nodes[i].parentIndex < 0)
                metadataMesh.rootNodeIndices.push_back(static_cast<int>(i));
        }
    }

    document.nodes = std::move(metadataMesh.nodes);
    document.rootNodeIndices = std::move(metadataMesh.rootNodeIndices);
    errorMessage.clear();
    return true;
}

Mesh loadGlbMesh(
    const std::filesystem::path& assetPath,
    bool normalizeToUnit)
{
    Mesh mesh;
    mesh.vertexStrideFloats = kGltfVertexStride;
    mesh.sourceDependencies.push_back(assetPath.lexically_normal());

    JsonValue root;
    std::vector<std::uint8_t> binaryChunk;
    std::string error;
    if (!parseGlb(assetPath, root, binaryChunk, error))
    {
        std::cerr << error << '\n';
        return mesh;
    }

    buildNodeHierarchy(root, mesh);
    buildSkins(root, binaryChunk, mesh);
    buildAnimations(root, binaryChunk, mesh);

    const JsonValue* scenes = root.find("scenes");
    const int defaultSceneIndex = root.find("scene") ? root.find("scene")->asInt(0) : 0;
    if (scenes && scenes->isArray()
        && defaultSceneIndex >= 0
        && static_cast<std::size_t>(defaultSceneIndex) < scenes->arrayValue.size())
    {
        mesh.rootNodeIndices = readIntArray(scenes->arrayValue[static_cast<std::size_t>(defaultSceneIndex)].find("nodes"));
    }
    else if (!mesh.nodes.empty())
    {
        mesh.rootNodeIndices.reserve(mesh.nodes.size());
        for (std::size_t i = 0; i < mesh.nodes.size(); ++i)
        {
            if (mesh.nodes[i].parentIndex < 0)
                mesh.rootNodeIndices.push_back(static_cast<int>(i));
        }
    }

    std::unordered_set<std::string> dependencySet;
    for (int nodeIndex : mesh.rootNodeIndices)
        appendNodeRecursive(root, binaryChunk, assetPath, nodeIndex, mesh, dependencySet);

    if (normalizeToUnit)
    {
        if (mesh.hasSkinning)
        {
            std::cerr
                << "GLB animation note: normalize=true is ignored for skinned assets so bind-space remains valid."
                << '\n';
        }
        else
        {
            normalizeMeshToUnit(mesh);
        }
    }

    mesh.hasTexcoords = computeTangents(mesh);

    if (mesh.drawRanges.empty() && !mesh.indices.empty())
    {
        mesh.drawRanges.push_back({
            0,
            mesh.indices.size(),
            {},
            -1,
            -1 });
    }

    std::cout
        << "GLB loaded: " << mesh.indices.size() / 3
        << " triangles, " << mesh.materials.size()
        << " materials, " << mesh.skins.size() << " skins, "
        << mesh.animations.size() << " animations" << '\n';
    return mesh;
}


} // namespace heritage::graphics
