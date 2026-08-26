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
namespace {
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

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

void stableSurfaceBasis(
    const heritage::math::Vec3& normal,
    heritage::math::Vec3& forward,
    heritage::math::Vec3& right)
{
    // TIRE15C3: procedural track debris must be world-anchored. Using the
    // latest tire contact direction as the cell basis made every existing
    // shred rotate when the driver steered through that cell. Project a fixed
    // world axis onto the support plane instead; the deterministic per-piece
    // random angle then remains stable for the lifetime of the track state.
    const heritage::math::Vec3 worldForward{ 0.0f, 0.0f, 1.0f };
    const heritage::math::Vec3 worldRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 tangent = subtract(
        worldForward, scale(normal, dot(worldForward, normal)));
    if (length(tangent) <= 0.10f)
    {
        tangent = subtract(
            worldRight, scale(normal, dot(worldRight, normal)));
    }
    forward = normalize(tangent, { 0.0f, 0.0f, 1.0f });
    right = normalize(cross(normal, forward), { 1.0f, 0.0f, 0.0f });
    forward = normalize(cross(right, normal), forward);
}

std::uint32_t rubberSeed(
    const heritage::physics::rubber::TrackRubberVisualCell& cell,
    std::uint32_t sequence)
{
    const std::int64_t x = static_cast<std::int64_t>(std::floor(cell.globalPosition.x * 2.0));
    const std::int64_t z = static_cast<std::int64_t>(std::floor(cell.globalPosition.z * 2.0));
    std::uint64_t value = static_cast<std::uint64_t>(x) * 0x9e3779b97f4a7c15ULL;
    value ^= static_cast<std::uint64_t>(z) + 0x85ebca6bULL + (value << 6u) + (value >> 2u);
    // Keep visual slots stable as rubber state updates. Do not mix the update
    // serial into this seed or existing resting flakes would jump every touch.
    value ^= static_cast<std::uint64_t>(sequence) * 0xc2b2ae3d27d4eb4fULL;
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31u;
    return static_cast<std::uint32_t>(value ^ (value >> 32u));
}


} // namespace

void SurfacePresentationRenderer::clearMarbleGpuCache() const
{
    for (auto& [address, chunk] : m_marbleGpuChunks)
    {
        (void)address;
        for (MarbleGpuPage& page : chunk.pages)
        {
            if (page.vbo != 0)
                glDeleteBuffers(1, &page.vbo);
            if (page.vao != 0)
                glDeleteVertexArrays(1, &page.vao);
        }
    }
    m_marbleGpuChunks.clear();
    m_marbleGpuLocations.clear();
    m_lastMarbleCellCount = 0;
    m_lastMarbleResidentChunkCount = 0;
}

