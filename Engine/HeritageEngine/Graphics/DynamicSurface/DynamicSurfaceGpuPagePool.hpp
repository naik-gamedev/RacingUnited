#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glad/glad.h>

#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfacePagePool.hpp"

namespace heritage::graphics::dynamicsurface {

struct DynamicSurfaceGpuPagePoolStats
{
    bool ready = false;
    std::size_t capacityPages = 0;
    std::size_t residentPages = 0;
    std::size_t dirtyPages = 0;
    std::size_t committedBytes = 0;
    std::uint64_t pageTableGeneration = 0;
    std::uint64_t pageTableUploads = 0;
    std::uint64_t initializedPages = 0;
    std::uint64_t mipRegenerations = 0;
    std::uint64_t hydroPageUploads = 0;
    std::uint64_t trackPageUploads = 0;
    double synchronizationCpuMs = 0.0;
};

// DSURF02 renderer-side mirror of DynamicSurfacePagePool. The physical storage
// is three texture arrays (Hydro/Track/Contamination) with one layer per
// resident software page. Virtual identity remains owned by SurfaceWorld.
class DynamicSurfaceGpuPagePool
{
public:
    DynamicSurfaceGpuPagePool() = default;
    DynamicSurfaceGpuPagePool(const DynamicSurfaceGpuPagePool&) = delete;
    DynamicSurfaceGpuPagePool& operator=(const DynamicSurfaceGpuPagePool&) = delete;

    bool initialize(std::size_t requestedPhysicalPages, std::string& errorMessage);
    void shutdown();

    bool synchronize(
        const heritage::physics::dynamicsurface::DynamicSurfacePagePool& pagePool,
        std::string& errorMessage);

    bool ready() const { return m_ready; }
    const DynamicSurfaceGpuPagePoolStats& stats() const { return m_stats; }

    GLuint hydroTextureArray() const { return m_hydroTextureArray; }
    GLuint trackTextureArray() const { return m_trackTextureArray; }
    GLuint contaminationTextureArray() const { return m_contaminationTextureArray; }
    GLuint pageTableBuffer() const { return m_pageTableBuffer; }
    GLuint dirtyQueueBuffer() const { return m_dirtyQueueBuffer; }

    bool uploadHydroMip(
        std::uint32_t physicalSlot,
        std::uint32_t mipLevel,
        std::uint32_t resolution,
        const std::uint16_t* hydroRgba4,
        std::string& errorMessage);
    bool uploadTrackMip(
        std::uint32_t physicalSlot,
        std::uint32_t mipLevel,
        std::uint32_t resolution,
        const float* trackRgba,
        std::string& errorMessage);

private:
    struct GpuPageTableWords
    {
        std::uint32_t chunkXLo = 0;
        std::uint32_t chunkXHi = 0;
        std::uint32_t chunkZLo = 0;
        std::uint32_t chunkZHi = 0;
        std::uint32_t pagePacked = 0;
        std::uint32_t physicalSlot = 0;
        std::uint32_t generation = 0;
        std::uint32_t dirtyPlaneMask = 0;
    };

    bool createTextureArray(
        GLuint& texture,
        GLenum internalFormat,
        std::uint32_t resolution,
        std::uint32_t mipLevels,
        std::size_t layers,
        std::string& errorMessage);
    bool initializeNewPhysicalSlots(
        const std::vector<std::uint32_t>& slots,
        std::string& errorMessage);
    void uploadPageTable(
        const std::vector<heritage::physics::dynamicsurface::PhysicalPageAssignment>& assignments);
    void uploadDirtyQueue(
        const std::vector<heritage::physics::dynamicsurface::PhysicalPageAssignment>& assignments);

    static GpuPageTableWords packPageTableEntry(
        const heritage::physics::dynamicsurface::PhysicalPageAssignment& assignment);

    GLuint m_hydroTextureArray = 0;
    GLuint m_trackTextureArray = 0;
    GLuint m_contaminationTextureArray = 0;
    GLuint m_pageTableBuffer = 0;
    GLuint m_dirtyQueueBuffer = 0;
    GLuint m_initializationQueueBuffer = 0;
    GLuint m_pageInitializationProgram = 0;
    GLint m_initializationQueueCountUniform = -1;

    std::vector<std::uint32_t> m_slotGenerations;
    std::uint64_t m_uploadedTableGeneration = 0;
    bool m_ready = false;
    DynamicSurfaceGpuPagePoolStats m_stats{};
};

} // namespace heritage::graphics::dynamicsurface
