#include "TireFlexibleRingField.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kPi = 3.14159265358979323846;
constexpr VehicleScalar kTwoPi = 2.0 * kPi;
constexpr VehicleScalar kEpsilon = 1.0e-9;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar smoothStep(
    VehicleScalar edge0,
    VehicleScalar edge1,
    VehicleScalar value)
{
    const VehicleScalar t = std::clamp(
        (value - edge0) / std::max(edge1 - edge0, kEpsilon),
        VehicleScalar{0.0}, VehicleScalar{1.0});
    return t * t * (VehicleScalar{3.0} - VehicleScalar{2.0} * t);
}

VehicleScalar wrappedAngle(VehicleScalar angle)
{
    angle = std::fmod(angle, kTwoPi);
    return angle < 0.0 ? angle + kTwoPi : angle;
}

std::size_t fieldIndex(std::size_t station, std::size_t band)
{
    return station * TireFlexibleRingFieldBands + band;
}

VehicleScalar sampleDirectContact(
    const std::array<VehicleScalar, TireFlexibleRingContactCount>& values,
    VehicleScalar phi,
    std::size_t band)
{
    if (phi < 0.0 || phi > kPi)
        return 0.0;

    std::size_t upper = 1;
    while (upper + 1 < TireFlexibleRingContactStations
        && TireFlexibleRingContactPhiRadians[upper] < phi)
    {
        ++upper;
    }
    const std::size_t lower = upper - 1;
    const VehicleScalar a = TireFlexibleRingContactPhiRadians[lower];
    const VehicleScalar b = TireFlexibleRingContactPhiRadians[upper];
    VehicleScalar t = std::clamp(
        (phi - a) / std::max(b - a, kEpsilon),
        VehicleScalar{0.0}, VehicleScalar{1.0});
    t = t * t * (VehicleScalar{3.0} - VehicleScalar{2.0} * t);
    const VehicleScalar x = values[
        lower * TireFlexibleRingContactBands + band];
    const VehicleScalar y = values[
        upper * TireFlexibleRingContactBands + band];
    return x + (y - x) * t;
}

void solveElasticFoundation(
    const TireFlexibleRingFieldDescription& description,
    const std::array<VehicleScalar, TireFlexibleRingFieldCount>& target,
    const std::array<VehicleScalar, TireFlexibleRingFieldCount>& constraint,
    VehicleScalar pressureRatio,
    std::array<VehicleScalar, TireFlexibleRingFieldCount>& result)
{
    result = target;
    std::array<VehicleScalar, TireFlexibleRingFieldCount> next{};
    const VehicleScalar foundation = description.foundationStiffness
        * std::sqrt(std::clamp(pressureRatio, VehicleScalar{0.15}, VehicleScalar{3.0}));
    const VehicleScalar circumferential = description.circumferentialCoupling;
    const VehicleScalar lateral = description.lateralCoupling;

    // Jacobi relaxation solves a ring on a radial/lateral elastic foundation.
    // First- and second-neighbour terms approximate belt membrane and bending
    // coupling.  Contact is a penalty boundary condition, never a later clamp.
    for (int iteration = 0; iteration < 72; ++iteration)
    {
        for (std::size_t station = 0;
             station < TireFlexibleRingFieldStations; ++station)
        {
            const std::size_t stationMinusOne =
                (station + TireFlexibleRingFieldStations - 1)
                % TireFlexibleRingFieldStations;
            const std::size_t stationPlusOne =
                (station + 1) % TireFlexibleRingFieldStations;
            const std::size_t stationMinusTwo =
                (station + TireFlexibleRingFieldStations - 2)
                % TireFlexibleRingFieldStations;
            const std::size_t stationPlusTwo =
                (station + 2) % TireFlexibleRingFieldStations;

            for (std::size_t band = 0; band < TireFlexibleRingFieldBands; ++band)
            {
                const std::size_t index = fieldIndex(station, band);
                VehicleScalar numerator = 0.0;
                VehicleScalar denominator = foundation;

                const VehicleScalar contactWeight =
                    description.contactConstraintStiffness * constraint[index];
                numerator += target[index] * contactWeight;
                denominator += contactWeight;

                numerator += circumferential * VehicleScalar{0.82}
                    * (result[fieldIndex(stationMinusOne, band)]
                        + result[fieldIndex(stationPlusOne, band)]);
                denominator += circumferential * VehicleScalar{1.64};
                numerator += circumferential * VehicleScalar{0.18}
                    * (result[fieldIndex(stationMinusTwo, band)]
                        + result[fieldIndex(stationPlusTwo, band)]);
                denominator += circumferential * VehicleScalar{0.36};

                if (band > 0)
                {
                    numerator += lateral * result[fieldIndex(station, band - 1)];
                    denominator += lateral;
                }
                if (band + 1 < TireFlexibleRingFieldBands)
                {
                    numerator += lateral * result[fieldIndex(station, band + 1)];
                    denominator += lateral;
                }
                if (band > 1)
                {
                    numerator += lateral * VehicleScalar{0.20}
                        * result[fieldIndex(station, band - 2)];
                    denominator += lateral * VehicleScalar{0.20};
                }
                if (band + 2 < TireFlexibleRingFieldBands)
                {
                    numerator += lateral * VehicleScalar{0.20}
                        * result[fieldIndex(station, band + 2)];
                    denominator += lateral * VehicleScalar{0.20};
                }
                next[index] = numerator / std::max(denominator, kEpsilon);
            }
        }
        result = next;
    }
}

} // namespace

