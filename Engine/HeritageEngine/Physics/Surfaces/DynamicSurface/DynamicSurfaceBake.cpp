#include "DynamicSurfaceSystem.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <numbers>
#include <set>
#include <unordered_map>
#include <utility>

namespace heritage::physics::dynamicsurface {

namespace {

constexpr std::array<char, 8> kCacheMagic{{ 'H', 'D', 'S', 'F', '0', '1', '\0', '\0' }};
constexpr std::uint32_t kCacheVersion = 1u;
constexpr double kMinimumUpwardNormal = 0.15;
constexpr double kProjectedAreaEpsilonM2 = 1.0e-9;
constexpr double kQuantizationM = 0.001; // 1 mm adjacency identity.
constexpr double kChunkBoundaryToleranceM = 1.0e-7;
constexpr double kMaximumConnectedNormalBreakDegrees = 80.0;
constexpr double kMinimumConnectedNormalDot =
    0.17364817766693034885; // cos(80 degrees)
constexpr std::size_t kMaximumCacheChunkCount = 1'000'000;
constexpr std::size_t kMaximumCacheRecordCount = 100'000'000;

bool finite(double value)
{
    return std::isfinite(value);
}

bool finite(const heritage::math::DVec3& value)
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool finite(const heritage::math::Vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

heritage::math::DVec3 add(
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::DVec3 subtract(
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

heritage::math::DVec3 scale(
    const heritage::math::DVec3& value,
    double factor)
{
    return { value.x * factor, value.y * factor, value.z * factor };
}

heritage::math::DVec3 lerp(
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b,
    double t)
{
    return add(a, scale(subtract(b, a), t));
}

heritage::math::DVec3 cross(
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

double dot(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{
    return static_cast<double>(a.x) * static_cast<double>(b.x)
        + static_cast<double>(a.y) * static_cast<double>(b.y)
        + static_cast<double>(a.z) * static_cast<double>(b.z);
}

double length(const heritage::math::DVec3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

heritage::math::Vec3 normalizedOrUp(const heritage::math::Vec3& value)
{
    const double magnitude = std::sqrt(dot(value, value));
    if (!finite(magnitude) || magnitude <= 1.0e-12)
        return { 0.0f, 1.0f, 0.0f };
    return {
        static_cast<float>(static_cast<double>(value.x) / magnitude),
        static_cast<float>(static_cast<double>(value.y) / magnitude),
        static_cast<float>(static_cast<double>(value.z) / magnitude)
    };
}

double triangleArea3D(const StaticSurfacePatchTriangle& triangle)
{
    return 0.5 * length(cross(
        subtract(triangle.b, triangle.a),
        subtract(triangle.c, triangle.a)));
}

double projectedAreaXZ(
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b,
    const heritage::math::DVec3& c)
{
    return 0.5 * std::abs(
        (b.x - a.x) * (c.z - a.z)
        - (b.z - a.z) * (c.x - a.x));
}

std::int64_t chunkCoordinate(double value)
{
    return static_cast<std::int64_t>(std::floor(value / kChunkSizeM));
}

std::int64_t maximumChunkCoordinate(double maximum, double minimum)
{
    if (maximum > minimum)
        maximum = std::nextafter(maximum, -std::numeric_limits<double>::infinity());
    return chunkCoordinate(maximum);
}

template <typename T>
std::uint64_t hashBytes(std::uint64_t hash, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
    for (std::size_t i = 0; i < sizeof(T); ++i)
    {
        hash ^= static_cast<std::uint64_t>(bytes[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::uint64_t mixSeed(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ull;
    value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31u);
}

struct QuantizedPoint
{
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    bool operator==(const QuantizedPoint&) const = default;
    bool operator<(const QuantizedPoint& other) const
    {
        if (x != other.x)
            return x < other.x;
        if (y != other.y)
            return y < other.y;
        return z < other.z;
    }
};

QuantizedPoint quantize(const heritage::math::DVec3& point)
{
    return {
        static_cast<std::int64_t>(std::llround(point.x / kQuantizationM)),
        static_cast<std::int64_t>(std::llround(point.y / kQuantizationM)),
        static_cast<std::int64_t>(std::llround(point.z / kQuantizationM))
    };
}

struct QuantizedEdge
{
    QuantizedPoint a{};
    QuantizedPoint b{};

    bool operator==(const QuantizedEdge&) const = default;
    bool operator<(const QuantizedEdge& other) const
    {
        if (a < other.a)
            return true;
        if (other.a < a)
            return false;
        return b < other.b;
    }
};

QuantizedEdge quantizedEdge(
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b)
{
    QuantizedPoint qa = quantize(a);
    QuantizedPoint qb = quantize(b);
    if (qb < qa)
        std::swap(qa, qb);
    return { qa, qb };
}

struct EdgeCandidate
{
    QuantizedEdge key{};
    heritage::math::DVec3 a{ 0.0, 0.0, 0.0 };
    heritage::math::DVec3 b{ 0.0, 0.0, 0.0 };
    ChunkAddress chunk{};
    std::uint16_t sheet = 0;
    bool chunkBoundary = false;
};

struct DisjointSet
{
    explicit DisjointSet(std::size_t count)
        : parent(count), rank(count, 0u)
    {
        for (std::size_t i = 0; i < count; ++i)
            parent[i] = i;
    }

    std::size_t find(std::size_t value)
    {
        if (parent[value] != value)
            parent[value] = find(parent[value]);
        return parent[value];
    }

    void unite(std::size_t a, std::size_t b)
    {
        a = find(a);
        b = find(b);
        if (a == b)
            return;
        if (rank[a] < rank[b])
            std::swap(a, b);
        parent[b] = a;
        if (rank[a] == rank[b])
            ++rank[a];
    }

    std::vector<std::size_t> parent;
    std::vector<std::uint8_t> rank;
};

bool normalsConnect(
    const StaticSurfacePatchTriangle& a,
    const StaticSurfacePatchTriangle& b)
{
    return dot(a.normal, b.normal) >= kMinimumConnectedNormalDot;
}

std::array<std::pair<heritage::math::DVec3, heritage::math::DVec3>, 3>
triangleEdges(const StaticSurfacePatchTriangle& triangle)
{
    return {{
        { triangle.a, triangle.b },
        { triangle.b, triangle.c },
        { triangle.c, triangle.a }
    }};
}

bool edgeOnChunkBoundary(
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b,
    ChunkAddress chunk)
{
    const double minX = static_cast<double>(chunk.x) * kChunkSizeM;
    const double minZ = static_cast<double>(chunk.z) * kChunkSizeM;
    const double maxX = minX + kChunkSizeM;
    const double maxZ = minZ + kChunkSizeM;
    const auto close = [](double value, double target) {
        return std::abs(value - target) <= kChunkBoundaryToleranceM;
    };
    return (close(a.x, minX) && close(b.x, minX))
        || (close(a.x, maxX) && close(b.x, maxX))
        || (close(a.z, minZ) && close(b.z, minZ))
        || (close(a.z, maxZ) && close(b.z, maxZ));
}

using Polygon = std::vector<heritage::math::DVec3>;

enum class ClipAxis
{
    X,
    Z
};

Polygon clipAgainstPlane(
    const Polygon& input,
    ClipAxis axis,
    double boundary,
    bool keepGreater)
{
    Polygon output;
    if (input.empty())
        return output;
    output.reserve(input.size() + 1u);

    const auto coordinate = [axis](const heritage::math::DVec3& p) {
        return axis == ClipAxis::X ? p.x : p.z;
    };
    const auto inside = [&](const heritage::math::DVec3& p) {
        const double value = coordinate(p);
        return keepGreater
            ? value >= boundary - 1.0e-10
            : value <= boundary + 1.0e-10;
    };

    heritage::math::DVec3 previous = input.back();
    bool previousInside = inside(previous);
    for (const heritage::math::DVec3& current : input)
    {
        const bool currentInside = inside(current);
        if (currentInside != previousInside)
        {
            const double previousCoordinate = coordinate(previous);
            const double currentCoordinate = coordinate(current);
            const double denominator = currentCoordinate - previousCoordinate;
            if (std::abs(denominator) > 1.0e-15)
            {
                const double t = std::clamp(
                    (boundary - previousCoordinate) / denominator, 0.0, 1.0);
                heritage::math::DVec3 intersection = lerp(previous, current, t);
                if (axis == ClipAxis::X)
                    intersection.x = boundary;
                else
                    intersection.z = boundary;
                output.push_back(intersection);
            }
        }
        if (currentInside)
            output.push_back(current);
        previous = current;
        previousInside = currentInside;
    }
    return output;
}

Polygon clipToChunk(const Polygon& triangle, ChunkAddress chunk)
{
    const double minX = static_cast<double>(chunk.x) * kChunkSizeM;
    const double minZ = static_cast<double>(chunk.z) * kChunkSizeM;
    const double maxX = minX + kChunkSizeM;
    const double maxZ = minZ + kChunkSizeM;

    Polygon polygon = clipAgainstPlane(triangle, ClipAxis::X, minX, true);
    polygon = clipAgainstPlane(polygon, ClipAxis::X, maxX, false);
    polygon = clipAgainstPlane(polygon, ClipAxis::Z, minZ, true);
    polygon = clipAgainstPlane(polygon, ClipAxis::Z, maxZ, false);
    return polygon;
}

struct ChunkDraft
{
    std::vector<StaticSurfacePatchTriangle> triangles;
    std::vector<StaticSurfaceSheet> sheets;
    std::vector<StaticSurfaceBarrierSegment> barriers;
    std::vector<StaticSurfaceDrainRegion> drains;
    std::vector<EdgeCandidate> boundaryEdges;
};

struct SheetAccumulation
{
    std::size_t firstTriangle = std::numeric_limits<std::size_t>::max();
    std::uint32_t triangleCount = 0;
    heritage::math::DVec3 minimum{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity() };
    heritage::math::DVec3 maximum{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity() };
    heritage::math::DVec3 weightedNormal{ 0.0, 0.0, 0.0 };
    double totalArea = 0.0;
    double infiltrationAreaIntegral = 0.0;
    double maximumDrainage = 0.0;
    std::uint64_t materialMask = 0;
};

void expandBounds(SheetAccumulation& accumulation, const heritage::math::DVec3& p)
{
    accumulation.minimum.x = std::min(accumulation.minimum.x, p.x);
    accumulation.minimum.y = std::min(accumulation.minimum.y, p.y);
    accumulation.minimum.z = std::min(accumulation.minimum.z, p.z);
    accumulation.maximum.x = std::max(accumulation.maximum.x, p.x);
    accumulation.maximum.y = std::max(accumulation.maximum.y, p.y);
    accumulation.maximum.z = std::max(accumulation.maximum.z, p.z);
}

void buildChunkSheetsAndBoundaries(
    ChunkAddress address,
    ChunkDraft& draft,
    std::uint64_t sceneFingerprint)
{
    if (draft.triangles.empty())
        return;

    DisjointSet components(draft.triangles.size());
    std::map<QuantizedEdge, std::vector<std::size_t>> edgeOwners;
    for (std::size_t triangleIndex = 0;
         triangleIndex < draft.triangles.size();
         ++triangleIndex)
    {
        for (const auto& edge : triangleEdges(draft.triangles[triangleIndex]))
            edgeOwners[quantizedEdge(edge.first, edge.second)].push_back(triangleIndex);
    }

    for (const auto& entry : edgeOwners)
    {
        const std::vector<std::size_t>& owners = entry.second;
        for (std::size_t i = 0; i < owners.size(); ++i)
        {
            for (std::size_t j = i + 1; j < owners.size(); ++j)
            {
                if (normalsConnect(
                        draft.triangles[owners[i]],
                        draft.triangles[owners[j]]))
                {
                    components.unite(owners[i], owners[j]);
                }
            }
        }
    }

    // Deterministic sheet numbering by the first patch triangle in each
    // connected component, not by unordered-map iteration order.
    std::map<std::size_t, std::size_t> rootFirstTriangle;
    for (std::size_t i = 0; i < draft.triangles.size(); ++i)
    {
        const std::size_t root = components.find(i);
        auto [it, inserted] = rootFirstTriangle.emplace(root, i);
        if (!inserted)
            it->second = std::min(it->second, i);
    }
    std::vector<std::pair<std::size_t, std::size_t>> orderedRoots(
        rootFirstTriangle.begin(), rootFirstTriangle.end());
    std::sort(orderedRoots.begin(), orderedRoots.end(),
        [](const auto& left, const auto& right) {
            return left.second < right.second;
        });

    std::map<std::size_t, std::uint16_t> sheetForRoot;
    for (std::size_t i = 0; i < orderedRoots.size(); ++i)
    {
        const std::uint16_t id = static_cast<std::uint16_t>(
            std::min<std::size_t>(i, std::numeric_limits<std::uint16_t>::max()));
        sheetForRoot[orderedRoots[i].first] = id;
    }

    std::vector<SheetAccumulation> accumulations(orderedRoots.size());
    for (std::size_t i = 0; i < draft.triangles.size(); ++i)
    {
        StaticSurfacePatchTriangle& triangle = draft.triangles[i];
        triangle.surfaceSheetId = sheetForRoot[components.find(i)];
        SheetAccumulation& accumulation = accumulations[triangle.surfaceSheetId];
        accumulation.firstTriangle = std::min(accumulation.firstTriangle, i);
        ++accumulation.triangleCount;
        expandBounds(accumulation, triangle.a);
        expandBounds(accumulation, triangle.b);
        expandBounds(accumulation, triangle.c);
        const double area = triangleArea3D(triangle);
        accumulation.totalArea += area;
        accumulation.weightedNormal.x += static_cast<double>(triangle.normal.x) * area;
        accumulation.weightedNormal.y += static_cast<double>(triangle.normal.y) * area;
        accumulation.weightedNormal.z += static_cast<double>(triangle.normal.z) * area;
        accumulation.infiltrationAreaIntegral +=
            static_cast<double>(triangle.infiltrationCapacityMmPerHour) * area;
        accumulation.maximumDrainage = std::max(
            accumulation.maximumDrainage,
            static_cast<double>(triangle.drainageCapacityMmPerHour));
        if (triangle.materialId < 64u)
            accumulation.materialMask |= (1ull << triangle.materialId);
    }

    draft.sheets.resize(accumulations.size());
    for (std::size_t i = 0; i < accumulations.size(); ++i)
    {
        const SheetAccumulation& accumulation = accumulations[i];
        StaticSurfaceSheet sheet;
        sheet.id = static_cast<std::uint16_t>(i);
        sheet.triangleCount = accumulation.triangleCount;
        sheet.minimum = accumulation.minimum;
        sheet.maximum = accumulation.maximum;
        const double normalMagnitude = length(accumulation.weightedNormal);
        if (normalMagnitude > 1.0e-12)
        {
            sheet.averageNormal = {
                static_cast<float>(accumulation.weightedNormal.x / normalMagnitude),
                static_cast<float>(accumulation.weightedNormal.y / normalMagnitude),
                static_cast<float>(accumulation.weightedNormal.z / normalMagnitude)
            };
        }
        sheet.materialMask = accumulation.materialMask;
        if (accumulation.totalArea > 1.0e-12)
        {
            sheet.averageInfiltrationCapacityMmPerHour = static_cast<float>(
                accumulation.infiltrationAreaIntegral / accumulation.totalArea);
        }
        sheet.maximumDrainageCapacityMmPerHour =
            static_cast<float>(accumulation.maximumDrainage);
        std::uint64_t seed = sceneFingerprint;
        seed = hashBytes(seed, address.x);
        seed = hashBytes(seed, address.z);
        seed = hashBytes(seed, sheet.id);
        sheet.microtopographySeed = mixSeed(seed);
        draft.sheets[i] = sheet;
    }

    // Drain regions are static metadata derived from authored drainage-bearing
    // collision triangles. Patches are used so a drain crossing a chunk border
    // remains locally addressable in both chunks.
    for (std::size_t triangleIndex = 0;
         triangleIndex < draft.triangles.size();
         ++triangleIndex)
    {
        const StaticSurfacePatchTriangle& triangle = draft.triangles[triangleIndex];
        if (triangle.drainageCapacityMmPerHour <= 0.0f)
            continue;
        const double area = triangleArea3D(triangle);
        StaticSurfaceDrainRegion drain;
        std::uint64_t id = sceneFingerprint;
        id = hashBytes(id, address.x);
        id = hashBytes(id, address.z);
        id = hashBytes(id, triangle.surfaceSheetId);
        id = hashBytes(id, triangle.sourceTriangleIndex);
        id = hashBytes(id, triangleIndex);
        drain.id = mixSeed(id);
        drain.surfaceSheetId = triangle.surfaceSheetId;
        drain.center = scale(add(add(triangle.a, triangle.b), triangle.c), 1.0 / 3.0);
        drain.radiusM = static_cast<float>(std::sqrt(std::max(area, 0.0) / std::numbers::pi));
        drain.capacityMmPerHour = triangle.drainageCapacityMmPerHour;
        draft.drains.push_back(drain);
    }

    // Classify every sheet boundary. Artificial 100m chunk cuts are held as
    // candidates until the full scene can match them against the neighboring
    // chunk; unmatched ones become real scene/world boundaries.
    for (const auto& entry : edgeOwners)
    {
        const std::vector<std::size_t>& owners = entry.second;
        for (const std::size_t owner : owners)
        {
            const StaticSurfacePatchTriangle& triangle = draft.triangles[owner];
            bool sameSheetNeighbor = false;
            for (const std::size_t other : owners)
            {
                if (other == owner)
                    continue;
                if (draft.triangles[other].surfaceSheetId == triangle.surfaceSheetId)
                {
                    sameSheetNeighbor = true;
                    break;
                }
            }
            if (sameSheetNeighbor)
                continue;

            heritage::math::DVec3 edgeA{};
            heritage::math::DVec3 edgeB{};
            bool foundEdge = false;
            for (const auto& edge : triangleEdges(triangle))
            {
                if (quantizedEdge(edge.first, edge.second) == entry.first)
                {
                    edgeA = edge.first;
                    edgeB = edge.second;
                    foundEdge = true;
                    break;
                }
            }
            if (!foundEdge)
                continue;

            EdgeCandidate candidate;
            candidate.key = entry.first;
            candidate.a = edgeA;
            candidate.b = edgeB;
            candidate.chunk = address;
            candidate.sheet = triangle.surfaceSheetId;
            candidate.chunkBoundary = edgeOnChunkBoundary(edgeA, edgeB, address);
            if (candidate.chunkBoundary)
            {
                draft.boundaryEdges.push_back(candidate);
            }
            else
            {
                draft.barriers.push_back({
                    edgeA,
                    edgeB,
                    triangle.surfaceSheetId,
                    StaticSurfaceBarrierKind::HardGeometry
                });
            }
        }
    }
}

void addPatchTriangle(
    ChunkDraft& draft,
    const StaticSceneTriangle& source,
    std::uint32_t sourceTriangleIndex,
    ChunkAddress chunk,
    std::uint32_t fanTriangleIndex,
    std::uint64_t sceneFingerprint,
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b,
    const heritage::math::DVec3& c)
{
    if (projectedAreaXZ(a, b, c) <= kProjectedAreaEpsilonM2)
        return;

    StaticSurfacePatchTriangle triangle;
    triangle.a = a;
    triangle.b = b;
    triangle.c = c;
    triangle.normal = normalizedOrUp(source.normal);
    triangle.sourceTriangleIndex = sourceTriangleIndex;
    triangle.materialId = static_cast<std::uint32_t>(source.surfaceMaterial);
    triangle.hydrologyAuthored = source.surfaceProperties.hydrology.authored;
    triangle.authoredWetness = source.surfaceWetness;
    triangle.infiltrationCapacityMmPerHour = static_cast<float>(
        source.surfaceProperties.hydrology.infiltrationCapacityMmPerHour);
    triangle.drainageCapacityMmPerHour = static_cast<float>(
        source.surfaceProperties.hydrology.drainageCapacityMmPerHour);
    triangle.flowRoughness = static_cast<float>(
        source.surfaceProperties.hydrology.flowRoughness);
    triangle.depressionStorageM = static_cast<float>(
        source.surfaceProperties.hydrology.depressionStorageMm / 1000.0);
    triangle.hasAuthoredSurfaceTemperature =
        source.surfaceProperties.hasAuthoredSurfaceTemperature;
    triangle.authoredSurfaceTemperatureC = static_cast<float>(
        source.surfaceProperties.authoredSurfaceTemperatureC);

    std::uint64_t seed = sceneFingerprint;
    seed = hashBytes(seed, sourceTriangleIndex);
    seed = hashBytes(seed, chunk.x);
    seed = hashBytes(seed, chunk.z);
    seed = hashBytes(seed, fanTriangleIndex);
    triangle.microtopographySeed = mixSeed(seed);
    draft.triangles.push_back(triangle);
}

template <typename T>
bool writeValue(std::ofstream& stream, const T& value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return stream.good();
}

template <typename T>
bool readValue(std::ifstream& stream, T& value)
{
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return stream.good();
}

bool writeDVec3(std::ofstream& stream, const heritage::math::DVec3& value)
{
    return writeValue(stream, value.x)
        && writeValue(stream, value.y)
        && writeValue(stream, value.z);
}

bool readDVec3(std::ifstream& stream, heritage::math::DVec3& value)
{
    return readValue(stream, value.x)
        && readValue(stream, value.y)
        && readValue(stream, value.z);
}

bool writeVec3(std::ofstream& stream, const heritage::math::Vec3& value)
{
    return writeValue(stream, value.x)
        && writeValue(stream, value.y)
        && writeValue(stream, value.z);
}

bool readVec3(std::ifstream& stream, heritage::math::Vec3& value)
{
    return readValue(stream, value.x)
        && readValue(stream, value.y)
        && readValue(stream, value.z);
}

bool safeCount(std::uint64_t value)
{
    return value <= static_cast<std::uint64_t>(kMaximumCacheRecordCount);
}

} // namespace

std::uint64_t DynamicSurfaceSystem::staticSceneFingerprint(
    const std::vector<StaticSceneTriangle>& localTriangles,
    const heritage::math::DVec3& globalOrigin) const
{
    std::uint64_t hash = 1469598103934665603ull;
    hash = hashBytes(hash, kCacheVersion);
    hash = hashBytes(hash, kChunkSizeM);
    hash = hashBytes(hash, kLogicalResolution);
    hash = hashBytes(hash, kPhysicalPageResolution);
    hash = hashBytes(hash, kMinimumUpwardNormal);
    hash = hashBytes(hash, globalOrigin.x);
    hash = hashBytes(hash, globalOrigin.y);
    hash = hashBytes(hash, globalOrigin.z);
    for (const StaticSceneTriangle& triangle : localTriangles)
    {
        hash = hashBytes(hash, triangle.a);
        hash = hashBytes(hash, triangle.b);
        hash = hashBytes(hash, triangle.c);
        hash = hashBytes(hash, triangle.normal);
        const std::uint32_t material = static_cast<std::uint32_t>(triangle.surfaceMaterial);
        hash = hashBytes(hash, material);
        hash = hashBytes(hash, triangle.surfaceWetness);
        const auto& hydrology = triangle.surfaceProperties.hydrology;
        hash = hashBytes(hash, hydrology.authored);
        hash = hashBytes(hash, hydrology.infiltrationCapacityMmPerHour);
        hash = hashBytes(hash, hydrology.drainageCapacityMmPerHour);
        hash = hashBytes(hash, hydrology.flowRoughness);
        hash = hashBytes(hash, hydrology.depressionStorageMm);
        hash = hashBytes(hash, triangle.surfaceProperties.hasAuthoredSurfaceTemperature);
        hash = hashBytes(hash, triangle.surfaceProperties.authoredSurfaceTemperatureC);
    }
    return hash;
}

bool DynamicSurfaceSystem::loadOrBakeStaticScene(
    const std::vector<StaticSceneTriangle>& localTriangles,
    const heritage::math::DVec3& globalOrigin,
    const std::filesystem::path& cachePath,
    DynamicSurfaceStaticBakeReport& report)
{
    const auto started = std::chrono::steady_clock::now();
    const std::uint64_t fingerprint = staticSceneFingerprint(localTriangles, globalOrigin);
    if (!cachePath.empty()
        && loadStaticBakeCache(cachePath, fingerprint, report))
    {
        report.sourceTriangleCount = localTriangles.size();
        report.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        m_lastStaticBakeReport = report;
        return true;
    }

    if (!bakeStaticScene(localTriangles, globalOrigin, report))
        return false;

    report.sourceFingerprint = fingerprint;
    report.cachePath = cachePath;
    if (!cachePath.empty())
    {
        if (writeStaticBakeCache(cachePath, fingerprint))
            report.message += " Cache written.";
        else
            report.message += " Cache write failed; runtime bake remains valid.";
    }
    report.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    m_lastStaticBakeReport = report;
    return true;
}

bool DynamicSurfaceSystem::bakeStaticScene(
    const std::vector<StaticSceneTriangle>& localTriangles,
    const heritage::math::DVec3& globalOrigin,
    DynamicSurfaceStaticBakeReport& report)
{
    const auto started = std::chrono::steady_clock::now();
    m_chunks.clear();
    m_pagePool.clear();
    m_hydrology.clear();
    m_sheetLinks.clear();
    report = {};
    report.sourceTriangleCount = localTriangles.size();

    if (localTriangles.empty() || !finite(globalOrigin))
    {
        report.message = "Dynamic Surface static bake requires finite static collision triangles.";
        m_lastStaticBakeReport = report;
        return false;
    }

    const std::uint64_t sceneFingerprint = staticSceneFingerprint(localTriangles, globalOrigin);
    std::map<ChunkAddress, ChunkDraft> drafts;

    for (std::uint32_t sourceIndex = 0;
         sourceIndex < static_cast<std::uint32_t>(localTriangles.size());
         ++sourceIndex)
    {
        const StaticSceneTriangle& source = localTriangles[sourceIndex];
        if (!finite(source.normal)
            || static_cast<double>(source.normal.y) < kMinimumUpwardNormal)
        {
            continue;
        }

        const Polygon globalTriangle{{
            { globalOrigin.x + static_cast<double>(source.a.x),
              globalOrigin.y + static_cast<double>(source.a.y),
              globalOrigin.z + static_cast<double>(source.a.z) },
            { globalOrigin.x + static_cast<double>(source.b.x),
              globalOrigin.y + static_cast<double>(source.b.y),
              globalOrigin.z + static_cast<double>(source.b.z) },
            { globalOrigin.x + static_cast<double>(source.c.x),
              globalOrigin.y + static_cast<double>(source.c.y),
              globalOrigin.z + static_cast<double>(source.c.z) }
        }};
        if (!finite(globalTriangle[0])
            || !finite(globalTriangle[1])
            || !finite(globalTriangle[2])
            || projectedAreaXZ(
                globalTriangle[0], globalTriangle[1], globalTriangle[2])
                <= kProjectedAreaEpsilonM2)
        {
            continue;
        }

        ++report.acceptedTriangleCount;
        const double minimumX = std::min({
            globalTriangle[0].x, globalTriangle[1].x, globalTriangle[2].x });
        const double maximumX = std::max({
            globalTriangle[0].x, globalTriangle[1].x, globalTriangle[2].x });
        const double minimumZ = std::min({
            globalTriangle[0].z, globalTriangle[1].z, globalTriangle[2].z });
        const double maximumZ = std::max({
            globalTriangle[0].z, globalTriangle[1].z, globalTriangle[2].z });

        const std::int64_t minimumChunkX = chunkCoordinate(minimumX);
        const std::int64_t maximumChunkX = maximumChunkCoordinate(maximumX, minimumX);
        const std::int64_t minimumChunkZ = chunkCoordinate(minimumZ);
        const std::int64_t maximumChunkZ = maximumChunkCoordinate(maximumZ, minimumZ);

        for (std::int64_t chunkZ = minimumChunkZ; chunkZ <= maximumChunkZ; ++chunkZ)
        {
            for (std::int64_t chunkX = minimumChunkX; chunkX <= maximumChunkX; ++chunkX)
            {
                const ChunkAddress address{ chunkX, chunkZ };
                const Polygon clipped = clipToChunk(globalTriangle, address);
                if (clipped.size() < 3u)
                    continue;
                ChunkDraft& draft = drafts[address];
                for (std::size_t fan = 1; fan + 1 < clipped.size(); ++fan)
                {
                    addPatchTriangle(
                        draft,
                        source,
                        sourceIndex,
                        address,
                        static_cast<std::uint32_t>(fan - 1u),
                        sceneFingerprint,
                        clipped[0],
                        clipped[fan],
                        clipped[fan + 1u]);
                }
            }
        }
    }

    if (drafts.empty())
    {
        report.message = "No upward-facing collision area was suitable for Dynamic Surface.";
        m_lastStaticBakeReport = report;
        return false;
    }

    std::vector<EdgeCandidate> chunkBoundaryCandidates;
    for (auto& entry : drafts)
    {
        buildChunkSheetsAndBoundaries(entry.first, entry.second, sceneFingerprint);
        chunkBoundaryCandidates.insert(
            chunkBoundaryCandidates.end(),
            entry.second.boundaryEdges.begin(),
            entry.second.boundaryEdges.end());
    }

    // Match artificial chunk cuts in full 3D. Surface sheets stacked at the
    // same X/Z never link unless their boundary elevations actually agree.
    std::map<QuantizedEdge, std::vector<std::size_t>> candidatesByEdge;
    for (std::size_t i = 0; i < chunkBoundaryCandidates.size(); ++i)
        candidatesByEdge[chunkBoundaryCandidates[i].key].push_back(i);
    std::vector<bool> matched(chunkBoundaryCandidates.size(), false);

    for (const auto& entry : candidatesByEdge)
    {
        const std::vector<std::size_t>& indices = entry.second;
        for (std::size_t i = 0; i < indices.size(); ++i)
        {
            const EdgeCandidate& left = chunkBoundaryCandidates[indices[i]];
            for (std::size_t j = i + 1; j < indices.size(); ++j)
            {
                const EdgeCandidate& right = chunkBoundaryCandidates[indices[j]];
                if (left.chunk == right.chunk)
                    continue;
                StaticSurfaceSheetLink link;
                if (right.chunk < left.chunk)
                {
                    link.chunkA = right.chunk;
                    link.sheetA = right.sheet;
                    link.chunkB = left.chunk;
                    link.sheetB = left.sheet;
                }
                else
                {
                    link.chunkA = left.chunk;
                    link.sheetA = left.sheet;
                    link.chunkB = right.chunk;
                    link.sheetB = right.sheet;
                }
                link.edgeA = left.a;
                link.edgeB = left.b;
                m_sheetLinks.push_back(link);
                matched[indices[i]] = true;
                matched[indices[j]] = true;
            }
        }
    }

    std::sort(m_sheetLinks.begin(), m_sheetLinks.end(),
        [](const StaticSurfaceSheetLink& a, const StaticSurfaceSheetLink& b) {
            if (a.chunkA < b.chunkA)
                return true;
            if (b.chunkA < a.chunkA)
                return false;
            if (a.sheetA != b.sheetA)
                return a.sheetA < b.sheetA;
            if (a.chunkB < b.chunkB)
                return true;
            if (b.chunkB < a.chunkB)
                return false;
            if (a.sheetB != b.sheetB)
                return a.sheetB < b.sheetB;
            return quantizedEdge(a.edgeA, a.edgeB) < quantizedEdge(b.edgeA, b.edgeB);
        });
    m_sheetLinks.erase(std::unique(m_sheetLinks.begin(), m_sheetLinks.end(),
        [](const StaticSurfaceSheetLink& a, const StaticSurfaceSheetLink& b) {
            return a.chunkA == b.chunkA
                && a.sheetA == b.sheetA
                && a.chunkB == b.chunkB
                && a.sheetB == b.sheetB
                && quantizedEdge(a.edgeA, a.edgeB)
                    == quantizedEdge(b.edgeA, b.edgeB);
        }), m_sheetLinks.end());

    for (std::size_t i = 0; i < chunkBoundaryCandidates.size(); ++i)
    {
        if (matched[i])
            continue;
        const EdgeCandidate& candidate = chunkBoundaryCandidates[i];
        drafts[candidate.chunk].barriers.push_back({
            candidate.a,
            candidate.b,
            candidate.sheet,
            StaticSurfaceBarrierKind::WorldOrSceneEdge
        });
    }

    // Commit only complete deterministic bake results to live chunks.
    m_chunks.clear();
    for (auto& entry : drafts)
    {
        DynamicSurfaceChunk& chunk = acquireChunk(entry.first);
        chunk.setStaticSurfaceData(
            std::move(entry.second.triangles),
            std::move(entry.second.sheets),
            std::move(entry.second.barriers),
            std::move(entry.second.drains));
    }

    report.valid = !m_chunks.empty();
    report.loadedFromCache = false;
    report.sourceFingerprint = sceneFingerprint;
    report.chunkCount = m_chunks.size();
    report.crossChunkSheetLinkCount = m_sheetLinks.size();
    for (const auto& entry : m_chunks)
    {
        report.clippedPatchTriangleCount += entry.second.staticTriangles().size();
        report.surfaceSheetCount += entry.second.staticSheets().size();
        report.hardBarrierSegmentCount += entry.second.staticBarriers().size();
        report.drainRegionCount += entry.second.staticDrains().size();
    }
    report.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    report.message = "Dynamic Surface static scene baked into persistent 100m chunks and independent surface sheets.";
    m_lastStaticBakeReport = report;
    return report.valid;
}

bool DynamicSurfaceSystem::writeStaticBakeCache(
    const std::filesystem::path& cachePath,
    std::uint64_t fingerprint) const
{
    std::error_code error;
    if (cachePath.has_parent_path())
        std::filesystem::create_directories(cachePath.parent_path(), error);
    std::ofstream stream(cachePath, std::ios::binary | std::ios::trunc);
    if (!stream)
        return false;

    stream.write(kCacheMagic.data(), static_cast<std::streamsize>(kCacheMagic.size()));
    const std::uint64_t chunkCount = m_chunks.size();
    const std::uint64_t linkCount = m_sheetLinks.size();
    if (!writeValue(stream, kCacheVersion)
        || !writeValue(stream, fingerprint)
        || !writeValue(stream, chunkCount)
        || !writeValue(stream, linkCount))
    {
        return false;
    }

    for (const auto& entry : m_chunks)
    {
        const DynamicSurfaceChunk& chunk = entry.second;
        const std::uint64_t sheetCount = chunk.staticSheets().size();
        const std::uint64_t triangleCount = chunk.staticTriangles().size();
        const std::uint64_t barrierCount = chunk.staticBarriers().size();
        const std::uint64_t drainCount = chunk.staticDrains().size();
        if (!writeValue(stream, entry.first.x)
            || !writeValue(stream, entry.first.z)
            || !writeValue(stream, sheetCount)
            || !writeValue(stream, triangleCount)
            || !writeValue(stream, barrierCount)
            || !writeValue(stream, drainCount))
        {
            return false;
        }

        for (const StaticSurfaceSheet& sheet : chunk.staticSheets())
        {
            if (!writeValue(stream, sheet.id)
                || !writeValue(stream, sheet.triangleCount)
                || !writeDVec3(stream, sheet.minimum)
                || !writeDVec3(stream, sheet.maximum)
                || !writeVec3(stream, sheet.averageNormal)
                || !writeValue(stream, sheet.materialMask)
                || !writeValue(stream, sheet.averageInfiltrationCapacityMmPerHour)
                || !writeValue(stream, sheet.maximumDrainageCapacityMmPerHour)
                || !writeValue(stream, sheet.microtopographySeed))
            {
                return false;
            }
        }

        for (const StaticSurfacePatchTriangle& triangle : chunk.staticTriangles())
        {
            const std::uint8_t hydrologyAuthored = triangle.hydrologyAuthored ? 1u : 0u;
            const std::uint8_t temperatureAuthored =
                triangle.hasAuthoredSurfaceTemperature ? 1u : 0u;
            if (!writeDVec3(stream, triangle.a)
                || !writeDVec3(stream, triangle.b)
                || !writeDVec3(stream, triangle.c)
                || !writeVec3(stream, triangle.normal)
                || !writeValue(stream, triangle.sourceTriangleIndex)
                || !writeValue(stream, triangle.materialId)
                || !writeValue(stream, triangle.surfaceSheetId)
                || !writeValue(stream, hydrologyAuthored)
                || !writeValue(stream, triangle.authoredWetness)
                || !writeValue(stream, triangle.infiltrationCapacityMmPerHour)
                || !writeValue(stream, triangle.drainageCapacityMmPerHour)
                || !writeValue(stream, triangle.flowRoughness)
                || !writeValue(stream, triangle.depressionStorageM)
                || !writeValue(stream, temperatureAuthored)
                || !writeValue(stream, triangle.authoredSurfaceTemperatureC)
                || !writeValue(stream, triangle.microtopographySeed))
            {
                return false;
            }
        }

        for (const StaticSurfaceBarrierSegment& barrier : chunk.staticBarriers())
        {
            const std::uint8_t kind = static_cast<std::uint8_t>(barrier.kind);
            if (!writeDVec3(stream, barrier.a)
                || !writeDVec3(stream, barrier.b)
                || !writeValue(stream, barrier.surfaceSheetId)
                || !writeValue(stream, kind))
            {
                return false;
            }
        }

        for (const StaticSurfaceDrainRegion& drain : chunk.staticDrains())
        {
            if (!writeValue(stream, drain.id)
                || !writeValue(stream, drain.surfaceSheetId)
                || !writeDVec3(stream, drain.center)
                || !writeValue(stream, drain.radiusM)
                || !writeValue(stream, drain.capacityMmPerHour))
            {
                return false;
            }
        }
    }

    for (const StaticSurfaceSheetLink& link : m_sheetLinks)
    {
        if (!writeValue(stream, link.chunkA.x)
            || !writeValue(stream, link.chunkA.z)
            || !writeValue(stream, link.sheetA)
            || !writeValue(stream, link.chunkB.x)
            || !writeValue(stream, link.chunkB.z)
            || !writeValue(stream, link.sheetB)
            || !writeDVec3(stream, link.edgeA)
            || !writeDVec3(stream, link.edgeB))
        {
            return false;
        }
    }
    return stream.good();
}

bool DynamicSurfaceSystem::loadStaticBakeCache(
    const std::filesystem::path& cachePath,
    std::uint64_t expectedFingerprint,
    DynamicSurfaceStaticBakeReport& report)
{
    std::ifstream stream(cachePath, std::ios::binary);
    if (!stream)
        return false;

    std::array<char, 8> magic{};
    stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    std::uint32_t version = 0;
    std::uint64_t fingerprint = 0;
    std::uint64_t chunkCount = 0;
    std::uint64_t linkCount = 0;
    if (!stream.good()
        || magic != kCacheMagic
        || !readValue(stream, version)
        || !readValue(stream, fingerprint)
        || !readValue(stream, chunkCount)
        || !readValue(stream, linkCount)
        || version != kCacheVersion
        || fingerprint != expectedFingerprint
        || chunkCount > kMaximumCacheChunkCount
        || !safeCount(linkCount))
    {
        return false;
    }

    std::map<ChunkAddress, DynamicSurfaceChunk> loadedChunks;
    std::size_t totalSheets = 0;
    std::size_t totalTriangles = 0;
    std::size_t totalBarriers = 0;
    std::size_t totalDrains = 0;

    for (std::uint64_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
    {
        ChunkAddress address;
        std::uint64_t sheetCount = 0;
        std::uint64_t triangleCount = 0;
        std::uint64_t barrierCount = 0;
        std::uint64_t drainCount = 0;
        if (!readValue(stream, address.x)
            || !readValue(stream, address.z)
            || !readValue(stream, sheetCount)
            || !readValue(stream, triangleCount)
            || !readValue(stream, barrierCount)
            || !readValue(stream, drainCount)
            || !safeCount(sheetCount)
            || !safeCount(triangleCount)
            || !safeCount(barrierCount)
            || !safeCount(drainCount))
        {
            return false;
        }

        std::vector<StaticSurfaceSheet> sheets(static_cast<std::size_t>(sheetCount));
        std::vector<StaticSurfacePatchTriangle> triangles(
            static_cast<std::size_t>(triangleCount));
        std::vector<StaticSurfaceBarrierSegment> barriers(
            static_cast<std::size_t>(barrierCount));
        std::vector<StaticSurfaceDrainRegion> drains(
            static_cast<std::size_t>(drainCount));

        for (StaticSurfaceSheet& sheet : sheets)
        {
            if (!readValue(stream, sheet.id)
                || !readValue(stream, sheet.triangleCount)
                || !readDVec3(stream, sheet.minimum)
                || !readDVec3(stream, sheet.maximum)
                || !readVec3(stream, sheet.averageNormal)
                || !readValue(stream, sheet.materialMask)
                || !readValue(stream, sheet.averageInfiltrationCapacityMmPerHour)
                || !readValue(stream, sheet.maximumDrainageCapacityMmPerHour)
                || !readValue(stream, sheet.microtopographySeed))
            {
                return false;
            }
        }

        for (StaticSurfacePatchTriangle& triangle : triangles)
        {
            std::uint8_t hydrologyAuthored = 0;
            std::uint8_t temperatureAuthored = 0;
            if (!readDVec3(stream, triangle.a)
                || !readDVec3(stream, triangle.b)
                || !readDVec3(stream, triangle.c)
                || !readVec3(stream, triangle.normal)
                || !readValue(stream, triangle.sourceTriangleIndex)
                || !readValue(stream, triangle.materialId)
                || !readValue(stream, triangle.surfaceSheetId)
                || !readValue(stream, hydrologyAuthored)
                || !readValue(stream, triangle.authoredWetness)
                || !readValue(stream, triangle.infiltrationCapacityMmPerHour)
                || !readValue(stream, triangle.drainageCapacityMmPerHour)
                || !readValue(stream, triangle.flowRoughness)
                || !readValue(stream, triangle.depressionStorageM)
                || !readValue(stream, temperatureAuthored)
                || !readValue(stream, triangle.authoredSurfaceTemperatureC)
                || !readValue(stream, triangle.microtopographySeed))
            {
                return false;
            }
            triangle.hydrologyAuthored = hydrologyAuthored != 0u;
            triangle.hasAuthoredSurfaceTemperature = temperatureAuthored != 0u;
        }

        for (StaticSurfaceBarrierSegment& barrier : barriers)
        {
            std::uint8_t kind = 0;
            if (!readDVec3(stream, barrier.a)
                || !readDVec3(stream, barrier.b)
                || !readValue(stream, barrier.surfaceSheetId)
                || !readValue(stream, kind)
                || kind > static_cast<std::uint8_t>(
                    StaticSurfaceBarrierKind::WorldOrSceneEdge))
            {
                return false;
            }
            barrier.kind = static_cast<StaticSurfaceBarrierKind>(kind);
        }

        for (StaticSurfaceDrainRegion& drain : drains)
        {
            if (!readValue(stream, drain.id)
                || !readValue(stream, drain.surfaceSheetId)
                || !readDVec3(stream, drain.center)
                || !readValue(stream, drain.radiusM)
                || !readValue(stream, drain.capacityMmPerHour))
            {
                return false;
            }
        }

        DynamicSurfaceChunk chunk(address);
        chunk.setStaticSurfaceData(
            std::move(triangles),
            std::move(sheets),
            std::move(barriers),
            std::move(drains));
        loadedChunks.emplace(address, std::move(chunk));
        totalSheets += static_cast<std::size_t>(sheetCount);
        totalTriangles += static_cast<std::size_t>(triangleCount);
        totalBarriers += static_cast<std::size_t>(barrierCount);
        totalDrains += static_cast<std::size_t>(drainCount);
    }

    std::vector<StaticSurfaceSheetLink> loadedLinks(static_cast<std::size_t>(linkCount));
    for (StaticSurfaceSheetLink& link : loadedLinks)
    {
        if (!readValue(stream, link.chunkA.x)
            || !readValue(stream, link.chunkA.z)
            || !readValue(stream, link.sheetA)
            || !readValue(stream, link.chunkB.x)
            || !readValue(stream, link.chunkB.z)
            || !readValue(stream, link.sheetB)
            || !readDVec3(stream, link.edgeA)
            || !readDVec3(stream, link.edgeB))
        {
            return false;
        }
    }

    if (!stream.good() && !stream.eof())
        return false;

    m_pagePool.clear();
    m_hydrology.clear();
    m_chunks = std::move(loadedChunks);
    m_sheetLinks = std::move(loadedLinks);
    report = {};
    report.valid = !m_chunks.empty();
    report.loadedFromCache = true;
    report.sourceFingerprint = expectedFingerprint;
    report.cachePath = cachePath;
    report.chunkCount = m_chunks.size();
    report.surfaceSheetCount = totalSheets;
    report.clippedPatchTriangleCount = totalTriangles;
    report.hardBarrierSegmentCount = totalBarriers;
    report.drainRegionCount = totalDrains;
    report.crossChunkSheetLinkCount = m_sheetLinks.size();
    report.message = "Dynamic Surface static scene loaded from deterministic cache.";
    m_lastStaticBakeReport = report;
    return report.valid;
}

} // namespace heritage::physics::dynamicsurface
