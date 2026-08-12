#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "AssetMetadata.hpp"
#include "../Core/Math/Math.hpp"

namespace heritage::graphics {

// OpenGL-free scene-authoring extraction types used by non-render systems.
// The GLB parser remains shared with the renderer, but physics/content code can
// consume marked scene geometry without including Mesh.hpp / GLAD.
struct GlbSceneNodeInfo
{
    std::string name;
    int parentIndex = -1;
    AssetMetadataMap metadata;
};

struct GlbCollisionTriangle
{
    heritage::math::Vec3 a{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 b{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 c{ 0.0f, 0.0f, 0.0f };
    int nodeIndex = -1;
};

struct GlbStaticCollisionScene
{
    std::vector<GlbSceneNodeInfo> nodes;
    std::vector<GlbCollisionTriangle> triangles;
    bool spawnFound = false;
    heritage::math::Vec3 spawnPosition{ 0.0f, 0.0f, 0.0f };
    std::string spawnName;
    std::size_t collisionNodeCount = 0;
};

// Extracts only explicitly authored static collision geometry from a .glb.
// A node (or any ancestor) is considered collision authoring when it has:
//   heritage.role = "collision_mesh"
//   heritage.collision_type = "static_triangle_mesh"
//   heritage.collision = true
// or a fallback name containing the token "_Collision" / "Collision_".
//
// Spawn metadata may be a mesh or Blender Empty named SPAWN_PLAYER / PLAYER_SPAWN
// or carrying heritage.role = "spawn_player". Spawn position comes from the
// node transform and is later snapped to the imported drive surface by physics.
bool extractGlbStaticCollisionScene(
    const std::filesystem::path& path,
    GlbStaticCollisionScene& scene,
    std::string& errorMessage);

} // namespace heritage::graphics
