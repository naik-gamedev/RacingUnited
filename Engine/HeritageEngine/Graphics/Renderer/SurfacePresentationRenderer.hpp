#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include <glad/glad.h>

#include "../../Camera/ChaseCamera.hpp"
#include "../../Core/Math/Math.hpp"
#include "../../Core/Settings/VideoSettings.hpp"
#include "../EnvironmentMap.hpp"
#include "../TireMarkChunking.hpp"
#include "../../Physics/Surfaces/SurfaceWorld.hpp"

namespace heritage::graphics {

struct SurfaceTrackVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct SurfaceParticleVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
    float pointSize = 4.0f;
};

struct SurfacePresentationRendererStats
{
    std::uint64_t drawCalls = 0;
    std::uint64_t trackTriangles = 0;
    std::uint64_t visibleTrackMarks = 0;
    std::uint64_t visibleTireMarkSegments = 0;
    std::uint64_t visibleParticles = 0;
    std::uint64_t visibleRubberCells = 0;
    std::uint64_t visibleMarbles = 0;
    std::uint64_t visibleMovingRubber = 0;
    double waterHydrologyStepMs = 0.0;
    double waterHydrologyHz = 0.0;
    std::uint64_t waterWetCells = 0;
    std::uint64_t waterTotalCells = 0;
    std::uint64_t waterSupportCells = 0;
    double waterSimulationMinimumCellM = 0.0;
    double waterSimulationMaximumCellM = 0.0;
    std::uint64_t waterInterestSources = 0;
    std::uint64_t waterCadence30Cells = 0;
    std::uint64_t waterCadence20Cells = 0;
    std::uint64_t waterCadence6Cells = 0;
    std::uint64_t waterCadence2Cells = 0;
    std::uint64_t waterCadenceBackgroundCells = 0;
    std::uint64_t waterScheduledCells = 0;
    double waterMaximumFlowSpeedMps = 0.0;
    std::uint64_t waterPresentationBasins = 0;
    std::uint64_t waterActivePresentationBasins = 0;
    double surfaceThermalStepMs = 0.0;
    std::uint64_t surfaceThermalCells = 0;
    double surfaceTemperatureMinimumC = 0.0;
    double surfaceTemperatureAverageC = 0.0;
    double surfaceTemperatureMaximumC = 0.0;
    std::uint64_t surfaceThermalTireContacts = 0;

};

// Driven-surface renderer for visual evidence of authoritative SurfaceWorld
// state. TIRE16K keeps tire marks in persistent 100 m GPU-cache pages so old
// skid history is not re-tessellated or re-uploaded every frame. Chunks are an
// invisible batching mechanism only and never influence physics or mark shape.
class SurfacePresentationRenderer
{
public:
    bool initialize();
    void shutdown();

    void beginFrameStats() { m_frameStats = {}; }
    const SurfacePresentationRendererStats& frameStats() const { return m_frameStats; }

    void draw(
        const heritage::physics::SurfaceWorld& surfaces,
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings,
        const heritage::camera::CameraFrame& cameraFrame,
        const EnvironmentMap& environmentMap) const;

private:

    struct TireMarkGpuPage
    {
        GLuint vao = 0;
        GLuint vbo = 0;
        std::uint32_t segmentCount = 0;
        std::uint32_t capacitySegments = 0;
        double minimumBirthTimeSeconds = 0.0;
        double maximumBirthTimeSeconds = 0.0;
        std::size_t pendingByteOffset = 0;
        std::vector<std::uint8_t> pendingUpload;
    };

    struct TireMarkGpuChunk
    {
        heritage::math::DVec3 globalOrigin{ 0.0, 0.0, 0.0 };
        std::vector<TireMarkGpuPage> pages;
    };

    struct TireMarkGpuTailLocation
    {
        heritage::graphics::tiremarks::ChunkAddress address{};
        GLuint vbo = 0;
        std::uint32_t recordIndex = 0;
        std::uint32_t flags = 0;
        double birthTimeSeconds = 0.0;
    };

    // TIRE16L resting marble presentation uses the same invisible 100 m
    // presentation-chunk philosophy as tire marks, but stores one compact cell
    // record rather than reconstructing dozens of flakes on the CPU each frame.
    struct MarbleGpuCellKey
    {
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t z = 0;
        std::uint8_t material = 0;

        bool operator<(const MarbleGpuCellKey& other) const
        {
            if (x != other.x) return x < other.x;
            if (y != other.y) return y < other.y;
            if (z != other.z) return z < other.z;
            return material < other.material;
        }
    };

    struct MarbleGpuPage
    {
        GLuint vao = 0;
        GLuint vbo = 0;
        std::uint32_t cellCount = 0;
        std::uint32_t capacityCells = 0;
        std::vector<std::uint8_t> cpuMirror;
        std::size_t dirtyBeginByte = static_cast<std::size_t>(-1);
        std::size_t dirtyEndByte = 0;
    };

    struct MarbleGpuChunk
    {
        heritage::math::DVec3 globalOrigin{ 0.0, 0.0, 0.0 };
        std::vector<MarbleGpuPage> pages;
    };

    struct MarbleGpuLocation
    {
        heritage::graphics::tiremarks::ChunkAddress address{};
        std::size_t pageIndex = 0;
        std::uint32_t recordIndex = 0;
        std::uint64_t updateSerial = 0;
        std::uint64_t visualSignature = 0;
    };

