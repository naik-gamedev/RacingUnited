#include "DynamicSurfaceGpuPagePool.hpp"

#include "../ShaderProgram.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>

namespace heritage::graphics::dynamicsurface {
namespace {

using heritage::physics::dynamicsurface::kPhysicalPageMipLevels;
using heritage::physics::dynamicsurface::kPhysicalPageResolution;
using heritage::physics::dynamicsurface::kHydroAuthorityResolution;

constexpr const char* kPageInitializationComputeShader = R"glsl(
#version 460 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba16f, binding = 1) uniform writeonly image2DArray uTrackPages;
layout(rgba8,   binding = 2) uniform writeonly image2DArray uContaminationPages;

layout(std430, binding = 3) readonly buffer InitializationQueue
{
    uint physicalSlots[];
};

uniform uint uQueueCount;

void main()
{
    const uvec3 gid = gl_GlobalInvocationID;
    if (gid.z >= uQueueCount)
        return;

    const int layer = int(physicalSlots[gid.z]);
    const ivec3 texel = ivec3(int(gid.x), int(gid.y), layer);

    // Hydro RGBA4 is cleared separately with glClearTexSubImage because RGBA4
    // is a filterable normalized texture format, not an imageStore format.
    // Track/rubber/contamination retain the existing 64x64 plane.
    if (gid.x < 64u && gid.y < 64u)
    {
        imageStore(uTrackPages, texel, vec4(20.0, 0.0, 0.0, 0.0));
        imageStore(uContaminationPages, texel, vec4(0.0));
    }
}
)glsl";

std::pair<std::uint32_t, std::uint32_t> splitSigned64(std::int64_t value)
{
    const std::uint64_t bits = static_cast<std::uint64_t>(value);
    return {
        static_cast<std::uint32_t>(bits & 0xffffffffull),
        static_cast<std::uint32_t>(bits >> 32u)
    };
}

std::uint32_t packPageAddress(
    const heritage::physics::dynamicsurface::PageAddress& page)
{
    return static_cast<std::uint32_t>(page.sheet)
        | (static_cast<std::uint32_t>(page.x) << 16u)
        | (static_cast<std::uint32_t>(page.z) << 24u);
}

bool checkNoGlError(std::string& errorMessage, const char* operation)
{
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR)
        return true;
    errorMessage = std::string("OpenGL error during ") + operation
        + ": " + std::to_string(static_cast<unsigned int>(error));
    return false;
}

} // namespace