void SurfacePresentationRenderer::syncMarbleGpuCache(
    const heritage::physics::SurfaceWorld& surfaces,
    const heritage::math::DVec3& cameraGlobal) const
{
    const auto& rubber = surfaces.trackRubber();
    const std::size_t currentCellCount = rubber.cellCount();
    const std::size_t currentResidentChunks = rubber.residentChunkCount();

    if (currentCellCount == 0)
    {
        if (!m_marbleGpuChunks.empty())
            clearMarbleGpuCache();
        return;
    }

    // Explicit reset and common eviction cases are safest as a cache rebuild.
    // This is presentation-only and rare; normal contact/wake updates patch one
    // compact cell record in-place instead.
    if ((m_lastMarbleCellCount != 0 && currentCellCount < m_lastMarbleCellCount)
        || (m_lastMarbleResidentChunkCount != 0
            && currentResidentChunks < m_lastMarbleResidentChunkCount))
    {
        clearMarbleGpuCache();
    }

    auto& cells = m_marbleCellScratch;
    cells.clear();
    rubber.collectPresentationCellsUnsorted(
        cameraGlobal, kMarbleGpuDrawDistanceM + 8.0, cells, true);
    const float cellSize = rubber.description().cellSizeM;
    const double verticalLayerSize = std::max(
        static_cast<double>(rubber.description().verticalLayerSizeM), 0.01);

    const auto floatBits = [](float value) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    };
    const auto signatureFor = [&](const auto& cell) {
        std::uint64_t value = cell.updateSerial * 0x9e3779b97f4a7c15ULL;
        const auto mix = [&](std::uint32_t bits) {
            value ^= static_cast<std::uint64_t>(bits)
                + 0x9e3779b97f4a7c15ULL + (value << 6u) + (value >> 2u);
        };
        mix(floatBits(cell.looseRubber));
        mix(floatBits(cell.marbleMaturity));
        mix(floatBits(cell.fragmentSeverity));
        mix(floatBits(cell.persistentPiecePopulation));
        mix(floatBits(cell.normal.x));
        mix(floatBits(cell.normal.y));
        mix(floatBits(cell.normal.z));
        return value;
    };

    const auto storeVec3 = [](float (&destination)[3], const heritage::math::Vec3& value) {
        destination[0] = value.x;
        destination[1] = value.y;
        destination[2] = value.z;
    };

    for (const auto& cell : cells)
    {
        const bool active = cell.looseRubber > 0.0012f
            || cell.persistentPiecePopulation >= 0.45f;
        MarbleGpuCellKey key;
        key.x = static_cast<std::int64_t>(std::floor(
            cell.globalPosition.x / static_cast<double>(cellSize)));
        key.y = static_cast<std::int64_t>(std::floor(
            (cell.globalPosition.y + verticalLayerSize * 0.5) / verticalLayerSize));
        key.z = static_cast<std::int64_t>(std::floor(
            cell.globalPosition.z / static_cast<double>(cellSize)));
        key.material = static_cast<std::uint8_t>(cell.material);

        const std::uint64_t visualSignature = signatureFor(cell);
        auto locationIt = m_marbleGpuLocations.find(key);
        if (locationIt != m_marbleGpuLocations.end()
            && locationIt->second.updateSerial == cell.updateSerial
            && locationIt->second.visualSignature == visualSignature)
        {
            continue;
        }
        if (locationIt == m_marbleGpuLocations.end() && !active)
            continue;

        MarbleCellGpuRecord record{};
        heritage::math::Vec3 normal = normalize(cell.normal, { 0.0f, 1.0f, 0.0f });
        heritage::math::Vec3 forward{};
        heritage::math::Vec3 right{};
        stableSurfaceBasis(normal, forward, right);
        (void)right;
        storeVec3(record.normal, normal);
        storeVec3(record.forward, forward);
        record.state[0] = cell.looseRubber;
        record.state[1] = cell.marbleMaturity;
        record.state[2] = cell.fragmentSeverity;
        record.state[3] = cell.persistentPiecePopulation;
        record.misc[0] = static_cast<float>(rubberSeed(cell, 0u) & 0x00ffffffu);
        if (record.misc[0] < 1.0f)
            record.misc[0] = 1.0f;
        record.misc[1] = cellSize;
        record.misc[2] = static_cast<float>(cell.passCount);
        record.misc[3] = active ? 1.0f : 0.0f;

        MarbleGpuLocation location;
        MarbleGpuPage* pagePointer = nullptr;
        if (locationIt == m_marbleGpuLocations.end())
        {
            const auto address = heritage::graphics::tiremarks::chunkAddress(
                cell.globalPosition);
            auto [chunkIt, inserted] = m_marbleGpuChunks.try_emplace(address);
            MarbleGpuChunk& chunk = chunkIt->second;
            if (inserted)
                chunk.globalOrigin = heritage::graphics::tiremarks::chunkOrigin(address);

            if (chunk.pages.empty()
                || chunk.pages.back().cellCount >= chunk.pages.back().capacityCells)
            {
                MarbleGpuPage page;
                page.capacityCells = marbleGpuPageCapacity(chunk.pages.size());
                page.cpuMirror.resize(
                    static_cast<std::size_t>(page.capacityCells)
                    * sizeof(MarbleCellGpuRecord));
                glGenVertexArrays(1, &page.vao);
                glGenBuffers(1, &page.vbo);
                glBindVertexArray(page.vao);
                glBindBuffer(GL_ARRAY_BUFFER, page.vbo);
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(page.cpuMirror.size()),
                    nullptr,
                    GL_DYNAMIC_DRAW);

                const GLsizei stride = static_cast<GLsizei>(sizeof(MarbleCellGpuRecord));
                const auto attribute = [stride](GLuint index, GLint count, std::size_t offset) {
                    glEnableVertexAttribArray(index);
                    glVertexAttribPointer(
                        index, count, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<const void*>(offset));
                };
                attribute(0, 3, offsetof(MarbleCellGpuRecord, centerLocal));
                attribute(1, 3, offsetof(MarbleCellGpuRecord, normal));
                attribute(2, 3, offsetof(MarbleCellGpuRecord, forward));
                attribute(3, 4, offsetof(MarbleCellGpuRecord, state));
                attribute(4, 4, offsetof(MarbleCellGpuRecord, misc));
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
                chunk.pages.push_back(std::move(page));
            }

            MarbleGpuPage& page = chunk.pages.back();
            location.address = address;
            location.pageIndex = chunk.pages.size() - 1u;
            location.recordIndex = page.cellCount++;
            location.updateSerial = cell.updateSerial;
            location.visualSignature = visualSignature;
            locationIt = m_marbleGpuLocations.emplace(key, location).first;
            pagePointer = &page;
        }
        else
        {
            location = locationIt->second;
            auto chunkIt = m_marbleGpuChunks.find(location.address);
            if (chunkIt == m_marbleGpuChunks.end()
                || location.pageIndex >= chunkIt->second.pages.size())
            {
                // Defensive cache self-heal. A reset/eviction should normally
                // have cleared this location together with its GPU pages.
                m_marbleGpuLocations.erase(locationIt);
                continue;
            }
            pagePointer = &chunkIt->second.pages[location.pageIndex];
            locationIt->second.updateSerial = cell.updateSerial;
            locationIt->second.visualSignature = visualSignature;
        }

        auto chunkIt = m_marbleGpuChunks.find(locationIt->second.address);
        if (chunkIt == m_marbleGpuChunks.end() || pagePointer == nullptr)
            continue;
        const heritage::math::Vec3 local = heritage::graphics::tiremarks::localFp32(
            cell.globalPosition, chunkIt->second.globalOrigin);
        storeVec3(record.centerLocal, local);

        MarbleGpuPage& page = *pagePointer;
        const std::size_t byteOffset = static_cast<std::size_t>(
            locationIt->second.recordIndex) * sizeof(MarbleCellGpuRecord);
        std::memcpy(page.cpuMirror.data() + byteOffset, &record, sizeof(record));
        page.dirtyBeginByte = std::min(page.dirtyBeginByte, byteOffset);
        page.dirtyEndByte = std::max(
            page.dirtyEndByte, byteOffset + sizeof(MarbleCellGpuRecord));
    }

    // One contiguous upload per dirty page, regardless of how many cells were
    // touched by tires/wakes this frame. Frozen pages incur no recurring upload.
    for (auto& [address, chunk] : m_marbleGpuChunks)
    {
        (void)address;
        for (MarbleGpuPage& page : chunk.pages)
        {
            if (page.dirtyBeginByte == static_cast<std::size_t>(-1)
                || page.dirtyEndByte <= page.dirtyBeginByte)
            {
                continue;
            }
            glBindBuffer(GL_ARRAY_BUFFER, page.vbo);
            glBufferSubData(
                GL_ARRAY_BUFFER,
                static_cast<GLintptr>(page.dirtyBeginByte),
                static_cast<GLsizeiptr>(page.dirtyEndByte - page.dirtyBeginByte),
                page.cpuMirror.data() + page.dirtyBeginByte);
            page.dirtyBeginByte = static_cast<std::size_t>(-1);
            page.dirtyEndByte = 0;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    m_lastMarbleCellCount = currentCellCount;
    m_lastMarbleResidentChunkCount = currentResidentChunks;
}

void SurfacePresentationRenderer::drawMarbleGpuCache(
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const heritage::math::DVec3& cameraGlobal) const
{
    if (m_marbleGpuChunks.empty())
        return;

    const float detailedDistance = static_cast<float>(kMarbleGpuDetailedDistanceM);
    const float drawDistance = static_cast<float>(kMarbleGpuDrawDistanceM);
    const float lodBlendWidth = heritage::graphics::lod::lodBlendWidthMeters(
        detailedDistance);
    const float visibilityFadeWidth = heritage::graphics::lod::visibilityFadeWidthMeters(
        drawDistance);

    glUseProgram(m_marbleProgram);
    glUniformMatrix4fv(
        m_marbleUniformView,
        1, GL_FALSE, view.m);
    glUniformMatrix4fv(
        m_marbleUniformProjection,
        1, GL_FALSE, projection.m);
    glUniform1f(
        m_marbleUniformDetailedDistance, detailedDistance);
    glUniform1f(
        m_marbleUniformLodBlendWidth, lodBlendWidth);
    glUniform1f(
        m_marbleUniformDrawDistance, drawDistance);
    glUniform1f(
        m_marbleUniformVisibilityFadeWidth,
        visibilityFadeWidth);
    const GLint chunkOriginLocation = m_marbleUniformChunkOriginRelative;

    const double conservativeRange = kMarbleGpuDrawDistanceM
        + heritage::graphics::tiremarks::kChunkHorizontalHalfDiagonalM;
    const double conservativeRangeSquared = conservativeRange * conservativeRange;
    for (const auto& [address, chunk] : m_marbleGpuChunks)
    {
        (void)address;
        const double dx = chunk.globalOrigin.x - cameraGlobal.x;
        const double dz = chunk.globalOrigin.z - cameraGlobal.z;
        if (dx * dx + dz * dz > conservativeRangeSquared)
            continue;

        const heritage::math::Vec3 relativeOrigin =
            heritage::graphics::presentation::cameraRelativeFp32(
                chunk.globalOrigin, cameraGlobal);
        glUniform3f(
            chunkOriginLocation,
            relativeOrigin.x, relativeOrigin.y, relativeOrigin.z);

        const double chunkDistance = std::sqrt(dx * dx + dz * dz);
        const std::uint64_t approximateTrianglesPerCell =
            chunkDistance <= kMarbleGpuDetailedDistanceM ? 48u : 4u;
        for (const MarbleGpuPage& page : chunk.pages)
        {
            if (page.cellCount == 0)
                continue;
            glBindVertexArray(page.vao);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(page.cellCount));
            ++m_frameStats.drawCalls;
            m_frameStats.visibleRubberCells += page.cellCount;
            m_frameStats.trackTriangles +=
                static_cast<std::uint64_t>(page.cellCount)
                * approximateTrianglesPerCell;
        }
    }
    glBindVertexArray(0);
}

