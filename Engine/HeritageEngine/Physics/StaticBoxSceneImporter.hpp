#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "../Core/Math/Math.hpp"
#include "CollisionSystem.hpp"

namespace heritage::physics {

struct StaticBoxSceneDescriptor
{
    std::string name;
    heritage::math::Vec3 center{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 halfExtents{ 0.5f, 0.5f, 0.5f };
    SurfaceMaterial surfaceMaterial = SurfaceMaterial::Default;
    float surfaceWetness = 0.0f;
    float friction = 0.90f;
    float restitution = 0.02f;
};

struct StaticBoxSceneSpawn
{
    heritage::math::Vec3 groundPoint{ 0.0f, 0.0f, 0.0f };
    bool found = false;
    bool explicitMarker = false;
    std::string sourceName;
};

// Reads an OBJ used purely as a simple collision-proxy document. Every OBJ
// object/group that owns faces becomes one axis-aligned static box based on the
// vertices referenced by those faces. This is intentionally a prototype bridge
// for Blender-authored driveable scenes; arbitrary triangle-mesh collision is a
// later world-physics feature.
bool loadStaticBoxSceneFromObj(
    const std::filesystem::path& path,
    bool blenderCoordinates,
    std::vector<StaticBoxSceneDescriptor>& output,
    StaticBoxSceneSpawn* spawn,
    std::string& error);

} // namespace heritage::physics