bool DynamicSurfaceGpuPagePool::initialize(
    std::size_t requestedPhysicalPages,
    std::string& errorMessage)
{
    shutdown();
    errorMessage.clear();

    if (requestedPhysicalPages == 0)
    {
        errorMessage = "Dynamic Surface GPU page pool requested zero physical pages.";
        return false;
    }

    GLint maximumArrayLayers = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maximumArrayLayers);
    if (maximumArrayLayers <= 0)
    {
        errorMessage = "OpenGL reports no texture-array layers for Dynamic Surface.";
        return false;
    }

    const std::size_t capacity = std::min<std::size_t>(
        requestedPhysicalPages,
        static_cast<std::size_t>(maximumArrayLayers));
    if (capacity == 0)
    {
        errorMessage = "Dynamic Surface page capacity was clamped to zero.";
        return false;
    }

    while (glGetError() != GL_NO_ERROR)
    {
        // Keep initialization diagnostics local to this subsystem.
    }

    if (!createTextureArray(
            m_hydroTextureArray, GL_RGBA4,
            kHydroAuthorityResolution, 1u, capacity, errorMessage)
        || !createTextureArray(
            m_trackTextureArray, GL_RGBA16F,
            kPhysicalPageResolution, kPhysicalPageMipLevels, capacity, errorMessage)
        || !createTextureArray(
            m_contaminationTextureArray, GL_RGBA8,
            kPhysicalPageResolution, kPhysicalPageMipLevels, capacity, errorMessage))
    {
        shutdown();
        return false;
    }

    glGenBuffers(1, &m_pageTableBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pageTableBuffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(capacity * sizeof(GpuPageTableWords)),
        nullptr,
        GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_dirtyQueueBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_dirtyQueueBuffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(capacity * sizeof(GpuPageTableWords)),
        nullptr,
        GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_initializationQueueBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_initializationQueueBuffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(capacity * sizeof(std::uint32_t)),
        nullptr,
        GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    if (!m_pageTableBuffer || !m_dirtyQueueBuffer || !m_initializationQueueBuffer)
    {
        errorMessage = "OpenGL could not allocate Dynamic Surface SSBOs.";
        shutdown();
        return false;
    }

    m_pageInitializationProgram = buildComputeShaderProgram(
        kPageInitializationComputeShader);
    if (!m_pageInitializationProgram)
    {
        errorMessage = "Dynamic Surface physical-page initialization compute shader failed.";
        shutdown();
        return false;
    }
    m_initializationQueueCountUniform = glGetUniformLocation(
        m_pageInitializationProgram, "uQueueCount");

    if (!checkNoGlError(errorMessage, "Dynamic Surface page-pool allocation"))
    {
        shutdown();
        return false;
    }

    m_slotGenerations.assign(capacity, 0u);
    m_uploadedTableGeneration = 0;
    m_ready = true;
    m_stats = {};
    m_stats.ready = true;
    m_stats.capacityPages = capacity;
    const std::size_t hydroTexels = static_cast<std::size_t>(
        kHydroAuthorityResolution) * kHydroAuthorityResolution;
    const std::size_t hydroBytesPerPage = hydroTexels * 2u; // GL_RGBA4 = 16 bits/texel
    const std::size_t trackTexels =
        heritage::physics::dynamicsurface::physicalPageMipTexelCount();
    const std::size_t trackBytesPerPage = trackTexels * 8u; // RGBA16F
    const std::size_t contaminationBytesPerPage = trackTexels * 4u;
    m_stats.committedBytes = capacity * (
        hydroBytesPerPage + trackBytesPerPage + contaminationBytesPerPage);
    return true;
}

void DynamicSurfaceGpuPagePool::shutdown()
{
    if (m_pageInitializationProgram)
        glDeleteProgram(m_pageInitializationProgram);
    if (m_initializationQueueBuffer)
        glDeleteBuffers(1, &m_initializationQueueBuffer);
    if (m_dirtyQueueBuffer)
        glDeleteBuffers(1, &m_dirtyQueueBuffer);
    if (m_pageTableBuffer)
        glDeleteBuffers(1, &m_pageTableBuffer);
    if (m_contaminationTextureArray)
        glDeleteTextures(1, &m_contaminationTextureArray);
    if (m_trackTextureArray)
        glDeleteTextures(1, &m_trackTextureArray);
    if (m_hydroTextureArray)
        glDeleteTextures(1, &m_hydroTextureArray);

    m_hydroTextureArray = 0;
    m_trackTextureArray = 0;
    m_contaminationTextureArray = 0;
    m_pageTableBuffer = 0;
    m_dirtyQueueBuffer = 0;
    m_initializationQueueBuffer = 0;
    m_pageInitializationProgram = 0;
    m_initializationQueueCountUniform = -1;
    m_slotGenerations.clear();
    m_uploadedTableGeneration = 0;
    m_ready = false;
    m_stats = {};
}

