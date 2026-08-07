#include "VehicleDefinitionCompiler.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace heritage::vehicles {
namespace {

bool finite(float value)
{
    return std::isfinite(value);
}

bool finite(const heritage::math::Vec3& value)
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool safeId(const std::string& value)
{
    if (value.empty() || value.size() > 64)
        return false;

    return std::all_of(
        value.begin(),
        value.end(),
        [](unsigned char character) {
            return (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9')
                || character == '_'
                || character == '-';
        });
}

void addIssue(
    VehicleDefinitionCompileResult& result,
    VehicleDefinitionIssueSeverity severity,
    const std::string& code,
    const std::string& message)
{
    result.issues.push_back({ severity, code, message });
}

void addError(
    VehicleDefinitionCompileResult& result,
    const std::string& code,
    const std::string& message)
{
    addIssue(result, VehicleDefinitionIssueSeverity::Error, code, message);
}

void addWarning(
    VehicleDefinitionCompileResult& result,
    const std::string& code,
    const std::string& message)
{
    addIssue(result, VehicleDefinitionIssueSeverity::Warning, code, message);
}

template<typename Component>
std::unordered_map<std::string, std::size_t> indexComponents(
    const std::vector<Component>& components,
    const char* label,
    VehicleDefinitionCompileResult& result)
{
    std::unordered_map<std::string, std::size_t> indices;
    for (std::size_t index = 0; index < components.size(); ++index)
    {
        const std::string& id = components[index].id;
        if (!safeId(id))
        {
            addError(
                result,
                "unsafe_component_id",
                std::string(label) + " at index " + std::to_string(index + 1)
                    + " needs a lowercase identifier of at most 64 characters.");
            continue;
        }

        if (!indices.emplace(id, index).second)
        {
            addError(
                result,
                "duplicate_component_id",
                std::string(label) + " repeats ID '" + id + "'.");
        }
    }
    return indices;
}

template<typename Map>
std::size_t resolveReference(
    const Map& indices,
    const std::string& id,
    const std::string& owner,
    const char* relationship,
    VehicleDefinitionCompileResult& result)
{
    const auto found = indices.find(id);
    if (found != indices.end())
        return found->second;

    addError(
        result,
        "missing_component_reference",
        owner + " references missing " + relationship + " '" + id + "'.");
    return kInvalidVehicleComponentIndex;
}

void addReason(std::vector<std::string>& reasons, const std::string& reason)
{
    if (std::find(reasons.begin(), reasons.end(), reason) == reasons.end())
        reasons.push_back(reason);
}

std::string join(const std::vector<std::string>& values, const char* separator)
{
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index > 0)
            output << separator;
        output << values[index];
    }
    return output.str();
}

} // namespace

std::string VehicleDefinitionCompileResult::issueSummary() const
{
    std::ostringstream output;
    for (std::size_t index = 0; index < issues.size(); ++index)
    {
        if (index > 0)
            output << '\n';
        output
            << (issues[index].severity == VehicleDefinitionIssueSeverity::Error
                ? "ERROR" : "WARNING")
            << " [" << issues[index].code << "] "
            << issues[index].message;
    }
    return output.str();
}