    void clearTireMarkGpuCache() const;
    void syncTireMarkGpuCache(
        const heritage::physics::SurfacePresentation& presentation) const;
    void clearTireMarkGpuEndFeather(std::uint64_t serial) const;
    void drawTireMarkGpuCache(
        const heritage::physics::SurfacePresentation& presentation,
        const heritage::math::Mat4& view,
        const heritage::math::Mat4& projection,
        const heritage::math::DVec3& cameraGlobal) const;

    void clearMarbleGpuCache() const;
    void syncMarbleGpuCache(
        const heritage::physics::SurfaceWorld& surfaces,
        const heritage::math::DVec3& cameraGlobal) const;
    void drawMarbleGpuCache(
        const heritage::math::Mat4& view,
        const heritage::math::Mat4& projection,
        const heritage::math::DVec3& cameraGlobal) const;
    void drawMovingRubberGpu(
        const heritage::physics::SurfaceWorld& surfaces,
        const heritage::math::Mat4& view,
        const heritage::math::Mat4& projection,
        const heritage::math::DVec3& cameraGlobal) const;

    GLuint m_trackProgram = 0;

    // PERF10: all surface-presentation uniform locations are immutable program
    // metadata. Cache them once during initialize() instead of asking the GL
    // driver to resolve strings every frame.
    GLint m_trackUniformView = -1;
    GLint m_trackUniformProjection = -1;
    GLint m_trackUniformGamma = -1;
    GLint m_trackUniformBrightness = -1;
    GLint m_trackUniformContrast = -1;
    GLint m_trackUniformSaturation = -1;

    GLint m_tireMarkUniformView = -1;
    GLint m_tireMarkUniformProjection = -1;
    GLint m_tireMarkUniformPresentationTime = -1;
    GLint m_tireMarkUniformHistoryFloorBirthTime = -1;
    GLint m_tireMarkUniformRetirementSeconds = -1;
    GLint m_tireMarkUniformDetailedDistance = -1;
    GLint m_tireMarkUniformLodBlendWidth = -1;
    GLint m_tireMarkUniformDrawDistance = -1;
    GLint m_tireMarkUniformVisibilityFadeWidth = -1;
    GLint m_tireMarkUniformCapDistance = -1;
    GLint m_tireMarkUniformChunkOriginRelative = -1;

    GLint m_marbleUniformView = -1;
    GLint m_marbleUniformProjection = -1;
    GLint m_marbleUniformDetailedDistance = -1;
    GLint m_marbleUniformLodBlendWidth = -1;
    GLint m_marbleUniformDrawDistance = -1;
    GLint m_marbleUniformVisibilityFadeWidth = -1;
    GLint m_marbleUniformChunkOriginRelative = -1;

    GLint m_movingRubberUniformView = -1;
    GLint m_movingRubberUniformProjection = -1;
    GLint m_movingRubberUniformDrawDistance = -1;
    GLint m_movingRubberUniformVisibilityFadeWidth = -1;

    GLint m_particleUniformView = -1;
    GLint m_particleUniformProjection = -1;
    GLuint m_tireMarkProgram = 0;
    GLuint m_marbleProgram = 0;
    GLuint m_movingRubberProgram = 0;
    GLuint m_particleProgram = 0;
    GLuint m_trackVao = 0;
    GLuint m_trackVbo = 0;

    // PERF10: transient CPU staging is renderer-owned and reused. The previous
    // draw path allocated/freed a ~6.7 MB (~6.4 MiB) 240k TrackVertex reserve every
    // rendered frame even when there were no debug track quads to upload.
    mutable std::vector<SurfaceTrackVertex> m_trackVertexScratch;
    mutable std::vector<SurfaceParticleVertex> m_particleVertexScratch;
    mutable std::vector<heritage::physics::rubber::TrackRubberVisualCell>
        m_marbleCellScratch;
    mutable std::vector<heritage::physics::rubber::TrackRubberTransientVisual>
        m_movingRubberPacketScratch;

    GLuint m_particleVao = 0;
    GLuint m_particleVbo = 0;
    GLuint m_movingRubberVao = 0;
    GLuint m_movingRubberVbo = 0;
    mutable std::uint32_t m_movingRubberCapacity = 0;
    mutable std::map<heritage::graphics::tiremarks::ChunkAddress, TireMarkGpuChunk>
        m_tireMarkGpuChunks;
    mutable std::map<std::uint64_t, TireMarkGpuTailLocation> m_tireMarkGpuTailLocations;
    mutable std::map<heritage::graphics::tiremarks::ChunkAddress, MarbleGpuChunk>
        m_marbleGpuChunks;
    mutable std::map<MarbleGpuCellKey, MarbleGpuLocation> m_marbleGpuLocations;
    mutable std::size_t m_lastMarbleCellCount = 0;
    mutable std::size_t m_lastMarbleResidentChunkCount = 0;
    mutable std::uint64_t m_lastCachedTireMarkSerial = 0;
    mutable double m_lastTireMarkCachePresentationTime = -1.0;
    mutable SurfacePresentationRendererStats m_frameStats{};
};

} // namespace heritage::graphics
