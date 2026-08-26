#include "SurfaceHydrology.hpp"

#include "../../../Core/Jobs/JobSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace heritage::physics::water {
namespace {

std::uint64_t prebakedTileKey(std::int32_t x, std::int32_t z)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32u)
        | static_cast<std::uint32_t>(z);
}

} // namespace

bool SurfaceHydrology::rasterPrebakedPuddleResponseTileUncached(
    std::int32_t tileX,
    std::int32_t tileZ,
    std::uint32_t outputResolution,
    std::vector<std::uint8_t>& encodedCapacityAndFlow) const
{
    encodedCapacityAndFlow.clear();
    if (outputResolution == 0u || outputResolution > 1024u
        || m_prebakedTriangles.empty())
    {
        return false;
    }
    const PrebakedTriangleTileSpan* bucket = prebakedTriangleTileSpan(tileX, tileZ);
    if (!bucket || bucket->count == 0u)
        return false;

    // LIVETRACK15: capacity is free-water basin depth only. Microscopic
    // material film is a separate runtime wetness state, so the 4-bit topology
    // can spend its shallow codes on real sub-millimetre depressions instead of
    // reserving 0.1 mm everywhere. This is what lets the bake keep many more
    // small authored puddles without making ordinary slopes into water sheets.
    // LIVETRACK21I: literal 4-bit ultra-shallow standing-depth ladder selected by the
    // user. Every baked standing-depth ceiling texel is exactly one of these depths in mm:
    // 0.00, 0.01, 0.05, then 0.05-mm steps through 0.70 mm. There is no
    // dithering and no alternate bake curve. Deeper geometric basins saturate
    // at the 0.70-mm presentation code.
    static constexpr std::array<double, 16> kWaterLevelsM{{
        0.0, 0.00001, 0.00005, 0.00010, 0.00015, 0.00020, 0.00025, 0.00030,
        0.00035, 0.00040, 0.00045, 0.00050, 0.00055, 0.00060, 0.00065, 0.00070
    }};
    static_assert(kWaterLevelsM[0] == 0.0 && kWaterLevelsM[1] == 0.00001
        && kWaterLevelsM[2] == 0.00005 && kWaterLevelsM[15] == 0.00070);
    const auto encodeCapacity = [&](double capacityM) -> std::uint8_t {
        // The regular part of the ladder (codes 2..15) is arithmetic, so the
        // bake does not need a binary search. Midpoints preserve nearest-level
        // quantisation: 0/0.01 midpoint=0.005 mm; 0.01/0.05 midpoint=0.03 mm.
        capacityM = std::clamp(capacityM, 0.0, 0.00070);
        std::uint32_t code = 0u;
        if (capacityM >= 0.000005)
        {
            if (capacityM < 0.000030)
                code = 1u;
            else
                code = static_cast<std::uint32_t>(std::clamp(
                    static_cast<int>(std::floor(capacityM / 0.00005 + 0.5)) + 1,
                    2, 15));
        }
        return static_cast<std::uint8_t>(code * 17u);
    };
    const auto encodeRunoffAccumulation = [](float areaM2) -> std::uint8_t {
        // 4-bit logarithmic upstream catchment ladder. Zero means no convergent
        // upstream area; the upper codes represent progressively larger gutters,
        // channels and drainage routes without tying the result to mesh density.
        static constexpr std::array<double, 16> kRunoffAreaLevelsM2{{
            0.0, 0.25, 0.50, 1.0, 2.0, 4.0, 8.0, 16.0,
            32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 2048.0, 4096.0
        }};
        const double area = std::max(static_cast<double>(areaM2), 0.0);
        if (area <= 0.125)
            return 0u;
        auto found = std::lower_bound(
            kRunoffAreaLevelsM2.begin() + 1, kRunoffAreaLevelsM2.end(), area);
        std::size_t code = found == kRunoffAreaLevelsM2.end()
            ? 15u : static_cast<std::size_t>(found - kRunoffAreaLevelsM2.begin());
        if (found != kRunoffAreaLevelsM2.end() && found != kRunoffAreaLevelsM2.begin() + 1)
        {
            const double upperError = std::abs(*found - area);
            const double lowerError = std::abs(*(found - 1) - area);
            if (lowerError <= upperError)
                --code;
        }
        return static_cast<std::uint8_t>(std::min<std::size_t>(code, 15u) * 17u);
    };
    const auto encodeFlowAngle = [](float x, float z) -> std::uint8_t {
        const double lengthSquared = static_cast<double>(x) * x
            + static_cast<double>(z) * z;
        if (lengthSquared < 1.0e-8)
            return 0u;
        constexpr double kPi = 3.14159265358979323846;
        const double angle = std::atan2(static_cast<double>(z),
            static_cast<double>(x));
        const double normalized = (angle + kPi) / (2.0 * kPi);
        const int code = std::clamp(
            1 + static_cast<int>(std::lround(normalized * 14.0)), 1, 15);
        return static_cast<std::uint8_t>(code * 17);
    };

    const std::size_t texelCount = static_cast<std::size_t>(outputResolution)
        * static_cast<std::size_t>(outputResolution);
    encodedCapacityAndFlow.assign(texelCount * 3u, 0u);
    // Unsupported/non-basin texels carry zero free-water capacity. Ordinary
    // 0.1 mm rain film is rendered from the separate wetting memory, so a
    // missing topology sample can never invent or erase a standing puddle.

    std::vector<double> highestSurfaceM(texelCount,
        -(std::numeric_limits<double>::infinity)());
    constexpr double kTileWorldSizeM = 10.0;
    const double texelSizeM = kTileWorldSizeM
        / static_cast<double>(outputResolution);
    const double tileOriginX = static_cast<double>(tileX) * kTileWorldSizeM;
    const double tileOriginZ = static_cast<double>(tileZ) * kTileWorldSizeM;

    for (std::uint32_t bucketIndex = 0u; bucketIndex < bucket->count; ++bucketIndex)
    {
        const std::uint64_t flatIndex = bucket->firstIndex + bucketIndex;
        if (flatIndex >= m_prebakedTriangleTileIndices.size())
            continue;
        const std::int32_t triangleIndex = m_prebakedTriangleTileIndices[
            static_cast<std::size_t>(flatIndex)];
        if (triangleIndex < 0
            || static_cast<std::size_t>(triangleIndex) >= m_prebakedTriangles.size())
        {
            continue;
        }
        const PrebakedTriangle& triangle = m_prebakedTriangles[
            static_cast<std::size_t>(triangleIndex)];
        const double minimumX = std::min({ triangle.a.x, triangle.b.x, triangle.c.x });
        const double maximumX = std::max({ triangle.a.x, triangle.b.x, triangle.c.x });
        const double minimumZ = std::min({ triangle.a.z, triangle.b.z, triangle.c.z });
        const double maximumZ = std::max({ triangle.a.z, triangle.b.z, triangle.c.z });
        int firstX = static_cast<int>(std::ceil(
            (minimumX - tileOriginX) / texelSizeM - 0.5));
        int lastX = static_cast<int>(std::floor(
            (maximumX - tileOriginX) / texelSizeM - 0.5));
        int firstZ = static_cast<int>(std::ceil(
            (minimumZ - tileOriginZ) / texelSizeM - 0.5));
        int lastZ = static_cast<int>(std::floor(
            (maximumZ - tileOriginZ) / texelSizeM - 0.5));
        firstX = std::clamp(firstX, 0, static_cast<int>(outputResolution) - 1);
        lastX = std::clamp(lastX, 0, static_cast<int>(outputResolution) - 1);
        firstZ = std::clamp(firstZ, 0, static_cast<int>(outputResolution) - 1);
        lastZ = std::clamp(lastZ, 0, static_cast<int>(outputResolution) - 1);
        if (firstX > lastX || firstZ > lastZ)
            continue;

        const double denominator =
            (triangle.b.z - triangle.c.z) * (triangle.a.x - triangle.c.x)
            + (triangle.c.x - triangle.b.x) * (triangle.a.z - triangle.c.z);
        if (!std::isfinite(denominator) || std::abs(denominator) <= 1.0e-12)
            continue;
        for (int z = firstZ; z <= lastZ; ++z)
        {
            const double worldZ = tileOriginZ
                + (static_cast<double>(z) + 0.5) * texelSizeM;
            for (int x = firstX; x <= lastX; ++x)
            {
                const double worldX = tileOriginX
                    + (static_cast<double>(x) + 0.5) * texelSizeM;
                const double wa = ((triangle.b.z - triangle.c.z)
                        * (worldX - triangle.c.x)
                    + (triangle.c.x - triangle.b.x)
                        * (worldZ - triangle.c.z)) / denominator;
                const double wb = ((triangle.c.z - triangle.a.z)
                        * (worldX - triangle.c.x)
                    + (triangle.a.x - triangle.c.x)
                        * (worldZ - triangle.c.z)) / denominator;
                const double wc = 1.0 - wa - wb;
                constexpr double kInsideEpsilon = -1.0e-9;
                if (wa < kInsideEpsilon || wb < kInsideEpsilon || wc < kInsideEpsilon)
                    continue;
                const double surfaceElevationM = wa * triangle.a.y
                    + wb * triangle.b.y + wc * triangle.c.y;
                const std::size_t texel = static_cast<std::size_t>(z)
                    * outputResolution + static_cast<std::size_t>(x);
                // GPU Hydro intentionally owns one X/Z field. If stacked road
                // surfaces overlap, the highest authored collision triangle wins.
                if (surfaceElevationM + 1.0e-8 < highestSurfaceM[texel])
                    continue;
                highestSurfaceM[texel] = surfaceElevationM;
                // Free-water capacity is purely geometric: spill height minus
                // the authored surface. Material depressionStorageM belongs to
                // the porous/microscopic wet-film model and must not force every
                // road texel to look like a puddle.
                const double spillElevationM = wa * triangle.spillElevationA
                    + wb * triangle.spillElevationB + wc * triangle.spillElevationC;
                const double capacityM = std::clamp(
                    spillElevationM - surfaceElevationM, 0.0, 0.0320);

                // LIVETRACK20 reconstructs the static drainage network inside
                // each authored triangle. This keeps crown-to-gutter convergence
                // and along-gutter routing visible instead of painting one
                // triangle-wide direction/intensity value.
                double flowX = wa * triangle.flowAX + wb * triangle.flowBX
                    + wc * triangle.flowCX;
                double flowZ = wa * triangle.flowAZ + wb * triangle.flowBZ
                    + wc * triangle.flowCZ;
                const double flowLength = std::hypot(flowX, flowZ);
                std::uint8_t flowCode = 0u;
                double runoffAreaM2 = 0.0;
                if (flowLength > 1.0e-6)
                {
                    flowX /= flowLength;
                    flowZ /= flowLength;
                    flowCode = encodeFlowAngle(
                        static_cast<float>(flowX), static_cast<float>(flowZ));
                    runoffAreaM2 = wa * triangle.runoffAccumulationAM2
                        + wb * triangle.runoffAccumulationBM2
                        + wc * triangle.runoffAccumulationCM2;
                }
                else if (capacityM > 0.0000005)
                {
                    // Preserve contributing area at a genuine terminal basin
                    // minimum even though its downhill direction correctly
                    // becomes zero. A perfectly flat/open non-basin remains
                    // runoff=0, so no fake river is introduced.
                    runoffAreaM2 = wa * triangle.runoffAccumulationAM2
                        + wb * triangle.runoffAccumulationBM2
                        + wc * triangle.runoffAccumulationCM2;
                }
                encodedCapacityAndFlow[texel * 3u] = encodeRunoffAccumulation(
                    static_cast<float>(std::max(runoffAreaM2, 0.0)));
                encodedCapacityAndFlow[texel * 3u + 1u] = encodeCapacity(capacityM);
                encodedCapacityAndFlow[texel * 3u + 2u] = flowCode;
            }
        }
    }
    return true;
}

