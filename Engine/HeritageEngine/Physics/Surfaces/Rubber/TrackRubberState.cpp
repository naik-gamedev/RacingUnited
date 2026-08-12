#include "TrackRubberState.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace heritage::physics::rubber {
namespace {

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float vectorLength(const heritage::math::Vec3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

heritage::math::Vec3 normalize(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitude = vectorLength(value);
    if (!std::isfinite(magnitude) || magnitude <= 1.0e-6f)
        return fallback;
    return { value.x / magnitude, value.y / magnitude, value.z / magnitude };
}

float dot(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

heritage::math::Vec3 scale(const heritage::math::Vec3& value, float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float random01(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>((state >> 8u) & 0x00ffffffu)
        / static_cast<float>(0x01000000u);
}

std::uint32_t mixSeed(std::uint64_t serial, std::uint32_t salt)
{
    std::uint64_t value = serial ^ (static_cast<std::uint64_t>(salt) << 32u);
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31u;
    return static_cast<std::uint32_t>(value ^ (value >> 32u));
}

TrackRubberDescription sanitized(TrackRubberDescription value)
{
    if (!std::isfinite(value.cellSizeM))
        value.cellSizeM = 0.50f;
    value.cellSizeM = std::clamp(value.cellSizeM, 0.10f, 2.0f);
    value.chunkSizeCells = std::clamp<std::uint32_t>(value.chunkSizeCells, 4u, 256u);
    value.maximumCellCount = std::clamp<std::size_t>(
        value.maximumCellCount, 256u, 1u << 23u);
    value.maximumResidentChunkCount = std::clamp<std::size_t>(
        value.maximumResidentChunkCount, 1u, 1u << 18u);
    value.maximumPersistentPieceCount = std::clamp<std::size_t>(
        value.maximumPersistentPieceCount, 1024u, 5000000u);
    value.maximumTransientPacketCount = std::clamp<std::size_t>(
        value.maximumTransientPacketCount, 64u, 65536u);
    if (!std::isfinite(value.nominalPiecesPerLooseUnit))
        value.nominalPiecesPerLooseUnit = 110.0f;
    value.nominalPiecesPerLooseUnit = std::clamp(
        value.nominalPiecesPerLooseUnit, 8.0f, 512.0f);
    if (!std::isfinite(value.verticalLayerSizeM))
        value.verticalLayerSizeM = 2.0f;
    value.verticalLayerSizeM = std::clamp(value.verticalLayerSizeM, 0.25f, 100.0f);
    value.maximumDryDepositedGripGain = std::clamp(
        value.maximumDryDepositedGripGain, 0.0f, 0.30f);
    value.maximumWetDepositedGripLoss = std::clamp(
        value.maximumWetDepositedGripLoss, 0.0f, 0.40f);
    value.maximumLooseRubberGripLoss = std::clamp(
        value.maximumLooseRubberGripLoss, 0.0f, 0.60f);
    value.maximumLooseRollingResistanceGain = std::clamp(
        value.maximumLooseRollingResistanceGain, 0.0f, 0.50f);
    value.depositedDryAgeRatePerSecond = std::clamp(
        value.depositedDryAgeRatePerSecond, 0.0, 0.1);
    value.looseDryAgeRatePerSecond = std::clamp(
        value.looseDryAgeRatePerSecond, 0.0, 0.1);
    value.depositedRainWashRatePerWetSecond = std::clamp(
        value.depositedRainWashRatePerWetSecond, 0.0, 0.5);
    value.looseRainWashRatePerWetSecond = std::clamp(
        value.looseRainWashRatePerWetSecond, 0.0, 1.0);
    return value;
}

std::int64_t floorDivide(std::int64_t value, std::int64_t divisor)
{
    std::int64_t quotient = value / divisor;
    if (value % divisor < 0)
        --quotient;
    return quotient;
}

bool quantize(double coordinate, double inverseCellSize, std::int64_t& result)
{
    if (!std::isfinite(coordinate) || !std::isfinite(inverseCellSize))
        return false;
    const double value = std::floor(coordinate * inverseCellSize);
    if (!std::isfinite(value)
        || value < static_cast<double>((std::numeric_limits<std::int64_t>::min)())
        || value > static_cast<double>((std::numeric_limits<std::int64_t>::max)()))
    {
        return false;
    }
    result = static_cast<std::int64_t>(value);
    return true;
}

bool sameAddressing(
    const TrackRubberDescription& a,
    const TrackRubberDescription& b)
{
    return a.cellSizeM == b.cellSizeM
        && a.chunkSizeCells == b.chunkSizeCells
        && a.verticalLayerSizeM == b.verticalLayerSizeM;
}

} // namespace

bool rubberCapableSurface(SurfaceMaterial material)
{
    return material == SurfaceMaterial::Default
        || material == SurfaceMaterial::Asphalt
        || material == SurfaceMaterial::Kerb
        || material == SurfaceMaterial::PaintedLine;
}

TrackRubberState::TrackRubberState(const TrackRubberDescription& description)
    : m_description(sanitized(description))
{
}

void TrackRubberState::clear()
{
    m_chunks.clear();
    m_chunkLru.clear();
    m_cellCount = 0;
    m_elapsedSeconds = 0.0;
    m_globalWetExposureSeconds = 0.0;
    m_updateSerial = 0;
    m_contactSamples = 0;
    m_depositedGeneration = 0.0;
    m_looseGeneration = 0.0;
    m_persistentPiecePopulation = 0.0;
    m_transientPiecePopulation = 0.0;
    m_transientLooseQuantity = 0.0;
    m_transientPackets.clear();
}

void TrackRubberState::setDescription(const TrackRubberDescription& description)
{
    const TrackRubberDescription next = sanitized(description);
    if (!sameAddressing(m_description, next))
    {
        m_description = next;
        clear();
        return;
    }
    m_description = next;
    // A development-time packet-budget reduction must never delete rubber.
    // Fold excess moving packets into retained packets rather than truncating
    // the vector (which would silently lose authoritative quantity/pieces).
    while (m_transientPackets.size() > m_description.maximumTransientPacketCount)
    {
        const TransientPacket extra = m_transientPackets.back();
        m_transientPackets.pop_back();
        const std::size_t targetIndex = static_cast<std::size_t>(extra.seed)
            % m_description.maximumTransientPacketCount;
        TransientPacket& target = m_transientPackets[targetIndex];
        const float combined = target.quantity + extra.quantity;
        if (combined > 1.0e-8f)
        {
            const float targetWeight = target.quantity / combined;
            const float extraWeight = extra.quantity / combined;
            target.marbleMaturity = clamp01(
                target.marbleMaturity * targetWeight
                    + extra.marbleMaturity * extraWeight);
            target.fragmentSeverity = clamp01(
                target.fragmentSeverity * targetWeight
                    + extra.fragmentSeverity * extraWeight);
            target.velocityMps = add(
                scale(target.velocityMps, targetWeight),
                scale(extra.velocityMps, extraWeight));
        }
        target.quantity += extra.quantity;
        target.piecePopulation += extra.piecePopulation;
    }
    while (m_chunks.size() > m_description.maximumResidentChunkCount
        || m_cellCount > m_description.maximumCellCount)
    {
        if (!evictLeastRecentlyUsedChunk())
            break;
    }
}

void TrackRubberState::advance(float deltaTimeSeconds, float globalWetness)
{
    if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0f)
        return;
    const float boundedDt = std::min(deltaTimeSeconds, 1.0f);
    const double dt = static_cast<double>(boundedDt);
    const double wetness = static_cast<double>(clamp01(globalWetness));
    m_elapsedSeconds += dt;
    m_globalWetExposureSeconds += dt * wetness;

    // TIRE15C5 moving rubber is authoritative but deliberately lightweight.
    // Integrate in small bounded slices so a low render/world rate cannot tunnel
    // an airborne flake straight through its support plane.
    float remaining = boundedDt;
    while (remaining > 1.0e-6f)
    {
        const float step = std::min(remaining, 0.02f);
        integrateTransients(step);
        remaining -= step;
    }
}

std::size_t TrackRubberState::ChunkKeyHash::operator()(const ChunkKey& key) const
{
    const std::uint64_t x = static_cast<std::uint64_t>(key.x);
    const std::uint64_t z = static_cast<std::uint64_t>(key.z);
    std::uint64_t value = x + 0x9e3779b97f4a7c15ull;
    value ^= z + 0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebull;
    value ^= value >> 31u;
    return static_cast<std::size_t>(value);
}

std::size_t TrackRubberState::LocalKeyHash::operator()(const LocalKey& key) const
{
    std::size_t value = static_cast<std::size_t>(key.x) * 0x9e3779b1u;
    value ^= static_cast<std::size_t>(key.z) * 0x85ebca6bu
        + 0x9e3779b9u + (value << 6u) + (value >> 2u);
    const std::uint64_t layer = static_cast<std::uint64_t>(key.verticalLayer);
    value ^= static_cast<std::size_t>(layer ^ (layer >> 32u)) * 0x27d4eb2du
        + 0x9e3779b9u + (value << 6u) + (value >> 2u);
    value ^= static_cast<std::size_t>(key.material) * 0xc2b2ae35u
        + 0x9e3779b9u + (value << 6u) + (value >> 2u);
    return value;
}

TrackRubberState::Address TrackRubberState::addressFor(
    const heritage::math::DVec3& globalPosition,
    SurfaceMaterial material) const
{
    Address address;
    const double inverse = 1.0 / static_cast<double>(m_description.cellSizeM);
    std::int64_t cellX = 0;
    std::int64_t cellZ = 0;
    std::int64_t layer = 0;
    if (!quantize(globalPosition.x, inverse, cellX)
        || !quantize(globalPosition.z, inverse, cellZ)
        || !quantize(
            globalPosition.y,
            1.0 / static_cast<double>(m_description.verticalLayerSizeM),
            layer))
    {
        return address;
    }

    const std::int64_t chunkSize = static_cast<std::int64_t>(m_description.chunkSizeCells);
    const std::int64_t chunkX = floorDivide(cellX, chunkSize);
    const std::int64_t chunkZ = floorDivide(cellZ, chunkSize);
    const std::int64_t localX = cellX - chunkX * chunkSize;
    const std::int64_t localZ = cellZ - chunkZ * chunkSize;
    if (localX < 0 || localZ < 0 || localX >= chunkSize || localZ >= chunkSize)
        return address;

    address.valid = true;
    address.chunk = { chunkX, chunkZ };
    address.cell = {
        static_cast<std::uint16_t>(localX),
        static_cast<std::uint16_t>(localZ),
        layer,
        static_cast<std::uint8_t>(material)
    };
    return address;
}

heritage::math::DVec3 TrackRubberState::cellCenter(
    const ChunkKey& chunk,
    const LocalKey& cell) const
{
    const std::int64_t chunkSize = static_cast<std::int64_t>(m_description.chunkSizeCells);
    const std::int64_t cellX = chunk.x * chunkSize + static_cast<std::int64_t>(cell.x);
    const std::int64_t cellZ = chunk.z * chunkSize + static_cast<std::int64_t>(cell.z);
    const double size = static_cast<double>(m_description.cellSizeM);
    return {
        (static_cast<double>(cellX) + 0.5) * size,
        0.0,
        (static_cast<double>(cellZ) + 0.5) * size
    };
}

TrackRubberState::Cell TrackRubberState::resolvedCell(const Cell& cell) const
{
    Cell result = cell;
    const double elapsed = std::max(m_elapsedSeconds - cell.timeAtWriteSeconds, 0.0);
    const double wetExposure = std::max(
        m_globalWetExposureSeconds - cell.wetExposureAtWriteSeconds, 0.0);
    const double depositedDecay = std::exp(
        -m_description.depositedDryAgeRatePerSecond * elapsed
        -m_description.depositedRainWashRatePerWetSecond * wetExposure);
    const double looseDecay = std::exp(
        -m_description.looseDryAgeRatePerSecond * elapsed
        -m_description.looseRainWashRatePerWetSecond * wetExposure);
    result.depositedRubber = clamp01(static_cast<float>(
        static_cast<double>(cell.depositedRubber) * depositedDecay));
    result.looseRubber = clamp01(static_cast<float>(
        static_cast<double>(cell.looseRubber) * looseDecay));
    result.persistentPiecePopulation = std::max(
        0.0f,
        static_cast<float>(
            static_cast<double>(cell.persistentPiecePopulation) * looseDecay));
    if (result.looseRubber <= 1.0e-6f
        || result.persistentPiecePopulation <= 1.0e-4f)
    {
        result.persistentPiecePopulation = 0.0f;
        if (result.looseRubber <= 1.0e-6f)
        {
            result.marbleMaturity = 0.0f;
            result.fragmentSeverity = 0.0f;
        }
    }
    result.timeAtWriteSeconds = m_elapsedSeconds;
    result.wetExposureAtWriteSeconds = m_globalWetExposureSeconds;
    return result;
}

TrackRubberSample TrackRubberState::sampleFromCell(
    const Cell& rawCell,
    float localWetness) const
{
    const Cell cell = resolvedCell(rawCell);
    TrackRubberSample sample;
    sample.valid = true;
    sample.material = cell.material;
    sample.depositedRubber = cell.depositedRubber;
    sample.looseRubber = cell.looseRubber;
    sample.marbleMaturity = clamp01(cell.marbleMaturity);
    sample.fragmentSeverity = clamp01(cell.fragmentSeverity);
    sample.persistentPiecePopulation = std::max(
        cell.persistentPiecePopulation, 0.0f);
    // Fresh tacky shreds are easier to pick back up; mature rounded marbles
    // still contaminate a tire, but a little less readily.
    sample.pickupAvailability = cell.looseRubber
        * (1.0f - 0.28f * sample.marbleMaturity);
    sample.passCount = cell.passCount;

    const float wetness = clamp01(localWetness);
    const float depositedEffect = cell.depositedRubber
        * (m_description.maximumDryDepositedGripGain * (1.0f - wetness)
            - m_description.maximumWetDepositedGripLoss * wetness);
    const float maturity = clamp01(cell.marbleMaturity);
    const float loosePenalty = cell.looseRubber
        * m_description.maximumLooseRubberGripLoss
        * (0.78f + 0.22f * maturity);
    sample.contactFrictionScale = std::clamp(
        1.0f + depositedEffect - loosePenalty, 0.65f, 1.12f);
    sample.rollingResistanceScale = 1.0f
        + cell.looseRubber * m_description.maximumLooseRollingResistanceGain
            * (0.70f + 0.30f * maturity);
    return sample;
}

TrackRubberSample TrackRubberState::sample(
    const heritage::math::DVec3& globalPosition,
    SurfaceMaterial material,
    float localWetness) const
{
    TrackRubberSample empty;
    empty.valid = rubberCapableSurface(material);
    empty.material = material;
    if (!empty.valid)
        return empty;

    const Address address = addressFor(globalPosition, material);
    if (!address.valid)
        return empty;
    const auto chunk = m_chunks.find(address.chunk);
    if (chunk == m_chunks.end())
        return empty;
    const auto cell = chunk->second.cells.find(address.cell);
    if (cell == chunk->second.cells.end())
        return empty;
    return sampleFromCell(cell->second, localWetness);
}

void TrackRubberState::touchChunk(const ChunkKey& key, Chunk& chunk)
{
    m_chunkLru.erase(chunk.lruIterator);
    m_chunkLru.push_front(key);
    chunk.lruIterator = m_chunkLru.begin();
}

TrackRubberChunkSnapshot TrackRubberState::snapshotChunk(
    const ChunkKey& key,
    const Chunk& chunk) const
{
    TrackRubberChunkSnapshot snapshot;
    snapshot.chunkX = key.x;
    snapshot.chunkZ = key.z;
    snapshot.cells.reserve(chunk.cells.size());
    for (const auto& entry : chunk.cells)
    {
        const Cell resolved = resolvedCell(entry.second);
        TrackRubberPersistedCell cell;
        cell.localX = entry.first.x;
        cell.localZ = entry.first.z;
        cell.verticalLayer = entry.first.verticalLayer;
        cell.material = entry.first.material;
        cell.depositedRubber = resolved.depositedRubber;
        cell.looseRubber = resolved.looseRubber;
        cell.marbleMaturity = resolved.marbleMaturity;
        cell.fragmentSeverity = resolved.fragmentSeverity;
        cell.persistentPiecePopulation = resolved.persistentPiecePopulation;
        cell.surfaceHeightM = resolved.surfaceHeightM;
        cell.normal = resolved.normal;
        cell.forward = resolved.forward;
        cell.passProgress = resolved.passProgress;
        cell.passCount = resolved.passCount;
        cell.updateSerial = resolved.updateSerial;
        snapshot.cells.push_back(cell);
    }
    std::sort(
        snapshot.cells.begin(), snapshot.cells.end(),
        [](const TrackRubberPersistedCell& a, const TrackRubberPersistedCell& b) {
            if (a.verticalLayer != b.verticalLayer)
                return a.verticalLayer < b.verticalLayer;
            if (a.localZ != b.localZ)
                return a.localZ < b.localZ;
            if (a.localX != b.localX)
                return a.localX < b.localX;
            return a.material < b.material;
        });
    return snapshot;
}

void TrackRubberState::eraseChunk(
    const ChunkKey& key,
    std::unordered_map<ChunkKey, Chunk, ChunkKeyHash>::iterator found,
    bool notifyEviction)
{
    if (found == m_chunks.end())
        return;
    if (notifyEviction && m_chunkEvictionCallback)
        m_chunkEvictionCallback(snapshotChunk(key, found->second));
    double removedPieces = 0.0;
    for (const auto& entry : found->second.cells)
        removedPieces += static_cast<double>(entry.second.persistentPiecePopulation);
    m_persistentPiecePopulation = std::max(
        0.0, m_persistentPiecePopulation - removedPieces);
    m_cellCount -= (std::min)(m_cellCount, found->second.cells.size());
    m_chunkLru.erase(found->second.lruIterator);
    m_chunks.erase(found);
}

bool TrackRubberState::evictLeastRecentlyUsedChunk(const ChunkKey* protectedChunk)
{
    if (m_chunkLru.empty())
        return false;
    auto position = m_chunkLru.end();
    while (position != m_chunkLru.begin())
    {
        --position;
        if (protectedChunk != nullptr && *position == *protectedChunk)
            continue;
        const ChunkKey key = *position;
        const auto found = m_chunks.find(key);
        if (found == m_chunks.end())
        {
            m_chunkLru.erase(position);
            return true;
        }
        eraseChunk(key, found, true);
        return true;
    }
    return false;
}

void TrackRubberState::makeRoomForChunk(const ChunkKey* protectedChunk)
{
    while (m_chunks.size() >= m_description.maximumResidentChunkCount)
    {
        if (!evictLeastRecentlyUsedChunk(protectedChunk))
            break;
    }
}

void TrackRubberState::makeRoomForCell(const ChunkKey* protectedChunk)
{
    while (m_cellCount >= m_description.maximumCellCount)
    {
        if (evictLeastRecentlyUsedChunk(protectedChunk))
            continue;
        if (protectedChunk != nullptr)
        {
            const auto found = m_chunks.find(*protectedChunk);
            if (found != m_chunks.end() && !found->second.cells.empty())
            {
                found->second.cells.erase(found->second.cells.begin());
                --m_cellCount;
                continue;
            }
        }
        break;
    }
}

TrackRubberState::Cell& TrackRubberState::acquireCell(
    const heritage::math::DVec3& globalPosition,
    SurfaceMaterial material)
{
    const Address address = addressFor(globalPosition, material);
    // Callers validate finite/capable inputs before reaching here. In the
    // impossible invalid-address fallback, use a stable origin cell.
    const Address safeAddress = address.valid
        ? address : addressFor({ 0.0, 0.0, 0.0 }, SurfaceMaterial::Default);

    auto chunk = m_chunks.find(safeAddress.chunk);
    if (chunk == m_chunks.end())
    {
        makeRoomForChunk();
        m_chunkLru.push_front(safeAddress.chunk);
        Chunk value;
        value.lruIterator = m_chunkLru.begin();
        chunk = m_chunks.emplace(safeAddress.chunk, std::move(value)).first;
    }
    else
    {
        touchChunk(safeAddress.chunk, chunk->second);
    }

    auto cell = chunk->second.cells.find(safeAddress.cell);
    if (cell == chunk->second.cells.end())
    {
        makeRoomForCell(&safeAddress.chunk);
        Cell initial;
        initial.material = material;
        initial.surfaceHeightM = globalPosition.y;
        initial.timeAtWriteSeconds = m_elapsedSeconds;
        initial.wetExposureAtWriteSeconds = m_globalWetExposureSeconds;
        cell = chunk->second.cells.emplace(safeAddress.cell, initial).first;
        ++m_cellCount;
    }
    else
    {
        const float piecesBefore = cell->second.persistentPiecePopulation;
        cell->second = resolvedCell(cell->second);
        m_persistentPiecePopulation = std::max(
            0.0,
            m_persistentPiecePopulation
                + static_cast<double>(cell->second.persistentPiecePopulation - piecesBefore));
    }
    return cell->second;
}

float TrackRubberState::addLooseAbsolute(
    const heritage::math::DVec3& position,
    SurfaceMaterial material,
    float quantity,
    float maturity,
    float fragmentSeverity,
    float piecePopulation,
    const heritage::math::Vec3& supportNormal,
    const heritage::math::Vec3& supportForward)
{
    if (!rubberCapableSurface(material)
        || !std::isfinite(position.x) || !std::isfinite(position.y)
        || !std::isfinite(position.z) || !std::isfinite(quantity)
        || quantity <= 0.0f)
    {
        return 0.0f;
    }

    Cell& cell = acquireCell(position, material);
    const float capacity = std::max(1.0f - cell.looseRubber, 0.0f);
    const float accepted = std::min(quantity, capacity);
    if (accepted <= 1.0e-9f)
        return 0.0f;

    const float oldLoose = cell.looseRubber;
    const float newLoose = oldLoose + accepted;
    const float sourceMaturity = clamp01(maturity);
    const float sourceSeverity = clamp01(fragmentSeverity);
    cell.marbleMaturity = newLoose > 1.0e-8f
        ? clamp01((oldLoose * clamp01(cell.marbleMaturity)
            + accepted * sourceMaturity) / newLoose)
        : sourceMaturity;
    cell.fragmentSeverity = newLoose > 1.0e-8f
        ? clamp01((oldLoose * clamp01(cell.fragmentSeverity)
            + accepted * sourceSeverity) / newLoose)
        : sourceSeverity;
    cell.looseRubber = clamp01(newLoose);

    const float acceptedPieces = std::max(piecePopulation, 0.0f)
        * (accepted / std::max(quantity, 1.0e-8f));
    cell.persistentPiecePopulation += acceptedPieces;
    m_persistentPiecePopulation += static_cast<double>(acceptedPieces);

    if (cell.updateSerial == 0u)
    {
        cell.surfaceHeightM = position.y;
        cell.normal = normalize(supportNormal, { 0.0f, 1.0f, 0.0f });
        cell.forward = normalize(supportForward, { 0.0f, 0.0f, 1.0f });
    }
    cell.material = material;
    cell.updateSerial = ++m_updateSerial;
    cell.timeAtWriteSeconds = m_elapsedSeconds;
    cell.wetExposureAtWriteSeconds = m_globalWetExposureSeconds;
    return accepted;
}

float TrackRubberState::transferLooseRubber(
    const heritage::math::DVec3& sourcePosition,
    const heritage::math::DVec3& destinationPosition,
    SurfaceMaterial material,
    float requestedQuantity)
{
    if (requestedQuantity <= 0.0f)
        return 0.0f;
    const Address sourceAddress = addressFor(sourcePosition, material);
    const Address destinationAddress = addressFor(destinationPosition, material);
    if (!sourceAddress.valid || !destinationAddress.valid
        || (sourceAddress.chunk == destinationAddress.chunk
            && sourceAddress.cell == destinationAddress.cell))
    {
        return 0.0f;
    }

    // Make sure the destination exists before taking any references; insertion
    // may rehash chunk/cell containers.
    acquireCell(destinationPosition, material);

    auto sourceChunk = m_chunks.find(sourceAddress.chunk);
    auto destinationChunk = m_chunks.find(destinationAddress.chunk);
    if (sourceChunk == m_chunks.end() || destinationChunk == m_chunks.end())
        return 0.0f;
    auto sourceIt = sourceChunk->second.cells.find(sourceAddress.cell);
    auto destinationIt = destinationChunk->second.cells.find(destinationAddress.cell);
    if (sourceIt == sourceChunk->second.cells.end()
        || destinationIt == destinationChunk->second.cells.end())
    {
        return 0.0f;
    }

    const float sourcePiecesBeforeResolve = sourceIt->second.persistentPiecePopulation;
    sourceIt->second = resolvedCell(sourceIt->second);
    m_persistentPiecePopulation = std::max(
        0.0,
        m_persistentPiecePopulation
            + static_cast<double>(sourceIt->second.persistentPiecePopulation
                - sourcePiecesBeforeResolve));
    const float destinationPiecesBeforeResolve = destinationIt->second.persistentPiecePopulation;
    destinationIt->second = resolvedCell(destinationIt->second);
    m_persistentPiecePopulation = std::max(
        0.0,
        m_persistentPiecePopulation
            + static_cast<double>(destinationIt->second.persistentPiecePopulation
                - destinationPiecesBeforeResolve));

    Cell sourceSnapshot = sourceIt->second;
    const float available = std::max(sourceSnapshot.looseRubber, 0.0f);
    const float capacity = std::max(1.0f - destinationIt->second.looseRubber, 0.0f);
    const float moved = std::min({ requestedQuantity, available, capacity });
    if (moved <= 1.0e-9f)
        return 0.0f;

    const float movedPieces = sourceSnapshot.persistentPiecePopulation
        * (moved / std::max(available, 1.0e-8f));
    sourceIt->second.looseRubber = clamp01(sourceIt->second.looseRubber - moved);
    sourceIt->second.persistentPiecePopulation = std::max(
        sourceIt->second.persistentPiecePopulation - movedPieces, 0.0f);
    m_persistentPiecePopulation = std::max(
        0.0, m_persistentPiecePopulation - static_cast<double>(movedPieces));
    if (sourceIt->second.looseRubber <= 1.0e-6f)
    {
        sourceIt->second.marbleMaturity = 0.0f;
        sourceIt->second.fragmentSeverity = 0.0f;
        sourceIt->second.persistentPiecePopulation = 0.0f;
    }
    sourceIt->second.updateSerial = ++m_updateSerial;
    sourceIt->second.timeAtWriteSeconds = m_elapsedSeconds;
    sourceIt->second.wetExposureAtWriteSeconds = m_globalWetExposureSeconds;

    const float accepted = addLooseAbsolute(
        destinationPosition, material, moved,
        sourceSnapshot.marbleMaturity, sourceSnapshot.fragmentSeverity,
        movedPieces, sourceSnapshot.normal, sourceSnapshot.forward);
    if (accepted + 1.0e-7f < moved)
    {
        // No material is allowed to vanish because a destination filled between
        // planning and commit. Put any unaccepted remainder back at the source.
        const float fraction = (moved - accepted) / moved;
        addLooseAbsolute(
            sourcePosition, material, moved - accepted,
            sourceSnapshot.marbleMaturity, sourceSnapshot.fragmentSeverity,
            movedPieces * fraction, sourceSnapshot.normal, sourceSnapshot.forward);
    }
    return accepted;
}

float TrackRubberState::liftLooseRubber(
    const heritage::math::DVec3& sourcePosition,
    SurfaceMaterial material,
    float requestedQuantity,
    const heritage::math::Vec3& initialVelocityMps,
    std::uint32_t seed)
{
    if (requestedQuantity <= 0.0f
        || m_transientPackets.size() >= m_description.maximumTransientPacketCount)
    {
        return 0.0f;
    }
    const Address sourceAddress = addressFor(sourcePosition, material);
    if (!sourceAddress.valid)
        return 0.0f;
    auto sourceChunk = m_chunks.find(sourceAddress.chunk);
    if (sourceChunk == m_chunks.end())
        return 0.0f;
    auto sourceIt = sourceChunk->second.cells.find(sourceAddress.cell);
    if (sourceIt == sourceChunk->second.cells.end())
        return 0.0f;

    const float piecesBeforeResolve = sourceIt->second.persistentPiecePopulation;
    sourceIt->second = resolvedCell(sourceIt->second);
    m_persistentPiecePopulation = std::max(
        0.0,
        m_persistentPiecePopulation
            + static_cast<double>(sourceIt->second.persistentPiecePopulation
                - piecesBeforeResolve));
    const Cell sourceSnapshot = sourceIt->second;
    const float available = std::max(sourceSnapshot.looseRubber, 0.0f);
    const float lifted = std::min(requestedQuantity, available);
    if (lifted <= 1.0e-9f)
        return 0.0f;

    const float liftedPieces = sourceSnapshot.persistentPiecePopulation
        * (lifted / std::max(available, 1.0e-8f));
    sourceIt->second.looseRubber = clamp01(sourceIt->second.looseRubber - lifted);
    sourceIt->second.persistentPiecePopulation = std::max(
        sourceIt->second.persistentPiecePopulation - liftedPieces, 0.0f);
    m_persistentPiecePopulation = std::max(
        0.0, m_persistentPiecePopulation - static_cast<double>(liftedPieces));
    if (sourceIt->second.looseRubber <= 1.0e-6f)
    {
        sourceIt->second.marbleMaturity = 0.0f;
        sourceIt->second.fragmentSeverity = 0.0f;
        sourceIt->second.persistentPiecePopulation = 0.0f;
    }
    sourceIt->second.updateSerial = ++m_updateSerial;

    TransientPacket packet;
    packet.material = material;
    packet.quantity = lifted;
    packet.marbleMaturity = clamp01(sourceSnapshot.marbleMaturity);
    packet.fragmentSeverity = clamp01(sourceSnapshot.fragmentSeverity);
    packet.piecePopulation = liftedPieces;
    packet.supportPlanePoint = sourcePosition;
    packet.supportPlaneNormal = normalize(sourceSnapshot.normal, { 0.0f, 1.0f, 0.0f });
    packet.supportForward = normalize(sourceSnapshot.forward, { 0.0f, 0.0f, 1.0f });
    packet.globalPosition = sourcePosition;
    packet.globalPosition.x += static_cast<double>(packet.supportPlaneNormal.x) * 0.008;
    packet.globalPosition.y += static_cast<double>(packet.supportPlaneNormal.y) * 0.008;
    packet.globalPosition.z += static_cast<double>(packet.supportPlaneNormal.z) * 0.008;
    packet.velocityMps = initialVelocityMps;
    packet.seed = seed != 0u ? seed : mixSeed(++m_updateSerial, 0xa51du);

    std::uint32_t randomState = packet.seed;
    heritage::math::Vec3 forward = packet.supportForward;
    heritage::math::Vec3 right = normalize(
        cross(packet.supportPlaneNormal, forward), { 1.0f, 0.0f, 0.0f });
    forward = normalize(cross(right, packet.supportPlaneNormal), forward);
    packet.axisRight = right;
    packet.axisForward = forward;
    packet.axisNormal = packet.supportPlaneNormal;
    const float angle = random01(randomState) * 6.28318530718f;
    packet.axisForward = normalize(
        add(scale(forward, std::cos(angle)), scale(right, std::sin(angle))), forward);
    packet.axisRight = normalize(
        cross(packet.axisNormal, packet.axisForward), right);
    packet.lengthM = (0.014f + 0.034f * random01(randomState))
        * (0.85f + 0.85f * packet.fragmentSeverity);
    packet.widthM = (0.004f + 0.008f * random01(randomState))
        * (0.90f + 0.55f * packet.fragmentSeverity);
    packet.angularVelocityRadPerSecond = {
        (random01(randomState) * 2.0f - 1.0f) * 16.0f,
        (random01(randomState) * 2.0f - 1.0f) * 12.0f,
        (random01(randomState) * 2.0f - 1.0f) * 18.0f
    };
    packet.flutterPhase = random01(randomState) * 6.28318530718f;
    m_transientPackets.push_back(packet);
    m_transientPiecePopulation += static_cast<double>(liftedPieces);
    m_transientLooseQuantity += static_cast<double>(lifted);
    return lifted;
}

void TrackRubberState::enqueueFreshTransient(
    const heritage::math::DVec3& sourcePosition,
    const TrackRubberContactInput& input,
    float quantity,
    float maturity,
    float fragmentSeverity,
    float pieceDensity)
{
    if (quantity <= 0.0f || !rubberCapableSurface(input.material))
        return;

    const heritage::math::Vec3 normal = normalize(input.normal, { 0.0f, 1.0f, 0.0f });
    heritage::math::Vec3 forward = normalize(input.forward, { 0.0f, 0.0f, 1.0f });
    const heritage::math::Vec3 right = normalize(cross(normal, forward), { 1.0f, 0.0f, 0.0f });
    forward = normalize(cross(right, normal), forward);
    const float speed = std::abs(input.forwardSpeedMps);
    const float rearwardSign = input.forwardSpeedMps >= 0.0f ? -1.0f : 1.0f;
    const float severity = clamp01(fragmentSeverity);
    // TIRE15C5A: give a freshly torn flake enough ballistic separation to be
    // visually readable behind a moving tire. C5's ~centimetre-high hop was
    // physically possible but nearly impossible to see from the chase camera.
    // The lab generation multiplier changes *how much* rubber is shed, never
    // this launch velocity, so 1000x remains a quantity stress-test rather than
    // a cartoon impulse multiplier.
    heritage::math::Vec3 velocity = add(
        scale(forward, rearwardSign * (0.30f + speed * (0.055f + 0.022f * severity))),
        add(
            scale(right, input.lateralSlipSpeedMps * 0.16f),
            scale(normal, 0.18f + 0.62f * severity + speed * 0.012f)));

    const double availablePieces = std::max(
        static_cast<double>(m_description.maximumPersistentPieceCount)
            - (m_persistentPiecePopulation + m_transientPiecePopulation),
        0.0);
    const float requestedPieces = std::max(quantity * std::max(pieceDensity, 0.0f), 0.0f);
    const float acceptedPieces = static_cast<float>(std::min(
        static_cast<double>(requestedPieces), availablePieces));

    // High-rate tire contacts merge into a nearby packet for a few milliseconds.
    // This keeps 1000 Hz tire simulation from manufacturing 1000 tiny objects/s.
    for (auto it = m_transientPackets.rbegin(); it != m_transientPackets.rend(); ++it)
    {
        if (it->phase != TrackRubberTransientPhase::Airborne
            || it->material != input.material || it->ageSeconds > 0.040f)
        {
            continue;
        }
        const double dx = it->supportPlanePoint.x - sourcePosition.x;
        const double dy = it->supportPlanePoint.y - sourcePosition.y;
        const double dz = it->supportPlanePoint.z - sourcePosition.z;
        const double mergeRadius = static_cast<double>(m_description.cellSizeM * 0.55f);
        if (dx * dx + dy * dy + dz * dz > mergeRadius * mergeRadius)
            continue;

        const float oldQuantity = it->quantity;
        const float combined = oldQuantity + quantity;
        if (combined > 1.0e-9f)
        {
            it->velocityMps = scale(
                add(scale(it->velocityMps, oldQuantity), scale(velocity, quantity)),
                1.0f / combined);
            it->marbleMaturity = clamp01(
                (it->marbleMaturity * oldQuantity + clamp01(maturity) * quantity) / combined);
            it->fragmentSeverity = clamp01(
                (it->fragmentSeverity * oldQuantity + severity * quantity) / combined);
        }
        it->quantity = combined;
        it->piecePopulation += acceptedPieces;
        m_transientPiecePopulation += static_cast<double>(acceptedPieces);
        m_transientLooseQuantity += static_cast<double>(quantity);
        return;
    }

    if (m_transientPackets.size() >= m_description.maximumTransientPacketCount)
    {
        // Bounded fallback: if the moving-packet pool is saturated, analytically
        // settle this aggregate at the support point rather than dropping mass.
        addLooseAbsolute(
            sourcePosition, input.material, quantity, maturity, severity,
            acceptedPieces, normal, forward);
        return;
    }

    TransientPacket packet;
    packet.material = input.material;
    packet.quantity = quantity;
    packet.marbleMaturity = clamp01(maturity);
    packet.fragmentSeverity = severity;
    packet.piecePopulation = acceptedPieces;
    packet.supportPlanePoint = sourcePosition;
    packet.supportPlaneNormal = normal;
    packet.supportForward = forward;
    packet.globalPosition = sourcePosition;
    packet.globalPosition.x += static_cast<double>(normal.x) * 0.012;
    packet.globalPosition.y += static_cast<double>(normal.y) * 0.012;
    packet.globalPosition.z += static_cast<double>(normal.z) * 0.012;
    packet.velocityMps = velocity;
    packet.seed = mixSeed(++m_updateSerial, 0x51ed5u);
    std::uint32_t randomState = packet.seed;
    const float angle = random01(randomState) * 6.28318530718f;
    packet.axisForward = normalize(
        add(scale(forward, std::cos(angle)), scale(right, std::sin(angle))), forward);
    packet.axisRight = normalize(cross(normal, packet.axisForward), right);
    packet.axisNormal = normal;
    packet.lengthM = (0.016f + random01(randomState) * 0.034f)
        * (0.82f + 0.90f * severity);
    packet.widthM = (0.0042f + random01(randomState) * 0.0065f)
        * (0.88f + 0.62f * severity);
    packet.angularVelocityRadPerSecond = {
        (random01(randomState) * 2.0f - 1.0f) * (12.0f + 10.0f * severity),
        (random01(randomState) * 2.0f - 1.0f) * (8.0f + 8.0f * severity),
        (random01(randomState) * 2.0f - 1.0f) * (14.0f + 12.0f * severity)
    };
    packet.flutterPhase = random01(randomState) * 6.28318530718f;
    m_transientPackets.push_back(packet);
    m_transientPiecePopulation += static_cast<double>(acceptedPieces);
    m_transientLooseQuantity += static_cast<double>(quantity);
}

float TrackRubberState::commitTransient(const TransientPacket& packet)
{
    if (packet.quantity <= 0.0f)
        return 0.0f;

    heritage::math::Vec3 normal = normalize(
        packet.supportPlaneNormal, { 0.0f, 1.0f, 0.0f });
    heritage::math::Vec3 forward = normalize(
        packet.supportForward, { 0.0f, 0.0f, 1.0f });
    heritage::math::Vec3 right = normalize(cross(normal, forward), { 1.0f, 0.0f, 0.0f });
    forward = normalize(cross(right, normal), forward);

    const float cell = m_description.cellSizeM;
    const heritage::math::Vec3 offsets[] = {
        { 0.0f, 0.0f, 0.0f },
        scale(right, 0.72f * cell), scale(right, -0.72f * cell),
        scale(forward, 0.72f * cell), scale(forward, -0.72f * cell),
        add(scale(right, 0.72f * cell), scale(forward, 0.72f * cell)),
        add(scale(right, -0.72f * cell), scale(forward, 0.72f * cell)),
        add(scale(right, 0.72f * cell), scale(forward, -0.72f * cell)),
        add(scale(right, -0.72f * cell), scale(forward, -0.72f * cell))
    };

    float remainingQuantity = packet.quantity;
    float remainingPieces = packet.piecePopulation;
    float acceptedTotal = 0.0f;
    for (const auto& offset : offsets)
    {
        if (remainingQuantity <= 1.0e-8f)
            break;
        heritage::math::DVec3 destination{
            packet.globalPosition.x + static_cast<double>(offset.x),
            packet.globalPosition.y + static_cast<double>(offset.y),
            packet.globalPosition.z + static_cast<double>(offset.z)
        };
        // Snap the packet back onto its known support plane before becoming
        // static state. This avoids small ballistic integration error in Y.
        const heritage::math::DVec3 delta{
            destination.x - packet.supportPlanePoint.x,
            destination.y - packet.supportPlanePoint.y,
            destination.z - packet.supportPlanePoint.z
        };
        const double height = delta.x * normal.x + delta.y * normal.y + delta.z * normal.z;
        destination.x -= static_cast<double>(normal.x) * height;
        destination.y -= static_cast<double>(normal.y) * height;
        destination.z -= static_cast<double>(normal.z) * height;

        const float requestedPieces = remainingPieces;
        const float accepted = addLooseAbsolute(
            destination, packet.material, remainingQuantity,
            packet.marbleMaturity, packet.fragmentSeverity,
            requestedPieces, normal, forward);
        if (accepted <= 0.0f)
            continue;
        const float fraction = accepted / std::max(remainingQuantity, 1.0e-8f);
        remainingPieces *= (1.0f - fraction);
        remainingQuantity -= accepted;
        acceptedTotal += accepted;
    }
    return acceptedTotal;
}

void TrackRubberState::integrateTransients(float deltaTimeSeconds)
{
    if (m_transientPackets.empty() || deltaTimeSeconds <= 0.0f)
        return;
    const float dt = std::min(deltaTimeSeconds, 0.02f);

    for (std::size_t index = 0; index < m_transientPackets.size(); )
    {
        TransientPacket& packet = m_transientPackets[index];
        packet.ageSeconds += dt;
        const heritage::math::Vec3 supportNormal = normalize(
            packet.supportPlaneNormal, { 0.0f, 1.0f, 0.0f });

        if (packet.phase == TrackRubberTransientPhase::Airborne)
        {
            packet.velocityMps.y -= 9.80665f * dt;
            // Thin rubber flakes have appreciable drag but are not feathers.
            const float dragRate = 0.62f + 0.55f * (1.0f - packet.marbleMaturity);
            const float drag = std::exp(-dragRate * dt);
            packet.velocityMps = scale(packet.velocityMps, drag);

            packet.globalPosition.x += static_cast<double>(packet.velocityMps.x * dt);
            packet.globalPosition.y += static_cast<double>(packet.velocityMps.y * dt);
            packet.globalPosition.z += static_cast<double>(packet.velocityMps.z * dt);

            // Integrate a tiny orientation basis directly. This is not rigid-body
            // physics; it is just enough rotational state for convincing tumble.
            packet.axisRight = normalize(
                add(packet.axisRight,
                    scale(cross(packet.angularVelocityRadPerSecond, packet.axisRight), dt)),
                packet.axisRight);
            packet.axisForward = normalize(
                add(packet.axisForward,
                    scale(cross(packet.angularVelocityRadPerSecond, packet.axisForward), dt)),
                packet.axisForward);
            packet.axisNormal = normalize(cross(packet.axisForward, packet.axisRight), packet.axisNormal);
            packet.axisRight = normalize(cross(packet.axisNormal, packet.axisForward), packet.axisRight);

            const float relativeAirSpeed = vectorLength(packet.velocityMps);
            packet.flutterPhase += dt * (9.0f + relativeAirSpeed * 2.6f);
            const float flutterAmplitude = std::min(
                packet.widthM * 0.55f,
                (0.0007f + 0.00012f * relativeAirSpeed)
                    * (1.0f + 0.45f * packet.fragmentSeverity));
            packet.bendVertex1M = std::sin(packet.flutterPhase) * flutterAmplitude;
            packet.bendVertex3M = std::sin(packet.flutterPhase * 1.37f + 1.2f)
                * flutterAmplitude * 0.85f;

            const heritage::math::DVec3 delta{
                packet.globalPosition.x - packet.supportPlanePoint.x,
                packet.globalPosition.y - packet.supportPlanePoint.y,
                packet.globalPosition.z - packet.supportPlanePoint.z
            };
            const double signedHeight = delta.x * supportNormal.x
                + delta.y * supportNormal.y + delta.z * supportNormal.z;
            const float normalVelocity = dot(packet.velocityMps, supportNormal);
            if (signedHeight <= 0.0035 && normalVelocity < 0.0f)
            {
                const double correction = 0.0035 - signedHeight;
                packet.globalPosition.x += static_cast<double>(supportNormal.x) * correction;
                packet.globalPosition.y += static_cast<double>(supportNormal.y) * correction;
                packet.globalPosition.z += static_cast<double>(supportNormal.z) * correction;

                const float impactSpeed = -normalVelocity;
                const heritage::math::Vec3 tangentVelocity = subtract(
                    packet.velocityMps, scale(supportNormal, normalVelocity));
                if (impactSpeed > 1.15f && packet.ageSeconds < 1.4f)
                {
                    packet.velocityMps = add(
                        scale(tangentVelocity, 0.72f),
                        scale(supportNormal, impactSpeed * 0.16f));
                    packet.angularVelocityRadPerSecond = scale(
                        packet.angularVelocityRadPerSecond, 0.82f);
                }
                else
                {
                    packet.phase = TrackRubberTransientPhase::MobileGround;
                    packet.groundTimeSeconds = 0.0f;
                    packet.velocityMps = scale(tangentVelocity, 0.58f);
                    packet.angularVelocityRadPerSecond = scale(
                        packet.angularVelocityRadPerSecond, 0.45f);
                }
            }
        }
        else
        {
            packet.groundTimeSeconds += dt;
            const heritage::math::Vec3 gravity{ 0.0f, -9.80665f, 0.0f };
            const heritage::math::Vec3 tangentialGravity = subtract(
                gravity, scale(supportNormal, dot(gravity, supportNormal)));
            packet.velocityMps = add(packet.velocityMps, scale(tangentialGravity, dt));
            const float groundDragRate = 6.5f - 2.5f * packet.marbleMaturity;
            packet.velocityMps = scale(packet.velocityMps, std::exp(-groundDragRate * dt));
            packet.globalPosition.x += static_cast<double>(packet.velocityMps.x * dt);
            packet.globalPosition.y += static_cast<double>(packet.velocityMps.y * dt);
            packet.globalPosition.z += static_cast<double>(packet.velocityMps.z * dt);

            const heritage::math::DVec3 delta{
                packet.globalPosition.x - packet.supportPlanePoint.x,
                packet.globalPosition.y - packet.supportPlanePoint.y,
                packet.globalPosition.z - packet.supportPlanePoint.z
            };
            const double height = delta.x * supportNormal.x
                + delta.y * supportNormal.y + delta.z * supportNormal.z;
            packet.globalPosition.x += static_cast<double>(supportNormal.x) * (0.0025 - height);
            packet.globalPosition.y += static_cast<double>(supportNormal.y) * (0.0025 - height);
            packet.globalPosition.z += static_cast<double>(supportNormal.z) * (0.0025 - height);

            const float align = 1.0f - std::exp(-10.0f * dt);
            packet.axisNormal = normalize(
                add(scale(packet.axisNormal, 1.0f - align), scale(supportNormal, align)),
                supportNormal);
            packet.axisForward = normalize(
                subtract(packet.axisForward,
                    scale(packet.axisNormal, dot(packet.axisForward, packet.axisNormal))),
                packet.supportForward);
            packet.axisRight = normalize(
                cross(packet.axisNormal, packet.axisForward), { 1.0f, 0.0f, 0.0f });
            packet.bendVertex1M *= std::exp(-8.0f * dt);
            packet.bendVertex3M *= std::exp(-8.0f * dt);
        }

        const bool wantsCommit =
            (packet.phase == TrackRubberTransientPhase::MobileGround
                && packet.groundTimeSeconds > 0.07f
                && vectorLength(packet.velocityMps) < 0.10f)
            || packet.ageSeconds > 4.0f;
        if (wantsCommit)
        {
            const float oldQuantity = packet.quantity;
            const float oldPieces = packet.piecePopulation;
            const float accepted = commitTransient(packet);
            if (accepted > 0.0f)
            {
                const float fraction = std::clamp(
                    accepted / std::max(oldQuantity, 1.0e-8f), 0.0f, 1.0f);
                const float committedPieces = oldPieces * fraction;
                packet.quantity = std::max(oldQuantity - accepted, 0.0f);
                packet.piecePopulation = std::max(oldPieces - committedPieces, 0.0f);
                m_transientLooseQuantity = std::max(
                    0.0, m_transientLooseQuantity - static_cast<double>(accepted));
                m_transientPiecePopulation = std::max(
                    0.0, m_transientPiecePopulation - static_cast<double>(committedPieces));
            }
            if (packet.quantity <= 1.0e-7f)
            {
                m_transientPackets[index] = m_transientPackets.back();
                m_transientPackets.pop_back();
                continue;
            }
            // A completely saturated neighborhood keeps the residual packet
            // mobile instead of silently destroying it.
            packet.phase = TrackRubberTransientPhase::MobileGround;
            packet.velocityMps = scale(packet.axisRight, 0.16f);
            packet.groundTimeSeconds = 0.0f;
        }
        ++index;
    }
}

void TrackRubberState::applyRubberDelta(
    const heritage::math::DVec3& globalPosition,
    const TrackRubberContactInput& input,
    float depositedDelta,
    float looseDelta,
    float looseSourceMaturity,
    float looseSourceFragmentSeverity,
    float looseSourcePieceDensity,
    float maturationDelta,
    float passProgressDelta,
    bool recordContact)
{
    if (depositedDelta == 0.0f && looseDelta == 0.0f
        && maturationDelta <= 0.0f && passProgressDelta <= 0.0f)
    {
        return;
    }

    Cell& cell = acquireCell(globalPosition, input.material);
    const float localWetness = clamp01(input.wetness);
    if (localWetness > 0.0f && input.deltaTimeSeconds > 0.0f)
    {
        const double localWetSeconds = static_cast<double>(localWetness)
            * static_cast<double>(input.deltaTimeSeconds);
        cell.depositedRubber = clamp01(static_cast<float>(
            static_cast<double>(cell.depositedRubber)
            * std::exp(-m_description.depositedRainWashRatePerWetSecond
                * localWetSeconds)));
        const double looseWash = std::exp(
            -m_description.looseRainWashRatePerWetSecond * localWetSeconds);
        cell.looseRubber = clamp01(static_cast<float>(
            static_cast<double>(cell.looseRubber) * looseWash));
        const float piecesBeforeWash = cell.persistentPiecePopulation;
        cell.persistentPiecePopulation = std::max(
            0.0f,
            static_cast<float>(
                static_cast<double>(cell.persistentPiecePopulation) * looseWash));
        m_persistentPiecePopulation = std::max(
            0.0,
            m_persistentPiecePopulation
                + static_cast<double>(
                    cell.persistentPiecePopulation - piecesBeforeWash));
    }

    cell.depositedRubber = clamp01(
        cell.depositedRubber + std::max(depositedDelta, 0.0f)
            * (1.0f - cell.depositedRubber));
    if (looseDelta >= 0.0f)
    {
        const float oldLoose = cell.looseRubber;
        const float actualAdded = std::max(looseDelta, 0.0f) * (1.0f - oldLoose);
        const float newLoose = clamp01(oldLoose + actualAdded);
        if (actualAdded > 0.0f && newLoose > 1.0e-7f)
        {
            const float sourceMaturity = clamp01(looseSourceMaturity);
            const float sourceSeverity = clamp01(looseSourceFragmentSeverity);
            cell.marbleMaturity = clamp01(
                (oldLoose * clamp01(cell.marbleMaturity)
                    + actualAdded * sourceMaturity)
                / newLoose);
            cell.fragmentSeverity = clamp01(
                (oldLoose * clamp01(cell.fragmentSeverity)
                    + actualAdded * sourceSeverity)
                / newLoose);

            const float sourcePieceDensity = std::clamp(
                std::isfinite(looseSourcePieceDensity)
                    ? looseSourcePieceDensity : m_description.nominalPiecesPerLooseUnit,
                0.0f,
                m_description.nominalPiecesPerLooseUnit * 4.0f);
            const double requestedPieces =
                static_cast<double>(actualAdded * sourcePieceDensity);
            const double availablePieces = std::max(
                static_cast<double>(m_description.maximumPersistentPieceCount)
                    - (m_persistentPiecePopulation + m_transientPiecePopulation),
                0.0);
            const float acceptedPieces = static_cast<float>(
                std::min(requestedPieces, availablePieces));
            cell.persistentPiecePopulation += acceptedPieces;
            m_persistentPiecePopulation += static_cast<double>(acceptedPieces);
        }
        cell.looseRubber = newLoose;
    }
    else
    {
        const float oldLoose = cell.looseRubber;
        const float oldPieces = cell.persistentPiecePopulation;
        cell.looseRubber = clamp01(cell.looseRubber + looseDelta);
        if (oldLoose > 1.0e-7f && cell.looseRubber < oldLoose)
        {
            const float retainedFraction = std::clamp(
                cell.looseRubber / oldLoose, 0.0f, 1.0f);
            cell.persistentPiecePopulation *= retainedFraction;
            m_persistentPiecePopulation = std::max(
                0.0,
                m_persistentPiecePopulation
                    + static_cast<double>(
                        cell.persistentPiecePopulation - oldPieces));
        }
        if (cell.looseRubber <= 1.0e-6f)
        {
            cell.marbleMaturity = 0.0f;
            cell.fragmentSeverity = 0.0f;
            cell.persistentPiecePopulation = 0.0f;
        }
    }
    if (maturationDelta > 0.0f && cell.looseRubber > 1.0e-6f)
    {
        cell.marbleMaturity = clamp01(
            cell.marbleMaturity
            + maturationDelta * (1.0f - cell.marbleMaturity));
    }
    // Anchor presentation geometry to the first physical support frame for
    // this world cell. Repeated passes may arrive with a different steering
    // direction and must not make already-resting debris rise/sink/tilt.
    if (cell.updateSerial == 0u)
    {
        cell.surfaceHeightM = globalPosition.y;
        cell.normal = normalize(input.normal, { 0.0f, 1.0f, 0.0f });
        cell.forward = normalize(input.forward, { 0.0f, 0.0f, 1.0f });
    }
    cell.material = input.material;
    cell.updateSerial = ++m_updateSerial;
    cell.timeAtWriteSeconds = m_elapsedSeconds;
    cell.wetExposureAtWriteSeconds = m_globalWetExposureSeconds;

    if (passProgressDelta > 0.0f)
    {
        cell.passProgress += passProgressDelta;
        while (cell.passProgress >= 1.0f)
        {
            ++cell.passCount;
            cell.passProgress -= 1.0f;
        }
    }
    if (recordContact)
        ++m_contactSamples;
}

TrackRubberSample TrackRubberState::applyContact(
    const heritage::math::DVec3& globalPosition,
    const TrackRubberContactInput& input)
{
    if (!rubberCapableSurface(input.material)
        || !std::isfinite(globalPosition.x)
        || !std::isfinite(globalPosition.y)
        || !std::isfinite(globalPosition.z)
        || !std::isfinite(input.deltaTimeSeconds)
        || input.deltaTimeSeconds <= 0.0f)
    {
        return sample(globalPosition, input.material, input.wetness);
    }

    const float dt = std::clamp(input.deltaTimeSeconds, 0.0f, 0.05f);
    const float speed = std::abs(input.forwardSpeedMps);
    // Speed alone never tears off visible marbles. Motion is only a weak
    // bonded-rubber transfer signal; loose debris requires tire stress/wear.
    const float motion = clamp01((speed - 1.5f) / 30.0f);
    const float nominalLoad = std::max(input.nominalLoadN, 250.0f);
    const float loadScale = std::clamp(input.normalLoadN / nominalLoad, 0.0f, 2.25f);
    const float slipSpeed = std::sqrt(
        input.longitudinalSlipSpeedMps * input.longitudinalSlipSpeedMps
        + input.lateralSlipSpeedMps * input.lateralSlipSpeedMps);
    const float slipSeverity = clamp01((slipSpeed - 0.25f) / 2.75f);
    const float wetness = clamp01(input.wetness);

    const float temperatureDelta = (input.treadTemperatureC - 78.0f) / 36.0f;
    const float tackFactor = std::clamp(
        0.35f + 0.85f * std::exp(-(temperatureDelta * temperatureDelta)),
        0.30f,
        1.20f);
    const float slipPowerSignal = clamp01(
        (std::max(input.slipDissipationWatts, 0.0f) - 250.0f) / 10000.0f);
    const float wearRateMPerSecond = std::max(input.treadWearDepthDeltaM, 0.0f)
        / std::max(dt, 1.0e-6f);
    const float wearSignal = clamp01(
        (wearRateMPerSecond - 1.0e-8f) / 2.0e-5f);

    // TIRE15C2: no artificial "70% tire remaining" switch. Fresh tires can
    // shed when sufficiently stressed, while a worn tread becomes gradually
    // more susceptible. Compound shedding propensity is explicit authoring
    // data rather than inferred from a brand name or final friction value.
    const float treadWearFraction = clamp01(input.treadWearFraction);
    const float wearSusceptibility = 0.48f
        + 0.72f * std::pow(treadWearFraction, 0.72f);
    const float compoundShedding = std::clamp(
        std::isfinite(input.compoundSheddingFactor)
            ? input.compoundSheddingFactor : 1.0f,
        0.20f, 3.0f);
    const float generationMultiplier = std::clamp(
        std::isfinite(input.generationMultiplier)
            ? input.generationMultiplier : 1.0f,
        0.0f, 1000.0f);
    const float maturationMultiplier = std::clamp(
        std::isfinite(input.maturationMultiplier)
            ? input.maturationMultiplier : 1.0f,
        0.0f, 1000.0f);

    // Rates are per second and multiplied by dt exactly once. Normal rolling
    // can slowly rubber-in a line; loose debris is driven by real scrub/slip
    // work, measured wear and compound/tread susceptibility.
    const float depositionRate =
        (0.0015f * motion
            + 0.015f * slipSeverity
            + 0.025f * slipPowerSignal
            + 0.020f * wearSignal)
        * loadScale * tackFactor * (1.0f - 0.72f * wetness);
    const float depositedGenerated = std::max(
        dt * depositionRate * generationMultiplier, 0.0f);

    const float slipTearSignal = std::pow(slipSeverity, 1.35f);
    const float powerTearSignal = std::pow(slipPowerSignal, 1.20f);
    // TIRE16G calibration: repeated real driving tests showed the TIRE16D
    // 0.12 production baseline was still too sparse for a continuously worked
    // road tire. Raise normal 1x generation by exactly 3x to 0.36. This remains
    // well below C5's original 1.0 flood while making multi-minute cornering and
    // sliding capable of producing visible marbles on a useful timescale. The
    // Tire Lab multiplier remains on top for deliberate stress testing.
    constexpr float kLooseRubberProductionCalibration = 0.360f;
    const float looseRate =
        (0.012f * slipTearSignal
            + 0.022f * powerTearSignal
            + 0.030f * wearSignal)
        * loadScale * tackFactor * wearSusceptibility * compoundShedding
        * (1.0f - 0.45f * wetness)
        * kLooseRubberProductionCalibration;
    const float freshLooseGenerated = std::max(
        dt * looseRate * generationMultiplier, 0.0f);

    // Fresh fragment character records how violently this material was torn
    // from the tire. Severe slip/power/wear/overheat biases the distribution
    // toward fewer, larger chunks rather than simply scaling every piece.
    const float thermalAbuse = clamp01(
        (input.treadTemperatureC - 100.0f) / 45.0f);
    const float freshFragmentSeverity = clamp01(
        0.42f * slipSeverity
        + 0.30f * slipPowerSignal
        + 0.18f * wearSignal
        + 0.10f * thermalAbuse);
    const float freshPieceDensity = std::clamp(
        m_description.nominalPiecesPerLooseUnit
            * (1.08f - 0.48f * freshFragmentSeverity),
        8.0f,
        m_description.nominalPiecesPerLooseUnit * 1.20f);

    // A passing tire keeps the repeatedly driven centre line comparatively
    // clean by sweeping loose rubber toward the shoulders. Mature rounded
    // pieces are more mobile than fresh tacky shreds, so an off-line car can
    // visibly/physically reshuffle an established marble band.
    TrackRubberSample before = sample(globalPosition, input.material, wetness);
    const float mobility = 0.55f + 0.65f * clamp01(before.marbleMaturity);
    const float sweepFraction = clamp01(
        dt * (0.035f + speed * 0.006f + slipSeverity * 0.22f) * mobility);
    const float sweptLoose = before.looseRubber * sweepFraction;

    // A contacting tire also removes a small amount through pickup. Fresh
    // tacky shreds transfer more readily than rounded mature marbles. The tire
    // contamination model independently consumes the same availability signal;
    // this term keeps the shared world reservoir from being inexhaustible.
    const float pickupRate = (0.018f + 0.075f * tackFactor)
        * loadScale * (0.35f + 0.65f * motion)
        * (1.0f - 0.42f * clamp01(before.marbleMaturity));
    const float pickedLoose = before.looseRubber
        * clamp01(dt * pickupRate);

    // Marble maturity is physical track state, not elapsed-time animation.
    // Repeated tire traffic, agitation/slip, tack and concentration progressively
    // clump fresh shreds/flakes into rounder mature marbles. With no traffic a
    // pile simply sits where it was left (apart from slow ageing/rain wash).
    const float concentration = clamp01(before.looseRubber * 5.0f);
    const float maturationRate =
        (0.020f + 0.13f * motion + 0.20f * slipSeverity
            + 0.14f * slipPowerSignal)
        * loadScale * tackFactor * (0.18f + 0.82f * concentration)
        * maturationMultiplier;
    const float maturationDelta = std::max(dt * maturationRate, 0.0f);
    constexpr float kFreshShredMaturity = 0.035f;

    // First mature the material actually under the tire. Pickup genuinely
    // removes material onto the tire; sweeping is handled separately below as
    // conservative cell-to-cell transfer, so swept rubber cannot disappear.
    applyRubberDelta(
        globalPosition, input,
        depositedGenerated, 0.0f, 0.0f, 0.0f, 0.0f, maturationDelta,
        speed * dt / std::max(m_description.cellSizeM, 0.10f),
        true);
    if (pickedLoose > 0.0f)
    {
        applyRubberDelta(
            globalPosition, input,
            0.0f, -pickedLoose, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, false);
    }

    const heritage::math::Vec3 normal = normalize(input.normal, { 0.0f, 1.0f, 0.0f });
    heritage::math::Vec3 forward = normalize(input.forward, { 0.0f, 0.0f, 1.0f });
    const heritage::math::Vec3 right = normalize(
        cross(normal, forward), { 1.0f, 0.0f, 0.0f });
    forward = normalize(cross(right, normal), forward);

    const float lateralBias = std::clamp(
        input.lateralSlipSpeedMps / std::max(speed + 2.0f, 4.0f),
        -0.35f,
        0.35f);
    // TIRE15C5 conservation fix: left + right is exactly one. C4 used 0.46
    // per side, which silently destroyed about 8% of every swept quantity.
    const float leftWeight = 0.50f * (1.0f - lateralBias);
    const float rightWeight = 0.50f * (1.0f + lateralBias);
    const float nearOffset = std::clamp(
        0.42f + input.tireWidthM * 0.75f, 0.42f, 0.75f);
    const float farOffset = nearOffset + 0.52f + 0.18f * clamp01(slipSeverity);

    const auto offsetPosition = [&](float lateralOffset) {
        return heritage::math::DVec3{
            globalPosition.x + static_cast<double>(right.x * lateralOffset),
            globalPosition.y + static_cast<double>(right.y * lateralOffset),
            globalPosition.z + static_cast<double>(right.z * lateralOffset)
        };
    };

    if (sweptLoose > 0.0f)
    {
        const struct SweepTarget { float offset; float fraction; } targets[] = {
            { -nearOffset, leftWeight * 0.78f },
            {  nearOffset, rightWeight * 0.78f },
            { -farOffset, leftWeight * 0.22f },
            {  farOffset, rightWeight * 0.22f }
        };
        for (const SweepTarget& target : targets)
        {
            transferLooseRubber(
                globalPosition, offsetPosition(target.offset), input.material,
                sweptLoose * target.fraction);
        }
    }

    // Fresh tear-off is no longer pre-painted into a shoulder cell. It becomes
    // authoritative moving rubber first; gravity/drag/tumble/ground sliding
    // decide where it finally joins the static track reservoir.
    if (freshLooseGenerated > 0.0f)
    {
        enqueueFreshTransient(
            globalPosition, input, freshLooseGenerated,
            kFreshShredMaturity, freshFragmentSeverity, freshPieceDensity);
    }

    m_depositedGeneration += static_cast<double>(depositedGenerated);
    // Sweeping moves existing material; it is not new generation.
    m_looseGeneration += static_cast<double>(freshLooseGenerated);
    TrackRubberSample result = sample(globalPosition, input.material, wetness);
    result.freshLooseGenerated = freshLooseGenerated;
    result.freshFragmentSeverity = freshFragmentSeverity;
    return result;
}

TrackRubberWakeResult TrackRubberState::applyWake(
    const heritage::math::DVec3& globalPosition,
    const TrackRubberWakeInput& input)
{
    TrackRubberWakeResult result;
    if (!std::isfinite(globalPosition.x) || !std::isfinite(globalPosition.y)
        || !std::isfinite(globalPosition.z) || !std::isfinite(input.deltaTimeSeconds)
        || !std::isfinite(input.speedMps) || input.deltaTimeSeconds <= 0.0f)
    {
        return result;
    }

    const float dt = std::clamp(input.deltaTimeSeconds, 0.0f, 0.05f);
    const float speed = std::abs(input.speedMps);
    const float speedSignal = clamp01((speed - 10.0f) / 62.0f);
    if (speedSignal <= 0.0f)
        return result;

    heritage::math::Vec3 up = normalize(input.up, { 0.0f, 1.0f, 0.0f });
    heritage::math::Vec3 forward = normalize(input.forward, { 0.0f, 0.0f, 1.0f });
    forward = normalize(subtract(forward, scale(up, dot(forward, up))), forward);
    const heritage::math::Vec3 right = normalize(cross(up, forward), { 1.0f, 0.0f, 0.0f });
    forward = normalize(cross(right, up), forward);

    const float width = std::clamp(input.vehicleWidthM, 0.35f, 4.5f);
    const float lengthM = std::clamp(input.vehicleLengthM, 0.8f, 12.0f);
    const float halfWidth = width * 0.5f;
    const float halfLength = lengthM * 0.5f;
    const float rideHeight = std::clamp(input.rideHeightM, 0.04f, 0.80f);
    const float rideHeightScale = std::clamp(0.18f / rideHeight, 0.55f, 1.55f);
    const float loadFactor = input.referenceWeightN > 100.0f
        ? std::clamp(input.normalLoadN / input.referenceWeightN, 0.55f, 2.25f)
        : 1.0f;
    const float aeroFactor = std::clamp(
        std::isfinite(input.aeroWakeFactor) ? input.aeroWakeFactor : 1.0f,
        0.20f, 3.0f);
    const float authority = std::pow(speedSignal, 1.55f)
        * std::sqrt(loadFactor) * aeroFactor;

    const float frontReach = 0.65f + 0.80f * speedSignal;
    const float trailingReach = 2.0f + speed * 0.075f;
    const float sideReach = halfWidth + 0.55f + speed * 0.010f;
    const double searchRadius = static_cast<double>(
        std::max(halfLength + trailingReach, sideReach) + 1.0f);
    const double chunkSizeM = static_cast<double>(m_description.cellSizeM)
        * static_cast<double>(m_description.chunkSizeCells);
    const std::int64_t minChunkX = static_cast<std::int64_t>(
        std::floor((globalPosition.x - searchRadius) / chunkSizeM));
    const std::int64_t maxChunkX = static_cast<std::int64_t>(
        std::floor((globalPosition.x + searchRadius) / chunkSizeM));
    const std::int64_t minChunkZ = static_cast<std::int64_t>(
        std::floor((globalPosition.z - searchRadius) / chunkSizeM));
    const std::int64_t maxChunkZ = static_cast<std::int64_t>(
        std::floor((globalPosition.z + searchRadius) / chunkSizeM));

    struct WakeCandidate
    {
        heritage::math::DVec3 source{};
        heritage::math::DVec3 destination{};
        heritage::math::Vec3 liftVelocity{};
        SurfaceMaterial material = SurfaceMaterial::Default;
        float groundQuantity = 0.0f;
        float liftQuantity = 0.0f;
        std::uint32_t seed = 0;
    };
    std::vector<WakeCandidate> candidates;
    candidates.reserve(256);

    const auto fieldAt = [&](const heritage::math::DVec3& point,
                             float& zoneFactor,
                             float& lateralFactor,
                             float& localLongitudinal,
                             float& localLateral) {
        const heritage::math::Vec3 delta{
            static_cast<float>(point.x - globalPosition.x),
            static_cast<float>(point.y - globalPosition.y),
            static_cast<float>(point.z - globalPosition.z)
        };
        localLongitudinal = dot(delta, forward);
        localLateral = dot(delta, right);
        const float localVertical = dot(delta, up);
        if (std::abs(localVertical) > 1.8f)
            return false;

        float wakeHalfWidth = halfWidth + 0.28f;
        zoneFactor = 0.0f;
        if (localLongitudinal > halfLength)
        {
            const float frontDistance = localLongitudinal - halfLength;
            if (frontDistance > frontReach)
                return false;
            zoneFactor = 0.40f * (1.0f - frontDistance / std::max(frontReach, 0.01f));
            wakeHalfWidth += 0.20f * frontDistance;
        }
        else if (localLongitudinal >= -halfLength)
        {
            zoneFactor = 1.0f;
            wakeHalfWidth += 0.10f + 0.12f * speedSignal;
        }
        else
        {
            const float trailDistance = -halfLength - localLongitudinal;
            if (trailDistance > trailingReach)
                return false;
            wakeHalfWidth += 0.18f * trailDistance + 0.22f * speedSignal;
            zoneFactor = std::exp(
                -trailDistance / std::max(1.0f, trailingReach * 0.58f));
        }
        if (std::abs(localLateral) > wakeHalfWidth)
            return false;
        const float normalizedLateral = std::abs(localLateral)
            / std::max(wakeHalfWidth, 0.05f);
        lateralFactor = std::pow(clamp01(1.0f - normalizedLateral), 1.25f);
        return lateralFactor > 0.0f && zoneFactor > 0.0f;
    };

    for (std::int64_t chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ)
    {
        for (std::int64_t chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX)
        {
            const ChunkKey chunkKey{ chunkX, chunkZ };
            const auto chunk = m_chunks.find(chunkKey);
            if (chunk == m_chunks.end())
                continue;
            for (const auto& entry : chunk->second.cells)
            {
                const Cell cell = resolvedCell(entry.second);
                if (cell.looseRubber <= 0.0002f)
                    continue;

                heritage::math::DVec3 source = cellCenter(chunkKey, entry.first);
                source.y = cell.surfaceHeightM;
                float zoneFactor = 0.0f;
                float lateralFactor = 0.0f;
                float localLongitudinal = 0.0f;
                float localLateral = 0.0f;
                if (!fieldAt(
                        source, zoneFactor, lateralFactor,
                        localLongitudinal, localLateral))
                {
                    continue;
                }

                const float mobility = 0.48f + 0.62f * clamp01(cell.marbleMaturity);
                const float wakeRate = (0.30f + 2.55f * authority)
                    * zoneFactor * lateralFactor * mobility
                    * (0.75f + 0.25f * rideHeightScale);
                const float moveFraction = std::clamp(
                    1.0f - std::exp(-wakeRate * dt), 0.0f, 0.22f);
                const float requested = cell.looseRubber * moveFraction;
                if (requested <= 1.0e-7f)
                    continue;

                float sideSign = localLateral >= 0.0f ? 1.0f : -1.0f;
                if (std::abs(localLateral) < 0.05f)
                {
                    const auto globalCellX = static_cast<std::int64_t>(entry.first.x)
                        + chunkKey.x * static_cast<std::int64_t>(m_description.chunkSizeCells);
                    const auto globalCellZ = static_cast<std::int64_t>(entry.first.z)
                        + chunkKey.z * static_cast<std::int64_t>(m_description.chunkSizeCells);
                    sideSign = ((globalCellX ^ globalCellZ) & 1LL) != 0 ? 1.0f : -1.0f;
                }

                const bool frontZone = localLongitudinal > halfLength;
                const float rearwardWeight = frontZone ? 0.08f : -0.42f;
                heritage::math::Vec3 flow = normalize(
                    add(scale(right, sideSign * (0.72f + 0.30f * lateralFactor)),
                        scale(forward, rearwardWeight)),
                    scale(right, sideSign));
                const float travel = m_description.cellSizeM
                    * (0.78f + 1.45f * speedSignal)
                    * (0.75f + 0.35f * zoneFactor);
                heritage::math::DVec3 destination{
                    source.x + static_cast<double>(flow.x * travel),
                    source.y + static_cast<double>(flow.y * travel),
                    source.z + static_cast<double>(flow.z * travel)
                };

                const float freshLiftBias = 1.0f - clamp01(cell.marbleMaturity);
                const float liftFraction = std::clamp(
                    authority * zoneFactor * lateralFactor
                        * (0.035f + 0.16f * freshLiftBias
                            + 0.07f * clamp01(cell.fragmentSeverity))
                        * rideHeightScale,
                    0.0f, 0.34f);
                const float liftQuantity = requested * liftFraction;
                const float groundQuantity = requested - liftQuantity;
                const float flowSpeed = 0.55f + speed * (0.018f + 0.018f * zoneFactor);
                const heritage::math::Vec3 liftVelocity = add(
                    scale(flow, flowSpeed),
                    scale(up, 0.16f + speed * 0.008f * zoneFactor * rideHeightScale));
                candidates.push_back({
                    source, destination, liftVelocity, cell.material,
                    groundQuantity, liftQuantity,
                    mixSeed(cell.updateSerial + m_updateSerial, 0x0ae105u)
                });
                if (candidates.size() >= 512u)
                    break;
            }
            if (candidates.size() >= 512u)
                break;
        }
        if (candidates.size() >= 512u)
            break;
    }

    for (const WakeCandidate& candidate : candidates)
    {
        float lifted = 0.0f;
        if (candidate.liftQuantity > 0.0f)
        {
            lifted = liftLooseRubber(
                candidate.source, candidate.material, candidate.liftQuantity,
                candidate.liftVelocity, candidate.seed);
            result.liftedLoose += lifted;
        }
        if (candidate.groundQuantity > 0.0f)
        {
            const float moved = transferLooseRubber(
                candidate.source, candidate.destination, candidate.material,
                candidate.groundQuantity);
            result.groundMovedLoose += moved;
        }
        if (lifted > 0.0f || candidate.groundQuantity > 0.0f)
            ++result.affectedCells;
    }

    // A passing vehicle also perturbs rubber that is already airborne/mobile.
    // This gives nearby visible flakes the same wake direction as the aggregate
    // cell migration without making them independent rigid bodies.
    for (TransientPacket& packet : m_transientPackets)
    {
        float zoneFactor = 0.0f;
        float lateralFactor = 0.0f;
        float localLongitudinal = 0.0f;
        float localLateral = 0.0f;
        if (!fieldAt(
                packet.globalPosition, zoneFactor, lateralFactor,
                localLongitudinal, localLateral))
        {
            continue;
        }
        const float sideSign = localLateral >= 0.0f ? 1.0f : -1.0f;
        const heritage::math::Vec3 flow = normalize(
            add(scale(right, sideSign), scale(forward, -0.35f)),
            scale(right, sideSign));
        const float impulse = dt * authority * zoneFactor * lateralFactor
            * (3.0f + speed * 0.055f);
        packet.velocityMps = add(packet.velocityMps, scale(flow, impulse));
        if (packet.phase == TrackRubberTransientPhase::Airborne)
            packet.velocityMps = add(packet.velocityMps, scale(up, impulse * 0.28f));
        packet.angularVelocityRadPerSecond = add(
            packet.angularVelocityRadPerSecond,
            scale(flow, impulse * 2.2f));
    }

    return result;
}

void TrackRubberState::collectPresentationCells(
    const heritage::math::DVec3& center,
    double radiusM,
    std::vector<TrackRubberVisualCell>& output,
    std::size_t maximumOutput) const
{
    output.clear();
    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)
        || !std::isfinite(radiusM) || radiusM <= 0.0 || maximumOutput == 0u)
    {
        return;
    }

