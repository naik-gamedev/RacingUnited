#include "GltfInternal.hpp"

namespace heritage::graphics::gltf_internal {

void appendExtrasMetadata(
    const JsonValue& value,
    const std::string& key,
    AssetMetadataMap& output)
{
    if (value.isObject())
    {
        for (const auto& [childKey, childValue] : value.objectValue)
        {
            const std::string flattenedKey = key.empty()
                ? childKey
                : key + "." + childKey;
            appendExtrasMetadata(childValue, flattenedKey, output);
        }
        return;
    }

    if (key.empty())
        return;

    AssetMetadataValue metadata;
    if (value.isString())
    {
        metadata.type = AssetMetadataValueType::String;
        metadata.stringValue = value.stringValue;
    }
    else if (value.isNumber())
    {
        metadata.type = AssetMetadataValueType::Number;
        metadata.numberValue = value.numberValue;
    }
    else if (value.isBool())
    {
        metadata.type = AssetMetadataValueType::Boolean;
        metadata.boolValue = value.boolValue;
    }
    else
    {
        // Arrays and nulls are intentionally not promoted into the first
        // scalar Heritage metadata contract. Blender vehicle authoring fields
        // are scalar and remain unambiguous through this path.
        return;
    }

    output[key] = std::move(metadata);
}

Mat4 nodeLocalTransform(const MeshNode& node)
{
    if (node.hasMatrix)
    {
        Mat4 result{};
        result.m = node.localMatrix;
        return result;
    }

    return multiply(
        translationMatrix(node.translation[0], node.translation[1], node.translation[2]),
        multiply(
            quaternionMatrix(
                node.rotation[0],
                node.rotation[1],
                node.rotation[2],
                node.rotation[3]),
            scaleMatrix(node.scale[0], node.scale[1], node.scale[2])));
}

MeshNode buildNodeRecord(const JsonValue& nodeValue)
{
    MeshNode node;
    if (const JsonValue* nameValue = nodeValue.find("name"); nameValue && nameValue->isString())
        node.name = nameValue->asString();
    node.children = readIntArray(nodeValue.find("children"));
    if (const JsonValue* extras = nodeValue.find("extras"); extras && extras->isObject())
        appendExtrasMetadata(*extras, {}, node.metadata);

    if (const JsonValue* matrix = nodeValue.find("matrix");
        matrix && matrix->isArray() && matrix->arrayValue.size() >= 16)
    {
        node.hasMatrix = true;
        for (std::size_t i = 0; i < 16; ++i)
            node.localMatrix[i] = static_cast<float>(matrix->arrayValue[i].asDouble(i % 5 == 0 ? 1.0 : 0.0));
    }
    else
    {
        node.translation = readVec3(nodeValue.find("translation"), { 0.0f, 0.0f, 0.0f });
        node.rotation = readVec4(nodeValue.find("rotation"), { 0.0f, 0.0f, 0.0f, 1.0f });
        node.scale = readVec3(nodeValue.find("scale"), { 1.0f, 1.0f, 1.0f });
    }
    return node;
}

void buildNodeHierarchy(const JsonValue& root, Mesh& mesh)
{
    const JsonValue* nodes = root.find("nodes");
    if (!nodes || !nodes->isArray())
        return;

    mesh.nodes.reserve(nodes->arrayValue.size());
    for (const JsonValue& nodeValue : nodes->arrayValue)
        mesh.nodes.push_back(buildNodeRecord(nodeValue));

    for (std::size_t nodeIndex = 0; nodeIndex < mesh.nodes.size(); ++nodeIndex)
    {
        for (int child : mesh.nodes[nodeIndex].children)
        {
            if (child >= 0 && static_cast<std::size_t>(child) < mesh.nodes.size())
                mesh.nodes[static_cast<std::size_t>(child)].parentIndex = static_cast<int>(nodeIndex);
        }
    }
}


} // namespace heritage::graphics::gltf_internal
