#include "Mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

namespace heritage::graphics {
namespace {

constexpr std::size_t kObjVertexStride = 12;
constexpr std::size_t kPositionOffset = 0;
constexpr std::size_t kNormalOffset = 3;
constexpr std::size_t kUvOffset = 6;
constexpr std::size_t kTangentOffset = 8;

std::array<float, 3> convertAuthoringVector(
    const std::array<float, 3>& value,
    bool blenderCoordinates)
{
    if (!blenderCoordinates)
        return value;

    // Racing United authoring convention follows Blender: X right, Y forward,
    // Z up. Blender's default OBJ export writes X right, Y up and -Z forward.
    // Heritage native simulation is X right, Y up, Z forward, so only the OBJ
    // forward axis must be reflected here. This reflection reverses handedness,
    // therefore face winding is reversed below when this mode is active.
    return { value[0], value[1], -value[2] };
}

bool isAuthoringMetadataObject(const std::string& name)
{
    std::string lowered = name;
    std::transform(
        lowered.begin(), lowered.end(), lowered.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered.find("spawn_player") != std::string::npos
        || lowered.find("player_spawn") != std::string::npos
        || lowered.find("playerspawn") != std::string::npos;
}

std::string trim(std::string value)
{
    const auto first = std::find_if_not(
        value.begin(), value.end(),
        [](unsigned char c) { return std::isspace(c) != 0; });
    if (first == value.end())
        return {};

    const auto last = std::find_if_not(
        value.rbegin(), value.rend(),
        [](unsigned char c) { return std::isspace(c) != 0; }).base();
    return std::string(first, last);
}

int resolveObjIndex(int rawIndex, std::size_t count)
{
    if (rawIndex > 0)
    {
        const int resolved = rawIndex - 1;
        return resolved >= 0 && resolved < static_cast<int>(count)
            ? resolved
            : -1;
    }
    if (rawIndex < 0)
    {
        const int resolved = static_cast<int>(count) + rawIndex;
        return resolved >= 0 && resolved < static_cast<int>(count)
            ? resolved
            : -1;
    }
    return -1;
}

int parseInteger(const std::string& value)
{
    if (value.empty())
        return 0;
    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        return 0;
    }
}

struct FacePoint
{
    int position = -1;
    int texcoord = -1;
    int normal = -1;
};

FacePoint parseFacePoint(
    const std::string& chunk,
    std::size_t positionCount,
    std::size_t texcoordCount,
    std::size_t normalCount)
{
    FacePoint result;

    const std::size_t firstSlash = chunk.find('/');
    if (firstSlash == std::string::npos)
    {
        result.position = resolveObjIndex(
            parseInteger(chunk), positionCount);
        return result;
    }

    result.position = resolveObjIndex(
        parseInteger(chunk.substr(0, firstSlash)),
        positionCount);

    const std::size_t secondSlash = chunk.find('/', firstSlash + 1);
    if (secondSlash == std::string::npos)
    {
        result.texcoord = resolveObjIndex(
            parseInteger(chunk.substr(firstSlash + 1)),
            texcoordCount);
        return result;
    }

    result.texcoord = resolveObjIndex(
        parseInteger(chunk.substr(
            firstSlash + 1,
            secondSlash - firstSlash - 1)),
        texcoordCount);
    result.normal = resolveObjIndex(
        parseInteger(chunk.substr(secondSlash + 1)),
        normalCount);
    return result;
}

std::array<float, 3> vectorSubtract(
    const std::array<float, 3>& a,
    const std::array<float, 3>& b)
{
    return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}

std::array<float, 3> vectorAdd(
    const std::array<float, 3>& a,
    const std::array<float, 3>& b)
{
    return { a[0] + b[0], a[1] + b[1], a[2] + b[2] };
}

std::array<float, 3> vectorScale(
    const std::array<float, 3>& value,
    float scale)
{
    return {
        value[0] * scale,
        value[1] * scale,
        value[2] * scale
    };
}

float vectorDot(
    const std::array<float, 3>& a,
    const std::array<float, 3>& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

std::array<float, 3> vectorCross(
    const std::array<float, 3>& a,
    const std::array<float, 3>& b)
{
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

std::array<float, 3> vectorNormalize(
    const std::array<float, 3>& value,
    const std::array<float, 3>& fallback)
{
    const float lengthSquared = vectorDot(value, value);
    if (lengthSquared <= 1.0e-12f)
        return fallback;

    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return vectorScale(value, inverseLength);
}

std::array<float, 3> vertexVec3(
    const Mesh& mesh,
    std::size_t vertexIndex,
    std::size_t offset)
{
    const std::size_t base =
        vertexIndex * mesh.vertexStrideFloats + offset;
    return {
        mesh.vertices[base],
        mesh.vertices[base + 1],
        mesh.vertices[base + 2]
    };
}

std::array<float, 2> vertexUv(
    const Mesh& mesh,
    std::size_t vertexIndex)
{
    const std::size_t base =
        vertexIndex * mesh.vertexStrideFloats + kUvOffset;
    return {
        mesh.vertices[base],
        mesh.vertices[base + 1]
    };
}

bool computeTangentsInternal(Mesh& mesh)
{
    if (mesh.vertexStrideFloats < kObjVertexStride
        || mesh.indices.size() < 3)
    {
        return false;
    }

    const std::size_t vertexCount =
        mesh.vertices.size() / mesh.vertexStrideFloats;
    std::vector<std::array<float, 3>> tangents(
        vertexCount, { 0.0f, 0.0f, 0.0f });
    std::vector<std::array<float, 3>> bitangents(
        vertexCount, { 0.0f, 0.0f, 0.0f });
    bool hasUsableTexcoords = false;

    for (std::size_t index = 0; index + 2 < mesh.indices.size(); index += 3)
    {
        const std::size_t i0 = mesh.indices[index];
        const std::size_t i1 = mesh.indices[index + 1];
        const std::size_t i2 = mesh.indices[index + 2];
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
            continue;

        const auto p0 = vertexVec3(mesh, i0, kPositionOffset);
        const auto p1 = vertexVec3(mesh, i1, kPositionOffset);
        const auto p2 = vertexVec3(mesh, i2, kPositionOffset);
        const auto uv0 = vertexUv(mesh, i0);
        const auto uv1 = vertexUv(mesh, i1);
        const auto uv2 = vertexUv(mesh, i2);

        const auto edge1 = vectorSubtract(p1, p0);
        const auto edge2 = vectorSubtract(p2, p0);
        const float du1 = uv1[0] - uv0[0];
        const float dv1 = uv1[1] - uv0[1];
        const float du2 = uv2[0] - uv0[0];
        const float dv2 = uv2[1] - uv0[1];
        const float denominator = du1 * dv2 - du2 * dv1;
        if (std::abs(denominator) <= 1.0e-12f)
            continue;

        hasUsableTexcoords = true;
        const float reciprocal = 1.0f / denominator;
        const std::array<float, 3> tangent = {
            reciprocal * (dv2 * edge1[0] - dv1 * edge2[0]),
            reciprocal * (dv2 * edge1[1] - dv1 * edge2[1]),
            reciprocal * (dv2 * edge1[2] - dv1 * edge2[2])
        };
        const std::array<float, 3> bitangent = {
            reciprocal * (-du2 * edge1[0] + du1 * edge2[0]),
            reciprocal * (-du2 * edge1[1] + du1 * edge2[1]),
            reciprocal * (-du2 * edge1[2] + du1 * edge2[2])
        };

        tangents[i0] = vectorAdd(tangents[i0], tangent);
        tangents[i1] = vectorAdd(tangents[i1], tangent);
        tangents[i2] = vectorAdd(tangents[i2], tangent);
        bitangents[i0] = vectorAdd(bitangents[i0], bitangent);
        bitangents[i1] = vectorAdd(bitangents[i1], bitangent);
        bitangents[i2] = vectorAdd(bitangents[i2], bitangent);
    }

    for (std::size_t vertex = 0; vertex < vertexCount; ++vertex)
    {
        const auto normal = vectorNormalize(
            vertexVec3(mesh, vertex, kNormalOffset),
            { 0.0f, 1.0f, 0.0f });

        auto tangent = tangents[vertex];
        tangent = vectorSubtract(
            tangent,
            vectorScale(normal, vectorDot(normal, tangent)));
        tangent = vectorNormalize(
            tangent,
            std::abs(normal[1]) < 0.95f
                ? vectorNormalize(
                    vectorCross({ 0.0f, 1.0f, 0.0f }, normal),
                    { 1.0f, 0.0f, 0.0f })
                : vectorNormalize(
                    vectorCross({ 1.0f, 0.0f, 0.0f }, normal),
                    { 0.0f, 0.0f, 1.0f }));

        const auto computedBitangent = vectorCross(normal, tangent);
        const float handedness =
            vectorDot(computedBitangent, bitangents[vertex]) < 0.0f
            ? -1.0f
            : 1.0f;

        const std::size_t base =
            vertex * mesh.vertexStrideFloats + kTangentOffset;
        mesh.vertices[base] = tangent[0];
        mesh.vertices[base + 1] = tangent[1];
        mesh.vertices[base + 2] = tangent[2];
        mesh.vertices[base + 3] = handedness;
    }

    return hasUsableTexcoords;
}

void appendMaterialLibrary(
    Mesh& mesh,
    const std::filesystem::path& materialPath)
{
    mesh.sourceDependencies.push_back(materialPath);
    MaterialLibraryLoadResult library = loadMaterialLibrary(materialPath);
    for (auto& [name, material] : library.materials)
        mesh.materials.insert_or_assign(name, std::move(material));

    for (const std::string& warning : library.warnings)
        std::cerr << "OBJ material warning: " << warning << '\n';
}


bool computeIndexedBounds(
    const Mesh& mesh,
    std::size_t firstIndex,
    std::size_t indexCount,
    std::array<float, 3>& center,
    float& radius)
{
    if (mesh.vertexStrideFloats < 3
        || mesh.vertices.empty()
        || mesh.indices.empty()
        || firstIndex >= mesh.indices.size())
    {
        return false;
    }

    const std::size_t end = std::min(
        mesh.indices.size(),
        firstIndex + indexCount);
    if (end <= firstIndex)
        return false;

    const std::size_t vertexCount =
        mesh.vertices.size() / mesh.vertexStrideFloats;
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
    bool havePoint = false;

    for (std::size_t i = firstIndex; i < end; ++i)
    {
        const std::size_t vertexIndex = mesh.indices[i];
        if (vertexIndex >= vertexCount)
            continue;
        const std::size_t base = vertexIndex * mesh.vertexStrideFloats;
        const std::array<float, 3> point{
            mesh.vertices[base],
            mesh.vertices[base + 1],
            mesh.vertices[base + 2] };
        if (!havePoint)
        {
            minimum = maximum = point;
            havePoint = true;
        }
        else
        {
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                minimum[axis] = std::min(minimum[axis], point[axis]);
                maximum[axis] = std::max(maximum[axis], point[axis]);
            }
        }
    }

    if (!havePoint)
        return false;

    center = {
        (minimum[0] + maximum[0]) * 0.5f,
        (minimum[1] + maximum[1]) * 0.5f,
        (minimum[2] + maximum[2]) * 0.5f };
    float radiusSquared = 0.0f;
    for (std::size_t i = firstIndex; i < end; ++i)
    {
        const std::size_t vertexIndex = mesh.indices[i];
        if (vertexIndex >= vertexCount)
            continue;
        const std::size_t base = vertexIndex * mesh.vertexStrideFloats;
        const float dx = mesh.vertices[base] - center[0];
        const float dy = mesh.vertices[base + 1] - center[1];
        const float dz = mesh.vertices[base + 2] - center[2];
        radiusSquared = std::max(
            radiusSquared,
            dx * dx + dy * dy + dz * dz);
    }
    radius = std::sqrt(radiusSquared);
    return std::isfinite(radius);
}

void computeDrawRangeBounds(Mesh& mesh)
{
    for (MeshDrawRange& range : mesh.drawRanges)
    {
        range.hasBounds = computeIndexedBounds(
            mesh,
            range.firstIndex,
            range.indexCount,
            range.boundsCenter,
            range.boundsRadius);
    }
}

bool isTireVisualNodeName(const std::string& name)
{
    std::string lowered = name;
    std::transform(
        lowered.begin(), lowered.end(), lowered.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered.find("tire") != std::string::npos
        || lowered.find("tyre") != std::string::npos;
}

void computeTireVisualGeometry(Mesh& mesh)
{
    if (mesh.vertexStrideFloats < 3 || mesh.nodes.empty())
        return;

    const std::size_t vertexCount =
        mesh.vertices.size() / mesh.vertexStrideFloats;

    for (std::size_t nodeIndex = 0; nodeIndex < mesh.nodes.size(); ++nodeIndex)
    {
        MeshNode& node = mesh.nodes[nodeIndex];
        node.hasTireVisualGeometry = false;
        if (!isTireVisualNodeName(node.name))
            continue;

        std::array<float, 3> minimum{};
        std::array<float, 3> maximum{};
        bool havePoint = false;
        const auto visitNodeVertices = [&](const auto& visitor)
        {
            for (const MeshDrawRange& range : mesh.drawRanges)
            {
                if (range.nodeIndex != static_cast<int>(nodeIndex))
                    continue;
                const std::size_t end = std::min(
                    range.firstIndex + range.indexCount, mesh.indices.size());
                for (std::size_t i = range.firstIndex; i < end; ++i)
                {
                    const std::size_t vertexIndex = mesh.indices[i];
                    if (vertexIndex >= vertexCount)
                        continue;
                    const std::size_t base =
                        vertexIndex * mesh.vertexStrideFloats;
                    visitor(std::array<float, 3>{
                        mesh.vertices[base + 0],
                        mesh.vertices[base + 1],
                        mesh.vertices[base + 2] });
                }
            }
        };

        visitNodeVertices([&](const std::array<float, 3>& p)
        {
            if (!havePoint)
            {
                minimum = maximum = p;
                havePoint = true;
                return;
            }
            for (int axis = 0; axis < 3; ++axis)
            {
                minimum[axis] = std::min(minimum[axis], p[axis]);
                maximum[axis] = std::max(maximum[axis], p[axis]);
            }
        });
        if (!havePoint)
            continue;

        std::array<float, 3> extent{
            maximum[0] - minimum[0],
            maximum[1] - minimum[1],
            maximum[2] - minimum[2] };
        int axleAxis = 0;
        if (extent[1] < extent[axleAxis]) axleAxis = 1;
        if (extent[2] < extent[axleAxis]) axleAxis = 2;
        const int radialA = (axleAxis + 1) % 3;
        const int radialB = (axleAxis + 2) % 3;
        const float radialMinimumExtent =
            std::min(extent[radialA], extent[radialB]);
        const float radialMaximumExtent =
            std::max(extent[radialA], extent[radialB]);
        if (radialMinimumExtent <= 1.0e-5f
            || radialMaximumExtent / radialMinimumExtent > 1.35f
            || extent[axleAxis] >= radialMinimumExtent * 0.80f)
        {
            continue;
        }

        const std::array<float, 3> center{
            (minimum[0] + maximum[0]) * 0.5f,
            (minimum[1] + maximum[1]) * 0.5f,
            (minimum[2] + maximum[2]) * 0.5f };
        float innerRadius = std::numeric_limits<float>::max();
        float outerRadius = 0.0f;
        visitNodeVertices([&](const std::array<float, 3>& p)
        {
            const float da = p[radialA] - center[radialA];
            const float db = p[radialB] - center[radialB];
            const float radius = std::sqrt(da * da + db * db);
            innerRadius = std::min(innerRadius, radius);
            outerRadius = std::max(outerRadius, radius);
        });
        if (!std::isfinite(innerRadius) || !std::isfinite(outerRadius)
            || outerRadius <= 1.0e-4f
            || innerRadius < 0.0f
            || innerRadius >= outerRadius * 0.95f)
        {
            continue;
        }

        node.hasTireVisualGeometry = true;
        node.tireVisualCenter = center;
        node.tireVisualAxleAxis = axleAxis;
        node.tireVisualHalfWidth = extent[axleAxis] * 0.5f;
        node.tireVisualInnerRadius = innerRadius;
        node.tireVisualOuterRadius = outerRadius;

        std::cout
            << "Heritage tire visual geometry: " << node.name
            << " axis=" << axleAxis
            << " width_mm=" << node.tireVisualHalfWidth * 2000.0f
            << " diameter_mm=" << node.tireVisualOuterRadius * 2000.0f
            << " bead_diameter_mm=" << node.tireVisualInnerRadius * 2000.0f
            << '\n';
    }
}

} // namespace

bool computeTangents(Mesh& mesh)
{
    return computeTangentsInternal(mesh);
}

Mesh loadObjMesh(
    const std::string& path,
    bool normalizeToUnit,
    bool blenderCoordinates)
{
    Mesh mesh;
    mesh.vertexStrideFloats = kObjVertexStride;
    mesh.sourceDependencies.push_back(
        std::filesystem::path(path).lexically_normal());

    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 3>> normals;
    std::vector<std::array<float, 2>> texcoords;
    std::map<std::tuple<int, int, int>, unsigned int> cache;
    bool skipCurrentObject = false;
    std::string currentMaterial;

    const std::filesystem::path objPath(path);
    std::ifstream file(objPath);
    if (!file.is_open())
    {
        std::cerr << "Could not open OBJ: " << path << '\n';
        return mesh;
    }

    auto acquireVertex = [&](const FacePoint& facePoint) -> unsigned int {
        const auto key = std::make_tuple(
            facePoint.position,
            facePoint.texcoord,
            facePoint.normal);
        const auto cached = cache.find(key);
        if (cached != cache.end())
            return cached->second;

        const unsigned int vertexIndex =
            static_cast<unsigned int>(
                mesh.vertices.size() / mesh.vertexStrideFloats);
        cache[key] = vertexIndex;

        const auto& position = positions[
            static_cast<std::size_t>(facePoint.position)];
        const std::array<float, 3> normal =
            facePoint.normal >= 0
            ? normals[static_cast<std::size_t>(facePoint.normal)]
            : std::array<float, 3>{ 0.0f, 1.0f, 0.0f };
        const std::array<float, 2> uv =
            facePoint.texcoord >= 0
            ? texcoords[static_cast<std::size_t>(facePoint.texcoord)]
            : std::array<float, 2>{ 0.0f, 0.0f };
        mesh.vertices.insert(
            mesh.vertices.end(),
            {
                position[0], position[1], position[2],
                normal[0], normal[1], normal[2],
                uv[0], uv[1],
                0.0f, 0.0f, 0.0f, 1.0f
            });
        return vertexIndex;
    };

    auto appendTriangle = [&](const std::array<FacePoint, 3>& points) {
        if (points[0].position < 0
            || points[1].position < 0
            || points[2].position < 0)
        {
            return;
        }

        if (mesh.drawRanges.empty()
            || mesh.drawRanges.back().materialName != currentMaterial)
        {
            MeshDrawRange range;
            range.firstIndex = mesh.indices.size();
            range.materialName = currentMaterial;
            mesh.drawRanges.push_back(std::move(range));
        }

        for (const FacePoint& point : points)
            mesh.indices.push_back(acquireVertex(point));
        mesh.drawRanges.back().indexCount += 3;
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
            float x = 0.0f, y = 0.0f, z = 0.0f;
            stream >> x >> y >> z;
            positions.push_back(
                convertAuthoringVector(
                    { x, y, z }, blenderCoordinates));
        }
        else if (token == "vn")
        {
            float x = 0.0f, y = 1.0f, z = 0.0f;
            stream >> x >> y >> z;
            normals.push_back(
                vectorNormalize(
                    convertAuthoringVector(
                        { x, y, z }, blenderCoordinates),
                    { 0.0f, 1.0f, 0.0f }));
        }
        else if (token == "vt")
        {
            float u = 0.0f, v = 0.0f;
            stream >> u >> v;
            texcoords.push_back({ u, v });
        }
        else if (token == "mtllib")
        {
            std::string materialName;
            std::getline(stream, materialName);
            materialName = trim(std::move(materialName));
            if (!materialName.empty())
            {
                const std::filesystem::path requested(materialName);
                if (!requested.is_absolute()
                    && !requested.has_root_name()
                    && std::none_of(
                        requested.begin(), requested.end(),
                        [](const auto& part) { return part == ".."; }))
                {
                    appendMaterialLibrary(
                        mesh,
                        (objPath.parent_path() / requested)
                            .lexically_normal());
                }
                else
                {
                    std::cerr
                        << "OBJ material warning: unsafe mtllib path ignored: "
                        << materialName << '\n';
                }
            }
        }
        else if (token == "usemtl")
        {
            std::getline(stream, currentMaterial);
            currentMaterial = trim(std::move(currentMaterial));
        }
        else if (token == "o")
        {
            std::string objectName;
            std::getline(stream, objectName);
            objectName = trim(std::move(objectName));
            skipCurrentObject = blenderCoordinates
                && isAuthoringMetadataObject(objectName);
        }
        else if (token == "f")
        {
            if (skipCurrentObject)
                continue;

            std::vector<FacePoint> face;
            std::string chunk;
            while (stream >> chunk)
            {
                face.push_back(
                    parseFacePoint(
                        chunk,
                        positions.size(),
                        texcoords.size(),
                        normals.size()));
            }

            for (std::size_t triangle = 1;
                 triangle + 1 < face.size();
                 ++triangle)
            {
                const std::array<FacePoint, 3> trianglePoints =
                    blenderCoordinates
                    ? std::array<FacePoint, 3>{
                        face[0], face[triangle + 1], face[triangle] }
                    : std::array<FacePoint, 3>{
                        face[0], face[triangle], face[triangle + 1] };
                appendTriangle(trianglePoints);
            }
        }
    }

    if (normalizeToUnit)
        normalizeMeshToUnit(mesh);

    mesh.hasTexcoords = computeTangents(mesh);

    if (mesh.drawRanges.empty() && !mesh.indices.empty())
    {
        mesh.drawRanges.push_back({
            0,
            mesh.indices.size(),
            {}
        });
    }

    std::cout
        << "OBJ loaded: " << mesh.indices.size() / 3
        << " triangles, " << mesh.materials.size()
        << " materials\n";
    return mesh;
}


void normalizeMeshToUnit(Mesh& mesh)
{
    if (mesh.vertices.empty() || mesh.vertexStrideFloats < 3)
        return;

    const std::size_t stride = mesh.vertexStrideFloats;
    float minX = mesh.vertices[0], maxX = mesh.vertices[0];
    float minY = mesh.vertices[1], maxY = mesh.vertices[1];
    float minZ = mesh.vertices[2], maxZ = mesh.vertices[2];
    for (std::size_t index = 0; index < mesh.vertices.size(); index += stride)
    {
        minX = std::min(minX, mesh.vertices[index]);
        maxX = std::max(maxX, mesh.vertices[index]);
        minY = std::min(minY, mesh.vertices[index + 1]);
        maxY = std::max(maxY, mesh.vertices[index + 1]);
        minZ = std::min(minZ, mesh.vertices[index + 2]);
        maxZ = std::max(maxZ, mesh.vertices[index + 2]);
    }

    const float centerX = (minX + maxX) * 0.5f;
    const float centerY = (minY + maxY) * 0.5f;
    const float centerZ = (minZ + maxZ) * 0.5f;
    const float maximumDimension =
        std::max({ maxX - minX, maxY - minY, maxZ - minZ });
    const float scale =
        maximumDimension > 1.0e-8f
        ? 2.0f / maximumDimension
        : 1.0f;

    for (std::size_t index = 0; index < mesh.vertices.size(); index += stride)
    {
        mesh.vertices[index] = (mesh.vertices[index] - centerX) * scale;
        mesh.vertices[index + 1] = (mesh.vertices[index + 1] - centerY) * scale;
        mesh.vertices[index + 2] = (mesh.vertices[index + 2] - centerZ) * scale;
    }
}

void uploadMesh(Mesh& mesh)
{
    if (mesh.vertexStrideFloats < 6
        || mesh.vertices.empty()
        || mesh.indices.empty())
    {
        return;
    }

    // PERF03 visibility data is authoring/load-time work, never frame work.
    computeDrawRangeBounds(mesh);
    computeTireVisualGeometry(mesh);

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);
    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        mesh.vertices.size() * sizeof(float),
        mesh.vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        mesh.indices.size() * sizeof(unsigned int),
        mesh.indices.data(),
        GL_STATIC_DRAW);