    struct Candidate
    {
        double distanceSquared = 0.0;
        TrackRubberVisualCell visual{};
    };

    const double chunkSizeM = static_cast<double>(m_description.cellSizeM)
        * static_cast<double>(m_description.chunkSizeCells);
    const std::int64_t minChunkX = static_cast<std::int64_t>(
        std::floor((center.x - radiusM) / chunkSizeM));
    const std::int64_t maxChunkX = static_cast<std::int64_t>(
        std::floor((center.x + radiusM) / chunkSizeM));
    const std::int64_t minChunkZ = static_cast<std::int64_t>(
        std::floor((center.z - radiusM) / chunkSizeM));
    const std::int64_t maxChunkZ = static_cast<std::int64_t>(
        std::floor((center.z + radiusM) / chunkSizeM));
    const double radiusSquared = radiusM * radiusM;

    std::vector<Candidate> candidates;
    candidates.reserve((std::min)(
        m_cellCount,
        maximumOutput * static_cast<std::size_t>(2)));

    for (std::int64_t chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ)
    {
        for (std::int64_t chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX)
        {
            const ChunkKey chunkKey{ chunkX, chunkZ };
            const auto chunk = m_chunks.find(chunkKey);
            if (chunk == m_chunks.end())
                continue;
            for (const auto& entry : chunk->second.cells)
            {
                const Cell cell = resolvedCell(entry.second);
                if (cell.depositedRubber <= 0.0010f
                    && cell.looseRubber <= 0.00035f
                    && cell.persistentPiecePopulation < 0.35f)
                {
                    continue;
                }

                heritage::math::DVec3 position = cellCenter(chunkKey, entry.first);
                position.y = cell.surfaceHeightM;
                const double dx = position.x - center.x;
                const double dy = position.y - center.y;
                const double dz = position.z - center.z;
                const double distanceSquared = dx * dx + dy * dy + dz * dz;
                if (distanceSquared > radiusSquared)
                    continue;

                TrackRubberVisualCell visual;
                visual.globalPosition = position;
                visual.normal = cell.normal;
                visual.forward = cell.forward;
                visual.material = cell.material;
                visual.depositedRubber = cell.depositedRubber;
                visual.looseRubber = cell.looseRubber;
                visual.marbleMaturity = clamp01(cell.marbleMaturity);
                visual.fragmentSeverity = clamp01(cell.fragmentSeverity);
                visual.persistentPiecePopulation = std::max(
                    cell.persistentPiecePopulation, 0.0f);
                visual.passCount = cell.passCount;
                visual.updateSerial = cell.updateSerial;
                candidates.push_back({ distanceSquared, visual });
            }
        }
    }

