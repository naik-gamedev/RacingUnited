#include "StaticTriangleSceneImporter.hpp"

#include "../Graphics/GltfSceneData.hpp"

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
    float& wetness);

const heritage::graphics::AssetMetadataValue* metadataValue(
    const heritage::graphics::AssetMetadataMap& metadata,
    const char* key)
{
    const auto found = metadata.find(key);
    return found != metadata.end() ? &found->second : nullptr;
}

std::string metadataString(
    const heritage::graphics::AssetMetadataMap& metadata,
    const char* key)
{
    const auto* value = metadataValue(metadata, key);
    return value && value->type == heritage::graphics::AssetMetadataValueType::String
        ? value->stringValue
        : std::string{};
}

float metadataNumber(
    const heritage::graphics::AssetMetadataMap& metadata,
    const char* key,
    float fallback)
{
    const auto* value = metadataValue(metadata, key);
    return value && value->type == heritage::graphics::AssetMetadataValueType::Number
        ? static_cast<float>(value->numberValue)
        : fallback;
}

bool metadataDouble(
    const heritage::graphics::AssetMetadataMap& metadata,
    const char* key,
    double& value)
{
    const auto* metadataEntry = metadataValue(metadata, key);
    if (!metadataEntry
        || metadataEntry->type != heritage::graphics::AssetMetadataValueType::Number
        || !std::isfinite(metadataEntry->numberValue))
    {
        return false;
    }
    value = metadataEntry->numberValue;
    return true;
}

void applySurfaceDouble(
    const heritage::graphics::AssetMetadataMap& metadata,
    const char* key,
    double minimum,
    double maximum,
    double& destination,
    bool& authored)
{
    double value = 0.0;
    if (!metadataDouble(metadata, key, value)
        || value < minimum || value > maximum)
    {
        return;
    }
    destination = value;
    authored = true;
}

void applySurfacePropertyMetadata(
    const heritage::graphics::AssetMetadataMap& metadata,
    heritage::physics::SurfaceMaterialProperties& properties)
{
    double temperature = 0.0;
    if (metadataDouble(metadata, "heritage.surface.temperature_c", temperature)
        && temperature >= -100.0 && temperature <= 150.0)
    {
        properties.hasAuthoredSurfaceTemperature = true;
        properties.authoredSurfaceTemperatureC = temperature;
    }

    if (!properties.deformable.enabled)
        return;

    bool authored = properties.deformable.authored;
    applySurfaceDouble(metadata, "heritage.surface.density_kg_m3",
        25.0, 5000.0, properties.deformable.densityKgM3, authored);
    applySurfaceDouble(metadata, "heritage.surface.loose_depth_m",
        0.0, 2.0, properties.deformable.initialLooseDepthM, authored);
    applySurfaceDouble(metadata, "heritage.surface.moisture",
        0.0, 1.0, properties.deformable.initialMoisture, authored);
    applySurfaceDouble(metadata, "heritage.surface.bekker_kc",
        0.0, 1.0e8, properties.deformable.bekkerKc, authored);
    applySurfaceDouble(metadata, "heritage.surface.bekker_kphi",
        0.0, 1.0e9, properties.deformable.bekkerKphi, authored);
    applySurfaceDouble(metadata, "heritage.surface.sinkage_exponent",
        0.20, 4.0, properties.deformable.sinkageExponent, authored);
    applySurfaceDouble(metadata, "heritage.surface.cohesion_pa",
        0.0, 2.0e6, properties.deformable.cohesionPa, authored);
    applySurfaceDouble(metadata, "heritage.surface.friction_angle_deg",
        0.0, 60.0, properties.deformable.frictionAngleDegrees, authored);
    applySurfaceDouble(metadata, "heritage.surface.shear_modulus_m",
        0.001, 1.0, properties.deformable.shearDeformationModulusM, authored);
    applySurfaceDouble(metadata, "heritage.surface.compaction_stiffness_gain",
        0.0, 20.0, properties.deformable.compactionStiffnessGain, authored);
    applySurfaceDouble(metadata, "heritage.surface.compaction_shear_gain",
        0.0, 10.0, properties.deformable.compactionShearGain, authored);
    applySurfaceDouble(metadata, "heritage.surface.plastic_rut_fraction",
        0.0, 1.0, properties.deformable.plasticRutFraction, authored);
    applySurfaceDouble(metadata, "heritage.surface.compaction_rate_hz",
        0.0, 20.0, properties.deformable.compactionRateHz, authored);
    applySurfaceDouble(metadata, "heritage.surface.loose_depth_loss_per_compaction_m",
        0.0, 2.0, properties.deformable.looseDepthLossPerCompactionM, authored);
    applySurfaceDouble(metadata, "heritage.surface.mf_friction_scale",
        0.0, 1.0, properties.deformable.mfBaseFrictionScale, authored);
    applySurfaceDouble(metadata, "heritage.surface.stiffness_scale",
        0.0, 2.0, properties.deformable.baseStiffnessScale, authored);
    applySurfaceDouble(metadata, "heritage.surface.rolling_resistance_scale",
        0.0, 20.0, properties.deformable.rollingResistanceScale, authored);
    applySurfaceDouble(metadata, "heritage.surface.relaxation_scale",
        0.1, 20.0, properties.deformable.relaxationScale, authored);
    properties.deformable.authored = authored;
}

