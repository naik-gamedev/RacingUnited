#include "SurfaceField.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace heritage::physics {
namespace {

bool finiteFloat(float value)
{
    return std::isfinite(value);
}

bool finiteDouble(double value)
{
    return std::isfinite(value);
}

SurfaceFieldDescription sanitized(SurfaceFieldDescription value)
{
    if (!finiteFloat(value.cellSizeM) || value.cellSizeM < 0.05f)
        value.cellSizeM = 0.25f;
    value.cellSizeM = std::clamp(value.cellSizeM, 0.05f, 5.0f);

    value.maximumCellCount = std::clamp<std::size_t>(
        value.maximumCellCount, 64u, 1u << 23u);
    value.chunkSizeCells = std::clamp<std::uint32_t>(
        value.chunkSizeCells, 4u, 512u);
    value.maximumResidentChunkCount = std::clamp<std::size_t>(
        value.maximumResidentChunkCount, 1u, 1u << 18u);
    if (!finiteFloat(value.verticalLayerSizeM) || value.verticalLayerSizeM < 0.25f)
        value.verticalLayerSizeM = 2.0f;
    value.verticalLayerSizeM = std::clamp(value.verticalLayerSizeM, 0.25f, 100.0f);
    return value;
}

SurfaceFieldSample initialSample(
    SurfaceMaterial material,
    const SurfaceFieldInitialState& initial)
{
    SurfaceFieldSample value;
    value.valid = true;
    value.material = material;
    value.looseDepthM = std::max(initial.looseDepthM, 0.0f);
    value.compaction = std::clamp(initial.compaction, 0.0f, 1.0f);
    value.moisture = std::clamp(initial.moisture, 0.0f, 1.0f);
    value.rutDepthM = std::max(initial.rutDepthM, 0.0f);
    return value;
}

bool sameAddressing(
    const SurfaceFieldDescription& a,
    const SurfaceFieldDescription& b)
{
    return a.cellSizeM == b.cellSizeM
        && a.chunkSizeCells == b.chunkSizeCells
        && a.verticalLayerSizeM == b.verticalLayerSizeM;
}

std::int64_t floorDivide(std::int64_t value, std::int64_t divisor)
{
    std::int64_t quotient = value / divisor;
    const std::int64_t remainder = value % divisor;
    if (remainder < 0)
        --quotient;
    return quotient;
}

bool quantizedCellCoordinate(
    double coordinate,
    double inverseCellSize,
    std::int64_t& result)
{
    if (!finiteDouble(coordinate) || !finiteDouble(inverseCellSize))
        return false;

    const double quantized = std::floor(coordinate * inverseCellSize);
    if (!finiteDouble(quantized)
        || quantized < static_cast<double>((std::numeric_limits<std::int64_t>::min)())
        || quantized > static_cast<double>((std::numeric_limits<std::int64_t>::max)()))
    {
        return false;
    }

    result = static_cast<std::int64_t>(quantized);
    return true;
}

} // namespace

SurfaceField::SurfaceField(const SurfaceFieldDescription& description)
    : m_description(sanitized(description))
{
}

void SurfaceField::clear()
{
    m_chunks.clear();
    m_chunkLru.clear();
    m_cellCount = 0;
}

void SurfaceField::setDescription(const SurfaceFieldDescription& description)
{
    const SurfaceFieldDescription next = sanitized(description);
    if (!sameAddressing(m_description, next))
    {
        m_description = next;
        clear();
        return;
    }

    m_description = next;
    while (m_chunks.size() > m_description.maximumResidentChunkCount)
    {
        if (!evictLeastRecentlyUsedChunk())
            break;
    }
    while (m_cellCount > m_description.maximumCellCount)
    {
        if (!evictLeastRecentlyUsedChunk())
            break;
    }
}

double SurfaceField::chunkSizeM() const
{
    return static_cast<double>(m_description.cellSizeM)
        * static_cast<double>(m_description.chunkSizeCells);
}

std::size_t SurfaceField::ChunkKeyHash::operator()(const ChunkKey& key) const
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

std::size_t SurfaceField::LocalKeyHash::operator()(const LocalKey& key) const
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

