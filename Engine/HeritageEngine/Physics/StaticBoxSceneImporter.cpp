#include "StaticBoxSceneImporter.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

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
    bool blenderCoordinates)
{
    if (!blenderCoordinates)
        return value;

    // Racing United authoring convention follows Blender exactly:
    // X = left/right, Y = forward/backward, Z = height. Blender's default
    // OBJ export writes X/right, Y/up and -Z/forward; native Heritage Engine
    // uses X/right, Y/up, Z/forward. Reflect OBJ Z at the import boundary.
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

int parsePositionIndex(
    const std::string& token,
    std::size_t positionCount)
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

bool isPreferredGroundName(const std::string& objectName)
{
    const std::string name = lower(objectName);
    return name.find("road") != std::string::npos
        || name.find("asphalt") != std::string::npos
        || name.find("tarmac") != std::string::npos
        || name.find("ground") != std::string::npos
        || name.find("floor") != std::string::npos
        || name.find("pavement") != std::string::npos;
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
        || name.find("tarmac") != std::string::npos)
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

} // namespace

bool loadStaticBoxSceneFromObj(
    const std::filesystem::path& path,
    bool blenderCoordinates,
    std::vector<StaticBoxSceneDescriptor>& output,
    StaticBoxSceneSpawn* spawn,
    std::string& error)
{
    output.clear();
    error.clear();
    if (spawn)
        *spawn = {};

    std::ifstream file(path);
    if (!file.is_open())
    {
        error = "Could not open collision OBJ: " + path.string();
        return false;
    }

    std::vector<heritage::math::Vec3> positions;
    positions.reserve(1024);

    std::vector<std::string> groupOrder;
    std::unordered_map<std::string, Bounds> groupBounds;

    // OBJ distinguishes object names (o) from group names (g). Blender may emit
    // both for the same mesh, and material/export grouping can change the g name
    // after a meaningful object name such as SPAWN_PLAYER. Collision authoring in
    // Racing United is object-centric, so faces belong to the current object when
    // one exists; group names are only a fallback for OBJ files without objects.
    std::string currentObject;
    std::string currentGroup = "COL_DEFAULT";

    const auto ensureOwner = [&](const std::string& owner) {
        if (groupBounds.find(owner) == groupBounds.end())
        {
            groupOrder.push_back(owner);
            groupBounds.emplace(owner, Bounds{});
        }
    };

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
            positions.push_back(convertPosition(value, blenderCoordinates));
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
            const std::string& owner = currentObject.empty()
                ? currentGroup
                : currentObject;
            ensureOwner(owner);
            Bounds& bounds = groupBounds[owner];
            std::string point;
            while (stream >> point)
            {
                const int positionIndex = parsePositionIndex(point, positions.size());
                if (positionIndex <= 0
                    || positionIndex > static_cast<int>(positions.size()))
                {
                    error = "Collision OBJ contains an invalid vertex index in object/group '"
                        + owner + "'.";
                    output.clear();
                    return false;
                }
                include(bounds, positions[static_cast<std::size_t>(positionIndex - 1)]);
            }
        }
    }

    constexpr float kMinimumHalfExtent = 0.005f;
    float bestGroundScore = -1.0f;
    bool bestGroundPreferred = false;
    StaticBoxSceneSpawn inferredSpawn;

    for (const std::string& name : groupOrder)
    {
        const auto found = groupBounds.find(name);
        if (found == groupBounds.end() || !found->second.hasVertex)
            continue;

        const Bounds& bounds = found->second;
        const heritage::math::Vec3 center{
            (bounds.minimum.x + bounds.maximum.x) * 0.5f,
            (bounds.minimum.y + bounds.maximum.y) * 0.5f,
            (bounds.minimum.z + bounds.maximum.z) * 0.5f
        };
        const heritage::math::Vec3 rawHalfExtents{
            (bounds.maximum.x - bounds.minimum.x) * 0.5f,
            (bounds.maximum.y - bounds.minimum.y) * 0.5f,
            (bounds.maximum.z - bounds.minimum.z) * 0.5f
        };

        // OBJ has no usable Blender object-origin/pivot contract. A tiny mesh/group
        // named SPAWN_PLAYER is therefore treated as authored spawn metadata. Its
        // lowest vertex is the road/ground height; it never becomes a collider.
        if (isSpawnMarkerName(name))
        {
            if (spawn)
            {
                spawn->groundPoint = { center.x, bounds.minimum.y, center.z };
                spawn->found = true;
                spawn->explicitMarker = true;
                spawn->sourceName = name;
            }
            continue;
        }

        StaticBoxSceneDescriptor descriptor;
        descriptor.name = name;
        descriptor.center = center;
        descriptor.halfExtents = {
            (std::max)(kMinimumHalfExtent, rawHalfExtents.x),
            (std::max)(kMinimumHalfExtent, rawHalfExtents.y),
            (std::max)(kMinimumHalfExtent, rawHalfExtents.z)
        };
        inferSurface(name, descriptor.surfaceMaterial, descriptor.surfaceWetness);

        // Fallback spawn for existing creator scenes that do not yet contain an
        // explicit marker. Prefer a road/ground-named, broad, flat proxy box.
        // This is intentionally only a convenience heuristic for the OBJ bridge.
        if (!spawn || !spawn->found)
        {
            const float horizontalArea =
                (std::max)(0.0f, rawHalfExtents.x * 2.0f)
                * (std::max)(0.0f, rawHalfExtents.z * 2.0f);
            const float fullHeight = (std::max)(0.01f, rawHalfExtents.y * 2.0f);
            const float flatness = 1.0f / fullHeight;
            const float score = horizontalArea * flatness;
            const bool preferred = isPreferredGroundName(name);
            if ((preferred && !bestGroundPreferred)
                || (preferred == bestGroundPreferred && score > bestGroundScore))
            {
                bestGroundPreferred = preferred;
                bestGroundScore = score;
                inferredSpawn.groundPoint = {
                    center.x,
                    bounds.maximum.y,
                    center.z
                };
                inferredSpawn.found = true;
                inferredSpawn.explicitMarker = false;
                inferredSpawn.sourceName = name;
            }
        }

        output.push_back(std::move(descriptor));
    }

    if (spawn && !spawn->found && inferredSpawn.found)
        *spawn = inferredSpawn;

    if (output.empty())
    {
        error = "Collision OBJ contained no object/group with referenced faces.";
        return false;
    }

    return true;
}

} // namespace heritage::physics