    const auto stableLess = [](const Candidate& a, const Candidate& b) {
        if (a.distanceSquared != b.distanceSquared)
            return a.distanceSquared < b.distanceSquared;
        if (a.visual.globalPosition.z != b.visual.globalPosition.z)
            return a.visual.globalPosition.z < b.visual.globalPosition.z;
        if (a.visual.globalPosition.x != b.visual.globalPosition.x)
            return a.visual.globalPosition.x < b.visual.globalPosition.x;
        return static_cast<std::uint8_t>(a.visual.material)
            < static_cast<std::uint8_t>(b.visual.material);
    };

    if (candidates.size() > maximumOutput)
    {
        std::nth_element(
            candidates.begin(),
            candidates.begin() + static_cast<std::ptrdiff_t>(maximumOutput),
            candidates.end(),
            stableLess);
        candidates.resize(maximumOutput);
    }
    std::sort(candidates.begin(), candidates.end(), stableLess);

    output.reserve(candidates.size());
    for (const Candidate& candidate : candidates)
        output.push_back(candidate.visual);
}


void TrackRubberState::collectPresentationCellsUnsorted(
    const heritage::math::DVec3& center,
    double radiusM,
    std::vector<TrackRubberVisualCell>& output,
    bool includeInactive) const
{
    output.clear();
    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)
        || !std::isfinite(radiusM) || radiusM <= 0.0)
    {
        return;
    }

    const double chunkSizeM = static_cast<double>(m_description.cellSizeM)
        * static_cast<double>(m_description.chunkSizeCells);
    const std::int64_t minChunkX = static_cast<std::int64_t>(
        std::floor((center.x - radiusM) / chunkSizeM));
    const std::int64_t maxChunkX = static_cast<std::int64_t>(
        std::floor((center.x + radiusM) / chunkSizeM));
    const std::int64_t minChunkZ = static_cast<std::int64_t>(
        std::floor((center.z - radiusM) / chunkSizeM));
    const std::int64_t maxChunkZ = static_cast<std::int64_t>(
        std::floor((center.z + radiusM) / chunkSizeM));
    const double radiusSquared = radiusM * radiusM;

    output.reserve((std::min)(m_cellCount, static_cast<std::size_t>(65536)));
    for (std::int64_t chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ)
    {
        for (std::int64_t chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX)
        {
            const ChunkKey chunkKey{ chunkX, chunkZ };
            const auto chunk = m_chunks.find(chunkKey);
            if (chunk == m_chunks.end())
                continue;

            for (const auto& entry : chunk->second.cells)
            {
                const Cell cell = resolvedCell(entry.second);
                if (!includeInactive
                    && cell.depositedRubber <= 0.0010f
                    && cell.looseRubber <= 0.00035f
                    && cell.persistentPiecePopulation < 0.35f)
                {
                    continue;
                }

                heritage::math::DVec3 position = cellCenter(chunkKey, entry.first);
                position.y = cell.surfaceHeightM;
                const double dx = position.x - center.x;
                const double dy = position.y - center.y;
                const double dz = position.z - center.z;
                if (dx * dx + dy * dy + dz * dz > radiusSquared)
                    continue;

                TrackRubberVisualCell visual;
                visual.globalPosition = position;
                visual.normal = cell.normal;
                visual.forward = cell.forward;
                visual.material = cell.material;
                visual.depositedRubber = cell.depositedRubber;
                visual.looseRubber = cell.looseRubber;
                visual.marbleMaturity = clamp01(cell.marbleMaturity);
                visual.fragmentSeverity = clamp01(cell.fragmentSeverity);
                visual.persistentPiecePopulation = std::max(
                    cell.persistentPiecePopulation, 0.0f);
                visual.passCount = cell.passCount;
                visual.updateSerial = cell.updateSerial;
                output.push_back(visual);
            }
        }
    }
}

