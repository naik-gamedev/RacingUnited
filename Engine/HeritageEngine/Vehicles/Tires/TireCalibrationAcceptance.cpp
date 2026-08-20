#include "TireCalibrationAcceptance.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace heritage::vehicles {
namespace {

constexpr std::size_t kMaximumStoredViolations = 256;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

TireCalibrationValueEnvelope relativeEnvelope(
    VehicleScalar value,
    VehicleScalar relativeTolerance,
    VehicleScalar absoluteTolerance)
{
    const VehicleScalar tolerance = std::max(
        std::abs(value) * relativeTolerance,
        absoluteTolerance);
    return { true, value - tolerance, value + tolerance };
}

bool valueInside(
    VehicleScalar value,
    const TireCalibrationValueEnvelope& envelope)
{
    return !envelope.enabled
        || (finiteValue(value)
            && value >= envelope.minimum
            && value <= envelope.maximum);
}

void addViolation(
    TireCalibrationAcceptanceReport& report,
    std::size_t sampleIndex,
    const char* code,
    const std::string& message)
{
    ++report.violationCount;
    if (report.violations.size() < kMaximumStoredViolations)
        report.violations.push_back({ sampleIndex, code, message });
}

bool finiteForce(const TireForceResult& force)
{
    return finiteValue(force.longitudinalForce)
        && finiteValue(force.lateralForce)
        && finiteValue(force.aligningTorque)
        && finiteValue(force.overturningMoment)
        && finiteValue(force.rollingResistanceMoment)
        && finiteValue(force.effectiveFriction)
        && finiteValue(force.longitudinalSlipStiffness)
        && finiteValue(force.corneringStiffness);
}

void checkEnvelopeValue(
    TireCalibrationAcceptanceReport& report,
    std::size_t sampleIndex,
    const char* code,
    const char* label,
    VehicleScalar value,
    const TireCalibrationValueEnvelope& expected)
{
    if (valueInside(value, expected))
        return;
    std::ostringstream message;
    message << label << '=' << value << " outside ["
        << expected.minimum << ',' << expected.maximum << ']';
    addViolation(report, sampleIndex, code, message.str());
}

} // namespace

TireCalibrationAcceptanceEnvelope buildRelativeTireCalibrationEnvelope(
    const TireCalibrationSweepResult& reference,
    const std::string& source,
    const std::string& provenance,
    bool synthetic,
    VehicleScalar confidence,
    VehicleScalar relativeForceTolerance,
    VehicleScalar absoluteForceToleranceN,
    VehicleScalar relativeMomentTolerance,
    VehicleScalar absoluteMomentToleranceNm)
{
    TireCalibrationAcceptanceEnvelope result;
    result.sweepName = reference.name;
    result.primaryAxis = reference.primary.axis;
    result.secondaryAxis = reference.secondary.axis;
    result.source = source;
    result.provenance = provenance;
    result.synthetic = synthetic;
    result.confidence = std::clamp(confidence,
        VehicleScalar{0.0}, VehicleScalar{1.0});
    if (!reference.valid || reference.samples.empty()
        || source.empty() || provenance.empty()
        || !finiteValue(relativeForceTolerance)
        || !finiteValue(absoluteForceToleranceN)
        || !finiteValue(relativeMomentTolerance)
        || !finiteValue(absoluteMomentToleranceNm)
        || relativeForceTolerance < 0.0
        || absoluteForceToleranceN < 0.0
        || relativeMomentTolerance < 0.0
        || absoluteMomentToleranceNm < 0.0)
    {
        result.points.clear();
        return result;
    }

    result.points.reserve(reference.samples.size());
    for (const TireCalibrationSample& sample : reference.samples)
    {
        TireCalibrationAcceptancePoint point;
        point.primaryIndex = sample.primaryIndex;
        point.secondaryIndex = sample.secondaryIndex;
        point.primaryValue = sample.primaryValue;
        point.secondaryValue = sample.secondaryValue;
        point.longitudinalForceN = relativeEnvelope(
            sample.force.longitudinalForce,
            relativeForceTolerance, absoluteForceToleranceN);
        point.lateralForceN = relativeEnvelope(
            sample.force.lateralForce,
            relativeForceTolerance, absoluteForceToleranceN);
        point.aligningTorqueNm = relativeEnvelope(
            sample.force.aligningTorque,
            relativeMomentTolerance, absoluteMomentToleranceNm);
        point.overturningMomentNm = relativeEnvelope(
            sample.force.overturningMoment,
            relativeMomentTolerance, absoluteMomentToleranceNm);
        point.rollingResistanceMomentNm = relativeEnvelope(
            sample.force.rollingResistanceMoment,
            relativeMomentTolerance, absoluteMomentToleranceNm);
        result.points.push_back(point);
    }
    return result;
}

