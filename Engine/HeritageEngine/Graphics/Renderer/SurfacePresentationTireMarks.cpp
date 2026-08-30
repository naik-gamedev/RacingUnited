#include "SurfacePresentationRenderer.hpp"
#include "SurfacePresentationRendererInternal.hpp"

#include "../LodTransitionPolicy.hpp"
#include "../PresentationPrecision.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace heritage::graphics {
using namespace surface_presentation_detail;

void SurfacePresentationRenderer::clearTireMarkGpuCache() const
{
    for (auto& [address, chunk] : m_tireMarkGpuChunks)
    {
        (void)address;
        for (TireMarkGpuPage& page : chunk.pages)
        {
            if (page.vbo != 0)
                glDeleteBuffers(1, &page.vbo);
            if (page.vao != 0)
                glDeleteVertexArrays(1, &page.vao);
        }
    }
    m_tireMarkGpuChunks.clear();
    m_tireMarkGpuTailLocations.clear();
    m_lastCachedTireMarkSerial = 0;
    m_lastTireMarkCachePresentationTime = -1.0;
}

void SurfacePresentationRenderer::clearTireMarkGpuEndFeather(
    std::uint64_t serial) const
{
    const auto tailIt = m_tireMarkGpuTailLocations.find(serial);
    if (tailIt == m_tireMarkGpuTailLocations.end())
        return;

    const TireMarkGpuTailLocation location = tailIt->second;
    const std::uint32_t updatedFlags = location.flags & ~2u;
    const float updatedFlagsFloat = static_cast<float>(updatedFlags);
    constexpr std::size_t kFlagByteOffset =
        offsetof(TireMarkGpuRecord, misc) + sizeof(float) * 3;

    const auto chunkIt = m_tireMarkGpuChunks.find(location.address);
    if (chunkIt != m_tireMarkGpuChunks.end())
    {
        for (TireMarkGpuPage& page : chunkIt->second.pages)
        {
            if (page.vbo != location.vbo)
                continue;

            const std::size_t recordByteOffset =
                static_cast<std::size_t>(location.recordIndex) * sizeof(TireMarkGpuRecord);
            const std::size_t fieldByteOffset = recordByteOffset + kFlagByteOffset;
            const std::size_t pendingBegin = page.pendingByteOffset;
            const std::size_t pendingEnd = pendingBegin + page.pendingUpload.size();

            if (!page.pendingUpload.empty()
                && fieldByteOffset >= pendingBegin
                && fieldByteOffset + sizeof(float) <= pendingEnd)
            {
                const std::size_t localOffset = fieldByteOffset - pendingBegin;
                std::memcpy(
                    page.pendingUpload.data() + localOffset,
                    &updatedFlagsFloat,
                    sizeof(updatedFlagsFloat));
            }
            else
            {
                glBindBuffer(GL_ARRAY_BUFFER, page.vbo);
                glBufferSubData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLintptr>(fieldByteOffset),
                    static_cast<GLsizeiptr>(sizeof(updatedFlagsFloat)),
                    &updatedFlagsFloat);
            }
            break;
        }
    }

    // Whether or not the page survived a cache/history transition, this serial
    // is no longer a trail tail once a successor exists.
    m_tireMarkGpuTailLocations.erase(tailIt);
}