void TrackRubberState::collectTransientPresentation(
    const heritage::math::DVec3& center,
    double radiusM,
    std::vector<TrackRubberTransientVisual>& output,
    std::size_t maximumOutput) const
{
    output.clear();
    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)
        || !std::isfinite(radiusM) || radiusM <= 0.0 || maximumOutput == 0u)
    {
        return;
    }

    struct Candidate
    {
        double distanceSquared = 0.0;
        TrackRubberTransientVisual visual{};
    };
    std::vector<Candidate> candidates;
    candidates.reserve(std::min(m_transientPackets.size(), maximumOutput * 2u));
    const double radiusSquared = radiusM * radiusM;
    for (const TransientPacket& packet : m_transientPackets)
    {
        const double dx = packet.globalPosition.x - center.x;
        const double dy = packet.globalPosition.y - center.y;
        const double dz = packet.globalPosition.z - center.z;
        const double distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared > radiusSquared)
            continue;
        TrackRubberTransientVisual visual;
        visual.globalPosition = packet.globalPosition;
        visual.axisRight = packet.axisRight;
        visual.axisForward = packet.axisForward;
        visual.axisNormal = packet.axisNormal;
        visual.material = packet.material;
        visual.phase = packet.phase;
        visual.lengthM = packet.lengthM;
        visual.widthM = packet.widthM;
        visual.bendVertex1M = packet.bendVertex1M;
        visual.bendVertex3M = packet.bendVertex3M;
        visual.ageSeconds = packet.ageSeconds;
        visual.opacity = packet.phase == TrackRubberTransientPhase::Airborne ? 0.98f : 0.94f;
        visual.quantity = packet.quantity;
        visual.piecePopulation = packet.piecePopulation;
        visual.marbleMaturity = packet.marbleMaturity;
        visual.fragmentSeverity = packet.fragmentSeverity;
        visual.seed = packet.seed;
        candidates.push_back({ distanceSquared, visual });
    }

    const auto less = [](const Candidate& a, const Candidate& b) {
        if (a.distanceSquared != b.distanceSquared)
            return a.distanceSquared < b.distanceSquared;
        return a.visual.seed < b.visual.seed;
    };
    if (candidates.size() > maximumOutput)
    {
        std::nth_element(
            candidates.begin(),
            candidates.begin() + static_cast<std::ptrdiff_t>(maximumOutput),
            candidates.end(), less);
        candidates.resize(maximumOutput);
    }
    std::sort(candidates.begin(), candidates.end(), less);
    output.reserve(candidates.size());
    for (const Candidate& candidate : candidates)
        output.push_back(candidate.visual);
}


