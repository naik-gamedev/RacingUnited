#include "VegetationSystem.hpp"
#include "LodTransitionPolicy.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace heritage::graphics {
namespace {

std::string lowercase(std::string text)
{
    std::transform(
        text.begin(), text.end(), text.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return text;
}

float clamp01(float value)
{
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

} // namespace

void VegetationSystem::reset()
{
    m_species.clear();
    m_speciesById.clear();
    m_instances.clear();
    m_chunkPopulation.clear();
    m_wind = {};
    m_lastError.clear();
}

bool VegetationSystem::registerSpecies(const VegetationSpecies& speciesValue)
{
    m_lastError.clear();
    if (speciesValue.id.empty())
    {
        m_lastError = "Vegetation species id cannot be empty.";
        return false;
    }
    if (m_speciesById.find(speciesValue.id) != m_speciesById.end())
    {
        m_lastError = "Vegetation species already exists: " + speciesValue.id;
        return false;
    }
    if (!validLodPolicy(speciesValue.lod))
    {
        m_lastError = "Vegetation LOD distances must be finite, non-negative, and monotonically increasing.";
        return false;
    }
    if (!std::isfinite(speciesValue.trunkWindResponse)
        || !std::isfinite(speciesValue.branchWindResponse)
        || !std::isfinite(speciesValue.foliageWindResponse))
    {
        m_lastError = "Vegetation wind response values must be finite.";
        return false;
    }

    VegetationSpecies normalized = speciesValue;
    normalized.trunkWindResponse = clamp01(normalized.trunkWindResponse);
    normalized.branchWindResponse = clamp01(normalized.branchWindResponse);
    normalized.foliageWindResponse = clamp01(normalized.foliageWindResponse);

    const std::uint32_t index = static_cast<std::uint32_t>(m_species.size());
    m_species.push_back(normalized);
    m_speciesById.emplace(normalized.id, index);
    return true;
}

bool VegetationSystem::registerSpeciesWithDefaults(
    const std::string& id,
    VegetationKind kind,
    bool clusterOctahedral,
    bool wholePlantOctahedral,
    bool terrainCoverage)
{
    VegetationSpecies value;
    value.id = id;
    value.kind = kind;
    value.lod = defaultLodPolicy(kind);
    value.hasClusterOctahedral = clusterOctahedral;
    value.hasWholePlantOctahedral = wholePlantOctahedral;
    value.hasTerrainCoverage = terrainCoverage;

    switch (kind)
    {
    case VegetationKind::Tree:
        value.trunkWindResponse = 0.25f;
        value.branchWindResponse = 0.65f;
        value.foliageWindResponse = 1.0f;
        break;
    case VegetationKind::Shrub:
        value.trunkWindResponse = 0.45f;
        value.branchWindResponse = 0.80f;
        value.foliageWindResponse = 1.0f;
        break;
    case VegetationKind::Grass:
    case VegetationKind::Reed:
        value.trunkWindResponse = 0.90f;
        value.branchWindResponse = 1.0f;
        value.foliageWindResponse = 1.0f;
        break;
    default:
        break;
    }

    return registerSpecies(value);
}

bool VegetationSystem::setSpeciesLodPolicy(
    const std::string& id,
    const VegetationLodPolicy& policy)
{
    m_lastError.clear();
    if (!validLodPolicy(policy))
    {
        m_lastError = "Vegetation LOD distances must be finite, non-negative, and monotonically increasing.";
        return false;
    }
    const auto found = m_speciesById.find(id);
    if (found == m_speciesById.end())
    {
        m_lastError = "Unknown vegetation species: " + id;
        return false;
    }
    m_species[found->second].lod = policy;
    return true;
}

bool VegetationSystem::setSpeciesWindResponse(
    const std::string& id,
    float trunk,
    float branch,
    float foliage)
{
    m_lastError.clear();
    if (!std::isfinite(trunk) || !std::isfinite(branch) || !std::isfinite(foliage))
    {
        m_lastError = "Vegetation wind response values must be finite.";
        return false;
    }
    const auto found = m_speciesById.find(id);
    if (found == m_speciesById.end())
    {
        m_lastError = "Unknown vegetation species: " + id;
        return false;
    }
    VegetationSpecies& value = m_species[found->second];
    value.trunkWindResponse = clamp01(trunk);
    value.branchWindResponse = clamp01(branch);
    value.foliageWindResponse = clamp01(foliage);
    return true;
}

bool VegetationSystem::hasSpecies(const std::string& id) const
{
    return m_speciesById.find(id) != m_speciesById.end();
}

const VegetationSpecies* VegetationSystem::species(const std::string& id) const
{
    const auto found = m_speciesById.find(id);
    if (found == m_speciesById.end())
        return nullptr;
    return &m_species[found->second];
}

bool VegetationSystem::addInstance(
    const std::string& speciesId,
    const heritage::math::DVec3& globalPosition,
    float yawDegrees,
    float uniformScale,
    std::uint32_t variationSeed)
{
    m_lastError.clear();
    const auto found = m_speciesById.find(speciesId);
    if (found == m_speciesById.end())
    {
        m_lastError = "Unknown vegetation species: " + speciesId;
        return false;
    }
    if (!finitePosition(globalPosition))
    {
        m_lastError = "Vegetation instance position must be finite.";
        return false;
    }
    if (!std::isfinite(yawDegrees)
        || !std::isfinite(uniformScale)
        || uniformScale < kMinimumInstanceScale
        || uniformScale > kMaximumInstanceScale)
    {
        m_lastError = "Vegetation yaw/scale is invalid. Scale must remain within 0.25x..4.0x.";
        return false;
    }

    QuantizedVegetationInstance instance;
    instance.speciesIndex = found->second;
    if (!encodeAxis(globalPosition.x, instance.chunkX, instance.localX)
        || !encodeAxis(globalPosition.y, instance.chunkY, instance.localY)
        || !encodeAxis(globalPosition.z, instance.chunkZ, instance.localZ))
    {
        m_lastError = "Vegetation instance is outside the supported chunk coordinate range.";
        return false;
    }
    instance.yaw = encodeYaw(yawDegrees);
    instance.scale = encodeScale(uniformScale);
    instance.variationSeed = variationSeed;

    m_instances.push_back(instance);
    const ChunkKey key{ instance.chunkX, instance.chunkY, instance.chunkZ };
    ++m_chunkPopulation[key];
    return true;
}

void VegetationSystem::clearInstances()
{
    m_instances.clear();
    m_chunkPopulation.clear();
    m_lastError.clear();
}

bool VegetationSystem::instanceGlobalPosition(
    std::size_t index,
    heritage::math::DVec3& position) const
{
    if (index >= m_instances.size())
        return false;
    const QuantizedVegetationInstance& instance = m_instances[index];
    position = {
        decodeAxis(instance.chunkX, instance.localX),
        decodeAxis(instance.chunkY, instance.localY),
        decodeAxis(instance.chunkZ, instance.localZ)
    };
    return true;
}

VegetationRepresentationBlend VegetationSystem::representationBlendForDistance(
    const std::string& speciesId,
    float distanceMeters) const
{
    const VegetationSpecies* value = species(speciesId);
    if (!value || !std::isfinite(distanceMeters))
        return {};

    const float distance = (std::max)(0.0f, distanceMeters);
    const auto hardRepresentation = [&](float sampleDistance) {
        if (sampleDistance <= value->lod.detailedEndMeters)
        {
            return value->hasClusterOctahedral
                ? VegetationRepresentation::ClusterOctahedral
                : VegetationRepresentation::DetailedGeometry;
        }
        if (sampleDistance <= value->lod.mergedClusterEndMeters)
        {
            return value->hasClusterOctahedral
                ? VegetationRepresentation::MergedClusterOctahedral
                : VegetationRepresentation::DetailedGeometry;
        }
        if (sampleDistance <= value->lod.wholePlantEndMeters)
        {
            return value->hasWholePlantOctahedral
                ? VegetationRepresentation::WholePlantOctahedral
                : (value->hasClusterOctahedral
                    ? VegetationRepresentation::MergedClusterOctahedral
                    : VegetationRepresentation::DetailedGeometry);
        }
        if (sampleDistance <= value->lod.terrainCoverageEndMeters
            && value->hasTerrainCoverage)
        {
            return VegetationRepresentation::TerrainCoverage;
        }
        return VegetationRepresentation::Culled;
    };

    const float boundaries[] = {
        value->lod.detailedEndMeters,
        value->lod.mergedClusterEndMeters,
        value->lod.wholePlantEndMeters,
        value->lod.terrainCoverageEndMeters
    };
    for (const float boundary : boundaries)
    {
        const float width = lod::lodBlendWidthMeters(boundary);
        const float halfWidth = width * 0.5f;
        if (distance < boundary - halfWidth || distance > boundary + halfWidth)
            continue;

        const VegetationRepresentation nearRepresentation = hardRepresentation(
            (std::max)(0.0f, boundary - halfWidth - 0.001f));
        const VegetationRepresentation farRepresentation = hardRepresentation(
            boundary + halfWidth + 0.001f);
        if (nearRepresentation == farRepresentation)
            continue;

        const lod::LodCrossfade blend = lod::crossfadeAtBoundary(
            distance, boundary, width);
        return {
            nearRepresentation,
            farRepresentation,
            blend.nearWeight,
            blend.farWeight
        };
    }

    const VegetationRepresentation representation = hardRepresentation(distance);
    return { representation, representation, 1.0f, 0.0f };
}

VegetationRepresentation VegetationSystem::representationForDistance(
    const std::string& speciesId,
    float distanceMeters) const
{
    const VegetationRepresentationBlend blend = representationBlendForDistance(
        speciesId, distanceMeters);
    return blend.farWeight > blend.nearWeight
        ? blend.farRepresentation
        : blend.nearRepresentation;
}

void VegetationSystem::setWind(const VegetationWindState& windValue)
{
    m_wind.velocityMetersPerSecond = {
        std::isfinite(windValue.velocityMetersPerSecond.x)
            ? windValue.velocityMetersPerSecond.x : 0.0f,
        std::isfinite(windValue.velocityMetersPerSecond.y)
            ? windValue.velocityMetersPerSecond.y : 0.0f,
        std::isfinite(windValue.velocityMetersPerSecond.z)
            ? windValue.velocityMetersPerSecond.z : 0.0f
    };
    m_wind.gust = std::isfinite(windValue.gust) ? clamp01(windValue.gust) : 0.0f;
    m_wind.turbulence = std::isfinite(windValue.turbulence)
        ? clamp01(windValue.turbulence) : 0.0f;
}

VegetationStats VegetationSystem::stats() const
{
    VegetationStats result;
    result.speciesCount = m_species.size();
    result.instanceCount = m_instances.size();
    result.occupiedChunkCount = m_chunkPopulation.size();
    result.packedBytes = m_instances.size() * sizeof(QuantizedVegetationInstance);
    return result;
}

VegetationKind VegetationSystem::parseKind(const std::string& text)
{
    const std::string value = lowercase(text);
    if (value == "tree" || value == "treeland") return VegetationKind::Tree;
    if (value == "shrub" || value == "bush" || value == "shrubland") return VegetationKind::Shrub;
    if (value == "grass" || value == "grassland") return VegetationKind::Grass;
    if (value == "reed") return VegetationKind::Reed;
    if (value == "flower") return VegetationKind::Flower;
    if (value == "crop") return VegetationKind::Crop;
    return VegetationKind::Generic;
}

const char* VegetationSystem::kindName(VegetationKind kind)
{
    switch (kind)
    {
    case VegetationKind::Tree: return "tree";
    case VegetationKind::Shrub: return "shrub";
    case VegetationKind::Grass: return "grass";
    case VegetationKind::Reed: return "reed";
    case VegetationKind::Flower: return "flower";
    case VegetationKind::Crop: return "crop";
    default: return "generic";
    }
}

const char* VegetationSystem::representationName(VegetationRepresentation value)
{
    switch (value)
    {
    case VegetationRepresentation::DetailedGeometry: return "detailed_geometry";
    case VegetationRepresentation::ClusterOctahedral: return "cluster_octahedral";
    case VegetationRepresentation::MergedClusterOctahedral: return "merged_cluster_octahedral";
    case VegetationRepresentation::WholePlantOctahedral: return "whole_plant_octahedral";
    case VegetationRepresentation::TerrainCoverage: return "terrain_coverage";
    default: return "culled";
    }
}

VegetationLodPolicy VegetationSystem::defaultLodPolicy(VegetationKind kind)
{
    switch (kind)
    {
    case VegetationKind::Tree:
        return { 60.0f, 250.0f, 2500.0f, 15000.0f };
    case VegetationKind::Shrub:
        return { 30.0f, 120.0f, 800.0f, 3000.0f };
    case VegetationKind::Grass:
        // Near grass remains geometry/cards. A 0.5 m-ish authored clump can
        // opt into cluster octahedral representation in the next distance band.
        return { 18.0f, 70.0f, 220.0f, 700.0f };
    case VegetationKind::Reed:
        return { 25.0f, 90.0f, 300.0f, 900.0f };
    case VegetationKind::Flower:
        return { 15.0f, 50.0f, 150.0f, 450.0f };
    case VegetationKind::Crop:
        return { 25.0f, 100.0f, 350.0f, 1200.0f };
    default:
        return { 40.0f, 150.0f, 1200.0f, 5000.0f };
    }
}

std::size_t VegetationSystem::ChunkKeyHash::operator()(const ChunkKey& key) const
{
    // Small integer chunk coordinates dominate normal worlds. Mix all axes
    // without depending on platform-specific hash implementations.
    std::size_t seed = static_cast<std::uint32_t>(key.x);
    seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(key.y))
        + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(key.z))
        + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

bool VegetationSystem::validLodPolicy(const VegetationLodPolicy& policy)
{
    return std::isfinite(policy.detailedEndMeters)
        && std::isfinite(policy.mergedClusterEndMeters)
        && std::isfinite(policy.wholePlantEndMeters)
        && std::isfinite(policy.terrainCoverageEndMeters)
        && policy.detailedEndMeters >= 0.0f
        && policy.mergedClusterEndMeters >= policy.detailedEndMeters
        && policy.wholePlantEndMeters >= policy.mergedClusterEndMeters
        && policy.terrainCoverageEndMeters >= policy.wholePlantEndMeters;
}

bool VegetationSystem::finitePosition(const heritage::math::DVec3& position)
{
    return std::isfinite(position.x)
        && std::isfinite(position.y)
        && std::isfinite(position.z);
}

bool VegetationSystem::encodeAxis(
    double worldMeters,
    std::int32_t& chunk,
    std::uint16_t& local)
{
    const double rawChunk = std::floor(worldMeters / kChunkSizeMeters);
    if (rawChunk < static_cast<double>(std::numeric_limits<std::int32_t>::min())
        || rawChunk > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
    {
        return false;
    }

    chunk = static_cast<std::int32_t>(rawChunk);
    double localMeters = worldMeters - rawChunk * kChunkSizeMeters;
    localMeters = (std::max)(0.0, (std::min)(kChunkSizeMeters, localMeters));
    const double normalized = localMeters / kChunkSizeMeters;
    const long encoded = std::lround(normalized * 65535.0);
    local = static_cast<std::uint16_t>(
        (std::max)(0L, (std::min)(65535L, encoded)));
    return true;
}

double VegetationSystem::decodeAxis(std::int32_t chunk, std::uint16_t local)
{
    return static_cast<double>(chunk) * kChunkSizeMeters
        + (static_cast<double>(local) / 65535.0) * kChunkSizeMeters;
}

std::uint16_t VegetationSystem::encodeYaw(float yawDegrees)
{
    float wrapped = std::fmod(yawDegrees, 360.0f);
    if (wrapped < 0.0f)
        wrapped += 360.0f;
    return static_cast<std::uint16_t>(
        std::lround((wrapped / 360.0f) * 65535.0f));
}

std::uint16_t VegetationSystem::encodeScale(float scale)
{
    const float normalized =
        (scale - kMinimumInstanceScale)
        / (kMaximumInstanceScale - kMinimumInstanceScale);
    return static_cast<std::uint16_t>(
        std::lround(clamp01(normalized) * 65535.0f));
}

} // namespace heritage::graphics
