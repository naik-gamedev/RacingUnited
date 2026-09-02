#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Graphics/AssetMetadata.hpp"
#include "../Core/Math/Math.hpp"

namespace heritage::vehicles {

struct VehicleAssetPartMetadata
{
    int nodeIndex = -1;
    int parentNodeIndex = -1;
    std::string nodeName;
    std::string slot;
    std::string role;
    std::string partType;
    std::string partId;
    std::string corner;
    bool replaceable = false;
    bool rotatesWithWheel = false;
    heritage::graphics::AssetMetadataMap properties;
};


struct VehicleAssetWheelFitmentDatumMetadata
{
    int nodeIndex = -1;
    std::string nodeName;
    std::string corner;
    std::string role;
    heritage::math::Vec3 localPosition{};
    // For wheel_spin_axis datums, the node's local +X axis is the authored
    // rotation-axis direction after the glTF basis conversion.
    heritage::math::Vec3 localAxis{ 1.0f, 0.0f, 0.0f };
    std::string provenance = "asset_authored";
    float confidence = 0.75f;
};

struct VehicleAssetSuspensionHardpointMetadata
{
    int nodeIndex = -1;
    std::string nodeName;
    std::string corner;
    std::string id;
    heritage::math::Vec3 localPosition{};
    std::string provenance = "asset_authored";
    float confidence = 0.75f;
};

// Coarse but deterministic ride-height datums reconstructed from the GLB's
// authored node transforms and POSITION accessor bounds. Tire geometry defines
// the authored road plane; non-wheel geometry defines front/rear lowest points.
struct VehicleAssetRideHeightGeometryMetadata
{
    bool valid = false;
    heritage::math::Vec3 bodyMinimum{};
    heritage::math::Vec3 bodyMaximum{};
    float referenceGroundPlaneLocalY = 0.0f;
    float axleSplitLocalZ = 0.0f;
    float frontLowestBodyLocalY = 0.0f;
    float rearLowestBodyLocalY = 0.0f;
    float frontAuthoredClearanceM = 0.0f;
    float rearAuthoredClearanceM = 0.0f;
    std::size_t bodyNodeCount = 0;
    std::size_t tireNodeCount = 0;
    std::string provenance = "glb_accessor_bounds_v1";
};

// Nominal geometry reconstructed from the authored tire mesh bounds. This is
// deliberately separate from the vehicle definition's factory reference and
// from live suspension geometry so tooling can expose mismatches instead of
// silently treating the visual model as engineering truth.
struct VehicleAssetWheelGeometryMetadata
{
    bool valid = false;
    float wheelbaseM = 0.0f;
    float frontTrackM = 0.0f;
    float rearTrackM = 0.0f;
    heritage::math::Vec3 frontLeftCenter{};
    heritage::math::Vec3 frontRightCenter{};
    heritage::math::Vec3 rearLeftCenter{};
    heritage::math::Vec3 rearRightCenter{};
    std::size_t tireNodeCount = 0;
    std::string provenance = "glb_tire_accessor_bounds_v1";
};

struct VehicleAssetMetadata
{
    std::filesystem::path sourcePath;
    std::vector<VehicleAssetPartMetadata> parts;
    std::vector<VehicleAssetWheelFitmentDatumMetadata> wheelFitmentDatums;
    std::vector<VehicleAssetSuspensionHardpointMetadata> suspensionHardpoints;
    VehicleAssetRideHeightGeometryMetadata rideHeightGeometry;
    VehicleAssetWheelGeometryMetadata wheelGeometry;
    std::vector<std::string> warnings;

    const VehicleAssetPartMetadata* findBySlot(const std::string& slot) const;
    const VehicleAssetWheelFitmentDatumMetadata* findWheelFitmentDatum(
        const std::string& corner,
        const std::string& role) const;
    const VehicleAssetSuspensionHardpointMetadata* findSuspensionHardpoint(
        const std::string& corner,
        const std::string& id) const;
};

struct VehiclePartCompatibility
{
    bool compatible = false;
    bool complete = false;
    std::vector<std::string> notes;

    std::string summary() const;
};

// Reads Blender Custom Properties exported through glTF node `extras` and
// discovers Heritage semantic parts such as WH_FL / WH_FL_Tire.
bool inspectVehicleAssetMetadata(
    const std::filesystem::path& glbPath,
    VehicleAssetMetadata& output,
    std::string& errorMessage);

// Compatibility check between a wheel/rim part and tire part. The contract is
// deliberately data-driven: diameter is authoritative, and optional authored
// rim-width min/max ranges are used when present. Unknown data is reported
// rather than invented.
VehiclePartCompatibility checkTireWheelCompatibility(
    const VehicleAssetPartMetadata& wheel,
    const VehicleAssetPartMetadata& tire);

// Nominal unloaded tire outside diameter derived from ISO-style size fields.
// Returns <= 0 when required metadata is absent.
double tireNominalDiameterMeters(const VehicleAssetPartMetadata& tire);

} // namespace heritage::vehicles
