#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <vector>

#include "../CollisionSystem.hpp"

namespace heritage::physics {

// TIRE15/CLEAN10 persistent driven-surface storage. Static collision geometry
// remains the geometric query source; SurfaceField stores bounded dynamic state
// at stable FP64 world coordinates. Storage is sparse inside spatial chunks so
// long circuits/stages do not require one monolithic dense terrain texture.
struct SurfaceFieldDescription
{
    float cellSizeM = 0.25f;

    // Hard safety bound across all resident chunks. CLEAN10 raises the default
    // from the original 16,384-cell prototype so a full circuit can retain a
    // useful driven history while still remaining explicitly bounded.
    std::size_t maximumCellCount = 262144;

    // Number of cells along one X/Z edge of a chunk. With the default 0.25 m
    // cells, 64 cells gives a 16 m square streaming tile.
    std::uint32_t chunkSizeCells = 64;

    // Hard bound on resident tiles. Eviction is LRU by chunk and therefore
    // does not scan the complete field when the cache is full.
    std::size_t maximumResidentChunkCount = 8192;

    // X/Z alone would alias a bridge with the road beneath it. Contact height
    // is therefore quantized into a deliberately coarse vertical layer: fine
    // enough to distinguish stacked roads, coarse enough that centimetres of
    // rutting/contact jitter cannot create a second surface identity.
    float verticalLayerSizeM = 2.0f;
};

struct SurfaceFieldInitialState
{
    float looseDepthM = 0.0f;
    float compaction = 0.0f;
    float moisture = 0.0f;
    float rutDepthM = 0.0f;
};

struct SurfaceFieldSample
{
    bool valid = false;
    SurfaceMaterial material = SurfaceMaterial::Default;
    float looseDepthM = 0.0f;
    float compaction = 0.0f;
    float moisture = 0.0f;
    float rutDepthM = 0.0f;
    float longitudinalShearHistoryM = 0.0f;
    float lateralShearHistoryM = 0.0f;
    float displacedVolumeM3 = 0.0f;
    std::uint32_t passCount = 0;
};

// State delta only. Spatial ownership belongs to SurfaceField/SurfaceWorld;
// tire providers do not own or encode world-addressing policy.
struct SurfaceFieldUpdate
{
    SurfaceMaterial material = SurfaceMaterial::Default;
    SurfaceFieldInitialState initialState{};

    // Persistent plastic deformation. rutDepthTargetM is monotonic in TIRE15:
    // recovery/weather relaxation can be added later as an explicit mechanism.
    float rutDepthTargetM = 0.0f;
    float rutDepthDeltaM = 0.0f;
    float compactionDelta = 0.0f;
    float looseDepthDeltaM = 0.0f;
    float moistureDelta = 0.0f;
    float longitudinalShearHistoryDeltaM = 0.0f;
    float lateralShearHistoryDeltaM = 0.0f;
    float displacedVolumeDeltaM3 = 0.0f;
    // Fractional traversal of this field cell by a wheel footprint. One full
    // unit promotes the public passCount; this avoids counting 1000-Hz
    // substeps as separate vehicle passes.
    float passProgressDelta = 0.0f;
    bool countPass = false;
};

// Stable payload for future scene streaming, persistence and replication.
// Chunks are addressed in integer tile coordinates, while cells are local to
// that chunk. CLEAN10 supplies the hook without imposing a file format.
struct SurfaceFieldPersistedCell
{
    std::uint16_t localX = 0;
    std::uint16_t localZ = 0;
    std::int64_t verticalLayer = 0;
    std::uint8_t material = 0;
    SurfaceFieldSample state{};
    float passProgress = 0.0f;
};

struct SurfaceFieldChunkSnapshot
{
    std::int64_t chunkX = 0;
    std::int64_t chunkZ = 0;
    std::vector<SurfaceFieldPersistedCell> cells;
};

class SurfaceField
{
public:
    using ChunkEvictionCallback =
        std::function<void(const SurfaceFieldChunkSnapshot&)>;

    explicit SurfaceField(const SurfaceFieldDescription& description = {});

    void clear();
    void setDescription(const SurfaceFieldDescription& description);
    const SurfaceFieldDescription& description() const { return m_description; }
    std::size_t cellCount() const { return m_cellCount; }
    std::size_t residentChunkCount() const { return m_chunks.size(); }
    double chunkSizeM() const;

    // SurfaceField itself is addressed only with absolute/global coordinates.
    // Local FP32 -> global FP64 conversion belongs to SurfaceWorld.
    SurfaceFieldSample sample(
        const heritage::math::DVec3& globalPosition,
        SurfaceMaterial material,
        const SurfaceFieldInitialState& initialState) const;

    SurfaceFieldSample apply(
        const heritage::math::DVec3& globalPosition,
        const SurfaceFieldUpdate& update);

    // Explicit streaming/persistence seam. An external scene streamer can save
    // evicted chunks and restore them later without SurfaceField knowing about
    // files, network transport or presentation.
    void setChunkEvictionCallback(ChunkEvictionCallback callback);
    bool snapshotChunk(
        std::int64_t chunkX,
        std::int64_t chunkZ,
        SurfaceFieldChunkSnapshot& snapshot) const;
    bool restoreChunk(const SurfaceFieldChunkSnapshot& snapshot);

private:
    struct ChunkKey
    {
        std::int64_t x = 0;
        std::int64_t z = 0;

        bool operator==(const ChunkKey& other) const
        {
            return x == other.x && z == other.z;
        }
    };

    struct LocalKey
    {
        std::uint16_t x = 0;
        std::uint16_t z = 0;
        std::int64_t verticalLayer = 0;
        std::uint8_t material = 0;

        bool operator==(const LocalKey& other) const
        {
            return x == other.x
                && z == other.z
                && verticalLayer == other.verticalLayer
                && material == other.material;
        }
    };

    struct ChunkKeyHash
    {
        std::size_t operator()(const ChunkKey& key) const;
    };

    struct LocalKeyHash
    {
        std::size_t operator()(const LocalKey& key) const;
    };

    struct Cell
    {
        SurfaceFieldSample state{};
        float passProgress = 0.0f;
    };

    struct Chunk
    {
        std::unordered_map<LocalKey, Cell, LocalKeyHash> cells;
        std::list<ChunkKey>::iterator lruIterator;
    };

    struct Address
    {
        bool valid = false;
        ChunkKey chunk{};
        LocalKey cell{};
    };

    Address addressFor(
        const heritage::math::DVec3& globalPosition,
        SurfaceMaterial material) const;
    void touchChunk(
        const ChunkKey& key,
        Chunk& chunk);
    void makeRoomForChunk(const ChunkKey* protectedChunk = nullptr);
    void makeRoomForCell(const ChunkKey* protectedChunk = nullptr);
    bool evictLeastRecentlyUsedChunk(const ChunkKey* protectedChunk = nullptr);
    SurfaceFieldChunkSnapshot snapshotChunk(const ChunkKey& key, const Chunk& chunk) const;
    void eraseChunk(
        const ChunkKey& key,
        std::unordered_map<ChunkKey, Chunk, ChunkKeyHash>::iterator found,
        bool notifyEviction);

    SurfaceFieldDescription m_description{};
    std::unordered_map<ChunkKey, Chunk, ChunkKeyHash> m_chunks;
    std::list<ChunkKey> m_chunkLru;
    std::size_t m_cellCount = 0;
    ChunkEvictionCallback m_chunkEvictionCallback;
};

} // namespace heritage::physics
