#include "EntitySceneDocument.hpp"

#include "EntityPrefabDocument.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace heritage::entities {
namespace {

struct EntityDefinition
{
    std::string key;
    int sectionLine = 0;
    std::unordered_map<std::string, std::pair<std::string, int>> values;
};

std::string trim(std::string value)
{
    const auto notSpace = [](unsigned char character) {
        return !std::isspace(character);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

bool parseBoolean(const std::string& value, bool& output)
{
    const std::string normalized = lower(trim(value));
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
    {
        output = true;
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
    {
        output = false;
        return true;
    }
    return false;
}

bool parseVec3(const std::string& value, heritage::math::Vec3& output)
{
    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');

    std::istringstream stream(normalized);
    heritage::math::Vec3 parsed{};
    std::string trailing;
    if (!(stream >> parsed.x >> parsed.y >> parsed.z) || (stream >> trailing))
        return false;

    output = parsed;
    return true;
}

bool parseColor(const std::string& value, heritage::math::Vec3& output)
{
    if (!parseVec3(value, output))
        return false;

    output.x = (std::max)(0.0f, (std::min)(1.0f, output.x));
    output.y = (std::max)(0.0f, (std::min)(1.0f, output.y));
    output.z = (std::max)(0.0f, (std::min)(1.0f, output.z));
    return true;
}

bool parseUnsigned64(const std::string& value, std::uint64_t& output)
{
    const std::string clean = trim(value);
    if (clean.empty())
        return false;

    std::size_t consumed = 0;
    try
    {
        const unsigned long long parsed = std::stoull(clean, &consumed, 10);
        if (consumed != clean.size() || parsed == 0)
            return false;
        output = static_cast<std::uint64_t>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::vector<std::string> splitTags(const std::string& value)
{
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ','))
    {
        item = trim(item);
        if (!item.empty()
            && std::find(result.begin(), result.end(), item) == result.end())
        {
            result.push_back(item);
        }
    }
    return result;
}

bool validEntityKey(const std::string& key)
{
    if (key.empty())
        return false;

    return std::all_of(
        key.begin(),
        key.end(),
        [](unsigned char character) {
            return std::isalnum(character)
                || character == '_'
                || character == '-'
                || character == '.';
        });
}

bool parsePrimitiveType(
    const std::string& value,
    DebugPrimitiveType& output)
{
    const std::string normalized = lower(trim(value));
    if (normalized == "box")
    {
        output = DebugPrimitiveType::Box;
        return true;
    }
    if (normalized == "cylinder")
    {
        output = DebugPrimitiveType::Cylinder;
        return true;
    }
    if (normalized == "sphere")
    {
        output = DebugPrimitiveType::Sphere;
        return true;
    }
    return false;
}

const char* primitiveName(DebugPrimitiveType type)
{
    switch (type)
    {
    case DebugPrimitiveType::Cylinder:
        return "cylinder";
    case DebugPrimitiveType::Sphere:
        return "sphere";
    case DebugPrimitiveType::Box:
    default:
        return "box";
    }
}

std::string vec3Text(const heritage::math::Vec3& value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6)
        << value.x << ", " << value.y << ", " << value.z;
    return stream.str();
}

void rollback(
    EntityRegistry* registry,
    std::vector<EntityHandle>& handles)
{
    if (!registry)
    {
        handles.clear();
        return;
    }

    for (auto iterator = handles.rbegin(); iterator != handles.rend(); ++iterator)
    {
        if (registry->exists(*iterator))
            registry->destroy(*iterator);
    }
    handles.clear();
}

std::string lineError(
    const std::filesystem::path& path,
    int line,
    const std::string& message)
{
    return message + "\nLine " + std::to_string(line) + " in:\n" + path.string();
}

bool safePrefabRelativePath(const std::filesystem::path& relativePath)
{
    if (relativePath.empty()
        || relativePath.is_absolute()
        || relativePath.has_root_name())
    {
        return false;
    }

    const std::filesystem::path normalized = relativePath.lexically_normal();
    for (const auto& component : normalized)
    {
        if (component == "..")
            return false;
    }

    return lower(normalized.extension().string()) == ".hprefab";
}

std::filesystem::path prefabRootForDocument(
    const std::filesystem::path& documentPath)
{
    std::filesystem::path current = documentPath.parent_path();
    while (!current.empty())
    {
        const std::string folder = lower(current.filename().string());
        if (folder == "scenes" || folder == "prefabs")
            return (current.parent_path() / "Prefabs").lexically_normal();

        const std::filesystem::path parent = current.parent_path();
        if (parent == current)
            break;
        current = parent;
    }

    return {};
}

} // namespace

bool EntitySceneDocument::load(
    const std::filesystem::path& path,
    const std::string& expectedSceneId,
    EntityRegistry* registry,
    EntitySceneDocumentInfo& info,
    std::vector<EntityHandle>& createdEntities,
    std::string& errorMessage)
{
    createdEntities.clear();
    info = {};

    std::ifstream file(path);
    if (!file)
    {
        errorMessage = "Module scene document was not found:\n" + path.string();
        return false;
    }

    std::unordered_map<std::string, std::pair<std::string, int>> metadata;
    std::vector<EntityDefinition> definitions;
    std::unordered_set<std::string> definitionKeys;
    EntityDefinition* currentEntity = nullptr;

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line))
    {
        ++lineNumber;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        if (line.front() == '[')
        {
            if (line.back() != ']')
            {
                errorMessage = lineError(path, lineNumber, "Unclosed scene-document section header.");
                return false;
            }

            const std::string section = trim(line.substr(1, line.size() - 2));
            const std::string prefix = "entity:";
            if (lower(section.substr(0, (std::min)(section.size(), prefix.size()))) != prefix)
            {
                errorMessage = lineError(
                    path,
                    lineNumber,
                    "Unsupported section '[" + section + "]'. Expected [entity:scene_local_key].");
                return false;
            }

            const std::string key = trim(section.substr(prefix.size()));
            if (!validEntityKey(key))
            {
                errorMessage = lineError(
                    path,
                    lineNumber,
                    "Invalid entity section key '" + key
                    + "'. Use letters, numbers, underscore, dash or dot.");
                return false;
            }
            if (!definitionKeys.insert(key).second)
            {
                errorMessage = lineError(
                    path,
                    lineNumber,
                    "Duplicate entity section key '" + key + "'.");
                return false;
            }

            definitions.push_back({});
            definitions.back().key = key;
            definitions.back().sectionLine = lineNumber;
            currentEntity = &definitions.back();
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos)
        {
            errorMessage = lineError(path, lineNumber, "Malformed scene-document line; expected key = value.");
            return false;
        }

        const std::string key = lower(trim(line.substr(0, separator)));
        const std::string value = trim(line.substr(separator + 1));
        if (key.empty())
        {
            errorMessage = lineError(path, lineNumber, "Scene-document property name cannot be empty.");
            return false;
        }

        auto& destination = currentEntity ? currentEntity->values : metadata;
        if (destination.contains(key))
        {
            errorMessage = lineError(
                path,
                lineNumber,
                "Duplicate property '" + key + "'.");
            return false;
        }
        destination.emplace(key, std::make_pair(value, lineNumber));
    }