bool SurfaceHydrology::prebakedFarPuddleResponseTile(
    std::int32_t tileX,
    std::int32_t tileZ,
    std::vector<std::uint8_t>& encodedCapacityAndFlow) const
{
    encodedCapacityAndFlow.clear();
    if (m_prebakedFarTiles.empty() || m_prebakedFarPayload.empty())
        return false;
    const std::uint64_t key = prebakedTileKey(tileX, tileZ);
    const auto found = std::lower_bound(
        m_prebakedFarTiles.begin(), m_prebakedFarTiles.end(), key,
        [](const PrebakedFarTileIndex& entry, std::uint64_t value) {
            return entry.key < value;
        });
    if (found == m_prebakedFarTiles.end() || found->key != key)
        return false;
    const std::size_t offset = static_cast<std::size_t>(found->payloadOffset);
    const std::size_t bytes = static_cast<std::size_t>(found->payloadBytes);
    if (offset > m_prebakedFarPayload.size()
        || bytes > m_prebakedFarPayload.size() - offset)
    {
        return false;
    }
    constexpr std::size_t kFarTexels = 32u * 32u;
    constexpr std::size_t kFarBytes = kFarTexels * 3u;
    encodedCapacityAndFlow.resize(kFarBytes);
    std::size_t outputTexel = 0u;
    const auto appendCodes = [&](std::uint8_t capacityFlow, std::uint8_t runoffNibble,
                                 std::size_t count) {
        const std::uint8_t capacity = static_cast<std::uint8_t>(
            ((capacityFlow >> 4u) & 0x0fu) * 17u);
        const std::uint8_t flow = static_cast<std::uint8_t>((capacityFlow & 0x0fu) * 17u);
        const std::uint8_t runoff = static_cast<std::uint8_t>((runoffNibble & 0x0fu) * 17u);
        for (std::size_t i = 0; i < count; ++i)
        {
            encodedCapacityAndFlow[outputTexel * 3u] = runoff;
            encodedCapacityAndFlow[outputTexel * 3u + 1u] = capacity;
            encodedCapacityAndFlow[outputTexel * 3u + 2u] = flow;
            ++outputTexel;
        }
    };

    // LIVETRACK21I .hhyd v15 stores 4-bit runoff-route intensity + 4-bit standing-depth ceiling
    // + 4-bit flow. Runtime atlases remain ordinary uncompressed textures.
    if (found->encoding == 2u)
    {
        if (bytes != 2u)
            return false;
        appendCodes(m_prebakedFarPayload[offset], m_prebakedFarPayload[offset + 1u],
            kFarTexels);
        return outputTexel == kFarTexels;
    }
    if (found->encoding == 0u)
    {
        // Two texels use three bytes: CF0, CF1, runoff0|runoff1.
        if (bytes != (kFarTexels / 2u) * 3u)
            return false;
        std::size_t cursor = offset;
        for (std::size_t pair = 0u; pair < kFarTexels / 2u; ++pair)
        {
            const std::uint8_t cf0 = m_prebakedFarPayload[cursor++];
            const std::uint8_t cf1 = m_prebakedFarPayload[cursor++];
            const std::uint8_t rr = m_prebakedFarPayload[cursor++];
            appendCodes(cf0, static_cast<std::uint8_t>(rr >> 4u), 1u);
            appendCodes(cf1, static_cast<std::uint8_t>(rr & 0x0fu), 1u);
        }
        return outputTexel == kFarTexels;
    }
    if (found->encoding != 1u || (bytes % 4u) != 0u)
        return false;
    const std::size_t end = offset + bytes;
    std::size_t cursor = offset;
    while (cursor < end)
    {
        const std::uint16_t count = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(m_prebakedFarPayload[cursor])
            | (static_cast<std::uint16_t>(m_prebakedFarPayload[cursor + 1u]) << 8u));
        const std::uint8_t cf = m_prebakedFarPayload[cursor + 2u];
        const std::uint8_t runoff = m_prebakedFarPayload[cursor + 3u];
        cursor += 4u;
        if (count == 0u || outputTexel + count > kFarTexels)
            return false;
        appendCodes(cf, runoff, count);
    }
    return outputTexel == kFarTexels;
}