bool validTireFlexibleRingFieldDescription(
    const TireFlexibleRingFieldDescription& d)
{
    return finiteValue(d.unloadedRadiusM)
        && d.unloadedRadiusM >= 0.05 && d.unloadedRadiusM <= 2.5
        && finiteValue(d.rimRadiusM)
        && d.rimRadiusM >= 0.015
        && d.rimRadiusM < d.unloadedRadiusM - 0.005
        && finiteValue(d.sectionWidthM)
        && d.sectionWidthM >= 0.03 && d.sectionWidthM <= 1.5
        && finiteValue(d.maximumDeflectionM)
        && d.maximumDeflectionM >= 0.005
        && d.maximumDeflectionM <= d.unloadedRadiusM - d.rimRadiusM
        && finiteValue(d.referencePressurePa)
        && d.referencePressurePa >= 20000.0
        && d.referencePressurePa <= 1000000.0
        && finiteValue(d.verticalStiffnessNPerM)
        && d.verticalStiffnessNPerM >= 1000.0
        && d.verticalStiffnessNPerM <= 10000000.0
        && finiteValue(d.circumferentialCoupling)
        && d.circumferentialCoupling > 0.0
        && finiteValue(d.lateralCoupling) && d.lateralCoupling > 0.0
        && finiteValue(d.foundationStiffness) && d.foundationStiffness > 0.0
        && finiteValue(d.contactConstraintStiffness)
        && d.contactConstraintStiffness > 0.0
        && finiteValue(d.effectivePoissonRatio)
        && d.effectivePoissonRatio >= 0.0
        && d.effectivePoissonRatio < 0.5;
}