void TrackRubberState::collectTransientPresentationUnsorted(
    const heritage::math::DVec3& center,
    double radiusM,
    std::vector<TrackRubberTransientVisual>& output) const
{
    output.clear();
    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)
        || !std::isfinite(radiusM) || radiusM <= 0.0)
    {
        return;
    }

    output.reserve(m_transientPackets.size());
    const double radiusSquared = radiusM * radiusM;
    for (const TransientPacket& packet : m_transientPackets)
    {
        const double dx = packet.globalPosition.x - center.x;
        const double dy = packet.globalPosition.y - center.y;
        const double dz = packet.globalPosition.z - center.z;
        if (dx * dx + dy * dy + dz * dz > radiusSquared)
            continue;

        TrackRubberTransientVisual visual;
        visual.globalPosition = packet.globalPosition;
        visual.axisRight = packet.axisRight;
        visual.axisForward = packet.axisForward;
        visual.axisNormal = packet.axisNormal;
        visual.material = packet.material;
        visual.phase = packet.phase;
        visual.lengthM = packet.lengthM;
        visual.widthM = packet.widthM;
        visual.bendVertex1M = packet.bendVertex1M;
        visual.bendVertex3M = packet.bendVertex3M;
        visual.ageSeconds = packet.ageSeconds;
        visual.opacity = packet.phase == TrackRubberTransientPhase::Airborne ? 0.98f : 0.94f;
        visual.quantity = packet.quantity;
        visual.piecePopulation = packet.piecePopulation;
        visual.marbleMaturity = packet.marbleMaturity;
        visual.fragmentSeverity = packet.fragmentSeverity;
        visual.seed = packet.seed;
        output.push_back(visual);
    }
}

