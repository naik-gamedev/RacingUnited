#include "EntityMeshRenderer.hpp"

#include "../../Physics/Surfaces/SurfaceWorld.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace heritage::graphics {
namespace {

// LIVETRACK04: the expensive high-resolution Hydro field lives and evolves on
// the GPU. This older persistent-page upload path remains only for Track/rubber/
// temperature state. It must never raster/upload CPU Hydro while GPU Hydro
// authority is ready, otherwise we would run the water system twice.
constexpr std::size_t kMaximumDynamicSurfacePageUploadsPerFrame = 16u;
constexpr float kDynamicSurfaceTrackPresentationHz = 2.0f;


double distanceSquaredToPageBounds(
    const heritage::math::DVec3& cameraGlobal,
    const heritage::physics::dynamicsurface::VirtualPageAddress& address)
{
    const double pageSizeM = heritage::physics::dynamicsurface::kPhysicalPageWorldSizeM;
    const double minX = static_cast<double>(address.chunk.x)
        * heritage::physics::dynamicsurface::kChunkSizeM;
    const double minZ = static_cast<double>(address.chunk.z)
        * heritage::physics::dynamicsurface::kChunkSizeM;
    const double maxX = minX + pageSizeM;
    const double maxZ = minZ + pageSizeM;
    const double nearestX = std::clamp(cameraGlobal.x, minX, maxX);
    const double nearestZ = std::clamp(cameraGlobal.z, minZ, maxZ);
    const double dx = cameraGlobal.x - nearestX;
    const double dz = cameraGlobal.z - nearestZ;
    return dx * dx + dz * dz;
}

struct PageRefreshCandidate
{
    heritage::physics::dynamicsurface::PhysicalPageAssignment assignment{};
    double distanceSquaredM2 = 0.0;
    float oldestRefreshSeconds = 0.0f;
    bool uploadHydro = false;
    bool uploadTrack = false;
    std::uint64_t hydroRevision = 0;
    std::uint64_t trackRevision = 0;
};


} // namespace

