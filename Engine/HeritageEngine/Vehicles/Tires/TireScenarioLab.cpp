#include "TireScenarioLab.hpp"

#include "TireSlipDynamics.hpp"
#include "TireThermal.hpp"
#include "TireWear.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <system_error>

namespace heritage::vehicles {
namespace {

constexpr VehicleScalar kPi = 3.14159265358979323846;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

void assignError(std::string* error, const std::string& value)
{
    if (error)
        *error = value;
}

TireContactInput contactInput(
    const TireModelDescription& tire,
    VehicleScalar wheelRadiusM,
    VehicleScalar speedMps,
    VehicleScalar normalLoadN,
    VehicleScalar longitudinalSlip,
    VehicleScalar slipAngleRadians,
    VehicleScalar pressurePa,
    VehicleScalar frictionScale = 1.0,
    VehicleScalar stiffnessScale = 1.0)
{
    TireContactInput input;
    input.normalLoad = normalLoadN;
    input.longitudinalSlip = longitudinalSlip;
    input.slipAngleRadians = slipAngleRadians;
    input.forwardSpeedMps = speedMps;
    input.wheelRadiusM = wheelRadiusM;
    input.inflationPressurePa = pressurePa;
    input.frictionMultiplier = frictionScale;
    input.stiffnessMultiplier = stiffnessScale;
    return input;
}

TireScenarioSample commonSample(
    VehicleScalar timeSeconds,
    VehicleScalar targetLongitudinalSlip,
    VehicleScalar effectiveLongitudinalSlip,
    VehicleScalar targetSlipAngleRadians,
    VehicleScalar effectiveSlipAngleRadians,
    const TireContactInput& input,
    const TireForceResult& force,
    const tires::TireThermalOutput& thermal,
    const tires::TireWearOutput& wear,
    const tires::TireFailureOutput& failure)
{
    TireScenarioSample sample;
    sample.timeSeconds = timeSeconds;
    sample.targetLongitudinalSlip = targetLongitudinalSlip;
    sample.effectiveLongitudinalSlip = effectiveLongitudinalSlip;
    sample.targetSlipAngleRadians = targetSlipAngleRadians;
    sample.effectiveSlipAngleRadians = effectiveSlipAngleRadians;
    sample.forwardSpeedMps = input.forwardSpeedMps;
    sample.normalLoadN = input.normalLoad;
    sample.force = force;
    if (thermal.valid)
    {
        sample.treadTemperatureC = thermal.treadTemperatureC;
        sample.carcassTemperatureC = thermal.carcassTemperatureC;
        sample.gasTemperatureC = thermal.gasTemperatureC;
        sample.rimTemperatureC = thermal.rimTemperatureC;
        sample.inflationPressurePa = thermal.inflationPressurePa;
    }
    else
    {
        sample.inflationPressurePa = input.inflationPressurePa;
    }
    if (wear.valid)
    {
        sample.averageTreadDepthM = wear.averageTreadDepthM;
        sample.minimumTreadDepthM = wear.minimumTreadDepthM;
        sample.flatSpotDepthM = wear.flatSpotDepthM;
        sample.wearFraction = wear.wearFraction;
    }
    if (failure.valid)
    {
        sample.failureStage = failure.stage;
        sample.containedGasMassRatio = failure.containedGasMassRatio;
        sample.structuralIntegrity = failure.structuralIntegrity;
        sample.treadAttachment = failure.treadAttachment;
        sample.rimContactFraction = failure.rimContactFraction;
    }
    return sample;
}

TireScenarioResult relaxationStep(
    const TireModelDescription& tire,
    VehicleScalar wheelRadiusM)
{
    TireScenarioResult result;
    result.name = "relaxation_step";
    result.integrationStepSeconds = 0.001;
    result.sampleIntervalSeconds = 0.01;

    tires::TireSlipDynamicsDescription description;
    description.longitudinalRelaxationLengthM =
        tire.longitudinalRelaxationLength;
    description.lateralRelaxationLengthM = tire.lateralRelaxationLength;
    tires::TireSlipDynamicsState state;
    const VehicleScalar speedMps = 25.0;
    const VehicleScalar targetLongitudinalSlip = 0.12;
    const VehicleScalar targetSlipAngle = 6.0 * kPi / 180.0;
    const int steps = 3000;
    const int sampleStride = 10;
    for (int step = 0; step <= steps; ++step)
    {
        const VehicleScalar time = step * result.integrationStepSeconds;
        const VehicleScalar targetKappa = time >= 0.50
            ? targetLongitudinalSlip : 0.0;
        const VehicleScalar targetAlpha = time >= 1.50
            ? targetSlipAngle : 0.0;
        const VehicleScalar longitudinalLength =
            tires::magicFormulaLongitudinalRelaxationLengthM(
                tire.slipDynamicsCoefficients,
                tire.nominalLoad,
                tire.nominalLoad,
                wheelRadiusM,
                tire.longitudinalRelaxationLength);
        const VehicleScalar lateralLength =
            tires::magicFormulaLateralRelaxationLengthM(
                tire.slipDynamicsCoefficients,
                tire.nominalLoad,
                tire.nominalLoad,
                wheelRadiusM,
                0.0,
                tire.magicFormula.pKy6,
                tire.lateralRelaxationLength);
        description.longitudinalRelaxationLengthM = longitudinalLength;
        description.lateralRelaxationLengthM = lateralLength;
        if (step > 0)
        {
            tires::integrateTireSlipDynamics(
                description,
                targetKappa,
                targetAlpha,
                speedMps,
                result.integrationStepSeconds,
                state);
        }
        if (step % sampleStride == 0)
        {
            const TireContactInput input = contactInput(
                tire, wheelRadiusM, speedMps, tire.nominalLoad,
                state.longitudinalSlip, state.slipAngleRadians,
                tire.inflationPressurePa);
            result.samples.push_back(commonSample(
                time, targetKappa, state.longitudinalSlip,
                targetAlpha, state.slipAngleRadians,
                input, evaluateAdvancedRoadTire(tire, input), {}, {}, {}));
        }
    }
    result.valid = !result.samples.empty();
    return result;
}

TireScenarioResult thermalWearScenario(
    const TireModelDescription& tire,
    VehicleScalar wheelRadiusM,
    const std::string& name)
{
    TireScenarioResult result;
    result.name = name;
    if (!tire.thermal.enabled || !tire.wear.enabled)
    {
        result.error = "The fitted tire does not enable both thermal and wear providers.";
        return result;
    }

    const bool flatSpot = name == "braking_flat_spot";
    const bool heatingCooling = name == "heating_cooling";
    const bool brakeRimSoak = name == "brake_rim_soak";
    result.integrationStepSeconds = flatSpot ? 0.001 : 0.01;
    result.sampleIntervalSeconds = flatSpot ? 0.02 : 0.10;
    const VehicleScalar duration = (heatingCooling || brakeRimSoak) ? 180.0
        : (flatSpot ? 12.0 : 90.0);
    const int steps = static_cast<int>(std::llround(
        duration / result.integrationStepSeconds));
    const int sampleStride = static_cast<int>(std::llround(
        result.sampleIntervalSeconds / result.integrationStepSeconds));

    tires::TireThermalState thermalState;
    tires::TireWearState wearState;
    tires::TireThermalOutput thermal =
        tires::evaluateTireThermalState(tire.thermal, thermalState);
    tires::TireWearInput wearInput;
    wearInput.nominalLoadN = tire.nominalLoad;
    wearInput.referencePressurePa = tire.referenceInflationPressurePa;
    VehicleScalar wheelRotationDegrees = 0.0;

    for (int step = 0; step <= steps; ++step)
    {
        const VehicleScalar time = step * result.integrationStepSeconds;
        const bool cooling = (heatingCooling && time >= 60.0)
            || (brakeRimSoak && time >= 45.0);
        const VehicleScalar brakeSpeedMps = std::max(
            VehicleScalar{15.0}, VehicleScalar{45.0} - time * VehicleScalar{0.65});
        const VehicleScalar speedMps = brakeRimSoak
            ? (cooling ? VehicleScalar{20.0} : brakeSpeedMps)
            : (flatSpot ? VehicleScalar{25.0}
                : (cooling ? VehicleScalar{20.0} : VehicleScalar{30.0}));
        const VehicleScalar targetKappa = cooling ? 0.0
            : (flatSpot ? -1.0 : (brakeRimSoak ? -0.04 : 0.08));
        const VehicleScalar targetAlpha = cooling || flatSpot
            ? 0.0 : 7.0 * kPi / 180.0;
        const TireContactInput input = contactInput(
            tire, wheelRadiusM, speedMps, tire.nominalLoad,
            targetKappa, targetAlpha, thermal.inflationPressurePa,
            thermal.frictionScale, thermal.stiffnessScale);
        const TireForceResult force = evaluateAdvancedRoadTire(tire, input);

        tires::TireThermalInput thermalInput;
        thermalInput.grounded = !cooling;
        thermalInput.forwardSpeedMps = speedMps;
        thermalInput.longitudinalSlipVelocityMps = targetKappa * speedMps;
        thermalInput.lateralSlipVelocityMps = std::tan(targetAlpha) * speedMps;
        thermalInput.longitudinalForceN = force.longitudinalForce;
        thermalInput.lateralForceN = force.lateralForce;
        thermalInput.radialDissipationWatts = cooling ? 0.0 : 120.0;
        thermalInput.rollingResistanceDissipationWatts = cooling ? 0.0 : 80.0;
        thermalInput.brakeDissipationWatts = brakeRimSoak && !cooling
            ? VehicleScalar{900.0} * speedMps
                / std::max(wheelRadiusM, VehicleScalar{0.05})
            : VehicleScalar{0.0};
        thermalInput.contactPatchAreaM2 = 0.020;
        if (step > 0)
        {
            thermal = tires::advanceTireThermal(
                tire.thermal, thermalInput,
                result.integrationStepSeconds, thermalState);
        }

        if (!flatSpot)
        {
            wheelRotationDegrees += speedMps / wheelRadiusM
                * result.integrationStepSeconds * 180.0 / kPi;
        }
        wearInput.grounded = !cooling;
        wearInput.wheelRotationDegrees = wheelRotationDegrees;
        wearInput.normalLoadN = tire.nominalLoad;
        wearInput.camberAngleRadians = targetAlpha == 0.0
            ? 0.0 : -2.0 * kPi / 180.0;
        wearInput.inflationPressurePa = thermal.inflationPressurePa;
        wearInput.bulkTreadTemperatureC = thermal.treadTemperatureC;
        wearInput.slipDissipationWatts = thermal.slipDissipationWatts;
        const tires::TireWearOutput wear = step > 0
            ? tires::advanceTireWear(
                tire.wear, tire.thermal, wearInput,
                result.integrationStepSeconds, wearState)
            : tires::evaluateTireWearState(
                tire.wear, tire.thermal, wearInput, wearState);

        if (step % sampleStride == 0)
        {
            result.samples.push_back(commonSample(
                time, targetKappa, targetKappa, targetAlpha, targetAlpha,
                input, force, thermal, wear, {}));
        }
    }
    result.valid = !result.samples.empty();
    return result;
}

TireScenarioResult failureScenario(
    const TireModelDescription& tire,
    VehicleScalar wheelRadiusM,
    const std::string& name)
{
    TireScenarioResult result;
    result.name = name;
    if (!tire.thermal.enabled || !tire.failure.enabled)
    {
        result.error = "The fitted tire does not enable both thermal and failure providers.";
        return result;
    }

    const bool blowout = name == "blowout_pressure_loss";
    result.integrationStepSeconds = blowout ? 0.001 : 0.01;
    result.sampleIntervalSeconds = blowout ? 0.02 : 0.10;
    const VehicleScalar duration = blowout ? 12.0 : 120.0;
    const int steps = static_cast<int>(std::llround(
        duration / result.integrationStepSeconds));
    const int sampleStride = static_cast<int>(std::llround(
        result.sampleIntervalSeconds / result.integrationStepSeconds));

    tires::TireThermalState thermalState;
    tires::TireFailureState failureState;
    tires::TireThermalOutput thermal =
        tires::evaluateTireThermalState(tire.thermal, thermalState);
    tires::TireFailureInput failureInput;
    failureInput.grounded = true;
    failureInput.referenceGaugePressurePa =
        tire.thermal.referenceGaugePressurePa;
    failureInput.referenceTemperatureC = tire.thermal.referenceTemperatureC;
    failureInput.identifiedReferencePressurePa =
        tire.referenceInflationPressurePa;
    failureInput.nominalLoadN = tire.nominalLoad;
    failureInput.normalLoadN = tire.nominalLoad;
    failureInput.forwardSpeedMps = blowout ? 32.0 : 18.0;
    failureInput.lateralSlipVelocityMps = blowout ? 2.0 : 0.5;
    failureInput.radialDissipationWatts = blowout ? 1200.0 : 100.0;
    failureInput.inflationGaugePressurePa = thermal.inflationPressurePa;
    failureInput.gasTemperatureC = thermal.gasTemperatureC;
    failureInput.carcassTemperatureC = thermal.carcassTemperatureC;
    tires::triggerTireFailure(
        tire.failure,
        failureInput,
        blowout ? tires::TireFailureStage::Blowout
                : tires::TireFailureStage::SlowPuncture,
        failureState);

    tires::TireFailureOutput failure = tires::evaluateTireFailureState(
        tire.failure, failureInput, failureState);
    for (int step = 0; step <= steps; ++step)
    {
        const VehicleScalar time = step * result.integrationStepSeconds;
        failureInput.inflationGaugePressurePa = thermal.inflationPressurePa;
        failureInput.gasTemperatureC = thermal.gasTemperatureC;
        failureInput.carcassTemperatureC = thermal.carcassTemperatureC;
        if (step > 0)
        {
            failure = tires::advanceTireFailure(
                tire.failure, failureInput,
                result.integrationStepSeconds, failureState);
            thermalState.containedGasMassRatio = failure.containedGasMassRatio;
            tires::TireThermalInput thermalInput;
            thermalInput.grounded = true;
            thermalInput.forwardSpeedMps = failureInput.forwardSpeedMps;
            thermalInput.radialDissipationWatts =
                failureInput.radialDissipationWatts;
            thermal = tires::advanceTireThermal(
                tire.thermal, thermalInput,
                result.integrationStepSeconds, thermalState);
        }

        const TireContactInput input = contactInput(
            tire, wheelRadiusM, failureInput.forwardSpeedMps,
            tire.nominalLoad, 0.02, 2.0 * kPi / 180.0,
            thermal.inflationPressurePa,
            failure.forceCapacityScale,
            failure.carcassSupportScale);
        const TireForceResult force = evaluateAdvancedRoadTire(tire, input);
        if (step % sampleStride == 0)
        {
            result.samples.push_back(commonSample(
                time, 0.02, 0.02, 2.0 * kPi / 180.0,
                2.0 * kPi / 180.0, input, force, thermal, {}, failure));
        }
    }
    result.valid = !result.samples.empty();
    return result;
}

bool finiteSample(const TireScenarioSample& sample)
{
    return finiteValue(sample.timeSeconds)
        && finiteValue(sample.force.longitudinalForce)
        && finiteValue(sample.force.lateralForce)
        && finiteValue(sample.force.aligningTorque)
        && finiteValue(sample.inflationPressurePa)
        && finiteValue(sample.rimTemperatureC)
        && finiteValue(sample.averageTreadDepthM)
        && finiteValue(sample.flatSpotDepthM)
        && finiteValue(sample.containedGasMassRatio);
}

} // namespace

std::vector<std::string> standardTireScenarioNames()
{
    return {
        "relaxation_step",
        "heating_cooling",
        "sustained_cornering_wear",
        "braking_flat_spot",
        "brake_rim_soak",
        "slow_puncture_pressure_loss",
        "blowout_pressure_loss"
    };
}

TireScenarioResult runStandardTireScenario(
    const TireModelDescription& tire,
    VehicleScalar wheelRadiusM,
    const std::string& scenarioName)
{
    TireScenarioResult result;
    result.name = scenarioName;
    if (!validTireModelDescription(tire)
        || !finiteValue(wheelRadiusM) || wheelRadiusM <= 0.0)
    {
        result.error = "The tire scenario received an invalid fitted tire or wheel radius.";
        return result;
    }

    if (scenarioName == "relaxation_step")
        result = relaxationStep(tire, wheelRadiusM);
    else if (scenarioName == "heating_cooling"
        || scenarioName == "sustained_cornering_wear"
        || scenarioName == "braking_flat_spot"
        || scenarioName == "brake_rim_soak")
        result = thermalWearScenario(tire, wheelRadiusM, scenarioName);
    else if (scenarioName == "slow_puncture_pressure_loss"
        || scenarioName == "blowout_pressure_loss")
        result = failureScenario(tire, wheelRadiusM, scenarioName);
    else
        result.error = "Unknown canonical tire scenario.";

    if (result.valid)
    {
        const bool allFinite = std::all_of(
            result.samples.begin(), result.samples.end(), finiteSample);
        if (!allFinite)
        {
            result.valid = false;
            result.samples.clear();
            result.error = "The tire scenario produced a non-finite sample.";
        }
    }
    return result;
}

bool exportTireScenarioCsv(
    const TireScenarioResult& result,
    const std::filesystem::path& path,
    std::string* error)
{
    if (!result.valid || result.samples.empty() || path.empty())
    {
        assignError(error, "Only a valid non-empty tire scenario can be exported.");
        return false;
    }
    std::error_code filesystemError;
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path(), filesystemError);
        if (filesystemError)
        {
            assignError(error, "Could not create the tire scenario export directory: "
                + filesystemError.message());
            return false;
        }
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        assignError(error, "Could not open the tire scenario CSV export.");
        return false;
    }
    output << "scenario,time_s,target_kappa,effective_kappa,target_alpha_rad"
        << ",effective_alpha_rad,speed_mps,normal_load_n,fx_n,fy_n,mz_nm"
        << ",tread_temp_c,carcass_temp_c,gas_temp_c,rim_temp_c,pressure_pa"
        << ",average_tread_depth_m,minimum_tread_depth_m,flat_spot_depth_m"
        << ",wear_fraction,failure_stage,gas_mass_ratio,structural_integrity"
        << ",tread_attachment,rim_contact_fraction\n";
    output << std::fixed << std::setprecision(9);
    for (const TireScenarioSample& sample : result.samples)
    {
        output << result.name << ',' << sample.timeSeconds
            << ',' << sample.targetLongitudinalSlip
            << ',' << sample.effectiveLongitudinalSlip
            << ',' << sample.targetSlipAngleRadians
            << ',' << sample.effectiveSlipAngleRadians
            << ',' << sample.forwardSpeedMps
            << ',' << sample.normalLoadN
            << ',' << sample.force.longitudinalForce
            << ',' << sample.force.lateralForce
            << ',' << sample.force.aligningTorque
            << ',' << sample.treadTemperatureC
            << ',' << sample.carcassTemperatureC
            << ',' << sample.gasTemperatureC
            << ',' << sample.rimTemperatureC
            << ',' << sample.inflationPressurePa
            << ',' << sample.averageTreadDepthM
            << ',' << sample.minimumTreadDepthM
            << ',' << sample.flatSpotDepthM
            << ',' << sample.wearFraction
            << ',' << tires::tireFailureStageName(sample.failureStage)
            << ',' << sample.containedGasMassRatio
            << ',' << sample.structuralIntegrity
            << ',' << sample.treadAttachment
            << ',' << sample.rimContactFraction << '\n';
    }
    if (!output)
    {
        assignError(error, "Writing the tire scenario CSV export failed.");
        return false;
    }
    if (error)
        error->clear();
    return true;
}

} // namespace heritage::vehicles
