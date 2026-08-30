#include "TireFlexibleRingField.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

struct TireCorneringPresentationAuthority
{
    VehicleScalar lateralDeflection = 0.0;
    VehicleScalar torsion = 0.0;
};

TireCorneringPresentationAuthority corneringPresentationAuthority(
    const TireFlexibleRingFieldDescription& description,
    const TireFlexibleRingDynamicsInput& input)
{
    TireCorneringPresentationAuthority result;
    if (!input.grounded)
        return result;

    // TIRE45J: visual carcass side-bend/torsion follows actual tire physics,
    // not the mere existence of a structural DOF. One-substep-old Fy/Mz/slip
    // is deliberate: the carcass solve runs before the current MF6.2 force
    // evaluation in the 1 kHz wheel phase, and a 1 ms delay is negligible.
    // Low-speed parking/contact-patch torsion is faded out; a real curb-side
    // contact bypasses this authority entirely through unilateral constraints.
    const VehicleScalar loadN = std::max(
        std::abs(input.normalLoadN), VehicleScalar{250.0});
    const VehicleScalar speedAuthority = smoothStep(
        VehicleScalar{0.75}, VehicleScalar{3.0},
        std::abs(input.forwardSpeedMps));
    const VehicleScalar lateralLoadRatio =
        std::abs(input.lateralForceN) / loadN;
    const VehicleScalar forceAuthority = smoothStep(
        VehicleScalar{0.025}, VehicleScalar{0.18}, lateralLoadRatio);
    const VehicleScalar slipAuthority = smoothStep(
        VehicleScalar{0.35} * kPi / VehicleScalar{180.0},
        VehicleScalar{2.0} * kPi / VehicleScalar{180.0},
        std::abs(input.slipAngleRadians));
    const VehicleScalar momentReferenceNm = loadN * std::max(
        description.unloadedRadiusM, VehicleScalar{0.10});
    const VehicleScalar normalizedAligningMoment =
        std::abs(input.aligningMomentNm)
        / std::max(momentReferenceNm, VehicleScalar{1.0});
    const VehicleScalar momentAuthority = smoothStep(
        VehicleScalar{0.003}, VehicleScalar{0.030},
        normalizedAligningMoment);

    // A measured lateral force is already physical authority and must not
    // disappear merely because the car is negotiating a very slow corner.
    // Slip-angle-only and aligning-moment-only presentation remain speed-gated
    // so near-standstill kinematic/noise angles cannot resurrect the old parked
    // twist artefact.
    result.lateralDeflection = std::max(
        forceAuthority, speedAuthority * slipAuthority);
    result.torsion = std::max(
        forceAuthority,
        speedAuthority * std::max(slipAuthority, momentAuthority));
    return result;
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
        && finiteValue(d.authoredShapePressurePa)
        && d.authoredShapePressurePa > d.referencePressurePa
        && d.authoredShapePressurePa <= 2000000.0
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
        || !finiteValue(input.contactPatchTwistRadians)
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
        VehicleScalar{0.12}, VehicleScalar{5.0});
    const VehicleScalar pressureCompliance = std::clamp(
        std::pow(pressureRatio, VehicleScalar{-0.68}),
        VehicleScalar{0.38}, VehicleScalar{2.75});
    // Below roughly half the tire's identified pressure the contained air no
    // longer carries the section in the ordinary inflated-torus mode.  The
    // tread belt still spans the footprint while the sidewalls fold outward.
    // Blend this regime away completely before normal road pressures so the
    // validated milestone shape above about one bar remains unchanged.
    const VehicleScalar deflatedRegime = input.grounded
        ? VehicleScalar{1.0} - smoothStep(
            VehicleScalar{0.10}, VehicleScalar{0.48}, pressureRatio)
        : VehicleScalar{0.0};

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
    const VehicleScalar structuralCompliance = std::clamp(
        pressureCompliance
            * std::pow(stiffnessRatio, VehicleScalar{-0.24}),
        VehicleScalar{0.38}, VehicleScalar{2.80});

    // The rim radius is the bead-seat datum. A passenger-car J flange extends
    // beyond that seat, and the inflated bead/sidewall must retain clearance
    // outside it. This reduced-order envelope is proportional for kart,
    // low-profile, road and tall off-road tires instead of assuming that every
    // tire has the prototype's 80 mm of usable radial travel.
    const VehicleScalar flangeHeightM = std::clamp(
        sidewallHeightM * VehicleScalar{0.15},
        VehicleScalar{0.006}, VehicleScalar{0.018});
    const VehicleScalar flangeClearanceM = std::clamp(
        sidewallHeightM * VehicleScalar{0.12},
        VehicleScalar{0.005}, VehicleScalar{0.014});
    const VehicleScalar minimumCarcassRadiusM = std::min(
        radiusM - VehicleScalar{0.004},
        d.rimRadiusM + flangeHeightM + flangeClearanceM);
    const VehicleScalar profileRatio = sidewallHeightM
        / std::max(d.sectionWidthM, VehicleScalar{0.03});
    const VehicleScalar profileCompliance = std::clamp(
        profileRatio / VehicleScalar{0.40},
        VehicleScalar{0.42}, VehicleScalar{2.10});
    const VehicleScalar collapseFraction = std::clamp(
        (VehicleScalar{0.34}
            + VehicleScalar{0.12}
                * std::max(structuralCompliance - VehicleScalar{1.0},
                    VehicleScalar{0.0}))
            * std::pow(profileCompliance, VehicleScalar{0.18})
            * std::pow(stiffnessRatio, VehicleScalar{-0.24}),
        VehicleScalar{0.24}, VehicleScalar{0.72});
    const VehicleScalar maximumRadialCompressionM = std::min(
        std::min(d.maximumDeflectionM, sidewallHeightM * collapseFraction),
        radiusM - minimumCarcassRadiusM);
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
    std::array<VehicleScalar, TireFlexibleRingFieldCount> directSupportField{};
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
            // Collision penetration is already a geometric boundary
            // condition: it is the exact distance required to return the
            // authored tread surface to the collider. Scaling that distance
            // by low-pressure compliance makes the tread travel beyond a flat
            // road plane and produces a false concave arch at zero pressure.
            // Pressure belongs in the foundation, carcass envelope and
            // sidewall-volume coupling below; it must not be applied a second
            // time to the contact correction itself.
            const VehicleScalar directSupport = haveDistributedContact
                ? smoothStep(
                    maximumDirectM * VehicleScalar{0.025},
                    maximumDirectM * VehicleScalar{0.30},
                    directM)
                : VehicleScalar{0.0};
            directSupportField[index] = directSupport;

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
            * d.effectivePoissonRatio * VehicleScalar{0.68}
            * structuralCompliance * profileCompliance
            * (VehicleScalar{1.0}
                + deflatedRegime * VehicleScalar{0.58});
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
                VehicleScalar{0.18}
                    - deflatedRegime * VehicleScalar{0.10},
                VehicleScalar{0.86}, absoluteWidth);
            const VehicleScalar freeBias = width < 0.0
                ? negativeFreeBias : positiveFreeBias;
            targetLateral[fieldIndex(station, band)] += std::copysign(
                std::min(displacedSectionM * sidewallShape * freeBias,
                    halfWidthM * (VehicleScalar{0.30}
                        + deflatedRegime * VehicleScalar{0.15})),
                width);
            if (displacedSectionM > 1.0e-7)
                constraint[fieldIndex(station, band)] = std::max(
                    constraint[fieldIndex(station, band)],
                    VehicleScalar{1.10}
                        + deflatedRegime * VehicleScalar{0.75});
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

    // In the deflated regime the steel tread belt lies on the road; it does
    // not behave like a pressure-softened foam ring. The elastic solve above
    // supplies smooth shoulders and sidewalls, then this bounded projection
    // restores the collision boundary across the supported tread. Because the
    // direct samples include the authored crown, their final positions share
    // the same support plane rather than leaving the centre as a shallow arch.
    if (deflatedRegime > 1.0e-6)
    {
        for (std::size_t station = 0;
             station < TireFlexibleRingFieldStations; ++station)
        {
            const VehicleScalar theta = kTwoPi
                * static_cast<VehicleScalar>(station)
                / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
            const VehicleScalar lowerTread = smoothStep(
                VehicleScalar{0.20}, VehicleScalar{0.92}, std::sin(theta));
            for (std::size_t band = 0;
                 band < TireFlexibleRingFieldBands; ++band)
            {
                const std::size_t index = fieldIndex(station, band);
                const VehicleScalar tread = VehicleScalar{1.0} - smoothStep(
                    VehicleScalar{0.64}, VehicleScalar{0.88},
                    std::abs(TireFlexibleRingWidthCoordinates[band]));
                const VehicleScalar support = smoothStep(
                    VehicleScalar{0.12}, VehicleScalar{0.78},
                    directSupportField[index]);
                const VehicleScalar projection = std::clamp(
                    deflatedRegime * lowerTread * tread * support
                        * VehicleScalar{0.96},
                    VehicleScalar{0.0}, VehicleScalar{0.96});
                output.forwardDisplacementM[index] +=
                    (targetForward[index] - output.forwardDisplacementM[index])
                        * projection;
                output.downDisplacementM[index] +=
                    (targetDown[index] - output.downDisplacementM[index])
                        * projection;
            }
        }
    }

    const VehicleScalar yaw = std::clamp(
        input.ringYawRadians, VehicleScalar{-0.35}, VehicleScalar{0.35});
    const VehicleScalar windup = std::clamp(
        input.ringWindupRadians, VehicleScalar{-0.35}, VehicleScalar{0.35});
    const VehicleScalar contactTwist = std::clamp(
        input.contactPatchTwistRadians,
        VehicleScalar{-0.35}, VehicleScalar{0.35});
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

            // The rigid-ring translation is the physical belt mode. On a
            // grounded tire the footprint restrains the lower belt while the
            // rim carries the vehicle laterally, creating the familiar curved
            // carcass rather than translating every section equally. Preserve
            // a modest crown response and concentrate displacement smoothly in
            // the loaded lower half. Tall sidewalls have more bending travel;
            // a low-profile sports tire remains visibly and physically stiffer.
            const VehicleScalar lowerHemisphere = input.grounded
                ? smoothStep(VehicleScalar{-0.15}, VehicleScalar{0.98}, radialDown)
                : VehicleScalar{0.0};
            const VehicleScalar lateralBendingScale = input.grounded
                ? VehicleScalar{0.55}
                    + lowerHemisphere * VehicleScalar{1.25}
                        * std::clamp(
                            profileCompliance * structuralCompliance,
                            VehicleScalar{0.45}, VehicleScalar{1.80})
                : VehicleScalar{1.0};
            const VehicleScalar footprintTwist = contactTwist
                * lowerHemisphere * lowerHemisphere;
            const VehicleScalar localYaw = yaw + footprintTwist;

            // Non-radial rigid-ring modes are assembled here, before
            // presentation. The radial road-envelope state is intentionally
            // absent: direct contact already owns radial shape, and applying
            // road height again as a whole-belt translation violates carcass
            // tension and bead/rim clearance.
            output.forwardDisplacementM[index] +=
                input.ringLongitudinalOffsetM
                + tangentForward * radiusM * windup
                + lateralPositionM * localYaw;
            output.downDisplacementM[index] +=
                tangentDown * radiusM * windup;
            output.lateralDisplacementM[index] +=
                input.ringLateralOffsetM * lateralBendingScale
                - forwardPositionM * localYaw;

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
            if (radialDisplacementM < 0.0)
            {
                // Progressive bead/flange bottoming. The asymptote avoids a
                // hard flat clamp while guaranteeing that the carcass cannot
                // be visually crushed through the protected rim envelope.
                const VehicleScalar compression = -radialDisplacementM;
                radialDisplacementM = -maximumRadialCompressionM
                    * std::tanh(compression
                        / std::max(maximumRadialCompressionM, kEpsilon));
            }
            else
            {
                radialDisplacementM = std::min(
                    radialDisplacementM, maximumCrownExpansionM);
            }
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

            // All imported tire meshes use the 150 PSI shape convention.
            // Normalize the solved displacement so the identified/reference
            // pressure remains exactly unchanged, then fade continuously to
            // the undeformed authored mesh at 150 PSI. At zero pressure the
            // modest cap leaves the dedicated flat-carcass regime in charge
            // rather than exaggerating it without bound.
            const VehicleScalar authoredShapeScale = std::clamp(
                (d.authoredShapePressurePa - input.inflationPressurePa)
                    / std::max(
                        d.authoredShapePressurePa - d.referencePressurePa,
                        kEpsilon),
                VehicleScalar{0.0}, VehicleScalar{1.15});
            output.forwardDisplacementM[index] *= authoredShapeScale;
            output.downDisplacementM[index] *= authoredShapeScale;
            output.lateralDisplacementM[index] *= authoredShapeScale;
            maximumDisplacementM = std::max(maximumDisplacementM,
                std::max(std::abs(output.forwardDisplacementM[index]),
                    std::max(std::abs(output.downDisplacementM[index]),
                        std::abs(output.lateralDisplacementM[index]))));
        }
    }

    output.valid = maximumDisplacementM > 1.0e-7;
    return output;
}