    const std::unordered_set<std::string> allowedMetadata = {
        "id", "type", "clear_color", "overlay", "title", "subtitle", "text"
    };
    for (const auto& [key, value] : metadata)
    {
        if (!allowedMetadata.contains(key))
        {
            errorMessage = lineError(
                path,
                value.second,
                "Unknown scene property '" + key + "'.");
            return false;
        }
    }

    const auto metadataValue = [&](const char* key) -> std::string {
        const auto found = metadata.find(key);
        return found == metadata.end() ? std::string{} : found->second.first;
    };

    info.id = metadataValue("id");
    if (info.id.empty())
        info.id = expectedSceneId;
    if (!expectedSceneId.empty() && info.id != expectedSceneId)
    {
        errorMessage = "Scene document ID mismatch. Requested '" + expectedSceneId
            + "' but the document declares '" + info.id + "'.";
        return false;
    }

    info.type = lower(metadataValue("type"));
    if (info.type.empty())
        info.type = definitions.empty() ? "empty" : "entities";
    if (info.type != "empty" && info.type != "entities")
    {
        errorMessage = "Scene '" + info.id + "' requested unsupported type '"
            + info.type + "'.\n\nSupported today: empty, entities";
        return false;
    }
    if (info.type == "empty" && !definitions.empty())
    {
        errorMessage = "Scene '" + info.id
            + "' declares type=empty but also contains entity sections.";
        return false;
    }