TireFlexibleRingFieldOutput evaluateTireFlexibleRingField(
    const TireFlexibleRingFieldDescription& d,
    const TireFlexibleRingFieldInput& input)
{
    TireFlexibleRingFieldOutput output;
    if (!validTireFlexibleRingFieldDescription(d)
        || !finiteValue(input.verticalDeflectionM)
        || !finiteValue(input.contactPatchLengthM)
        || !finiteValue(input.contactPatchWidthM)
        || !finiteValue(input.normalLoadN)
        || !finiteValue(input.inflationPressurePa)
        || !finiteValue(input.ringLongitudinalOffsetM)
        || !finiteValue(input.ringLateralOffsetM)
        || !finiteValue(input.ringYawRadians)
        || !finiteValue(input.ringWindupRadians)
        || !finiteValue(input.flatSpotDepthM)
        || !finiteValue(input.flatSpotSector)
        || !finiteValue(input.wheelRotationRadians))
    {
        return output;
    }
    for (std::size_t index = 0;
         index < TireFlexibleRingContactCount; ++index)
    {
        const VehicleScalar contact = input.directContactCompressionM[index];
        const VehicleScalar forward =
            input.directContactForwardDisplacementM[index];
        const VehicleScalar down =
            input.directContactDownDisplacementM[index];
        const VehicleScalar lateral =
            input.directContactLateralDisplacementM[index];
        const VehicleScalar vectorMagnitude = std::sqrt(
            forward * forward + down * down + lateral * lateral);
        if (!finiteValue(contact) || contact < 0.0
            || !finiteValue(forward) || !finiteValue(down)
            || !finiteValue(lateral)
            || vectorMagnitude > contact * VehicleScalar{1.01}
                + VehicleScalar{1.0e-7})
        {
            return output;
        }
    }

    const VehicleScalar radiusM = d.unloadedRadiusM;
    const VehicleScalar halfWidthM = d.sectionWidthM * VehicleScalar{0.5};
    const VehicleScalar sidewallHeightM = radiusM - d.rimRadiusM;
    const VehicleScalar deflectionM = input.grounded
        ? std::clamp(input.verticalDeflectionM, VehicleScalar{0.0},
            std::min(d.maximumDeflectionM, sidewallHeightM * VehicleScalar{0.86}))
        : VehicleScalar{0.0};
    const VehicleScalar pressureRatio = std::clamp(
        std::max(input.inflationPressurePa, VehicleScalar{15000.0})
            / d.referencePressurePa,
        VehicleScalar{0.12}, VehicleScalar{3.0});
    const VehicleScalar pressureCompliance = std::clamp(
        VehicleScalar{1.0} / std::sqrt(pressureRatio),
        VehicleScalar{0.58}, VehicleScalar{2.25});

    // Pressure generates hoop tension while the belt plies and carcass carry
    // tensile load.  Their reduced-order result is a radial structural
    // envelope: a normally inflated tire may flatten at the footprint, but it
    // cannot consume the complete sidewall or grow an equal balloon on the
    // unloaded crown.  Lower pressure permits progressively more collapse;
    // over-pressure permits only the small construction strain of a belted
    // tire rather than geometric volume redistribution around the whole ring.
    const VehicleScalar stiffnessRatio = std::clamp(
        d.verticalStiffnessNPerM / VehicleScalar{220000.0},
        VehicleScalar{0.20}, VehicleScalar{5.0});
    const VehicleScalar collapseFraction = std::clamp(
        VehicleScalar{0.52}
            * std::pow(pressureRatio, VehicleScalar{-0.38})
            * std::pow(stiffnessRatio, VehicleScalar{-0.24}),
        VehicleScalar{0.34}, VehicleScalar{0.82});
    const VehicleScalar maximumRadialCompressionM = std::min(
        d.maximumDeflectionM,
        sidewallHeightM * collapseFraction);
    const VehicleScalar maximumCrownExpansionM = std::min(
        sidewallHeightM * VehicleScalar{0.05},
        radiusM * VehicleScalar{0.005}
            * std::max(pressureRatio - VehicleScalar{1.0},
                VehicleScalar{0.0}));

    VehicleScalar maximumDirectM = 0.0;
    for (VehicleScalar contact : input.directContactCompressionM)
        maximumDirectM = std::max(maximumDirectM, contact);
    const bool haveDistributedContact = maximumDirectM > 0.00015;

    const VehicleScalar loadedRadiusM = radiusM - deflectionM;
    const VehicleScalar authoredPatchHalfWidth = std::clamp(
        input.contactPatchWidthM * VehicleScalar{0.5},
        VehicleScalar{0.01}, halfWidthM);

    std::array<VehicleScalar, TireFlexibleRingFieldCount> targetForward{};
    std::array<VehicleScalar, TireFlexibleRingFieldCount> targetDown{};
    std::array<VehicleScalar, TireFlexibleRingFieldCount> targetLateral{};
    std::array<VehicleScalar, TireFlexibleRingFieldCount> constraint{};
    std::array<VehicleScalar, TireFlexibleRingFieldStations> sectionCompression{};
    std::array<VehicleScalar, TireFlexibleRingFieldStations> negativeContact{};
    std::array<VehicleScalar, TireFlexibleRingFieldStations> positiveContact{};

    for (std::size_t station = 0;
         station < TireFlexibleRingFieldStations; ++station)
    {
        const VehicleScalar theta = kTwoPi
            * static_cast<VehicleScalar>(station)
            / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
        const VehicleScalar radialForward = std::cos(theta);
        const VehicleScalar radialDown = std::sin(theta);
        VehicleScalar compressionAccumulator = 0.0;
        VehicleScalar compressionWeight = 0.0;

        for (std::size_t band = 0; band < TireFlexibleRingFieldBands; ++band)
        {
            const std::size_t index = fieldIndex(station, band);
            const VehicleScalar widthCoordinate =
                TireFlexibleRingWidthCoordinates[band];
            const VehicleScalar absoluteWidth = std::abs(widthCoordinate);
            const VehicleScalar directM = sampleDirectContact(
                input.directContactCompressionM, theta, band);
            const VehicleScalar directForwardM = sampleDirectContact(
                input.directContactForwardDisplacementM, theta, band);
            const VehicleScalar directDownM = sampleDirectContact(
                input.directContactDownDisplacementM, theta, band);
            const VehicleScalar directLateralM = sampleDirectContact(
                input.directContactLateralDisplacementM, theta, band);
            const VehicleScalar directSupport = haveDistributedContact
                ? smoothStep(
                    maximumDirectM * VehicleScalar{0.025},
                    maximumDirectM * VehicleScalar{0.30},
                    directM)
                : VehicleScalar{0.0};

            VehicleScalar analyticCompressionM = 0.0;
            if (input.grounded && radialDown > 0.05 && deflectionM > 0.0)
            {
                analyticCompressionM = std::clamp(
                    radiusM - loadedRadiusM / radialDown,
                    VehicleScalar{0.0}, deflectionM);
                const VehicleScalar lateralM = absoluteWidth * halfWidthM;
                const VehicleScalar fallbackWidthSupport = VehicleScalar{1.0}
                    - smoothStep(
                        authoredPatchHalfWidth * VehicleScalar{0.78},
                        std::min(halfWidthM,
                            authoredPatchHalfWidth * VehicleScalar{1.18}
                                + VehicleScalar{0.001}),
                        lateralM);
                analyticCompressionM *= haveDistributedContact
                    ? directSupport : fallbackWidthSupport;
            }

            const VehicleScalar tangentForward = -radialDown;
            const VehicleScalar tangentDown = radialForward;
            const VehicleScalar directRadialCompressionM = std::max(
                -(directForwardM * radialForward
                    + directDownM * radialDown),
                VehicleScalar{0.0});
            const VehicleScalar directTangentialM =
                directForwardM * tangentForward
                + directDownM * tangentDown;
            const VehicleScalar radialCompressionM = std::clamp(
                std::max(analyticCompressionM, directRadialCompressionM),
                VehicleScalar{0.0}, d.maximumDeflectionM);

            targetForward[index] = -radialForward * radialCompressionM
                + tangentForward * directTangentialM;
            targetDown[index] = -radialDown * radialCompressionM
                + tangentDown * directTangentialM;
            targetLateral[index] = directLateralM;
            const VehicleScalar analyticWeight = analyticCompressionM > 1.0e-6
                ? VehicleScalar{0.82} : VehicleScalar{0.0};
            constraint[index] = std::max(directSupport, analyticWeight);

            if (absoluteWidth <= 0.65)
            {
                const VehicleScalar centerWeight = VehicleScalar{1.0}
                    - absoluteWidth * VehicleScalar{0.45};
                compressionAccumulator += radialCompressionM * centerWeight;
                compressionWeight += centerWeight;
            }
            // Only an actual lateral collider-normal component biases the
            // section toward the free side. Shoulder probes hitting a road top
            // therefore cannot be mistaken for a thumb pressing the sidewall
            // inward merely because the probe originated near the shoulder.
            if (widthCoordinate < -0.05)
                negativeContact[station] += std::abs(directLateralM);
            else if (widthCoordinate > 0.05)
                positiveContact[station] += std::abs(directLateralM);
        }
        sectionCompression[station] = compressionAccumulator
            / std::max(compressionWeight, VehicleScalar{1.0});
    }

    // Couple radial shortening to a bounded lateral section expansion inside
    // the same equilibrium problem.  Near-incompressible rubber and contained
    // air move displaced section volume toward both sidewalls; a one-sided
    // obstacle gives the free side more of that displacement.
    std::array<VehicleScalar, TireFlexibleRingFieldStations> smoothedCompression =
        sectionCompression;
    for (int pass = 0; pass < 4; ++pass)
    {
        const auto previous = smoothedCompression;
        for (std::size_t station = 0;
             station < TireFlexibleRingFieldStations; ++station)
        {
            const std::size_t before =
                (station + TireFlexibleRingFieldStations - 1)
                % TireFlexibleRingFieldStations;
            const std::size_t after =
                (station + 1) % TireFlexibleRingFieldStations;
            smoothedCompression[station] =
                previous[before] * VehicleScalar{0.25}
                + previous[station] * VehicleScalar{0.50}
                + previous[after] * VehicleScalar{0.25};
        }
    }
    for (std::size_t station = 0;
         station < TireFlexibleRingFieldStations; ++station)
    {
        const VehicleScalar displacedSectionM = smoothedCompression[station]
            * d.effectivePoissonRatio * VehicleScalar{0.46}
            * pressureCompliance;
        const VehicleScalar contactTotal = negativeContact[station]
            + positiveContact[station];
        const VehicleScalar negativeFreeBias = contactTotal > 1.0e-8
            ? VehicleScalar{0.82}
                + VehicleScalar{0.36} * positiveContact[station] / contactTotal
            : VehicleScalar{1.0};
        const VehicleScalar positiveFreeBias = contactTotal > 1.0e-8
            ? VehicleScalar{0.82}
                + VehicleScalar{0.36} * negativeContact[station] / contactTotal
            : VehicleScalar{1.0};
        for (std::size_t band = 0; band < TireFlexibleRingFieldBands; ++band)
        {
            const VehicleScalar width = TireFlexibleRingWidthCoordinates[band];
            const VehicleScalar absoluteWidth = std::abs(width);
            if (absoluteWidth <= 0.18)
                continue;
            const VehicleScalar sidewallShape = smoothStep(
                VehicleScalar{0.18}, VehicleScalar{0.86}, absoluteWidth);
            const VehicleScalar freeBias = width < 0.0
                ? negativeFreeBias : positiveFreeBias;
            targetLateral[fieldIndex(station, band)] += std::copysign(
                std::min(displacedSectionM * sidewallShape * freeBias,
                    halfWidthM * VehicleScalar{0.20}),
                width);
            if (displacedSectionM > 1.0e-7)
                constraint[fieldIndex(station, band)] = std::max(
                    constraint[fieldIndex(station, band)], VehicleScalar{0.38});
        }
    }

    // Flat-spot geometry enters the same target field.  It is material-fixed,
    // transformed to the current world-anchored ring angle before solving.
    if (input.flatSpotDepthM > 1.0e-7)
    {
        VehicleScalar wrappedSector = std::fmod(
            input.flatSpotSector, VehicleScalar{16.0});
        if (wrappedSector < 0.0)
            wrappedSector += VehicleScalar{16.0};
        const VehicleScalar sectorAngle = kTwoPi
            * wrappedSector / VehicleScalar{16.0};
        const VehicleScalar flatTheta = wrappedAngle(
            VehicleScalar{0.5} * kPi + sectorAngle + input.wheelRotationRadians);
        for (std::size_t station = 0;
             station < TireFlexibleRingFieldStations; ++station)
        {
            const VehicleScalar theta = kTwoPi
                * static_cast<VehicleScalar>(station)
                / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
            const VehicleScalar angularMask = smoothStep(
                std::cos(kPi / VehicleScalar{7.0}), VehicleScalar{1.0},
                std::cos(theta - flatTheta));
            for (std::size_t band = 0; band < TireFlexibleRingFieldBands; ++band)
            {
                const VehicleScalar treadMask = VehicleScalar{1.0}
                    - smoothStep(VehicleScalar{0.68}, VehicleScalar{0.98},
                        std::abs(TireFlexibleRingWidthCoordinates[band]));
                const VehicleScalar depth = std::max(input.flatSpotDepthM,
                    VehicleScalar{0.0}) * angularMask * treadMask;
                const std::size_t index = fieldIndex(station, band);
                targetForward[index] -= std::cos(theta) * depth;
                targetDown[index] -= std::sin(theta) * depth;
                constraint[index] = std::max(
                    constraint[index], angularMask * treadMask);
            }
        }
    }

    solveElasticFoundation(d, targetForward, constraint, pressureRatio,
        output.forwardDisplacementM);
    solveElasticFoundation(d, targetDown, constraint, pressureRatio,
        output.downDisplacementM);
    solveElasticFoundation(d, targetLateral, constraint, pressureRatio,
        output.lateralDisplacementM);

    const VehicleScalar yaw = std::clamp(
        input.ringYawRadians, VehicleScalar{-0.35}, VehicleScalar{0.35});
    const VehicleScalar windup = std::clamp(
        input.ringWindupRadians, VehicleScalar{-0.35}, VehicleScalar{0.35});
    VehicleScalar maximumDisplacementM = 0.0;
    for (std::size_t station = 0;
         station < TireFlexibleRingFieldStations; ++station)
    {
        const VehicleScalar theta = kTwoPi
            * static_cast<VehicleScalar>(station)
            / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
        const VehicleScalar radialForward = std::cos(theta);
        const VehicleScalar radialDown = std::sin(theta);
        const VehicleScalar tangentForward = -radialDown;
        const VehicleScalar tangentDown = radialForward;
        for (std::size_t band = 0; band < TireFlexibleRingFieldBands; ++band)
        {
            const std::size_t index = fieldIndex(station, band);
            const VehicleScalar lateralPositionM = halfWidthM
                * TireFlexibleRingWidthCoordinates[band];
            const VehicleScalar forwardPositionM = radiusM * radialForward;

            // Non-radial rigid-ring modes are assembled here, before
            // presentation. The radial road-envelope state is intentionally
            // absent: direct contact already owns radial shape, and applying
            // road height again as a whole-belt translation violates carcass
            // tension and bead/rim clearance.
            output.forwardDisplacementM[index] +=
                input.ringLongitudinalOffsetM
                + tangentForward * radiusM * windup
                + lateralPositionM * yaw;
            output.downDisplacementM[index] +=
                tangentDown * radiusM * windup;
            output.lateralDisplacementM[index] +=
                input.ringLateralOffsetM - forwardPositionM * yaw;

            const VehicleScalar radialLimit = std::min(
                d.maximumDeflectionM * VehicleScalar{1.15},
                sidewallHeightM * VehicleScalar{0.88});
            const VehicleScalar lateralLimit = halfWidthM * VehicleScalar{0.45};

            // Preserve tangential belt motion, but project the radial part
            // into the pressure/carcass envelope. This is the reduced-order
            // equivalent of hoop tensile strength and bead restraint. At
            // reference pressure it leaves roughly half the sidewall height
            // as clearance even under the largest authored contact demand.
            VehicleScalar radialDisplacementM =
                output.forwardDisplacementM[index] * radialForward
                + output.downDisplacementM[index] * radialDown;
            VehicleScalar tangentialDisplacementM =
                output.forwardDisplacementM[index] * tangentForward
                + output.downDisplacementM[index] * tangentDown;
            radialDisplacementM = std::clamp(
                radialDisplacementM,
                -maximumRadialCompressionM,
                maximumCrownExpansionM);
            tangentialDisplacementM = std::clamp(
                tangentialDisplacementM, -radialLimit, radialLimit);
            output.forwardDisplacementM[index] =
                radialForward * radialDisplacementM
                + tangentForward * tangentialDisplacementM;
            output.downDisplacementM[index] =
                radialDown * radialDisplacementM
                + tangentDown * tangentialDisplacementM;
            output.lateralDisplacementM[index] = std::clamp(
                output.lateralDisplacementM[index], -lateralLimit, lateralLimit);
            maximumDisplacementM = std::max(maximumDisplacementM,
                std::max(std::abs(output.forwardDisplacementM[index]),
                    std::max(std::abs(output.downDisplacementM[index]),
                        std::abs(output.lateralDisplacementM[index]))));
        }
    }

    output.valid = maximumDisplacementM > 1.0e-7;
    return output;
}

} // namespace heritage::vehicles::tires
