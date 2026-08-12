#include "GltfInternal.hpp"

namespace heritage::graphics::gltf_internal {

Mat4 identityMatrix()
{
    Mat4 result{};
    result.m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f };
    return result;
}

Mat4 multiply(const Mat4& left, const Mat4& right)
{
    Mat4 result{};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            result.m[static_cast<std::size_t>(column * 4 + row)] =
                left.m[static_cast<std::size_t>(0 * 4 + row)] * right.m[static_cast<std::size_t>(column * 4 + 0)]
                + left.m[static_cast<std::size_t>(1 * 4 + row)] * right.m[static_cast<std::size_t>(column * 4 + 1)]
                + left.m[static_cast<std::size_t>(2 * 4 + row)] * right.m[static_cast<std::size_t>(column * 4 + 2)]
                + left.m[static_cast<std::size_t>(3 * 4 + row)] * right.m[static_cast<std::size_t>(column * 4 + 3)];
        }
    }
    return result;
}

Mat4 translationMatrix(float x, float y, float z)
{
    Mat4 result = identityMatrix();
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

Mat4 scaleMatrix(float x, float y, float z)
{
    Mat4 result = identityMatrix();
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    return result;
}

Mat4 quaternionMatrix(float x, float y, float z, float w)
{
    Mat4 result = identityMatrix();
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    result.m[0] = 1.0f - 2.0f * (yy + zz);
    result.m[1] = 2.0f * (xy + wz);
    result.m[2] = 2.0f * (xz - wy);

    result.m[4] = 2.0f * (xy - wz);
    result.m[5] = 1.0f - 2.0f * (xx + zz);
    result.m[6] = 2.0f * (yz + wx);

    result.m[8] = 2.0f * (xz + wy);
    result.m[9] = 2.0f * (yz - wx);
    result.m[10] = 1.0f - 2.0f * (xx + yy);
    return result;
}

std::array<float, 3> transformPoint(const Mat4& matrix, const std::array<float, 3>& value)
{
    return {
        matrix.m[0] * value[0] + matrix.m[4] * value[1] + matrix.m[8] * value[2] + matrix.m[12],
        matrix.m[1] * value[0] + matrix.m[5] * value[1] + matrix.m[9] * value[2] + matrix.m[13],
        matrix.m[2] * value[0] + matrix.m[6] * value[1] + matrix.m[10] * value[2] + matrix.m[14] };
}

std::array<float, 3> normalizeVec3(const std::array<float, 3>& value, const std::array<float, 3>& fallback)
{
    const float lengthSquared =
        value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
    if (lengthSquared <= 1.0e-12f)
        return fallback;
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    return { value[0] * inverse, value[1] * inverse, value[2] * inverse };
}

std::array<float, 3> transformDirection(const Mat4& matrix, const std::array<float, 3>& value)
{
    return normalizeVec3(
        {
            matrix.m[0] * value[0] + matrix.m[4] * value[1] + matrix.m[8] * value[2],
            matrix.m[1] * value[0] + matrix.m[5] * value[1] + matrix.m[9] * value[2],
            matrix.m[2] * value[0] + matrix.m[6] * value[1] + matrix.m[10] * value[2] },
        { 0.0f, 1.0f, 0.0f });
}

bool invertUpper3x3(const Mat4& matrix, std::array<float, 9>& inverse)
{
    const float a00 = matrix.m[0], a01 = matrix.m[4], a02 = matrix.m[8];
    const float a10 = matrix.m[1], a11 = matrix.m[5], a12 = matrix.m[9];
    const float a20 = matrix.m[2], a21 = matrix.m[6], a22 = matrix.m[10];

    const float c00 = a11 * a22 - a12 * a21;
    const float c01 = -(a10 * a22 - a12 * a20);
    const float c02 = a10 * a21 - a11 * a20;
    const float c10 = -(a01 * a22 - a02 * a21);
    const float c11 = a00 * a22 - a02 * a20;
    const float c12 = -(a00 * a21 - a01 * a20);
    const float c20 = a01 * a12 - a02 * a11;
    const float c21 = -(a00 * a12 - a02 * a10);
    const float c22 = a00 * a11 - a01 * a10;

    const float determinant = a00 * c00 + a01 * c01 + a02 * c02;
    if (std::abs(determinant) <= 1.0e-12f)
        return false;

    const float inverseDet = 1.0f / determinant;
    inverse = {
        c00 * inverseDet, c10 * inverseDet, c20 * inverseDet,
        c01 * inverseDet, c11 * inverseDet, c21 * inverseDet,
        c02 * inverseDet, c12 * inverseDet, c22 * inverseDet };
    return true;
}

std::array<float, 3> transformNormal(const Mat4& matrix, const std::array<float, 3>& value)
{
    std::array<float, 9> inverse{};
    if (!invertUpper3x3(matrix, inverse))
        return transformDirection(matrix, value);

    // transpose(inverse(M)) * n
    return normalizeVec3(
        {
            inverse[0] * value[0] + inverse[1] * value[1] + inverse[2] * value[2],
            inverse[3] * value[0] + inverse[4] * value[1] + inverse[5] * value[2],
            inverse[6] * value[0] + inverse[7] * value[1] + inverse[8] * value[2] },
        { 0.0f, 1.0f, 0.0f });
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}

bool parseGlb(
    const std::filesystem::path& path,
    JsonValue& root,
    std::vector<std::uint8_t>& binaryChunk,
    std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        error = "Could not open GLB: " + heritage::paths::toUtf8(path);
        return false;
    }

    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    if (bytes.size() < 20)
    {
        error = "GLB file is too small: " + heritage::paths::toUtf8(path);
        return false;
    }

    const std::uint32_t magic = readU32(bytes, 0);
    const std::uint32_t version = readU32(bytes, 4);
    const std::uint32_t totalLength = readU32(bytes, 8);
    if (magic != kGlbMagic)
    {
        error = "Asset is not a valid GLB: " + heritage::paths::toUtf8(path);
        return false;
    }
    if (version != 2)
    {
        error = "Only glTF 2.0 binary assets are supported: " + heritage::paths::toUtf8(path);
        return false;
    }
    if (totalLength > bytes.size())
    {
        error = "GLB length header exceeds file size: " + heritage::paths::toUtf8(path);
        return false;
    }

    std::string jsonText;
    binaryChunk.clear();

    std::size_t offset = 12;
    while (offset + 8 <= totalLength)
    {
        const std::uint32_t chunkLength = readU32(bytes, offset);
        const std::uint32_t chunkType = readU32(bytes, offset + 4);
        offset += 8;
        if (offset + chunkLength > totalLength)
        {
            error = "GLB chunk exceeds file size: " + heritage::paths::toUtf8(path);
            return false;
        }

        if (chunkType == kJsonChunkType)
        {
            jsonText.assign(
                reinterpret_cast<const char*>(bytes.data() + offset),
                reinterpret_cast<const char*>(bytes.data() + offset + chunkLength));
        }
        else if (chunkType == kBinChunkType)
        {
            binaryChunk.assign(
                bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                bytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
        }
        offset += chunkLength;
    }

    if (jsonText.empty())
    {
        error = "GLB JSON chunk is missing: " + heritage::paths::toUtf8(path);
        return false;
    }

    if (!parseJsonDocument(jsonText, root, error))
    {
        error = "Could not parse GLB JSON: " + error;
        return false;
    }
    return true;
}

std::optional<BufferViewInfo> getBufferViewInfo(
    const JsonValue& root,
    int index)
{
    const JsonValue* bufferViews = root.find("bufferViews");
    if (!bufferViews || !bufferViews->isArray()
        || index < 0
        || static_cast<std::size_t>(index) >= bufferViews->arrayValue.size())
    {
        return std::nullopt;
    }

    const JsonValue& value = bufferViews->arrayValue[static_cast<std::size_t>(index)];
    BufferViewInfo info;
    info.buffer = value.find("buffer") ? value.find("buffer")->asInt(-1) : 0;
    info.byteOffset = value.find("byteOffset") ? value.find("byteOffset")->asSize(0) : 0;
    info.byteLength = value.find("byteLength") ? value.find("byteLength")->asSize(0) : 0;
    info.byteStride = value.find("byteStride") ? value.find("byteStride")->asSize(0) : 0;
    return info;
}

std::optional<AccessorInfo> getAccessorInfo(
    const JsonValue& root,
    int index)
{
    const JsonValue* accessors = root.find("accessors");
    if (!accessors || !accessors->isArray()
        || index < 0
        || static_cast<std::size_t>(index) >= accessors->arrayValue.size())
    {
        return std::nullopt;
    }

    const JsonValue& value = accessors->arrayValue[static_cast<std::size_t>(index)];
    AccessorInfo info;
    info.bufferView = value.find("bufferView") ? value.find("bufferView")->asInt(-1) : -1;
    info.byteOffset = value.find("byteOffset") ? value.find("byteOffset")->asSize(0) : 0;
    info.count = value.find("count") ? value.find("count")->asSize(0) : 0;
    info.componentType = value.find("componentType") ? value.find("componentType")->asInt(0) : 0;
    info.type = value.find("type") ? value.find("type")->asString() : std::string();
    info.normalized = value.find("normalized") ? value.find("normalized")->asBool(false) : false;
    return info;
}

std::size_t componentCount(const std::string& type)
{
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT2") return 4;
    if (type == "MAT3") return 9;
    if (type == "MAT4") return 16;
    return 0;
}

std::size_t componentSize(int componentType)
{
    switch (componentType)
    {
    case 5120: // BYTE
    case 5121: // UNSIGNED_BYTE
        return 1;
    case 5122: // SHORT
    case 5123: // UNSIGNED_SHORT
        return 2;
    case 5125: // UNSIGNED_INT
    case 5126: // FLOAT
        return 4;
    default:
        return 0;
    }
}

double readComponentAsDouble(const std::uint8_t* bytes, int componentType, bool normalized)
{
    switch (componentType)
    {
    case 5120:
    {
        const auto value = *reinterpret_cast<const std::int8_t*>(bytes);
        return normalized
            ? std::max(-1.0, static_cast<double>(value) / 127.0)
            : static_cast<double>(value);
    }
    case 5121:
    {
        const auto value = *reinterpret_cast<const std::uint8_t*>(bytes);
        return normalized
            ? static_cast<double>(value) / 255.0
            : static_cast<double>(value);
    }
    case 5122:
    {
        std::int16_t value = 0;
        std::memcpy(&value, bytes, sizeof(value));
        return normalized
            ? std::max(-1.0, static_cast<double>(value) / 32767.0)
            : static_cast<double>(value);
    }
    case 5123:
    {
        std::uint16_t value = 0;
        std::memcpy(&value, bytes, sizeof(value));
        return normalized
            ? static_cast<double>(value) / 65535.0
            : static_cast<double>(value);
    }
    case 5125:
    {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes, sizeof(value));
        return static_cast<double>(value);
    }
    case 5126:
    {
        float value = 0.0f;
        std::memcpy(&value, bytes, sizeof(value));
        return static_cast<double>(value);
    }
    default:
        return 0.0;
    }
}