    const auto color = metadata.find("clear_color");
    if (color != metadata.end() && !parseColor(color->second.first, info.clearColor))
    {
        errorMessage = lineError(
            path,
            color->second.second,
            "Invalid clear_color. Expected three numbers such as 0.01, 0.02, 0.03.");
        return false;
    }

    const auto overlay = metadata.find("overlay");
    if (overlay != metadata.end()
        && !parseBoolean(overlay->second.first, info.showOverlay))
    {
        errorMessage = lineError(
            path,
            overlay->second.second,
            "Invalid overlay value. Expected true or false.");
        return false;
    }

    info.title = metadataValue("title");
    info.subtitle = metadataValue("subtitle");
    info.text = metadataValue("text");
    info.entityCount = definitions.size();

    if (definitions.empty())
    {
        errorMessage.clear();
        return true;
    }
    if (!registry)
    {
        errorMessage = "Scene '" + info.id
            + "' contains entities, but the active runtime did not provide an EntityRegistry.";
        return false;
    }

    const std::unordered_set<std::string> allowedEntityValues = {
        "name", "persistent_id", "parent", "tags",
        "position", "rotation", "scale", "prefab", "name_prefix",
        "mesh", "mesh_color", "mesh_visible", "mesh_normalize", "mesh_double_sided",
        "mesh_blender_coordinates",
        "debug_primitive", "debug_color", "debug_visible"
    };

    std::unordered_map<std::string, EntityHandle> handleByKey;
    handleByKey.reserve(definitions.size());

    for (const EntityDefinition& definition : definitions)
    {
        for (const auto& [key, value] : definition.values)
        {
            if (!allowedEntityValues.contains(key))
            {
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    value.second,
                    "Unknown entity property '" + key
                    + "' in [entity:" + definition.key + "].");
                return false;
            }
        }

        const auto nameIt = definition.values.find("name");
        const auto prefabIt = definition.values.find("prefab");
        const auto persistentIt = definition.values.find("persistent_id");

        EntityHandle handle = InvalidEntity;
        if (prefabIt != definition.values.end())
        {
            if (persistentIt != definition.values.end())
            {
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    persistentIt->second.second,
                    "Prefab instances receive fresh persistent IDs and cannot declare persistent_id in [entity:"
                        + definition.key + "].");
                return false;
            }