bool SurfaceHydrology::rasterPrebakedPuddleResponseTile(
    std::int32_t tileX,
    std::int32_t tileZ,
    std::uint32_t outputResolution,
    std::vector<std::uint8_t>& encodedCapacityAndFlow) const
{
    if (outputResolution == 32u
        && prebakedFarPuddleResponseTile(tileX, tileZ, encodedCapacityAndFlow))
    {
        return true;
    }
    return rasterPrebakedPuddleResponseTileUncached(
        tileX, tileZ, outputResolution, encodedCapacityAndFlow);
}

void SurfaceHydrology::rebuildPrebakedFarTileCache()
{
    m_prebakedFarTiles.clear();
    m_prebakedFarPayload.clear();
    if (m_prebakedTriangleTileSpans.empty())
        return;

    std::vector<std::uint64_t> keys;
    keys.reserve(m_prebakedTriangleTileSpans.size());
    for (const PrebakedTriangleTileSpan& span : m_prebakedTriangleTileSpans)
    {
        if (span.count != 0u)
            keys.push_back(span.key);
    }

    // LIVETRACK15 world bake: never create one std::vector payload for every
    // world tile at once. A 200 km^2 scene can contain roughly two million
    // authored 10 m tiles, so encode bounded batches and append them directly
    // into the flat .hhyd payload. This keeps transient bake memory bounded
    // while retaining JobSystem parallelism inside each batch.
    struct EncodedTile
    {
        std::uint8_t encoding = 0u;
        std::vector<std::uint8_t> payload;
    };
    constexpr std::size_t kEncodeBatchTiles = 4096u;
    m_prebakedFarTiles.reserve(keys.size());
    std::uint64_t offset = 0u;

    for (std::size_t batchBegin = 0u; batchBegin < keys.size(); batchBegin += kEncodeBatchTiles)
    {
        const std::size_t batchEnd = std::min(keys.size(), batchBegin + kEncodeBatchTiles);
        const std::size_t batchCount = batchEnd - batchBegin;
        std::vector<EncodedTile> encoded(batchCount);

        const auto buildRange = [&](std::size_t localBegin, std::size_t localEnd, std::size_t) {
            std::vector<std::uint8_t> raw;
            std::vector<std::uint8_t> rle;
            raw.reserve(32u * 32u * 3u);
            rle.reserve(256u);
            for (std::size_t localIndex = localBegin; localIndex < localEnd; ++localIndex)
            {
                const std::size_t index = batchBegin + localIndex;
                const std::uint32_t ux = static_cast<std::uint32_t>(keys[index] >> 32u);
                const std::uint32_t uz = static_cast<std::uint32_t>(keys[index] & 0xffffffffu);
                const std::int32_t tileX = static_cast<std::int32_t>(ux);
                const std::int32_t tileZ = static_cast<std::int32_t>(uz);
                raw.clear();
                if (!rasterPrebakedPuddleResponseTileUncached(tileX, tileZ, 32u, raw)
                    || raw.size() != 32u * 32u * 3u)
                {
                    continue;
                }
                // Exact 12-bit topology packing. Per texel we have runoff,
                // capacity and flow as logical nibbles. Raw mode stores two
                // texels in three bytes (12 bits/texel); constant and RLE modes
                // shrink repetitive roads/terrain much further.
                constexpr std::size_t kTexels = 32u * 32u;
                std::vector<std::uint8_t> cf(kTexels);
                std::vector<std::uint8_t> runoff(kTexels);
                for (std::size_t texel = 0u; texel < kTexels; ++texel)
                {
                    const std::uint8_t runoffCode = static_cast<std::uint8_t>(
                        std::min<unsigned>(15u, (unsigned(raw[texel * 3u]) + 8u) / 17u));
                    const std::uint8_t capacityCode = static_cast<std::uint8_t>(
                        std::min<unsigned>(15u, (unsigned(raw[texel * 3u + 1u]) + 8u) / 17u));
                    const std::uint8_t flowCode = static_cast<std::uint8_t>(
                        std::min<unsigned>(15u, (unsigned(raw[texel * 3u + 2u]) + 8u) / 17u));
                    cf[texel] = static_cast<std::uint8_t>((capacityCode << 4u) | flowCode);
                    runoff[texel] = runoffCode;
                }

                bool constant = true;
                for (std::size_t texel = 1u; texel < kTexels; ++texel)
                {
                    if (cf[texel] != cf[0] || runoff[texel] != runoff[0])
                    {
                        constant = false;
                        break;
                    }
                }
                if (constant)
                {
                    encoded[localIndex].encoding = 2u;
                    encoded[localIndex].payload = { cf[0], runoff[0] };
                    continue;
                }

                rle.clear();
                std::size_t texel = 0u;
                while (texel < kTexels)
                {
                    const std::uint8_t cfValue = cf[texel];
                    const std::uint8_t runoffValue = runoff[texel];
                    std::uint16_t count = 1u;
                    while (texel + count < kTexels && count < 65535u
                        && cf[texel + count] == cfValue
                        && runoff[texel + count] == runoffValue)
                    {
                        ++count;
                    }
                    rle.push_back(static_cast<std::uint8_t>(count & 0xffu));
                    rle.push_back(static_cast<std::uint8_t>((count >> 8u) & 0xffu));
                    rle.push_back(cfValue);
                    rle.push_back(runoffValue);
                    texel += count;
                }

                std::vector<std::uint8_t> packed;
                packed.reserve((kTexels / 2u) * 3u);
                for (std::size_t pair = 0u; pair < kTexels; pair += 2u)
                {
                    packed.push_back(cf[pair]);
                    packed.push_back(cf[pair + 1u]);
                    packed.push_back(static_cast<std::uint8_t>(
                        (runoff[pair] << 4u) | runoff[pair + 1u]));
                }
                if (rle.size() < packed.size())
                {
                    encoded[localIndex].encoding = 1u;
                    encoded[localIndex].payload = rle;
                }
                else
                {
                    encoded[localIndex].encoding = 0u;
                    encoded[localIndex].payload = std::move(packed);
                }
            }
        };

        if (m_jobSystem && batchCount >= 256u)
            m_jobSystem->parallelFor(batchCount, 64u, buildRange);
        else
            buildRange(0u, batchCount, 0u);

        for (std::size_t localIndex = 0u; localIndex < batchCount; ++localIndex)
        {
            EncodedTile& tile = encoded[localIndex];
            if (tile.payload.empty())
                continue;
            PrebakedFarTileIndex entry;
            entry.key = keys[batchBegin + localIndex];
            entry.payloadOffset = offset;
            entry.payloadBytes = static_cast<std::uint32_t>(tile.payload.size());
            entry.encoding = tile.encoding;
            m_prebakedFarTiles.push_back(entry);
            m_prebakedFarPayload.insert(
                m_prebakedFarPayload.end(), tile.payload.begin(), tile.payload.end());
            offset += static_cast<std::uint64_t>(tile.payload.size());
        }
    }

    m_stats.prebakedWorldTileCount = m_prebakedFarTiles.size();
    m_stats.prebakedFarPayloadBytes = static_cast<std::uint64_t>(m_prebakedFarPayload.size());
}

} // namespace heritage::physics::water