bool accessorByteLayout(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    int accessorIndex,
    AccessorInfo& accessor,
    BufferViewInfo& bufferView,
    std::size_t& stride,
    const std::uint8_t*& start,
    std::string& error)
{
    const auto accessorInfo = getAccessorInfo(root, accessorIndex);
    if (!accessorInfo)
    {
        error = "Invalid accessor index in GLB.";
        return false;
    }
    accessor = *accessorInfo;
    if (accessor.bufferView < 0)
    {
        error = "Sparse/accessor-without-bufferView is not supported yet.";
        return false;
    }

    const auto bufferViewInfo = getBufferViewInfo(root, accessor.bufferView);
    if (!bufferViewInfo)
    {
        error = "Invalid bufferView index in GLB.";
        return false;
    }
    bufferView = *bufferViewInfo;
    if (bufferView.buffer != 0)
    {
        error = "GLB uses an unsupported buffer index.";
        return false;
    }

    const std::size_t components = componentCount(accessor.type);
    const std::size_t bytesPerComponent = componentSize(accessor.componentType);
    if (components == 0 || bytesPerComponent == 0)
    {
        error = "GLB accessor uses an unsupported component type.";
        return false;
    }

    stride = bufferView.byteStride != 0
        ? bufferView.byteStride
        : components * bytesPerComponent;

    const std::size_t baseOffset = bufferView.byteOffset + accessor.byteOffset;
    const std::size_t required =
        accessor.count == 0 ? 0 : baseOffset + (accessor.count - 1) * stride + components * bytesPerComponent;
    if (required > binaryChunk.size())
    {
        error = "GLB accessor exceeds binary buffer bounds.";
        return false;
    }

    start = binaryChunk.data() + baseOffset;
    return true;
}