SurfaceField::Address SurfaceField::addressFor(
    const heritage::math::DVec3& globalPosition,
    SurfaceMaterial material) const
{
    Address address;
    const double inverse = 1.0 / static_cast<double>(m_description.cellSizeM);
    std::int64_t cellX = 0;
    std::int64_t cellZ = 0;
    std::int64_t verticalLayer = 0;
    if (!quantizedCellCoordinate(globalPosition.x, inverse, cellX)
        || !quantizedCellCoordinate(globalPosition.z, inverse, cellZ)
        || !quantizedCellCoordinate(
            globalPosition.y,
            1.0 / static_cast<double>(m_description.verticalLayerSizeM),
            verticalLayer))
    {
        return address;
    }

    const std::int64_t chunkSize =
        static_cast<std::int64_t>(m_description.chunkSizeCells);
    const std::int64_t chunkX = floorDivide(cellX, chunkSize);
    const std::int64_t chunkZ = floorDivide(cellZ, chunkSize);
    const std::int64_t localX = cellX - chunkX * chunkSize;
    const std::int64_t localZ = cellZ - chunkZ * chunkSize;
    if (localX < 0 || localZ < 0
        || localX >= chunkSize || localZ >= chunkSize)
    {
        return address;
    }

    address.valid = true;
    address.chunk = { chunkX, chunkZ };
    address.cell = {
        static_cast<std::uint16_t>(localX),
        static_cast<std::uint16_t>(localZ),
        verticalLayer,
        static_cast<std::uint8_t>(material)
    };
    return address;
}

SurfaceFieldSample SurfaceField::sample(
    const heritage::math::DVec3& globalPosition,
    SurfaceMaterial material,
    const SurfaceFieldInitialState& initialState) const
{
    const Address address = addressFor(globalPosition, material);
    if (!address.valid)
        return initialSample(material, initialState);

    const auto chunk = m_chunks.find(address.chunk);
    if (chunk == m_chunks.end())
        return initialSample(material, initialState);
    const auto cell = chunk->second.cells.find(address.cell);
    if (cell == chunk->second.cells.end())
        return initialSample(material, initialState);
    return cell->second.state;
}

void SurfaceField::touchChunk(const ChunkKey& key, Chunk& chunk)
{
    m_chunkLru.erase(chunk.lruIterator);
    m_chunkLru.push_front(key);
    chunk.lruIterator = m_chunkLru.begin();
}