    const GLsizei stride =
        static_cast<GLsizei>(
            mesh.vertexStrideFloats * sizeof(float));
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    if (mesh.vertexStrideFloats >= 8)
    {
        glVertexAttribPointer(
            2, 2, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
    }

    if (mesh.vertexStrideFloats >= 12)
    {
        glVertexAttribPointer(
            3, 4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(8 * sizeof(float)));
        glEnableVertexAttribArray(3);
    }

    if (mesh.vertexStrideFloats >= 20)
    {
        glVertexAttribPointer(
            4, 4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(12 * sizeof(float)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(
            5, 4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(16 * sizeof(float)));
        glEnableVertexAttribArray(5);
    }

    if (mesh.vertexStrideFloats >= 24)
    {
        glVertexAttribPointer(
            6, 4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(20 * sizeof(float)));
        glEnableVertexAttribArray(6);
    }

    glBindVertexArray(0);
}

void destroyMesh(Mesh& mesh)
{
    if (mesh.vao)
        glDeleteVertexArrays(1, &mesh.vao);
    if (mesh.vbo)
        glDeleteBuffers(1, &mesh.vbo);
    if (mesh.ebo)
        glDeleteBuffers(1, &mesh.ebo);
    mesh = {};
}

} // namespace heritage::graphics
