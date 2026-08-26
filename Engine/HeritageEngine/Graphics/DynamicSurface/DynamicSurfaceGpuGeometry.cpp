#include "DynamicSurfaceGpuShaders.hpp"
#include "DynamicSurfaceGpuRuntime.hpp"

#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace heritage::graphics::dynamicsurface {

void DynamicSurfaceGpuRuntime::destroyExactGeometryAtlas()
{
    if (m_geometryTileMetaBuffer) glDeleteBuffers(1, &m_geometryTileMetaBuffer);
    if (m_geometryBinIndexBuffer) glDeleteBuffers(1, &m_geometryBinIndexBuffer);
    if (m_geometryBinHeaderBuffer) glDeleteBuffers(1, &m_geometryBinHeaderBuffer);
    if (m_geometryTriangleBuffer) glDeleteBuffers(1, &m_geometryTriangleBuffer);
    m_geometryTriangleBuffer = 0;
    m_geometryBinHeaderBuffer = 0;
    m_geometryBinIndexBuffer = 0;
    m_geometryTileMetaBuffer = 0;
    m_geometryAtlasReady = false;
    m_stats.exactGeometrySupportReady = false;
    m_stats.geometryTriangles = 0;
    m_stats.geometryBinReferences = 0;
    m_stats.geometryUploadMiB = 0.0;
}
bool DynamicSurfaceGpuRuntime::rebuildExactGeometryAtlas(
    const heritage::physics::dynamicsurface::DynamicSurfaceSystem* dynamicSurface,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (!dynamicSurface || !m_centerTileValid)
    {
        errorMessage = "Dynamic Surface static triangle bake is unavailable.";
        return false;
    }

    using heritage::physics::dynamicsurface::ChunkAddress;
    using heritage::physics::dynamicsurface::StaticSurfacePatchTriangle;
    constexpr std::uint32_t binsPerChunk = kGeometryBinResolution * kGeometryBinResolution;
    constexpr std::uint32_t chunkSlots = kGeometryGridResolution * kGeometryGridResolution;

    m_geometryCenterChunkX = static_cast<std::int32_t>(std::floor(
        (static_cast<double>(m_centerTileX) * kTileWorldSizeM) / 100.0));
    m_geometryCenterChunkZ = static_cast<std::int32_t>(std::floor(
        (static_cast<double>(m_centerTileZ) * kTileWorldSizeM) / 100.0));

    std::vector<GpuSurfaceTriangle> gpuTriangles;
    std::vector<GpuBinHeader> gpuHeaders(
        static_cast<std::size_t>(chunkSlots) * binsPerChunk);
    std::vector<std::uint32_t> gpuIndices;
    std::vector<GpuTileGeometryMeta> meta(chunkSlots);

    const auto binCoordinate = [](double value) -> std::uint32_t {
        const double scaled = value * (static_cast<double>(kGeometryBinResolution) / 100.0);
        return static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(std::floor(scaled)), 0,
            static_cast<int>(kGeometryBinResolution) - 1));
    };

    std::uint32_t validChunks = 0u;
    for (int gz = 0; gz < static_cast<int>(kGeometryGridResolution); ++gz)
    {
        for (int gx = 0; gx < static_cast<int>(kGeometryGridResolution); ++gx)
        {
            const std::uint32_t slot = static_cast<std::uint32_t>(
                gz * static_cast<int>(kGeometryGridResolution) + gx);
            meta[slot].binHeaderBase = slot * binsPerChunk;
            const std::int32_t chunkX = m_geometryCenterChunkX + gx - kGeometryGridHalfSpan;
            const std::int32_t chunkZ = m_geometryCenterChunkZ + gz - kGeometryGridHalfSpan;
            const auto* chunk = dynamicSurface->findChunk(ChunkAddress{
                static_cast<std::int64_t>(chunkX), static_cast<std::int64_t>(chunkZ) });
            if (!chunk || chunk->staticTriangles().empty())
                continue;

            const double originX = static_cast<double>(chunkX) * 100.0;
            const double originZ = static_cast<double>(chunkZ) * 100.0;
            const auto& sourceTriangles = chunk->staticTriangles();
            const std::uint32_t triangleBase = static_cast<std::uint32_t>(gpuTriangles.size());
            std::vector<std::uint32_t> counts(binsPerChunk, 0u);
            struct Bounds { std::uint32_t minX, maxX, minZ, maxZ; };
            std::vector<Bounds> boundsList;
            boundsList.reserve(sourceTriangles.size());

            for (const StaticSurfacePatchTriangle& triangle : sourceTriangles)
            {
                GpuSurfaceTriangle gpu;
                gpu.a = { static_cast<float>(triangle.a.x - originX),
                    static_cast<float>(triangle.a.y),
                    static_cast<float>(triangle.a.z - originZ),
                    static_cast<float>(triangle.surfaceSheetId) };
                gpu.b = { static_cast<float>(triangle.b.x - originX),
                    static_cast<float>(triangle.b.y),
                    static_cast<float>(triangle.b.z - originZ), 0.0f };
                gpu.c = { static_cast<float>(triangle.c.x - originX),
                    static_cast<float>(triangle.c.y),
                    static_cast<float>(triangle.c.z - originZ), 0.0f };
                gpu.hydro = { std::max(triangle.infiltrationCapacityMmPerHour, 0.0f),
                    std::max(triangle.drainageCapacityMmPerHour, 0.0f),
                    std::max(std::abs(triangle.flowRoughness), 0.005f),
                    std::max(triangle.depressionStorageM, 0.0f) };
                gpuTriangles.push_back(gpu);

                const double minX = std::clamp(std::min({ triangle.a.x, triangle.b.x, triangle.c.x }) - originX, 0.0, 99.999999);
                const double maxX = std::clamp(std::max({ triangle.a.x, triangle.b.x, triangle.c.x }) - originX, 0.0, 99.999999);
                const double minZ = std::clamp(std::min({ triangle.a.z, triangle.b.z, triangle.c.z }) - originZ, 0.0, 99.999999);
                const double maxZ = std::clamp(std::max({ triangle.a.z, triangle.b.z, triangle.c.z }) - originZ, 0.0, 99.999999);
                Bounds bounds{ binCoordinate(minX), binCoordinate(maxX),
                    binCoordinate(minZ), binCoordinate(maxZ) };
                boundsList.push_back(bounds);
                for (std::uint32_t z = bounds.minZ; z <= bounds.maxZ; ++z)
                    for (std::uint32_t x = bounds.minX; x <= bounds.maxX; ++x)
                        ++counts[static_cast<std::size_t>(z) * kGeometryBinResolution + x];
            }

            std::uint32_t running = static_cast<std::uint32_t>(gpuIndices.size());
            std::vector<std::uint32_t> cursors(binsPerChunk, 0u);
            for (std::uint32_t bin = 0; bin < binsPerChunk; ++bin)
            {
                GpuBinHeader& header = gpuHeaders[
                    static_cast<std::size_t>(meta[slot].binHeaderBase) + bin];
                header.offset = running;
                header.count = counts[bin];
                cursors[bin] = running;
                running += counts[bin];
            }
            gpuIndices.resize(running);
            for (std::uint32_t localTriangle = 0;
                 localTriangle < static_cast<std::uint32_t>(boundsList.size());
                 ++localTriangle)
            {
                const Bounds& bounds = boundsList[localTriangle];
                for (std::uint32_t z = bounds.minZ; z <= bounds.maxZ; ++z)
                {
                    for (std::uint32_t x = bounds.minX; x <= bounds.maxX; ++x)
                    {
                        const std::uint32_t bin = z * kGeometryBinResolution + x;
                        gpuIndices[cursors[bin]++] = triangleBase + localTriangle;
                    }
                }
            }
            meta[slot].valid = 1u;
            ++validChunks;
        }
    }

    if (gpuTriangles.empty()) gpuTriangles.push_back({});
    if (gpuIndices.empty()) gpuIndices.push_back(0u);

    destroyExactGeometryAtlas();
    glGenBuffers(1, &m_geometryTriangleBuffer);
    glGenBuffers(1, &m_geometryBinHeaderBuffer);
    glGenBuffers(1, &m_geometryBinIndexBuffer);
    glGenBuffers(1, &m_geometryTileMetaBuffer);
    if (!m_geometryTriangleBuffer || !m_geometryBinHeaderBuffer
        || !m_geometryBinIndexBuffer || !m_geometryTileMetaBuffer)
    {
        errorMessage = "Failed to allocate DSURF04G exact-geometry SSBOs.";
        destroyExactGeometryAtlas();
        return false;
    }
    const auto upload = [](GLuint buffer, const void* data, std::size_t bytes) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
    };
    upload(m_geometryTriangleBuffer, gpuTriangles.data(), gpuTriangles.size() * sizeof(GpuSurfaceTriangle));
    upload(m_geometryBinHeaderBuffer, gpuHeaders.data(), gpuHeaders.size() * sizeof(GpuBinHeader));
    upload(m_geometryBinIndexBuffer, gpuIndices.data(), gpuIndices.size() * sizeof(std::uint32_t));
    upload(m_geometryTileMetaBuffer, meta.data(), meta.size() * sizeof(GpuTileGeometryMeta));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    if (!detail::checkNoGlError(errorMessage, "DSURF04G exact-geometry atlas upload"))
    {
        destroyExactGeometryAtlas();
        return false;
    }

    m_geometryAtlasReady = true;
    m_stats.exactGeometrySupportReady = true;
    m_stats.geometryTriangles = gpuTriangles.size();
    m_stats.geometryBinReferences = gpuIndices.size();
    const std::size_t bytes = gpuTriangles.size() * sizeof(GpuSurfaceTriangle)
        + gpuHeaders.size() * sizeof(GpuBinHeader)
        + gpuIndices.size() * sizeof(std::uint32_t)
        + meta.size() * sizeof(GpuTileGeometryMeta);
    m_stats.geometryUploadMiB = static_cast<double>(bytes) / (1024.0 * 1024.0);
    (void)validChunks;
    return true;
}

} // namespace heritage::graphics::dynamicsurface
