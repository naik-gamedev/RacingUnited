#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../Core/Math/Math.hpp"

namespace heritage::physics {

struct StaticTriangleBvhPrimitive
{
    heritage::math::Vec3 minimum{};
    heritage::math::Vec3 maximum{};
    heritage::math::Vec3 centroid{};
    std::uint32_t triangleIndex = 0;
};

// Deterministic median-split BVH over immutable creator triangle bounds.
// Leaves retain original triangle indices; callers own geometry and narrow
// phase. Rebuilding occurs only when a static scene is replaced.
class StaticTriangleBvh
{
public:
    void clear();
    void build(std::vector<StaticTriangleBvhPrimitive> primitives);

    bool empty() const { return m_nodes.empty(); }
    std::size_t nodeCount() const { return m_nodes.size(); }
    std::size_t leafCount() const { return m_leafCount; }
    std::size_t maximumDepth() const { return m_maximumDepth; }

    bool bounds(
        heritage::math::Vec3& minimum,
        heritage::math::Vec3& maximum) const;

    // Appends original triangle indices whose individual bounds overlap the
    // query. Results are sorted by triangle index for stable tie-breaking.
    void queryAabb(
        const heritage::math::Vec3& minimum,
        const heritage::math::Vec3& maximum,
        std::vector<std::uint32_t>& triangleIndices,
        std::size_t& nodeTestCount) const;

private:
    struct Node
    {
        heritage::math::Vec3 minimum{};
        heritage::math::Vec3 maximum{};
        std::uint32_t firstPrimitive = 0;
        std::uint32_t primitiveCount = 0;
        std::uint32_t leftChild = 0;
        std::uint32_t rightChild = 0;
        bool leaf = false;
    };

    std::uint32_t buildRange(
        std::uint32_t first,
        std::uint32_t count,
        std::size_t depth);

    std::vector<StaticTriangleBvhPrimitive> m_primitives;
    std::vector<Node> m_nodes;
    std::size_t m_leafCount = 0;
    std::size_t m_maximumDepth = 0;
};

} // namespace heritage::physics