bool EntityMeshRenderer::initializeSurfaceWetnessMaterialBindings()
{
    if (!m_program)
        return false;

    const auto uniform = [this](const char* name) {
        return glGetUniformLocation(m_program, name);
    };
    m_uniforms.surfaceWetnessReceiver = uniform("uSurfaceWetnessReceiver");
    m_uniforms.dynamicSurfaceHydroPages = uniform("uDynamicSurfaceHydroPages");
    m_uniforms.dynamicSurfacePageIndirection =
        uniform("uDynamicSurfacePageIndirection");
    m_uniforms.dynamicSurfaceIndirectionOriginRelativeXZ =
        uniform("uDynamicSurfaceIndirectionOriginRelativeXZ");
    m_uniforms.dynamicSurfacePageWorldSizeM =
        uniform("uDynamicSurfacePageWorldSizeM");
    m_uniforms.dynamicSurfaceIndirectionResolution =
        uniform("uDynamicSurfaceIndirectionResolution");
    m_uniforms.dynamicSurfaceHydroBaseMip =
        uniform("uDynamicSurfaceHydroBaseMip");
    m_uniforms.dynamicSurfaceCameraGlobalY =
        uniform("uDynamicSurfaceCameraGlobalY");
    m_uniforms.dynamicSurfaceActive = uniform("uDynamicSurfaceActive");
    m_uniforms.gpuDynamicSurfaceAuthorityActive =
        uniform("uGpuDynamicSurfaceAuthorityActive");
    m_uniforms.gpuWaterAtlas = uniform("uGpuWaterAtlas");
    m_uniforms.gpuWaterPresentationAtlas = uniform("uGpuWaterPresentationAtlas");
    m_uniforms.gpuWaterPresentationReady = uniform("uGpuWaterPresentationReady");
    m_uniforms.gpuSnowAtlas = uniform("uGpuSnowAtlas");
    m_uniforms.gpuMudAtlas = uniform("uGpuMudAtlas");
    m_uniforms.gpuTileIndirection = uniform("uGpuTileIndirection");
    m_uniforms.gpuDynamicSurfaceCenterOriginRelativeXZ =
        uniform("uGpuDynamicSurfaceCenterOriginRelativeXZ");
    m_uniforms.gpuDynamicSurfaceTileMapCenter =
        uniform("uGpuDynamicSurfaceTileMapCenter");
    m_uniforms.gpuDynamicSurfaceTileResolution =
        uniform("uGpuDynamicSurfaceTileResolution");
    m_uniforms.gpuDynamicSurfaceAtlasColumns =
        uniform("uGpuDynamicSurfaceAtlasColumns");
    m_uniforms.gpuDynamicSurfaceSnowReady = uniform("uGpuDynamicSurfaceSnowReady");
    m_uniforms.gpuDynamicSurfaceMudReady = uniform("uGpuDynamicSurfaceMudReady");
    m_uniforms.surfaceWetnessBreakupMask = uniform("uSurfaceWetnessBreakupMask");
    m_uniforms.hasSurfaceWetnessBreakupMask =
        uniform("uHasSurfaceWetnessBreakupMask");
    m_uniforms.surfacePatternCameraModuloXZ =
        uniform("uSurfacePatternCameraModuloXZ");
    m_uniforms.surfacePresentationTime = uniform("uSurfacePresentationTime");
    m_uniforms.surfaceWeatherFilmWetness = uniform("uSurfaceWeatherFilmWetness");
    m_uniforms.surfaceWeatherFilmDepthM = uniform("uSurfaceWeatherFilmDepthM");

    glUniform1i(m_uniforms.dynamicSurfaceHydroPages, 12);
    glUniform1i(m_uniforms.surfaceWetnessBreakupMask, 14);
    glUniform1i(m_uniforms.dynamicSurfacePageIndirection, 15);
    glUniform1i(m_uniforms.gpuWaterAtlas, 16);
    glUniform1i(m_uniforms.gpuWaterPresentationAtlas, 20);
    glUniform1i(m_uniforms.gpuSnowAtlas, 17);
    glUniform1i(m_uniforms.gpuMudAtlas, 18);
    glUniform1i(m_uniforms.gpuTileIndirection, 19);
    return initializeSurfaceWetnessResources();
}

