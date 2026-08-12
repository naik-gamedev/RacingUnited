#include "StaticTriangleBvh.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace heritage::physics {
namespace {

constexpr std::uint32_t kLeafPrimitiveCount = 8;
constexpr float kCentroidExtentEpsilon = 1.0e-6f;

float component(const heritage::math::Vec3& value, int axis)
{
    if (axis == 0) return value.x;
    if (axis == 1) return value.y;
    return value.z;
}

bool overlaps(
    const heritage::math::Vec3& minimumA,
    const heritage::math::Vec3& maximumA,
    const heritage::math::Vec3& minimumB,
    const heritage::math::Vec3& maximumB)
{
    return minimumA.x <= maximumB.x && maximumA.x >= minimumB.x
        && minimumA.y <= maximumB.y && maximumA.y >= minimumB.y
        && minimumA.z <= maximumB.z && maximumA.z >= minimumB.z;
}

} // namespace

void StaticTriangleBvh::clear()
{
    m_primitives.clear();
    m_nodes.clear();
    m_leafCount = 0;
    m_maximumDepth = 0;
}

void StaticTriangleBvh::build(
    std::vector<StaticTriangleBvhPrimitive> primitives)
{
    clear();
    if (primitives.empty())
        return;

    m_primitives = std::move(primitives);
    m_nodes.reserve(m_primitives.size() * 2u);
    buildRange(0u, static_cast<std::uint32_t>(m_primitives.size()), 1u);
}

bool StaticTriangleBvh::bounds(
    heritage::math::Vec3& minimum,
    heritage::math::Vec3& maximum) const
{
    if (m_nodes.empty())
        return false;
    minimum = m_nodes.front().minimum;
    maximum = m_nodes.front().maximum;
    return true;
}

std::uint32_t StaticTriangleBvh::buildRange(
    std::uint32_t first,
    std::uint32_t count,
    std::size_t depth)
{
    const float high = (std::numeric_limits<float>::max)();
    const float low = (std::numeric_limits<float>::lowest)();
    heritage::math::Vec3 minimum{ high, high, high };
    heritage::math::Vec3 maximum{ low, low, low };
    heritage::math::Vec3 centroidMinimum{ high, high, high };
    heritage::math::Vec3 centroidMaximum{ low, low, low };
    for (std::uint32_t offset = 0; offset < count; ++offset)
    {
        const StaticTriangleBvhPrimitive& primitive =
            m_primitives[first + offset];
        minimum.x = std::min(minimum.x, primitive.minimum.x);
        minimum.y = std::min(minimum.y, primitive.minimum.y);
        minimum.z = std::min(minimum.z, primitive.minimum.z);
        maximum.x = std::max(maximum.x, primitive.maximum.x);
        maximum.y = std::max(maximum.y, primitive.maximum.y);
        maximum.z = std::max(maximum.z, primitive.maximum.z);
        centroidMinimum.x = std::min(
            centroidMinimum.x, primitive.centroid.x);
        centroidMinimum.y = std::min(
            centroidMinimum.y, primitive.centroid.y);
        centroidMinimum.z = std::min(
            centroidMinimum.z, primitive.centroid.z);
        centroidMaximum.x = std::max(
            centroidMaximum.x, primitive.centroid.x);
        centroidMaximum.y = std::max(
            centroidMaximum.y, primitive.centroid.y);
        centroidMaximum.z = std::max(
            centroidMaximum.z, primitive.centroid.z);
    }

    const std::uint32_t nodeIndex = static_cast<std::uint32_t>(m_nodes.size());
    m_nodes.push_back({});
    m_nodes[nodeIndex].minimum = minimum;
    m_nodes[nodeIndex].maximum = maximum;
    m_maximumDepth = std::max(m_maximumDepth, depth);

    const heritage::math::Vec3 centroidExtent{
        centroidMaximum.x - centroidMinimum.x,
        centroidMaximum.y - centroidMinimum.y,
        centroidMaximum.z - centroidMinimum.z
    };
    int splitAxis = 0;
    if (centroidExtent.y > centroidExtent.x)
        splitAxis = 1;
    if (component(centroidExtent, 2) > component(centroidExtent, splitAxis))
        splitAxis = 2;

    if (count <= kLeafPrimitiveCount
        || component(centroidExtent, splitAxis) <= kCentroidExtentEpsilon)
    {
        Node& node = m_nodes[nodeIndex];
        node.firstPrimitive = first;
        node.primitiveCount = count;
        node.leaf = true;
        ++m_leafCount;
        return nodeIndex;
    }

    std::stable_sort(
        m_primitives.begin() + first,
        m_primitives.begin() + first + count,
        [splitAxis](
            const StaticTriangleBvhPrimitive& left,
            const StaticTriangleBvhPrimitive& right) {
            const float leftValue = component(left.centroid, splitAxis);
            const float rightValue = component(right.centroid, splitAxis);
            if (leftValue != rightValue)
                return leftValue < rightValue;
            return left.triangleIndex < right.triangleIndex;
        });

    const std::uint32_t leftCount = count / 2u;
    const std::uint32_t rightCount = count - leftCount;
    const std::uint32_t leftChild = buildRange(first, leftCount, depth + 1u);
    const std::uint32_t rightChild = buildRange(
        first + leftCount, rightCount, depth + 1u);
    m_nodes[nodeIndex].leftChild = leftChild;
    m_nodes[nodeIndex].rightChild = rightChild;
    return nodeIndex;
}

void StaticTriangleBvh::queryAabb(
    const heritage::math::Vec3& minimum,
    const heritage::math::Vec3& maximum,
    std::vector<std::uint32_t>& triangleIndices,
    std::size_t& nodeTestCount) const
{
    triangleIndices.clear();
    nodeTestCount = 0;
    if (m_nodes.empty())
        return;

    std::vector<std::uint32_t> stack;
    stack.reserve(m_maximumDepth * 2u + 1u);
    stack.push_back(0u);
    while (!stack.empty())
    {
        const std::uint32_t nodeIndex = stack.back();
        stack.pop_back();
        const Node& node = m_nodes[nodeIndex];
        ++nodeTestCount;
        if (!overlaps(minimum, maximum, node.minimum, node.maximum))
            continue;

        if (!node.leaf)
        {
            // Right is pushed first so the deterministic left branch is
            // visited first. Final indices are sorted independently below.
            stack.push_back(node.rightChild);
            stack.push_back(node.leftChild);
            continue;
        }

        for (std::uint32_t offset = 0;
             offset < node.primitiveCount;
             ++offset)
        {
            const StaticTriangleBvhPrimitive& primitive =
                m_primitives[node.firstPrimitive + offset];
            if (overlaps(
                    minimum,
                    maximum,
                    primitive.minimum,
                    primitive.maximum))
            {
                triangleIndices.push_back(primitive.triangleIndex);
            }
        }
    }

    std::sort(triangleIndices.begin(), triangleIndices.end());
}

} // namespace heritage::physics
