#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Core/Math/Math.hpp"

namespace heritage::graphics {

// VEG01 foundation: one vegetation architecture for trees, shrubs, grass,
// reeds, flowers, crops and future plant families. The renderer-specific
// octahedral impostor implementation arrives later; this system deliberately
// owns the stable species/LOD/wind/instance contract first.
enum class VegetationKind
{
    Tree,
    Shrub,
    Grass,
    Reed,
    Flower,
    Crop,
    Generic
};

enum class VegetationRepresentation
{
    DetailedGeometry,
    ClusterOctahedral,
    MergedClusterOctahedral,
    WholePlantOctahedral,
    TerrainCoverage,
    Culled
};

struct VegetationRepresentationBlend
{
    VegetationRepresentation nearRepresentation = VegetationRepresentation::Culled;
    VegetationRepresentation farRepresentation = VegetationRepresentation::Culled;
    float nearWeight = 1.0f;
    float farWeight = 0.0f;
};

struct VegetationLodPolicy
{
    // Distances are monotonically increasing boundaries in metres.
    // Their meaning is representation-oriented rather than asset-name-oriented:
    // the best available representation is selected at each band.
    float detailedEndMeters = 50.0f;
    float mergedClusterEndMeters = 200.0f;
    float wholePlantEndMeters = 1800.0f;
    float terrainCoverageEndMeters = 8000.0f;
};

struct VegetationSpecies
{
    std::string id;
    VegetationKind kind = VegetationKind::Generic;
    VegetationLodPolicy lod{};

    // Optional asset capabilities. VEG01 does not render these yet; the flags
    // let authoring/configuration exist before an octahedral baker/shader does.
    bool hasClusterOctahedral = false;
    bool hasWholePlantOctahedral = false;
    bool hasTerrainCoverage = false;

    // Hierarchical wind response weights consumed by future vegetation shaders.
    // They remain normalized authoring controls instead of hard-coded species
    // behavior in renderer code.
    float trunkWindResponse = 0.25f;
    float branchWindResponse = 0.65f;
    float foliageWindResponse = 1.0f;
};

// Compact large-world placement used by the VEG01 registry. Absolute world
// placement is split into signed 64 m chunk coordinates plus 16-bit local
// coordinates. That gives ~0.98 mm local resolution while avoiding FP16's
// distance-dependent precision loss. This is storage, not a render transform.
struct QuantizedVegetationInstance
{
    std::uint32_t speciesIndex = 0;
    std::int32_t chunkX = 0;
    std::int32_t chunkY = 0;
    std::int32_t chunkZ = 0;
    std::uint16_t localX = 0;
    std::uint16_t localY = 0;
    std::uint16_t localZ = 0;
    std::uint16_t yaw = 0;
    std::uint16_t scale = 0;
    std::uint32_t variationSeed = 0;
};

static_assert(
    sizeof(QuantizedVegetationInstance) <= 32,
    "VEG01 vegetation placement must stay compact.");

struct VegetationWindState
{
    heritage::math::Vec3 velocityMetersPerSecond{ 0.0f, 0.0f, 0.0f };
    float gust = 0.0f;
    float turbulence = 0.0f;
};

struct VegetationStats
{
    std::size_t speciesCount = 0;
    std::size_t instanceCount = 0;
    std::size_t occupiedChunkCount = 0;
    std::size_t packedBytes = 0;
};

class VegetationSystem
{
public:
    static constexpr double kChunkSizeMeters = 64.0;
    static constexpr float kMinimumInstanceScale = 0.25f;
    static constexpr float kMaximumInstanceScale = 4.0f;

    void reset();
    bool isAvailable() const { return true; }

    bool registerSpecies(const VegetationSpecies& species);
    bool registerSpeciesWithDefaults(
        const std::string& id,
        VegetationKind kind,
        bool clusterOctahedral,
        bool wholePlantOctahedral,
        bool terrainCoverage);
    bool setSpeciesLodPolicy(
        const std::string& id,
        const VegetationLodPolicy& policy);
    bool setSpeciesWindResponse(
        const std::string& id,
        float trunk,
        float branch,
        float foliage);

    bool hasSpecies(const std::string& id) const;
    const VegetationSpecies* species(const std::string& id) const;
    std::size_t speciesCount() const { return m_species.size(); }

    bool addInstance(
        const std::string& speciesId,
        const heritage::math::DVec3& globalPosition,
        float yawDegrees = 0.0f,
        float uniformScale = 1.0f,
        std::uint32_t variationSeed = 0);
    void clearInstances();
    std::size_t instanceCount() const { return m_instances.size(); }
    std::size_t occupiedChunkCount() const { return m_chunkPopulation.size(); }

    bool instanceGlobalPosition(
        std::size_t index,
        heritage::math::DVec3& position) const;

    // Renderers should prefer the blend form so Heritage's master LOD policy
    // can crossfade/morph between representations instead of popping. The
    // legacy single-representation query returns the dominant side for callers
    // that cannot blend yet.
    VegetationRepresentationBlend representationBlendForDistance(
        const std::string& speciesId,
        float distanceMeters) const;
    VegetationRepresentation representationForDistance(
        const std::string& speciesId,
        float distanceMeters) const;

    void setWind(const VegetationWindState& wind);
    const VegetationWindState& wind() const { return m_wind; }

    VegetationStats stats() const;
    const std::string& lastError() const { return m_lastError; }

    static VegetationKind parseKind(const std::string& text);
    static const char* kindName(VegetationKind kind);
    static const char* representationName(VegetationRepresentation value);
    static VegetationLodPolicy defaultLodPolicy(VegetationKind kind);

private:
    struct ChunkKey
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;

        bool operator==(const ChunkKey& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct ChunkKeyHash
    {
        std::size_t operator()(const ChunkKey& key) const;
    };

    static bool validLodPolicy(const VegetationLodPolicy& policy);
    static bool finitePosition(const heritage::math::DVec3& position);
    static bool encodeAxis(
        double worldMeters,
        std::int32_t& chunk,
        std::uint16_t& local);
    static double decodeAxis(std::int32_t chunk, std::uint16_t local);
    static std::uint16_t encodeYaw(float yawDegrees);
    static std::uint16_t encodeScale(float scale);

    std::vector<VegetationSpecies> m_species;
    std::unordered_map<std::string, std::uint32_t> m_speciesById;
    std::vector<QuantizedVegetationInstance> m_instances;
    std::unordered_map<ChunkKey, std::size_t, ChunkKeyHash> m_chunkPopulation;
    VegetationWindState m_wind{};
    std::string m_lastError;
};

} // namespace heritage::graphics