TireCalibrationAcceptanceReport evaluateTireCalibrationAcceptance(
    const TireModelDescription& tire,
    const TireCalibrationSweepResult& candidate,
    const TireCalibrationAcceptanceEnvelope& envelope)
{
    TireCalibrationAcceptanceReport report;
    report.usedSyntheticReference = envelope.synthetic;
    if (!validTireModelDescription(tire)
        || !candidate.valid || candidate.samples.empty())
    {
        report.error = "Tire acceptance requires a valid tire and non-empty candidate sweep.";
        return report;
    }
    if (envelope.schema != "heritage_tire_acceptance_v1"
        || envelope.sweepName != candidate.name
        || envelope.primaryAxis != candidate.primary.axis
        || envelope.secondaryAxis != candidate.secondary.axis
        || envelope.source.empty() || envelope.provenance.empty()
        || !finiteValue(envelope.confidence)
        || envelope.confidence < 0.0 || envelope.confidence > 1.0
        || envelope.points.size() != candidate.samples.size())
    {
        report.error = "Tire acceptance envelope metadata or sample topology does not match the candidate.";
        return report;
    }

    report.valid = true;
    report.evaluatedSampleCount = candidate.samples.size();
    for (std::size_t index = 0; index < candidate.samples.size(); ++index)
    {
        const TireCalibrationSample& sample = candidate.samples[index];
        const TireCalibrationAcceptancePoint& expected = envelope.points[index];
        if (sample.primaryIndex != expected.primaryIndex
            || sample.secondaryIndex != expected.secondaryIndex
            || std::abs(sample.primaryValue - expected.primaryValue) > 1.0e-12
            || std::abs(sample.secondaryValue - expected.secondaryValue) > 1.0e-12)
        {
            addViolation(report, index, "sample_topology",
                "Candidate sample axes do not match the acceptance point.");
            continue;
        }
        if (!finiteForce(sample.force))
        {
            addViolation(report, index, "non_finite",
                "Candidate contains a non-finite force, moment or stiffness.");
            continue;
        }

        if (!tire.magicFormulaUsesLegacySeed)
        {
            const auto& validity = tire.magicFormula;
            if (sample.input.normalLoad < validity.minimumLoadN
                || sample.input.normalLoad > validity.maximumLoadN
                || sample.input.inflationPressurePa < validity.minimumPressurePa
                || sample.input.inflationPressurePa > validity.maximumPressurePa
                || std::abs(sample.input.longitudinalSlip)
                    > validity.maximumAbsLongitudinalSlip
                || std::abs(sample.input.slipAngleRadians)
                    > validity.maximumAbsSlipAngleRadians
                || std::abs(sample.input.camberAngleRadians)
                    > validity.maximumAbsCamberRadians)
            {
                addViolation(report, index, "outside_fit_validity",
                    "Candidate operating point is outside fitted parameter validity.");
            }
        }

        if (candidate.name == "pure_longitudinal"
            && std::abs(sample.input.longitudinalSlip) > 1.0e-5
            && sample.force.longitudinalForce
                * sample.input.longitudinalSlip < 0.0)
        {
            addViolation(report, index, "wrong_fx_sign",
                "Longitudinal force opposes the authored slip-ratio convention.");
        }
        if (candidate.name == "pure_lateral"
            && std::abs(sample.input.slipAngleRadians) > 1.0e-5
            && sample.force.lateralForce
                * sample.input.slipAngleRadians > 0.0)
        {
            addViolation(report, index, "wrong_fy_sign",
                "Lateral force violates the authored slip-angle convention.");
        }

        checkEnvelopeValue(report, index, "fx_envelope", "Fx",
            sample.force.longitudinalForce, expected.longitudinalForceN);
        checkEnvelopeValue(report, index, "fy_envelope", "Fy",
            sample.force.lateralForce, expected.lateralForceN);
        checkEnvelopeValue(report, index, "mz_envelope", "Mz",
            sample.force.aligningTorque, expected.aligningTorqueNm);
        checkEnvelopeValue(report, index, "mx_envelope", "Mx",
            sample.force.overturningMoment, expected.overturningMomentNm);
        checkEnvelopeValue(report, index, "my_envelope", "My",
            sample.force.rollingResistanceMoment,
            expected.rollingResistanceMomentNm);

        if (sample.primaryIndex > 0 && index > 0)
        {
            const TireCalibrationSample& previous = candidate.samples[index - 1];
            if (previous.secondaryIndex == sample.secondaryIndex)
            {
                const VehicleScalar localLoad = std::max({
                    std::abs(previous.input.normalLoad),
                    std::abs(sample.input.normalLoad), VehicleScalar{1.0} });
                if (std::hypot(
                        sample.force.longitudinalForce
                            - previous.force.longitudinalForce,
                        sample.force.lateralForce
                            - previous.force.lateralForce)
                    > localLoad
                        * envelope.maximumAdjacentForceJumpLoadFraction)
                {
                    addViolation(report, index, "force_discontinuity",
                        "Adjacent force samples exceed the configured continuity gate.");
                }
                const VehicleScalar radius = std::max(
                    std::abs(sample.input.wheelRadiusM), VehicleScalar{0.01});
                if (std::abs(sample.force.aligningTorque
                        - previous.force.aligningTorque)
                    > localLoad * radius
                        * envelope.maximumAdjacentMomentJumpLoadRadiusFraction)
                {
                    addViolation(report, index, "moment_discontinuity",
                        "Adjacent aligning-moment samples exceed the configured continuity gate.");
                }
            }
        }
    }

    report.passed = report.violationCount == 0;
    return report;
}

} // namespace heritage::vehicles
