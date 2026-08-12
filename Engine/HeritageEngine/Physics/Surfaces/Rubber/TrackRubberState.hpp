#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <vector>

#include "../../../Core/Math/Math.hpp"
#include "../../CollisionSystem.hpp"

namespace heritage::physics::rubber {

// TIRE15C specialized world-rubber layer. Deposited racing-line rubber and
// loose rubber/marbles share a bounded spatial cache, but they are deliberately
// not represented as deformable terrain and individual marbles are not rigid
// bodies. Tire pickup stays tire-owned; this class owns the common track state.
struct TrackRubberDescription
{
    float cellSizeM = 0.50f;
    std::uint32_t chunkSizeCells = 32;
    std::size_t maximumCellCount = 524288;
    std::size_t maximumResidentChunkCount = 8192;
    std::size_t maximumPersistentPieceCount = 500000;
    std::size_t maximumTransientPacketCount = 8192;
    float verticalLayerSizeM = 2.0f;

    // Logical debris population is persisted as aggregate cell state. This is
    // not a pool of 500k rigid bodies: nearby pieces are reconstructed
    // deterministically by the renderer while distant debris stays cell-based.
    float nominalPiecesPerLooseUnit = 110.0f;

    // Contact response at fully developed normalized state. These are bounded
    // track-evolution modifiers around the existing tire/surface model, not a
    // replacement friction law.
    float maximumDryDepositedGripGain = 0.075f;
    float maximumWetDepositedGripLoss = 0.090f;
    float maximumLooseRubberGripLoss = 0.28f;
    float maximumLooseRollingResistanceGain = 0.12f;

    // Lazy ageing rates. globalWetExposureSeconds integrates weather wetness,
    // so a value of one second means one second of fully wet exposure.
    // Dry track rubber is session-persistent. State changes through tire
    // pickup/sweeping, rain/washing or explicit reset rather than an arbitrary
    // visual lifetime. Long-term oxidation/weathering can be a later world
    // persistence policy.
    double depositedDryAgeRatePerSecond = 0.0;
    double looseDryAgeRatePerSecond = 0.0;
    double depositedRainWashRatePerWetSecond = 0.00080;
    double looseRainWashRatePerWetSecond = 0.0100;
};

struct TrackRubberSample
{
    bool valid = false;
    SurfaceMaterial material = SurfaceMaterial::Default;
    float depositedRubber = 0.0f;
    float looseRubber = 0.0f;
    float contactFrictionScale = 1.0f;
    float rollingResistanceScale = 1.0f;
    float pickupAvailability = 0.0f;
    float marbleMaturity = 0.0f;
    float fragmentSeverity = 0.0f;
    float persistentPiecePopulation = 0.0f;
    float freshLooseGenerated = 0.0f;
    float freshFragmentSeverity = 0.0f;
    std::uint32_t passCount = 0;
};

struct TrackRubberContactInput
{
    SurfaceMaterial material = SurfaceMaterial::Default;
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    float deltaTimeSeconds = 0.001f;
    float wetness = 0.0f;
    float normalLoadN = 0.0f;
    float nominalLoadN = 3500.0f;
    float tireWidthM = 0.20f;
    float forwardSpeedMps = 0.0f;
    float longitudinalSlipSpeedMps = 0.0f;
    float lateralSlipSpeedMps = 0.0f;
    float slipDissipationWatts = 0.0f;
    float treadWearDepthDeltaM = 0.0f;
    float treadWearFraction = 0.0f;
    float treadTemperatureC = 20.0f;

    // Tire-specific shedding propensity. One is the neutral baseline. This is
    // deliberately independent from final tire grip; tire authoring can make a
    // soft/abrasion-prone compound shed more without directly multiplying Fx/Fy.
    float compoundSheddingFactor = 1.0f;