void SurfacePresentationRenderer::drawMovingRubberGpu(
    const heritage::physics::SurfaceWorld& surfaces,
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const heritage::math::DVec3& cameraGlobal) const
{
    auto& packets = m_movingRubberPacketScratch;
    packets.clear();
    surfaces.trackRubber().collectTransientPresentationUnsorted(
        cameraGlobal, kMovingRubberGpuDrawDistanceM, packets);
    if (packets.empty())
        return;

    std::vector<MovingRubberGpuRecord> records;
    records.reserve(packets.size());
    std::uint64_t approximateRepresentatives = 0;
    const auto storeVec3 = [](float (&destination)[3], const heritage::math::Vec3& value) {
        destination[0] = value.x;
        destination[1] = value.y;
        destination[2] = value.z;
    };
    for (const auto& packet : packets)
    {
        MovingRubberGpuRecord record{};
        const heritage::math::Vec3 relative =
            heritage::graphics::presentation::cameraRelativeFp32(
                packet.globalPosition, cameraGlobal);
        storeVec3(record.centerRelative, relative);
        storeVec3(record.axisRight, packet.axisRight);
        storeVec3(record.axisForward, packet.axisForward);
        storeVec3(record.axisNormal, packet.axisNormal);
        record.shape[0] = packet.lengthM;
        record.shape[1] = packet.widthM;
        record.shape[2] = packet.bendVertex1M;
        record.shape[3] = packet.bendVertex3M;
        record.state[0] = packet.ageSeconds;
        record.state[1] = packet.opacity;
        record.state[2] = packet.piecePopulation;
        record.state[3] = packet.fragmentSeverity;
        record.misc[0] = static_cast<float>(packet.seed & 0x00ffffffu);
        if (record.misc[0] < 1.0f)
            record.misc[0] = 1.0f;
        record.misc[1] = packet.phase
            == heritage::physics::rubber::TrackRubberTransientPhase::Airborne
            ? 0.0f : 1.0f;
        record.misc[2] = packet.marbleMaturity;
        record.misc[3] = packet.quantity;
        records.push_back(record);
        approximateRepresentatives += static_cast<std::uint64_t>(std::clamp(
            static_cast<int>(std::ceil(packet.piecePopulation)), 0, 12));
    }

    const std::uint32_t required = static_cast<std::uint32_t>(records.size());
    if (required > m_movingRubberCapacity)
    {
        std::uint32_t capacity = std::max<std::uint32_t>(256u, m_movingRubberCapacity);
        while (capacity < required)
            capacity *= 2u;
        m_movingRubberCapacity = capacity;
        glBindBuffer(GL_ARRAY_BUFFER, m_movingRubberVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                static_cast<std::size_t>(capacity) * sizeof(MovingRubberGpuRecord)),
            nullptr,
            GL_DYNAMIC_DRAW);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_movingRubberVbo);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizeiptr>(records.size() * sizeof(MovingRubberGpuRecord)),
        records.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const float drawDistance = static_cast<float>(kMovingRubberGpuDrawDistanceM);
    glUseProgram(m_movingRubberProgram);
    glUniformMatrix4fv(
        m_movingRubberUniformView,
        1, GL_FALSE, view.m);
    glUniformMatrix4fv(
        m_movingRubberUniformProjection,
        1, GL_FALSE, projection.m);
    glUniform1f(
        m_movingRubberUniformDrawDistance, drawDistance);
    glUniform1f(
        m_movingRubberUniformVisibilityFadeWidth,
        heritage::graphics::lod::visibilityFadeWidthMeters(drawDistance));
    glBindVertexArray(m_movingRubberVao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(records.size()));
    glBindVertexArray(0);
    ++m_frameStats.drawCalls;
    m_frameStats.visibleMovingRubber += approximateRepresentatives;
    m_frameStats.trackTriangles += approximateRepresentatives * 2u;
}


} // namespace heritage::graphics