bool readFloatAccessor(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    int accessorIndex,
    std::size_t expectedComponents,
    std::vector<float>& output,
    std::string& error)
{
    AccessorInfo accessor;
    BufferViewInfo bufferView;
    std::size_t stride = 0;
    const std::uint8_t* start = nullptr;
    if (!accessorByteLayout(root, binaryChunk, accessorIndex, accessor, bufferView, stride, start, error))
        return false;

    const std::size_t actualComponents = componentCount(accessor.type);
    if (actualComponents < expectedComponents)
    {
        error = "GLB accessor has fewer components than expected.";
        return false;
    }

    const std::size_t bytesPerComponent = componentSize(accessor.componentType);
    output.assign(accessor.count * expectedComponents, 0.0f);

    for (std::size_t element = 0; element < accessor.count; ++element)
    {
        const std::uint8_t* elementBytes = start + element * stride;
        for (std::size_t component = 0; component < expectedComponents; ++component)
        {
            output[element * expectedComponents + component] = static_cast<float>(
                readComponentAsDouble(
                    elementBytes + component * bytesPerComponent,
                    accessor.componentType,
                    accessor.normalized));
        }
    }
    return true;
}

bool readIndexAccessor(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    int accessorIndex,
    std::vector<unsigned int>& output,
    std::string& error)
{
    AccessorInfo accessor;
    BufferViewInfo bufferView;
    std::size_t stride = 0;
    const std::uint8_t* start = nullptr;
    if (!accessorByteLayout(root, binaryChunk, accessorIndex, accessor, bufferView, stride, start, error))
        return false;

    if (componentCount(accessor.type) != 1)
    {
        error = "GLB index accessor must be SCALAR.";
        return false;
    }

    if (accessor.componentType != 5121
        && accessor.componentType != 5123
        && accessor.componentType != 5125)
    {
        error = "GLB index accessor uses an unsupported component type.";
        return false;
    }

    const std::size_t bytesPerComponent = componentSize(accessor.componentType);
    output.assign(accessor.count, 0u);
    for (std::size_t i = 0; i < accessor.count; ++i)
    {
        const std::uint8_t* valueBytes = start + i * stride;
        output[i] = static_cast<unsigned int>(
            readComponentAsDouble(valueBytes, accessor.componentType, false));
        (void)bytesPerComponent;
    }
    return true;
}