void EntityMeshRenderer::bindSurfaceWetnessMaterialState(
    const heritage::math::DVec3& cameraGlobal,
    float elapsedSeconds)
{
    constexpr double kSurfacePatternModuloM = 4096.0;
    glUniform2f(
        m_uniforms.surfacePatternCameraModuloXZ,
        static_cast<float>(std::fmod(cameraGlobal.x, kSurfacePatternModuloM)),
        static_cast<float>(std::fmod(cameraGlobal.z, kSurfacePatternModuloM)));
    glUniform1f(m_uniforms.surfacePresentationTime, elapsedSeconds);
    glUniform1f(m_uniforms.surfaceWeatherFilmWetness, m_surfaceWeatherFilmWetness);
    glUniform1f(m_uniforms.surfaceWeatherFilmDepthM, m_surfaceWeatherFilmDepthM);
    glUniform1i(
        m_uniforms.hasSurfaceWetnessBreakupMask,
        m_surfaceWetnessBreakupTexture != 0 ? 1 : 0);

    const bool active = m_dynamicSurfaceGpuPagePool.ready()
        && m_dynamicSurfacePageIndirectionTexture != 0;
    glUniform1i(m_uniforms.dynamicSurfaceActive, active ? 1 : 0);
    glUniform2f(
        m_uniforms.dynamicSurfaceIndirectionOriginRelativeXZ,
        static_cast<float>(m_dynamicSurfaceIndirectionOriginGlobal.x - cameraGlobal.x),
        static_cast<float>(m_dynamicSurfaceIndirectionOriginGlobal.z - cameraGlobal.z));
    glUniform1f(
        m_uniforms.dynamicSurfacePageWorldSizeM,
        static_cast<float>(heritage::physics::dynamicsurface::kPhysicalPageWorldSizeM));
    glUniform1i(
        m_uniforms.dynamicSurfaceIndirectionResolution,
        kDynamicSurfaceIndirectionResolution);
    glUniform1i(
        m_uniforms.dynamicSurfaceHydroBaseMip,
        static_cast<GLint>(kDynamicSurfaceHydroBaseMip));
    glUniform1f(
        m_uniforms.dynamicSurfaceCameraGlobalY,
        static_cast<float>(cameraGlobal.y));

    glActiveTexture(GL_TEXTURE0 + 12);
    glBindTexture(
        GL_TEXTURE_2D_ARRAY,
        m_dynamicSurfaceGpuPagePool.hydroTextureArray());
    glActiveTexture(GL_TEXTURE0 + 14);
    glBindTexture(GL_TEXTURE_2D, m_surfaceWetnessBreakupTexture);
    glActiveTexture(GL_TEXTURE0 + 15);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfacePageIndirectionTexture);

    const auto& gpuStats = m_dynamicSurfaceGpuLodPrototype.stats();
    const bool gpuAuthorityActive = m_dynamicSurfaceGpuLodPrototype.authoritativeReady();
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceAuthorityActive,
        gpuAuthorityActive ? 1 : 0);
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceSnowReady,
        gpuStats.snowReady ? 1 : 0);
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceMudReady,
        gpuStats.mudReady ? 1 : 0);
    glUniform1i(
        m_uniforms.gpuWaterPresentationReady,
        gpuStats.waterPresentationReady ? 1 : 0);
    const double centerOriginX =
        static_cast<double>(m_dynamicSurfaceGpuLodPrototype.centerTileX())
        * static_cast<double>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuLodPrototype::tileWorldSizeM());
    const double centerOriginZ =
        static_cast<double>(m_dynamicSurfaceGpuLodPrototype.centerTileZ())
        * static_cast<double>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuLodPrototype::tileWorldSizeM());
    glUniform2f(
        m_uniforms.gpuDynamicSurfaceCenterOriginRelativeXZ,
        static_cast<float>(centerOriginX - cameraGlobal.x),
        static_cast<float>(centerOriginZ - cameraGlobal.z));

    glUniform2i(
        m_uniforms.gpuDynamicSurfaceTileMapCenter,
        heritage::graphics::dynamicsurface::DynamicSurfaceGpuLodPrototype::tileMapHalfSpan(),
        heritage::graphics::dynamicsurface::DynamicSurfaceGpuLodPrototype::tileMapHalfSpan());
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceTileResolution,
        static_cast<GLint>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuLodPrototype::tileResolution()));
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceAtlasColumns,
        static_cast<GLint>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuLodPrototype::atlasColumns()));

    glActiveTexture(GL_TEXTURE0 + 16);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuLodPrototype.waterTexture());
    glActiveTexture(GL_TEXTURE0 + 17);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuLodPrototype.snowTexture());
    glActiveTexture(GL_TEXTURE0 + 18);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuLodPrototype.mudTexture());
    glActiveTexture(GL_TEXTURE0 + 19);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuLodPrototype.tileIndirectionTexture());
    glActiveTexture(GL_TEXTURE0 + 20);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuLodPrototype.waterPresentationTexture());
}

