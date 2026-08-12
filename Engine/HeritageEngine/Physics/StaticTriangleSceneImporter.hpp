#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "CollisionSystem.hpp"

namespace heritage::physics {

struct StaticTriangleSceneSpawn
{
    heritage::math::Vec3 groundPoint{ 0.0f, 0.0f, 0.0f };
    bool found = false;
    bool explicitMarker = false;
    std::string sourceName;
};

// Reads a Blender-exported OBJ as exact static query triangles. This bridge is
// intentionally limited to read-only scene queries (most importantly vehicle
// suspension/tire raycasts) until Heritage Engine gains full production static
// triangle-mesh rigid-body contact. Blender's default OBJ export converts its
// X/right, Y/forward, Z/up authoring space to OBJ X/right, Y/up, -Z/forward;
// the importer converts that deterministic file convention into native engine
// X/right, Y/up, Z/forward without asking creators to rotate their scene.
bool loadStaticTriangleSceneFromObj(
    const std::filesystem::path& path,
    bool blenderDefaultObjCoordinates,
    std::vector<StaticSceneTriangle>& output,
    StaticTriangleSceneSpawn* spawn,
    std::string& error);

// Scans an OBJ only for an authored SPAWN_PLAYER mesh. This lets the marker
// live in the visual scene even if a creator exports a separate collision OBJ
// without it.

// Reads static triangle collision directly from a glTF Binary scene. Collision
// geometry must be explicitly marked by Blender Custom Properties / glTF extras
// (heritage.role=collision_mesh, heritage.collision_type=static_triangle_mesh)
// or by a fallback *_Collision / Collision_* node name. A SPAWN_PLAYER Empty or
// node is read from the same GLB and snapped to the imported drive surface.
bool loadStaticTriangleSceneFromGlb(
    const std::filesystem::path& path,
    std::vector<StaticSceneTriangle>& output,
    StaticTriangleSceneSpawn* spawn,
    std::string& error);

bool loadStaticTriangleSceneSpawnFromObj(
    const std::filesystem::path& path,
    bool blenderDefaultObjCoordinates,
    StaticTriangleSceneSpawn& spawn,
    std::string& error);

// Keeps an authored spawn marker's X/Z but snaps its height to the highest
// walkable triangle directly beneath/through that horizontal location.
bool snapStaticTriangleSceneSpawnToSurface(
    const std::vector<StaticSceneTriangle>& triangles,
    StaticTriangleSceneSpawn& spawn);

} // namespace heritage::physics