VehicleDefinitionCompileResult VehicleDefinitionCompiler::compile(
    const VehicleDefinitionV2Source& source)
{
    VehicleDefinitionCompileResult result;
    result.definition.schemaVersion = source.schemaVersion;
    result.definition.id = source.id;
    result.definition.displayName = source.displayName;
    result.definition.classification = source.classification;
    result.definition.bodyAsset = source.bodyAsset;
    result.definition.requirements = source.requirements;
    result.definition.bodies = source.bodies;

    if (source.schemaVersion != kVehicleDefinitionSchemaVersion)
        addError(result, "schema_version", "schemaVersion must be 2.");
    if (!safeId(source.id))
    {
        addError(
            result,
            "unsafe_definition_id",
            "Definition ID must use lowercase letters, numbers, underscores or hyphens.");
    }
    if (source.displayName.empty() || source.displayName.size() > 160)
    {
        addError(
            result,
            "display_name",
            "Display name must contain 1 to 160 characters.");
    }

    if (source.bodies.empty() || source.bodies.size() > 16)
        addError(result, "body_count", "A vehicle requires 1 to 16 bodies.");
    if (source.powerUnits.size() > 8)
        addError(result, "power_unit_count", "At most 8 power units are allowed.");
    if (source.transmissions.size() > 8)
        addError(result, "transmission_count", "At most 8 transmissions are allowed.");
    if (source.contactUnits.empty() || source.contactUnits.size() > 32)
        addError(result, "contact_count", "A vehicle requires 1 to 32 contact units.");
    if (source.driveConnections.size() > 16)
        addError(result, "drive_connection_count", "At most 16 drive connections are allowed.");

    const auto bodyIndices = indexComponents(source.bodies, "Body", result);
    const auto powerIndices = indexComponents(source.powerUnits, "Power unit", result);
    const auto transmissionIndices = indexComponents(
        source.transmissions, "Transmission", result);
    const auto contactIndices = indexComponents(
        source.contactUnits, "Contact unit", result);
    indexComponents(source.driveConnections, "Drive connection", result);

    std::size_t primaryBodyCount = 0;
    for (const VehicleBodyDefinition& body : source.bodies)
    {
        if (body.role == "primary")
            ++primaryBodyCount;
        if (!finite(body.massKg) || body.massKg <= 0.0f || body.massKg > 1000000.0f)
        {
            addError(
                result,
                "body_mass",
                "Body '" + body.id + "' needs a finite positive mass.");
        }
    }
    if (primaryBodyCount != 1)
    {
        addError(
            result,
            "primary_body_count",
            "A definition must identify exactly one primary body.");
    }

    for (const VehiclePowerUnitDefinition& power : source.powerUnits)
    {
        CompiledVehiclePowerUnit compiled;
        compiled.authored = power;
        compiled.mountBodyIndex = resolveReference(
            bodyIndices,
            power.mountBody,
            "Power unit '" + power.id + "'",
            "body",
            result);
        result.definition.powerUnits.push_back(std::move(compiled));

        if (!finite(power.maximumTorqueNm) || power.maximumTorqueNm < 0.0f
            || !finite(power.idleRpm) || power.idleRpm < 0.0f
            || !finite(power.redlineRpm) || power.redlineRpm <= power.idleRpm
            || !finite(power.engineBrakingTorqueNm)
            || power.engineBrakingTorqueNm < 0.0f)
        {
            addError(
                result,
                "power_unit_parameters",
                "Power unit '" + power.id + "' has invalid torque or speed limits.");
        }
    }

    for (const VehicleTransmissionDefinition& transmission : source.transmissions)
    {
        CompiledVehicleTransmission compiled;
        compiled.authored = transmission;
        compiled.powerUnitIndex = resolveReference(
            powerIndices,
            transmission.powerUnit,
            "Transmission '" + transmission.id + "'",
            "power unit",
            result);
        result.definition.transmissions.push_back(std::move(compiled));

        if (transmission.forwardRatios.size() > 32)
        {
            addError(
                result,
                "gear_count",
                "Transmission '" + transmission.id
                    + "' exceeds 32 forward ratios.");
        }
        for (float ratio : transmission.forwardRatios)
        {
            if (!finite(ratio) || ratio <= 0.0f)
            {
                addError(
                    result,
                    "forward_ratio",
                    "Transmission '" + transmission.id
                        + "' contains a non-positive forward ratio.");
                break;
            }
        }
        if (!finite(transmission.reverseRatio) || transmission.reverseRatio >= 0.0f
            || !finite(transmission.finalDriveRatio)
            || transmission.finalDriveRatio <= 0.0f
            || !finite(transmission.efficiency)
            || transmission.efficiency <= 0.0f
            || transmission.efficiency > 1.0f
            || !finite(transmission.shiftDurationSeconds)
            || transmission.shiftDurationSeconds < 0.0f
            || !finite(transmission.clutchEngagementRate)
            || transmission.clutchEngagementRate <= 0.0f)
        {
            addError(
                result,
                "transmission_parameters",
                "Transmission '" + transmission.id + "' has invalid runtime parameters.");
        }
    }

    for (const VehicleContactUnitDefinition& contact : source.contactUnits)
    {
        CompiledVehicleContactUnit compiled;
        compiled.authored = contact;
        compiled.mountBodyIndex = resolveReference(
            bodyIndices,
            contact.mountBody,
            "Contact unit '" + contact.id + "'",
            "body",
            result);
        result.definition.contactUnits.push_back(std::move(compiled));

        if (!finite(contact.localMount) || !finite(contact.suspensionDirection)
            || !finite(contact.radiusM) || contact.radiusM <= 0.0f
            || !finite(contact.restLengthM) || contact.restLengthM <= 0.0f
            || !finite(contact.maximumCompressionM) || contact.maximumCompressionM < 0.0f
            || !finite(contact.maximumDroopM) || contact.maximumDroopM < 0.0f
            || !finite(contact.springRateNPerM) || contact.springRateNPerM <= 0.0f
            || !finite(contact.bumpDampingNsPerM) || contact.bumpDampingNsPerM < 0.0f
            || !finite(contact.reboundDampingNsPerM) || contact.reboundDampingNsPerM < 0.0f
            || !finite(contact.serviceBrakeFactor) || contact.serviceBrakeFactor < 0.0f
            || !finite(contact.parkingBrakeFactor) || contact.parkingBrakeFactor < 0.0f)
        {
            addError(
                result,
                "contact_parameters",
                "Contact unit '" + contact.id + "' has invalid wheel or suspension data.");
        }
    }

    std::vector<std::size_t> driveReferenceCounts(source.contactUnits.size(), 0);
    for (const VehicleDriveConnectionDefinition& connection : source.driveConnections)
    {
        CompiledVehicleDriveConnection compiled;
        compiled.id = connection.id;
        compiled.transmissionIndex = resolveReference(
            transmissionIndices,
            connection.transmission,
            "Drive connection '" + connection.id + "'",
            "transmission",
            result);

        std::unordered_set<std::size_t> uniqueContacts;
        for (const std::string& contactId : connection.contactUnits)
        {
            const std::size_t contactIndex = resolveReference(
                contactIndices,
                contactId,
                "Drive connection '" + connection.id + "'",
                "contact unit",
                result);
            if (contactIndex == kInvalidVehicleComponentIndex)
                continue;
            if (!uniqueContacts.insert(contactIndex).second)
            {
                addError(
                    result,
                    "duplicate_drive_target",
                    "Drive connection '" + connection.id
                        + "' repeats contact unit '" + contactId + "'.");
                continue;
            }
            compiled.contactUnitIndices.push_back(contactIndex);
            ++driveReferenceCounts[contactIndex];
        }
        result.definition.driveConnections.push_back(std::move(compiled));
    }

    for (const CompiledVehicleDriveConnection& connection
        : result.definition.driveConnections)
    {
        if (connection.contactUnitIndices.empty())
            continue;
        const float factor = 1.0f
            / static_cast<float>(connection.contactUnitIndices.size());
        for (std::size_t contactIndex : connection.contactUnitIndices)
            result.definition.contactUnits[contactIndex].driveFactor += factor;
    }

    if (source.powerUnits.empty())
        addWarning(result, "unpowered", "Definition is an unpowered vehicle or trailer.");
    else if (source.driveConnections.empty())
        addWarning(result, "undriven", "No drive connection reaches a contact unit.");
    if (!source.powerUnits.empty())
    {
        addWarning(
            result,
            "placement_metadata_only",
            "Power-unit placement is retained but does not yet derive mass distribution.");
    }

    std::vector<std::string> providerReasons;
    if (source.bodies.size() != 1)
        addReason(providerReasons, "one rigid body");
    if (source.powerUnits.size() != 1)
        addReason(providerReasons, "one power unit");
    if (source.transmissions.size() != 1)
        addReason(providerReasons, "one transmission");
    if (source.contactUnits.size() != 4)
        addReason(providerReasons, "four wheel contacts");
    if (source.driveConnections.size() != 1)
        addReason(providerReasons, "one drivetrain route");
    if (source.requirements.leanDynamics)
        addReason(providerReasons, "lean_dynamics provider");
    if (source.requirements.articulation)
        addReason(providerReasons, "articulation provider");
    if (source.requirements.trackContacts)
        addReason(providerReasons, "continuous_track provider");

    for (const VehiclePowerUnitDefinition& power : source.powerUnits)
    {
        if (power.kind != "combustion")
            addReason(providerReasons, "power unit provider '" + power.kind + "'");
    }
    for (const VehicleTransmissionDefinition& transmission : source.transmissions)
    {
        if (transmission.kind != "manual" && transmission.kind != "direct")
            addReason(providerReasons, "transmission provider '" + transmission.kind + "'");
        if (transmission.forwardRatios.empty()
            || transmission.forwardRatios.size() > 16)
        {
            addReason(providerReasons, "1 to 16 native forward ratios");
        }
    }
    for (const VehicleContactUnitDefinition& contact : source.contactUnits)
    {
        if (contact.kind != "wheel")
            addReason(providerReasons, "contact provider '" + contact.kind + "'");
        if (contact.suspensionProvider != "raycast_linear")
        {
            addReason(
                providerReasons,
                "suspension provider '" + contact.suspensionProvider + "'");
        }
        if (contact.tireProvider != "advanced_road")
            addReason(providerReasons, "tire provider '" + contact.tireProvider + "'");
    }
    if (!source.contactUnits.empty()
        && std::all_of(
            driveReferenceCounts.begin(),
            driveReferenceCounts.end(),
            [](std::size_t count) { return count == 0; }))
    {
        addReason(providerReasons, "at least one driven contact");
    }

    result.valid = std::none_of(
        result.issues.begin(),
        result.issues.end(),
        [](const VehicleDefinitionIssue& issue) {
            return issue.severity == VehicleDefinitionIssueSeverity::Error;
        });
    result.currentSolverReady = result.valid && providerReasons.empty();
    result.definition.runtimeProvider = result.currentSolverReady
        ? "raycast_wheel_v1"
        : "unresolved";

    if (result.valid && !result.currentSolverReady)
    {
        addWarning(
            result,
            "future_runtime_providers",
            "Definition is valid but awaits: " + join(providerReasons, ", ") + ".");
    }

    std::ostringstream summary;
    summary
        << "schema v" << source.schemaVersion
        << " | " << source.bodies.size() << " bodies"
        << " | " << source.powerUnits.size() << " power units"
        << " | " << source.transmissions.size() << " transmissions"
        << " | " << source.contactUnits.size() << " contacts"
        << " | provider " << result.definition.runtimeProvider;
    result.summary = summary.str();
    return result;
}

} // namespace heritage::vehicles