void EntityMeshRenderer::updateDynamicSurfaceStatePages(
    const heritage::physics::SurfaceWorld* surfaceWorld,
    const heritage::math::DVec3& cameraGlobal,
    float elapsedSeconds)
{
    const auto started = std::chrono::steady_clock::now();
    if (!surfaceWorld || !m_dynamicSurfaceGpuPagePool.ready()
        || !m_dynamicSurfacePageIndirectionTexture)
    {
        return;
    }

    const auto weather = surfaceWorld->weatherOutput();
    m_surfaceWeatherFilmDepthM = weather.valid
        ? static_cast<float>(std::max(weather.waterFilmDepthM, 0.0))
        : 0.0f;
    m_surfaceWeatherFilmWetness = weather.valid
        ? static_cast<float>(std::clamp(weather.effectiveWetness, 0.0, 1.0))
        : 0.0f;

    const auto assignments = surfaceWorld->dynamicSurface().pagePool().residentAssignments();

    // DSURF03 indirection is only a lookup structure. The actual Hydro/support
    // data remains world anchored inside persistent page-array layers.
    const double pageSizeM = heritage::physics::dynamicsurface::kPhysicalPageWorldSizeM;
    const double tableWorldSizeM =
        static_cast<double>(kDynamicSurfaceIndirectionResolution) * pageSizeM;
    const double originX = std::floor(cameraGlobal.x / pageSizeM) * pageSizeM
        - tableWorldSizeM * 0.5;
    const double originZ = std::floor(cameraGlobal.z / pageSizeM) * pageSizeM
        - tableWorldSizeM * 0.5;
    const bool indirectionDirty =
        originX != m_dynamicSurfaceIndirectionOriginGlobal.x
        || originZ != m_dynamicSurfaceIndirectionOriginGlobal.z
        || surfaceWorld->dynamicSurface().pagePool().tableGeneration()
            != m_dynamicSurfaceIndirectionTableGeneration;
    m_dynamicSurfaceIndirectionOriginGlobal = { originX, cameraGlobal.y, originZ };

    if (indirectionDirty)
    {
        const std::size_t tableTexelCount =
            static_cast<std::size_t>(kDynamicSurfaceIndirectionResolution)
            * static_cast<std::size_t>(kDynamicSurfaceIndirectionResolution);
        // LIVETRACK03: one indirection value per 100m X/Z tile. Hydro no
        // longer stores/chooses up to four vertical surface sheets. Sheet 0 is
        // only the canonical page-pool address used to carry the one shared
        // X/Z water field; Track can still keep its separate sheet-aware state.
        m_dynamicSurfaceIndirectionScratch.assign(tableTexelCount, 0);

        for (const auto& assignment : assignments)
        {
            const auto& address = assignment.virtualAddress;
            if (address.page.sheet != 0u)
                continue;
            const double pageOriginX = static_cast<double>(address.chunk.x)
                * heritage::physics::dynamicsurface::kChunkSizeM
                + static_cast<double>(address.page.x) * pageSizeM;
            const double pageOriginZ = static_cast<double>(address.chunk.z)
                * heritage::physics::dynamicsurface::kChunkSizeM
                + static_cast<double>(address.page.z) * pageSizeM;
            const int tableX = static_cast<int>(std::floor(
                (pageOriginX - originX) / pageSizeM + 0.5));
            const int tableZ = static_cast<int>(std::floor(
                (pageOriginZ - originZ) / pageSizeM + 0.5));
            if (tableX < 0 || tableZ < 0
                || tableX >= kDynamicSurfaceIndirectionResolution
                || tableZ >= kDynamicSurfaceIndirectionResolution)
            {
                continue;
            }
            const std::size_t index =
                static_cast<std::size_t>(tableZ) * kDynamicSurfaceIndirectionResolution
                + static_cast<std::size_t>(tableX);
            m_dynamicSurfaceIndirectionScratch[index] =
                static_cast<std::int32_t>(assignment.physicalSlot + 1u);
        }

        glActiveTexture(GL_TEXTURE0 + 15);
        glBindTexture(GL_TEXTURE_2D, m_dynamicSurfacePageIndirectionTexture);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            kDynamicSurfaceIndirectionResolution,
            kDynamicSurfaceIndirectionResolution,
            GL_RED_INTEGER,
            GL_INT,
            m_dynamicSurfaceIndirectionScratch.data());
        m_dynamicSurfaceIndirectionTableGeneration =
            surfaceWorld->dynamicSurface().pagePool().tableGeneration();
    }

    const auto& dynamicSurface = surfaceWorld->dynamicSurface();
    const bool gpuHydroAuthority = m_dynamicSurfaceGpuLodPrototype.authoritativeReady();
    const auto& hydroStats = dynamicSurface.hydroStats();
    const auto& thermalStats = dynamicSurface.thermalStats();
    if (assignments.empty())
    {
        m_frameStats.dynamicSurfaceStateUploadCpuMs +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
        return;
    }

    std::vector<PageRefreshCandidate> candidates;
    candidates.reserve(assignments.size());
    for (const auto& assignment : assignments)
    {
        const double refreshHz =
            heritage::physics::dynamicsurface::UpdateCadence::hydroNearHz;
        const bool canonicalHydroPage = assignment.virtualAddress.page.sheet == 0u;
        const double hydroDistanceSquared = distanceSquaredToPageBounds(
            cameraGlobal, assignment.virtualAddress);
        const double hydroRange =
            heritage::physics::dynamicsurface::UpdateCadence::puddleVisualRangeM;
        const bool hydroInPresentationRange = !gpuHydroAuthority
            && canonicalHydroPage
            && hydroDistanceSquared <= hydroRange * hydroRange;
        const std::uint64_t hydroRevision = !gpuHydroAuthority
            && hydroStats.available && canonicalHydroPage
            ? dynamicSurface.hydroPageRevision(assignment.virtualAddress)
            : 0u;
        const std::uint64_t trackRevision = thermalStats.available
            ? dynamicSurface.trackPageRevision(assignment.virtualAddress)
            : 0u;

        const auto hydroStepIt = m_dynamicSurfaceHydroUploadedStep.find(
            assignment.virtualAddress);
        const auto hydroGenerationIt = m_dynamicSurfaceHydroUploadedGeneration.find(
            assignment.virtualAddress);
        const auto hydroTimeIt = m_dynamicSurfaceHydroLastRefreshSeconds.find(
            assignment.virtualAddress);
        const bool hydroNeverUploaded = hydroRevision > 0u
            && (hydroStepIt == m_dynamicSurfaceHydroUploadedStep.end()
                || hydroGenerationIt == m_dynamicSurfaceHydroUploadedGeneration.end()
                || hydroGenerationIt->second != assignment.generation);
        const bool hydroAdvanced = hydroRevision > 0u
            && (hydroNeverUploaded || hydroStepIt->second != hydroRevision);
        const float hydroInterval = refreshHz > 0.0
            ? static_cast<float>(1.0 / refreshHz)
            : (std::numeric_limits<float>::max)();
        const bool hydroDue = hydroInPresentationRange
            && refreshHz > 0.0 && hydroAdvanced
            && (hydroNeverUploaded
                || hydroTimeIt == m_dynamicSurfaceHydroLastRefreshSeconds.end()
                || elapsedSeconds - hydroTimeIt->second >= hydroInterval);

        const auto trackStepIt = m_dynamicSurfaceTrackUploadedStep.find(
            assignment.virtualAddress);
        const auto trackGenerationIt = m_dynamicSurfaceTrackUploadedGeneration.find(
            assignment.virtualAddress);
        const auto trackTimeIt = m_dynamicSurfaceTrackLastRefreshSeconds.find(
            assignment.virtualAddress);
        const bool trackNeverUploaded = trackRevision > 0u
            && (trackStepIt == m_dynamicSurfaceTrackUploadedStep.end()
                || trackGenerationIt == m_dynamicSurfaceTrackUploadedGeneration.end()
                || trackGenerationIt->second != assignment.generation);
        const bool trackAdvanced = trackRevision > 0u
            && (trackNeverUploaded || trackStepIt->second != trackRevision);
        const float trackInterval = 1.0f / kDynamicSurfaceTrackPresentationHz;
        const bool trackDue = trackAdvanced
            && (trackNeverUploaded
                || trackTimeIt == m_dynamicSurfaceTrackLastRefreshSeconds.end()
                || elapsedSeconds - trackTimeIt->second >= trackInterval);

        if (!hydroDue && !trackDue)
            continue;


        float oldestRefresh = (std::numeric_limits<float>::max)();
        if (hydroDue)
            oldestRefresh = hydroNeverUploaded || hydroTimeIt == m_dynamicSurfaceHydroLastRefreshSeconds.end()
                ? -(std::numeric_limits<float>::max)()
                : hydroTimeIt->second;
        if (trackDue)
        {
            const float trackRefresh = trackNeverUploaded
                || trackTimeIt == m_dynamicSurfaceTrackLastRefreshSeconds.end()
                ? -(std::numeric_limits<float>::max)()
                : trackTimeIt->second;
            oldestRefresh = std::min(oldestRefresh, trackRefresh);
        }
        candidates.push_back({
            assignment,
            hydroDistanceSquared,
            oldestRefresh,
            hydroDue,
            trackDue,
            hydroRevision,
            trackRevision });
    }
    std::sort(
        candidates.begin(), candidates.end(),
        [](const PageRefreshCandidate& a, const PageRefreshCandidate& b) {
            if (a.oldestRefreshSeconds != b.oldestRefreshSeconds)
                return a.oldestRefreshSeconds < b.oldestRefreshSeconds;
            return a.distanceSquaredM2 < b.distanceSquaredM2;
        });

    const std::size_t uploadCount = std::min(
        candidates.size(), kMaximumDynamicSurfacePageUploadsPerFrame);
    for (std::size_t candidateIndex = 0; candidateIndex < uploadCount; ++candidateIndex)
    {
        const PageRefreshCandidate& candidate = candidates[candidateIndex];
        const auto& assignment = candidate.assignment;
        const std::uint32_t hydroResolution =
            heritage::physics::dynamicsurface::kHydroAuthorityResolution;
        const std::uint32_t trackResolution =
            heritage::physics::dynamicsurface::kTrackAuthorityResolution;

        bool uploadHydro = candidate.uploadHydro;
        bool uploadTrack = candidate.uploadTrack;
        if (uploadHydro)
        {
            uploadHydro = dynamicSurface.rasterHydroPage(
                assignment.virtualAddress,
                hydroResolution,
                m_dynamicSurfaceHydroScratch);
        }

        if (uploadTrack)
        {
            uploadTrack = dynamicSurface.rasterTrackPage(
                assignment.virtualAddress,
                trackResolution,
                m_dynamicSurfaceTrackScratch);
        }
        if (!uploadHydro && !uploadTrack)
            continue;

        // Legacy CPU Hydro upload is reachable only before LIVETRACK04 GPU
        // authority becomes ready. Once ready, uploadHydro is false above and
        // this page mirror carries Track/rubber/temperature state only.
        std::string uploadError;
        bool uploadOk = true;
        if (uploadHydro
            && !m_dynamicSurfaceGpuPagePool.uploadHydroMip(
                assignment.physicalSlot,
                kDynamicSurfaceHydroBaseMip,
                hydroResolution,
                m_dynamicSurfaceHydroScratch.data(),
                uploadError))
        {
            uploadOk = false;
        }
        if (uploadOk && uploadTrack
            && !m_dynamicSurfaceGpuPagePool.uploadTrackMip(
                assignment.physicalSlot,
                kDynamicSurfaceHydroBaseMip,
                trackResolution,
                m_dynamicSurfaceTrackScratch.data(),
                uploadError))
        {
            uploadOk = false;
        }

        if (!uploadOk)
        {
            static bool reportedUploadFailure = false;
            if (!reportedUploadFailure)
            {
                std::cerr << "Heritage Dynamic Surface Hydro/Track upload warning: "
                          << uploadError << '\n';
                reportedUploadFailure = true;
            }
            continue;
        }

        if (uploadHydro)
        {
            m_dynamicSurfaceHydroUploadedStep[assignment.virtualAddress] =
                candidate.hydroRevision;
            m_dynamicSurfaceHydroUploadedGeneration[assignment.virtualAddress] =
                assignment.generation;
            m_dynamicSurfaceHydroLastRefreshSeconds[assignment.virtualAddress] =
                elapsedSeconds;
            ++m_frameStats.dynamicSurfaceHydroPageUploads;
        }
        if (uploadTrack)
        {
            m_dynamicSurfaceTrackUploadedStep[assignment.virtualAddress] =
                candidate.trackRevision;
            m_dynamicSurfaceTrackUploadedGeneration[assignment.virtualAddress] =
                assignment.generation;
            m_dynamicSurfaceTrackLastRefreshSeconds[assignment.virtualAddress] =
                elapsedSeconds;
            ++m_frameStats.dynamicSurfaceTrackPageUploads;
        }
    }

    m_frameStats.dynamicSurfaceStateUploadCpuMs +=
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
}

