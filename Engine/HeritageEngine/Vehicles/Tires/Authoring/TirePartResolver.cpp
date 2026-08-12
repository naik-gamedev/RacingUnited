#include "TirePartResolver.hpp"

#include "../MagicFormula/TirePropertyFile.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

bool validOperationalPressure(VehicleScalar pressurePa)
{
    return std::isfinite(static_cast<double>(pressurePa))
        && pressurePa >= 20000.0
        && pressurePa <= 2000000.0;
}

TireFamilyBaselineInput baselineInputFromPart(const TirePartDefinition& definition)
{
    TireFamilyBaselineInput input;
    input.sectionWidthM = definition.engineering.sectionWidthM;
    input.aspectRatio = definition.engineering.aspectRatio;
    input.rimRadiusM = definition.engineering.rimRadiusM;
    input.nominalLoadN = definition.engineering.nominalLoadN;
    input.inflationPressurePa = definition.engineering.referenceInflationPressurePa;
    return input;
}

void applyFitmentPressure(
    heritage::vehicles::TireModelDescription& model,
    const TirePartFitment& fitment,
    bool& applied)
{
    if (fitment.coldInflationPressurePa <= 0.0)
        return;

    model.inflationPressurePa = fitment.coldInflationPressurePa;
    model.thermal.referenceGaugePressurePa = fitment.coldInflationPressurePa;
    applied = true;
}

} // namespace

bool validTirePartFitment(const TirePartFitment& fitment)
{
    return fitment.coldInflationPressurePa == 0.0
        || validOperationalPressure(fitment.coldInflationPressurePa);
}

TirePartResolutionResult resolveTirePart(
    const TirePartDefinition& definition,
    const std::filesystem::path& propertyRoot,
    const TirePartFitment& fitment)
{
    TirePartResolutionResult result;
    if (!validTirePartDefinition(definition))
    {
        result.errorMessage = "Tire part definition is invalid.";
        return result;
    }
    if (!validTirePartFitment(fitment))
    {
        result.errorMessage = "Tire fitment cold pressure is outside supported bounds.";
        return result;
    }

    const TireFamilyBaselineInput input = baselineInputFromPart(definition);
    const TireFamilyBaselineResult neutralBaseline = buildTireFamilyBaseline(
        definition.family,
        input);
    if (!neutralBaseline.valid)
    {
        result.errorMessage = neutralBaseline.errorMessage;
        return result;
    }

    if (!definition.propertyFile.empty())
    {
        std::filesystem::path propertyPath(definition.propertyFile);
        if (propertyPath.is_relative() && !propertyRoot.empty())
            propertyPath = propertyRoot / propertyPath;
        propertyPath = propertyPath.lexically_normal();

        const TirePropertyFileLoadResult loaded = loadTirePropertyFile(propertyPath);
        if (!loaded.success)
        {
            result.errorMessage = "Tire part property file failed to load: " + loaded.errorMessage;
            return result;
        }

        const std::string provenance = definition.propertyProvenance.empty()
            ? std::string("tire_part_property_file")
            : definition.propertyProvenance;
        const VehicleScalar confidence = definition.propertyConfidence > 0.0
            ? definition.propertyConfidence
            : VehicleScalar{0.50};

        result.model = tireModelDescriptionFromPropertyFile(
            loaded.data,
            neutralBaseline.model.provider,
            propertyPath.string(),
            provenance,
            confidence,
            neutralBaseline.model);
        result.source = TirePartResolutionSource::AuthoritativePropertyFile;
        result.resolvedPropertyFile = propertyPath;
    }
    else
    {
        const TireFamilyBaselineResult generated = buildTireFamilyBaseline(
            definition.family,
            input,
            definition.performanceBias);
        if (!generated.valid)
        {
            result.errorMessage = generated.errorMessage;
            return result;
        }
        result.model = generated.model;
        result.model.parameterSource = definition.displayName;
        result.model.parameterProvenance = "heritage_estimated_tire_part";
        result.model.parameterConfidence = std::min(
            result.model.parameterConfidence,
            VehicleScalar{0.25});
        result.source = TirePartResolutionSource::EstimatedFamilyBaseline;
    }

    applyFitmentPressure(result.model, fitment, result.fitmentPressureApplied);
    if (!heritage::vehicles::validTireModelDescription(result.model))
    {
        result.errorMessage = "Resolved tire part produced an invalid runtime tire model.";
        result.source = TirePartResolutionSource::None;
        return result;
    }

    result.valid = true;
    return result;
}

} // namespace heritage::vehicles::tires