bool surfaceFromText(
    const std::string& text,
    SurfaceMaterial& material,
    float& wetness)
{
    const std::string name = lower(text);
    if (name.empty())
        return false;

    if (name == "wet_asphalt" || name == "wetasphalt")
    {
        material = SurfaceMaterial::Asphalt;
        wetness = 1.0f;
        return true;
    }
    if (name == "asphalt" || name == "tarmac" || name == "road")
        material = SurfaceMaterial::Asphalt;
    else if (name == "gravel")
        material = SurfaceMaterial::Gravel;
    else if (name == "mud")
        material = SurfaceMaterial::Mud;
    else if (name == "sand")
        material = SurfaceMaterial::Sand;
    else if (name == "soft_soil" || name == "softsoil")
        material = SurfaceMaterial::SoftSoil;
    else if (name == "deep_snow" || name == "deepsnow" || name == "powder_snow")
        material = SurfaceMaterial::DeepSnow;
    else if (name == "dirt" || name == "soil")
        material = SurfaceMaterial::Dirt;
    else if (name == "grass")
        material = SurfaceMaterial::Grass;
    else if (name == "snow")
        material = SurfaceMaterial::Snow;
    else if (name == "ice")
        material = SurfaceMaterial::Ice;
    else if (name == "kerb" || name == "curb")
        material = SurfaceMaterial::Kerb;
    else if (name == "paint" || name == "painted_line" || name == "line")
        material = SurfaceMaterial::PaintedLine;
    else
        return false;
    return true;
}