bool EntityMeshRenderer::initializeSurfaceWetnessResources()
{
    glGenTextures(1, &m_dynamicSurfacePageIndirectionTexture);
    if (m_dynamicSurfacePageIndirectionTexture)
    {
        glBindTexture(GL_TEXTURE_2D, m_dynamicSurfacePageIndirectionTexture);
        glTexStorage2D(
            GL_TEXTURE_2D,
            1,
            GL_R32I,
            kDynamicSurfaceIndirectionResolution,
            kDynamicSurfaceIndirectionResolution);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        const GLint zero = 0;
        glClearTexImage(
            m_dynamicSurfacePageIndirectionTexture,
            0,
            GL_RED_INTEGER,
            GL_INT,
            &zero);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    std::string breakupError;
    const Texture2D* breakup = m_textureCache.acquire(
        m_assetRoot / "Surface" / "Water" / "Water_ShorelineBreakup_A8.png",
        TextureColorSpace::Linear,
        1,
        false,
        breakupError);
    if (breakup)
    {
        m_surfaceWetnessBreakupTexture = breakup->id;
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glBindTexture(GL_TEXTURE_2D, m_surfaceWetnessBreakupTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    }
    else
    {
        std::cerr << "Heritage Dynamic Surface shoreline texture unavailable: "
                  << breakupError << '\n';
    }

    if (!m_dynamicSurfacePageIndirectionTexture)
    {
        std::cerr << "Heritage Dynamic Surface indirection texture allocation failed.\n";
        return false;
    }

    std::cout
        << "LIVETRACK03 water path ready: one 100m X/Z / 256x256 RGBA4 Hydro field (0.390625m/texel), no vertical water sheets, near 6Hz / distant 1-per-60s; "
        << "Track plane remains 64x64 sheet-aware; puddle optics fade to wet material by 100m; legacy 10m/512 CFD retired.\n";
    return true;
}

void EntityMeshRenderer::shutdownSurfaceWetnessResources()
{
    if (m_dynamicSurfacePageIndirectionTexture)
        glDeleteTextures(1, &m_dynamicSurfacePageIndirectionTexture);
    m_dynamicSurfacePageIndirectionTexture = 0;
    m_surfaceWetnessBreakupTexture = 0;
    m_dynamicSurfaceIndirectionTableGeneration = 0u;
    m_dynamicSurfaceIndirectionOriginGlobal = {};
    m_dynamicSurfaceIndirectionScratch.clear();
    m_dynamicSurfaceHydroScratch.clear();
    m_dynamicSurfaceTrackScratch.clear();
    m_dynamicSurfaceHydroUploadedStep.clear();
    m_dynamicSurfaceHydroUploadedGeneration.clear();
    m_dynamicSurfaceTrackUploadedStep.clear();
    m_dynamicSurfaceTrackUploadedGeneration.clear();
    m_dynamicSurfaceHydroLastRefreshSeconds.clear();
    m_dynamicSurfaceTrackLastRefreshSeconds.clear();
}

} // namespace heritage::graphics