void SurfacePresentationRenderer::syncTireMarkGpuCache(
    const heritage::physics::SurfacePresentation& presentation) const
{
    const double presentationTime = presentation.elapsedSeconds();
    const std::uint64_t firstSerial = presentation.firstTireMarkSerial();
    const std::uint64_t lastSerial = presentation.lastTireMarkSerial();

    // Reset/scene reload: SurfacePresentation restarts both elapsed time and
    // serials. Drop GPU pages rather than accidentally associating old buffers
    // with a new world's marks.
    if (presentationTime + 1.0e-6 < m_lastTireMarkCachePresentationTime
        || (m_lastCachedTireMarkSerial != 0
            && lastSerial != 0
            && lastSerial < m_lastCachedTireMarkSerial))
    {
        clearTireMarkGpuCache();
    }

    if (lastSerial == 0)
    {
        if (!m_tireMarkGpuChunks.empty())
            clearTireMarkGpuCache();
        m_lastTireMarkCachePresentationTime = presentationTime;
        return;
    }

    // Retire whole GPU pages once every record on that page is guaranteed to
    // be gone from the authoritative one-million / twenty-minute history. A
    // partially old page remains resident and its stale records are discarded
    // in the geometry shader, avoiding CPU-side compaction/rebuild work.
    double historyFloorBirthTime = -std::numeric_limits<double>::infinity();
    if (!presentation.tireMarkSegments().empty())
        historyFloorBirthTime = presentation.tireMarkSegments().front().birthTimeSeconds;
    const double expiryBirthTime = std::max(
        presentationTime - heritage::physics::SurfacePresentation::kTireMarkRetirementSeconds,
        historyFloorBirthTime - 1.0e-6);

    for (auto tailIt = m_tireMarkGpuTailLocations.begin();
         tailIt != m_tireMarkGpuTailLocations.end();)
    {
        if (tailIt->second.birthTimeSeconds < expiryBirthTime)
            tailIt = m_tireMarkGpuTailLocations.erase(tailIt);
        else
            ++tailIt;
    }

    for (auto chunkIt = m_tireMarkGpuChunks.begin();
         chunkIt != m_tireMarkGpuChunks.end();)
    {
        auto& pages = chunkIt->second.pages;
        for (auto pageIt = pages.begin(); pageIt != pages.end();)
        {
            if (pageIt->segmentCount > 0
                && pageIt->maximumBirthTimeSeconds < expiryBirthTime)
            {
                if (pageIt->vbo != 0)
                    glDeleteBuffers(1, &pageIt->vbo);
                if (pageIt->vao != 0)
                    glDeleteVertexArrays(1, &pageIt->vao);
                pageIt = pages.erase(pageIt);
            }
            else
            {
                ++pageIt;
            }
        }
        if (pages.empty())
            chunkIt = m_tireMarkGpuChunks.erase(chunkIt);
        else
            ++chunkIt;
    }

    std::uint64_t nextSerial = m_lastCachedTireMarkSerial == 0
        ? firstSerial
        : m_lastCachedTireMarkSerial + 1;
    if (nextSerial < firstSerial)
        nextSerial = firstSerial;

    for (std::uint64_t serial = nextSerial; serial <= lastSerial; ++serial)
    {
        const heritage::physics::SurfaceTireMarkSegment* mark =
            presentation.tireMarkSegmentBySerial(serial);
        if (mark == nullptr || mark->serial == 0)
            continue;

        // The CPU presentation initially marks a freshly emitted segment as a
        // possible trail tail. If a successor arrives, clear only that prior
        // segment's provisional end-feather flag. This is a four-byte GPU patch,
        // not a geometry rebuild/re-upload, and preserves continuous longitudinal
        // darkness without the 11.5 cm "barcode" overlap from TIRE16K.
        if (mark->previousSegmentSerial != 0)
            clearTireMarkGpuEndFeather(mark->previousSegmentSerial);

        const heritage::math::DVec3 midpoint{
            (mark->startGlobalPosition.x + mark->endGlobalPosition.x) * 0.5,
            (mark->startGlobalPosition.y + mark->endGlobalPosition.y) * 0.5,
            (mark->startGlobalPosition.z + mark->endGlobalPosition.z) * 0.5
        };
        const heritage::graphics::tiremarks::ChunkAddress address =
            heritage::graphics::tiremarks::chunkAddress(midpoint);
        auto [chunkIt, inserted] = m_tireMarkGpuChunks.try_emplace(address);
        TireMarkGpuChunk& chunk = chunkIt->second;
        if (inserted)
            chunk.globalOrigin = heritage::graphics::tiremarks::chunkOrigin(address);

        if (chunk.pages.empty()
            || chunk.pages.back().segmentCount >= chunk.pages.back().capacitySegments)
        {
            TireMarkGpuPage page;
            page.capacitySegments = tireMarkGpuPageCapacity(chunk.pages.size());
            glGenVertexArrays(1, &page.vao);
            glGenBuffers(1, &page.vbo);
            glBindVertexArray(page.vao);
            glBindBuffer(GL_ARRAY_BUFFER, page.vbo);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    sizeof(TireMarkGpuRecord) * page.capacitySegments),
                nullptr,
                GL_DYNAMIC_DRAW);

            const GLsizei stride = static_cast<GLsizei>(sizeof(TireMarkGpuRecord));
            const auto attribute = [stride](GLuint location, GLint count, std::size_t offset) {
                glEnableVertexAttribArray(location);
                glVertexAttribPointer(
                    location, count, GL_FLOAT, GL_FALSE, stride,
                    reinterpret_cast<const void*>(offset));
            };
            attribute(0, 3, offsetof(TireMarkGpuRecord, startLocal));
            attribute(1, 3, offsetof(TireMarkGpuRecord, endLocal));
            attribute(2, 3, offsetof(TireMarkGpuRecord, startNormal));
            attribute(3, 3, offsetof(TireMarkGpuRecord, endNormal));
            attribute(4, 3, offsetof(TireMarkGpuRecord, startRight));
            attribute(5, 3, offsetof(TireMarkGpuRecord, endRight));
            attribute(6, 4, offsetof(TireMarkGpuRecord, startData));
            attribute(7, 4, offsetof(TireMarkGpuRecord, endData));
            attribute(8, 4, offsetof(TireMarkGpuRecord, misc));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            chunk.pages.push_back(std::move(page));
        }

        TireMarkGpuPage& page = chunk.pages.back();
        TireMarkGpuRecord record{};
        const heritage::math::Vec3 startLocal =
            heritage::graphics::tiremarks::localFp32(
                mark->startGlobalPosition, chunk.globalOrigin);
        const heritage::math::Vec3 endLocal =
            heritage::graphics::tiremarks::localFp32(
                mark->endGlobalPosition, chunk.globalOrigin);
        const auto storeVec3 = [](float (&destination)[3], const heritage::math::Vec3& value) {
            destination[0] = value.x;
            destination[1] = value.y;
            destination[2] = value.z;
        };
        storeVec3(record.startLocal, startLocal);
        storeVec3(record.endLocal, endLocal);
        storeVec3(record.startNormal, mark->startNormal);
        storeVec3(record.endNormal, mark->endNormal);
        storeVec3(record.startRight, mark->startRight);
        storeVec3(record.endRight, mark->endRight);
        record.startData[0] = mark->startWidthM;
        record.startData[1] = mark->startIntensity;
        record.startData[2] = mark->startLoadFractions[0];
        record.startData[3] = mark->startLoadFractions[1];
        record.endData[0] = mark->endWidthM;
        record.endData[1] = mark->endIntensity;
        record.endData[2] = mark->endLoadFractions[0];
        record.endData[3] = mark->endLoadFractions[1];
        record.misc[0] = mark->startLoadFractions[2];
        record.misc[1] = mark->endLoadFractions[2];
        record.misc[2] = static_cast<float>(mark->birthTimeSeconds);
        record.misc[3] = static_cast<float>(
            (mark->startFeather ? 1u : 0u) | (mark->endFeather ? 2u : 0u));

        if (page.pendingUpload.empty())
        {
            page.pendingByteOffset = static_cast<std::size_t>(page.segmentCount)
                * sizeof(TireMarkGpuRecord);
        }
        const std::size_t oldBytes = page.pendingUpload.size();
        page.pendingUpload.resize(oldBytes + sizeof(TireMarkGpuRecord));
        std::memcpy(page.pendingUpload.data() + oldBytes, &record, sizeof(record));

        const std::uint32_t recordIndex = page.segmentCount;
        const std::uint32_t flags =
            (mark->startFeather ? 1u : 0u) | (mark->endFeather ? 2u : 0u);
        if (mark->endFeather)
        {
            m_tireMarkGpuTailLocations[mark->serial] = TireMarkGpuTailLocation{
                address, page.vbo, recordIndex, flags, mark->birthTimeSeconds
            };
        }

        if (page.segmentCount == 0)
        {
            page.minimumBirthTimeSeconds = mark->birthTimeSeconds;
            page.maximumBirthTimeSeconds = mark->birthTimeSeconds;
        }
        else
        {
            page.minimumBirthTimeSeconds = std::min(
                page.minimumBirthTimeSeconds, mark->birthTimeSeconds);
            page.maximumBirthTimeSeconds = std::max(
                page.maximumBirthTimeSeconds, mark->birthTimeSeconds);
        }
        ++page.segmentCount;
    }

    // Batch all records appended to the same page this frame into one upload.
    // A frozen page is never touched again; old history therefore consumes no
    // recurring CPU tessellation, sorting, memcpy or glBufferData work.
    for (auto& [address, chunk] : m_tireMarkGpuChunks)
    {
        (void)address;
        for (TireMarkGpuPage& page : chunk.pages)
        {
            if (page.pendingUpload.empty())
                continue;
            glBindBuffer(GL_ARRAY_BUFFER, page.vbo);
            glBufferSubData(
                GL_ARRAY_BUFFER,
                static_cast<GLintptr>(page.pendingByteOffset),
                static_cast<GLsizeiptr>(page.pendingUpload.size()),
                page.pendingUpload.data());
            page.pendingUpload.clear();
            page.pendingByteOffset = 0;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    m_lastCachedTireMarkSerial = lastSerial;
    m_lastTireMarkCachePresentationTime = presentationTime;
}

void SurfacePresentationRenderer::drawTireMarkGpuCache(
    const heritage::physics::SurfacePresentation& presentation,
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const heritage::math::DVec3& cameraGlobal,
    bool nearMaterialAuthority,
    float weatherWetness) const
{
    if (m_tireMarkGpuChunks.empty())
        return;

    const float detailedDistance = static_cast<float>(kTireMarkGpuDetailedDistanceM);
    const float drawDistance = static_cast<float>(kTireMarkGpuDrawDistanceM);
    const float lodBlendWidth = heritage::graphics::lod::lodBlendWidthMeters(
        detailedDistance);
    const float visibilityFadeWidth = heritage::graphics::lod::visibilityFadeWidthMeters(
        drawDistance);
    const float presentationTime = static_cast<float>(presentation.elapsedSeconds());
    const float historyFloorBirthTime = presentation.tireMarkSegments().empty()
        ? -1.0e20f
        : static_cast<float>(presentation.tireMarkSegments().front().birthTimeSeconds);

    glEnable(GL_POLYGON_OFFSET_FILL);
    // LIVETRACK22: the far vector LOD is a decal-like overlay. Positive offset
    // pushed it behind coplanar road depth on conventional OpenGL depth and
    // caused the observed clip/flicker. Pull it slightly toward the camera.
    glPolygonOffset(-1.0f, -4.0f);
    glUseProgram(m_tireMarkProgram);
    glUniformMatrix4fv(
        m_tireMarkUniformView,
        1, GL_FALSE, view.m);
    glUniformMatrix4fv(
        m_tireMarkUniformProjection,
        1, GL_FALSE, projection.m);
    glUniform1f(
        m_tireMarkUniformPresentationTime,
        presentationTime);
    glUniform1f(
        m_tireMarkUniformHistoryFloorBirthTime,
        historyFloorBirthTime);
    glUniform1f(
        m_tireMarkUniformRetirementSeconds,
        static_cast<float>(heritage::physics::SurfacePresentation::kTireMarkRetirementSeconds));
    glUniform1f(
        m_tireMarkUniformDetailedDistance,
        detailedDistance);
    glUniform1f(
        m_tireMarkUniformLodBlendWidth,
        lodBlendWidth);
    glUniform1f(
        m_tireMarkUniformDrawDistance,
        drawDistance);
    glUniform1f(
        m_tireMarkUniformVisibilityFadeWidth,
        visibilityFadeWidth);
    glUniform1f(
        m_tireMarkUniformCapDistance,
        static_cast<float>(kTireMarkGpuCapDistanceM));
    glUniform1i(
        m_tireMarkUniformNearMaterialAuthority,
        nearMaterialAuthority ? 1 : 0);
    glUniform1f(
        m_tireMarkUniformWeatherWetness,
        std::clamp(weatherWetness, 0.0f, 1.0f));
    const GLint chunkOriginLocation = m_tireMarkUniformChunkOriginRelative;

    const double conservativeChunkRange = kTireMarkGpuDrawDistanceM
        + heritage::graphics::tiremarks::kChunkHorizontalHalfDiagonalM;
    const double conservativeChunkRangeSquared =
        conservativeChunkRange * conservativeChunkRange;

    for (const auto& [address, chunk] : m_tireMarkGpuChunks)
    {
        (void)address;
        const double dx = chunk.globalOrigin.x - cameraGlobal.x;
        const double dz = chunk.globalOrigin.z - cameraGlobal.z;
        const double chunkDistanceSquared = dx * dx + dz * dz;
        if (chunkDistanceSquared > conservativeChunkRangeSquared)
            continue;

        const heritage::math::Vec3 relativeOrigin =
            heritage::graphics::presentation::cameraRelativeFp32(
                chunk.globalOrigin, cameraGlobal);
        glUniform3f(
            chunkOriginLocation,
            relativeOrigin.x, relativeOrigin.y, relativeOrigin.z);

        const double chunkDistance = std::sqrt(chunkDistanceSquared);
        const std::uint64_t approximateTrianglesPerSegment =
            chunkDistance <= kTireMarkGpuDetailedDistanceM
                ? 10u
                : 2u;
        for (const TireMarkGpuPage& page : chunk.pages)
        {
            if (page.segmentCount == 0)
                continue;
            glBindVertexArray(page.vao);
            glDrawArrays(
                GL_POINTS, 0, static_cast<GLsizei>(page.segmentCount));
            ++m_frameStats.drawCalls;
            m_frameStats.visibleTireMarkSegments += page.segmentCount;
            m_frameStats.trackTriangles +=
                static_cast<std::uint64_t>(page.segmentCount)
                * approximateTrianglesPerSegment;
        }
    }

    glBindVertexArray(0);
    glDisable(GL_POLYGON_OFFSET_FILL);
}



} // namespace heritage::graphics