std::vector<int> readIntArray(const JsonValue* value)
{
    std::vector<int> result;
    if (!value || !value->isArray())
        return result;
    result.reserve(value->arrayValue.size());
    for (const JsonValue& entry : value->arrayValue)
        result.push_back(entry.asInt(-1));
    return result;
}

std::array<float, 3> readVec3(const JsonValue* value, const std::array<float, 3>& fallback)
{
    if (!value || !value->isArray() || value->arrayValue.size() < 3)
        return fallback;
    return {
        static_cast<float>(value->arrayValue[0].asDouble(fallback[0])),
        static_cast<float>(value->arrayValue[1].asDouble(fallback[1])),
        static_cast<float>(value->arrayValue[2].asDouble(fallback[2])) };
}

std::array<float, 4> readVec4(const JsonValue* value, const std::array<float, 4>& fallback)
{
    if (!value || !value->isArray() || value->arrayValue.size() < 4)
        return fallback;
    return {
        static_cast<float>(value->arrayValue[0].asDouble(fallback[0])),
        static_cast<float>(value->arrayValue[1].asDouble(fallback[1])),
        static_cast<float>(value->arrayValue[2].asDouble(fallback[2])),
        static_cast<float>(value->arrayValue[3].asDouble(fallback[3])) };
}

bool readMat4Accessor(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    int accessorIndex,
    std::vector<std::array<float, 16>>& output,
    std::string& error)
{
    AccessorInfo accessor;
    BufferViewInfo bufferView;
    std::size_t stride = 0;
    const std::uint8_t* start = nullptr;
    if (!accessorByteLayout(root, binaryChunk, accessorIndex, accessor, bufferView, stride, start, error))
        return false;

    if (accessor.type != "MAT4")
    {
        error = "Expected a MAT4 accessor.";
        return false;
    }

    const std::size_t bytesPerComponent = componentSize(accessor.componentType);
    if (bytesPerComponent == 0)
    {
        error = "Unsupported MAT4 accessor component type.";
        return false;
    }

    output.assign(accessor.count, {});
    for (std::size_t element = 0; element < accessor.count; ++element)
    {
        const std::uint8_t* elementBytes = start + element * stride;
        for (std::size_t component = 0; component < 16; ++component)
        {
            output[element][component] = static_cast<float>(
                readComponentAsDouble(
                    elementBytes + component * bytesPerComponent,
                    accessor.componentType,
                    accessor.normalized));
        }
    }
    return true;
}


} // namespace heritage::graphics::gltf_internal