    // Development-only acceleration controls supplied by SurfaceWorld. Normal
    // gameplay is always 1x; the Tire Lab can deliberately run up to 1000x.
    float generationMultiplier = 1.0f;
    float maturationMultiplier = 1.0f;
};


enum class TrackRubberTransientPhase : std::uint8_t
{
    Airborne = 0,
    MobileGround
};

// Lightweight authoritative moving-rubber packet. One packet may represent
// several real fragments; it is not a rigid body. The renderer reconstructs
// each nearby packet as a two-triangle, two-sided deformable flake.
struct TrackRubberTransientVisual
{
    heritage::math::DVec3 globalPosition{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 axisRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 axisForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 axisNormal{ 0.0f, 1.0f, 0.0f };
    SurfaceMaterial material = SurfaceMaterial::Default;
    TrackRubberTransientPhase phase = TrackRubberTransientPhase::Airborne;
    float lengthM = 0.025f;
    float widthM = 0.007f;
    float bendVertex1M = 0.0f;
    float bendVertex3M = 0.0f;
    float ageSeconds = 0.0f;
    float opacity = 1.0f;
    float quantity = 0.0f;
    float piecePopulation = 0.0f;
    float marbleMaturity = 0.0f;
    float fragmentSeverity = 0.0f;
    std::uint32_t seed = 0;
};

// Analytical vehicle wake input. This deliberately avoids CFD and per-marble
// rigid bodies: a bounded local wake field transfers aggregate loose rubber
// between cells and may lift a small fraction into authoritative transient
// packets. The aeroWakeFactor is an authoring seam for future active aero data.
struct TrackRubberWakeInput
{
    heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 up{ 0.0f, 1.0f, 0.0f };
    float deltaTimeSeconds = 1.0f / 60.0f;
    float speedMps = 0.0f;
    float vehicleWidthM = 1.8f;
    float vehicleLengthM = 4.2f;
    float rideHeightM = 0.16f;
    float normalLoadN = 0.0f;
    float referenceWeightN = 0.0f;
    float aeroWakeFactor = 1.0f;
};

struct TrackRubberWakeResult
{
    float groundMovedLoose = 0.0f;
    float liftedLoose = 0.0f;
    std::size_t affectedCells = 0;
};

struct TrackRubberVisualCell
{
    heritage::math::DVec3 globalPosition{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    SurfaceMaterial material = SurfaceMaterial::Default;
    float depositedRubber = 0.0f;
    float looseRubber = 0.0f;
    float marbleMaturity = 0.0f;
    float fragmentSeverity = 0.0f;
    float persistentPiecePopulation = 0.0f;
    std::uint32_t passCount = 0;
    std::uint64_t updateSerial = 0;
};

struct TrackRubberStats
{
    std::size_t activeCells = 0;
    std::size_t residentChunks = 0;
    std::uint64_t contactSamples = 0;
    double depositedGeneration = 0.0;
    double looseGeneration = 0.0;
    std::size_t persistentPieces = 0;
    std::size_t transientPackets = 0;
    double transientLoose = 0.0;
};

// Chunk snapshot seam mirrors the generic SurfaceField streaming philosophy
// without pretending rubber is generic terrain. Snapshots store resolved state
// at the instant they are produced, so weather clocks remain local to the
// active world rather than leaking into persistence formats.
struct TrackRubberPersistedCell
{
    std::uint16_t localX = 0;
    std::uint16_t localZ = 0;
    std::int64_t verticalLayer = 0;
    std::uint8_t material = 0;
    float depositedRubber = 0.0f;
    float looseRubber = 0.0f;
    float marbleMaturity = 0.0f;
    float fragmentSeverity = 0.0f;
    float persistentPiecePopulation = 0.0f;
    double surfaceHeightM = 0.0;
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    float passProgress = 0.0f;
    std::uint32_t passCount = 0;
    std::uint64_t updateSerial = 0;
};

struct TrackRubberChunkSnapshot
{
    std::int64_t chunkX = 0;
    std::int64_t chunkZ = 0;
    std::vector<TrackRubberPersistedCell> cells;
};

bool rubberCapableSurface(SurfaceMaterial material);

class TrackRubberState
{
public:
    using ChunkEvictionCallback =
        std::function<void(const TrackRubberChunkSnapshot&)>;

