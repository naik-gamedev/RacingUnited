#include "SurfaceHydrology.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace heritage::physics::water {
namespace {

bool finitePosition(const heritage::math::DVec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

std::uint64_t prebakedTileKey(std::int32_t x, std::int32_t z)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32u)
        | static_cast<std::uint32_t>(z);
}

} // namespace

void SurfaceHydrology::rebuildPrebakedTriangleTileLookup()
{
    struct TileTrianglePair
    {
        std::uint64_t key = 0;
        std::int32_t triangle = -1;
    };
    m_prebakedTriangleTileSpans.clear();
    m_prebakedTriangleTileIndices.clear();
    if (m_prebakedTriangles.empty())
        return;

    std::vector<TileTrianglePair> pairs;
    pairs.reserve(m_prebakedTriangles.size() + m_prebakedTriangles.size() / 8u);
    constexpr double kTileSizeM = 10.0;
    for (std::size_t index = 0; index < m_prebakedTriangles.size(); ++index)
    {
        const PrebakedTriangle& triangle = m_prebakedTriangles[index];
        const double minimumX = std::min({ triangle.a.x, triangle.b.x, triangle.c.x });
        const double maximumX = std::max({ triangle.a.x, triangle.b.x, triangle.c.x });
        const double minimumZ = std::min({ triangle.a.z, triangle.b.z, triangle.c.z });
        const double maximumZ = std::max({ triangle.a.z, triangle.b.z, triangle.c.z });
        const std::int64_t minimumTileX = static_cast<std::int64_t>(
            std::floor(minimumX / kTileSizeM));
        const std::int64_t maximumTileX = static_cast<std::int64_t>(
            std::floor(std::nextafter(maximumX, minimumX) / kTileSizeM));
        const std::int64_t minimumTileZ = static_cast<std::int64_t>(
            std::floor(minimumZ / kTileSizeM));
        const std::int64_t maximumTileZ = static_cast<std::int64_t>(
            std::floor(std::nextafter(maximumZ, minimumZ) / kTileSizeM));
        for (std::int64_t z = minimumTileZ; z <= maximumTileZ; ++z)
        {
            for (std::int64_t x = minimumTileX; x <= maximumTileX; ++x)
            {
                if (x < (std::numeric_limits<std::int32_t>::min)()
                    || x > (std::numeric_limits<std::int32_t>::max)()
                    || z < (std::numeric_limits<std::int32_t>::min)()
                    || z > (std::numeric_limits<std::int32_t>::max)())
                {
                    continue;
                }
                pairs.push_back({
                    prebakedTileKey(static_cast<std::int32_t>(x), static_cast<std::int32_t>(z)),
                    static_cast<std::int32_t>(index) });
            }
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](const TileTrianglePair& a, const TileTrianglePair& b) {
        if (a.key != b.key) return a.key < b.key;
        return a.triangle < b.triangle;
    });

    m_prebakedTriangleTileIndices.reserve(pairs.size());
    m_prebakedTriangleTileSpans.reserve(std::max<std::size_t>(pairs.size() / 2u, 16u));
    std::size_t cursor = 0u;
    while (cursor < pairs.size())
    {
        const std::uint64_t key = pairs[cursor].key;
        const std::uint64_t first = static_cast<std::uint64_t>(m_prebakedTriangleTileIndices.size());
        std::uint32_t count = 0u;
        while (cursor < pairs.size() && pairs[cursor].key == key)
        {
            m_prebakedTriangleTileIndices.push_back(pairs[cursor].triangle);
            ++count;
            ++cursor;
        }
        m_prebakedTriangleTileSpans.push_back({ key, first, count, 0u });
    }
}

const SurfaceHydrology::PrebakedTriangleTileSpan* SurfaceHydrology::prebakedTriangleTileSpan(
    std::int32_t tileX, std::int32_t tileZ) const
{
    const std::uint64_t key = prebakedTileKey(tileX, tileZ);
    const auto found = std::lower_bound(
        m_prebakedTriangleTileSpans.begin(), m_prebakedTriangleTileSpans.end(), key,
        [](const PrebakedTriangleTileSpan& span, std::uint64_t value) {
            return span.key < value;
        });
    return found != m_prebakedTriangleTileSpans.end() && found->key == key
        ? &*found : nullptr;
}

