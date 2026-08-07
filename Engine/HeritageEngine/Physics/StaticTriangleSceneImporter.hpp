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