    explicit TrackRubberState(const TrackRubberDescription& description = {});

    void clear();
    void setDescription(const TrackRubberDescription& description);
    const TrackRubberDescription& description() const { return m_description; }

    // Advances only global clocks/exposure. Cells age lazily when touched or
    // rendered, so rain/weather cost does not scale with total track length.
    void advance(float deltaTimeSeconds, float globalWetness);

    TrackRubberSample sample(
        const heritage::math::DVec3& globalPosition,
        SurfaceMaterial material,
        float localWetness) const;

    TrackRubberSample applyContact(
        const heritage::math::DVec3& globalPosition,
        const TrackRubberContactInput& input);

    TrackRubberWakeResult applyWake(
        const heritage::math::DVec3& globalPosition,
        const TrackRubberWakeInput& input);

    void collectPresentationCells(
        const heritage::math::DVec3& center,
        double radiusM,
        std::vector<TrackRubberVisualCell>& output,
        std::size_t maximumOutput = 8192) const;

    // TIRE16L GPU presentation feed: spatially bounded but deliberately
    // unsorted. includeInactive=true also returns allocated cells whose loose
    // population has dropped to zero so a persistent GPU cache can invalidate
    // its old record without scanning/rebuilding the whole marble field.
    void collectPresentationCellsUnsorted(
        const heritage::math::DVec3& center,
        double radiusM,
        std::vector<TrackRubberVisualCell>& output,
        bool includeInactive = false) const;

    void collectTransientPresentation(
        const heritage::math::DVec3& center,
        double radiusM,
        std::vector<TrackRubberTransientVisual>& output,
        std::size_t maximumOutput = 2048) const;

    // Moving packets already have a hard authoritative pool bound; the GPU
    // renderer does not need a per-frame distance sort just to turn them into
    // flakes.
    void collectTransientPresentationUnsorted(
        const heritage::math::DVec3& center,
        double radiusM,
        std::vector<TrackRubberTransientVisual>& output) const;

    TrackRubberStats stats() const;
    std::size_t cellCount() const { return m_cellCount; }
    std::size_t residentChunkCount() const { return m_chunks.size(); }

    void setChunkEvictionCallback(ChunkEvictionCallback callback);
    bool snapshotChunk(
        std::int64_t chunkX,
        std::int64_t chunkZ,
        TrackRubberChunkSnapshot& snapshot) const;
    bool restoreChunk(const TrackRubberChunkSnapshot& snapshot);

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
            return x == other.x && z == other.z
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
        float depositedRubber = 0.0f;
        float looseRubber = 0.0f;
        float marbleMaturity = 0.0f;
        float fragmentSeverity = 0.0f;
        float persistentPiecePopulation = 0.0f;
        double surfaceHeightM = 0.0;
        heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
        heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
        SurfaceMaterial material = SurfaceMaterial::Default;
        float passProgress = 0.0f;
        std::uint32_t passCount = 0;
        std::uint64_t updateSerial = 0;
        double timeAtWriteSeconds = 0.0;
        double wetExposureAtWriteSeconds = 0.0;
    };

