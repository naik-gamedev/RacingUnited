#pragma once

#include "../GltfBinary.hpp"
#include "../GltfSceneData.hpp"
#include "../../Core/Paths/Utf8Path.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace heritage::graphics::gltf_internal {

inline constexpr std::uint32_t kGlbMagic = 0x46546C67; // 'glTF'
inline constexpr std::uint32_t kJsonChunkType = 0x4E4F534A; // 'JSON'
inline constexpr std::uint32_t kBinChunkType = 0x004E4942; // 'BIN\0'
inline constexpr std::size_t kGltfVertexStride = 24;

struct JsonValue
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::unordered_map<std::string, JsonValue> objectValue;

    bool isNull() const { return type == Type::Null; }
    bool isBool() const { return type == Type::Bool; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    bool asBool(bool fallback = false) const
    {
        return isBool() ? boolValue : fallback;
    }

    int asInt(int fallback = 0) const
    {
        return isNumber() ? static_cast<int>(numberValue) : fallback;
    }

    std::size_t asSize(std::size_t fallback = 0) const
    {
        return isNumber() && numberValue >= 0.0
            ? static_cast<std::size_t>(numberValue)
            : fallback;
    }

    double asDouble(double fallback = 0.0) const
    {
        return isNumber() ? numberValue : fallback;
    }

    const std::string& asString() const
    {
        static const std::string empty;
        return isString() ? stringValue : empty;
    }

    const JsonValue* find(const std::string& key) const
    {
        if (!isObject())
            return nullptr;
        const auto iterator = objectValue.find(key);
        return iterator != objectValue.end() ? &iterator->second : nullptr;
    }
};

struct Mat4
{
    std::array<float, 16> m{};
};

struct BufferViewInfo
{
    int buffer = -1;
    std::size_t byteOffset = 0;
    std::size_t byteLength = 0;
    std::size_t byteStride = 0;
};


struct AccessorInfo
{
    int bufferView = -1;
    std::size_t byteOffset = 0;
    std::size_t count = 0;
    int componentType = 0;
    std::string type;
    bool normalized = false;
};


struct ImageSource
{
    MaterialTextureReference texture;
};


bool parseJsonDocument(std::string_view text, JsonValue& value, std::string& error);

Mat4 identityMatrix();

Mat4 multiply(const Mat4& left, const Mat4& right);

Mat4 translationMatrix(float x, float y, float z);

Mat4 scaleMatrix(float x, float y, float z);

Mat4 quaternionMatrix(float x, float y, float z, float w);

std::array<float, 3> transformPoint(const Mat4& matrix, const std::array<float, 3>& value);

std::array<float, 3> normalizeVec3(const std::array<float, 3>& value, const std::array<float, 3>& fallback);

std::array<float, 3> transformDirection(const Mat4& matrix, const std::array<float, 3>& value);

bool invertUpper3x3(const Mat4& matrix, std::array<float, 9>& inverse);

std::array<float, 3> transformNormal(const Mat4& matrix, const std::array<float, 3>& value);

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset);

bool parseGlb(
    const std::filesystem::path& path,
    JsonValue& root,
    std::vector<std::uint8_t>& binaryChunk,
    std::string& error);

std::optional<BufferViewInfo> getBufferViewInfo(
    const JsonValue& root,
    int index);

std::optional<AccessorInfo> getAccessorInfo(
    const JsonValue& root,
    int index);

std::size_t componentCount(const std::string& type);

std::size_t componentSize(int componentType);

double readComponentAsDouble(const std::uint8_t* bytes, int componentType, bool normalized);

bool accessorByteLayout(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    int accessorIndex,
    AccessorInfo& accessor,
    BufferViewInfo& bufferView,
    std::size_t& stride,
    const std::uint8_t*& start,
    std::string& error);

bool readFloatAccessor(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    int accessorIndex,
    std::size_t expectedComponents,
    std::vector<float>& output,
    std::string& error);

bool readIndexAccessor(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    int accessorIndex,
    std::vector<unsigned int>& output,
    std::string& error);

std::vector<int> readIntArray(const JsonValue* value);

std::array<float, 3> readVec3(const JsonValue* value, const std::array<float, 3>& fallback);

std::array<float, 4> readVec4(const JsonValue* value, const std::array<float, 4>& fallback);

void appendExtrasMetadata(
    const JsonValue& value,
    const std::string& key,
    AssetMetadataMap& output);

Mat4 nodeLocalTransform(const MeshNode& node);

MeshNode buildNodeRecord(const JsonValue& nodeValue);

bool readMat4Accessor(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    int accessorIndex,
    std::vector<std::array<float, 16>>& output,
    std::string& error);

bool parseEmbeddedImage(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const std::filesystem::path& assetPath,
    int imageIndex,
    ImageSource& image,
    std::string& error);

std::optional<ImageSource> buildTextureSource(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const std::filesystem::path& assetPath,
    int textureIndex,
    std::string& error);

std::string materialKey(const JsonValue& root, int materialIndex);

MaterialDefinition buildMaterialDefinition(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const std::filesystem::path& assetPath,
    int materialIndex,
    std::string& warning);

void collectExternalTextureDependencies(
    const MaterialDefinition& material,
    std::unordered_set<std::string>& deduplicated,
    std::vector<std::filesystem::path>& output);

int attributeAccessorIndex(const JsonValue* attributes, const char* name);

void appendPrimitive(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const std::filesystem::path& assetPath,
    const JsonValue& primitive,
    int nodeIndex,
    int skinIndex,
    Mesh& mesh,
    std::unordered_set<std::string>& dependencySet);

void appendNodeRecursive(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const std::filesystem::path& assetPath,
    int nodeIndex,
    Mesh& mesh,
    std::unordered_set<std::string>& dependencySet);

void buildNodeHierarchy(const JsonValue& root, Mesh& mesh);

void buildSkins(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    Mesh& mesh);

void buildAnimations(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    Mesh& mesh);

std::string lowerAsciiScene(std::string text);

const AssetMetadataValue* metadataValue(
    const AssetMetadataMap& metadata,
    const char* key);

std::string metadataString(
    const AssetMetadataMap& metadata,
    const char* key);

bool metadataBool(
    const AssetMetadataMap& metadata,
    const char* key,
    bool fallback);

bool hasCollisionNameToken(const std::string& name);

bool isCollisionAuthoringNode(const MeshNode& node);

bool isSpawnAuthoringNode(const MeshNode& node);

void appendCollisionPrimitive(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const JsonValue& primitive,
    int nodeIndex,
    const Mat4& globalTransform,
    GlbStaticCollisionScene& scene,
    std::string& error);

void extractCollisionNodeRecursive(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    int nodeIndex,
    const Mat4& parentTransform,
    bool inheritedCollision,
    const std::vector<MeshNode>& nodes,
    GlbStaticCollisionScene& scene,
    std::vector<char>& visited,
    std::string& error);

} // namespace heritage::graphics::gltf_internal