void TrackRubberState::setChunkEvictionCallback(ChunkEvictionCallback callback)
{
    m_chunkEvictionCallback = std::move(callback);
}

bool TrackRubberState::snapshotChunk(
    std::int64_t chunkX,
    std::int64_t chunkZ,
    TrackRubberChunkSnapshot& snapshot) const
{
    const ChunkKey key{ chunkX, chunkZ };
    const auto found = m_chunks.find(key);
    if (found == m_chunks.end())
        return false;
    snapshot = snapshotChunk(key, found->second);
    return true;
}

bool TrackRubberState::restoreChunk(const TrackRubberChunkSnapshot& snapshot)
{
    const ChunkKey key{ snapshot.chunkX, snapshot.chunkZ };
    auto existing = m_chunks.find(key);
    if (existing != m_chunks.end())
        eraseChunk(key, existing, false);

    makeRoomForChunk();
    m_chunkLru.push_front(key);
    Chunk chunk;
    chunk.lruIterator = m_chunkLru.begin();
    auto inserted = m_chunks.emplace(key, std::move(chunk)).first;

    const std::uint32_t chunkSize = m_description.chunkSizeCells;
    for (const TrackRubberPersistedCell& source : snapshot.cells)
    {
        if (source.localX >= chunkSize || source.localZ >= chunkSize)
            continue;
        const SurfaceMaterial material = static_cast<SurfaceMaterial>(source.material);
        if (!rubberCapableSurface(material))
            continue;
        makeRoomForCell(&key);
        LocalKey local{
            source.localX, source.localZ, source.verticalLayer, source.material
        };
        Cell cell;
        cell.depositedRubber = clamp01(source.depositedRubber);
        cell.looseRubber = clamp01(source.looseRubber);
        cell.marbleMaturity = clamp01(source.marbleMaturity);
        cell.fragmentSeverity = clamp01(source.fragmentSeverity);
        const double availablePieces = std::max(
            static_cast<double>(m_description.maximumPersistentPieceCount)
                - (m_persistentPiecePopulation + m_transientPiecePopulation),
            0.0);
        cell.persistentPiecePopulation = static_cast<float>(
            std::min(
                static_cast<double>(std::max(
                    source.persistentPiecePopulation, 0.0f)),
                availablePieces));
        cell.surfaceHeightM = std::isfinite(source.surfaceHeightM)
            ? source.surfaceHeightM : 0.0;
        cell.normal = normalize(source.normal, { 0.0f, 1.0f, 0.0f });
        cell.forward = normalize(source.forward, { 0.0f, 0.0f, 1.0f });
        cell.material = material;
        cell.passProgress = std::clamp(source.passProgress, 0.0f, 0.999999f);
        cell.passCount = source.passCount;
        cell.updateSerial = source.updateSerial;
        cell.timeAtWriteSeconds = m_elapsedSeconds;
        cell.wetExposureAtWriteSeconds = m_globalWetExposureSeconds;
        const auto result = inserted->second.cells.emplace(local, cell);
        if (result.second)
        {
            ++m_cellCount;
            m_persistentPiecePopulation += static_cast<double>(
                cell.persistentPiecePopulation);
        }
    }
    if (inserted->second.cells.empty())
    {
        eraseChunk(key, inserted, false);
        return false;
    }
    return true;
}

TrackRubberStats TrackRubberState::stats() const
{
    TrackRubberStats result;
    result.activeCells = m_cellCount;
    result.residentChunks = m_chunks.size();
    result.contactSamples = m_contactSamples;
    result.depositedGeneration = m_depositedGeneration;
    result.looseGeneration = m_looseGeneration;
    result.persistentPieces = static_cast<std::size_t>(
        std::max(std::llround(
            m_persistentPiecePopulation + m_transientPiecePopulation), 0LL));
    result.transientPackets = m_transientPackets.size();
    result.transientLoose = m_transientLooseQuantity;
    return result;
}

} // namespace heritage::physics::rubber
