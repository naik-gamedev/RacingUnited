#include "SurfacePresentationRenderer.hpp"
#include "SurfacePresentationRendererInternal.hpp"
#include "SurfacePresentationShaders.hpp"

#include "../ShaderProgram.hpp"
#include "../LodTransitionPolicy.hpp"
#include "../PresentationPrecision.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace heritage::graphics {
using namespace surface_presentation_detail;
using namespace surface_presentation_shaders;
namespace {


using TrackVertex = SurfaceTrackVertex;
using ParticleVertex = SurfaceParticleVertex;

// Compact TIRE16K logical record uploaded once to a persistent GPU page.
// Positions are FP32 relative to a 100 m FP64 chunk origin; the authoritative
// SurfacePresentation history remains FP64. The geometry shader expands this

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

float dot(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

float length(const heritage::math::Vec3& value)
{
    return std::sqrt(dot(value, value));
}

heritage::math::Vec3 normalize(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitude = length(value);
    if (magnitude <= 1.0e-6f)
        return fallback;
    return { value.x / magnitude, value.y / magnitude, value.z / magnitude };
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

heritage::math::Mat4 lookAt(
    const heritage::math::Vec3& eye,
    const heritage::math::Vec3& target,
    const heritage::math::Vec3& up)
{
    const heritage::math::Vec3 forward = normalize(subtract(target, eye), { 0.0f, 0.0f, -1.0f });
    const heritage::math::Vec3 side = normalize(cross(forward, up), { 1.0f, 0.0f, 0.0f });
    const heritage::math::Vec3 correctedUp = cross(side, forward);

    heritage::math::Mat4 result = heritage::math::identity();
    result.m[0] = side.x;
    result.m[1] = correctedUp.x;
    result.m[2] = -forward.x;
    result.m[4] = side.y;
    result.m[5] = correctedUp.y;
    result.m[6] = -forward.y;
    result.m[8] = side.z;
    result.m[9] = correctedUp.z;
    result.m[10] = -forward.z;
    result.m[12] = -dot(side, eye);
    result.m[13] = -dot(correctedUp, eye);
    result.m[14] = dot(forward, eye);
    return result;
}

heritage::math::Vec3 trackCenterColor(heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    switch (material)
    {
    case SurfaceMaterial::Mud: return { 0.075f, 0.045f, 0.025f };
    case SurfaceMaterial::Sand: return { 0.36f, 0.27f, 0.14f };
    case SurfaceMaterial::DeepSnow: return { 0.58f, 0.66f, 0.72f };
    case SurfaceMaterial::SoftSoil: return { 0.12f, 0.075f, 0.035f };
    default: return { 0.09f, 0.08f, 0.07f };
    }
}

heritage::math::Vec3 trackShoulderColor(heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    switch (material)
    {
    case SurfaceMaterial::Mud: return { 0.16f, 0.09f, 0.045f };
    case SurfaceMaterial::Sand: return { 0.62f, 0.48f, 0.25f };
    case SurfaceMaterial::DeepSnow: return { 0.88f, 0.92f, 0.96f };
    case SurfaceMaterial::SoftSoil: return { 0.24f, 0.14f, 0.06f };
    default: return { 0.18f, 0.15f, 0.10f };
    }
}

heritage::math::Vec3 particleColor(
    heritage::physics::SurfaceParticleKind kind,
    heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    using heritage::physics::SurfaceParticleKind;
    switch (kind)
    {
    case SurfaceParticleKind::WaterSpray: return { 0.67f, 0.75f, 0.80f };
    case SurfaceParticleKind::Dust:
        return material == SurfaceMaterial::Sand
            ? heritage::math::Vec3{ 0.72f, 0.60f, 0.38f }
            : heritage::math::Vec3{ 0.52f, 0.43f, 0.30f };
    case SurfaceParticleKind::Mud: return { 0.13f, 0.075f, 0.035f };
    case SurfaceParticleKind::Snow: return { 0.90f, 0.94f, 0.98f };
    case SurfaceParticleKind::RubberShred: return { 0.002f, 0.002f, 0.002f };
    case SurfaceParticleKind::TireFailureSmoke: return { 0.48f, 0.49f, 0.50f };
    case SurfaceParticleKind::TireFailureDebris: return { 0.018f, 0.020f, 0.022f };
    case SurfaceParticleKind::LooseDebris:
    default:
        return material == SurfaceMaterial::Gravel
            ? heritage::math::Vec3{ 0.36f, 0.34f, 0.31f }
            : heritage::math::Vec3{ 0.29f, 0.21f, 0.13f };
    }
}

void appendTriangle(
    std::vector<TrackVertex>& vertices,
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b,
    const heritage::math::Vec3& c,
    const heritage::math::Vec3& color,
    float alpha)
{
    vertices.push_back({ a.x, a.y, a.z, color.x, color.y, color.z, alpha });
    vertices.push_back({ b.x, b.y, b.z, color.x, color.y, color.z, alpha });
    vertices.push_back({ c.x, c.y, c.z, color.x, color.y, color.z, alpha });
}

void appendQuad(
    std::vector<TrackVertex>& vertices,
    const heritage::math::Vec3& p0,
    const heritage::math::Vec3& p1,
    const heritage::math::Vec3& p2,
    const heritage::math::Vec3& p3,
    const heritage::math::Vec3& color,
    float alpha)
{
    appendTriangle(vertices, p0, p1, p2, color, alpha);
    appendTriangle(vertices, p0, p2, p3, color, alpha);
}

} // namespace

bool SurfacePresentationRenderer::initialize()
{
    shutdown();

    m_trackProgram = buildShaderProgram(kTrackVertexShader, kTrackFragmentShader);
    m_tireMarkProgram = buildShaderProgram(
        kTireMarkVertexShader, kTireMarkGeometryShader, kTireMarkFragmentShader);
    m_marbleProgram = buildShaderProgram(
        kMarbleVertexShader, kMarbleGeometryShader, kRubberFragmentShader);
    m_movingRubberProgram = buildShaderProgram(
        kMovingRubberVertexShader, kMovingRubberGeometryShader, kRubberFragmentShader);
    m_particleProgram = buildShaderProgram(kParticleVertexShader, kParticleFragmentShader);
    if (!m_trackProgram || !m_tireMarkProgram || !m_marbleProgram
        || !m_movingRubberProgram || !m_particleProgram)
    {
        shutdown();
        return false;
    }

    // PERF10: cache every presentation-program uniform location once. This
    // removes dozens of driver string lookups from each rendered view.
    m_trackUniformView = glGetUniformLocation(m_trackProgram, "uView");
    m_trackUniformProjection = glGetUniformLocation(m_trackProgram, "uProjection");
    m_trackUniformGamma = glGetUniformLocation(m_trackProgram, "uGamma");
    m_trackUniformBrightness = glGetUniformLocation(m_trackProgram, "uBrightness");
    m_trackUniformContrast = glGetUniformLocation(m_trackProgram, "uContrast");
    m_trackUniformSaturation = glGetUniformLocation(m_trackProgram, "uSaturation");

    m_tireMarkUniformView = glGetUniformLocation(m_tireMarkProgram, "uView");
    m_tireMarkUniformProjection = glGetUniformLocation(m_tireMarkProgram, "uProjection");
    m_tireMarkUniformPresentationTime = glGetUniformLocation(
        m_tireMarkProgram, "uPresentationTime");
    m_tireMarkUniformHistoryFloorBirthTime = glGetUniformLocation(
        m_tireMarkProgram, "uHistoryFloorBirthTime");
    m_tireMarkUniformRetirementSeconds = glGetUniformLocation(
        m_tireMarkProgram, "uRetirementSeconds");
    m_tireMarkUniformDetailedDistance = glGetUniformLocation(
        m_tireMarkProgram, "uDetailedDistance");
    m_tireMarkUniformLodBlendWidth = glGetUniformLocation(
        m_tireMarkProgram, "uLodBlendWidth");
    m_tireMarkUniformDrawDistance = glGetUniformLocation(
        m_tireMarkProgram, "uDrawDistance");
    m_tireMarkUniformVisibilityFadeWidth = glGetUniformLocation(
        m_tireMarkProgram, "uVisibilityFadeWidth");
    m_tireMarkUniformCapDistance = glGetUniformLocation(
        m_tireMarkProgram, "uCapDistance");
    m_tireMarkUniformChunkOriginRelative = glGetUniformLocation(
        m_tireMarkProgram, "uChunkOriginRelative");

    m_marbleUniformView = glGetUniformLocation(m_marbleProgram, "uView");
    m_marbleUniformProjection = glGetUniformLocation(m_marbleProgram, "uProjection");
    m_marbleUniformDetailedDistance = glGetUniformLocation(
        m_marbleProgram, "uDetailedDistance");
    m_marbleUniformLodBlendWidth = glGetUniformLocation(
        m_marbleProgram, "uLodBlendWidth");
    m_marbleUniformDrawDistance = glGetUniformLocation(
        m_marbleProgram, "uDrawDistance");
    m_marbleUniformVisibilityFadeWidth = glGetUniformLocation(
        m_marbleProgram, "uVisibilityFadeWidth");
    m_marbleUniformChunkOriginRelative = glGetUniformLocation(
        m_marbleProgram, "uChunkOriginRelative");

    m_movingRubberUniformView = glGetUniformLocation(
        m_movingRubberProgram, "uView");
    m_movingRubberUniformProjection = glGetUniformLocation(
        m_movingRubberProgram, "uProjection");
    m_movingRubberUniformDrawDistance = glGetUniformLocation(
        m_movingRubberProgram, "uDrawDistance");
    m_movingRubberUniformVisibilityFadeWidth = glGetUniformLocation(
        m_movingRubberProgram, "uVisibilityFadeWidth");

    m_particleUniformView = glGetUniformLocation(m_particleProgram, "uView");
    m_particleUniformProjection = glGetUniformLocation(
        m_particleProgram, "uProjection");

    // PERF10: allocate transient staging capacity once, not once per frame.
    // 240k * 28-byte TrackVertex was a ~6.7 MB (~6.4 MiB) reserve/free cycle per view.
    m_trackVertexScratch.clear();
    m_trackVertexScratch.reserve(240000);
    m_particleVertexScratch.clear();
    m_particleVertexScratch.reserve(1024);
    m_marbleCellScratch.clear();
    m_movingRubberPacketScratch.clear();

    glGenVertexArrays(1, &m_trackVao);
    glGenBuffers(1, &m_trackVbo);
    glBindVertexArray(m_trackVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_trackVbo);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrackVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, sizeof(TrackVertex),
        reinterpret_cast<const void*>(sizeof(float) * 3));

    glGenVertexArrays(1, &m_particleVao);
    glGenBuffers(1, &m_particleVbo);
    glBindVertexArray(m_particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
        reinterpret_cast<const void*>(sizeof(float) * 3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
        reinterpret_cast<const void*>(sizeof(float) * 7));

    glGenVertexArrays(1, &m_movingRubberVao);
    glGenBuffers(1, &m_movingRubberVbo);
    glBindVertexArray(m_movingRubberVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_movingRubberVbo);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
    {
        const GLsizei stride = static_cast<GLsizei>(sizeof(MovingRubberGpuRecord));
        const auto attribute = [stride](GLuint index, GLint count, std::size_t offset) {
            glEnableVertexAttribArray(index);
            glVertexAttribPointer(
                index, count, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const void*>(offset));
        };
        attribute(0, 3, offsetof(MovingRubberGpuRecord, centerRelative));
        attribute(1, 3, offsetof(MovingRubberGpuRecord, axisRight));
        attribute(2, 3, offsetof(MovingRubberGpuRecord, axisForward));
        attribute(3, 3, offsetof(MovingRubberGpuRecord, axisNormal));
        attribute(4, 4, offsetof(MovingRubberGpuRecord, shape));
        attribute(5, 4, offsetof(MovingRubberGpuRecord, state));
        attribute(6, 4, offsetof(MovingRubberGpuRecord, misc));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    // WATER12: settled-water parcel rendering is retired from the active
    // presentation path, so do not allocate its 65k-particle SSBOs/FBOs at
    // startup. The class stays in-tree for future detached spray/splash work.
    return true;
}

void SurfacePresentationRenderer::shutdown()
{
    clearTireMarkGpuCache();
    clearMarbleGpuCache();
    if (m_trackVbo)
        glDeleteBuffers(1, &m_trackVbo);
    if (m_trackVao)
        glDeleteVertexArrays(1, &m_trackVao);
    if (m_particleVbo)
        glDeleteBuffers(1, &m_particleVbo);
    if (m_particleVao)
        glDeleteVertexArrays(1, &m_particleVao);
    if (m_movingRubberVbo)
        glDeleteBuffers(1, &m_movingRubberVbo);
    if (m_movingRubberVao)
        glDeleteVertexArrays(1, &m_movingRubberVao);
    if (m_trackProgram)
        glDeleteProgram(m_trackProgram);
    if (m_tireMarkProgram)
        glDeleteProgram(m_tireMarkProgram);
    if (m_marbleProgram)
        glDeleteProgram(m_marbleProgram);
    if (m_movingRubberProgram)
        glDeleteProgram(m_movingRubberProgram);
    if (m_particleProgram)
        glDeleteProgram(m_particleProgram);
    m_trackVbo = 0;
    m_trackVao = 0;
    std::vector<SurfaceTrackVertex>().swap(m_trackVertexScratch);
    std::vector<SurfaceParticleVertex>().swap(m_particleVertexScratch);
    std::vector<heritage::physics::rubber::TrackRubberVisualCell>().swap(
        m_marbleCellScratch);
    std::vector<heritage::physics::rubber::TrackRubberTransientVisual>().swap(
        m_movingRubberPacketScratch);
    m_particleVbo = 0;
    m_particleVao = 0;
    m_movingRubberVbo = 0;
    m_movingRubberVao = 0;
    m_movingRubberCapacity = 0;
    m_trackProgram = 0;
    m_trackUniformView = -1;
    m_trackUniformProjection = -1;
    m_trackUniformGamma = -1;
    m_trackUniformBrightness = -1;
    m_trackUniformContrast = -1;
    m_trackUniformSaturation = -1;
    m_tireMarkUniformView = -1;
    m_tireMarkUniformProjection = -1;
    m_tireMarkUniformPresentationTime = -1;
    m_tireMarkUniformHistoryFloorBirthTime = -1;
    m_tireMarkUniformRetirementSeconds = -1;
    m_tireMarkUniformDetailedDistance = -1;
    m_tireMarkUniformLodBlendWidth = -1;
    m_tireMarkUniformDrawDistance = -1;
    m_tireMarkUniformVisibilityFadeWidth = -1;
    m_tireMarkUniformCapDistance = -1;
    m_tireMarkUniformChunkOriginRelative = -1;
    m_marbleUniformView = -1;
    m_marbleUniformProjection = -1;
    m_marbleUniformDetailedDistance = -1;
    m_marbleUniformLodBlendWidth = -1;
    m_marbleUniformDrawDistance = -1;
    m_marbleUniformVisibilityFadeWidth = -1;
    m_marbleUniformChunkOriginRelative = -1;
    m_movingRubberUniformView = -1;
    m_movingRubberUniformProjection = -1;
    m_movingRubberUniformDrawDistance = -1;
    m_movingRubberUniformVisibilityFadeWidth = -1;
    m_particleUniformView = -1;
    m_particleUniformProjection = -1;
    m_tireMarkProgram = 0;
    m_marbleProgram = 0;
    m_movingRubberProgram = 0;
    m_particleProgram = 0;
    m_frameStats = {};
}

void SurfacePresentationRenderer::draw(
    const heritage::physics::SurfaceWorld& surfaces,
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings,
    const heritage::camera::CameraFrame& cameraFrame,
    const EnvironmentMap& environmentMap) const
{
    // WATER15 moved settled-water environment reflection to EntityMeshRenderer.
    // Keep the shared draw signature stable for other presentation callers.
    (void)environmentMap;
    if (!m_trackProgram || !m_tireMarkProgram || !m_marbleProgram
        || !m_movingRubberProgram || !m_particleProgram)
        return;

    const heritage::math::Vec3 eyeLocal = cameraFrame.valid
        ? cameraFrame.eyeLocal
        : heritage::math::Vec3{ 0.0f, 3.4f, 8.5f };
    const heritage::math::Vec3 targetLocal = cameraFrame.valid
        ? cameraFrame.targetLocal
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const heritage::math::Vec3 cameraUp = cameraFrame.valid
        ? cameraFrame.up
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const heritage::math::Vec3 cameraRelativeTarget = subtract(targetLocal, eyeLocal);
    const heritage::math::Mat4 view = lookAt(
        { 0.0f, 0.0f, 0.0f }, cameraRelativeTarget, cameraUp);
    const heritage::math::DVec3 cameraGlobal = surfaces.localToGlobal(eyeLocal);

    auto& trackVertices = m_trackVertexScratch;
    trackVertices.clear();
    constexpr double kTrackDrawDistanceM = 85.0;
    constexpr double kTrackDrawDistanceSquared =
        kTrackDrawDistanceM * kTrackDrawDistanceM;

    // TIRE16K: old tire-mark history is no longer scanned, distance-sorted,
    // tessellated and copied into one giant dynamic VBO every frame. Only new
    // serials are appended once to 100 m persistent GPU-cache pages. The GPU
    // performs per-segment age/range rejection and expands the six-control
    // pressure ribbon from the compact logical record.
    syncTireMarkGpuCache(surfaces.presentation());

    for (const heritage::physics::SurfaceTrackMark& mark
        : surfaces.presentation().trackMarks())
    {
        if (mark.updateSerial == 0 || mark.intensity <= 0.001f)
            continue;
        const double dx = mark.globalPosition.x - cameraGlobal.x;
        const double dy = mark.globalPosition.y - cameraGlobal.y;
        const double dz = mark.globalPosition.z - cameraGlobal.z;
        const double distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared > kTrackDrawDistanceSquared)
            continue;
        const float rangeVisibility = heritage::graphics::lod::visibilityWeight(
            static_cast<float>(std::sqrt(distanceSquared)),
            static_cast<float>(kTrackDrawDistanceM));
        if (rangeVisibility <= 0.0001f)
            continue;

        heritage::math::Vec3 normal = normalize(
            mark.normal, { 0.0f, 1.0f, 0.0f });
        heritage::math::Vec3 forward = normalize(
            mark.forward, { 0.0f, 0.0f, 1.0f });
        heritage::math::Vec3 right = normalize(
            cross(normal, forward), { 1.0f, 0.0f, 0.0f });
        forward = normalize(cross(right, normal), forward);

        const float rutVisual = std::clamp(mark.rutDepthM, 0.0f, 0.16f);
        const float surfaceLift = 0.007f + rutVisual * 0.04f;
        const float shoulderLift = surfaceLift + std::min(rutVisual * 0.35f, 0.028f);
        heritage::math::Vec3 center = heritage::graphics::presentation::cameraRelativeFp32(mark.globalPosition, cameraGlobal);
        center = add(center, scale(normal, surfaceLift));

        const float halfLength = std::max(mark.lengthM * 0.5f, 0.10f);
        const float halfWidth = std::max(mark.widthM * 0.5f, 0.04f);
        const float centerHalfWidth = halfWidth * 0.68f;
        const float alpha = (0.20f + 0.43f * std::clamp(mark.intensity, 0.0f, 1.0f))
            * rangeVisibility;

        const auto point = [&](float longitudinal, float lateral, float lift) {
            return add(
                center,
                add(
                    scale(forward, longitudinal),
                    add(scale(right, lateral), scale(normal, lift))));
        };

        const heritage::math::Vec3 centerColor = trackCenterColor(mark.material);
        const heritage::math::Vec3 shoulderColor = trackShoulderColor(mark.material);

        appendQuad(
            trackVertices,
            point(-halfLength, -centerHalfWidth, 0.0f),
            point(halfLength, -centerHalfWidth, 0.0f),
            point(halfLength, centerHalfWidth, 0.0f),
            point(-halfLength, centerHalfWidth, 0.0f),
            centerColor,
            alpha);
        appendQuad(
            trackVertices,
            point(-halfLength, -halfWidth, shoulderLift),
            point(halfLength, -halfWidth, shoulderLift),
            point(halfLength, -centerHalfWidth, 0.0f),
            point(-halfLength, -centerHalfWidth, 0.0f),
            shoulderColor,
            alpha * 0.75f);
        appendQuad(
            trackVertices,
            point(-halfLength, centerHalfWidth, 0.0f),
            point(halfLength, centerHalfWidth, 0.0f),
            point(halfLength, halfWidth, shoulderLift),
            point(-halfLength, halfWidth, shoulderLift),
            shoulderColor,
            alpha * 0.75f);
        ++m_frameStats.visibleTrackMarks;
    }

    // OPT03C: production CPU Hydro has been retired. Surface presentation keeps
    // only immutable .hhyd topology metadata here; live water/tire telemetry is
    // owned by DynamicSurfaceGpuRuntime and reported by the renderer/F8 path.
    const auto& topologyStats = surfaces.hydrology().stats();
    m_frameStats.waterHydrologyStepMs = 0.0;
    m_frameStats.waterHydrologyHz = 0.0;
    m_frameStats.waterWetCells = 0u;
    m_frameStats.waterTotalCells = topologyStats.supportCellCount;
    m_frameStats.waterSupportCells = topologyStats.supportCellCount;
    m_frameStats.waterSimulationMinimumCellM = 0.0;
    m_frameStats.waterSimulationMaximumCellM = 0.0;
    m_frameStats.waterPresentationBasins = topologyStats.prebakedWorldTileCount;
    m_frameStats.waterActivePresentationBasins = 0u;
    m_frameStats.waterInterestSources =
        surfaces.dynamicSurface().interestSources().size();
    m_frameStats.waterCadence30Cells = 0u;
    m_frameStats.waterCadence20Cells = 0u;
    m_frameStats.waterCadence6Cells = 0u;
    m_frameStats.waterCadence2Cells = 0u;
    m_frameStats.waterCadenceBackgroundCells = 0u;
    m_frameStats.waterScheduledCells = 0u;
    m_frameStats.waterMaximumFlowSpeedMps = 0.0;

    // DSURF04: Track.R is now the persistent, sheet-aware road-temperature
    // authority. SurfaceWeather's scalar road temperature is only an
    // environmental compatibility/reference value after this cutover.
    const auto& thermalStats = surfaces.dynamicSurface().thermalStats();
    m_frameStats.surfaceThermalStepMs = thermalStats.lastStepMilliseconds;
    m_frameStats.surfaceThermalCells = thermalStats.validTexels;
    m_frameStats.surfaceTemperatureMinimumC = thermalStats.minimumTemperatureC;
    m_frameStats.surfaceTemperatureAverageC = thermalStats.averageTemperatureC;
    m_frameStats.surfaceTemperatureMaximumC = thermalStats.maximumTemperatureC;
    m_frameStats.surfaceThermalTireContacts = thermalStats.tireContactCount;

    // TIRE16L: resting marbles are synchronized as compact persistent GPU
    // cell records. Moving packets remain authoritative CPU simulation but
    // are uploaded one-record-per-packet and expanded into flakes on the GPU.
    // Neither path contributes per-flake CPU triangles to trackVertices.
    syncMarbleGpuCache(surfaces, cameraGlobal);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    // WATER15: no second settled-water geometry pass exists here. Wetness,
    // drying lines, puddles, pooling, reflections and ripples are composited
    // over the original scene surface by EntityMeshRenderer. This permanently
    // removes the WATER08-WATER14 water-ring mesh from depth ownership.

    // TIRE16K persistent GPU tire-mark pages. No per-frame history vector,
    // sorting, ribbon tessellation or giant dynamic-VBO upload remains here.
    drawTireMarkGpuCache(
        surfaces.presentation(), view, projection, cameraGlobal);
    drawMarbleGpuCache(view, projection, cameraGlobal);
    drawMovingRubberGpu(surfaces, view, projection, cameraGlobal);

    if (!trackVertices.empty())
    {
        glUseProgram(m_trackProgram);
        glUniformMatrix4fv(
            m_trackUniformView,
            1, GL_FALSE, view.m);
        glUniformMatrix4fv(
            m_trackUniformProjection,
            1, GL_FALSE, projection.m);
        glUniform1f(m_trackUniformGamma, videoSettings.gamma);
        glUniform1f(m_trackUniformBrightness, videoSettings.brightness);
        glUniform1f(m_trackUniformContrast, videoSettings.contrast);
        glUniform1f(m_trackUniformSaturation, videoSettings.saturation);
        glBindVertexArray(m_trackVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_trackVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(trackVertices.size() * sizeof(TrackVertex)),
            trackVertices.data(),
            GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(trackVertices.size()));
        ++m_frameStats.drawCalls;
        m_frameStats.trackTriangles += trackVertices.size() / 3;
    }

    auto& particleVertices = m_particleVertexScratch;
    particleVertices.clear();
    constexpr double kParticleDrawDistanceM = 110.0;
    constexpr double kParticleDrawDistanceSquared =
        kParticleDrawDistanceM * kParticleDrawDistanceM;
    for (const heritage::physics::SurfacePresentationParticle& particle
        : surfaces.presentation().particles())
    {
        if (particle.kind == heritage::physics::SurfaceParticleKind::RubberShred)
            continue;
        if (particle.ageSeconds >= particle.lifetimeSeconds)
            continue;
        const double dx = particle.globalPosition.x - cameraGlobal.x;
        const double dy = particle.globalPosition.y - cameraGlobal.y;
        const double dz = particle.globalPosition.z - cameraGlobal.z;
        const double distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared > kParticleDrawDistanceSquared)
            continue;
        const float distance = static_cast<float>(std::sqrt(distanceSquared));
        const float rangeVisibility = heritage::graphics::lod::visibilityWeight(
            distance, static_cast<float>(kParticleDrawDistanceM));
        if (rangeVisibility <= 0.0001f)
            continue;

        const float life = particle.lifetimeSeconds > 0.0001f
            ? std::clamp(
                1.0f - particle.ageSeconds / particle.lifetimeSeconds,
                0.0f, 1.0f)
            : 0.0f;
        const heritage::math::Vec3 relative = heritage::graphics::presentation::cameraRelativeFp32(
            particle.globalPosition, cameraGlobal);
        const heritage::math::Vec3 color = particleColor(
            particle.kind, particle.material);
        const bool failureSmoke = particle.kind
            == heritage::physics::SurfaceParticleKind::TireFailureSmoke;
        const bool failureDebris = particle.kind
            == heritage::physics::SurfaceParticleKind::TireFailureDebris;
        const float pointSize = std::clamp(
            particle.sizeM * 600.0f / std::max(distance, 1.0f),
            2.0f,
            particle.kind == heritage::physics::SurfaceParticleKind::Dust
                ? 22.0f
                : (failureSmoke ? 42.0f : (failureDebris ? 30.0f : 15.0f)));
        particleVertices.push_back({
            relative.x, relative.y, relative.z,
            color.x, color.y, color.z,
            particle.opacity * life * rangeVisibility,
            pointSize
        });
        ++m_frameStats.visibleParticles;
    }

    if (!particleVertices.empty())
    {
        glUseProgram(m_particleProgram);
        glUniformMatrix4fv(
            m_particleUniformView,
            1, GL_FALSE, view.m);
        glUniformMatrix4fv(
            m_particleUniformProjection,
            1, GL_FALSE, projection.m);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glBindVertexArray(m_particleVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(particleVertices.size() * sizeof(ParticleVertex)),
            particleVertices.data(),
            GL_DYNAMIC_DRAW);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(particleVertices.size()));
        glDisable(GL_PROGRAM_POINT_SIZE);
        ++m_frameStats.drawCalls;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

} // namespace heritage::graphics