bool DynamicSurfaceGpuPagePool::synchronize(
    const heritage::physics::dynamicsurface::DynamicSurfacePagePool& pagePool,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (!m_ready)
    {
        errorMessage = "Dynamic Surface GPU page pool is not initialized.";
        return false;
    }

    const auto start = std::chrono::steady_clock::now();
    const auto assignments = pagePool.residentAssignments();
    const auto dirty = pagePool.dirtyAssignments();

    std::vector<std::uint32_t> newSlots;
    newSlots.reserve(assignments.size());
    for (const auto& assignment : assignments)
    {
        if (assignment.physicalSlot >= m_slotGenerations.size())
        {
            errorMessage = "Dynamic Surface physical slot exceeds GPU pool capacity.";
            return false;
        }
        std::uint32_t& knownGeneration = m_slotGenerations[assignment.physicalSlot];
        if (knownGeneration != assignment.generation)
        {
            knownGeneration = assignment.generation;
            newSlots.push_back(assignment.physicalSlot);
        }
    }

    if (pagePool.tableGeneration() != m_uploadedTableGeneration)
    {
        uploadPageTable(assignments);
        m_uploadedTableGeneration = pagePool.tableGeneration();
        ++m_stats.pageTableUploads;
    }
    uploadDirtyQueue(dirty);

    if (!newSlots.empty())
    {
        if (!initializeNewPhysicalSlots(newSlots, errorMessage))
            return false;
        m_stats.initializedPages += newSlots.size();
        // DSURF04D samples only authoritative mip0. New slots are initialized directly at their base levels, so there is no
        // whole-array mip regeneration stall.
    }

    m_stats.ready = true;
    m_stats.residentPages = assignments.size();
    m_stats.dirtyPages = dirty.size();
    m_stats.pageTableGeneration = pagePool.tableGeneration();
    m_stats.synchronizationCpuMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
    return true;
}

bool DynamicSurfaceGpuPagePool::createTextureArray(
    GLuint& texture,
    GLenum internalFormat,
    std::uint32_t resolution,
    std::uint32_t mipLevels,
    std::size_t layers,
    std::string& errorMessage)
{
    glGenTextures(1, &texture);
    if (!texture)
    {
        errorMessage = "OpenGL could not create a Dynamic Surface texture array.";
        return false;
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    glTexStorage3D(
        GL_TEXTURE_2D_ARRAY,
        static_cast<GLsizei>(mipLevels),
        internalFormat,
        static_cast<GLsizei>(resolution),
        static_cast<GLsizei>(resolution),
        static_cast<GLsizei>(layers));
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return checkNoGlError(errorMessage, "Dynamic Surface texture-array creation");
}

bool DynamicSurfaceGpuPagePool::initializeNewPhysicalSlots(
    const std::vector<std::uint32_t>& slots,
    std::string& errorMessage)
{
    if (slots.empty())
        return true;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_initializationQueueBuffer);
    glBufferSubData(
        GL_SHADER_STORAGE_BUFFER,
        0,
        static_cast<GLsizeiptr>(slots.size() * sizeof(std::uint32_t)),
        slots.data());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_initializationQueueBuffer);

    const GLfloat hydroClear[4]{ 0.0f, 0.0f, 0.5f, 0.5f };
    for (const std::uint32_t slot : slots)
    {
        glClearTexSubImage(
            m_hydroTextureArray,
            0,
            0, 0, static_cast<GLint>(slot),
            static_cast<GLsizei>(kHydroAuthorityResolution),
            static_cast<GLsizei>(kHydroAuthorityResolution),
            1,
            GL_RGBA,
            GL_FLOAT,
            hydroClear);
    }

    glUseProgram(m_pageInitializationProgram);
    if (m_initializationQueueCountUniform >= 0)
    {
        glUniform1ui(
            m_initializationQueueCountUniform,
            static_cast<GLuint>(slots.size()));
    }

    glBindImageTexture(
        1, m_trackTextureArray, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(
        2, m_contaminationTextureArray, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);

    glDispatchCompute(
        (kPhysicalPageResolution + 7u) / 8u,
        (kPhysicalPageResolution + 7u) / 8u,
        static_cast<GLuint>(slots.size()));
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return checkNoGlError(errorMessage, "Dynamic Surface page initialization dispatch");
}

void DynamicSurfaceGpuPagePool::uploadPageTable(
    const std::vector<heritage::physics::dynamicsurface::PhysicalPageAssignment>& assignments)
{
    std::vector<GpuPageTableWords> packed;
    packed.reserve(assignments.size());
    for (const auto& assignment : assignments)
        packed.push_back(packPageTableEntry(assignment));

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pageTableBuffer);
    if (!packed.empty())
    {
        glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            static_cast<GLsizeiptr>(packed.size() * sizeof(GpuPageTableWords)),
            packed.data());
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_pageTableBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void DynamicSurfaceGpuPagePool::uploadDirtyQueue(
    const std::vector<heritage::physics::dynamicsurface::PhysicalPageAssignment>& assignments)
{
    std::vector<GpuPageTableWords> packed;
    packed.reserve(assignments.size());
    for (const auto& assignment : assignments)
        packed.push_back(packPageTableEntry(assignment));

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_dirtyQueueBuffer);
    if (!packed.empty())
    {
        glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            static_cast<GLsizeiptr>(packed.size() * sizeof(GpuPageTableWords)),
            packed.data());
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_dirtyQueueBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

