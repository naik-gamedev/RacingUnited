#include "VehicleAssetMetadata.hpp"

#include "../Graphics/GltfBinary.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace heritage::vehicles {
namespace {

constexpr const char* kVehicleAssetMetadataMarker =
    "HERITAGE_VA01_GLTF_EXTRAS_VEHICLE_PARTS";

const heritage::graphics::AssetMetadataValue* property(
    const heritage::graphics::AssetMetadataMap& properties,
    const std::string& key)
{
    const auto iterator = properties.find(key);
    return iterator == properties.end() ? nullptr : &iterator->second;
}

std::string stringProperty(
    const heritage::graphics::AssetMetadataMap& properties,
    const std::string& key)
{
    const auto* value = property(properties, key);
    if (!value || value->type != heritage::graphics::AssetMetadataValueType::String)
        return {};
    return value->stringValue;
}

double numberProperty(
    const heritage::graphics::AssetMetadataMap& properties,
    const std::string& key,
    double fallback = 0.0)
{
    const auto* value = property(properties, key);
    if (!value || value->type != heritage::graphics::AssetMetadataValueType::Number)
        return fallback;
    return value->numberValue;
}

bool boolProperty(
    const heritage::graphics::AssetMetadataMap& properties,
    const std::string& key,
    bool fallback = false)
{
    const auto* value = property(properties, key);
    if (!value || value->type != heritage::graphics::AssetMetadataValueType::Boolean)
        return fallback;
    return value->boolValue;
}

struct Mat4
{
    std::array<float, 16> m{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f };
};

Mat4 multiply(const Mat4& left, const Mat4& right)
{
    Mat4 result{};
    result.m.fill(0.0f);
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            result.m[static_cast<std::size_t>(column * 4 + row)] =
                left.m[static_cast<std::size_t>(0 * 4 + row)] * right.m[static_cast<std::size_t>(column * 4 + 0)]
                + left.m[static_cast<std::size_t>(1 * 4 + row)] * right.m[static_cast<std::size_t>(column * 4 + 1)]
                + left.m[static_cast<std::size_t>(2 * 4 + row)] * right.m[static_cast<std::size_t>(column * 4 + 2)]
                + left.m[static_cast<std::size_t>(3 * 4 + row)] * right.m[static_cast<std::size_t>(column * 4 + 3)];
        }
    }
    return result;
}

