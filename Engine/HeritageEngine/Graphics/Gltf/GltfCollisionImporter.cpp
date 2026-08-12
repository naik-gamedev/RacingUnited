#include "GltfInternal.hpp"

namespace heritage::graphics::gltf_internal {

std::string lowerAsciiScene(std::string text)
{
    std::transform(
        text.begin(), text.end(), text.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return text;
}

const AssetMetadataValue* metadataValue(
    const AssetMetadataMap& metadata,
    const char* key)
{
    const auto found = metadata.find(key);
    return found != metadata.end() ? &found->second : nullptr;
}

std::string metadataString(
    const AssetMetadataMap& metadata,
    const char* key)
{
    const AssetMetadataValue* value = metadataValue(metadata, key);
    return value && value->type == AssetMetadataValueType::String
        ? value->stringValue
        : std::string{};
}

bool metadataBool(
    const AssetMetadataMap& metadata,
    const char* key,
    bool fallback = false)
{
    const AssetMetadataValue* value = metadataValue(metadata, key);
    return value && value->type == AssetMetadataValueType::Boolean
        ? value->boolValue
        : fallback;
}

bool hasCollisionNameToken(const std::string& name)
{
    const std::string lowered = lowerAsciiScene(name);
    return lowered == "collision"
        || lowered.rfind("collision_", 0) == 0
        || lowered.find("_collision") != std::string::npos;
}

bool isCollisionAuthoringNode(const MeshNode& node)
{
    const std::string role = lowerAsciiScene(metadataString(node.metadata, "heritage.role"));
    const std::string type = lowerAsciiScene(metadataString(node.metadata, "heritage.collision_type"));
    return role == "collision_mesh"
        || role == "collision"
        || type == "static_triangle_mesh"
        || metadataBool(node.metadata, "heritage.collision", false)
        || hasCollisionNameToken(node.name);
}

bool isSpawnAuthoringNode(const MeshNode& node)
{
    const std::string role = lowerAsciiScene(metadataString(node.metadata, "heritage.role"));
    if (role == "spawn_player" || role == "player_spawn")
        return true;

    std::string name = lowerAsciiScene(node.name);
    return name.find("spawn_player") != std::string::npos
        || name.find("player_spawn") != std::string::npos
        || name.find("playerspawn") != std::string::npos;
}

void appendCollisionPrimitive(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const JsonValue& primitive,
    int nodeIndex,
    const Mat4& globalTransform,
    GlbStaticCollisionScene& scene,
    std::string& error)
{
    const int mode = primitive.find("mode") ? primitive.find("mode")->asInt(4) : 4;
    if (mode != 4)
        return;

    const JsonValue* attributes = primitive.find("attributes");
    const int positionAccessorIndex = attributeAccessorIndex(attributes, "POSITION");
    if (positionAccessorIndex < 0)
        return;

    const auto positionAccessor = getAccessorInfo(root, positionAccessorIndex);
    if (!positionAccessor)
        return;

    std::vector<float> positions;
    if (!readFloatAccessor(
            root,
            binaryChunk,
            positionAccessorIndex,
            3,
            positions,
            error))
    {
        return;
    }

    std::vector<unsigned int> indices;
    const int indexAccessorIndex = primitive.find("indices")
        ? primitive.find("indices")->asInt(-1)
        : -1;
    if (indexAccessorIndex >= 0)
    {
        if (!readIndexAccessor(root, binaryChunk, indexAccessorIndex, indices, error))
            return;
    }
    else
    {
        indices.resize(positionAccessor->count);
        for (std::size_t index = 0; index < positionAccessor->count; ++index)
            indices[index] = static_cast<unsigned int>(index);
    }

    for (std::size_t triangleIndex = 0;
        triangleIndex + 2 < indices.size();
        triangleIndex += 3)
    {
        const unsigned int ia = indices[triangleIndex + 0];
        const unsigned int ib = indices[triangleIndex + 1];
        const unsigned int ic = indices[triangleIndex + 2];
        if (ia >= positionAccessor->count
            || ib >= positionAccessor->count
            || ic >= positionAccessor->count)
        {
            continue;
        }

        const auto a = transformPoint(
            globalTransform,
            { positions[static_cast<std::size_t>(ia) * 3 + 0],
              positions[static_cast<std::size_t>(ia) * 3 + 1],
              positions[static_cast<std::size_t>(ia) * 3 + 2] });
        const auto b = transformPoint(
            globalTransform,
            { positions[static_cast<std::size_t>(ib) * 3 + 0],
              positions[static_cast<std::size_t>(ib) * 3 + 1],
              positions[static_cast<std::size_t>(ib) * 3 + 2] });
        const auto c = transformPoint(
            globalTransform,
            { positions[static_cast<std::size_t>(ic) * 3 + 0],
              positions[static_cast<std::size_t>(ic) * 3 + 1],
              positions[static_cast<std::size_t>(ic) * 3 + 2] });

        GlbCollisionTriangle triangle;
        triangle.a = { a[0], a[1], a[2] };
        triangle.b = { b[0], b[1], b[2] };
        triangle.c = { c[0], c[1], c[2] };
        triangle.nodeIndex = nodeIndex;
        scene.triangles.push_back(triangle);
    }
}

void extractCollisionNodeRecursive(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    int nodeIndex,
    const Mat4& parentTransform,
    bool inheritedCollision,
    const std::vector<MeshNode>& nodes,
    GlbStaticCollisionScene& scene,
    std::vector<char>& visited,
    std::string& error)
{
    const JsonValue* jsonNodes = root.find("nodes");
    if (!jsonNodes || !jsonNodes->isArray()
        || nodeIndex < 0
        || static_cast<std::size_t>(nodeIndex) >= nodes.size()
        || static_cast<std::size_t>(nodeIndex) >= jsonNodes->arrayValue.size())
    {
        return;
    }

    if (visited[static_cast<std::size_t>(nodeIndex)])
        return;
    visited[static_cast<std::size_t>(nodeIndex)] = 1;

    const MeshNode& node = nodes[static_cast<std::size_t>(nodeIndex)];
    const JsonValue& jsonNode = jsonNodes->arrayValue[static_cast<std::size_t>(nodeIndex)];
    const Mat4 globalTransform = multiply(parentTransform, nodeLocalTransform(node));
    const bool collisionHere = inheritedCollision || isCollisionAuthoringNode(node);
    const bool spawnHere = isSpawnAuthoringNode(node);

    if (isCollisionAuthoringNode(node))
        ++scene.collisionNodeCount;

    if (spawnHere && !scene.spawnFound)
    {
        const auto position = transformPoint(globalTransform, { 0.0f, 0.0f, 0.0f });
        scene.spawnFound = true;
        scene.spawnPosition = { position[0], position[1], position[2] };
        scene.spawnName = node.name;
    }

    if (collisionHere && !spawnHere)
    {
        const JsonValue* meshIndexValue = jsonNode.find("mesh");
        const JsonValue* meshes = root.find("meshes");
        const int meshIndex = meshIndexValue ? meshIndexValue->asInt(-1) : -1;
        if (meshes && meshes->isArray()
            && meshIndex >= 0
            && static_cast<std::size_t>(meshIndex) < meshes->arrayValue.size())
        {
            const JsonValue& meshValue = meshes->arrayValue[static_cast<std::size_t>(meshIndex)];
            const JsonValue* primitives = meshValue.find("primitives");
            if (primitives && primitives->isArray())
            {
                for (const JsonValue& primitive : primitives->arrayValue)
                {
                    std::string primitiveError;
                    appendCollisionPrimitive(
                        root,
                        binaryChunk,
                        primitive,
                        nodeIndex,
                        globalTransform,
                        scene,
                        primitiveError);
                    if (!primitiveError.empty() && error.empty())
                        error = primitiveError;
                }
            }
        }
    }

    for (int child : node.children)
    {
        extractCollisionNodeRecursive(
            root,
            binaryChunk,
            child,
            globalTransform,
            collisionHere,
            nodes,
            scene,
            visited,
            error);
    }
}


} // namespace heritage::graphics::gltf_internal