SurfaceFieldChunkSnapshot SurfaceField::snapshotChunk(
    const ChunkKey& key,
    const Chunk& chunk) const
{
    SurfaceFieldChunkSnapshot snapshot;
    snapshot.chunkX = key.x;
    snapshot.chunkZ = key.z;
    snapshot.cells.reserve(chunk.cells.size());
    for (const auto& entry : chunk.cells)
    {
        SurfaceFieldPersistedCell cell;
        cell.localX = entry.first.x;
        cell.localZ = entry.first.z;
        cell.verticalLayer = entry.first.verticalLayer;
        cell.material = entry.first.material;
        cell.state = entry.second.state;
        cell.passProgress = entry.second.passProgress;
        snapshot.cells.push_back(cell);
    }
    std::sort(
        snapshot.cells.begin(),
        snapshot.cells.end(),
        [](const SurfaceFieldPersistedCell& a,
            const SurfaceFieldPersistedCell& b) {
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

void SurfaceField::eraseChunk(
    const ChunkKey& key,
    std::unordered_map<ChunkKey, Chunk, ChunkKeyHash>::iterator found,
    bool notifyEviction)
{
    if (found == m_chunks.end())
        return;

    if (notifyEviction && m_chunkEvictionCallback)
        m_chunkEvictionCallback(snapshotChunk(key, found->second));

    m_cellCount -= (std::min)(m_cellCount, found->second.cells.size());
    m_chunkLru.erase(found->second.lruIterator);
    m_chunks.erase(found);
}

bool SurfaceField::evictLeastRecentlyUsedChunk(const ChunkKey* protectedChunk)
{
    if (m_chunkLru.empty())
        return false;

    auto lru = m_chunkLru.end();
    while (lru != m_chunkLru.begin())
    {
        --lru;
        if (protectedChunk == nullptr || !(*lru == *protectedChunk))
        {
            const ChunkKey key = *lru;
            const auto found = m_chunks.find(key);
            if (found == m_chunks.end())
            {
                m_chunkLru.erase(lru);
                return true;
            }
            eraseChunk(key, found, true);
            return true;
        }
    }
    return false;
}

void SurfaceField::makeRoomForChunk(const ChunkKey* protectedChunk)
{
    while (m_chunks.size() >= m_description.maximumResidentChunkCount)
    {
        if (!evictLeastRecentlyUsedChunk(protectedChunk))
            break;
    }
}

void SurfaceField::makeRoomForCell(const ChunkKey* protectedChunk)
{
    while (m_cellCount >= m_description.maximumCellCount)
    {
        if (evictLeastRecentlyUsedChunk(protectedChunk))
            continue;

        // A deliberately tiny/custom cell budget can be smaller than one
        // actively written chunk. Preserve the hard memory bound without an
        // O(N)-over-the-world scan by dropping one cell from that one protected
        // tile. Default production budgets are large enough that this is only
        // an emergency/synthetic-test path.
        if (protectedChunk != nullptr)
        {
            const auto chunk = m_chunks.find(*protectedChunk);
            if (chunk != m_chunks.end() && !chunk->second.cells.empty())
            {
                chunk->second.cells.erase(chunk->second.cells.begin());
                --m_cellCount;
                continue;
            }
        }
        break;
    }
}

SurfaceFieldSample SurfaceField::apply(
    const heritage::math::DVec3& globalPosition,
    const SurfaceFieldUpdate& update)
{
    const Address address = addressFor(globalPosition, update.material);
    if (!address.valid)
        return initialSample(update.material, update.initialState);

    auto chunk = m_chunks.find(address.chunk);
    if (chunk == m_chunks.end())
    {
        makeRoomForChunk();
        m_chunkLru.push_front(address.chunk);
        Chunk value;
        value.lruIterator = m_chunkLru.begin();
        chunk = m_chunks.emplace(address.chunk, std::move(value)).first;
    }
    else
    {
        touchChunk(address.chunk, chunk->second);
    }

    auto found = chunk->second.cells.find(address.cell);
    if (found == chunk->second.cells.end())
    {
        // Prefer evicting another complete tile rather than scanning all cells.
        // If only the active tile exists, makeRoomForCell uses a bounded
        // in-tile fallback while still preserving the global hard cell cap.
        makeRoomForCell(&address.chunk);
        Cell cell;
        cell.state = initialSample(update.material, update.initialState);
        found = chunk->second.cells.emplace(address.cell, cell).first;
        ++m_cellCount;
    }

    Cell& cell = found->second;
    SurfaceFieldSample& state = cell.state;
    state.valid = true;
    state.material = update.material;
    state.compaction = std::clamp(
        state.compaction + update.compactionDelta, 0.0f, 1.0f);
    state.moisture = std::clamp(
        state.moisture + update.moistureDelta, 0.0f, 1.0f);
    state.looseDepthM = std::max(
        state.looseDepthM + update.looseDepthDeltaM, 0.0f);
    const float rutTarget = std::max(
        state.rutDepthM, std::max(update.rutDepthTargetM, 0.0f));
    state.rutDepthM = std::min(
        rutTarget,
        state.rutDepthM + std::max(update.rutDepthDeltaM, 0.0f));
    state.longitudinalShearHistoryM += std::max(
        update.longitudinalShearHistoryDeltaM, 0.0f);
    state.lateralShearHistoryM += std::max(
        update.lateralShearHistoryDeltaM, 0.0f);
    state.displacedVolumeM3 += std::max(update.displacedVolumeDeltaM3, 0.0f);
    cell.passProgress += std::max(update.passProgressDelta, 0.0f);
    if (cell.passProgress >= 1.0f)
    {
        const std::uint32_t completed = static_cast<std::uint32_t>(
            std::floor(cell.passProgress));
        state.passCount += completed;
        cell.passProgress -= static_cast<float>(completed);
    }
    if (update.countPass)
        ++state.passCount;
    return state;
}

void SurfaceField::setChunkEvictionCallback(ChunkEvictionCallback callback)
{
    m_chunkEvictionCallback = std::move(callback);
}

bool SurfaceField::snapshotChunk(
    std::int64_t chunkX,
    std::int64_t chunkZ,
    SurfaceFieldChunkSnapshot& snapshot) const
{
    const ChunkKey key{ chunkX, chunkZ };
    const auto found = m_chunks.find(key);
    if (found == m_chunks.end())
        return false;
    snapshot = snapshotChunk(key, found->second);
    return true;
}

bool SurfaceField::restoreChunk(const SurfaceFieldChunkSnapshot& snapshot)
{
    if (snapshot.cells.empty())
        return true;

    const ChunkKey key{ snapshot.chunkX, snapshot.chunkZ };
    auto existing = m_chunks.find(key);
    if (existing != m_chunks.end())
        eraseChunk(key, existing, false);

    makeRoomForChunk();
    m_chunkLru.push_front(key);
    Chunk chunk;
    chunk.lruIterator = m_chunkLru.begin();
    auto inserted = m_chunks.emplace(key, std::move(chunk)).first;

    bool complete = true;
    for (const SurfaceFieldPersistedCell& persisted : snapshot.cells)
    {
        if (persisted.localX >= m_description.chunkSizeCells
            || persisted.localZ >= m_description.chunkSizeCells)
        {
            complete = false;
            continue;
        }
        if (m_cellCount >= m_description.maximumCellCount)
        {
            if (!evictLeastRecentlyUsedChunk(&key))
            {
                complete = false;
                break;
            }
        }

        LocalKey local{
            persisted.localX,
            persisted.localZ,
            persisted.verticalLayer,
            persisted.material
        };
        Cell cell;
        cell.state = persisted.state;
        cell.passProgress = std::max(persisted.passProgress, 0.0f);
        const auto result = inserted->second.cells.emplace(local, cell);
        if (result.second)
            ++m_cellCount;
    }

    if (inserted->second.cells.empty())
    {
        eraseChunk(key, inserted, false);
        return false;
    }
    return complete;
}

} // namespace heritage::physics