Mat4 translationMatrix(float x, float y, float z)
{
    Mat4 result;
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

Mat4 scaleMatrix(float x, float y, float z)
{
    Mat4 result;
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    return result;
}

Mat4 quaternionMatrix(float x, float y, float z, float w)
{
    Mat4 result;
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;
    result.m[0] = 1.0f - 2.0f * (yy + zz);
    result.m[1] = 2.0f * (xy + wz);
    result.m[2] = 2.0f * (xz - wy);
    result.m[4] = 2.0f * (xy - wz);
    result.m[5] = 1.0f - 2.0f * (xx + zz);
    result.m[6] = 2.0f * (yz + wx);
    result.m[8] = 2.0f * (xz + wy);
    result.m[9] = 2.0f * (yz - wx);
    result.m[10] = 1.0f - 2.0f * (xx + yy);
    return result;
}

Mat4 localNodeMatrix(const heritage::graphics::MeshNode& node)
{
    if (node.hasMatrix)
    {
        Mat4 result;
        result.m = node.localMatrix;
        return result;
    }
    return multiply(
        translationMatrix(
            node.translation[0], node.translation[1], node.translation[2]),
        multiply(
            quaternionMatrix(
                node.rotation[0], node.rotation[1],
                node.rotation[2], node.rotation[3]),
            scaleMatrix(node.scale[0], node.scale[1], node.scale[2])));
}

Mat4 worldNodeMatrix(
    const heritage::graphics::GlbMetadataDocument& document,
    int nodeIndex,
    std::vector<Mat4>& cache,
    std::vector<unsigned char>& state)
{
    if (nodeIndex < 0
        || static_cast<std::size_t>(nodeIndex) >= document.nodes.size())
    {
        return {};
    }
    const std::size_t index = static_cast<std::size_t>(nodeIndex);
    if (state[index] == 2)
        return cache[index];
    if (state[index] == 1)
        return localNodeMatrix(document.nodes[index]);

    state[index] = 1;
    const auto& node = document.nodes[index];
    Mat4 world = localNodeMatrix(node);
    if (node.parentIndex >= 0)
    {
        world = multiply(
            worldNodeMatrix(document, node.parentIndex, cache, state),
            world);
    }
    cache[index] = world;
    state[index] = 2;
    return world;
}

heritage::math::Vec3 matrixTranslation(const Mat4& matrix)
{
    return { matrix.m[12], matrix.m[13], matrix.m[14] };
}

heritage::math::Vec3 matrixXAxis(const Mat4& matrix)
{
    const float x = matrix.m[0];
    const float y = matrix.m[1];
    const float z = matrix.m[2];
    const float magnitude = std::sqrt(x * x + y * y + z * z);
    if (!std::isfinite(magnitude) || magnitude <= 1.0e-8f)
        return { 1.0f, 0.0f, 0.0f };
    return { x / magnitude, y / magnitude, z / magnitude };
}

heritage::math::Vec3 transformPoint(
    const Mat4& matrix,
    float x,
    float y,
    float z)
{
    return {
        matrix.m[0] * x + matrix.m[4] * y + matrix.m[8] * z + matrix.m[12],
        matrix.m[1] * x + matrix.m[5] * y + matrix.m[9] * z + matrix.m[13],
        matrix.m[2] * x + matrix.m[6] * y + matrix.m[10] * z + matrix.m[14] };
}

std::string lowerAscii(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

bool wheelAssemblyNode(
    const heritage::graphics::MeshNode& node)
{
    const std::string lowerName = lowerAscii(node.name);
    const std::string partType = lowerAscii(stringProperty(
        node.metadata, "heritage.part_type"));
    const std::string role = lowerAscii(stringProperty(
        node.metadata, "heritage.role"));
    return lowerName.rfind("wh_", 0) == 0
        || partType == "wheel" || partType == "tire" || partType == "tyre"
        || partType == "brake" || role.find("wheel") != std::string::npos
        || role.find("tire") != std::string::npos
        || role.find("tyre") != std::string::npos;
}

bool tireGeometryNode(
    const heritage::graphics::MeshNode& node)
{
    const std::string lowerName = lowerAscii(node.name);
    const std::string partType = lowerAscii(stringProperty(
        node.metadata, "heritage.part_type"));
    return partType == "tire" || partType == "tyre"
        || lowerName.find("tire") != std::string::npos
        || lowerName.find("tyre") != std::string::npos;
}

struct WorldBounds
{
    bool valid = false;
    heritage::math::Vec3 minimum{};
    heritage::math::Vec3 maximum{};
};

WorldBounds transformedBounds(
    const heritage::graphics::GlbMetadataDocument::NodeGeometryBounds& local,
    const Mat4& world)
{
    WorldBounds result;
    if (!local.valid)
        return result;
    for (int xIndex = 0; xIndex < 2; ++xIndex)
    {
        for (int yIndex = 0; yIndex < 2; ++yIndex)
        {
            for (int zIndex = 0; zIndex < 2; ++zIndex)
            {
                const heritage::math::Vec3 point = transformPoint(
                    world,
                    xIndex ? local.maximum[0] : local.minimum[0],
                    yIndex ? local.maximum[1] : local.minimum[1],
                    zIndex ? local.maximum[2] : local.minimum[2]);
                if (!result.valid)
                {
                    result.minimum = point;
                    result.maximum = point;
                    result.valid = true;
                }
                else
                {
                    result.minimum.x = std::min(result.minimum.x, point.x);
                    result.minimum.y = std::min(result.minimum.y, point.y);
                    result.minimum.z = std::min(result.minimum.z, point.z);
                    result.maximum.x = std::max(result.maximum.x, point.x);
                    result.maximum.y = std::max(result.maximum.y, point.y);
                    result.maximum.z = std::max(result.maximum.z, point.z);
                }
            }
        }
    }
    return result;
}

void inspectRideHeightGeometry(
    const heritage::graphics::GlbMetadataDocument& document,
    std::vector<Mat4>& worldMatrices,
    std::vector<unsigned char>& worldMatrixState,
    VehicleAssetRideHeightGeometryMetadata& output,
    VehicleAssetWheelGeometryMetadata& wheelOutput)
{
    output = {};
    wheelOutput = {};
    if (document.nodeGeometryBounds.size() != document.nodes.size())
        return;

    std::vector<WorldBounds> bodyBounds;
    std::vector<WorldBounds> tireBounds;
    bodyBounds.reserve(document.nodes.size());
    tireBounds.reserve(4);
    for (std::size_t index = 0; index < document.nodes.size(); ++index)
    {
        const WorldBounds bounds = transformedBounds(
            document.nodeGeometryBounds[index],
            worldNodeMatrix(document, static_cast<int>(index),
                worldMatrices, worldMatrixState));
        if (!bounds.valid)
            continue;
        if (tireGeometryNode(document.nodes[index]))
            tireBounds.push_back(bounds);
        else if (!wheelAssemblyNode(document.nodes[index]))
            bodyBounds.push_back(bounds);
    }
    if (tireBounds.empty())
        return;

    float groundSum = 0.0f;
    float frontTireCenterZ = -std::numeric_limits<float>::infinity();
    float rearTireCenterZ = std::numeric_limits<float>::infinity();
    for (const WorldBounds& bounds : tireBounds)
    {
        groundSum += bounds.minimum.y;
        const float centerZ = 0.5f * (bounds.minimum.z + bounds.maximum.z);
        frontTireCenterZ = std::max(frontTireCenterZ, centerZ);
        rearTireCenterZ = std::min(rearTireCenterZ, centerZ);
    }
    const float axleSplitZ = 0.5f * (frontTireCenterZ + rearTireCenterZ);
    float lateralSplitX = 0.0f;
    for (const WorldBounds& bounds : tireBounds)
        lateralSplitX += 0.5f * (bounds.minimum.x + bounds.maximum.x);
    lateralSplitX /= static_cast<float>(tireBounds.size());

    heritage::math::Vec3 cornerCenterSums[4]{};
    std::size_t cornerCenterCounts[4]{};
    for (const WorldBounds& bounds : tireBounds)
    {
        const heritage::math::Vec3 center{
            0.5f * (bounds.minimum.x + bounds.maximum.x),
            0.5f * (bounds.minimum.y + bounds.maximum.y),
            0.5f * (bounds.minimum.z + bounds.maximum.z)
        };
        const bool front = center.z >= axleSplitZ;
        const bool right = center.x >= lateralSplitX;
        const std::size_t corner = front
            ? (right ? 1u : 0u)
            : (right ? 3u : 2u);
        cornerCenterSums[corner].x += center.x;
        cornerCenterSums[corner].y += center.y;
        cornerCenterSums[corner].z += center.z;
        ++cornerCenterCounts[corner];
    }
    bool completeWheelGeometry = true;
    heritage::math::Vec3 cornerCenters[4]{};
    for (std::size_t corner = 0; corner < 4; ++corner)
    {
        if (cornerCenterCounts[corner] == 0)
        {
            completeWheelGeometry = false;
            continue;
        }
        const float inverseCount = 1.0f
            / static_cast<float>(cornerCenterCounts[corner]);
        cornerCenters[corner] = {
            cornerCenterSums[corner].x * inverseCount,
            cornerCenterSums[corner].y * inverseCount,
            cornerCenterSums[corner].z * inverseCount
        };
    }
    if (completeWheelGeometry)
    {
        wheelOutput.frontLeftCenter = cornerCenters[0];
        wheelOutput.frontRightCenter = cornerCenters[1];
        wheelOutput.rearLeftCenter = cornerCenters[2];
        wheelOutput.rearRightCenter = cornerCenters[3];
        wheelOutput.frontTrackM = std::abs(
            cornerCenters[1].x - cornerCenters[0].x);
        wheelOutput.rearTrackM = std::abs(
            cornerCenters[3].x - cornerCenters[2].x);
        const float frontCenterZ = 0.5f
            * (cornerCenters[0].z + cornerCenters[1].z);
        const float rearCenterZ = 0.5f
            * (cornerCenters[2].z + cornerCenters[3].z);
        wheelOutput.wheelbaseM = std::abs(frontCenterZ - rearCenterZ);
        wheelOutput.tireNodeCount = tireBounds.size();
        wheelOutput.valid = std::isfinite(wheelOutput.wheelbaseM)
            && std::isfinite(wheelOutput.frontTrackM)
            && std::isfinite(wheelOutput.rearTrackM)
            && wheelOutput.wheelbaseM > 0.0f
            && wheelOutput.frontTrackM > 0.0f
            && wheelOutput.rearTrackM > 0.0f;
    }

    if (bodyBounds.empty())
        return;
    output.referenceGroundPlaneLocalY =
        groundSum / static_cast<float>(tireBounds.size());
    output.axleSplitLocalZ = axleSplitZ;
    output.frontLowestBodyLocalY = std::numeric_limits<float>::infinity();
    output.rearLowestBodyLocalY = std::numeric_limits<float>::infinity();
    output.bodyMinimum = bodyBounds.front().minimum;
    output.bodyMaximum = bodyBounds.front().maximum;
    for (const WorldBounds& bounds : bodyBounds)
    {
        output.bodyMinimum.x = std::min(output.bodyMinimum.x, bounds.minimum.x);
        output.bodyMinimum.y = std::min(output.bodyMinimum.y, bounds.minimum.y);
        output.bodyMinimum.z = std::min(output.bodyMinimum.z, bounds.minimum.z);
        output.bodyMaximum.x = std::max(output.bodyMaximum.x, bounds.maximum.x);
        output.bodyMaximum.y = std::max(output.bodyMaximum.y, bounds.maximum.y);
        output.bodyMaximum.z = std::max(output.bodyMaximum.z, bounds.maximum.z);
        if (bounds.maximum.z >= output.axleSplitLocalZ)
        {
            output.frontLowestBodyLocalY = std::min(
                output.frontLowestBodyLocalY, bounds.minimum.y);
        }
        if (bounds.minimum.z <= output.axleSplitLocalZ)
        {
            output.rearLowestBodyLocalY = std::min(
                output.rearLowestBodyLocalY, bounds.minimum.y);
        }
    }
    if (!std::isfinite(output.frontLowestBodyLocalY)
        || !std::isfinite(output.rearLowestBodyLocalY))
    {
        return;
    }
    output.frontAuthoredClearanceM = output.frontLowestBodyLocalY
        - output.referenceGroundPlaneLocalY;
    output.rearAuthoredClearanceM = output.rearLowestBodyLocalY
        - output.referenceGroundPlaneLocalY;
    output.bodyNodeCount = bodyBounds.size();
    output.tireNodeCount = tireBounds.size();
    output.valid = std::isfinite(output.frontAuthoredClearanceM)
        && std::isfinite(output.rearAuthoredClearanceM);
}

bool validWheelDatumRole(const std::string& role)
{
    return role == "hub_face_center"
        || role == "wheel_centerline"
        || role == "wheel_spin_axis";
}

bool parseWheelDatumNodeName(
    const std::string& nodeName,
    std::string& corner,
    std::string& role)
{
    struct Prefix
    {
        const char* text;
        const char* corner;
    };
    constexpr Prefix prefixes[] = {
        { "FIT_FL_", "front_left" },
        { "FIT_FR_", "front_right" },
        { "FIT_RL_", "rear_left" },
        { "FIT_RR_", "rear_right" }
    };
    for (const Prefix& prefix : prefixes)
    {
        const std::size_t length = std::char_traits<char>::length(prefix.text);
        if (nodeName.size() <= length
            || nodeName.compare(0, length, prefix.text) != 0)
        {
            continue;
        }
        corner = prefix.corner;
        const std::string suffix = nodeName.substr(length);
        if (suffix == "HubFace")
            role = "hub_face_center";
        else if (suffix == "WheelCenterline")
            role = "wheel_centerline";
        else if (suffix == "SpinAxis")
            role = "wheel_spin_axis";
        else
            return false;
        return true;
    }
    return false;
}

bool parseHardpointNodeName(
    const std::string& nodeName,
    std::string& corner,
    std::string& id)
{
    struct Prefix
    {
        const char* text;
        const char* corner;
    };
    constexpr Prefix prefixes[] = {
        { "SUS_FL_", "front_left" },
        { "SUS_FR_", "front_right" },
        { "SUS_RL_", "rear_left" },
        { "SUS_RR_", "rear_right" }
    };
    for (const Prefix& prefix : prefixes)
    {
        const std::size_t length = std::char_traits<char>::length(prefix.text);
        if (nodeName.size() <= length
            || nodeName.compare(0, length, prefix.text) != 0)
        {
            continue;
        }
        corner = prefix.corner;
        id = nodeName.substr(length);
        return !id.empty();
    }
    return false;
}

bool closeEnough(double left, double right, double tolerance = 0.01)
{
    return std::abs(left - right) <= tolerance;
}

void addWarning(
    VehicleAssetMetadata& output,
    const std::string& message)
{
    if (std::find(output.warnings.begin(), output.warnings.end(), message)
        == output.warnings.end())
    {
        output.warnings.push_back(message);
    }
}

} // namespace

const VehicleAssetPartMetadata* VehicleAssetMetadata::findBySlot(
    const std::string& slot) const
{
    const auto iterator = std::find_if(
        parts.begin(), parts.end(),
        [&](const VehicleAssetPartMetadata& part) {
            return part.slot == slot;
        });
    return iterator == parts.end() ? nullptr : &*iterator;
}

const VehicleAssetWheelFitmentDatumMetadata*
VehicleAssetMetadata::findWheelFitmentDatum(
    const std::string& corner,
    const std::string& role) const
{
    const auto iterator = std::find_if(
        wheelFitmentDatums.begin(), wheelFitmentDatums.end(),
        [&](const VehicleAssetWheelFitmentDatumMetadata& datum) {
            return datum.corner == corner && datum.role == role;
        });
    return iterator == wheelFitmentDatums.end() ? nullptr : &*iterator;
}

const VehicleAssetSuspensionHardpointMetadata*
VehicleAssetMetadata::findSuspensionHardpoint(
    const std::string& corner,
    const std::string& id) const
{
    const auto iterator = std::find_if(
        suspensionHardpoints.begin(), suspensionHardpoints.end(),
        [&](const VehicleAssetSuspensionHardpointMetadata& hardpoint) {
            return hardpoint.corner == corner && hardpoint.id == id;
        });
    return iterator == suspensionHardpoints.end() ? nullptr : &*iterator;
}

std::string VehiclePartCompatibility::summary() const
{
    std::ostringstream stream;
    stream << (compatible ? "compatible" : "incompatible");
    if (!complete)
        stream << " (incomplete metadata)";
    for (const std::string& note : notes)
        stream << "; " << note;
    return stream.str();
}

bool inspectVehicleAssetMetadata(
    const std::filesystem::path& glbPath,
    VehicleAssetMetadata& output,
    std::string& errorMessage)
{
    output = {};
    output.sourcePath = glbPath.lexically_normal();

    heritage::graphics::GlbMetadataDocument document;
    if (!heritage::graphics::inspectGlbMetadata(
            glbPath, document, errorMessage))
    {
        return false;
    }

    std::unordered_map<std::string, int> slotOwners;
    std::unordered_map<std::string, int> wheelDatumOwners;
    std::unordered_map<std::string, int> hardpointOwners;
    std::vector<Mat4> worldMatrices(document.nodes.size());
    std::vector<unsigned char> worldMatrixState(document.nodes.size(), 0);
    inspectRideHeightGeometry(
        document, worldMatrices, worldMatrixState,
        output.rideHeightGeometry, output.wheelGeometry);
    for (std::size_t nodeIndex = 0; nodeIndex < document.nodes.size(); ++nodeIndex)
    {
        const auto& node = document.nodes[nodeIndex];
        const auto& properties = node.metadata;

        std::string datumCorner = stringProperty(
            properties, "heritage.corner");
        std::string datumRole = stringProperty(
            properties, "heritage.datum_role");
        const bool explicitWheelDatum =
            stringProperty(properties, "heritage.part_type")
                == "wheel_fitment_datum"
            || !datumRole.empty();
        const bool namedWheelDatum = parseWheelDatumNodeName(
            node.name, datumCorner, datumRole);
        if (explicitWheelDatum || namedWheelDatum)
        {
            if (datumCorner.empty() || !validWheelDatumRole(datumRole))
            {
                addWarning(
                    output,
                    node.name
                        + ": wheel fitment datum needs a corner and one of hub_face_center / wheel_centerline / wheel_spin_axis.");
            }
            else
            {
                const std::string ownerKey = datumCorner + ":" + datumRole;
                const auto [iterator, inserted] = wheelDatumOwners.emplace(
                    ownerKey, static_cast<int>(nodeIndex));
                if (!inserted)
                {
                    addWarning(
                        output,
                        "Duplicate wheel fitment datum '" + ownerKey
                            + "' on GLB nodes "
                            + std::to_string(iterator->second) + " and "
                            + std::to_string(nodeIndex) + ".");
                }
                else
                {
                    const Mat4 world = worldNodeMatrix(
                        document, static_cast<int>(nodeIndex),
                        worldMatrices, worldMatrixState);
                    VehicleAssetWheelFitmentDatumMetadata datum;
                    datum.nodeIndex = static_cast<int>(nodeIndex);
                    datum.nodeName = node.name;
                    datum.corner = datumCorner;
                    datum.role = datumRole;
                    datum.localPosition = matrixTranslation(world);
                    datum.localAxis = matrixXAxis(world);
                    datum.provenance = stringProperty(
                        properties, "heritage.provenance");
                    if (datum.provenance.empty())
                        datum.provenance = "asset_authored";
                    datum.confidence = static_cast<float>(std::clamp(
                        numberProperty(properties, "heritage.confidence", 0.75),
                        0.0, 1.0));
                    output.wheelFitmentDatums.push_back(std::move(datum));
                }
            }
            // Engineering datums are metadata inputs, not replaceable render parts.
            continue;
        }

        std::string hardpointCorner = stringProperty(
            properties, "heritage.corner");
        std::string hardpointId = stringProperty(
            properties, "heritage.hardpoint_id");
        const bool explicitHardpoint =
            stringProperty(properties, "heritage.part_type")
                == "suspension_hardpoint"
            || !hardpointId.empty();
        const bool namedHardpoint = parseHardpointNodeName(
            node.name, hardpointCorner, hardpointId);
        if (explicitHardpoint || namedHardpoint)
        {
            if (hardpointCorner.empty() || hardpointId.empty())
            {
                addWarning(
                    output,
                    node.name + ": suspension hardpoint needs both corner and hardpoint ID.");
            }
            else
            {
                const std::string ownerKey = hardpointCorner + ":" + hardpointId;
                const auto [iterator, inserted] = hardpointOwners.emplace(
                    ownerKey, static_cast<int>(nodeIndex));
                if (!inserted)
                {
                    addWarning(
                        output,
                        "Duplicate suspension hardpoint '" + ownerKey
                            + "' on GLB nodes "
                            + std::to_string(iterator->second) + " and "
                            + std::to_string(nodeIndex) + ".");
                }
                else
                {
                    VehicleAssetSuspensionHardpointMetadata hardpoint;
                    hardpoint.nodeIndex = static_cast<int>(nodeIndex);
                    hardpoint.nodeName = node.name;
                    hardpoint.corner = hardpointCorner;
                    hardpoint.id = hardpointId;
                    hardpoint.localPosition = matrixTranslation(worldNodeMatrix(
                        document, static_cast<int>(nodeIndex),
                        worldMatrices, worldMatrixState));
                    hardpoint.provenance = stringProperty(
                        properties, "heritage.provenance");
                    if (hardpoint.provenance.empty())
                        hardpoint.provenance = "asset_authored";
                    hardpoint.confidence = static_cast<float>(std::clamp(
                        numberProperty(properties, "heritage.confidence", 0.75),
                        0.0, 1.0));
                    output.suspensionHardpoints.push_back(std::move(hardpoint));
                }
            }
            // Suspension anchors are metadata inputs rather than replaceable
            // render parts, even when they also carry Heritage extras.
            continue;
        }

        const bool semantic =
            properties.find("heritage.part_type") != properties.end()
            || properties.find("heritage.role") != properties.end()
            || properties.find("heritage.slot") != properties.end()
            || properties.find("heritage.part_id") != properties.end();
        if (!semantic)
            continue;

        VehicleAssetPartMetadata part;
        part.nodeIndex = static_cast<int>(nodeIndex);
        part.parentNodeIndex = node.parentIndex;
        part.nodeName = node.name;
        part.slot = stringProperty(properties, "heritage.slot");
        part.role = stringProperty(properties, "heritage.role");
        part.partType = stringProperty(properties, "heritage.part_type");
        part.partId = stringProperty(properties, "heritage.part_id");
        part.corner = stringProperty(properties, "heritage.corner");
        part.replaceable = boolProperty(properties, "heritage.replaceable", false);
        part.rotatesWithWheel = boolProperty(
            properties, "heritage.rotates_with_wheel", false);
        part.properties = properties;

        if (part.slot.empty())
            part.slot = part.nodeName;

        if (!part.slot.empty())
        {
            const auto [iterator, inserted] = slotOwners.emplace(
                part.slot, part.nodeIndex);
            if (!inserted)
            {
                addWarning(
                    output,
                    "Duplicate Heritage slot '" + part.slot
                    + "' on GLB nodes " + std::to_string(iterator->second)
                    + " and " + std::to_string(part.nodeIndex) + ".");
            }
        }

        if (part.partType == "wheel")
        {
            if (numberProperty(properties, "wheel.diameter_in") <= 0.0)
                addWarning(output, part.slot + ": wheel.diameter_in is missing.");
            if (numberProperty(properties, "wheel.width_in") <= 0.0)
                addWarning(output, part.slot + ": wheel.width_in is missing.");
            if (numberProperty(properties, "wheel.pcd_mm") <= 0.0)
                addWarning(output, part.slot + ": wheel.pcd_mm is missing.");
            if (numberProperty(properties, "wheel.bolt_count") <= 0.0)
                addWarning(output, part.slot + ": wheel.bolt_count is missing.");
        }
        else if (part.partType == "tire")
        {
            if (numberProperty(properties, "tire.width_mm") <= 0.0)
                addWarning(output, part.slot + ": tire.width_mm is missing.");
            if (numberProperty(properties, "tire.aspect_ratio") <= 0.0)
                addWarning(output, part.slot + ": tire.aspect_ratio is missing.");
            if (numberProperty(properties, "tire.rim_diameter_in") <= 0.0)
                addWarning(output, part.slot + ": tire.rim_diameter_in is missing.");

            if (part.parentNodeIndex >= 0
                && static_cast<std::size_t>(part.parentNodeIndex) < document.nodes.size())
            {
                const auto& parent = document.nodes[static_cast<std::size_t>(part.parentNodeIndex)];
                const std::string parentType = stringProperty(
                    parent.metadata, "heritage.part_type");
                if (parentType != "wheel")
                {
                    addWarning(
                        output,
                        part.slot + ": tire is not parented directly to a Heritage wheel part.");
                }
            }
        }

        output.parts.push_back(std::move(part));
    }

    if (output.parts.empty() && output.suspensionHardpoints.empty())
    {
        addWarning(
            output,
            "No Heritage semantic node metadata was found. In Blender, enable glTF Custom Properties/extras when exporting the GLB.");
    }

    std::cout
        << "[VehicleAssetMetadata] " << kVehicleAssetMetadataMarker
        << " asset=" << glbPath.filename().string()
        << " parts=" << output.parts.size()
        << " suspension_hardpoints=" << output.suspensionHardpoints.size()
        << " ride_height_geometry="
        << (output.rideHeightGeometry.valid ? "yes" : "no")
        << " wheel_geometry="
        << (output.wheelGeometry.valid ? "yes" : "no")
        << " warnings=" << output.warnings.size() << '\n';

    errorMessage.clear();
    return true;
}

VehiclePartCompatibility checkTireWheelCompatibility(
    const VehicleAssetPartMetadata& wheel,
    const VehicleAssetPartMetadata& tire)
{
    VehiclePartCompatibility result;
    if (wheel.partType != "wheel")
    {
        result.notes.push_back("selected wheel slot is not heritage.part_type=wheel");
        return result;
    }
    if (tire.partType != "tire")
    {
        result.notes.push_back("selected tire slot is not heritage.part_type=tire");
        return result;
    }

    result.compatible = true;
    result.complete = true;

    const double wheelDiameter = numberProperty(wheel.properties, "wheel.diameter_in");
    const double tireDiameter = numberProperty(tire.properties, "tire.rim_diameter_in");
    if (wheelDiameter <= 0.0 || tireDiameter <= 0.0)
    {
        result.complete = false;
        result.notes.push_back("rim diameter metadata is incomplete");
    }
    else if (!closeEnough(wheelDiameter, tireDiameter))
    {
        result.compatible = false;
        result.notes.push_back(
            "diameter mismatch: wheel " + std::to_string(wheelDiameter)
            + " in vs tire " + std::to_string(tireDiameter) + " in");
    }
    else
    {
        result.notes.push_back("rim diameter matches");
    }

    const double wheelWidth = numberProperty(wheel.properties, "wheel.width_in");
    const double minimumWidth = numberProperty(
        tire.properties, "tire.rim_width_min_in");
    const double maximumWidth = numberProperty(
        tire.properties, "tire.rim_width_max_in");
    if (minimumWidth > 0.0 && maximumWidth > 0.0 && wheelWidth > 0.0)
    {
        if (wheelWidth + 0.001 < minimumWidth || wheelWidth - 0.001 > maximumWidth)
        {
            result.compatible = false;
            result.notes.push_back("wheel width is outside the tire's authored rim-width range");
        }
        else
        {
            result.notes.push_back("wheel width is inside the tire's authored rim-width range");
        }
    }
    else
    {
        result.complete = false;
        result.notes.push_back("tire rim-width min/max metadata is not authored yet");
    }

    return result;
}

double tireNominalDiameterMeters(const VehicleAssetPartMetadata& tire)
{
    if (tire.partType != "tire")
        return 0.0;

    const double widthMm = numberProperty(tire.properties, "tire.width_mm");
    const double aspect = numberProperty(tire.properties, "tire.aspect_ratio");
    const double rimInches = numberProperty(tire.properties, "tire.rim_diameter_in");
    if (widthMm <= 0.0 || aspect <= 0.0 || rimInches <= 0.0)
        return 0.0;

    const double sidewallMm = widthMm * (aspect / 100.0);
    const double rimMm = rimInches * 25.4;
    return (rimMm + sidewallMm * 2.0) / 1000.0;
}

} // namespace heritage::vehicles
