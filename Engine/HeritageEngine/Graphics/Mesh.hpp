#pragma once

#include <glad/glad.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "AssetMetadata.hpp"
#include "Material.hpp"

namespace heritage::graphics {

struct MeshDrawRange
{
    std::size_t firstIndex = 0;
    std::size_t indexCount = 0;
    std::string materialName;
    int nodeIndex = -1;
    int skinIndex = -1;
    bool hasVertexColors = false;

    // PERF03: conservative local-space bounds are generated once at asset
    // upload time. The renderer transforms this sphere with the authored node
    // pose and can reject off-screen primitives before material/texture work.
    std::array<float, 3> boundsCenter{ 0.0f, 0.0f, 0.0f };
    float boundsRadius = 0.0f;
    bool hasBounds = false;

    // Collision/spawn authoring nodes never render. Cache that decision once
    // when the asset is loaded instead of walking names/metadata every frame.
    bool hiddenByAuthoring = false;
};

enum class AnimationTargetPath
{
    Translation,
    Rotation,
    Scale
};

enum class AnimationInterpolation
{
    Step,
    Linear,
    CubicSpline
};

struct AnimationChannel
{
    int nodeIndex = -1;
    AnimationTargetPath path = AnimationTargetPath::Translation;
    AnimationInterpolation interpolation = AnimationInterpolation::Linear;
    std::size_t componentCount = 0;
    std::vector<float> times;
    std::vector<float> values;
};

struct AnimationClip
{
    std::string name;
    float durationSeconds = 0.0f;
    std::vector<AnimationChannel> channels;
};

struct MeshNode
{
    std::string name;
    int parentIndex = -1;
    std::vector<int> children;
    bool hasMatrix = false;
    std::array<float, 16> localMatrix{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f };
    std::array<float, 3> translation{ 0.0f, 0.0f, 0.0f };
    std::array<float, 4> rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };

    // glTF node `extras` are preserved here as flattened scalar metadata.
    // Blender Custom Properties exported through glTF extras therefore survive
    // the asset pipeline and can be consumed by vehicle/content systems.
    AssetMetadataMap metadata;

    // TIRE09/VIS01 automatic visual-deformation geometry. For GLB nodes whose
    // names identify a tire/tyre, uploadMesh derives these values directly from
    // the authored indexed vertices. No tire-specific vertex colours, bones or
    // GLB edits are required for the first visual-deformation path.
    bool hasTireVisualGeometry = false;
    std::array<float, 3> tireVisualCenter{ 0.0f, 0.0f, 0.0f };
    int tireVisualAxleAxis = 0;
    float tireVisualHalfWidth = 0.0f;
    float tireVisualInnerRadius = 0.0f;
    float tireVisualOuterRadius = 0.0f;
};

struct MeshSkin
{
    std::vector<int> joints;
    std::vector<std::array<float, 16>> inverseBindMatrices;
};

struct Mesh
{
    // Interleaved vertex storage.
    // - Legacy debug meshes may keep 6 floats: position.xyz, normal.xyz.
    // - Textured static meshes use 12 floats: position.xyz, normal.xyz,
    //   uv.xy, tangent.xyz, tangentHandedness.
    // - glTF meshes use 24 floats: the 12-float textured layout plus
    //   JOINTS_0.xyzw, WEIGHTS_0.xyzw and COLOR_0.rgba as floats.
    //   COLOR_0 defaults to white when a primitive has no authored color.
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::size_t vertexStrideFloats = 6;
    bool hasTexcoords = false;
    bool hasSkinning = false;
    bool hasVertexColors = false;

    std::vector<MeshDrawRange> drawRanges;
    std::unordered_map<std::string, MaterialDefinition> materials;
    std::vector<MeshNode> nodes;
    std::vector<int> rootNodeIndices;
    std::vector<MeshSkin> skins;
    std::vector<AnimationClip> animations;

    // Source asset plus any external dependencies. The renderer watches these
    // files so mesh/material edits hot-reload without restarting the engine.
    std::vector<std::filesystem::path> sourceDependencies;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
};

Mesh loadObjMesh(
    const std::string& path,
    bool normalizeToUnit = true,
    bool blenderCoordinates = false);
Mesh loadGlbMesh(
    const std::filesystem::path& path,
    bool normalizeToUnit = false);
bool computeTangents(Mesh& mesh);
void normalizeMeshToUnit(Mesh& mesh);
void uploadMesh(Mesh& mesh);
void destroyMesh(Mesh& mesh);

} // namespace heritage::graphics
