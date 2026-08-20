#include "TireCalibrationLab.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <system_error>
#include <utility>

namespace heritage::vehicles {
namespace {

constexpr std::size_t kMaximumPrimarySamples = 2048;
constexpr std::size_t kMaximumSecondarySamples = 512;
constexpr std::size_t kMaximumTotalSamples = 262144;
constexpr VehicleScalar kPi = 3.14159265358979323846;
constexpr VehicleScalar kRadiansPerDegree = kPi / VehicleScalar{180.0};
constexpr VehicleScalar kPascalsPerPsi = 6894.757293168;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

void assignError(std::string* error, const std::string& message)
{
    if (error)
        *error = message;
}

VehicleScalar interpolatedValue(
    const TireCalibrationAxisRange& range,
    std::size_t index)
{
    if (range.sampleCount <= 1)
        return range.minimum;
    const VehicleScalar fraction = static_cast<VehicleScalar>(index)
        / static_cast<VehicleScalar>(range.sampleCount - 1);
    return range.minimum + (range.maximum - range.minimum) * fraction;
}

bool axisRangeValid(
    const TireCalibrationAxisRange& range,
    bool primary,
    std::string* error)
{
    if (range.axis == TireCalibrationAxis::None)
    {
        if (primary)
        {
            assignError(error, "The primary tire calibration axis cannot be None.");
            return false;
        }
        if (range.sampleCount != 1)
        {
            assignError(error, "A disabled secondary tire calibration axis must contain one sample.");
            return false;
        }
        return true;
    }

    const std::size_t maximum = primary
        ? kMaximumPrimarySamples : kMaximumSecondarySamples;
    if (range.sampleCount < 2 || range.sampleCount > maximum)
    {
        assignError(error, primary
            ? "The primary tire calibration axis requires 2..2048 samples."
            : "The secondary tire calibration axis requires 2..512 samples.");
        return false;
    }
    if (!finiteValue(range.minimum) || !finiteValue(range.maximum)
        || range.maximum <= range.minimum)
    {
        assignError(error, "Tire calibration axis bounds must be finite and increasing.");
        return false;
    }

    const VehicleScalar largestMagnitude = std::max(
        std::abs(range.minimum), std::abs(range.maximum));
    switch (range.axis)
    {
    case TireCalibrationAxis::LongitudinalSlip:
        if (largestMagnitude > VehicleScalar{2.0})
        {
            assignError(error, "Longitudinal-slip sweeps are bounded to +/-2.0.");
            return false;
        }
        break;
    case TireCalibrationAxis::SlipAngleRadians:
    case TireCalibrationAxis::CamberAngleRadians:
        if (largestMagnitude > VehicleScalar{1.55})
        {
            assignError(error, "Angular tire sweeps are bounded below 90 degrees.");
            return false;
        }
        break;
    case TireCalibrationAxis::NormalLoadNewtons:
        if (range.minimum < VehicleScalar{0.0}
            || range.maximum > VehicleScalar{1000000.0})
        {
            assignError(error, "Normal-load sweeps require 0..1000000 N.");
            return false;
        }
        break;
    case TireCalibrationAxis::InflationPressurePascals:
        if (range.minimum < VehicleScalar{0.0}
            || range.maximum > VehicleScalar{2000000.0})
        {
            assignError(error, "Inflation-pressure sweeps require 0..2000000 Pa gauge.");
            return false;
        }
        break;
    case TireCalibrationAxis::TurnSlipPerMeter:
        if (largestMagnitude > VehicleScalar{100.0})
        {
            assignError(error, "Turn-slip sweeps are bounded to +/-100 1/m.");
            return false;
        }
        break;
    case TireCalibrationAxis::None:
        break;
    }
    return true;
}

bool baselineValid(const TireContactInput& input)
{
    return finiteValue(input.normalLoad)
        && input.normalLoad >= VehicleScalar{0.0}
        && finiteValue(input.longitudinalSlip)
        && finiteValue(input.slipAngleRadians)
        && finiteValue(input.camberAngleRadians)
        && finiteValue(input.forwardSpeedMps)
        && input.forwardSpeedMps >= VehicleScalar{0.0}
        && finiteValue(input.turnSlipPerM)
        && finiteValue(input.contactPatchTurnMomentNm)
        && finiteValue(input.wheelRadiusM)
        && input.wheelRadiusM > VehicleScalar{0.0}
        && finiteValue(input.inflationPressurePa)
        && input.inflationPressurePa >= VehicleScalar{-1.0}
        && finiteValue(input.frictionMultiplier)
        && input.frictionMultiplier >= VehicleScalar{0.0}
        && finiteValue(input.stiffnessMultiplier)
        && input.stiffnessMultiplier >= VehicleScalar{0.0};
}

void applyAxis(
    TireContactInput& input,
    TireCalibrationAxis axis,
    VehicleScalar value)
{
    switch (axis)
    {
    case TireCalibrationAxis::LongitudinalSlip:
        input.longitudinalSlip = value;
        break;
    case TireCalibrationAxis::SlipAngleRadians:
        input.slipAngleRadians = value;
        break;
    case TireCalibrationAxis::NormalLoadNewtons:
        input.normalLoad = value;
        break;
    case TireCalibrationAxis::InflationPressurePascals:
        input.inflationPressurePa = value;
        break;
    case TireCalibrationAxis::CamberAngleRadians:
        input.camberAngleRadians = value;
        break;
    case TireCalibrationAxis::TurnSlipPerMeter:
        input.turnSlipPerM = value;
        break;
    case TireCalibrationAxis::None:
        break;
    }
}

bool finiteForce(const TireForceResult& force)
{
    return finiteValue(force.longitudinalForce)
        && finiteValue(force.lateralForce)
        && finiteValue(force.pureLongitudinalForce)
        && finiteValue(force.pureLateralForce)
        && finiteValue(force.effectiveFriction)
        && finiteValue(force.gripUtilization)
        && finiteValue(force.combinedSlipScale)
        && finiteValue(force.pneumaticTrail)
        && finiteValue(force.aligningTorque)
        && finiteValue(force.overturningMoment)
        && finiteValue(force.rollingResistanceMoment)
        && finiteValue(force.residualAligningTorque)
        && finiteValue(force.longitudinalSlipStiffness)
        && finiteValue(force.corneringStiffness)
        && finiteValue(force.camberStiffness)
        && finiteValue(force.turnSlipMoment);
}

TireCalibrationSweepDescription makeSweep(
    const std::string& name,
    const TireContactInput& baseline,
    TireCalibrationAxis primaryAxis,
    VehicleScalar primaryMinimum,
    VehicleScalar primaryMaximum,
    std::size_t primarySamples,
    TireCalibrationAxis secondaryAxis = TireCalibrationAxis::None,
    VehicleScalar secondaryMinimum = VehicleScalar{0.0},
    VehicleScalar secondaryMaximum = VehicleScalar{0.0},
    std::size_t secondarySamples = 1)
{
    TireCalibrationSweepDescription sweep;
    sweep.name = name;
    sweep.baseline = baseline;
    sweep.primary = {
        primaryAxis, primaryMinimum, primaryMaximum, primarySamples};
    sweep.secondary = {
        secondaryAxis, secondaryMinimum, secondaryMaximum, secondarySamples};
    return sweep;
}

std::string escapedCsvString(const std::string& value)
{
    std::string result = "\"";
    for (const char character : value)
    {
        if (character == '\"')
            result += "\"\"";
        else
            result += character;
    }
    result += '\"';
    return result;
}

} // namespace

const char* tireCalibrationAxisName(TireCalibrationAxis axis)
{
    switch (axis)
    {
    case TireCalibrationAxis::None: return "none";
    case TireCalibrationAxis::LongitudinalSlip: return "longitudinal_slip";
    case TireCalibrationAxis::SlipAngleRadians: return "slip_angle_rad";
    case TireCalibrationAxis::NormalLoadNewtons: return "normal_load_n";
    case TireCalibrationAxis::InflationPressurePascals: return "inflation_pressure_pa";
    case TireCalibrationAxis::CamberAngleRadians: return "camber_angle_rad";
    case TireCalibrationAxis::TurnSlipPerMeter: return "turn_slip_per_m";
    default: return "unknown";
    }
}

bool validTireCalibrationSweepDescription(
    const TireCalibrationSweepDescription& description,
    std::string* error)
{
    if (description.name.empty())
    {
        assignError(error, "A tire calibration sweep requires a name.");
        return false;
    }
    if (!baselineValid(description.baseline))
    {
        assignError(error, "The tire calibration baseline contains invalid physical inputs.");
        return false;
    }
    if (!axisRangeValid(description.primary, true, error)
        || !axisRangeValid(description.secondary, false, error))
    {
        return false;
    }
    if (description.secondary.axis == description.primary.axis)
    {
        assignError(error, "Primary and secondary tire calibration axes must be different.");
        return false;
    }
    if (description.primary.sampleCount
        > kMaximumTotalSamples / description.secondary.sampleCount)
    {
        assignError(error, "The tire calibration sweep exceeds 262144 samples.");
        return false;
    }
    if (error)
        error->clear();
    return true;
}

TireCalibrationSweepResult runTireCalibrationSweep(
    const TireModelDescription& tire,
    const TireCalibrationSweepDescription& description)
{
    TireCalibrationSweepResult result;
    result.name = description.name;
    result.primary = description.primary;
    result.secondary = description.secondary;

    if (!validTireModelDescription(tire))
    {
        result.error = "The tire calibration sweep received an invalid tire model.";
        return result;
    }
    if (!validTireCalibrationSweepDescription(description, &result.error))
        return result;

    const std::size_t totalSamples = description.primary.sampleCount
        * description.secondary.sampleCount;
    result.samples.reserve(totalSamples);
    for (std::size_t secondaryIndex = 0;
         secondaryIndex < description.secondary.sampleCount;
         ++secondaryIndex)
    {
        const VehicleScalar secondaryValue = interpolatedValue(
            description.secondary, secondaryIndex);
        for (std::size_t primaryIndex = 0;
             primaryIndex < description.primary.sampleCount;
             ++primaryIndex)
        {
            TireCalibrationSample sample;
            sample.primaryIndex = primaryIndex;
            sample.secondaryIndex = secondaryIndex;
            sample.primaryValue = interpolatedValue(
                description.primary, primaryIndex);
            sample.secondaryValue = secondaryValue;
            sample.input = description.baseline;
            applyAxis(
                sample.input, description.primary.axis, sample.primaryValue);
            applyAxis(
                sample.input, description.secondary.axis, sample.secondaryValue);
            sample.force = evaluateAdvancedRoadTire(tire, sample.input);
            if (!finiteForce(sample.force))
            {
                result.samples.clear();
                result.error = "The tire model produced a non-finite calibration sample.";
                return result;
            }
            result.samples.push_back(std::move(sample));
        }
    }

    result.valid = true;
    return result;
}

std::vector<TireCalibrationSweepDescription> standardTireCalibrationSweeps(
    const TireModelDescription& tire,
    VehicleScalar wheelRadiusM)
{
    std::vector<TireCalibrationSweepDescription> sweeps;
    if (!validTireModelDescription(tire)
        || !finiteValue(wheelRadiusM) || wheelRadiusM <= VehicleScalar{0.0})
    {
        return sweeps;
    }

    TireContactInput baseline;
    baseline.normalLoad = tire.nominalLoad;
    baseline.forwardSpeedMps = VehicleScalar{25.0};
    baseline.wheelRadiusM = wheelRadiusM;
    baseline.inflationPressurePa = tire.inflationPressurePa;

    sweeps.push_back(makeSweep(
        "pure_longitudinal", baseline,
        TireCalibrationAxis::LongitudinalSlip,
        VehicleScalar{-0.30}, VehicleScalar{0.30}, 121));

    sweeps.push_back(makeSweep(
        "pure_lateral", baseline,
        TireCalibrationAxis::SlipAngleRadians,
        VehicleScalar{-12.0} * kRadiansPerDegree,
        VehicleScalar{12.0} * kRadiansPerDegree,
        121));

    sweeps.push_back(makeSweep(
        "combined_slip_map", baseline,
        TireCalibrationAxis::LongitudinalSlip,
        VehicleScalar{-0.20}, VehicleScalar{0.20}, 41,
        TireCalibrationAxis::SlipAngleRadians,
        VehicleScalar{-12.0} * kRadiansPerDegree,
        VehicleScalar{12.0} * kRadiansPerDegree,
        49));

    TireContactInput loaded = baseline;
    loaded.slipAngleRadians = VehicleScalar{6.0} * kRadiansPerDegree;
    sweeps.push_back(makeSweep(
        "load_sensitivity", loaded,
        TireCalibrationAxis::NormalLoadNewtons,
        tire.nominalLoad * VehicleScalar{0.50},
        tire.nominalLoad * VehicleScalar{1.60},
        45));

    VehicleScalar minimumPressure = tire.referenceInflationPressurePa
        * VehicleScalar{0.55};
    VehicleScalar maximumPressure = tire.referenceInflationPressurePa
        * VehicleScalar{1.45};
    if (!tire.magicFormulaUsesLegacySeed)
    {
        minimumPressure = std::max(
            minimumPressure, tire.magicFormula.minimumPressurePa);
        maximumPressure = std::min(
            maximumPressure, tire.magicFormula.maximumPressurePa);
    }
    if (maximumPressure <= minimumPressure)
    {
        minimumPressure = std::max(
            VehicleScalar{1000.0}, tire.referenceInflationPressurePa * VehicleScalar{0.90});
        maximumPressure = std::max(
            minimumPressure + VehicleScalar{1000.0},
            tire.referenceInflationPressurePa * VehicleScalar{1.10});
    }
    sweeps.push_back(makeSweep(
        "pressure_sensitivity", loaded,
        TireCalibrationAxis::InflationPressurePascals,
        minimumPressure, maximumPressure, 45));

    TireContactInput cambered = baseline;
    cambered.slipAngleRadians = VehicleScalar{4.0} * kRadiansPerDegree;
    sweeps.push_back(makeSweep(
        "camber_sensitivity", cambered,
        TireCalibrationAxis::CamberAngleRadians,
        VehicleScalar{-6.0} * kRadiansPerDegree,
        VehicleScalar{6.0} * kRadiansPerDegree,
        49));

    sweeps.push_back(makeSweep(
        "turn_slip_sensitivity", loaded,
        TireCalibrationAxis::TurnSlipPerMeter,
        VehicleScalar{-2.0}, VehicleScalar{2.0}, 81));

    return sweeps;
}

std::vector<TireCalibrationSweepResult> runStandardTireCalibrationSuite(
    const TireModelDescription& tire,
    VehicleScalar wheelRadiusM)
{
    const auto descriptions = standardTireCalibrationSweeps(tire, wheelRadiusM);
    std::vector<TireCalibrationSweepResult> results;
    results.reserve(descriptions.size());
    for (const auto& description : descriptions)
        results.push_back(runTireCalibrationSweep(tire, description));
    return results;
}

bool exportTireCalibrationSweepCsv(
    const TireCalibrationSweepResult& result,
    const std::filesystem::path& path,
    std::string* error)
{
    if (!result.valid || result.samples.empty())
    {
        assignError(error, "Only a valid non-empty tire calibration sweep can be exported.");
        return false;
    }
    if (path.empty())
    {
        assignError(error, "Tire calibration CSV export requires a non-empty path.");
        return false;
    }

    std::error_code filesystemError;
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path(), filesystemError);
        if (filesystemError)
        {
            assignError(error, "Could not create the tire calibration export directory: "
                + filesystemError.message());
            return false;
        }
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        assignError(error, "Could not open the tire calibration CSV export.");
        return false;
    }

    output << "sweep,primary_axis,secondary_axis,primary_index,secondary_index"
        << ",primary_value,secondary_value,normal_load_n,longitudinal_slip"
        << ",slip_angle_rad,slip_angle_deg,camber_rad,camber_deg"
        << ",inflation_pressure_pa,inflation_pressure_psi,forward_speed_mps"
        << ",turn_slip_per_m,fx_n,fy_n,pure_fx_n,pure_fy_n"
        << ",mx_nm,my_nm,mz_nm,residual_mz_nm,pneumatic_trail_m"
        << ",grip_utilization,combined_slip_scale"
        << ",longitudinal_stiffness_n,cornering_stiffness_n_per_rad"
        << ",camber_stiffness_n_per_rad\n";
    output << std::fixed << std::setprecision(9);

    for (const auto& sample : result.samples)
    {
        output << escapedCsvString(result.name)
            << ',' << tireCalibrationAxisName(result.primary.axis)
            << ',' << tireCalibrationAxisName(result.secondary.axis)
            << ',' << sample.primaryIndex
            << ',' << sample.secondaryIndex
            << ',' << sample.primaryValue
            << ',' << sample.secondaryValue
            << ',' << sample.input.normalLoad
            << ',' << sample.input.longitudinalSlip
            << ',' << sample.input.slipAngleRadians
            << ',' << sample.input.slipAngleRadians / kRadiansPerDegree
            << ',' << sample.input.camberAngleRadians
            << ',' << sample.input.camberAngleRadians / kRadiansPerDegree
            << ',' << sample.input.inflationPressurePa
            << ',' << sample.input.inflationPressurePa / kPascalsPerPsi
            << ',' << sample.input.forwardSpeedMps
            << ',' << sample.input.turnSlipPerM
            << ',' << sample.force.longitudinalForce
            << ',' << sample.force.lateralForce
            << ',' << sample.force.pureLongitudinalForce
            << ',' << sample.force.pureLateralForce
            << ',' << sample.force.overturningMoment
            << ',' << sample.force.rollingResistanceMoment
            << ',' << sample.force.aligningTorque
            << ',' << sample.force.residualAligningTorque
            << ',' << sample.force.pneumaticTrail
            << ',' << sample.force.gripUtilization
            << ',' << sample.force.combinedSlipScale
            << ',' << sample.force.longitudinalSlipStiffness
            << ',' << sample.force.corneringStiffness
            << ',' << sample.force.camberStiffness
            << '\n';
    }

    if (!output)
    {
        assignError(error, "Writing the tire calibration CSV export failed.");
        return false;
    }
    if (error)
        error->clear();
    return true;
}

} // namespace heritage::vehicles