namespace {

struct CarcassVec3
{
    VehicleScalar x = 0.0;
    VehicleScalar y = 0.0;
    VehicleScalar z = 0.0;
};

CarcassVec3 addCarcass(const CarcassVec3& a, const CarcassVec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

CarcassVec3 subtractCarcass(const CarcassVec3& a, const CarcassVec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

CarcassVec3 scaleCarcass(const CarcassVec3& value, VehicleScalar scale)
{
    return { value.x * scale, value.y * scale, value.z * scale };
}

VehicleScalar dotCarcass(const CarcassVec3& a, const CarcassVec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

VehicleScalar lengthCarcass(const CarcassVec3& value)
{
    return std::sqrt(std::max(dotCarcass(value, value), VehicleScalar{0.0}));
}

CarcassVec3 normalizeCarcass(const CarcassVec3& value, const CarcassVec3& fallback)
{
    const VehicleScalar magnitude = lengthCarcass(value);
    if (magnitude <= VehicleScalar{1.0e-9})
        return fallback;
    return scaleCarcass(value, VehicleScalar{1.0} / magnitude);
}

CarcassVec3 stateDisplacement(
    const TireFlexibleRingDynamicState& state,
    std::size_t index)
{
    return {
        state.forwardDisplacementM[index],
        state.downDisplacementM[index],
        state.lateralDisplacementM[index]
    };
}

CarcassVec3 stateVelocity(
    const TireFlexibleRingDynamicState& state,
    std::size_t index)
{
    return {
        state.forwardVelocityMps[index],
        state.downVelocityMps[index],
        state.lateralVelocityMps[index]
    };
}

void writeStateDisplacement(
    TireFlexibleRingDynamicState& state,
    std::size_t index,
    const CarcassVec3& value)
{
    state.forwardDisplacementM[index] = value.x;
    state.downDisplacementM[index] = value.y;
    state.lateralDisplacementM[index] = value.z;
}

void writeStateVelocity(
    TireFlexibleRingDynamicState& state,
    std::size_t index,
    const CarcassVec3& value)
{
    state.forwardVelocityMps[index] = value.x;
    state.downVelocityMps[index] = value.y;
    state.lateralVelocityMps[index] = value.z;
}

CarcassVec3 restCarcassPosition(
    const TireFlexibleRingFieldDescription& description,
    std::size_t station,
    std::size_t band)
{
    const VehicleScalar theta = kTwoPi
        * static_cast<VehicleScalar>(station)
        / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
    const VehicleScalar width = TireFlexibleRingWidthCoordinates[band];
    const VehicleScalar absoluteWidth = std::abs(width);

    // TIRE45I: the structural rest cross-section must match the broad authored
    // tread that receives these displacement values in the renderer.  The old
    // historical 0.055*w^2 profile rounded the ENTIRE 13-band section.  On a
    // flat road that meant the centre tread needed the full radial correction
    // while the +/-0.65 and +/-0.82 bands needed progressively less.  Those
    // perfectly valid STRUCTURAL displacements were then applied to an authored
    // GLB tread that is already broad/flat, manufacturing a cross-width wedge
    // that looks like the tire is folding inward even with zero lateral motion.
    //
    // Keep full unloaded radius through 80% of half-width and retain curvature
    // only in the outer shoulder/sidewall region.  Real curb/sidewall contacts
    // still act through the exact 3-D road constraints and remain asymmetric.
    const VehicleScalar shoulder = smoothStep(
        VehicleScalar{0.80}, VehicleScalar{1.0}, absoluteWidth);
    const VehicleScalar radialScale = VehicleScalar{1.0}
        - VehicleScalar{0.055} * shoulder * shoulder;
    const VehicleScalar lateralScale = VehicleScalar{0.94};
    return {
        description.unloadedRadiusM * radialScale * std::cos(theta),
        description.unloadedRadiusM * radialScale * std::sin(theta),
        description.sectionWidthM * VehicleScalar{0.5}
            * lateralScale * width
    };
}

CarcassVec3 ringAnchorDisplacement(
    const TireFlexibleRingFieldDescription& description,
    const TireFlexibleRingDynamicsInput& input,
    const TireFlexibleRingDevelopmentTuning& tuning,
    const std::array<VehicleScalar, TireFlexibleRingFieldStations>& radialCompression,
    std::size_t station,
    std::size_t band)
{
    const VehicleScalar theta = kTwoPi
        * static_cast<VehicleScalar>(station)
        / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
    const VehicleScalar radialForward = std::cos(theta);
    const VehicleScalar radialDown = std::sin(theta);
    const VehicleScalar tangentForward = -radialDown;
    const VehicleScalar tangentDown = radialForward;
    const VehicleScalar width = TireFlexibleRingWidthCoordinates[band];
    const VehicleScalar absoluteWidth = std::abs(width);
    const VehicleScalar halfWidth = description.sectionWidthM * VehicleScalar{0.5};
    const VehicleScalar lateralPositionM = halfWidth * width;
    const VehicleScalar forwardPositionM = description.unloadedRadiusM * radialForward;

    const VehicleScalar lowerHemisphere = input.grounded
        ? smoothStep(VehicleScalar{-0.10}, VehicleScalar{0.98}, radialDown)
            * tuning.lowerHemisphereAnchorScale
        : VehicleScalar{0.0};
    const TireCorneringPresentationAuthority cornering =
        corneringPresentationAuthority(description, input);

    // TIRE45J: restore the two physically useful cornering modes that the
    // earlier quarantine intentionally removed while chasing the straight-line
    // artefact. Rigid-ring yaw gives broad carcass deflection; footprint twist
    // gives the lower-tread torsion visible in real cornering. Both are gated by
    // real Fy/Mz/slip and capped before they enter geometry, so parking-state
    // +/-7 degree torsion or tiny straight-line ring yaw cannot fold the tire.
    const VehicleScalar corneringRingYaw = std::clamp(
        input.ringYawRadians * tuning.yawAnchorScale,
        VehicleScalar{-0.0872664626}, VehicleScalar{0.0872664626}); // +/-5 deg
    const VehicleScalar corneringPatchTwist = std::clamp(
        input.contactPatchTwistRadians * tuning.contactTwistAnchorScale,
        VehicleScalar{-0.0698131701}, VehicleScalar{0.0698131701}); // +/-4 deg
    const VehicleScalar localYaw = std::clamp(
        cornering.torsion
            * (corneringRingYaw
                + corneringPatchTwist
                    * lowerHemisphere * lowerHemisphere),
        VehicleScalar{-0.10}, VehicleScalar{0.10});
    const VehicleScalar windup = std::clamp(
        input.ringWindupRadians * tuning.windupAnchorScale,
        VehicleScalar{-0.35}, VehicleScalar{0.35});

    CarcassVec3 anchor{
        input.ringLongitudinalOffsetM * tuning.longitudinalAnchorScale
            + tangentForward * description.unloadedRadiusM * windup
            + lateralPositionM * localYaw,
        tangentDown * description.unloadedRadiusM * windup,
        input.ringLateralOffsetM * tuning.lateralAnchorScale
            * cornering.lateralDeflection
            - forwardPositionM * localYaw
    };

    // Optional development-only near-incompressible section heuristic. TIRE45E
    // disables it in production because pure flat-road radial compression was
    // creating several millimetres of symmetric lateral sidewall motion. That
    // looked exactly like the residual inward lean from the rear despite zero
    // camber. Genuine lateral deformation remains physics-owned: MF lateral
    // force moves/yaws the rigid ring, and curb/side contacts enter below as
    // unilateral constraints with their real lateral collision normals.
    if (absoluteWidth > VehicleScalar{0.18})
    {
        const VehicleScalar sidewallShape = smoothStep(
            VehicleScalar{0.18}, VehicleScalar{0.90}, absoluteWidth);
        const VehicleScalar outward = radialCompression[station]
            * description.effectivePoissonRatio
            * tuning.poissonBulgeScale
            * VehicleScalar{0.72} * sidewallShape;
        anchor.z += std::copysign(outward, width);
    }

    if (input.flatSpotDepthM > VehicleScalar{1.0e-7})
    {
        VehicleScalar wrappedSector = std::fmod(
            input.flatSpotSector, VehicleScalar{16.0});
        if (wrappedSector < 0.0)
            wrappedSector += VehicleScalar{16.0};
        const VehicleScalar sectorAngle = kTwoPi
            * wrappedSector / VehicleScalar{16.0};
        const VehicleScalar flatTheta = wrappedAngle(
            VehicleScalar{0.5} * kPi + sectorAngle + input.wheelRotationRadians);
        const VehicleScalar angularMask = smoothStep(
            std::cos(kPi / VehicleScalar{7.0}), VehicleScalar{1.0},
            std::cos(theta - flatTheta));
        const VehicleScalar treadMask = VehicleScalar{1.0}
            - smoothStep(VehicleScalar{0.68}, VehicleScalar{0.98}, absoluteWidth);
        const VehicleScalar depth = std::max(
            input.flatSpotDepthM * tuning.flatSpotScale, VehicleScalar{0.0})
            * angularMask * treadMask;
        anchor.x -= radialForward * depth;
        anchor.y -= radialDown * depth;
    }

    return anchor;
}

CarcassVec3 solveRankOneContact(
    VehicleScalar baseDiagonal,
    const CarcassVec3& rhs,
    VehicleScalar contactStiffness,
    const CarcassVec3& unitNormal,
    VehicleScalar contactTarget)
{
    // Solve (aI + k nn^T)x = rhs + k*c*n analytically.  This keeps the
    // unilateral road/rim constraint inside the implicit structural solve and
    // avoids an after-the-fact vertex clamp.
    const VehicleScalar a = std::max(baseDiagonal, VehicleScalar{1.0e-9});
    if (contactStiffness <= VehicleScalar{0.0})
        return scaleCarcass(rhs, VehicleScalar{1.0} / a);

    const CarcassVec3 b = addCarcass(
        rhs, scaleCarcass(unitNormal, contactStiffness * contactTarget));
    const VehicleScalar inverseA = VehicleScalar{1.0} / a;
    const VehicleScalar correction = contactStiffness
        / (a * (a + contactStiffness));
    return subtractCarcass(
        scaleCarcass(b, inverseA),
        scaleCarcass(unitNormal, correction * dotCarcass(unitNormal, b)));
}

} // namespace

TireFlexibleRingFieldOutput advanceTireFlexibleRingDynamics(
    const TireFlexibleRingFieldDescription& d,
    const TireFlexibleRingDynamicsInput& input,
    TireFlexibleRingDynamicState& state)
{
    TireFlexibleRingFieldOutput output;
    if (!validTireFlexibleRingFieldDescription(d)
        || !finiteValue(input.deltaTimeSeconds)
        || input.deltaTimeSeconds <= 0.0
        || !finiteValue(input.inflationPressurePa)
        || !finiteValue(input.thermalStiffnessScale)
        || !finiteValue(input.normalLoadN)
        || !finiteValue(input.lateralForceN)
        || !finiteValue(input.aligningMomentNm)
        || !finiteValue(input.slipAngleRadians)
        || !finiteValue(input.forwardSpeedMps)
        || !finiteValue(input.ringLongitudinalOffsetM)
        || !finiteValue(input.ringLateralOffsetM)
        || !finiteValue(input.ringYawRadians)
        || !finiteValue(input.ringWindupRadians)
        || !finiteValue(input.contactPatchTwistRadians))
    {
        return output;
    }

    if (!state.initialized)
    {
        state = {};
        state.initialized = true;
    }

    static const TireFlexibleRingDevelopmentTuning kDefaultDevelopmentTuning{};
    const TireFlexibleRingDevelopmentTuning& tuning =
        input.developmentTuning != nullptr && input.developmentTuning->enabled
            ? *input.developmentTuning
            : kDefaultDevelopmentTuning;

    // The carcass has its own structural rate. VehicleSystem normally calls
    // this at 125 Hz from the 1 kHz wheel loop; clamp debugger stalls rather
    // than injecting one enormous energy step.
    const VehicleScalar dt = std::clamp(
        input.deltaTimeSeconds, VehicleScalar{0.001}, VehicleScalar{0.016});
    const VehicleScalar sidewallHeightM = d.unloadedRadiusM - d.rimRadiusM;
    const VehicleScalar pressureRatio = std::clamp(
        std::max(input.inflationPressurePa, VehicleScalar{0.0})
            / std::max(d.referencePressurePa, VehicleScalar{20000.0}),
        VehicleScalar{0.0}, VehicleScalar{5.0});
    const VehicleScalar thermalScale = std::clamp(
        VehicleScalar{1.0}
            + (input.thermalStiffnessScale - VehicleScalar{1.0})
                * tuning.thermalInfluence,
        VehicleScalar{0.25}, VehicleScalar{2.50});
    const VehicleScalar pneumaticMinimum = std::clamp(
        tuning.pneumaticMinimumScale,
        VehicleScalar{0.005}, VehicleScalar{2.0});
    const VehicleScalar pneumaticMaximum = std::max(
        pneumaticMinimum,
        std::clamp(tuning.pneumaticMaximumScale,
            VehicleScalar{0.05}, VehicleScalar{8.0}));
    const VehicleScalar pneumaticStiffnessScale = std::clamp(
        std::pow(std::max(pressureRatio, VehicleScalar{0.0}),
            std::clamp(tuning.pressureExponent,
                VehicleScalar{0.0}, VehicleScalar{2.0})) * thermalScale,
        pneumaticMinimum,
        pneumaticMaximum);

    // Six kilograms is the current road-tire belt dataset value.  If no rigid
    // ring metadata is available to this structural layer, distribute a
    // conservative effective carcass/belt mass across all controls.  The mass
    // term is what makes this a time-domain simulation rather than the old
    // instantaneous equilibrium approximation.
    const VehicleScalar effectiveCarcassMassKg = std::clamp(
        d.sectionWidthM / VehicleScalar{0.205} * VehicleScalar{6.0}
            * std::clamp(tuning.effectiveMassScale,
                VehicleScalar{0.05}, VehicleScalar{20.0}),
        VehicleScalar{0.20}, VehicleScalar{80.0});
    const VehicleScalar nodeMassKg = effectiveCarcassMassKg
        / static_cast<VehicleScalar>(TireFlexibleRingFieldCount);

    // Calibrate the aggregate radial support around the authored Kz rather
    // than prescribing the tire deflection itself. Contact geometry determines
    // where the carcass meets the road; this stiffness determines how strongly
    // the rim/belt structure resists that deformation.
    const VehicleScalar foundationPerNode = d.verticalStiffnessNPerM
        / static_cast<VehicleScalar>(TireFlexibleRingFieldBands * 4)
        * pneumaticStiffnessScale
        * std::clamp(tuning.foundationScale,
            VehicleScalar{0.0}, VehicleScalar{20.0});
    const VehicleScalar circumferentialK = foundationPerNode
        * d.circumferentialCoupling * VehicleScalar{0.42}
        * std::clamp(tuning.circumferentialScale,
            VehicleScalar{0.0}, VehicleScalar{20.0});
    const VehicleScalar lateralK = foundationPerNode
        * d.lateralCoupling * VehicleScalar{0.34}
        * std::clamp(tuning.lateralScale,
            VehicleScalar{0.0}, VehicleScalar{20.0});
    const VehicleScalar secondNeighborK = circumferentialK
        * VehicleScalar{0.18}
        * std::clamp(tuning.secondNeighborScale,
            VehicleScalar{0.0}, VehicleScalar{20.0});
    const VehicleScalar contactK = d.verticalStiffnessNPerM
        / static_cast<VehicleScalar>(TireFlexibleRingFieldBands)
        * std::clamp(d.contactConstraintStiffness / VehicleScalar{2.0},
            VehicleScalar{6.0}, VehicleScalar{32.0})
        * std::clamp(tuning.contactScale,
            VehicleScalar{0.0}, VehicleScalar{40.0});
    const VehicleScalar rimContactK = contactK * VehicleScalar{2.5}
        * std::clamp(tuning.rimContactScale,
            VehicleScalar{0.0}, VehicleScalar{40.0});

    // Critical-ish damping for the local foundation plus a small residual
    // structural term.  Contact damping is supplied by the implicit solve and
    // the velocity update below rather than a second geometry correction.
    const VehicleScalar localOmega = std::sqrt(
        std::max(foundationPerNode / std::max(nodeMassKg, kEpsilon),
            VehicleScalar{0.0}));
    const VehicleScalar dampingNsPerM = VehicleScalar{0.72}
        * VehicleScalar{2.0} * nodeMassKg * localOmega
        * std::clamp(tuning.dampingScale,
            VehicleScalar{0.0}, VehicleScalar{10.0});

    // TIRE45H — separate top-support geometry from true lateral contact.
    // The runtime road-envelope samples are cast along the suspension/support
    // direction. A mildly banked/sloped top face is therefore not sidewall
    // collision authority. Only a steep shoulder sample near the tire edge, or
    // an actual rigid-ring lateral mode, may keep lateral carcass state alive.
    // This also guarantees that old turn/curb deformation cannot remain as an
    // unexplained inward lean after the wheel returns to neutral straight-line
    // rolling.
    bool genuineSideContact = false;
    bool ordinaryTopContact = input.grounded;
    VehicleScalar centerRoadOverlapM = 0.0;
    bool haveCenterRoadOverlap = false;
    const std::size_t inputRoadSampleCount = std::min(
        input.roadSampleCount, TireFlexibleRingMaximumRoadSamples);
    const VehicleScalar sideQueryThresholdM = std::max(
        d.sectionWidthM * VehicleScalar{0.20}, VehicleScalar{0.025});
    for (std::size_t sampleIndex = 0;
         sampleIndex < inputRoadSampleCount; ++sampleIndex)
    {
        const auto& sample = input.roadSamples[sampleIndex];
        if (!sample.queried || !sample.supported)
            continue;
        const VehicleScalar absLateralNormal = std::abs(sample.normalLateral);
        const VehicleScalar absDownNormal = std::abs(sample.normalDown);
        if (std::abs(sample.queryLateralM) >= sideQueryThresholdM
            && absLateralNormal >= VehicleScalar{0.30}
            && absDownNormal <= VehicleScalar{0.90})
        {
            genuineSideContact = true;
        }
        if (sample.normalDown > VehicleScalar{-0.65})
            ordinaryTopContact = false;
        if (std::abs(sample.queryForwardM) <= VehicleScalar{1.0e-5}
            && std::abs(sample.queryLateralM) <= VehicleScalar{1.0e-5})
        {
            centerRoadOverlapM = std::clamp(
                d.unloadedRadiusM - sample.pointDownM,
                VehicleScalar{0.0}, d.maximumDeflectionM);
            haveCenterRoadOverlap = true;
        }
    }

    const TireCorneringPresentationAuthority cornering =
        corneringPresentationAuthority(d, input);
    const bool physicalCorneringAuthority =
        cornering.lateralDeflection >= VehicleScalar{0.01}
        || cornering.torsion >= VehicleScalar{0.01};
    const bool lateralAuthority =
        genuineSideContact || physicalCorneringAuthority;

    // TIRE45K: Fy/Mz are no longer just permission switches for pre-existing
    // rigid-ring DOFs.  They are the actual road-on-tire shear loads, so apply
    // them as distributed forces to the lower tread of the structural lattice.
    // This is what produces visible cornering side-bend and footprint torsion
    // even when the reduced rigid-ring lateral/yaw states themselves are small.
    // The distribution preserves total Fy, and the signed front/rear shear
    // pair preserves the requested self-aligning moment Mz with zero added net
    // lateral force. Straight rolling has zero Fy/Mz demand and therefore gets
    // no lateral structural forcing at all.
    std::array<VehicleScalar, TireFlexibleRingFieldCount>
        corneringShearWeight{};
    VehicleScalar corneringShearWeightSum = 0.0;
    VehicleScalar corneringMomentDenominatorM2 = 0.0;
    if (physicalCorneringAuthority)
    {
        for (std::size_t station = 0;
             station < TireFlexibleRingFieldStations; ++station)
        {
            const VehicleScalar theta = kTwoPi
                * static_cast<VehicleScalar>(station)
                / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
            const VehicleScalar radialDown = std::sin(theta);
            // Concentrate road shear in the footprint/lower tread, not in the
            // crown.  The smooth support still spans several controls so the
            // 24-station model bends rather than creating a single-node kink.
            const VehicleScalar stationWeight = smoothStep(
                VehicleScalar{0.70}, VehicleScalar{0.985}, radialDown);
            const VehicleScalar forwardPositionM =
                d.unloadedRadiusM * std::cos(theta);
            for (std::size_t band = 0;
                 band < TireFlexibleRingFieldBands; ++band)
            {
                const VehicleScalar absoluteWidth = std::abs(
                    TireFlexibleRingWidthCoordinates[band]);
                // The broad authored tread carries shear.  Fade through the
                // shoulder so sidewall controls respond elastically rather
                // than receiving a fake direct road force.
                const VehicleScalar bandWeight = VehicleScalar{1.0}
                    - smoothStep(VehicleScalar{0.72},
                                 VehicleScalar{0.98}, absoluteWidth);
                const VehicleScalar weight = stationWeight * bandWeight;
                const std::size_t index = fieldIndex(station, band);
                corneringShearWeight[index] = weight;
                corneringShearWeightSum += weight;
                corneringMomentDenominatorM2 +=
                    weight * forwardPositionM * forwardPositionM;
            }
        }
    }

    const VehicleScalar loadDeflectionM = std::clamp(
        std::max(input.normalLoadN, VehicleScalar{0.0})
            / std::max(d.verticalStiffnessNPerM, VehicleScalar{1000.0}),
        VehicleScalar{0.0}, d.maximumDeflectionM);
    const VehicleScalar ordinaryRadialCompressionLimitM = std::clamp(
        std::max(haveCenterRoadOverlap ? centerRoadOverlapM : VehicleScalar{0.0},
                 loadDeflectionM) * VehicleScalar{1.45}
            + VehicleScalar{0.003},
        VehicleScalar{0.008}, d.maximumDeflectionM);

    std::array<CarcassVec3, TireFlexibleRingFieldCount> oldDisplacement{};
    std::array<CarcassVec3, TireFlexibleRingFieldCount> oldVelocity{};
    std::array<CarcassVec3, TireFlexibleRingFieldCount> iterate{};
    for (std::size_t index = 0; index < TireFlexibleRingFieldCount; ++index)
    {
        oldDisplacement[index] = stateDisplacement(state, index);
        oldVelocity[index] = stateVelocity(state, index);
        if (!lateralAuthority)
        {
            oldDisplacement[index].z = 0.0;
            oldVelocity[index].z = 0.0;
        }
        iterate[index] = addCarcass(
            oldDisplacement[index], scaleCarcass(oldVelocity[index], dt));
    }

    // Associate each structural control with the nearest physical road-envelope
    // query once per 125 Hz step. Re-running the 9-25 sample search inside every
    // implicit iteration is both unnecessary and prohibitively expensive for a
    // vehicle grid. The association is only a lookup optimization: the contact
    // point/normal and explicit supported/miss state remain the real collision
    // query data, and the unilateral constraint is still solved every iteration.
    std::array<int, TireFlexibleRingFieldCount> roadSampleForNode{};
    roadSampleForNode.fill(-1);
    const std::size_t roadSampleCount = std::min(
        input.roadSampleCount, TireFlexibleRingMaximumRoadSamples);
    for (std::size_t station = 0;
         station < TireFlexibleRingFieldStations; ++station)
    {
        const VehicleScalar theta = kTwoPi
            * static_cast<VehicleScalar>(station)
            / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
        if (!input.grounded || std::sin(theta) <= tuning.groundStationThreshold)
            continue;
        for (std::size_t band = 0; band < TireFlexibleRingFieldBands; ++band)
        {
            const std::size_t index = fieldIndex(station, band);
            const CarcassVec3 predicted = addCarcass(
                restCarcassPosition(d, station, band), iterate[index]);
            VehicleScalar bestDistanceSquared =
                std::numeric_limits<VehicleScalar>::infinity();
            for (std::size_t sampleIndex = 0;
                 sampleIndex < roadSampleCount; ++sampleIndex)
            {
                const auto& sample = input.roadSamples[sampleIndex];
                if (!sample.queried)
                    continue;
                const VehicleScalar dx = (predicted.x - sample.queryForwardM)
                    * tuning.associationForwardScale;
                const VehicleScalar dz = (predicted.z - sample.queryLateralM)
                    * tuning.associationLateralScale;
                const VehicleScalar distanceSquared = dx * dx + dz * dz;
                if (distanceSquared < bestDistanceSquared)
                {
                    bestDistanceSquared = distanceSquared;
                    roadSampleForNode[index] = static_cast<int>(sampleIndex);
                }
            }
        }
    }

    std::array<VehicleScalar, TireFlexibleRingFieldStations> radialCompression{};
    for (std::size_t station = 0; station < TireFlexibleRingFieldStations; ++station)
    {
        const VehicleScalar theta = kTwoPi
            * static_cast<VehicleScalar>(station)
            / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
        const CarcassVec3 radial{ std::cos(theta), std::sin(theta), 0.0 };
        VehicleScalar sum = 0.0;
        VehicleScalar weight = 0.0;
        for (std::size_t band = 2; band + 2 < TireFlexibleRingFieldBands; ++band)
        {
            const VehicleScalar bandWeight = VehicleScalar{1.0}
                - VehicleScalar{0.45}
                    * std::abs(TireFlexibleRingWidthCoordinates[band]);
            sum += std::max(
                -dotCarcass(oldDisplacement[fieldIndex(station, band)], radial),
                VehicleScalar{0.0}) * bandWeight;
            weight += bandWeight;
        }
        radialCompression[station] = sum / std::max(weight, VehicleScalar{1.0})
            * tuning.radialCompressionScale;
    }

    const VehicleScalar massTerm = nodeMassKg / (dt * dt);
    const VehicleScalar dampingTerm = dampingNsPerM / dt;
    const VehicleScalar maximumMagnitude = std::max(
        d.maximumDeflectionM * VehicleScalar{1.35},
        sidewallHeightM * VehicleScalar{0.92})
        * std::clamp(tuning.maximumMagnitudeScale,
            VehicleScalar{0.10}, VehicleScalar{5.0});

    // Implicit Jacobi solve of the dynamic belt/carcass lattice.  The inertia
    // term references the predicted dynamic state; neighbour and foundation
    // terms are structural energies; road and rim are unilateral contacts.
    std::array<CarcassVec3, TireFlexibleRingFieldCount> next{};
    const int implicitIterations = std::clamp(tuning.implicitIterations, 1, 32);
    for (int iteration = 0; iteration < implicitIterations; ++iteration)
    {
        for (std::size_t station = 0;
             station < TireFlexibleRingFieldStations; ++station)
        {
            const std::size_t sm1 = (station + TireFlexibleRingFieldStations - 1)
                % TireFlexibleRingFieldStations;
            const std::size_t sp1 = (station + 1)
                % TireFlexibleRingFieldStations;
            const std::size_t sm2 = (station + TireFlexibleRingFieldStations - 2)
                % TireFlexibleRingFieldStations;
            const std::size_t sp2 = (station + 2)
                % TireFlexibleRingFieldStations;

            for (std::size_t band = 0; band < TireFlexibleRingFieldBands; ++band)
            {
                const std::size_t index = fieldIndex(station, band);
                const CarcassVec3 rest = restCarcassPosition(d, station, band);
                const CarcassVec3 anchor = ringAnchorDisplacement(
                    d, input, tuning, radialCompression, station, band);
                const VehicleScalar stationFoundation = std::clamp(
                    tuning.stationFoundationScale[station],
                    VehicleScalar{0.0}, VehicleScalar{10.0});
                const VehicleScalar bandFoundation = std::clamp(
                    tuning.bandFoundationScale[band],
                    VehicleScalar{0.0}, VehicleScalar{10.0});
                const VehicleScalar nodeFoundation = foundationPerNode
                    * stationFoundation * bandFoundation;
                const VehicleScalar nodeDamping = dampingTerm
                    * std::clamp(tuning.stationDampingScale[station],
                        VehicleScalar{0.0}, VehicleScalar{10.0})
                    * std::clamp(tuning.bandDampingScale[band],
                        VehicleScalar{0.0}, VehicleScalar{10.0});
                const VehicleScalar nodeAnchor = nodeFoundation
                    * std::clamp(tuning.stationAnchorScale[station],
                        VehicleScalar{0.0}, VehicleScalar{10.0})
                    * std::clamp(tuning.bandAnchorScale[band],
                        VehicleScalar{0.0}, VehicleScalar{10.0});
                const VehicleScalar nodeCircumferential = circumferentialK
                    * std::clamp(tuning.stationCircumferentialScale[station],
                        VehicleScalar{0.0}, VehicleScalar{10.0});
                const VehicleScalar nodeSecondNeighbor = secondNeighborK
                    * std::clamp(tuning.stationCircumferentialScale[station],
                        VehicleScalar{0.0}, VehicleScalar{10.0});
                const VehicleScalar nodeLateral = lateralK
                    * std::clamp(tuning.bandLateralScale[band],
                        VehicleScalar{0.0}, VehicleScalar{10.0});
                CarcassVec3 rhs = addCarcass(
                    scaleCarcass(
                        addCarcass(oldDisplacement[index],
                            scaleCarcass(oldVelocity[index], dt)),
                        massTerm),
                    scaleCarcass(oldDisplacement[index], nodeDamping));
                VehicleScalar diagonal = massTerm + nodeDamping;

                // The moving rim/rigid-ring anchor is a spring attachment, not
                // a displacement added after the carcass has been solved.
                rhs = addCarcass(rhs, scaleCarcass(anchor, nodeAnchor));
                diagonal += nodeAnchor;

                // TIRE45K physical contact-patch shear. The force term enters
                // the same implicit structural equilibrium as inertia, anchors,
                // belt coupling, road contact and rim contact. It is therefore
                // deformation physics, not a render-space displacement hack.
                if (physicalCorneringAuthority)
                {
                    const VehicleScalar shearWeight =
                        corneringShearWeight[index];
                    VehicleScalar lateralShearForceN = 0.0;
                    if (corneringShearWeightSum > kEpsilon)
                    {
                        lateralShearForceN += input.lateralForceN
                            * cornering.lateralDeflection
                            * shearWeight / corneringShearWeightSum;
                    }
                    if (corneringMomentDenominatorM2 > kEpsilon)
                    {
                        const VehicleScalar forwardPositionM =
                            d.unloadedRadiusM * std::cos(
                                kTwoPi * static_cast<VehicleScalar>(station)
                                / static_cast<VehicleScalar>(
                                    TireFlexibleRingFieldStations));
                        // With wheel axes (forward, down, right), a lateral
                        // force Fz at +x contributes My=-x*Fz. Hence the minus
                        // sign makes the distributed pair reproduce input Mz.
                        lateralShearForceN += -input.aligningMomentNm
                            * cornering.torsion
                            * forwardPositionM * shearWeight
                            / corneringMomentDenominatorM2;
                    }
                    rhs.z += lateralShearForceN;
                }

                rhs = addCarcass(rhs,
                    scaleCarcass(addCarcass(iterate[fieldIndex(sm1, band)],
                                            iterate[fieldIndex(sp1, band)]),
                                 nodeCircumferential));
                diagonal += nodeCircumferential * VehicleScalar{2.0};
                rhs = addCarcass(rhs,
                    scaleCarcass(addCarcass(iterate[fieldIndex(sm2, band)],
                                            iterate[fieldIndex(sp2, band)]),
                                 nodeSecondNeighbor));
                diagonal += nodeSecondNeighbor * VehicleScalar{2.0};
                if (band > 0)
                {
                    rhs = addCarcass(rhs,
                        scaleCarcass(iterate[fieldIndex(station, band - 1)], nodeLateral));
                    diagonal += nodeLateral;
                }
                if (band + 1 < TireFlexibleRingFieldBands)
                {
                    rhs = addCarcass(rhs,
                        scaleCarcass(iterate[fieldIndex(station, band + 1)], nodeLateral));
                    diagonal += nodeLateral;
                }

                CarcassVec3 candidate = scaleCarcass(rhs,
                    VehicleScalar{1.0} / std::max(diagonal, kEpsilon));
                const CarcassVec3 currentPosition = addCarcass(rest, candidate);

                // External road contact.  The nearest entry is one of the
                // collision queries already performed by the physical road-
                // envelope solver. An explicitly unsupported query creates no
                // contact, so partial edges are not replaced by an infinite
                // support plane.
                VehicleScalar activeContactK = 0.0;
                CarcassVec3 activeNormal{};
                VehicleScalar activeTarget = 0.0;
                const int roadSampleIndex = roadSampleForNode[index];
                if (roadSampleIndex >= 0)
                {
                    const TireFlexibleRingRoadSample* road =
                        &input.roadSamples[static_cast<std::size_t>(roadSampleIndex)];
                    if (road->supported)
                    {
                        CarcassVec3 normal = normalizeCarcass(
                            { road->normalForward, road->normalDown,
                              road->normalLateral },
                            { 0.0, -1.0, 0.0 });
                        if (!lateralAuthority)
                        {
                            // Support-envelope rays describe the road top.
                            // Without a genuine side contact or rigid lateral
                            // load, a small triangle/cross-slope normal must not
                            // become a sidewall indentation source.
                            normal.z = 0.0;
                            normal = normalizeCarcass(
                                normal, { 0.0, -1.0, 0.0 });
                        }
                        const CarcassVec3 point{
                            road->pointForwardM,
                            road->pointDownM,
                            road->pointLateralM
                        };
                        const VehicleScalar signedDistance = dotCarcass(
                            subtractCarcass(currentPosition, point), normal);
                        const VehicleScalar contactSlop = std::clamp(
                            tuning.contactSlopM,
                            VehicleScalar{0.0}, VehicleScalar{0.020});
                        if (signedDistance < -contactSlop)
                        {
                            activeContactK = contactK
                                * std::clamp(tuning.stationContactScale[station],
                                    VehicleScalar{0.0}, VehicleScalar{10.0})
                                * std::clamp(tuning.bandContactScale[band],
                                    VehicleScalar{0.0}, VehicleScalar{10.0});
                            activeNormal = normal;
                            activeTarget = dotCarcass(
                                subtractCarcass(point, rest), normal)
                                - contactSlop;
                        }
                    }
                }

                // Internal rim/flange collision. This is not a radial clamp:
                // when the carcass approaches the protected bead/flange radius,
                // it enters another unilateral contact constraint in exactly the
                // same structural solve as the road.
                const VehicleScalar currentRadius = std::sqrt(
                    currentPosition.x * currentPosition.x
                    + currentPosition.y * currentPosition.y);
                const VehicleScalar absoluteWidth = std::abs(
                    TireFlexibleRingWidthCoordinates[band]);
                const VehicleScalar flangeHeight = std::clamp(
                    sidewallHeightM * VehicleScalar{0.15}
                        * tuning.flangeHeightScale,
                    VehicleScalar{0.001}, VehicleScalar{0.050});
                const VehicleScalar flangeClearance = std::clamp(
                    sidewallHeightM * VehicleScalar{0.10}
                        * tuning.flangeClearanceScale,
                    VehicleScalar{0.0}, VehicleScalar{0.040});
                const VehicleScalar shoulderAllowance = sidewallHeightM
                    * VehicleScalar{0.035}
                    * tuning.shoulderAllowanceScale
                    * (VehicleScalar{1.0} - absoluteWidth);
                const VehicleScalar minimumRadius = d.rimRadiusM
                    + flangeHeight + flangeClearance + shoulderAllowance;
                if (currentRadius < minimumRadius)
                {
                    const CarcassVec3 rimNormal = normalizeCarcass(
                        { currentPosition.x, currentPosition.y, 0.0 },
                        { 0.0, 1.0, 0.0 });
                    const CarcassVec3 rimPoint = scaleCarcass(
                        rimNormal, minimumRadius);
                    const VehicleScalar rimTarget = dotCarcass(
                        subtractCarcass(rimPoint, rest), rimNormal);
                    if (activeContactK > 0.0)
                    {
                        // Two simultaneous rank-one contacts are rare. Resolve
                        // the road first, then one implicit rim pass using the
                        // already-updated candidate; both remain forces/constraints
                        // inside the iterative solve rather than hard geometry.
                        candidate = solveRankOneContact(
                            diagonal, rhs, activeContactK,
                            activeNormal, activeTarget);
                        const CarcassVec3 rimRhs = scaleCarcass(candidate, diagonal);
                        candidate = solveRankOneContact(
                            diagonal, rimRhs,
                            rimContactK
                                * std::clamp(tuning.stationContactScale[station],
                                    VehicleScalar{0.0}, VehicleScalar{10.0})
                                * std::clamp(tuning.bandContactScale[band],
                                    VehicleScalar{0.0}, VehicleScalar{10.0}),
                            rimNormal, rimTarget);
                    }
                    else
                    {
                        candidate = solveRankOneContact(
                            diagonal, rhs,
                            rimContactK
                                * std::clamp(tuning.stationContactScale[station],
                                    VehicleScalar{0.0}, VehicleScalar{10.0})
                                * std::clamp(tuning.bandContactScale[band],
                                    VehicleScalar{0.0}, VehicleScalar{10.0}),
                            rimNormal, rimTarget);
                    }
                }
                else if (activeContactK > 0.0)
                {
                    candidate = solveRankOneContact(
                        diagonal, rhs, activeContactK,
                        activeNormal, activeTarget);
                }

                if (ordinaryTopContact)
                {
                    // A normal road-top manifold cannot physically hold a
                    // passenger tire 60-70 mm collapsed at a 2-3 kN wheel load.
                    // TIRE45G traces showed exactly that stale state after a
                    // transient manoeuvre. Bound only inward RADIAL compression
                    // to the live hub/road overlap + load-supported allowance.
                    // Tangential shear stays free, and any steep side/curb
                    // contact disables this ordinary-road guard entirely.
                    const VehicleScalar theta = kTwoPi
                        * static_cast<VehicleScalar>(station)
                        / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
                    const CarcassVec3 radial{
                        std::cos(theta), std::sin(theta), 0.0 };
                    const VehicleScalar radialDisplacement =
                        dotCarcass(candidate, radial);
                    if (radialDisplacement < -ordinaryRadialCompressionLimitM)
                    {
                        candidate = addCarcass(candidate, scaleCarcass(
                            radial, -ordinaryRadialCompressionLimitM
                                - radialDisplacement));
                    }
                    if (!lateralAuthority)
                        candidate.z = 0.0;
                }

                // Numerical catastrophe guard only. It is deliberately far
                // outside ordinary carcass travel and is not part of the tire
                // shape model or contact solution.
                const VehicleScalar candidateMagnitude = lengthCarcass(candidate);
                if (!finiteValue(candidateMagnitude))
                    candidate = {};
                else if (candidateMagnitude > maximumMagnitude * VehicleScalar{2.5})
                    candidate = scaleCarcass(candidate,
                        maximumMagnitude * VehicleScalar{2.5} / candidateMagnitude);
                next[index] = candidate;
            }
        }
        iterate = next;
    }

    VehicleScalar maximumDisplacementM = 0.0;
    for (std::size_t index = 0; index < TireFlexibleRingFieldCount; ++index)
    {
        const CarcassVec3 displacement = iterate[index];
        CarcassVec3 velocity = scaleCarcass(
            subtractCarcass(displacement, oldDisplacement[index]),
            VehicleScalar{1.0} / dt);
        // Dissipate unresolved high-frequency lattice chatter while retaining
        // the physically useful carcass transient. This acts on velocity, not
        // on geometry or the road boundary.
        velocity = scaleCarcass(velocity, std::clamp(
            tuning.velocityRetention, VehicleScalar{0.0}, VehicleScalar{1.0}));
        writeStateDisplacement(state, index, displacement);
        writeStateVelocity(state, index, velocity);
        output.forwardDisplacementM[index] = displacement.x;
        output.downDisplacementM[index] = displacement.y;
        output.lateralDisplacementM[index] = displacement.z;
        maximumDisplacementM = std::max(
            maximumDisplacementM, lengthCarcass(displacement));
    }

    output.valid = maximumDisplacementM > VehicleScalar{1.0e-7};
    return output;
}

void relaxTireFlexibleRingDynamics(
    const TireFlexibleRingFieldDescription& d,
    VehicleScalar deltaTimeSeconds,
    TireFlexibleRingDynamicState& state)
{
    if (!state.initialized || !validTireFlexibleRingFieldDescription(d)
        || !finiteValue(deltaTimeSeconds) || deltaTimeSeconds <= 0.0)
    {
        return;
    }
    TireFlexibleRingDynamicsInput input;
    input.deltaTimeSeconds = deltaTimeSeconds;
    input.grounded = false;
    input.inflationPressurePa = d.referencePressurePa;
    input.thermalStiffnessScale = 1.0;
    advanceTireFlexibleRingDynamics(d, input, state);
}

} // namespace heritage::vehicles::tires