void inferGlbSurface(
    const heritage::graphics::GlbStaticCollisionScene& scene,
    int nodeIndex,
    SurfaceMaterial& material,
    float& wetness,
    heritage::physics::SurfaceMaterialProperties& properties)
{
    material = SurfaceMaterial::Default;
    wetness = 0.0f;

    // Child-most material identity wins, matching normal scene-authoring
    // inheritance. Physical parameter overrides are applied root -> child so a
    // broad parent terrain profile can be refined on one collision object.
    int current = nodeIndex;
    while (current >= 0 && static_cast<std::size_t>(current) < scene.nodes.size())
    {
        const auto& node = scene.nodes[static_cast<std::size_t>(current)];
        const std::string authoredSurface = metadataString(node.metadata, "heritage.surface");
        const std::string alternateSurface = metadataString(node.metadata, "heritage.surface_material");
        if (surfaceFromText(
                !authoredSurface.empty() ? authoredSurface : alternateSurface,
                material,
                wetness))
        {
            break;
        }

        SurfaceMaterial nameMaterial = SurfaceMaterial::Default;
        float nameWetness = 0.0f;
        inferSurface(node.name, nameMaterial, nameWetness);
        if (nameMaterial != SurfaceMaterial::Default || nameWetness > 0.0f)
        {
            material = nameMaterial;
            wetness = nameWetness;
            break;
        }
        current = node.parentIndex;
    }

    properties = defaultSurfaceMaterialProperties(material);

    std::vector<int> hierarchy;
    current = nodeIndex;
    while (current >= 0 && static_cast<std::size_t>(current) < scene.nodes.size())
    {
        hierarchy.push_back(current);
        current = scene.nodes[static_cast<std::size_t>(current)].parentIndex;
    }
    std::reverse(hierarchy.begin(), hierarchy.end());

    for (const int index : hierarchy)
    {
        const auto& metadata = scene.nodes[static_cast<std::size_t>(index)].metadata;
        wetness = std::clamp(
            metadataNumber(metadata, "heritage.wetness", wetness),
            0.0f,
            1.0f);
        applySurfacePropertyMetadata(metadata, properties);
    }

    if (!validSurfaceMaterialProperties(properties))
        properties = defaultSurfaceMaterialProperties(material);
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
    else if (name.find("deep_snow") != std::string::npos
        || name.find("deepsnow") != std::string::npos
        || name.find("powder_snow") != std::string::npos
        || name.find("snow_deep") != std::string::npos)
        material = SurfaceMaterial::DeepSnow;
    else if (name.find("mud") != std::string::npos)
        material = SurfaceMaterial::Mud;
    else if (name.find("sand") != std::string::npos)
        material = SurfaceMaterial::Sand;
    else if (name.find("soft_soil") != std::string::npos
        || name.find("softsoil") != std::string::npos
        || name.find("soil_soft") != std::string::npos)
        material = SurfaceMaterial::SoftSoil;
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


void resolveAutomaticSpawn(
    const std::vector<StaticSceneTriangle>& triangles,
    StaticTriangleSceneSpawn& spawn)
{
    if (spawn.found)
        return;

    bool foundAtOrigin = false;
    float highestY = -(std::numeric_limits<float>::max)();
    for (const StaticSceneTriangle& triangle : triangles)
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
        spawn.groundPoint = { 0.0f, highestY, 0.0f };
        spawn.found = true;
        spawn.explicitMarker = false;
        spawn.sourceName = "triangle-origin";
        return;
    }

    bool foundNearest = false;
    float nearestDistanceSquared = (std::numeric_limits<float>::max)();
    heritage::math::Vec3 nearest{};
    for (const StaticSceneTriangle& triangle : triangles)
    {
        if (std::abs(triangle.normal.y) < 0.15f)
            continue;
        const heritage::math::Vec3 center{
            (triangle.a.x + triangle.b.x + triangle.c.x) / 3.0f,
            (triangle.a.y + triangle.b.y + triangle.c.y) / 3.0f,
            (triangle.a.z + triangle.b.z + triangle.c.z) / 3.0f
        };
        const float distanceSquared = center.x * center.x + center.z * center.z;
        if (!foundNearest || distanceSquared < nearestDistanceSquared)
        {
            nearestDistanceSquared = distanceSquared;
            nearest = center;
            foundNearest = true;
        }
    }

    if (foundNearest)
    {
        spawn.groundPoint = nearest;
        spawn.found = true;
        spawn.explicitMarker = false;
        spawn.sourceName = "triangle-nearest-origin";
    }
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
                triangle.surfaceProperties = defaultSurfaceMaterialProperties(surface);
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
        resolveAutomaticSpawn(output, *spawn);

    return true;
}


bool loadStaticTriangleSceneFromGlb(
    const std::filesystem::path& path,
    std::vector<StaticSceneTriangle>& output,
    StaticTriangleSceneSpawn* spawn,
    std::string& error)
{
    output.clear();
    error.clear();
    if (spawn)
        *spawn = {};

    heritage::graphics::GlbStaticCollisionScene extracted;
    if (!heritage::graphics::extractGlbStaticCollisionScene(
            path,
            extracted,
            error))
    {
        return false;
    }

    output.reserve(extracted.triangles.size());
    for (const auto& source : extracted.triangles)
    {
        StaticSceneTriangle triangle;
        triangle.a = source.a;
        triangle.b = source.b;
        triangle.c = source.c;
        const heritage::math::Vec3 crossValue = cross(
            subtract(triangle.b, triangle.a),
            subtract(triangle.c, triangle.a));
        if (lengthSquared(crossValue) <= 1.0e-12f)
            continue;

        triangle.normal = normalized(crossValue);
        inferGlbSurface(
            extracted,
            source.nodeIndex,
            triangle.surfaceMaterial,
            triangle.surfaceWetness,
            triangle.surfaceProperties);
        output.push_back(triangle);
    }

    if (output.empty())
    {
        error = "Static triangle GLB contained no usable marked collision triangles.";
        return false;
    }

    if (spawn && extracted.spawnFound)
    {
        spawn->groundPoint = extracted.spawnPosition;
        spawn->found = true;
        spawn->explicitMarker = true;
        spawn->sourceName = extracted.spawnName.empty()
            ? "SPAWN_PLAYER"
            : extracted.spawnName;
    }

    if (spawn)
    {
        if (spawn->found)
            snapStaticTriangleSceneSpawnToSurface(output, *spawn);
        else
            resolveAutomaticSpawn(output, *spawn);
    }

    error.clear();
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