void SurfaceHydrology::rebuildPrebakedTriangleTopology(
    const std::vector<StaticSceneTriangle>& localTriangles,
    const heritage::math::DVec3& globalOrigin)
{
    m_prebakedTriangles.clear();
    m_prebakedTriangleTileSpans.clear();
    m_prebakedTriangleTileIndices.clear();
    if (localTriangles.empty() || !finitePosition(globalOrigin))
        return;

    // LIVETRACK20 static hydrology uses a welded collision-mesh vertex graph.
    // The previous one-downstream-neighbour-per-triangle solve could find broad
    // terminal basins, but it could not represent water that converges onto and
    // then travels ALONG a shared low edge (the common road-crown/gutter case).
    //
    // This bake has two complementary passes over the same immutable graph:
    //   1) priority flood from true open boundaries -> minimum escape/spill head;
    //   2) strict downhill vertex routing -> contributing-area accumulation.
    // Per-vertex results are stored on each authored triangle and reconstructed
    // barycentrically when the 256x256 / 32x32 presentation textures are made.
    constexpr double kEdgeQuantizationM = 0.002; // 2 mm X/Z edge matching.
    constexpr double kVertexQuantizationM = 0.002; // 2 mm welded XYZ matching.
    constexpr double kMaximumContinuousStepM = 0.030;
    constexpr double kPlateauToleranceM = 0.00002; // 0.02 mm.
    constexpr double kMaximumDetailedPuddleDepthM = 0.032;

    struct PointKey
    {
        std::int64_t x = 0;
        std::int64_t z = 0;
        bool operator==(const PointKey& other) const
        { return x == other.x && z == other.z; }
        bool operator<(const PointKey& other) const
        { return x != other.x ? x < other.x : z < other.z; }
    };
    struct EdgeKey
    {
        PointKey a{};
        PointKey b{};
        bool operator==(const EdgeKey& other) const
        { return a == other.a && b == other.b; }
    };
    struct EdgeKeyHash
    {
        std::size_t operator()(const EdgeKey& key) const
        {
            std::size_t seed = std::hash<std::int64_t>{}(key.a.x);
            seed ^= std::hash<std::int64_t>{}(key.a.z) + 0x9e3779b9u
                + (seed << 6u) + (seed >> 2u);
            seed ^= std::hash<std::int64_t>{}(key.b.x) + 0x9e3779b9u
                + (seed << 6u) + (seed >> 2u);
            seed ^= std::hash<std::int64_t>{}(key.b.z) + 0x9e3779b9u
                + (seed << 6u) + (seed >> 2u);
            return seed;
        }
    };
    struct VertexKey
    {
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t z = 0;
        bool operator==(const VertexKey& other) const
        { return x == other.x && y == other.y && z == other.z; }
    };
    struct VertexKeyHash
    {
        std::size_t operator()(const VertexKey& key) const
        {
            std::size_t seed = std::hash<std::int64_t>{}(key.x);
            seed ^= std::hash<std::int64_t>{}(key.y) + 0x9e3779b9u
                + (seed << 6u) + (seed >> 2u);
            seed ^= std::hash<std::int64_t>{}(key.z) + 0x9e3779b9u
                + (seed << 6u) + (seed >> 2u);
            return seed;
        }
    };
    struct EdgeUse
    {
        std::int32_t triangle = -1;
        std::uint8_t edge = 0u;
        double yA = 0.0;
        double yB = 0.0;
        std::int32_t vertexA = -1;
        std::int32_t vertexB = -1;
    };
    struct Vertex
    {
        heritage::math::DVec3 position{ 0.0, 0.0, 0.0 };
        std::vector<std::int32_t> neighbours;
        double ownAreaM2 = 0.0;
        bool openBoundary = false;
    };
    struct Node
    {
        PrebakedTriangle triangle{};
        std::array<std::int32_t, 3> vertices{{ -1, -1, -1 }};
    };

    const auto quantizeEdge = [](double value) {
        return static_cast<std::int64_t>(std::llround(value / kEdgeQuantizationM));
    };
    const auto quantizeVertex = [](double value) {
        return static_cast<std::int64_t>(std::llround(value / kVertexQuantizationM));
    };
    const auto makeEdgeKey = [&](const heritage::math::DVec3& p0,
                                 const heritage::math::DVec3& p1,
                                 double& yA, double& yB,
                                 bool& swapped) {
        PointKey a{ quantizeEdge(p0.x), quantizeEdge(p0.z) };
        PointKey b{ quantizeEdge(p1.x), quantizeEdge(p1.z) };
        yA = p0.y;
        yB = p1.y;
        swapped = false;
        if (b < a)
        {
            std::swap(a, b);
            std::swap(yA, yB);
            swapped = true;
        }
        return EdgeKey{ a, b };
    };

    std::vector<Vertex> vertices;
    vertices.reserve(localTriangles.size());
    std::unordered_map<VertexKey, std::int32_t, VertexKeyHash> vertexLookup;
    vertexLookup.reserve(localTriangles.size() * 2u);
    auto vertexIndexFor = [&](const heritage::math::DVec3& p) {
        const VertexKey key{ quantizeVertex(p.x), quantizeVertex(p.y),
            quantizeVertex(p.z) };
        const auto found = vertexLookup.find(key);
        if (found != vertexLookup.end())
            return found->second;
        const std::int32_t index = static_cast<std::int32_t>(vertices.size());
        vertices.push_back(Vertex{ p, {}, 0.0, false });
        vertexLookup.emplace(key, index);
        return index;
    };
    auto addGraphEdge = [&](std::int32_t a, std::int32_t b) {
        if (a < 0 || b < 0 || a == b)
            return;
        auto& na = vertices[static_cast<std::size_t>(a)].neighbours;
        auto& nb = vertices[static_cast<std::size_t>(b)].neighbours;
        if (std::find(na.begin(), na.end(), b) == na.end())
            na.push_back(b);
        if (std::find(nb.begin(), nb.end(), a) == nb.end())
            nb.push_back(a);
    };

    std::vector<Node> nodes;
    nodes.reserve(localTriangles.size());
    std::unordered_map<EdgeKey, std::vector<EdgeUse>, EdgeKeyHash> upwardEdges;
    upwardEdges.reserve(localTriangles.size() * 2u);
    std::unordered_set<EdgeKey, EdgeKeyHash> blockingEdges;
    blockingEdges.reserve(localTriangles.size());

    // Steep/vertical collision faces are projected to X/Z only as explicit
    // barriers. They prevent an otherwise unpaired road boundary from becoming
    // an artificial drain through a curb or wall.
    for (const StaticSceneTriangle& source : localTriangles)
    {
        const heritage::math::DVec3 p[3]{
            { globalOrigin.x + source.a.x, globalOrigin.y + source.a.y,
              globalOrigin.z + source.a.z },
            { globalOrigin.x + source.b.x, globalOrigin.y + source.b.y,
              globalOrigin.z + source.b.z },
            { globalOrigin.x + source.c.x, globalOrigin.y + source.c.y,
              globalOrigin.z + source.c.z }
        };
        if (source.normal.y >= static_cast<float>(m_description.minimumUpwardNormal))
            continue;
        for (int edge = 0; edge < 3; ++edge)
        {
            const auto& p0 = p[edge];
            const auto& p1 = p[(edge + 1) % 3];
            if (std::hypot(p1.x - p0.x, p1.z - p0.z) < 0.002)
                continue;
            double yA = 0.0, yB = 0.0;
            bool swapped = false;
            blockingEdges.insert(makeEdgeKey(p0, p1, yA, yB, swapped));
        }
    }

    for (const StaticSceneTriangle& source : localTriangles)
    {
        if (!std::isfinite(source.normal.y)
            || source.normal.y < static_cast<float>(m_description.minimumUpwardNormal))
        {
            continue;
        }

        Node node;
        node.triangle.a = { globalOrigin.x + source.a.x,
            globalOrigin.y + source.a.y, globalOrigin.z + source.a.z };
        node.triangle.b = { globalOrigin.x + source.b.x,
            globalOrigin.y + source.b.y, globalOrigin.z + source.b.z };
        node.triangle.c = { globalOrigin.x + source.c.x,
            globalOrigin.y + source.c.y, globalOrigin.z + source.c.z };
        node.triangle.normal = source.normal;
        node.triangle.depressionStorageM = std::max(
            source.surfaceProperties.hydrology.depressionStorageMm / 1000.0,
            0.0001);

        const double denominator =
            (node.triangle.b.z - node.triangle.c.z)
                * (node.triangle.a.x - node.triangle.c.x)
            + (node.triangle.c.x - node.triangle.b.x)
                * (node.triangle.a.z - node.triangle.c.z);
        if (!std::isfinite(denominator) || std::abs(denominator) <= 1.0e-12)
            continue;

        const heritage::math::DVec3 p[3]{ node.triangle.a, node.triangle.b,
            node.triangle.c };
        for (int corner = 0; corner < 3; ++corner)
            node.vertices[static_cast<std::size_t>(corner)] = vertexIndexFor(p[corner]);

        const double ax = node.triangle.b.x - node.triangle.a.x;
        const double az = node.triangle.b.z - node.triangle.a.z;
        const double bx = node.triangle.c.x - node.triangle.a.x;
        const double bz = node.triangle.c.z - node.triangle.a.z;
        const double projectedAreaM2 = std::max(
            0.5 * std::abs(ax * bz - az * bx), 1.0e-6);
        for (const std::int32_t vertex : node.vertices)
            vertices[static_cast<std::size_t>(vertex)].ownAreaM2 += projectedAreaM2 / 3.0;

        // Every triangle edge is a legal path along the authored surface. This
        // is what lets a valley/gutter carry water along its shared low edge.
        addGraphEdge(node.vertices[0], node.vertices[1]);
        addGraphEdge(node.vertices[1], node.vertices[2]);
        addGraphEdge(node.vertices[2], node.vertices[0]);

        const std::int32_t nodeIndex = static_cast<std::int32_t>(nodes.size());
        for (int edge = 0; edge < 3; ++edge)
        {
            double yA = 0.0, yB = 0.0;
            bool swapped = false;
            const EdgeKey key = makeEdgeKey(
                p[edge], p[(edge + 1) % 3], yA, yB, swapped);
            std::int32_t vertexA = node.vertices[static_cast<std::size_t>(edge)];
            std::int32_t vertexB = node.vertices[static_cast<std::size_t>((edge + 1) % 3)];
            if (swapped)
                std::swap(vertexA, vertexB);
            upwardEdges[key].push_back({ nodeIndex, static_cast<std::uint8_t>(edge),
                yA, yB, vertexA, vertexB });
        }
        nodes.push_back(std::move(node));
    }
    if (nodes.empty() || vertices.empty())
        return;

    // Weld separately-authored but height-compatible pavement seams and mark
    // only genuine unblocked exterior edges as priority-flood outlets.
    for (const auto& [key, uses] : upwardEdges)
    {
        std::vector<bool> compatible(uses.size(), false);
        for (std::size_t i = 0; i < uses.size(); ++i)
        {
            for (std::size_t j = i + 1u; j < uses.size(); ++j)
            {
                if (uses[i].triangle == uses[j].triangle)
                    continue;
                const double mismatchM = std::max(
                    std::abs(uses[i].yA - uses[j].yA),
                    std::abs(uses[i].yB - uses[j].yB));
                if (mismatchM > kMaximumContinuousStepM)
                    continue;
                compatible[i] = true;
                compatible[j] = true;
                addGraphEdge(uses[i].vertexA, uses[j].vertexA);
                addGraphEdge(uses[i].vertexB, uses[j].vertexB);
            }
        }

        const bool explicitBarrier = blockingEdges.find(key) != blockingEdges.end();
        for (std::size_t i = 0; i < uses.size(); ++i)
        {
            if (compatible[i] || explicitBarrier)
                continue;
            bool incompatiblePeer = false;
            for (std::size_t j = 0; j < uses.size(); ++j)
            {
                if (i != j && uses[i].triangle != uses[j].triangle)
                {
                    incompatiblePeer = true;
                    break;
                }
            }
            if (incompatiblePeer)
                continue; // height step/curb, not a free drain.
            if (uses[i].vertexA >= 0)
                vertices[static_cast<std::size_t>(uses[i].vertexA)].openBoundary = true;
            if (uses[i].vertexB >= 0)
                vertices[static_cast<std::size_t>(uses[i].vertexB)].openBoundary = true;
        }
    }

    // Minimum escape elevation (priority flood). A normal draining slope gets
    // escape==surface elevation and therefore zero puddle capacity. A true
    // depression receives the lowest saddle level needed to reach any unblocked
    // exterior boundary. Curbs remain closed because they were never seeded.
    const double infinity = (std::numeric_limits<double>::infinity)();
    std::vector<double> escapeElevationM(vertices.size(), infinity);
    using FloodNode = std::pair<double, std::int32_t>;
    std::priority_queue<FloodNode, std::vector<FloodNode>, std::greater<FloodNode>> flood;
    for (std::size_t i = 0; i < vertices.size(); ++i)
    {
        if (!vertices[i].openBoundary)
            continue;
        escapeElevationM[i] = vertices[i].position.y;
        flood.push({ escapeElevationM[i], static_cast<std::int32_t>(i) });
    }
    while (!flood.empty())
    {
        const auto [levelM, index] = flood.top();
        flood.pop();
        if (index < 0)
            continue;
        const std::size_t i = static_cast<std::size_t>(index);
        if (i >= vertices.size() || levelM > escapeElevationM[i] + 1.0e-12)
            continue;
        for (const std::int32_t neighbourIndex : vertices[i].neighbours)
        {
            if (neighbourIndex < 0)
                continue;
            const std::size_t neighbour = static_cast<std::size_t>(neighbourIndex);
            const double candidateM = std::max(levelM, vertices[neighbour].position.y);
            if (candidateM + 1.0e-12 < escapeElevationM[neighbour])
            {
                escapeElevationM[neighbour] = candidateM;
                flood.push({ candidateM, neighbourIndex });
            }
        }
    }

    // Multi-flow downhill routing. A one-edge D8-like choice is still too
    // coarse on a triangulated road: if longitudinal grade is steeper than the
    // crossfall it can ignore the smaller crossfall completely and never feed a
    // gutter. Instead distribute contributing area across every genuinely lower
    // mesh neighbour in proportion to downhill grade. This is a compact MFD
    // (multiple-flow-direction) solve on the irregular authored mesh.
    struct FlowTarget
    {
        std::int32_t vertex = -1;
        double fraction = 0.0;
    };
    std::vector<std::vector<FlowTarget>> outgoing(vertices.size());
    std::vector<std::int32_t> primaryDownstream(vertices.size(), -1);
    std::vector<float> flowX(vertices.size(), 0.0f);
    std::vector<float> flowZ(vertices.size(), 0.0f);
    for (std::size_t i = 0; i < vertices.size(); ++i)
    {
        const auto& source = vertices[i].position;
        double totalWeight = 0.0;
        double strongestWeight = 0.0;
        std::int32_t strongestTarget = -1;
        for (const std::int32_t neighbourIndex : vertices[i].neighbours)
        {
            if (neighbourIndex < 0)
                continue;
            const auto& target = vertices[static_cast<std::size_t>(neighbourIndex)].position;
            const double dropM = source.y - target.y;
            if (dropM <= kPlateauToleranceM)
                continue;
            const double dx = target.x - source.x;
            const double dz = target.z - source.z;
            const double horizontalM = std::hypot(dx, dz);
            if (horizontalM <= 1.0e-6)
                continue;
            const double grade = dropM / horizontalM;
            // A near-linear MFD exponent preserves the physical ratio between
            // longitudinal grade and road crossfall instead of collapsing to
            // whichever edge happens to be steepest in the triangulation.
            const double weight = std::pow(std::max(grade, 0.0), 1.10);
            if (weight <= 0.0)
                continue;
            outgoing[i].push_back({ neighbourIndex, weight });
            totalWeight += weight;
            if (weight > strongestWeight + 1.0e-15
                || (std::abs(weight - strongestWeight) <= 1.0e-15
                    && (strongestTarget < 0 || neighbourIndex < strongestTarget)))
            {
                strongestWeight = weight;
                strongestTarget = neighbourIndex;
            }
        }

        if (totalWeight <= 0.0)
            continue;
        primaryDownstream[i] = strongestTarget;
        double vx = 0.0;
        double vz = 0.0;
        for (FlowTarget& targetInfo : outgoing[i])
        {
            targetInfo.fraction /= totalWeight;
            const auto& target = vertices[static_cast<std::size_t>(targetInfo.vertex)].position;
            const double dx = target.x - source.x;
            const double dz = target.z - source.z;
            const double length = std::hypot(dx, dz);
            if (length > 1.0e-6)
            {
                vx += targetInfo.fraction * dx / length;
                vz += targetInfo.fraction * dz / length;
            }
        }
        const double vectorLength = std::hypot(vx, vz);
        if (vectorLength > 1.0e-8)
        {
            flowX[i] = static_cast<float>(vx / vectorLength);
            flowZ[i] = static_cast<float>(vz / vectorLength);
        }
    }

    // The strongest strict-downhill branch is used only to identify the local
    // sink floor for the bounded 32 mm detailed-puddle ceiling. All runoff area
    // itself uses the complete MFD graph below.
    std::vector<std::int32_t> roots(vertices.size(), -1);
    for (std::size_t start = 0; start < vertices.size(); ++start)
    {
        if (roots[start] >= 0)
            continue;
        std::size_t current = start;
        while (primaryDownstream[current] >= 0 && roots[current] < 0)
            current = static_cast<std::size_t>(primaryDownstream[current]);
        const std::int32_t root = roots[current] >= 0
            ? roots[current] : static_cast<std::int32_t>(current);
        current = start;
        while (roots[current] < 0)
        {
            roots[current] = root;
            if (primaryDownstream[current] < 0)
                break;
            current = static_cast<std::size_t>(primaryDownstream[current]);
        }
    }

    // Contributing-area accumulation over the complete multiple-flow DAG.
    // Every outgoing fraction is conserved, so a gutter receives the part of
    // rainfall driven by road crossfall while the longitudinal component keeps
    // moving downstream. This is the static runoff route that the texture needs.
    std::vector<double> accumulatedAreaM2(vertices.size(), 0.0);
    std::vector<std::uint32_t> incoming(vertices.size(), 0u);
    for (std::size_t i = 0; i < vertices.size(); ++i)
    {
        accumulatedAreaM2[i] = vertices[i].ownAreaM2;
        for (const FlowTarget& target : outgoing[i])
            ++incoming[static_cast<std::size_t>(target.vertex)];
    }
    std::deque<std::size_t> accumulationQueue;
    for (std::size_t i = 0; i < incoming.size(); ++i)
    {
        if (incoming[i] == 0u)
            accumulationQueue.push_back(i);
    }
    while (!accumulationQueue.empty())
    {
        const std::size_t i = accumulationQueue.front();
        accumulationQueue.pop_front();
        for (const FlowTarget& targetInfo : outgoing[i])
        {
            const std::size_t target = static_cast<std::size_t>(targetInfo.vertex);
            accumulatedAreaM2[target] += accumulatedAreaM2[i] * targetInfo.fraction;
            if (incoming[target] > 0u && --incoming[target] == 0u)
                accumulationQueue.push_back(target);
        }
    }

    std::vector<double> detailedSpillM(vertices.size(), 0.0);
    for (std::size_t i = 0; i < vertices.size(); ++i)
    {
        const std::int32_t rootIndex = roots[i] >= 0
            ? roots[i] : static_cast<std::int32_t>(i);
        const double rootElevationM = vertices[static_cast<std::size_t>(rootIndex)].position.y;
        const double detailedCeilingM = rootElevationM + kMaximumDetailedPuddleDepthM;
        // A completely closed component has no flood seed. In that case the
        // detailed model intentionally represents only the lowest 32 mm band.
        detailedSpillM[i] = std::isfinite(escapeElevationM[i])
            ? std::min(escapeElevationM[i], detailedCeilingM)
            : detailedCeilingM;
    }

    m_prebakedTriangles.reserve(nodes.size());
    for (Node& node : nodes)
    {
        const std::size_t va = static_cast<std::size_t>(node.vertices[0]);
        const std::size_t vb = static_cast<std::size_t>(node.vertices[1]);
        const std::size_t vc = static_cast<std::size_t>(node.vertices[2]);
        node.triangle.spillElevationA = detailedSpillM[va];
        node.triangle.spillElevationB = detailedSpillM[vb];
        node.triangle.spillElevationC = detailedSpillM[vc];
        node.triangle.flowAX = flowX[va];
        node.triangle.flowAZ = flowZ[va];
        node.triangle.flowBX = flowX[vb];
        node.triangle.flowBZ = flowZ[vb];
        node.triangle.flowCX = flowX[vc];
        node.triangle.flowCZ = flowZ[vc];
        node.triangle.runoffAccumulationAM2 = static_cast<float>(
            std::clamp(accumulatedAreaM2[va], 0.0, 65535.0));
        node.triangle.runoffAccumulationBM2 = static_cast<float>(
            std::clamp(accumulatedAreaM2[vb], 0.0, 65535.0));
        node.triangle.runoffAccumulationCM2 = static_cast<float>(
            std::clamp(accumulatedAreaM2[vc], 0.0, 65535.0));
        m_prebakedTriangles.push_back(std::move(node.triangle));
    }
    rebuildPrebakedTriangleTileLookup();
}

} // namespace heritage::physics::water