bool DynamicSurfaceGpuPagePool::uploadHydroMip(
    std::uint32_t physicalSlot,
    std::uint32_t mipLevel,
    std::uint32_t resolution,
    const std::uint16_t* hydroRgba4,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (!m_ready || !hydroRgba4
        || physicalSlot >= m_slotGenerations.size()
        || mipLevel != 0u
        || resolution == 0u)
    {
        errorMessage = "Invalid Dynamic Surface hydro page upload.";
        return false;
    }

    const std::uint32_t expectedResolution = kHydroAuthorityResolution;
    if (expectedResolution != resolution)
    {
        errorMessage = "Dynamic Surface hydro upload resolution mismatch.";
        return false;
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_hydroTextureArray);
    glTexSubImage3D(
        GL_TEXTURE_2D_ARRAY,
        static_cast<GLint>(mipLevel),
        0, 0, static_cast<GLint>(physicalSlot),
        static_cast<GLsizei>(resolution),
        static_cast<GLsizei>(resolution),
        1,
        GL_RGBA,
        GL_UNSIGNED_SHORT_4_4_4_4_REV,
        hydroRgba4);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    if (!checkNoGlError(errorMessage, "Dynamic Surface packed Hydro RGBA4 upload"))
        return false;
    ++m_stats.hydroPageUploads;
    return true;
}

bool DynamicSurfaceGpuPagePool::uploadTrackMip(
    std::uint32_t physicalSlot,
    std::uint32_t mipLevel,
    std::uint32_t resolution,
    const float* trackRgba,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (!m_ready || !trackRgba
        || physicalSlot >= m_slotGenerations.size()
        || mipLevel >= kPhysicalPageMipLevels
        || resolution == 0u)
    {
        errorMessage = "Invalid Dynamic Surface Track page upload.";
        return false;
    }

    const std::uint32_t expectedResolution =
        kPhysicalPageResolution >> mipLevel;
    if (expectedResolution != resolution)
    {
        errorMessage = "Dynamic Surface Track upload mip resolution mismatch.";
        return false;
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_trackTextureArray);
    glTexSubImage3D(
        GL_TEXTURE_2D_ARRAY,
        static_cast<GLint>(mipLevel),
        0, 0, static_cast<GLint>(physicalSlot),
        static_cast<GLsizei>(resolution),
        static_cast<GLsizei>(resolution),
        1,
        GL_RGBA,
        GL_FLOAT,
        trackRgba);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    if (!checkNoGlError(errorMessage, "Dynamic Surface Track upload"))
        return false;
    ++m_stats.trackPageUploads;
    return true;
}

DynamicSurfaceGpuPagePool::GpuPageTableWords
DynamicSurfaceGpuPagePool::packPageTableEntry(
    const heritage::physics::dynamicsurface::PhysicalPageAssignment& assignment)
{
    const auto [xLo, xHi] = splitSigned64(assignment.virtualAddress.chunk.x);
    const auto [zLo, zHi] = splitSigned64(assignment.virtualAddress.chunk.z);

    GpuPageTableWords result;
    result.chunkXLo = xLo;
    result.chunkXHi = xHi;
    result.chunkZLo = zLo;
    result.chunkZHi = zHi;
    result.pagePacked = packPageAddress(assignment.virtualAddress.page);
    result.physicalSlot = assignment.physicalSlot;
    result.generation = assignment.generation;
    result.dirtyPlaneMask = static_cast<std::uint32_t>(assignment.dirtyPlanes);
    return result;
}

} // namespace heritage::graphics::dynamicsurface
