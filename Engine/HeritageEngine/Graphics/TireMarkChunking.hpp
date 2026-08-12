#pragma once

#include <cmath>
#include <cstdint>

#include "../Core/Math/Math.hpp"

// TIRE16K tire-mark presentation chunking.
//
// Chunks are deliberately an invisible storage/batching mechanism only. They
// never quantize the authored skid path, choose LOD, alter opacity, or decide
// where a mark is placed. A segment is first generated in authoritative FP64
// world space, then converted once to FP32 relative to the centre of a 100 m
// presentation chunk for the GPU cache. Reconstructing origin + local therefore
// preserves a continuous skid across chunk boundaries without the cell-centred
// visual reconstruction that previously made loose marbles look like a grid.
namespace heritage::graphics::tiremarks {

inline constexpr double kChunkSizeM = 100.0;
inline constexpr double kChunkHalfSizeM = kChunkSizeM * 0.5;
inline constexpr double kChunkHorizontalHalfDiagonalM = 70.71067811865476;

struct ChunkAddress
{
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    bool operator<(const ChunkAddress& other) const
    {
        if (x != other.x)
            return x < other.x;
        if (y != other.y)
            return y < other.y;
        return z < other.z;
    }
};

inline ChunkAddress chunkAddress(const heritage::math::DVec3& globalPosition)
{
    return {
        static_cast<std::int64_t>(std::floor(globalPosition.x / kChunkSizeM)),
        // Keep common road elevations around y=0 in one vertical layer instead
        // of splitting +/- centimetre road noise across two GPU chunks.
        static_cast<std::int64_t>(std::floor(
            (globalPosition.y + kChunkHalfSizeM) / kChunkSizeM)),
        static_cast<std::int64_t>(std::floor(globalPosition.z / kChunkSizeM))
    };
}

inline heritage::math::DVec3 chunkOrigin(const ChunkAddress& address)
{
    return {
        (static_cast<double>(address.x) + 0.5) * kChunkSizeM,
        static_cast<double>(address.y) * kChunkSizeM,
        (static_cast<double>(address.z) + 0.5) * kChunkSizeM
    };
}

inline heritage::math::Vec3 localFp32(
    const heritage::math::DVec3& globalPosition,
    const heritage::math::DVec3& origin)
{
    return {
        static_cast<float>(globalPosition.x - origin.x),
        static_cast<float>(globalPosition.y - origin.y),
        static_cast<float>(globalPosition.z - origin.z)
    };
}

inline heritage::math::DVec3 reconstructGlobal(
    const heritage::math::DVec3& origin,
    const heritage::math::Vec3& localPosition)
{
    return {
        origin.x + static_cast<double>(localPosition.x),
        origin.y + static_cast<double>(localPosition.y),
        origin.z + static_cast<double>(localPosition.z)
    };
}

} // namespace heritage::graphics::tiremarks
