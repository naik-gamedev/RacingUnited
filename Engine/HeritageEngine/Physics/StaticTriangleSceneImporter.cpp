#include "StaticTriangleSceneImporter.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace heritage::physics {
namespace {

struct Bounds
{
    heritage::math::Vec3 minimum{
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)()
    };
    heritage::math::Vec3 maximum{
        -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)()
    };
    bool hasVertex = false;
};

std::string trim(const std::string& text)
{
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string lower(const std::string& text)
{
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

heritage::math::Vec3 convertPosition(
    const heritage::math::Vec3& value,
    bool blenderDefaultObjCoordinates)
{
    if (!blenderDefaultObjCoordinates)
        return value;

    // Blender authoring: X right, Y forward, Z up.
    // Blender default OBJ export: X right, Y up, Z = -authoring Y.
    // Heritage native simulation: X right, Y up, Z forward.
    return { value.x, value.y, -value.z };
}

void include(Bounds& bounds, const heritage::math::Vec3& value)
{
    bounds.minimum.x = (std::min)(bounds.minimum.x, value.x);
    bounds.minimum.y = (std::min)(bounds.minimum.y, value.y);
    bounds.minimum.z = (std::min)(bounds.minimum.z, value.z);
    bounds.maximum.x = (std::max)(bounds.maximum.x, value.x);
    bounds.maximum.y = (std::max)(bounds.maximum.y, value.y);
    bounds.maximum.z = (std::max)(bounds.maximum.z, value.z);
    bounds.hasVertex = true;
}

int parsePositionIndex(const std::string& token, std::size_t positionCount)
{
    const std::size_t slash = token.find('/');
    const std::string indexText = slash == std::string::npos
        ? token
        : token.substr(0, slash);
    if (indexText.empty())
        return 0;

    int index = 0;
    try
    {
        index = std::stoi(indexText);
    }
    catch (...)
    {
        return 0;
    }

    if (index < 0)
        index = static_cast<int>(positionCount) + index + 1;
    return index;
}

bool isSpawnMarkerName(const std::string& objectName)
{
    const std::string name = lower(objectName);
    return name.find("spawn_player") != std::string::npos
        || name.find("player_spawn") != std::string::npos
        || name.find("playerspawn") != std::string::npos;
}

void inferSurface(
    const std::string& objectName,
    SurfaceMaterial& material,
    float& wetness)
{
    const std::string name = lower(objectName);
    material = SurfaceMaterial::Default;
    wetness = 0.0f;

    if (name.find("wet_asphalt") != std::string::npos
        || name.find("wetasphalt") != std::string::npos)
    {
        material = SurfaceMaterial::Asphalt;
        wetness = 1.0f;
    }
    else if (name.find("asphalt") != std::string::npos
        || name.find("tarmac") != std::string::npos
        || name.find("road") != std::string::npos)
        material = SurfaceMaterial::Asphalt;
    else if (name.find("gravel") != std::string::npos)
        material = SurfaceMaterial::Gravel;
    else if (name.find("dirt") != std::string::npos)
        material = SurfaceMaterial::Dirt;
    else if (name.find("grass") != std::string::npos)
        material = SurfaceMaterial::Grass;
    else if (name.find("snow") != std::string::npos)
        material = SurfaceMaterial::Snow;
    else if (name.find("ice") != std::string::npos)
        material = SurfaceMaterial::Ice;
    else if (name.find("kerb") != std::string::npos
        || name.find("curb") != std::string::npos)
        material = SurfaceMaterial::Kerb;
    else if (name.find("paint") != std::string::npos
        || name.find("line") != std::string::npos)
        material = SurfaceMaterial::PaintedLine;
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float lengthSquared(const heritage::math::Vec3& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& v,
    const heritage::math::Vec3& fallback = { 0.0f, 1.0f, 0.0f })
{
    const float squared = lengthSquared(v);
    if (squared <= 1.0e-12f)
        return fallback;
    const float inverse = 1.0f / std::sqrt(squared);
    return { v.x * inverse, v.y * inverse, v.z * inverse };
}

bool horizontalPointOnTriangle(
    float x,
    float z,
    const StaticSceneTriangle& triangle,
    float& y)
{
    const float x1 = triangle.a.x;
    const float z1 = triangle.a.z;
    const float x2 = triangle.b.x;
    const float z2 = triangle.b.z;
    const float x3 = triangle.c.x;
    const float z3 = triangle.c.z;
    const float denominator =
        (z2 - z3) * (x1 - x3)
        + (x3 - x2) * (z1 - z3);
    if (std::abs(denominator) <= 1.0e-8f)
        return false;

    const float first =
        ((z2 - z3) * (x - x3)
            + (x3 - x2) * (z - z3)) / denominator;
    const float second =
        ((z3 - z1) * (x - x3)
            + (x1 - x3) * (z - z3)) / denominator;
    const float third = 1.0f - first - second;
    constexpr float epsilon = -1.0e-5f;
    if (first < epsilon || second < epsilon || third < epsilon)
        return false;

    y = first * triangle.a.y
        + second * triangle.b.y
        + third * triangle.c.y;
    return true;
}

bool parseObj(
    const std::filesystem::path& path,
    bool blenderDefaultObjCoordinates,
    std::vector<StaticSceneTriangle>* triangles,
    StaticTriangleSceneSpawn* explicitSpawn,
    std::string& error)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        error = "Could not open static triangle OBJ: " + path.string();
        return false;
    }

    std::vector<heritage::math::Vec3> positions;
    positions.reserve(4096);
    std::string currentObject;
    std::string currentGroup = "COL_DEFAULT";
    std::unordered_map<std::string, Bounds> spawnBounds;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream stream(line);
        std::string token;
        stream >> token;

        if (token == "v")
        {
            heritage::math::Vec3 value{};
            if (!(stream >> value.x >> value.y >> value.z))
                continue;
            positions.push_back(convertPosition(value, blenderDefaultObjCoordinates));
        }
        else if (token == "o")
        {
            std::string remainder;
            std::getline(stream, remainder);
            remainder = trim(remainder);
            currentObject = remainder.empty() ? "COL_UNNAMED" : remainder;
        }
        else if (token == "g")
        {
            std::string remainder;
            std::getline(stream, remainder);
            remainder = trim(remainder);
            currentGroup = remainder.empty() ? "COL_UNNAMED" : remainder;
        }
        else if (token == "f")
        {
            const std::string owner = currentObject.empty()
                ? currentGroup
                : currentObject;
            std::vector<int> indices;
            std::string point;
            while (stream >> point)
            {
                const int index = parsePositionIndex(point, positions.size());
                if (index <= 0 || index > static_cast<int>(positions.size()))
                {
                    error = "Static triangle OBJ contains an invalid vertex index in object/group '"
                        + owner + "'.";
                    return false;
                }
                indices.push_back(index - 1);
            }
            if (indices.size() < 3)
                continue;

            if (isSpawnMarkerName(owner))
            {
                Bounds& bounds = spawnBounds[owner];
                for (const int index : indices)
                    include(bounds, positions[static_cast<std::size_t>(index)]);
                continue;
            }

            if (!triangles)
                continue;

            SurfaceMaterial surface = SurfaceMaterial::Default;
            float wetness = 0.0f;
            inferSurface(owner, surface, wetness);
            for (std::size_t triangleIndex = 1;
                triangleIndex + 1 < indices.size();
                ++triangleIndex)
            {
                StaticSceneTriangle triangle;
                triangle.a = positions[static_cast<std::size_t>(indices[0])];
                triangle.b = positions[static_cast<std::size_t>(indices[triangleIndex])];
                triangle.c = positions[static_cast<std::size_t>(indices[triangleIndex + 1])];
                triangle.normal = normalized(cross(
                    subtract(triangle.b, triangle.a),
                    subtract(triangle.c, triangle.a)));
                triangle.surfaceMaterial = surface;
                triangle.surfaceWetness = wetness;
                if (lengthSquared(cross(
                        subtract(triangle.b, triangle.a),
                        subtract(triangle.c, triangle.a))) > 1.0e-12f)
                {
                    triangles->push_back(triangle);
                }
            }
        }
    }

    if (explicitSpawn)
    {
        for (const auto& entry : spawnBounds)
        {
            if (!entry.second.hasVertex)
                continue;
            const Bounds& bounds = entry.second;
            explicitSpawn->groundPoint = {
                (bounds.minimum.x + bounds.maximum.x) * 0.5f,
                bounds.minimum.y,
                (bounds.minimum.z + bounds.maximum.z) * 0.5f
            };
            explicitSpawn->found = true;
            explicitSpawn->explicitMarker = true;
            explicitSpawn->sourceName = entry.first;
            break;
        }
    }

    return true;
}

} // namespace