    struct TransientPacket
    {
        heritage::math::DVec3 globalPosition{ 0.0, 0.0, 0.0 };
        heritage::math::DVec3 supportPlanePoint{ 0.0, 0.0, 0.0 };
        heritage::math::Vec3 supportPlaneNormal{ 0.0f, 1.0f, 0.0f };
        heritage::math::Vec3 supportForward{ 0.0f, 0.0f, 1.0f };
        heritage::math::Vec3 velocityMps{ 0.0f, 0.0f, 0.0f };
        heritage::math::Vec3 axisRight{ 1.0f, 0.0f, 0.0f };
        heritage::math::Vec3 axisForward{ 0.0f, 0.0f, 1.0f };
        heritage::math::Vec3 axisNormal{ 0.0f, 1.0f, 0.0f };
        heritage::math::Vec3 angularVelocityRadPerSecond{ 0.0f, 0.0f, 0.0f };
        SurfaceMaterial material = SurfaceMaterial::Default;
        TrackRubberTransientPhase phase = TrackRubberTransientPhase::Airborne;
        float quantity = 0.0f;
        float marbleMaturity = 0.0f;
        float fragmentSeverity = 0.0f;
        float piecePopulation = 0.0f;
        float ageSeconds = 0.0f;
        float groundTimeSeconds = 0.0f;
        float lengthM = 0.025f;
        float widthM = 0.007f;
        float flutterPhase = 0.0f;
        float bendVertex1M = 0.0f;
        float bendVertex3M = 0.0f;
        std::uint32_t seed = 0;
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
    heritage::math::DVec3 cellCenter(const ChunkKey& chunk, const LocalKey& cell) const;
    Cell resolvedCell(const Cell& cell) const;
    TrackRubberSample sampleFromCell(const Cell& cell, float localWetness) const;
    Cell& acquireCell(
        const heritage::math::DVec3& globalPosition,
        SurfaceMaterial material);
    void touchChunk(const ChunkKey& key, Chunk& chunk);
    bool evictLeastRecentlyUsedChunk(const ChunkKey* protectedChunk = nullptr);
    void makeRoomForChunk(const ChunkKey* protectedChunk = nullptr);
    void makeRoomForCell(const ChunkKey* protectedChunk = nullptr);
    TrackRubberChunkSnapshot snapshotChunk(
        const ChunkKey& key,
        const Chunk& chunk) const;
    void eraseChunk(
        const ChunkKey& key,
        std::unordered_map<ChunkKey, Chunk, ChunkKeyHash>::iterator found,
        bool notifyEviction);
    float addLooseAbsolute(
        const heritage::math::DVec3& position,
        SurfaceMaterial material,
        float quantity,
        float maturity,
        float fragmentSeverity,
        float piecePopulation,
        const heritage::math::Vec3& supportNormal,
        const heritage::math::Vec3& supportForward);
    float transferLooseRubber(
        const heritage::math::DVec3& sourcePosition,
        const heritage::math::DVec3& destinationPosition,
        SurfaceMaterial material,
        float requestedQuantity);
    float liftLooseRubber(
        const heritage::math::DVec3& sourcePosition,
        SurfaceMaterial material,
        float requestedQuantity,
        const heritage::math::Vec3& initialVelocityMps,
        std::uint32_t seed);
    void enqueueFreshTransient(
        const heritage::math::DVec3& sourcePosition,
        const TrackRubberContactInput& input,
        float quantity,
        float maturity,
        float fragmentSeverity,
        float pieceDensity);
    void integrateTransients(float deltaTimeSeconds);
    float commitTransient(const TransientPacket& packet);

    void applyRubberDelta(
        const heritage::math::DVec3& globalPosition,
        const TrackRubberContactInput& input,
        float depositedDelta,
        float looseDelta,
        float looseSourceMaturity,
        float looseSourceFragmentSeverity,
        float looseSourcePieceDensity,
        float maturationDelta,
        float passProgressDelta,
        bool recordContact);

    TrackRubberDescription m_description{};
    std::unordered_map<ChunkKey, Chunk, ChunkKeyHash> m_chunks;
    std::list<ChunkKey> m_chunkLru;
    std::size_t m_cellCount = 0;
    double m_elapsedSeconds = 0.0;
    double m_globalWetExposureSeconds = 0.0;
    std::uint64_t m_updateSerial = 0;
    std::uint64_t m_contactSamples = 0;
    double m_depositedGeneration = 0.0;
    double m_looseGeneration = 0.0;
    double m_persistentPiecePopulation = 0.0;
    double m_transientPiecePopulation = 0.0;
    double m_transientLooseQuantity = 0.0;
    std::vector<TransientPacket> m_transientPackets;
    ChunkEvictionCallback m_chunkEvictionCallback;
};

} // namespace heritage::physics::rubber