            const auto meshIt = definition.values.find("mesh");
            const auto primitiveIt = definition.values.find("debug_primitive");
            if (meshIt != definition.values.end()
                || primitiveIt != definition.values.end())
            {
                const int line = meshIt != definition.values.end()
                    ? meshIt->second.second
                    : primitiveIt->second.second;
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    line,
                    "A prefab instance cannot also declare mesh/debug_primitive in [entity:"
                        + definition.key + "]. Put those components inside the .hprefab document.");
                return false;
            }

            const std::filesystem::path relativePrefab =
                std::filesystem::path(trim(prefabIt->second.first));
            const std::filesystem::path prefabRoot = prefabRootForDocument(path);
            if (prefabRoot.empty() || !safePrefabRelativePath(relativePrefab))
            {
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    prefabIt->second.second,
                    "Unsafe prefab path in [entity:" + definition.key
                        + "]. Use a module-Prefabs-relative .hprefab path without '..'.");
                return false;
            }

            PrefabInstantiationOptions options;
            options.overrideRootTransform = false;
            if (nameIt != definition.values.end())
                options.rootName = nameIt->second.first;

            const auto prefixIt = definition.values.find("name_prefix");
            if (prefixIt != definition.values.end())
                options.namePrefix = prefixIt->second.first;

            PrefabInstantiationResult prefabResult;
            std::string prefabError;
            if (!EntityPrefabDocument::instantiate(
                    prefabRoot / relativePrefab,
                    *registry,
                    options,
                    prefabResult,
                    prefabError))
            {
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    prefabIt->second.second,
                    "Could not instantiate prefab for [entity:"
                        + definition.key + "]:\n" + prefabError);
                return false;
            }

            handle = prefabResult.root;
            createdEntities.insert(
                createdEntities.end(),
                prefabResult.entities.begin(),
                prefabResult.entities.end());
        }
        else
        {
            const std::string name = nameIt == definition.values.end()
                ? definition.key
                : nameIt->second.first;

            if (persistentIt != definition.values.end())
            {
                std::uint64_t persistentId = 0;
                if (!parseUnsigned64(persistentIt->second.first, persistentId))
                {
                    rollback(registry, createdEntities);
                    errorMessage = lineError(
                        path,
                        persistentIt->second.second,
                        "Invalid persistent_id in [entity:" + definition.key
                        + "]. Expected a positive integer.");
                    return false;
                }
                handle = registry->createWithPersistentId(name, persistentId);
            }
            else
            {
                handle = registry->create(name);
            }

            if (handle == InvalidEntity)
            {
                rollback(registry, createdEntities);
                errorMessage = "Could not create [entity:" + definition.key + "]: "
                    + registry->lastError();
                return false;
            }

            createdEntities.push_back(handle);
        }

        handleByKey.emplace(definition.key, handle);
    }

    for (const EntityDefinition& definition : definitions)
    {
        const EntityHandle handle = handleByKey.at(definition.key);
        const auto applyVec3 = [&](const char* property, auto setter) -> bool {
            const auto found = definition.values.find(property);
            if (found == definition.values.end())
                return true;

            heritage::math::Vec3 value{};
            if (!parseVec3(found->second.first, value))
            {
                errorMessage = lineError(
                    path,
                    found->second.second,
                    "Invalid " + std::string(property)
                    + " in [entity:" + definition.key
                    + "]. Expected three numbers.");
                return false;
            }
            if (!(registry->*setter)(handle, value))
            {
                errorMessage = "Could not apply " + std::string(property)
                    + " to [entity:" + definition.key + "]: "
                    + registry->lastError();
                return false;
            }
            return true;
        };

        if (!applyVec3("position", &EntityRegistry::setPosition)
            || !applyVec3("rotation", &EntityRegistry::setRotationDegrees)
            || !applyVec3("scale", &EntityRegistry::setScale))
        {
            rollback(registry, createdEntities);
            return false;
        }

        const auto tagsIt = definition.values.find("tags");
        if (tagsIt != definition.values.end())
        {
            for (const std::string& tag : splitTags(tagsIt->second.first))
            {
                if (!registry->addTag(handle, tag))
                {
                    rollback(registry, createdEntities);
                    errorMessage = "Could not add tag '" + tag
                        + "' to [entity:" + definition.key + "]: "
                        + registry->lastError();
                    return false;
                }
            }
        }

        const auto meshIt = definition.values.find("mesh");
        const auto meshColorIt = definition.values.find("mesh_color");
        const auto meshVisibleIt = definition.values.find("mesh_visible");
        const auto meshNormalizeIt = definition.values.find("mesh_normalize");
        const auto meshDoubleSidedIt = definition.values.find("mesh_double_sided");
        const auto meshBlenderCoordinatesIt = definition.values.find("mesh_blender_coordinates");
        if (meshIt == definition.values.end()
            && (meshColorIt != definition.values.end()
                || meshVisibleIt != definition.values.end()
                || meshNormalizeIt != definition.values.end()
                || meshDoubleSidedIt != definition.values.end()
                || meshBlenderCoordinatesIt != definition.values.end()))
        {
            rollback(registry, createdEntities);
            int line = meshColorIt != definition.values.end() ? meshColorIt->second.second
                : meshVisibleIt != definition.values.end() ? meshVisibleIt->second.second
                : meshNormalizeIt != definition.values.end() ? meshNormalizeIt->second.second
                : meshDoubleSidedIt != definition.values.end() ? meshDoubleSidedIt->second.second
                : meshBlenderCoordinatesIt->second.second;
            errorMessage = lineError(
                path,
                line,
                "mesh_color/mesh_visible/mesh_normalize/mesh_double_sided/mesh_blender_coordinates requires mesh in [entity:"
                    + definition.key + "].");
            return false;
        }

        if (meshIt != definition.values.end())
        {
            const std::string meshPath = trim(meshIt->second.first);
            heritage::math::Vec3 meshColor{ 0.72f, 0.78f, 0.88f };
            if (meshColorIt != definition.values.end()
                && !parseColor(meshColorIt->second.first, meshColor))
            {
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    meshColorIt->second.second,
                    "Invalid mesh_color in [entity:" + definition.key
                    + "]. Expected three numbers.");
                return false;
            }

            bool meshNormalize = false;
            if (meshNormalizeIt != definition.values.end()
                && !parseBoolean(meshNormalizeIt->second.first, meshNormalize))
            {
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    meshNormalizeIt->second.second,
                    "Invalid mesh_normalize in [entity:" + definition.key
                    + "]. Expected true or false.");
                return false;
            }

            bool meshDoubleSided = false;
            if (meshDoubleSidedIt != definition.values.end()
                && !parseBoolean(meshDoubleSidedIt->second.first, meshDoubleSided))
            {
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    meshDoubleSidedIt->second.second,
                    "Invalid mesh_double_sided in [entity:" + definition.key
                    + "]. Expected true or false.");
                return false;
            }


            bool meshBlenderCoordinates = false;
            if (meshBlenderCoordinatesIt != definition.values.end()
                && !parseBoolean(meshBlenderCoordinatesIt->second.first, meshBlenderCoordinates))
            {
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    meshBlenderCoordinatesIt->second.second,
                    "Invalid mesh_blender_coordinates in [entity:" + definition.key
                    + "]. Expected true or false.");
                return false;
            }

            if (!registry->setMesh(
                    handle,
                    meshPath,
                    meshColor,
                    meshNormalize,
                    meshDoubleSided,
                    meshBlenderCoordinates))
            {
                rollback(registry, createdEntities);
                errorMessage = "Could not attach mesh to [entity:"
                    + definition.key + "]: " + registry->lastError();
                return false;
            }

            if (meshVisibleIt != definition.values.end())
            {
                bool meshVisible = true;
                if (!parseBoolean(meshVisibleIt->second.first, meshVisible))
                {
                    rollback(registry, createdEntities);
                    errorMessage = lineError(
                        path,
                        meshVisibleIt->second.second,
                        "Invalid mesh_visible in [entity:" + definition.key
                        + "]. Expected true or false.");
                    return false;
                }
                if (!registry->setMeshVisible(handle, meshVisible))
                {
                    rollback(registry, createdEntities);
                    errorMessage = "Could not set mesh visibility for [entity:"
                        + definition.key + "]: " + registry->lastError();
                    return false;
                }
            }
        }

        const auto primitiveIt = definition.values.find("debug_primitive");
        const auto colorIt = definition.values.find("debug_color");
        const auto visibleIt = definition.values.find("debug_visible");
        if (primitiveIt == definition.values.end()
            && (colorIt != definition.values.end() || visibleIt != definition.values.end()))
        {
            rollback(registry, createdEntities);
            const int line = colorIt != definition.values.end()
                ? colorIt->second.second
                : visibleIt->second.second;
            errorMessage = lineError(
                path,
                line,
                "debug_color/debug_visible requires debug_primitive in [entity:"
                    + definition.key + "].");
            return false;
        }

        if (primitiveIt != definition.values.end())
        {
            DebugPrimitiveType primitiveType{};
            if (!parsePrimitiveType(primitiveIt->second.first, primitiveType))
            {
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    primitiveIt->second.second,
                    "Invalid debug_primitive in [entity:" + definition.key
                    + "]. Expected box, cylinder or sphere.");
                return false;
            }

            heritage::math::Vec3 primitiveColor{ 0.65f, 0.72f, 0.82f };
            if (colorIt != definition.values.end()
                && !parseColor(colorIt->second.first, primitiveColor))
            {
                rollback(registry, createdEntities);
                errorMessage = lineError(
                    path,
                    colorIt->second.second,
                    "Invalid debug_color in [entity:" + definition.key
                    + "]. Expected three numbers.");
                return false;
            }

            if (!registry->setDebugPrimitive(handle, primitiveType, primitiveColor))
            {
                rollback(registry, createdEntities);
                errorMessage = "Could not attach debug primitive to [entity:"
                    + definition.key + "]: " + registry->lastError();
                return false;
            }

            if (visibleIt != definition.values.end())
            {
                bool visible = true;
                if (!parseBoolean(visibleIt->second.first, visible))
                {
                    rollback(registry, createdEntities);
                    errorMessage = lineError(
                        path,
                        visibleIt->second.second,
                        "Invalid debug_visible in [entity:" + definition.key
                        + "]. Expected true or false.");
                    return false;
                }
                if (!registry->setDebugPrimitiveVisible(handle, visible))
                {
                    rollback(registry, createdEntities);
                    errorMessage = "Could not set debug visibility for [entity:"
                        + definition.key + "]: " + registry->lastError();
                    return false;
                }
            }
        }
    }

    for (const EntityDefinition& definition : definitions)
    {
        const auto parentIt = definition.values.find("parent");
        if (parentIt == definition.values.end() || trim(parentIt->second.first).empty())
            continue;

        const std::string parentKey = trim(parentIt->second.first);
        const auto foundParent = handleByKey.find(parentKey);
        if (foundParent == handleByKey.end())
        {
            rollback(registry, createdEntities);
            errorMessage = lineError(
                path,
                parentIt->second.second,
                "Unknown parent key '" + parentKey
                    + "' in [entity:" + definition.key + "].");
            return false;
        }

        if (!registry->setParent(
                handleByKey.at(definition.key),
                foundParent->second,
                false))
        {
            rollback(registry, createdEntities);
            errorMessage = "Could not parent [entity:" + definition.key
                + "] to [entity:" + parentKey + "]: "
                + registry->lastError();
            return false;
        }
    }

    info.entityCount = createdEntities.size();
    errorMessage.clear();
    return true;
}