bool loadStaticTriangleSceneFromObj(
    const std::filesystem::path& path,
    bool blenderDefaultObjCoordinates,
    std::vector<StaticSceneTriangle>& output,
    StaticTriangleSceneSpawn* spawn,
    std::string& error)
{
    output.clear();
    error.clear();
    if (spawn)
        *spawn = {};

    StaticTriangleSceneSpawn explicitSpawn;
    if (!parseObj(
            path,
            blenderDefaultObjCoordinates,
            &output,
            &explicitSpawn,
            error))
    {
        output.clear();
        return false;
    }

    if (output.empty())
    {
        error = "Static triangle OBJ contained no usable triangles.";
        return false;
    }

    if (spawn && explicitSpawn.found)
    {
        *spawn = explicitSpawn;
        return true;
    }

    if (spawn)
    {
        // OBJ has no scene-origin metadata, but creator scenes are authored
        // around meaningful world coordinates. Prefer the highest walkable
        // surface directly beneath horizontal world origin. This produces a
        // useful deterministic fallback without using a giant mesh AABB top.
        bool foundAtOrigin = false;
        float highestY = -(std::numeric_limits<float>::max)();
        for (const StaticSceneTriangle& triangle : output)
        {
            if (std::abs(triangle.normal.y) < 0.15f)
                continue;
            float candidateY = 0.0f;
            if (!horizontalPointOnTriangle(0.0f, 0.0f, triangle, candidateY))
                continue;
            if (!foundAtOrigin || candidateY > highestY)
            {
                highestY = candidateY;
                foundAtOrigin = true;
            }
        }

        if (foundAtOrigin)
        {
            spawn->groundPoint = { 0.0f, highestY, 0.0f };
            spawn->found = true;
            spawn->explicitMarker = false;
            spawn->sourceName = "triangle-origin";
        }
        else
        {
            bool foundNearest = false;
            float nearestDistanceSquared = (std::numeric_limits<float>::max)();
            heritage::math::Vec3 nearest{};
            for (const StaticSceneTriangle& triangle : output)
            {
                if (std::abs(triangle.normal.y) < 0.15f)
                    continue;
                const heritage::math::Vec3 center{
                    (triangle.a.x + triangle.b.x + triangle.c.x) / 3.0f,
                    (triangle.a.y + triangle.b.y + triangle.c.y) / 3.0f,
                    (triangle.a.z + triangle.b.z + triangle.c.z) / 3.0f
                };
                const float distanceSquared =
                    center.x * center.x + center.z * center.z;
                if (!foundNearest || distanceSquared < nearestDistanceSquared)
                {
                    nearestDistanceSquared = distanceSquared;
                    nearest = center;
                    foundNearest = true;
                }
            }
            if (foundNearest)
            {
                spawn->groundPoint = nearest;
                spawn->found = true;
                spawn->explicitMarker = false;
                spawn->sourceName = "triangle-nearest-origin";
            }
        }
    }

    return true;
}

bool loadStaticTriangleSceneSpawnFromObj(
    const std::filesystem::path& path,
    bool blenderDefaultObjCoordinates,
    StaticTriangleSceneSpawn& spawn,
    std::string& error)
{
    spawn = {};
    error.clear();
    return parseObj(
        path,
        blenderDefaultObjCoordinates,
        nullptr,
        &spawn,
        error);
}

bool snapStaticTriangleSceneSpawnToSurface(
    const std::vector<StaticSceneTriangle>& triangles,
    StaticTriangleSceneSpawn& spawn)
{
    if (!spawn.found)
        return false;

    bool found = false;
    float highestY = -(std::numeric_limits<float>::max)();
    for (const StaticSceneTriangle& triangle : triangles)
    {
        if (std::abs(triangle.normal.y) < 0.15f)
            continue;
        float candidateY = 0.0f;
        if (!horizontalPointOnTriangle(
                spawn.groundPoint.x,
                spawn.groundPoint.z,
                triangle,
                candidateY))
        {
            continue;
        }
        if (!found || candidateY > highestY)
        {
            highestY = candidateY;
            found = true;
        }
    }

    if (found)
        spawn.groundPoint.y = highestY;
    return found;
}

} // namespace heritage::physics