bool EntitySceneDocument::save(
    const std::filesystem::path& path,
    const EntitySceneDocumentInfo& info,
    const EntityRegistry& registry,
    const std::vector<EntityHandle>& requestedEntities,
    std::string& errorMessage)
{
    std::vector<EntityHandle> entities;
    entities.reserve(requestedEntities.size());
    std::unordered_set<EntityHandle> included;
    for (EntityHandle handle : requestedEntities)
    {
        if (registry.exists(handle) && included.insert(handle).second)
            entities.push_back(handle);
    }

    std::unordered_map<EntityHandle, std::string> keyByHandle;
    keyByHandle.reserve(entities.size());
    for (EntityHandle handle : entities)
    {
        const std::uint64_t id = registry.persistentId(handle);
        if (id == 0)
        {
            errorMessage = "Could not serialize an entity with an invalid persistent ID.";
            return false;
        }
        keyByHandle.emplace(handle, "entity_" + std::to_string(id));
    }

    std::error_code directoryError;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError)
    {
        errorMessage = "Could not create scene-document directory: "
            + path.parent_path().string() + " (" + directoryError.message() + ")";
        return false;
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        errorMessage = "Could not open scene document for writing:\n" + path.string();
        return false;
    }

    file << "# Heritage Engine entity scene\n";
    file << "id = " << info.id << "\n";
    file << "type = " << (entities.empty() ? "empty" : "entities") << "\n";
    file << "clear_color = " << vec3Text(info.clearColor) << "\n";
    file << "overlay = " << (info.showOverlay ? "true" : "false") << "\n";
    if (!info.title.empty())
        file << "title = " << info.title << "\n";
    if (!info.subtitle.empty())
        file << "subtitle = " << info.subtitle << "\n";
    if (!info.text.empty())
        file << "text = " << info.text << "\n";

    for (EntityHandle handle : entities)
    {
        file << "\n[entity:" << keyByHandle.at(handle) << "]\n";
        file << "persistent_id = " << registry.persistentId(handle) << "\n";
        file << "name = " << registry.name(handle) << "\n";

        const EntityHandle parent = registry.parent(handle);
        const auto parentKey = keyByHandle.find(parent);
        if (parentKey != keyByHandle.end())
            file << "parent = " << parentKey->second << "\n";

        std::vector<std::string> tags;
        if (!registry.tags(handle, tags))
        {
            errorMessage = "Could not serialize tags: " + registry.lastError();
            return false;
        }
        if (!tags.empty())
        {
            file << "tags = ";
            for (std::size_t index = 0; index < tags.size(); ++index)
            {
                if (index > 0)
                    file << ", ";
                file << tags[index];
            }
            file << "\n";
        }

        heritage::math::Vec3 position{};
        heritage::math::Vec3 rotation{};
        heritage::math::Vec3 scale{};
        if (!registry.position(handle, position)
            || !registry.rotationDegrees(handle, rotation)
            || !registry.scale(handle, scale))
        {
            errorMessage = "Could not serialize transform: " + registry.lastError();
            return false;
        }
        file << "position = " << vec3Text(position) << "\n";
        file << "rotation = " << vec3Text(rotation) << "\n";
        file << "scale = " << vec3Text(scale) << "\n";

        MeshComponent meshComponent{};
        if (registry.mesh(handle, meshComponent))
        {
            file << "mesh = " << meshComponent.assetPath << "\n";
            file << "mesh_color = " << vec3Text(meshComponent.color) << "\n";
            file << "mesh_visible = " << (meshComponent.visible ? "true" : "false") << "\n";
            file << "mesh_normalize = " << (meshComponent.normalize ? "true" : "false") << "\n";
            file << "mesh_double_sided = " << (meshComponent.doubleSided ? "true" : "false") << "\n";
            file << "mesh_blender_coordinates = "
                << (meshComponent.blenderCoordinates ? "true" : "false") << "\n";
        }

        DebugPrimitiveComponent primitive{};
        if (registry.debugPrimitive(handle, primitive))
        {
            file << "debug_primitive = " << primitiveName(primitive.type) << "\n";
            file << "debug_color = " << vec3Text(primitive.color) << "\n";
            file << "debug_visible = " << (primitive.visible ? "true" : "false") << "\n";
        }
    }

    if (!file.good())
    {
        errorMessage = "Writing the scene document failed:\n" + path.string();
        return false;
    }

    errorMessage.clear();
    return true;
}

} // namespace heritage::entities
