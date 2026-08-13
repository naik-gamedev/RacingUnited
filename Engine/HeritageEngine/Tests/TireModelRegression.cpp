#include "PhysicsRegressionCommon.hpp"

#include "../Vehicles/TireModel.hpp"
#include "../Vehicles/Tires/MagicFormula/MagicFormula62.hpp"
#include "../Vehicles/Tires/MotorcycleTireProfile.hpp"
#include "../Vehicles/Tires/Authoring/TireFamilyBaseline.hpp"
#include "../Vehicles/Tires/Authoring/TirePartResolver.hpp"
#include "../Vehicles/Tires/TireSlipDynamics.hpp"
#include "../Vehicles/Tires/TireContactPatch.hpp"
#include "../Vehicles/Tires/TireContactGeometry.hpp"
#include "../Vehicles/Tires/TireFlexibleRingField.hpp"
#include "../Vehicles/Tires/TireRigidRing.hpp"
#include "../Vehicles/Tires/TireRoadEnveloping.hpp"
#include "../Vehicles/Tires/TireThermal.hpp"
#include "../Vehicles/Tires/TireWear.hpp"
#include "../Vehicles/Tires/TireSurfaceInteraction.hpp"
#include "../Vehicles/Tires/TireWetSurfaceInteraction.hpp"
#include "../Vehicles/Tires/TireWinterSurfaceInteraction.hpp"
#include "../Vehicles/Tires/TireShallowGranularInteraction.hpp"
#include "../Vehicles/Tires/TireDeformableTerrainInteraction.hpp"
#include "../Physics/SurfaceField.hpp"

#include <cmath>

namespace heritage::tests {
namespace {

using heritage::vehicles::TireContactInput;
using heritage::vehicles::TireForceResult;
using heritage::vehicles::TireModelDescription;
using heritage::vehicles::TireProviderKind;
using heritage::vehicles::VehicleScalar;

constexpr VehicleScalar kPi = 3.14159265358979323846;

bool finite(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

} // namespace

bool magicFormula62RoadCoreBehaves()
{
    TireModelDescription tire;
    tire.provider = TireProviderKind::MagicFormula62;

    TireContactInput pureLongitudinal;
    pureLongitudinal.normalLoad = tire.nominalLoad;
    pureLongitudinal.longitudinalSlip = 0.08;
    pureLongitudinal.forwardSpeedMps = 25.0;
    pureLongitudinal.wheelRadiusM = 0.316;
    const TireForceResult fx = heritage::vehicles::evaluateAdvancedRoadTire(
        tire,
        pureLongitudinal);

    TireContactInput pureLateral = pureLongitudinal;
    pureLateral.longitudinalSlip = 0.0;
    pureLateral.slipAngleRadians = 0.08;
    const TireForceResult fy = heritage::vehicles::evaluateAdvancedRoadTire(
        tire,
        pureLateral);

    TireContactInput combined = pureLongitudinal;
    combined.slipAngleRadians = 0.08;
    const TireForceResult both = heritage::vehicles::evaluateAdvancedRoadTire(
        tire,
        combined);

    if (!finite(fx.longitudinalForce)
        || !finite(fy.lateralForce)
        || !finite(both.aligningTorque)
        || !finite(both.rollingResistanceMoment))
    {
        return false;
    }

    return fx.longitudinalForce > 100.0
        && fy.lateralForce < -100.0
        && fx.longitudinalSlipStiffness > 1000.0
        && fy.corneringStiffness > 1000.0
        && both.combinedLongitudinalWeight <= 1.000001
        && both.combinedLateralWeight <= 1.000001
        && std::abs(both.longitudinalForce)
            <= std::abs(both.pureLongitudinalForce) + 1.0
        && both.effectiveFriction > 0.5
        && both.rollingResistanceMoment < 0.0;
}

bool magicFormula62TurnSlipReducesGripAndTrail()
{
    TireModelDescription tire;
    tire.provider = TireProviderKind::MagicFormula62;
    tire.magicFormulaUsesLegacySeed = false;
    tire.magicFormula = heritage::vehicles::seededMagicFormula62Parameters(tire, 0.300);
    tire.magicFormula.pDxP1 = 0.40;
    tire.magicFormula.pKyP1 = 1.00;
    tire.magicFormula.pDyP1 = 0.40;
    tire.magicFormula.pEcP1 = 0.50;
    tire.magicFormula.qDtP1 = 10.0;
    tire.magicFormula.qBrP1 = 0.10;
    tire.magicFormula.qDrP1 = 1.00;
    tire.magicFormula.qCrP2 = 0.10;

    TireContactInput input;
    input.normalLoad = tire.nominalLoad;
    input.longitudinalSlip = 0.08;
    input.slipAngleRadians = 0.08;
    input.forwardSpeedMps = 12.0;
    input.wheelRadiusM = 0.300;

    const TireForceResult baseline = heritage::vehicles::evaluateAdvancedRoadTire(
        tire, input);

    input.turnSlipPerM = 2.0;
    const TireForceResult turning = heritage::vehicles::evaluateAdvancedRoadTire(
        tire, input);

    return finite(baseline.longitudinalForce)
        && finite(turning.longitudinalForce)
        && finite(turning.lateralForce)
        && finite(turning.aligningTorque)
        && finite(turning.turnSlipMoment)
        && turning.normalizedTurnSlip > 0.50
        && turning.turnSlipLongitudinalReduction < 0.999
        && turning.turnSlipLateralReduction < 0.999
        && turning.turnSlipCorneringReduction < 0.999
        && turning.turnSlipTrailReduction < 0.999
        && std::abs(turning.longitudinalForce) < std::abs(baseline.longitudinalForce)
        && std::abs(turning.pneumaticTrail) < std::abs(baseline.pneumaticTrail)
        && std::abs(turning.turnSlipMoment) > 1.0;
}

bool magicFormula62MotorcycleLargeCamberBehaves()
{
    TireModelDescription tire;
    tire.provider = TireProviderKind::MagicFormula62Motorcycle;
    tire.motorcycleProfile.tireWidthM = 0.180;
    tire.motorcycleProfile.mcContourA = 0.50;
    tire.motorcycleProfile.mcContourB = 0.50;

    TireContactInput input;
    input.normalLoad = 1800.0;
    input.camberAngleRadians = 40.0 * kPi / 180.0;
    input.forwardSpeedMps = 22.0;
    input.wheelRadiusM = 0.300;
    const TireForceResult result = heritage::vehicles::evaluateAdvancedRoadTire(
        tire,
        input);

    TireContactInput opposite = input;
    opposite.camberAngleRadians = -input.camberAngleRadians;
    const TireForceResult mirror = heritage::vehicles::evaluateAdvancedRoadTire(
        tire,
        opposite);

    TireContactInput highLean = input;
    highLean.camberAngleRadians = 60.0 * kPi / 180.0;
    const TireForceResult highLeanResult = heritage::vehicles::evaluateAdvancedRoadTire(
        tire,
        highLean);

    return result.motorcycleContourValid
        && mirror.motorcycleContourValid
        && finite(result.lateralForce)
        && finite(result.aligningTorque)
        && highLeanResult.motorcycleContourValid
        && finite(highLeanResult.lateralForce)
        && finite(highLeanResult.aligningTorque)
        && std::abs(result.motorcycleContactLateralOffset) > 0.005
        && result.motorcycleCenterToRoad > 0.10
        && result.motorcycleCenterToRoad < input.wheelRadiusM + 0.01
        && result.motorcycleContactLateralOffset
            * mirror.motorcycleContactLateralOffset < 0.0
        && result.camberStiffness > 100.0
        && result.lateralForce * mirror.lateralForce < 0.0;
}

bool tireRelaxationDynamicsAreRateStable()
{
    heritage::vehicles::tires::TireSlipDynamicsDescription description;
    description.longitudinalRelaxationLengthM = 0.35;
    description.lateralRelaxationLengthM = 0.45;

    const auto integrateFor = [&](VehicleScalar dt) {
        heritage::vehicles::tires::TireSlipDynamicsState state;
        const int steps = static_cast<int>(1.0 / dt);
        for (int i = 0; i < steps; ++i)
        {
            heritage::vehicles::tires::integrateTireSlipDynamics(
                description,
                0.15,
                0.10,
                20.0,
                dt,
                state);
        }
        return state;
    };

    const auto highRate = integrateFor(0.001);
    const auto lowRate = integrateFor(1.0 / 120.0);

    heritage::vehicles::tires::TireSlipDynamicsCoefficients coefficients;
    coefficients.pTx1 = 1.1666666667;
    coefficients.pTx2 = 0.0;
    coefficients.pTx3 = 0.0;
    coefficients.pTy1 = 1.50;
    coefficients.pTy2 = 1.0;
    const VehicleScalar longitudinalLength =
        heritage::vehicles::tires::magicFormulaLongitudinalRelaxationLengthM(
            coefficients, 3500.0, 3500.0, 0.300, 0.35);
    const VehicleScalar lateralLength =
        heritage::vehicles::tires::magicFormulaLateralRelaxationLengthM(
            coefficients, 3500.0, 3500.0, 0.300, 0.0, 0.0, 0.45);

    return std::abs(highRate.longitudinalSlip - lowRate.longitudinalSlip) < 1.0e-4
        && std::abs(highRate.slipAngleRadians - lowRate.slipAngleRadians) < 1.0e-4
        && highRate.longitudinalSlip > 0.149
        && highRate.slipAngleRadians > 0.099
        && std::abs(longitudinalLength - 0.35) < 1.0e-4
        && std::abs(lateralLength - 0.45) < 1.0e-4;
}

bool tireContactPatchParkingTwistIsRateStable()
{
    heritage::vehicles::tires::TireContactPatchDescription description;

    const auto integrateFor = [&](VehicleScalar dt) {
        heritage::vehicles::tires::TireContactPatchState state;
        heritage::vehicles::tires::TireContactPatchOutput output;
        heritage::vehicles::tires::TireContactPatchInput input;
        input.wheelYawRateRadiansPerSecond = 0.50;
        input.forwardSpeedMps = 0.0;
        input.normalLoadN = 3500.0;
        input.effectiveFriction = 1.10;
        input.unloadedRadiusM = 0.300;
        input.zeroSpeedTurnMomentCoefficient = 0.20;
        const int steps = static_cast<int>(0.4 / dt);
        for (int i = 0; i < steps; ++i)
        {
            output = heritage::vehicles::tires::integrateTireContactPatch(
                description, input, dt, state);
        }
        return output;
    };

    const auto highRate = integrateFor(0.001);
    const auto lowRate = integrateFor(1.0 / 120.0);

    heritage::vehicles::tires::TireContactPatchState releaseState;
    heritage::vehicles::tires::TireContactPatchInput windup;
    windup.wheelYawRateRadiansPerSecond = 0.50;
    windup.forwardSpeedMps = 0.0;
    windup.normalLoadN = 3500.0;
    windup.effectiveFriction = 1.10;
    windup.unloadedRadiusM = 0.300;
    windup.zeroSpeedTurnMomentCoefficient = 0.20;
    for (int i = 0; i < 400; ++i)
        heritage::vehicles::tires::integrateTireContactPatch(
            description, windup, 0.001, releaseState);

    auto release = windup;
    release.wheelYawRateRadiansPerSecond = 0.0;
    release.forwardSpeedMps = 5.0;
    heritage::vehicles::tires::TireContactPatchOutput released;
    for (int i = 0; i < 300; ++i)
        released = heritage::vehicles::tires::integrateTireContactPatch(
            description, release, 0.001, releaseState);

    return finite(highRate.torsionalTwistRadians)
        && finite(highRate.parkingTurnMomentNm)
        && highRate.torsionalTwistRadians > 0.02
        && highRate.parkingTurnMomentNm < -1.0
        && highRate.parkingMomentBlend > 0.99
        && std::abs(highRate.torsionalTwistRadians - lowRate.torsionalTwistRadians) < 0.005
        && std::abs(highRate.parkingTurnMomentNm - lowRate.parkingTurnMomentNm) < 10.0
        && std::abs(released.torsionalTwistRadians) < 1.0e-3
        && released.parkingMomentBlend < 0.10;
}

bool tireContactGeometryEffectiveRadiusAndFootprintBehave()
{
    heritage::vehicles::tires::TireContactGeometryDescription description;
    description.unloadedRadiusM = 0.2979;
    description.nominalLoadN = 3300.0;
    description.verticalStiffnessNPerM = 220000.0;
    description.nominalWidthM = 0.205;
    description.rimRadiusM = 0.2159;
    description.referenceSpeedMps = 16.6666666667;
    description.useMagicFormulaEffectiveRadius = true;
    description.bReff = 8.386;
    description.dReff = 0.25826;
    description.fReff = 0.07394;
    description.qRe0 = 1.0;
    description.qV1 = 0.00076;
    description.useMagicFormulaContactLength = true;
    description.qRa1 = 0.67594;
    description.qRa2 = 0.73800;
    description.qRb1 = 1.04487;
    description.qRb2 = -1.19176;

    heritage::vehicles::tires::TireContactGeometryInput nominalInput;
    nominalInput.normalLoadN = 3300.0;
    nominalInput.inflationPressurePa = 220000.0;
    const auto nominal = heritage::vehicles::tires::evaluateTireContactGeometry(
        description, nominalInput);

    auto heavyInput = nominalInput;
    heavyInput.normalLoadN = 5000.0;
    const auto heavy = heritage::vehicles::tires::evaluateTireContactGeometry(
        description, heavyInput);

    auto fastInput = nominalInput;
    fastInput.wheelAngularVelocityRadPerS = 60.0 / description.unloadedRadiusM;
    const auto fast = heritage::vehicles::tires::evaluateTireContactGeometry(
        description, fastInput);

    auto knownDeflectionInput = nominalInput;
    knownDeflectionInput.verticalDeflectionKnown = true;
    knownDeflectionInput.verticalDeflectionM = 0.010;
    const auto known = heritage::vehicles::tires::evaluateTireContactGeometry(
        description, knownDeflectionInput);

    return nominal.valid && heavy.valid && fast.valid && known.valid
        && nominal.freeRollingRadiusM > nominal.effectiveRollingRadiusM
        && nominal.effectiveRollingRadiusM > nominal.loadedRadiusM
        && heavy.loadedRadiusM < nominal.loadedRadiusM
        && heavy.effectiveRollingRadiusM < nominal.effectiveRollingRadiusM
        && fast.freeRollingRadiusM > nominal.freeRollingRadiusM
        && nominal.contactPatchLengthM > 0.05
        && nominal.contactPatchWidthM > 0.05
        && nominal.contactPatchWidthM <= description.nominalWidthM
        && nominal.contactPatchAreaM2 > 0.005
        && heavy.contactPatchLengthM > nominal.contactPatchLengthM
        && std::abs(known.verticalDeflectionM - 0.010) < 1.0e-12
        && std::abs(known.loadedRadiusM
            - (known.freeRollingRadiusM - 0.010)) < 1.0e-12;
}

bool tireFlexibleRingFieldIsSmoothBoundedAndAsymmetric()
{
    using namespace heritage::vehicles::tires;

    TireFlexibleRingFieldDescription description;
    description.unloadedRadiusM = 0.316;
    description.rimRadiusM = 0.2159;
    description.sectionWidthM = 0.205;
    description.maximumDeflectionM = 0.080;
    description.referencePressurePa = 220000.0;
    description.verticalStiffnessNPerM = 220000.0;

    TireFlexibleRingFieldInput input;
    input.grounded = true;
    input.verticalDeflectionM = 0.028;
    input.contactPatchLengthM = 0.125;
    input.contactPatchWidthM = 0.165;
    input.normalLoadN = 3550.0;
    input.inflationPressurePa = 220000.0;

    const auto symmetric = evaluateTireFlexibleRingField(description, input);
    if (!symmetric.valid)
        return false;

    constexpr std::size_t bottomStation = 6;
    constexpr std::size_t centerBand = 6;
    const std::size_t bottomCenter = bottomStation
        * TireFlexibleRingFieldBands + centerBand;
    if (!(symmetric.downDisplacementM[bottomCenter] < -0.001))
    {
        return false;
    }

    // A flat road supplies a radial resolving normal even under shoulder
    // probes. It must bulge the negative sidewall in the negative direction
    // and the positive sidewall in the positive direction; interpreting probe
    // launch direction as contact normal produces the inverted thumbed-in
    // cross-section seen in the live wheel test.
    TireFlexibleRingFieldInput flatRoadContact = input;
    for (std::size_t station = 7; station <= 13; ++station)
    {
        const VehicleScalar phi =
            TireFlexibleRingContactPhiRadians[station];
        for (std::size_t band = 0;
             band < TireFlexibleRingContactBands; ++band)
        {
            const std::size_t index =
                station * TireFlexibleRingContactBands + band;
            flatRoadContact.directContactCompressionM[index] = 0.018;
            flatRoadContact.directContactForwardDisplacementM[index] =
                -std::cos(phi) * 0.018;
            flatRoadContact.directContactDownDisplacementM[index] =
                -std::sin(phi) * 0.018;
        }
    }
    const auto flatRoad = evaluateTireFlexibleRingField(
        description, flatRoadContact);
    const std::size_t bottomNegativeSide = bottomStation
        * TireFlexibleRingFieldBands;
    const std::size_t bottomPositiveSide = bottomStation
        * TireFlexibleRingFieldBands + (TireFlexibleRingFieldBands - 1);
    if (!flatRoad.valid
        || !(flatRoad.lateralDisplacementM[bottomNegativeSide] < -0.0001)
        || !(flatRoad.lateralDisplacementM[bottomPositiveSide] > 0.0001))
    {
        return false;
    }

    // A narrow raised kerb beneath the tyre returns two outward-facing side
    // normals. Preserve their signs: the lower carcass wraps into a bowl around
    // the kerb instead of both walls being inverted into inward thumb dents.
    TireFlexibleRingFieldInput narrowKerb = flatRoadContact;
    for (std::size_t station = 8; station <= 12; ++station)
    {
        for (std::size_t band = 0; band <= 2; ++band)
        {
            const std::size_t index =
                station * TireFlexibleRingContactBands + band;
            narrowKerb.directContactCompressionM[index] = 0.020;
            narrowKerb.directContactForwardDisplacementM[index] = 0.0;
            narrowKerb.directContactDownDisplacementM[index] = 0.0;
            narrowKerb.directContactLateralDisplacementM[index] = -0.020;
        }
        for (std::size_t band = TireFlexibleRingContactBands - 3;
             band < TireFlexibleRingContactBands; ++band)
        {
            const std::size_t index =
                station * TireFlexibleRingContactBands + band;
            narrowKerb.directContactCompressionM[index] = 0.020;
            narrowKerb.directContactForwardDisplacementM[index] = 0.0;
            narrowKerb.directContactDownDisplacementM[index] = 0.0;
            narrowKerb.directContactLateralDisplacementM[index] = 0.020;
        }
    }
    const auto wrappedKerb = evaluateTireFlexibleRingField(
        description, narrowKerb);
    if (!wrappedKerb.valid
        || !(wrappedKerb.lateralDisplacementM[bottomNegativeSide]
            < flatRoad.lateralDisplacementM[bottomNegativeSide])
        || !(wrappedKerb.lateralDisplacementM[bottomPositiveSide]
            > flatRoad.lateralDisplacementM[bottomPositiveSide]))
    {
        return false;
    }

    // Contacting the negative-width curb face indents that side directly; the
    // structural profile must put more displaced volume into the positive/free
    // sidewall, without exceeding a bounded fraction of the section width.
    for (std::size_t station = 7; station <= 13; ++station)
    {
        for (std::size_t band = 0; band <= 4; ++band)
        {
            input.directContactCompressionM[
                station * TireFlexibleRingContactBands + band] = 0.018;
            input.directContactLateralDisplacementM[
                station * TireFlexibleRingContactBands + band] = 0.018;
        }
    }
    const auto asymmetric = evaluateTireFlexibleRingField(description, input);
    const std::size_t negativeSide = bottomStation
        * TireFlexibleRingFieldBands;
    const std::size_t positiveSide = bottomStation
        * TireFlexibleRingFieldBands + (TireFlexibleRingFieldBands - 1);
    if (!asymmetric.valid
        || !(asymmetric.lateralDisplacementM[positiveSide] > 0.0001)
        || !(asymmetric.lateralDisplacementM[negativeSide] > 0.0001)
        || asymmetric.lateralDisplacementM[negativeSide]
            > description.sectionWidthM * 0.18)
    {
        return false;
    }

    TireFlexibleRingFieldInput lowPressure = input;
    lowPressure.directContactCompressionM = {};
    lowPressure.directContactForwardDisplacementM = {};
    lowPressure.directContactDownDisplacementM = {};
    lowPressure.directContactLateralDisplacementM = {};
    lowPressure.inflationPressurePa = 80000.0;
    TireFlexibleRingFieldInput highPressure = lowPressure;
    highPressure.inflationPressurePa = 320000.0;
    const auto low = evaluateTireFlexibleRingField(description, lowPressure);
    const auto high = evaluateTireFlexibleRingField(description, highPressure);
    if (!low.valid || !high.valid
        || !(std::abs(low.lateralDisplacementM[positiveSide])
            > std::abs(high.lateralDisplacementM[positiveSide])))
    {
        return false;
    }

    // Live cold-pressure changes must be measured against an immutable
    // construction/identification datum. With the same load and contact
    // geometry, lower pressure produces both greater loaded-radius loss and a
    // visibly broader lower-sidewall belly; high pressure restrains both.
    TireFlexibleRingFieldInput lowPressureContact = flatRoadContact;
    lowPressureContact.inflationPressurePa = 150000.0;
    TireFlexibleRingFieldInput highPressureContact = flatRoadContact;
    highPressureContact.inflationPressurePa = 300000.0;
    const auto lowContact = evaluateTireFlexibleRingField(
        description, lowPressureContact);
    const auto highContact = evaluateTireFlexibleRingField(
        description, highPressureContact);
    if (!lowContact.valid || !highContact.valid
        || !(std::abs(lowContact.downDisplacementM[bottomCenter])
            > std::abs(highContact.downDisplacementM[bottomCenter]) + 0.0005)
        || !(std::abs(lowContact.lateralDisplacementM[bottomPositiveSide])
            > std::abs(highContact.lateralDisplacementM[bottomPositiveSide])
                + 0.0005))
    {
        return false;
    }

    // Sidewall height is actual deformation capacity, not merely mesh scale.
    // A tall profile may build the broad low-pressure belly shown by a soft
    // road/off-road tire; a short performance sidewall must resist the same
    // normalized contact and approach its bead/flange limit much sooner.
    TireFlexibleRingFieldDescription lowProfile = description;
    lowProfile.unloadedRadiusM = lowProfile.rimRadiusM + 0.050;
    lowProfile.maximumDeflectionM = 0.045;
    TireFlexibleRingFieldDescription tallProfile = description;
    tallProfile.unloadedRadiusM = tallProfile.rimRadiusM + 0.125;
    tallProfile.maximumDeflectionM = 0.105;
    TireFlexibleRingFieldInput profileContact = lowPressureContact;
    profileContact.verticalDeflectionM = 0.030;
    const auto shortSidewall = evaluateTireFlexibleRingField(
        lowProfile, profileContact);
    const auto tallSidewall = evaluateTireFlexibleRingField(
        tallProfile, profileContact);
    if (!shortSidewall.valid || !tallSidewall.valid
        || !(std::abs(tallSidewall.lateralDisplacementM[bottomPositiveSide])
            > std::abs(shortSidewall.lateralDisplacementM[bottomPositiveSide])
                + 0.0005))
    {
        return false;
    }

    // The 1000 Hz rigid-ring state is presented as a bent carcass while
    // grounded: the road restrains the footprint and the displacement fades
    // toward the crown. It must not look like a rigid sideways translation.
    TireFlexibleRingFieldInput lateralBending = input;
    lateralBending.directContactCompressionM = {};
    lateralBending.directContactForwardDisplacementM = {};
    lateralBending.directContactDownDisplacementM = {};
    lateralBending.directContactLateralDisplacementM = {};
    lateralBending.ringLateralOffsetM = 0.006;
    const auto bent = evaluateTireFlexibleRingField(
        description, lateralBending);
    const std::size_t topSide = 18 * TireFlexibleRingFieldBands
        + (TireFlexibleRingFieldBands - 1);
    if (!bent.valid
        || !(std::abs(bent.lateralDisplacementM[bottomPositiveSide])
            > std::abs(bent.lateralDisplacementM[topSide]) * 1.35))
    {
        return false;
    }

    // Steering at/near standstill already has a physical contact-patch twist
    // state. Ensure that twist becomes a distributed lower-carcass torsion
    // rather than remaining telemetry-only or rotating the entire wheel mesh.
    TireFlexibleRingFieldInput torsion = lateralBending;
    torsion.ringLateralOffsetM = 0.0;
    torsion.contactPatchTwistRadians = 0.10;
    const auto twisted = evaluateTireFlexibleRingField(description, torsion);
    const std::size_t lowerShoulder = 5 * TireFlexibleRingFieldBands
        + (TireFlexibleRingFieldBands - 1);
    const std::size_t upperShoulder = 17 * TireFlexibleRingFieldBands
        + (TireFlexibleRingFieldBands - 1);
    if (!twisted.valid
        || !(std::abs(twisted.forwardDisplacementM[lowerShoulder]) > 0.001)
        || !(std::abs(twisted.forwardDisplacementM[lowerShoulder])
            > std::abs(twisted.forwardDisplacementM[upperShoulder]) * 2.0))
    {
        return false;
    }

    // The footprint may shorten the loaded radius, but pressure/carcass hoop
    // tension must preserve the unloaded crown and bead clearance. This guards
    // against reintroducing a whole-belt road-height translation, which makes
    // the top balloon by the same amount that the bottom collapses.
    TireFlexibleRingFieldInput severeContact = input;
    severeContact.directContactCompressionM = {};
    severeContact.directContactForwardDisplacementM = {};
    severeContact.directContactDownDisplacementM = {};
    severeContact.directContactLateralDisplacementM = {};
    for (std::size_t station = 0;
         station < TireFlexibleRingContactStations; ++station)
    {
        const VehicleScalar phi =
            TireFlexibleRingContactPhiRadians[station];
        for (std::size_t band = 0;
             band < TireFlexibleRingContactBands; ++band)
        {
            const std::size_t index =
                station * TireFlexibleRingContactBands + band;
            severeContact.directContactCompressionM[index] = 0.080;
            severeContact.directContactForwardDisplacementM[index] =
                -std::cos(phi) * 0.080;
            severeContact.directContactDownDisplacementM[index] =
                -std::sin(phi) * 0.080;
        }
    }
    const auto constrained = evaluateTireFlexibleRingField(
        description, severeContact);
    constexpr std::size_t topStation = 18;
    const std::size_t topCenter = topStation
        * TireFlexibleRingFieldBands + centerBand;
    const VehicleScalar bottomRadiusM = std::hypot(
        description.unloadedRadiusM
            + constrained.downDisplacementM[bottomCenter],
        constrained.forwardDisplacementM[bottomCenter]);
    const VehicleScalar topRadiusM = std::hypot(
        -description.unloadedRadiusM
            + constrained.downDisplacementM[topCenter],
        constrained.forwardDisplacementM[topCenter]);
    const VehicleScalar sidewallHeightM = description.unloadedRadiusM
        - description.rimRadiusM;
    if (!constrained.valid
        || !(bottomRadiusM + 0.010 < topRadiusM)
        || bottomRadiusM < description.rimRadiusM
            + sidewallHeightM * 0.45
        || topRadiusM > description.unloadedRadiusM + 0.0005)
    {
        return false;
    }

    TireFlexibleRingFieldInput severeLowPressure = severeContact;
    severeLowPressure.inflationPressurePa = 50000.0;
    const auto lowPressureConstrained = evaluateTireFlexibleRingField(
        description, severeLowPressure);
    const VehicleScalar lowPressureBottomRadiusM = std::hypot(
        description.unloadedRadiusM
            + lowPressureConstrained.downDisplacementM[bottomCenter],
        lowPressureConstrained.forwardDisplacementM[bottomCenter]);
    const VehicleScalar protectedFlangeRadiusM = description.rimRadiusM
        + std::clamp(sidewallHeightM * 0.15, 0.006, 0.018)
        + std::clamp(sidewallHeightM * 0.12, 0.005, 0.014);
    if (!lowPressureConstrained.valid
        || !(lowPressureBottomRadiusM > protectedFlangeRadiusM - 0.0001))
    {
        return false;
    }

    TireFlexibleRingFieldDescription softConstruction = description;
    softConstruction.verticalStiffnessNPerM = 110000.0;
    TireFlexibleRingFieldDescription stiffConstruction = description;
    stiffConstruction.verticalStiffnessNPerM = 440000.0;
    const auto soft = evaluateTireFlexibleRingField(
        softConstruction, severeContact);
    const auto stiff = evaluateTireFlexibleRingField(
        stiffConstruction, severeContact);
    const VehicleScalar softBottomRadiusM = std::hypot(
        description.unloadedRadiusM
            + soft.downDisplacementM[bottomCenter],
        soft.forwardDisplacementM[bottomCenter]);
    const VehicleScalar stiffBottomRadiusM = std::hypot(
        description.unloadedRadiusM
            + stiff.downDisplacementM[bottomCenter],
        stiff.forwardDisplacementM[bottomCenter]);
    if (!soft.valid || !stiff.valid
        || !(stiffBottomRadiusM > softBottomRadiusM + 0.002))
    {
        return false;
    }

    // Profile samples may change with non-uniform station spacing, but no single
    // adjacent section is allowed to become the old one-row silhouette kink.
    VehicleScalar largestAdjacentJump = 0.0;
    for (std::size_t station = 0;
         station < TireFlexibleRingFieldStations; ++station)
    {
        const std::size_t next = (station + 1) % TireFlexibleRingFieldStations;
        largestAdjacentJump = std::max(
            largestAdjacentJump,
            std::abs(
                asymmetric.downDisplacementM[
                    station * TireFlexibleRingFieldBands + centerBand]
                - asymmetric.downDisplacementM[
                    next * TireFlexibleRingFieldBands + centerBand]));
    }
    return largestAdjacentJump < 0.018;
}

bool tireRigidRingStructuralModesAreRateStable()
{
    heritage::vehicles::tires::TireRigidRingDescription description;
    description.enabled = true;
    description.longitudinalStiffnessNPerM = 800000.0;
    description.lateralStiffnessNPerM = 650000.0;
    description.yawStiffnessNmPerRad = 4500.0;
    description.longitudinalFrequencyHz = 65.0;
    description.lateralFrequencyHz = 55.0;
    description.radialFrequencyHz = 65.0;
    description.yawFrequencyHz = 50.0;
    description.windupFrequencyHz = 75.0;
    description.longitudinalDampingRatio = 0.18;
    description.lateralDampingRatio = 0.20;
    description.radialDampingRatio = 0.18;
    description.yawDampingRatio = 0.22;
    description.windupDampingRatio = 0.18;
    description.residualDampingRatio = 0.02;
    description.beltMassKg = 6.0;
    description.beltDiametralInertiaKgM2 = 0.18;
    description.beltPolarInertiaKgM2 = 0.33;

    const auto integrateFor = [&](VehicleScalar dt) {
        heritage::vehicles::tires::TireRigidRingState state;
        heritage::vehicles::tires::TireRigidRingOutput output;
        heritage::vehicles::tires::TireRigidRingInput input;
        input.deltaTimeSeconds = dt;
        input.forwardSpeedMps = 20.0;
        input.roadRadialOffsetM = 0.020;
        input.longitudinalForceN = 3200.0;
        input.lateralForceN = -2600.0;
        input.inflationPressurePa = 220000.0;
        input.referencePressurePa = 220000.0;
        input.thermalStiffnessScale = 1.0;
        input.aligningMomentNm = -90.0;
        input.longitudinalReactionMomentNm = -960.0;
        const int steps = static_cast<int>(0.20 / dt);
        for (int i = 0; i < steps; ++i)
            output = heritage::vehicles::tires::advanceTireRigidRing(
                description, input, state);
        return output;
    };

    const auto highRate = integrateFor(0.001);
    const auto lowRate = integrateFor(1.0 / 120.0);

    heritage::vehicles::tires::TireRigidRingState lowPressureState;
    heritage::vehicles::tires::TireRigidRingOutput lowPressure;
    heritage::vehicles::tires::TireRigidRingInput lowPressureInput;
    lowPressureInput.deltaTimeSeconds = 0.001;
    lowPressureInput.forwardSpeedMps = 20.0;
    lowPressureInput.lateralForceN = -2600.0;
    lowPressureInput.inflationPressurePa = 50000.0;
    lowPressureInput.referencePressurePa = 220000.0;
    lowPressureInput.thermalStiffnessScale = 1.0;
    for (int i = 0; i < 200; ++i)
        lowPressure = heritage::vehicles::tires::advanceTireRigidRing(
            description, lowPressureInput, lowPressureState);

    const VehicleScalar expectedLong = 3200.0 / 800000.0;
    const VehicleScalar expectedLat = -2600.0 / 650000.0;
    const VehicleScalar expectedYaw = -90.0 / 4500.0;
    const VehicleScalar windupW = 2.0 * kPi * 75.0;
    const VehicleScalar windupStiffness = 0.33 * windupW * windupW;
    const VehicleScalar expectedWindup = -960.0 / windupStiffness;

    return highRate.valid && lowRate.valid
        && finite(highRate.longitudinalOffsetM)
        && finite(highRate.lateralOffsetM)
        && finite(highRate.radialOffsetM)
        && finite(highRate.yawAngleRadians)
        && finite(highRate.windupAngleRadians)
        && std::abs(highRate.longitudinalOffsetM - expectedLong) < 2.0e-4
        && std::abs(highRate.lateralOffsetM - expectedLat) < 2.0e-4
        && std::abs(highRate.radialOffsetM - 0.020) < 2.0e-4
        && std::abs(highRate.yawAngleRadians - expectedYaw) < 5.0e-4
        && std::abs(highRate.windupAngleRadians - expectedWindup) < 5.0e-4
        && std::abs(highRate.longitudinalOffsetM - lowRate.longitudinalOffsetM) < 3.0e-4
        && std::abs(highRate.lateralOffsetM - lowRate.lateralOffsetM) < 3.0e-4
        && std::abs(highRate.radialOffsetM - lowRate.radialOffsetM) < 3.0e-4
        && std::abs(highRate.yawAngleRadians - lowRate.yawAngleRadians) < 5.0e-4
        && std::abs(highRate.windupAngleRadians - lowRate.windupAngleRadians) < 5.0e-4
        && lowPressure.valid
        && std::abs(lowPressure.lateralOffsetM) > std::abs(highRate.lateralOffsetM) * 1.8;
}

bool tireRoadEnvelopeFiltersShortObstacle()
{
    heritage::vehicles::tires::TireRoadEnvelopingDescription description;
    description.enabled = true;
    description.ellipseShiftScale = 0.82;
    description.ellipseLengthM = 0.115;
    description.ellipseHeightM = 0.055;
    description.ellipseOrder = 2.0;
    description.maximumRoadStepM = 0.10;
    description.widthCamCount = 3;
    description.sideCamCount = 3;

    const VehicleScalar contactLength = 0.140;
    const VehicleScalar contactWidth = 0.160;
    const VehicleScalar offset = heritage::vehicles::tires::roadEnvelopeCamCenterOffsetM(
        description, contactLength);
    const VehicleScalar lateral = heritage::vehicles::tires::roadEnvelopeLateralHalfSpanM(
        description, contactWidth);

    std::vector<heritage::vehicles::tires::TireRoadEnvelopeSample> frontStep = {
        { true, -offset, 0.0 },
        { true, 0.0, 0.0 },
        { true, offset, 0.050 }
    };
    const auto front = heritage::vehicles::tires::evaluateTireRoadEnvelope(
        description, contactLength, frontStep);

    std::vector<heritage::vehicles::tires::TireRoadEnvelopeSample> rearStep = {
        { true, -offset, 0.050 },
        { true, 0.0, 0.0 },
        { true, offset, 0.0 }
    };
    const auto rear = heritage::vehicles::tires::evaluateTireRoadEnvelope(
        description, contactLength, rearStep);

    std::vector<heritage::vehicles::tires::TireRoadEnvelopeSample> flat = {
        { true, -offset, 0.0 },
        { true, 0.0, 0.0 },
        { true, offset, 0.0 }
    };
    const auto level = heritage::vehicles::tires::evaluateTireRoadEnvelope(
        description, contactLength, flat);

    // Full 3x3 footprint with the right half riding a 30 mm sharp feature.
    std::vector<heritage::vehicles::tires::TireRoadEnvelopeSample> splitStep;
    for (VehicleScalar y : { -lateral, VehicleScalar{0.0}, lateral })
    {
        for (VehicleScalar x : { -offset, VehicleScalar{0.0}, offset })
        {
            heritage::vehicles::tires::TireRoadEnvelopeSample sample;
            sample.valid = true;
            sample.longitudinalOffsetM = x;
            sample.lateralOffsetM = y;
            sample.roadHeightRelativeToCenterM = y > 0.001 ? 0.030 : 0.0;
            splitStep.push_back(sample);
        }
    }
    const auto split = heritage::vehicles::tires::evaluateTireRoadEnvelope(
        description, contactLength, splitStep);

    const auto coarsePattern = heritage::vehicles::tires::buildTireRoadEnvelopeSamplePattern(
        description, contactLength, contactWidth, false);
    const auto refinedPattern = heritage::vehicles::tires::buildTireRoadEnvelopeSamplePattern(
        description, contactLength, contactWidth, true);
    std::vector<heritage::vehicles::tires::TireRoadEnvelopeSample> partialSupport = {
        { true, -offset, 0.0 },
        { true, 0.0, 0.0 },
        { false, offset, 0.0 }
    };

    const VehicleScalar planeFront =
        heritage::vehicles::tires::roadEnvelopeLocalPlaneHeightM(
            offset, 0.17364817766693033, -0.984807753012208);
    const VehicleScalar planeRear =
        heritage::vehicles::tires::roadEnvelopeLocalPlaneHeightM(
            -offset, 0.17364817766693033, -0.984807753012208);
    const VehicleScalar plane2D =
        heritage::vehicles::tires::roadEnvelopeLocalPlaneHeightM(
            offset, lateral, 0.10, -0.08, -0.99);
    const VehicleScalar degeneratePlane =
        heritage::vehicles::tires::roadEnvelopeLocalPlaneHeightM(
            offset, 0.5, 1.0e-8);

    return front.valid && rear.valid && level.valid && split.valid
        && front.validSampleCount >= 3
        && front.effectiveRoadHeightM > 0.001
        && front.effectiveRoadHeightM < 0.050
        && rear.effectiveRoadHeightM > 0.001
        && std::abs(front.effectiveRoadHeightM - rear.effectiveRoadHeightM) < 1.0e-9
        && front.effectiveRoadSlopeRadians > 0.0
        && rear.effectiveRoadSlopeRadians < 0.0
        && std::abs(level.effectiveRoadHeightM) < 1.0e-9
        && std::abs(level.effectiveRoadSlopeRadians) < 1.0e-9
        && split.validSampleCount == 9
        && split.totalSampleCount == 9
        && split.supportedFraction > 0.999
        && split.roughnessHeightRangeM > 0.029
        && split.effectiveCrossSlopeRadians > 0.01
        && heritage::vehicles::tires::tireRoadEnvelopeNeedsHeightRefinement(
            description, splitStep)
        && coarsePattern.size() == 5
        && refinedPattern.size() == 9
        && heritage::vehicles::tires::tireRoadEnvelopeHasPartialSupport(
            partialSupport)
        && !heritage::vehicles::tires::tireRoadEnvelopeHasPartialSupport(
            flat)
        && std::abs(planeFront) > 0.001
        && std::abs(planeFront + planeRear) < 1.0e-12
        && std::abs(plane2D) > 1.0e-6
        && std::abs(degeneratePlane) < 1.0e-12;
}

bool tireThermalPressureAndGripStateAreRateStable()
{
    heritage::vehicles::tires::TireThermalDescription description;
    description.enabled = true;
    description.referenceTemperatureC = 20.0;
    description.initialTreadTemperatureC = 20.0;
    description.initialCarcassTemperatureC = 20.0;
    description.initialGasTemperatureC = 20.0;
    description.ambientTemperatureC = 20.0;
    description.roadTemperatureC = 20.0;
    description.referenceGaugePressurePa = 220000.0;
    description.treadHeatCapacityJPerK = 2500.0;
    description.carcassHeatCapacityJPerK = 5000.0;
    description.gasHeatCapacityJPerK = 150.0;
    description.treadToCarcassConductanceWPerK = 50.0;
    description.carcassToGasConductanceWPerK = 12.0;

    heritage::vehicles::tires::TireThermalInput input;
    input.grounded = true;
    input.forwardSpeedMps = 25.0;
    input.longitudinalSlipVelocityMps = 1.2;
    input.lateralSlipVelocityMps = 0.5;
    input.longitudinalForceN = 3200.0;
    input.lateralForceN = 1800.0;
    input.radialDissipationWatts = 200.0;
    input.rollingResistanceDissipationWatts = 180.0;
    input.contactPatchAreaM2 = 0.018;

    const auto integrateFor = [&](VehicleScalar dt) {
        heritage::vehicles::tires::TireThermalState state;
        heritage::vehicles::tires::TireThermalOutput output;
        const int steps = static_cast<int>(12.0 / dt);
        for (int i = 0; i < steps; ++i)
        {
            output = heritage::vehicles::tires::advanceTireThermal(
                description, input, dt, state);
        }
        return output;
    };

    const auto highRate = integrateFor(0.001);
    const auto lowRate = integrateFor(1.0 / 120.0);

    heritage::vehicles::tires::TireThermalState warmState;
    warmState.initialized = true;
    warmState.treadTemperatureC = 70.0;
    warmState.carcassTemperatureC = 60.0;
    warmState.gasTemperatureC = 55.0;
    const auto warm = heritage::vehicles::tires::evaluateTireThermalState(
        description, warmState);

    heritage::vehicles::tires::TireThermalState hotState = warmState;
    hotState.treadTemperatureC = 180.0;
    const auto hot = heritage::vehicles::tires::evaluateTireThermalState(
        description, hotState);

    return highRate.valid && lowRate.valid && warm.valid && hot.valid
        && highRate.treadTemperatureC > 24.0
        && highRate.carcassTemperatureC > 20.2
        && highRate.gasTemperatureC > 20.0
        && highRate.inflationPressurePa > 220000.0
        && highRate.slipDissipationWatts > 1000.0
        && std::abs(highRate.treadTemperatureC - lowRate.treadTemperatureC) < 0.20
        && std::abs(highRate.carcassTemperatureC - lowRate.carcassTemperatureC) < 0.20
        && std::abs(highRate.inflationPressurePa - lowRate.inflationPressurePa) < 250.0
        && warm.frictionScale > 1.0
        && hot.frictionScale < warm.frictionScale
        && warm.stiffnessScale < 1.0;
}


bool tireSpatialTreadThermalWearAndFlatSpotBehave()
{
    heritage::vehicles::tires::TireThermalDescription thermal;
    thermal.enabled = true;
    thermal.referenceTemperatureC = 20.0;
    thermal.optimumTreadTemperatureC = 70.0;

    heritage::vehicles::tires::TireWearDescription description;
    description.enabled = true;
    description.wearDepthPerJoule = 2.5e-8; // accelerated regression datum
    description.surfaceSlipHeatFraction = 0.25;
    description.surfaceHeatCapacityJPerKPerCell = 20.0;

    heritage::vehicles::tires::TireWearInput input;
    input.grounded = true;
    input.wheelRotationDegrees = 0.0;
    input.normalLoadN = 3500.0;
    input.nominalLoadN = 3500.0;
    input.inflationPressurePa = 220000.0;
    input.referencePressurePa = 220000.0;
    input.bulkTreadTemperatureC = 70.0;
    input.slipDissipationWatts = 6000.0;

    const auto integrateLocked = [&](VehicleScalar dt) {
        heritage::vehicles::tires::TireWearState state;
        heritage::vehicles::tires::TireWearOutput output;
        const int steps = static_cast<int>(1.0 / dt);
        for (int i = 0; i < steps; ++i)
            output = heritage::vehicles::tires::advanceTireWear(
                description, thermal, input, dt, state);
        return output;
    };

    const auto highRate = integrateLocked(0.001);
    const auto lowRate = integrateLocked(1.0 / 120.0);

    heritage::vehicles::tires::TireWearState rotatingState;
    heritage::vehicles::tires::TireWearOutput rotating;
    const VehicleScalar dt = 0.001;
    for (int i = 0; i < 1000; ++i)
    {
        auto rotatingInput = input;
        rotatingInput.wheelRotationDegrees = static_cast<VehicleScalar>(i) * 7.2;
        rotating = heritage::vehicles::tires::advanceTireWear(
            description, thermal, rotatingInput, dt, rotatingState);
    }

    heritage::vehicles::tires::TireWearState distributionState;
    auto highPressure = input;
    highPressure.slipDissipationWatts = 0.0;
    highPressure.inflationPressurePa = 300000.0;
    highPressure.camberAngleRadians = 0.15;
    const auto distribution = heritage::vehicles::tires::evaluateTireWearState(
        description, thermal, highPressure, distributionState);

    return highRate.valid && lowRate.valid && rotating.valid && distribution.valid
        && highRate.flatSpotDepthM > 0.00005
        && highRate.flatSpotSector == 0
        && highRate.contactTreadRadiusLossM > highRate.averageTreadRadiusLossM
        && highRate.contactRadiusVariationM > 0.00005
        && highRate.minimumTreadDepthM < highRate.averageTreadDepthM
        && highRate.hottestSurfaceTemperatureC > highRate.centerSurfaceTemperatureC
        && highRate.contactFrictionScale < 1.0
        && rotating.flatSpotDepthM < highRate.flatSpotDepthM * 0.45
        && std::abs(rotating.contactRadiusVariationM)
            < highRate.contactRadiusVariationM * 0.55
        && std::abs(highRate.minimumTreadDepthM - lowRate.minimumTreadDepthM) < 2.0e-5
        && distribution.centerLoadFraction > description.baseCenterLoadFraction
        && distribution.outsideLoadFraction > distribution.insideLoadFraction;
}

bool tireTreadContaminationPickupAndCleaningBehave()
{
    using heritage::physics::SurfaceMaterial;
    using namespace heritage::vehicles::tires;

    TireWearDescription wear;
    wear.enabled = true;
    TireThermalDescription thermal;
    TireWearState treadState;
    TireWearInput initialize;
    initialize.nominalLoadN = 3500.0;
    advanceTireWear(wear, thermal, initialize, 0.001, treadState);

    TireContaminationDescription description;
    description.enabled = true;

    TireContaminationInput input;
    input.grounded = true;
    input.surfaceMaterial = SurfaceMaterial::Grass;
    input.surfaceWetness = 0.35;
    input.normalLoadN = 3500.0;
    input.nominalLoadN = 3500.0;
    input.inflationPressurePa = 220000.0;
    input.referencePressurePa = 220000.0;
    input.forwardSpeedMps = 12.0;
    input.longitudinalSlipVelocityMps = 1.2;
    input.lateralSlipVelocityMps = 0.8;
    input.bulkTreadTemperatureC = 65.0;

    TireContaminationOutput dirty;
    for (int i = 0; i < 1500; ++i)
    {
        // Rotate through the material-fixed cells so pickup becomes a tire
        // history rather than a single global flag.
        input.wheelRotationDegrees = static_cast<VehicleScalar>(i) * 9.0;
        dirty = advanceTireContamination(
            description, wear, input, 0.001, treadState);
    }

    if (!dirty.valid
        || dirty.averageTotal < 0.02
        || dirty.contactOrganic <= 0.01
        || dirty.contactMudFilm <= 0.001
        || dirty.contactFrictionScale >= 0.999
        || dirty.roadHeatTransferScale >= 0.999
        || dirty.rollingResistanceScale <= 1.0)
    {
        return false;
    }

    const VehicleScalar dirtyAverage = dirty.averageTotal;
    const VehicleScalar dirtyFriction = dirty.contactFrictionScale;

    input.surfaceMaterial = SurfaceMaterial::Asphalt;
    input.surfaceWetness = 0.0;
    input.longitudinalSlipVelocityMps = 2.5;
    input.lateralSlipVelocityMps = 0.5;
    TireContaminationOutput cleaned;
    for (int i = 0; i < 8000; ++i)
    {
        input.wheelRotationDegrees += 12.0;
        cleaned = advanceTireContamination(
            description, wear, input, 0.001, treadState);
    }

    // TIRE06 footprint composition must reach TIRE11 before the centre ray
    // crosses a boundary. A tire whose centre remains on asphalt but whose
    // 2D footprint is 25% grass should pick up organics while the supported
    // asphalt fraction simultaneously provides some cleaning.
    TireWearState splitState;
    advanceTireWear(wear, thermal, initialize, 0.001, splitState);
    TireContaminationInput splitInput = input;
    splitInput.surfaceMaterial = SurfaceMaterial::Asphalt;
    splitInput.surfaceWetness = 0.0;
    splitInput.surfaceRubberDebrisFraction = 0.0;
    splitInput.footprintSurfaceBlendValid = true;
    splitInput.footprintGrassFraction = 0.25;
    splitInput.footprintDirtFraction = 0.0;
    splitInput.footprintGravelFraction = 0.0;
    splitInput.footprintCleanHardFraction = 0.75;
    splitInput.footprintAverageWetness = 0.20;
    TireContaminationOutput splitEdge;
    for (int i = 0; i < 1400; ++i)
    {
        splitInput.wheelRotationDegrees += 11.0;
        splitEdge = advanceTireContamination(
            description, wear, splitInput, 0.001, splitState);
    }

    // Dynamic-track rubber pickup is an independent source channel and does
    // not require inventing another static collision material.
    input.footprintSurfaceBlendValid = false;
    input.surfaceRubberDebrisFraction = 1.0;
    TireContaminationOutput rubber;
    for (int i = 0; i < 1200; ++i)
    {
        input.wheelRotationDegrees += 10.0;
        rubber = advanceTireContamination(
            description, wear, input, 0.001, treadState);
    }

    return cleaned.valid && rubber.valid
        && cleaned.averageTotal < dirtyAverage * 0.80
        && cleaned.contactFrictionScale > dirtyFriction
        && cleaned.cleaningRatePerSecond > 0.2
        && splitEdge.valid
        && splitEdge.contactOrganic > 0.002
        && splitEdge.cleaningRatePerSecond > 0.0
        && rubber.contactRubberPickup > cleaned.contactRubberPickup
        && rubber.maximumCellTotal >= rubber.averageTotal;
}

bool tireWetSurfaceHydroplaningAndDrainageBehave()
{
    using heritage::physics::SurfaceMaterial;
    using namespace heritage::vehicles::tires;

    TireWearDescription wear;
    wear.enabled = true;
    TireThermalDescription thermal;
    TireWearState treadState;
    TireWearInput initialize;
    initialize.nominalLoadN = 3500.0;
    advanceTireWear(wear, thermal, initialize, 0.001, treadState);

    TireWetSurfaceDescription description;
    description.enabled = true;

    TireWetSurfaceInput input;
    input.grounded = true;
    input.surfaceMaterial = SurfaceMaterial::Asphalt;
    input.surfaceWetness = 1.0;
    input.normalLoadN = 3500.0;
    input.inflationPressurePa = 220000.0;
    input.referencePressurePa = 220000.0;
    input.forwardSpeedMps = 25.0;
    input.contactPatchLengthM = 0.125;
    input.contactPatchWidthM = 0.200;
    input.contactPatchAreaM2 = 0.025;
    input.currentAverageTreadDepthM = 0.0070;
    input.initialTreadDepthM = 0.0070;
    input.minimumTreadDepthM = 0.0005;

    const TireWetSurfaceOutput flooded = evaluateTireWetSurface(
        description, wear, input, treadState);

    TireWetSurfaceInput thinFilm = input;
    thinFilm.surfaceWetness = 0.05; // 0.15 mm with the default bridge.
    thinFilm.forwardSpeedMps = 10.0;
    const TireWetSurfaceOutput thin = evaluateTireWetSurface(
        description, wear, thinFilm, treadState);

    TireWetSurfaceInput worn = input;
    worn.currentAverageTreadDepthM = 0.0015;
    const TireWetSurfaceOutput wornFlooded = evaluateTireWetSurface(
        description, wear, worn, treadState);

    TireWetSurfaceInput lowPressure = input;
    lowPressure.inflationPressurePa = 150000.0;
    const TireWetSurfaceOutput lowPressureFlooded = evaluateTireWetSurface(
        description, wear, lowPressure, treadState);

    TireWetSurfaceInput highPressure = input;
    highPressure.inflationPressurePa = 300000.0;
    const TireWetSurfaceOutput highPressureFlooded = evaluateTireWetSurface(
        description, wear, highPressure, treadState);

    TireWetSurfaceOutput retained;
    for (int i = 0; i < 800; ++i)
    {
        input.wheelRotationDegrees = static_cast<VehicleScalar>(i) * 9.0;
        retained = advanceTireWetSurface(
            description, wear, input, 0.001, treadState);
    }
    const VehicleScalar retainedWet = retained.averageRetainedWaterDepthM;

    TireWetSurfaceInput dry = input;
    dry.surfaceWetness = 0.0;
    dry.forwardSpeedMps = 20.0;
    TireWetSurfaceOutput drying;
    for (int i = 0; i < 2500; ++i)
    {
        dry.wheelRotationDegrees += 10.0;
        drying = advanceTireWetSurface(
            description, wear, dry, 0.001, treadState);
    }

    return flooded.valid && thin.valid && wornFlooded.valid
        && lowPressureFlooded.valid && highPressureFlooded.valid
        && flooded.roadWaterDepthM > 0.0029
        && flooded.waterWedgeFraction > 0.5
        && flooded.hydroplaningFraction > 0.25
        && flooded.pavementContactFraction < 0.75
        && flooded.frictionScale < 0.75
        && flooded.hydrodynamicDragN > 20.0
        && flooded.classicalPressureHydroplaningSpeedMps > 20.0
        && thin.hydroplaningFraction < flooded.hydroplaningFraction * 0.20
        && thin.frictionScale < 1.0
        && wornFlooded.drainageDemandRatio > flooded.drainageDemandRatio * 2.0
        && wornFlooded.hydroplaningFraction >= flooded.hydroplaningFraction
        && lowPressureFlooded.classicalPressureHydroplaningSpeedMps
            < highPressureFlooded.classicalPressureHydroplaningSpeedMps
        && lowPressureFlooded.hydroplaningFraction
            >= highPressureFlooded.hydroplaningFraction
        && retainedWet > 1.0e-5
        && drying.averageRetainedWaterDepthM < retainedWet * 0.75;
}

bool tireWinterSurfaceIceSnowAndStudsBehave()
{
    using namespace heritage::vehicles::tires;

    TireWearDescription wear;
    wear.enabled = true;
    TireWearState state;
    state.initialized = true;
    for (auto& cell : state.cells)
        cell.remainingTreadDepthM = wear.initialTreadDepthM;

    TireWinterSurfaceDescription summerLike;
    summerLike.enabled = true;
    summerLike.winterCompoundEffectiveness = 0.10;
    summerLike.sipingDensity = 0.08;
    summerLike.snowTreadInterlock = 0.28;

    TireWinterSurfaceInput ice;
    ice.grounded = true;
    ice.surfaceMaterial = heritage::physics::SurfaceMaterial::Ice;
    ice.surfaceTemperatureC = -15.0;
    ice.normalLoadN = 3500.0;
    ice.nominalLoadN = 3500.0;
    ice.forwardSpeedMps = 12.0;
    ice.longitudinalSlipVelocityMps = 0.8;
    ice.currentAverageTreadDepthM = wear.initialTreadDepthM;
    ice.initialTreadDepthM = wear.initialTreadDepthM;
    ice.minimumTreadDepthM = wear.minimumTreadDepthM;

    const auto coldIce = evaluateTireWinterSurface(
        summerLike, wear, ice, state);
    auto warmIceInput = ice;
    warmIceInput.surfaceTemperatureC = -0.5;
    warmIceInput.longitudinalSlipVelocityMps = 2.5;
    const auto warmIce = evaluateTireWinterSurface(
        summerLike, wear, warmIceInput, state);

    auto studded = summerLike;
    studded.studsEnabled = true;
    studded.studCount = 120;
    studded.studProtrusionM = 0.0012;
    const auto studdedIce = evaluateTireWinterSurface(
        studded, wear, warmIceInput, state);

    auto winterTire = summerLike;
    winterTire.winterCompoundEffectiveness = 0.95;
    winterTire.sipingDensity = 0.90;
    winterTire.snowTreadInterlock = 0.85;
    const auto winterIce = evaluateTireWinterSurface(
        winterTire, wear, warmIceInput, state);

    TireWinterSurfaceInput snow = ice;
    snow.surfaceMaterial = heritage::physics::SurfaceMaterial::Snow;
    snow.surfaceTemperatureC = -5.0;
    snow.forwardSpeedMps = 8.0;
    snow.longitudinalSlipVelocityMps = 0.9;
    const auto snowInitial = evaluateTireWinterSurface(
        winterTire, wear, snow, state);

    const auto packedFor = [&](VehicleScalar dt) {
        TireWearState packedState;
        packedState.initialized = true;
        for (auto& cell : packedState.cells)
            cell.remainingTreadDepthM = wear.initialTreadDepthM;
        TireWinterSurfaceOutput out;
        const int steps = static_cast<int>(1.0 / dt);
        for (int i = 0; i < steps; ++i)
        {
            TireWinterSurfaceInput stepInput = snow;
            stepInput.wheelRotationDegrees = 360.0 * static_cast<VehicleScalar>(i) * dt * 2.0;
            out = advanceTireWinterSurface(
                winterTire, wear, stepInput, dt, packedState);
        }
        return out;
    };
    const auto packed1000 = packedFor(0.001);
    const auto packed120 = packedFor(1.0 / 120.0);

    TireWearState cleaningState;
    cleaningState.initialized = true;
    for (auto& cell : cleaningState.cells)
    {
        cell.remainingTreadDepthM = wear.initialTreadDepthM;
        cell.packedSnowFraction = 0.65;
    }
    TireWinterSurfaceInput clean = snow;
    clean.surfaceMaterial = heritage::physics::SurfaceMaterial::Asphalt;
    clean.forwardSpeedMps = 20.0;
    clean.longitudinalSlipVelocityMps = 0.4;
    TireWinterSurfaceOutput cleaned;
    for (int i = 0; i < 1000; ++i)
    {
        clean.wheelRotationDegrees = 360.0 * static_cast<VehicleScalar>(i) * 0.001 * 10.0;
        cleaned = advanceTireWinterSurface(
            winterTire, wear, clean, 0.001, cleaningState);
    }

    TireWinterSurfaceInput split = snow;
    split.footprintSurfaceBlendValid = true;
    split.footprintSnowFraction = 0.5;
    split.footprintIceFraction = 0.5;
    split.surfaceMaterial = heritage::physics::SurfaceMaterial::Snow;
    const auto mixed = evaluateTireWinterSurface(
        winterTire, wear, split, state);

    return coldIce.valid && warmIce.valid && studdedIce.valid
        && winterIce.valid && snowInitial.valid && mixed.valid
        && coldIce.iceFraction > 0.99
        && coldIce.frictionScale > warmIce.frictionScale
        && warmIce.iceMeltFilmDepthM > coldIce.iceMeltFilmDepthM
        && studdedIce.studFrictionContribution > 0.10
        && studdedIce.frictionScale > warmIce.frictionScale + 0.08
        && winterIce.frictionScale > warmIce.frictionScale
        && snowInitial.snowFraction > 0.99
        && snowInitial.frictionScale > warmIce.frictionScale
        && snowInitial.snowInterlockContribution > 0.05
        && packed1000.contactPackedSnowFraction > 0.05
        && std::abs(packed1000.contactPackedSnowFraction
            - packed120.contactPackedSnowFraction) < 0.06
        && cleaned.averagePackedSnowFraction < 0.40
        && std::abs(mixed.snowFraction - 0.5) < 1.0e-9
        && std::abs(mixed.iceFraction - 0.5) < 1.0e-9
        && mixed.frictionScale > warmIce.frictionScale
        && mixed.frictionScale < snowInitial.frictionScale;
}

bool tireShallowGranularGravelDirtBehaves()
{
    using heritage::physics::SurfaceMaterial;
    using namespace heritage::vehicles::tires;

    TireShallowGranularDescription roadTire;
    roadTire.enabled = true;

    TireShallowGranularInput gravel;
    gravel.grounded = true;
    gravel.surfaceMaterial = SurfaceMaterial::Gravel;
    gravel.surfaceWetness = 0.0;
    gravel.normalLoadN = 3500.0;
    gravel.nominalLoadN = 3500.0;
    gravel.forwardSpeedMps = 22.0;
    gravel.longitudinalSlipVelocityMps = 2.2;
    gravel.lateralSlipVelocityMps = 2.4;
    gravel.slipRatio = 0.12;
    gravel.slipAngleRadians = 8.0 * 3.14159265358979323846 / 180.0;
    gravel.unloadedRadiusM = 0.298;
    gravel.contactPatchLengthM = 0.125;
    gravel.contactPatchWidthM = 0.200;
    gravel.contactPatchAreaM2 = 0.025;
    gravel.currentAverageTreadDepthM = 0.0070;
    gravel.initialTreadDepthM = 0.0070;
    gravel.minimumTreadDepthM = 0.0005;

    const auto fullGravel = evaluateTireShallowGranular(roadTire, gravel);

    auto lowSlip = gravel;
    lowSlip.longitudinalSlipVelocityMps = 0.25;
    lowSlip.lateralSlipVelocityMps = 0.20;
    lowSlip.slipRatio = 0.015;
    lowSlip.slipAngleRadians = 0.8 * 3.14159265358979323846 / 180.0;
    const auto lowSlipGravel = evaluateTireShallowGranular(roadTire, lowSlip);

    auto aggressive = roadTire;
    aggressive.treadAggressiveness = 0.90;
    aggressive.treadEdgeDensity = 0.85;
    aggressive.openVoidRatio = 0.55;
    aggressive.granularShearCoupling = 0.90;
    aggressive.bulldozingCoupling = 0.80;
    const auto rallyGravel = evaluateTireShallowGranular(aggressive, gravel);

    auto worn = gravel;
    worn.currentAverageTreadDepthM = 0.0010;
    const auto wornGravel = evaluateTireShallowGranular(roadTire, worn);

    auto wetDirt = gravel;
    wetDirt.surfaceMaterial = SurfaceMaterial::Dirt;
    wetDirt.surfaceWetness = 0.70;
    wetDirt.footprintSurfaceBlendValid = false;
    const auto wetDirtResult = evaluateTireShallowGranular(roadTire, wetDirt);

    auto dryDirt = wetDirt;
    dryDirt.surfaceWetness = 0.0;
    const auto dryDirtResult = evaluateTireShallowGranular(roadTire, dryDirt);

    auto split = gravel;
    split.surfaceMaterial = SurfaceMaterial::Asphalt;
    split.footprintSurfaceBlendValid = true;
    split.footprintGravelFraction = 0.50;
    split.footprintDirtFraction = 0.0;
    split.footprintAverageWetness = 0.0;
    const auto halfGravel = evaluateTireShallowGranular(roadTire, split);

    auto straight = gravel;
    straight.lateralSlipVelocityMps = 0.0;
    straight.slipAngleRadians = 0.0;
    const auto straightGravel = evaluateTireShallowGranular(roadTire, straight);

    return fullGravel.valid && lowSlipGravel.valid && rallyGravel.valid
        && wornGravel.valid && wetDirtResult.valid && dryDirtResult.valid
        && halfGravel.valid && straightGravel.valid
        && fullGravel.granularSurfaceFraction > 0.99
        && fullGravel.sinkageM > 0.004
        && fullGravel.sinkageM < 0.0301
        && fullGravel.contactPressurePa > 100000.0
        && fullGravel.soilShearCapacityN > 1000.0
        && fullGravel.longitudinalShearMobilization
            > lowSlipGravel.longitudinalShearMobilization
        && fullGravel.lateralShearMobilization
            > lowSlipGravel.lateralShearMobilization
        && std::abs(fullGravel.longitudinalShearForceN)
            > std::abs(lowSlipGravel.longitudinalShearForceN)
        && std::abs(fullGravel.lateralShearForceN)
            > std::abs(lowSlipGravel.lateralShearForceN)
        && fullGravel.lateralBulldozingForceN < 0.0
        && std::abs(straightGravel.lateralBulldozingForceN) < 1.0e-9
        && fullGravel.plowingDragN > 20.0
        && fullGravel.compactionPowerW > 100.0
        && rallyGravel.treadEffectiveness > fullGravel.treadEffectiveness
        && std::abs(rallyGravel.lateralShearForceN)
            > std::abs(fullGravel.lateralShearForceN)
        && wornGravel.treadEffectiveness < fullGravel.treadEffectiveness
        && std::abs(wornGravel.longitudinalShearForceN)
            < std::abs(fullGravel.longitudinalShearForceN)
        && wetDirtResult.sinkageM > dryDirtResult.sinkageM
        && wetDirtResult.frictionScale < dryDirtResult.frictionScale
        && std::abs(halfGravel.granularSurfaceFraction - 0.5) < 1.0e-9
        && halfGravel.sinkageM < fullGravel.sinkageM
        && halfGravel.frictionScale > fullGravel.frictionScale;
}

bool tireDeformableTerrainPersistenceBehaves()
{
    using heritage::physics::SurfaceField;
    using heritage::physics::SurfaceFieldDescription;
    using heritage::physics::SurfaceMaterial;
    using heritage::vehicles::tires::TireDeformableTerrainDescription;
    using heritage::vehicles::tires::TireDeformableTerrainInput;
    using heritage::vehicles::tires::deformableTerrainInitialSurfaceState;
    using heritage::vehicles::tires::evaluateTireDeformableTerrain;
    using heritage::vehicles::tires::tireDeformableTerrainFieldUpdate;

    TireDeformableTerrainDescription tire;
    tire.enabled = true;
    tire.treadAggressiveness = 0.55;
    tire.treadEdgeDensity = 0.60;
    tire.openVoidRatio = 0.48;
    tire.soilShearCoupling = 0.80;
    tire.bulldozingCoupling = 0.75;
    tire.plowingCoupling = 0.80;
    tire.flotationCoupling = 0.35;

    TireDeformableTerrainInput input;
    input.grounded = true;
    input.surfaceMaterial = SurfaceMaterial::Sand;
    input.surfaceWetness = 0.05;
    input.normalLoadN = 3500.0;
    input.nominalLoadN = 3500.0;
    input.forwardSpeedMps = 8.0;
    input.longitudinalSlipVelocityMps = 2.0;
    input.lateralSlipVelocityMps = 1.4;
    input.slipRatio = 0.22;
    input.slipAngleRadians = 7.0 * kPi / 180.0;
    input.unloadedRadiusM = 0.298;
    input.contactPatchLengthM = 0.145;
    input.contactPatchWidthM = 0.205;
    input.contactPatchAreaM2 = 0.0297;
    input.currentAverageTreadDepthM = 0.0070;
    input.initialTreadDepthM = 0.0070;
    input.minimumTreadDepthM = 0.0005;

    SurfaceField field(SurfaceFieldDescription{0.25f, 128u});
    const heritage::math::DVec3 point{3.1, 0.0, -2.6};
    const auto initial = deformableTerrainInitialSurfaceState(
        input.surfaceMaterial, input.surfaceWetness);
    input.surfaceField = field.sample(point, input.surfaceMaterial, initial);
    const auto first = evaluateTireDeformableTerrain(tire, input);
    if (!first.valid || first.totalSinkageM <= 0.01
        || first.shearCapacityN <= 500.0
        || first.plowingDragN <= 20.0
        || first.mfFrictionScale >= 0.5)
    {
        return false;
    }

    // Let one wheel work the same terrain cell for one second. The field must
    // remember plastic rutting, compaction, shear history and displaced volume.
    const VehicleScalar dt = 0.001;
    for (int i = 0; i < 1000; ++i)
    {
        input.surfaceField = field.sample(point, input.surfaceMaterial, initial);
        const auto out = evaluateTireDeformableTerrain(tire, input);
        field.apply(point, tireDeformableTerrainFieldUpdate(
            tire, input, out, dt));
    }
    const auto worked = field.sample(point, input.surfaceMaterial, initial);
    if (!worked.valid || worked.rutDepthM <= 0.002f
        || worked.compaction <= 0.05f
        || worked.longitudinalShearHistoryM <= 0.1f
        || worked.displacedVolumeM3 <= 0.0f
        || worked.passCount == 0u
        || field.cellCount() != 1u)
    {
        return false;
    }

    input.surfaceField = worked;
    const auto second = evaluateTireDeformableTerrain(tire, input);
    if (!second.valid
        || second.persistentRutDepthM <= 0.002
        || second.compaction <= first.compaction
        || second.totalSinkageM <= second.persistentRutDepthM)
    {
        return false;
    }

    // An untouched cell must retain virgin state: deformation belongs to the
    // world location, not globally to the material or tire.
    const heritage::math::DVec3 untouchedPoint{6.0, 0.0, -2.6};
    const auto untouched = field.sample(
        untouchedPoint, input.surfaceMaterial, initial);
    if (untouched.rutDepthM != 0.0f || untouched.compaction != 0.0f
        || untouched.passCount != 0u)
    {
        return false;
    }

    // Surface families must remain distinguishable and partial footprint
    // blending must scale the deformable contribution rather than becoming a
    // binary center-ray switch.
    auto mudInput = input;
    mudInput.surfaceMaterial = SurfaceMaterial::Mud;
    mudInput.surfaceWetness = 0.9;
    mudInput.surfaceField = field.sample(
        point, SurfaceMaterial::Mud,
        deformableTerrainInitialSurfaceState(SurfaceMaterial::Mud, 0.9));
    const auto mud = evaluateTireDeformableTerrain(tire, mudInput);

    auto snowInput = input;
    snowInput.surfaceMaterial = SurfaceMaterial::DeepSnow;
    snowInput.surfaceWetness = 0.05;
    snowInput.surfaceField = field.sample(
        point, SurfaceMaterial::DeepSnow,
        deformableTerrainInitialSurfaceState(SurfaceMaterial::DeepSnow, 0.05));
    const auto snow = evaluateTireDeformableTerrain(tire, snowInput);

    auto partial = input;
    partial.surfaceMaterial = SurfaceMaterial::Asphalt;
    partial.footprintSurfaceBlendValid = true;
    partial.footprintSandFraction = 0.50;
    partial.footprintMudFraction = 0.0;
    partial.footprintSoftSoilFraction = 0.0;
    partial.footprintDeepSnowFraction = 0.0;
    partial.surfaceField = field.sample(point, SurfaceMaterial::Sand, initial);
    const auto halfSand = evaluateTireDeformableTerrain(tire, partial);

    if (!mud.valid || !snow.valid || !halfSand.valid
        || mud.moisture <= first.moisture
        || snow.totalSinkageM <= 0.01
        || std::abs(halfSand.terrainSurfaceFraction - 0.5) > 1.0e-9
        || halfSand.additionalContactCapacityN >= second.additionalContactCapacityN)
    {
        return false;
    }

    // TIRE15B scene-authored surface mechanics must feed the same solver, not
    // merely survive import as decorative metadata. A deliberately softer,
    // higher-drag sand profile should measurably alter the terrain response.
    auto authoredSandProperties =
        heritage::physics::defaultSurfaceMaterialProperties(
            SurfaceMaterial::Sand).deformable;
    authoredSandProperties.authored = true;
    authoredSandProperties.bekkerKphi *= 0.35;
    authoredSandProperties.rollingResistanceScale = 5.5;
    auto authoredSandInput = input;
    authoredSandInput.surfaceProperties = authoredSandProperties;
    authoredSandInput.surfacePropertiesValid = true;
    const auto authoredSandInitial = deformableTerrainInitialSurfaceState(
        authoredSandProperties, authoredSandInput.surfaceWetness);
    SurfaceField authoredSandField(SurfaceFieldDescription{0.25f, 128u});
    authoredSandInput.surfaceField = authoredSandField.sample(
        point, SurfaceMaterial::Sand, authoredSandInitial);
    const auto authoredSand = evaluateTireDeformableTerrain(
        tire, authoredSandInput);
    if (!authoredSand.valid
        || authoredSand.totalSinkageM <= first.totalSinkageM
        || authoredSand.rollingResistanceScale <= first.rollingResistanceScale)
    {
        return false;
    }

    // Persistent integration should be reasonably timestep insensitive.
    const auto runRate = [&](VehicleScalar step, int count) {
        SurfaceField rateField(SurfaceFieldDescription{0.25f, 128u});
        TireDeformableTerrainInput rateInput = input;
        for (int i = 0; i < count; ++i)
        {
            rateInput.surfaceField = rateField.sample(
                point, rateInput.surfaceMaterial, initial);
            const auto out = evaluateTireDeformableTerrain(tire, rateInput);
            rateField.apply(point, tireDeformableTerrainFieldUpdate(
                tire, rateInput, out, step));
        }
        return rateField.sample(point, rateInput.surfaceMaterial, initial);
    };
    const auto at1000 = runRate(0.001, 1000);
    const auto at125 = runRate(0.008, 125);
    return std::abs(at1000.compaction - at125.compaction) < 0.04f
        && std::abs(at1000.rutDepthM - at125.rutDepthM) < 0.01f;
}

bool tirePropertyFileImporterMapsMf62AndMotorcycleData()
{
    const std::string tir = R"TIR(
[UNITS]
LENGTH = 'mm'
FORCE = 'N'
ANGLE = 'deg'
MASS = 'kg'
TIME = 's'
[MODEL]
FITTYP = 62
TYRESIDE = 'RIGHT'
LONGVL = 16666.6666667
DAMP_LSG = 1.0
VX_STBL = 1000.0
[DIMENSION]
UNLOADED_RADIUS = 300.0
WIDTH = 180.0
RIM_RADIUS = 215.9
[OPERATING_CONDITIONS]
NOMPRES = 0.22
INFLPRES = 0.23
[INERTIA]
BELT_MASS = 6.0
BELT_IXX = 180000.0
BELT_IYY = 330000.0
[VERTICAL]
FNOMIN = 1800.0
VERTICAL_STIFFNESS = 220.0
VERTICAL_DAMPING = 1.8
BREFF = 8.386
DREFF = 0.25826
FREFF = 0.07394
Q_RE0 = 1.0
Q_V1 = 0.00076
Q_V2 = 0.0
MC_CONTOUR_A = 0.50
MC_CONTOUR_B = 0.50
[CONTACT_PATCH]
Q_RA1 = 0.67594
Q_RA2 = 0.73800
Q_RB1 = 1.04487
Q_RB2 = -1.19176
ELLIPS_SHIFT = 0.82
ELLIPS_LENGTH = 115.0
ELLIPS_HEIGHT = 55.0
ELLIPS_ORDER = 2.0
ELLIPS_MAX_STEP = 100.0
ELLIPS_NWIDTH = 3
ELLIPS_NLENGTH = 3
ENV_C1 = 1.0
ENV_C2 = 1.0
[STRUCTURAL]
LONGITUDINAL_STIFFNESS = 800.0
LATERAL_STIFFNESS = 650.0
YAW_STIFFNESS = 78539.81633974483
FREQ_LONG = 65.0
FREQ_LAT = 55.0
FREQ_YAW = 50.0
FREQ_WINDUP = 75.0
DAMP_LONG = 0.18
DAMP_LAT = 0.20
DAMP_YAW = 0.22
DAMP_WINDUP = 0.18
DAMP_RESIDUAL = 0.02
DAMP_VLOW = 0.10
Q_BVX = 0.0
Q_BVT = 0.0
[HERITAGE_THERMAL]
ENABLED = 1
REFERENCE_TEMP_C = 20.0
INITIAL_TREAD_C = 20.0
INITIAL_CARCASS_C = 20.0
INITIAL_GAS_C = 20.0
AMBIENT_TEMP_C = 20.0
ROAD_TEMP_C = 22.0
AMBIENT_PRESSURE_PA = 101325.0
TREAD_CAPACITY_JPK = 4200.0
CARCASS_CAPACITY_JPK = 9500.0
GAS_CAPACITY_JPK = 220.0
K_TREAD_CARCASS_WPK = 55.0
K_TREAD_ROAD_WPK = 90.0
K_TREAD_AIR_WPK = 10.0
K_CARCASS_AIR_WPK = 8.0
K_CARCASS_GAS_WPK = 10.0
K_GAS_AMBIENT_WPK = 2.0
K_TREAD_AIR_SPEED = 0.70
K_CARCASS_AIR_SPEED = 0.35
SLIP_HEAT_TREAD_FRACTION = 0.85
SLIP_HEAT_EFFICIENCY = 0.92
CARCASS_LOSS_EFFICIENCY = 0.95
OPTIMUM_TREAD_C = 70.0
COLD_SPAN_C = 60.0
HOT_SPAN_C = 70.0
MAX_COLD_GRIP_LOSS = 0.10
MAX_HOT_GRIP_LOSS = 0.28
MIN_FRICTION_SCALE = 0.65
MAX_FRICTION_SCALE = 1.12
STIFFNESS_TEMP_SLOPE = -0.0012
MIN_STIFFNESS_SCALE = 0.78
MAX_STIFFNESS_SCALE = 1.10
[HERITAGE_TREAD_STATE]
ENABLED = 1
INITIAL_TREAD_DEPTH_M = 0.007
MINIMUM_TREAD_DEPTH_M = 0.0005
WEAR_DEPTH_PER_J = 1.0e-9
WEAR_LOAD_EXPONENT = 0.30
WEAR_TEMP_SENSITIVITY_PER_C = 0.018
MIN_WEAR_TEMP_SCALE = 0.35
MAX_WEAR_TEMP_SCALE = 6.0
SURFACE_CELL_CAPACITY_JPK = 24.0
SURFACE_SLIP_HEAT_FRACTION = 0.18
SURFACE_TO_BULK_HZ = 2.0
CIRCUMFERENTIAL_DIFFUSION_HZ = 0.8
LATERAL_DIFFUSION_HZ = 1.2
MAX_SURFACE_OFFSET_C = 120.0
BASE_CENTER_LOAD_FRACTION = 0.40
PRESSURE_CENTER_BIAS_GAIN = 0.25
CAMBER_SHOULDER_BIAS_PER_RAD = 0.90
MAX_SHOULDER_BIAS = 0.30
MAX_WEAR_FRICTION_LOSS = 0.18
WEAR_FRICTION_EXPONENT = 3.0
FLATSPOT_FRICTION_LOSS_PER_MM = 0.015
MAX_FLATSPOT_FRICTION_LOSS = 0.12
[HERITAGE_CONTAMINATION]
ENABLED = 1
GRASS_ORGANIC_PICKUP_HZ = 2.2
DIRT_MINERAL_PICKUP_HZ = 1.8
GRAVEL_FINES_PICKUP_HZ = 1.4
RUBBER_PICKUP_HZ = 1.2
MUD_FILM_PICKUP_HZ = 2.0
BASE_HARD_CLEAN_HZ = 0.22
SPEED_CLEAN_PER_M = 0.010
SLIP_CLEAN_PER_M = 0.080
HOT_CLEAN_PER_C = 0.003
HOT_CLEAN_THRESHOLD_C = 55.0
ORGANIC_RETENTION = 0.85
MINERAL_RETENTION = 0.60
GRAVEL_RETENTION = 0.50
RUBBER_RETENTION = 0.75
MUD_RETENTION = 0.95
ORGANIC_MAX_FRICTION_LOSS = 0.22
MINERAL_MAX_FRICTION_LOSS = 0.10
GRAVEL_MAX_FRICTION_LOSS = 0.14
RUBBER_MAX_FRICTION_LOSS = 0.12
MUD_MAX_FRICTION_LOSS = 0.34
MAX_COMBINED_FRICTION_LOSS = 0.48
ORGANIC_HEAT_INSULATION = 0.14
MINERAL_HEAT_INSULATION = 0.06
GRAVEL_HEAT_INSULATION = 0.04
RUBBER_HEAT_INSULATION = 0.10
MUD_HEAT_INSULATION = 0.32
MIN_ROAD_HEAT_TRANSFER = 0.48
ORGANIC_RR_GAIN = 0.12
MINERAL_RR_GAIN = 0.05
GRAVEL_RR_GAIN = 0.10
RUBBER_RR_GAIN = 0.04
MUD_RR_GAIN = 0.25
MAX_RR_SCALE = 1.55
[HERITAGE_WET_SURFACE]
ENABLED = 1
WETNESS_ONE_WATER_DEPTH_M = 0.0030
MIN_ACTIVE_WATER_DEPTH_M = 0.00003
FULLY_WETTED_WATER_DEPTH_M = 0.00025
TREAD_VOID_RATIO = 0.30
DRAINAGE_EFFICIENCY = 0.82
DRAINAGE_REFERENCE_SPEED_MPS = 12.0
MIN_DRAINAGE_TREAD_DEPTH_M = 0.0005
WATER_DENSITY_KGM3 = 997.0
HYDRO_LIFT_COEFFICIENT = 0.70
HYDRO_DRAG_COEFFICIENT = 0.85
DRAINAGE_ONSET_RATIO = 0.18
DRAINAGE_FULL_RATIO = 1.10
MAX_HYDROPLANING_FRACTION = 0.985
THIN_FILM_MAX_FRICTION_LOSS = 0.20
THIN_FILM_SPEED_REFERENCE_MPS = 15.0
HYDRO_FRICTION_FLOOR = 0.055
HYDRO_STIFFNESS_FLOOR = 0.10
HYDRO_RELAXATION_GAIN = 1.50
MAX_RELAXATION_SCALE = 2.75
WET_RR_GAIN = 0.12
MAX_RR_SCALE = 1.45
WET_ROAD_HEAT_TRANSFER_GAIN = 0.65
MAX_ROAD_HEAT_TRANSFER_SCALE = 1.80
RETAINED_WATER_MAX_DEPTH_M = 0.0008
RETAINED_WATER_PICKUP_HZ = 10.0
RETAINED_WATER_RELEASE_HZ = 2.5
RETAINED_WATER_SPEED_RELEASE_PER_M = 0.018
[HERITAGE_WINTER_SURFACE]
ENABLED = 1
WINTER_COMPOUND_EFFECTIVENESS = 0.90
SIPING_DENSITY = 0.80
SNOW_TREAD_INTERLOCK = 0.72
SNOW_SELF_CLEANING = 0.65
STUDS_ENABLED = 1
STUD_COUNT = 120
STUD_PROTRUSION_M = 0.0012
[HERITAGE_SHALLOW_GRANULAR]
ENABLED = 1
TREAD_AGGRESSIVENESS = 0.65
TREAD_EDGE_DENSITY = 0.70
OPEN_VOID_RATIO = 0.42
GRANULAR_SHEAR_COUPLING = 0.80
BULLDOZING_COUPLING = 0.75
PLOWING_COUPLING = 0.85
MIN_WORN_TREAD_EFFECTIVENESS = 0.35
TREAD_DEPTH_EFFECT_EXPONENT = 0.80
MAX_SINKAGE_M = 0.040
MAX_GRANULAR_FORCE_RATIO = 0.75
MAX_PLOWING_FORCE_RATIO = 0.40
MIN_BASE_FRICTION_SCALE = 0.20
MAX_BASE_FRICTION_SCALE = 0.80
[HERITAGE_DEFORMABLE_TERRAIN]
ENABLED = 1
TREAD_AGGRESSIVENESS = 0.72
TREAD_EDGE_DENSITY = 0.68
OPEN_VOID_RATIO = 0.52
SOIL_SHEAR_COUPLING = 0.88
BULLDOZING_COUPLING = 0.76
PLOWING_COUPLING = 0.82
FLOTATION_COUPLING = 0.44
MIN_WORN_TREAD_EFFECTIVENESS = 0.32
TREAD_DEPTH_EFFECT_EXPONENT = 0.86
MAX_SINKAGE_M = 0.42
MAX_TERRAIN_FORCE_RATIO = 1.35
MAX_PLOWING_FORCE_RATIO = 0.78
MIN_MF_FRICTION_SCALE = 0.05
MAX_MF_FRICTION_SCALE = 0.32
[INFLATION_PRESSURE_RANGE]
PRESMIN = 0.08
PRESMAX = 0.50
[VERTICAL_FORCE_RANGE]
FZMIN = 100.0
FZMAX = 10000.0
[LONG_SLIP_RANGE]
KPUMIN = -1.5
KPUMAX = 1.5
[SLIP_ANGLE_RANGE]
ALPMIN = -70.0
ALPMAX = 70.0
[INCLINATION_ANGLE_RANGE]
CAMMIN = -65.0
CAMMAX = 65.0
[SCALING_COEFFICIENTS]
LMUX = 0.98
LMUY = 1.02
LKYC = 1.10
LMP = 0.85
[LONGITUDINAL_COEFFICIENTS]
PCX1 = 1.70
PDX1 = 1.25
PKX1 = 24.0
PTX1 = 1.1666666667
PTX2 = 0.0
PTX3 = 0.0
[LATERAL_COEFFICIENT]
PCY1 = 1.35
PDY1 = 1.22
PKY1 = 28.0
PKY2 = 1.9
PKY4 = 2.0
PTY1 = 1.50
PTY2 = 1.0
[ALIGNING_COEFFICIENTS]
QBZ1 = 7.5
QDZ1 = 0.20
[TURNSLIP_COEFFICIENTS]
PDXP1 = 0.40
PKYP1 = 1.00
PDYP1 = 0.40
PECP1 = 0.50
QDTP1 = 10.0
QCRP1 = 0.20
QCRP2 = 0.10
QBRP1 = 0.10
QDRP1 = 1.00
)TIR";

    const auto loaded = heritage::vehicles::tires::parseTirePropertyFileText(
        tir, "regression-inline.tir");
    if (!loaded.success)
        return false;

    const std::string incompleteTir = R"TIR(
[MODEL]
FITTYP = 62
[DIMENSION]
UNLOADED_RADIUS = 0.30
[VERTICAL]
FNOMIN = 1800.0
[LONGITUDINAL_COEFFICIENT]
PCX1 = 1.7
PDX1 = 1.2
PKX1 = 24.0
[LATERAL_COEFFICIENT]
PCY1 = 1.3
PDY1 = 1.2
PKY1 = 28.0
PKY2 = 1.9
)TIR";
    const auto incomplete = heritage::vehicles::tires::parseTirePropertyFileText(
        incompleteTir, "regression-incomplete.tir");
    if (incomplete.success
        || incomplete.errorMessage.find("PKY4") == std::string::npos)
    {
        return false;
    }

    TireModelDescription fallback;
    fallback.provider = TireProviderKind::MagicFormula62;
    const TireModelDescription model =
        heritage::vehicles::tireModelDescriptionFromPropertyFile(
            loaded.data,
            TireProviderKind::MagicFormula62,
            "regression-inline.tir",
            "regression_synthetic",
            0.25,
            fallback);

    const VehicleScalar deg = kPi / 180.0;
    return loaded.data.fitType == 62
        && loaded.data.tireSide == "RIGHT"
        && std::abs(loaded.data.magicFormula.unloadedRadiusM - 0.300) < 1.0e-9
        && std::abs(loaded.data.widthM - 0.180) < 1.0e-9
        && std::abs(loaded.data.magicFormula.referenceSpeedMps - 16.6666666667) < 1.0e-6
        && std::abs(loaded.data.magicFormula.nominalPressurePa - 220000.0) < 1.0
        && std::abs(loaded.data.inflationPressurePa - 230000.0) < 1.0
        && std::abs(loaded.data.verticalStiffnessNPerM - 220000.0) < 1.0
        && std::abs(loaded.data.verticalDampingNsPerM - 1800.0) < 1.0
        && std::abs(loaded.data.magicFormula.maximumAbsCamberRadians - 65.0 * deg) < 1.0e-9
        && std::abs(loaded.data.magicFormula.pCx1 - 1.70) < 1.0e-12
        && std::abs(loaded.data.magicFormula.pDx1 - 1.25) < 1.0e-12
        && std::abs(loaded.data.magicFormula.lKygamma - 1.10) < 1.0e-12
        && std::abs(loaded.data.magicFormula.lMp - 0.85) < 1.0e-12
        && loaded.data.hasMotorcycleContour
        && std::abs(loaded.data.pTx1 - 1.1666666667) < 1.0e-9
        && std::abs(loaded.data.pTy1 - 1.50) < 1.0e-12
        && std::abs(loaded.data.pTy2 - 1.0) < 1.0e-12
        && std::abs(loaded.data.magicFormula.pDxP1 - 0.40) < 1.0e-12
        && std::abs(loaded.data.magicFormula.qCrP1 - 0.20) < 1.0e-12
        && loaded.data.hasEffectiveRollingRadiusModel
        && std::abs(loaded.data.bReff - 8.386) < 1.0e-12
        && std::abs(loaded.data.qV1 - 0.00076) < 1.0e-12
        && loaded.data.hasContactPatchLengthModel
        && std::abs(loaded.data.qRa1 - 0.67594) < 1.0e-12
        && std::abs(loaded.data.qRb2 + 1.19176) < 1.0e-12
        && loaded.data.hasRigidRingModel
        && std::abs(loaded.data.structuralLongitudinalStiffnessNPerM - 800000.0) < 1.0
        && std::abs(loaded.data.structuralLateralStiffnessNPerM - 650000.0) < 1.0
        && std::abs(loaded.data.structuralFrequencyLongHz - 65.0) < 1.0e-12
        && std::abs(loaded.data.structuralYawStiffnessNmPerRad - 4500.0) < 1.0e-6
        && std::abs(loaded.data.structuralFrequencyYawHz - 50.0) < 1.0e-12
        && std::abs(loaded.data.structuralFrequencyWindupHz - 75.0) < 1.0e-12
        && loaded.data.hasRoadEnvelopingModel
        && std::abs(loaded.data.ellipseLengthM - 0.115) < 1.0e-12
        && std::abs(loaded.data.ellipseMaximumStepM - 0.100) < 1.0e-12
        && loaded.data.ellipseWidthCount == 3
        && loaded.data.ellipseSideCount == 3
        && model.rigidRing.enabled
        && std::abs(model.rigidRing.yawFrequencyHz - 50.0) < 1.0e-12
        && std::abs(model.rigidRing.windupFrequencyHz - 75.0) < 1.0e-12
        && std::abs(model.rigidRing.beltPolarInertiaKgM2 - 0.33) < 1.0e-12
        && model.roadEnveloping.enabled
        && loaded.data.hasHeritageThermalModel
        && model.thermal.enabled
        && std::abs(model.thermal.optimumTreadTemperatureC - 70.0) < 1.0e-12
        && std::abs(model.thermal.referenceGaugePressurePa - 230000.0) < 1.0
        && loaded.data.hasHeritageTreadState
        && model.wear.enabled
        && std::abs(model.wear.initialTreadDepthM - 0.007) < 1.0e-12
        && std::abs(model.wear.wearDepthPerJoule - 1.0e-9) < 1.0e-18
        && loaded.data.hasHeritageContaminationModel
        && model.contamination.enabled
        && std::abs(model.contamination.grassOrganicPickupRateHz - 2.2) < 1.0e-12
        && std::abs(model.contamination.mudMaximumFrictionLoss - 0.34) < 1.0e-12
        && loaded.data.hasHeritageWetSurfaceModel
        && model.wetSurface.enabled
        && std::abs(model.wetSurface.wetnessOneWaterDepthM - 0.0030) < 1.0e-12
        && std::abs(model.wetSurface.hydrodynamicLiftCoefficient - 0.70) < 1.0e-12
        && loaded.data.hasHeritageWinterSurfaceModel
        && model.winterSurface.enabled
        && std::abs(model.winterSurface.winterCompoundEffectiveness - 0.90) < 1.0e-12
        && std::abs(model.winterSurface.sipingDensity - 0.80) < 1.0e-12
        && std::abs(model.winterSurface.snowTreadInterlock - 0.72) < 1.0e-12
        && model.winterSurface.studsEnabled
        && model.winterSurface.studCount == 120
        && std::abs(model.winterSurface.studProtrusionM - 0.0012) < 1.0e-12
        && loaded.data.hasHeritageShallowGranularModel
        && model.shallowGranularSurface.enabled
        && std::abs(model.shallowGranularSurface.treadAggressiveness - 0.65) < 1.0e-12
        && std::abs(model.shallowGranularSurface.openVoidRatio - 0.42) < 1.0e-12
        && std::abs(model.shallowGranularSurface.granularShearCoupling - 0.80) < 1.0e-12
        && loaded.data.hasHeritageDeformableTerrainModel
        && model.deformableTerrainSurface.enabled
        && std::abs(model.deformableTerrainSurface.treadAggressiveness - 0.72) < 1.0e-12
        && std::abs(model.deformableTerrainSurface.openVoidRatio - 0.52) < 1.0e-12
        && std::abs(model.deformableTerrainSurface.soilShearCoupling - 0.88) < 1.0e-12
        && std::abs(model.deformableTerrainSurface.flotationCoupling - 0.44) < 1.0e-12
        && std::abs(model.deformableTerrainSurface.maximumSinkageM - 0.42) < 1.0e-12
        && model.contactGeometry.useMagicFormulaEffectiveRadius
        && model.contactGeometry.useMagicFormulaContactLength
        && loaded.data.unsupportedAssignmentCount == 0
        && model.provider == TireProviderKind::MagicFormula62Motorcycle
        && model.importedPropertyFile
        && !model.magicFormulaUsesLegacySeed
        && model.importedFitType == 62
        && model.parameterProvenance == "regression_synthetic"
        && model.parameterTireSide == "RIGHT"
        && std::abs(model.parameterConfidence - 0.25) < 1.0e-12
        && heritage::vehicles::validTireModelDescription(model);
}

bool tireFamilyBaselinesAreCoherentAndBrandNeutral()
{
    using heritage::vehicles::tires::TireFamily;
    using heritage::vehicles::tires::TireFamilyBaselineInput;
    using heritage::vehicles::tires::TirePartDefinition;
    using heritage::vehicles::tires::buildTireFamilyBaseline;
    using heritage::vehicles::tires::validTirePartDefinition;

    TireFamilyBaselineInput roadInput;
    roadInput.sectionWidthM = 0.205;
    roadInput.aspectRatio = 0.45;
    roadInput.rimRadiusM = 0.2159;
    roadInput.nominalLoadN = 3500.0;
    roadInput.inflationPressurePa = 220000.0;

    const auto road = buildTireFamilyBaseline(TireFamily::RoadSummerPerformance, roadInput);
    const auto slick = buildTireFamilyBaseline(TireFamily::RacingSlick, roadInput);
    const auto wet = buildTireFamilyBaseline(TireFamily::RacingWet, roadInput);
    const auto winter = buildTireFamilyBaseline(TireFamily::Winter, roadInput);
    const auto studded = buildTireFamilyBaseline(TireFamily::StuddedIce, roadInput);
    const auto gravel = buildTireFamilyBaseline(TireFamily::RallyGravel, roadInput);
    const auto atv = buildTireFamilyBaseline(TireFamily::LowPressureOffRoad, roadInput);

    TireFamilyBaselineInput motorcycleInput = roadInput;
    motorcycleInput.sectionWidthM = 0.180;
    motorcycleInput.aspectRatio = 0.55;
    motorcycleInput.rimRadiusM = 0.2159;
    motorcycleInput.nominalLoadN = 1800.0;
    const auto motorcycle = buildTireFamilyBaseline(TireFamily::Motorcycle, motorcycleInput);

    TirePartDefinition a;
    a.id = "tire_a";
    a.displayName = "A";
    a.manufacturer = "Manufacturer One";
    a.model = "Model One";
    a.family = TireFamily::RacingWet;

    TirePartDefinition b = a;
    b.id = "tire_b";
    b.displayName = "B";
    b.manufacturer = "Completely Different Brand";
    b.model = "Different Product";

    const VehicleScalar expectedRoadRadius =
        roadInput.rimRadiusM + roadInput.sectionWidthM * roadInput.aspectRatio;

    return road.valid
        && slick.valid
        && wet.valid
        && winter.valid
        && studded.valid
        && gravel.valid
        && atv.valid
        && motorcycle.valid
        && validTirePartDefinition(a)
        && validTirePartDefinition(b)
        && std::abs(road.model.contactGeometry.unloadedRadiusM - expectedRoadRadius) < 1.0e-12
        && std::abs(road.model.contactGeometry.nominalWidthM - roadInput.sectionWidthM) < 1.0e-12
        && slick.model.wetSurface.treadVoidRatio < road.model.wetSurface.treadVoidRatio
        && wet.model.wetSurface.drainageEfficiency > road.model.wetSurface.drainageEfficiency
        && winter.model.winterSurface.winterCompoundEffectiveness > road.model.winterSurface.winterCompoundEffectiveness
        && studded.model.winterSurface.studsEnabled
        && studded.model.winterSurface.studCount > 0
        && gravel.model.shallowGranularSurface.treadAggressiveness > road.model.shallowGranularSurface.treadAggressiveness
        && atv.model.deformableTerrainSurface.flotationCoupling > road.model.deformableTerrainSurface.flotationCoupling
        && motorcycle.model.provider == TireProviderKind::MagicFormula62Motorcycle
        && motorcycle.model.motorcycleProfile.tireWidthM == motorcycleInput.sectionWidthM
        && road.model.parameterProvenance == "heritage_estimated_family_baseline"
        && road.model.parameterConfidence < 0.5;
}

bool tirePerformanceBiasesMapToMechanismsWithoutForceMultipliers()
{
    using heritage::vehicles::tires::TireFamily;
    using heritage::vehicles::tires::TireFamilyBaselineInput;
    using heritage::vehicles::tires::TirePerformanceBias;
    using heritage::vehicles::tires::applyTirePerformanceBias;
    using heritage::vehicles::tires::buildTireFamilyBaseline;

    TireFamilyBaselineInput input;
    const auto neutral = buildTireFamilyBaseline(TireFamily::RoadSummerPerformance, input);
    if (!neutral.valid)
    {
        return false;
    }

    TirePerformanceBias dryBias;
    dryBias.dry = 1.0;
    const auto dry = buildTireFamilyBaseline(TireFamily::RoadSummerPerformance, input, dryBias);

    TirePerformanceBias wetBias;
    wetBias.wet = 1.0;
    const auto wet = buildTireFamilyBaseline(TireFamily::RoadSummerPerformance, input, wetBias);

    TirePerformanceBias winterBias;
    winterBias.snowIce = 1.0;
    const auto winter = buildTireFamilyBaseline(TireFamily::RoadSummerPerformance, input, winterBias);

    TirePerformanceBias mudBias;
    mudBias.mud = 1.0;
    const auto mud = buildTireFamilyBaseline(TireFamily::RoadSummerPerformance, input, mudBias);

    TirePerformanceBias sandBias;
    sandBias.sand = 1.0;
    const auto sand = buildTireFamilyBaseline(TireFamily::RoadSummerPerformance, input, sandBias);

    TirePerformanceBias gravelBias;
    gravelBias.gravel = 1.0;
    const auto gravel = buildTireFamilyBaseline(TireFamily::RoadSummerPerformance, input, gravelBias);

    TirePerformanceBias enduranceBias;
    enduranceBias.wearEndurance = 1.0;
    const auto endurance = buildTireFamilyBaseline(TireFamily::RoadSummerPerformance, input, enduranceBias);

    TirePerformanceBias noStudMagic;
    noStudMagic.snowIce = 1.0;
    const auto summerWinterTuned = buildTireFamilyBaseline(
        TireFamily::RoadSummerPerformance, input, noStudMagic);

    TireModelDescription imported = neutral.model;
    imported.importedPropertyFile = true;
    imported.parameterSource = "synthetic measured tire";
    imported.parameterProvenance = "measured_regression";
    const auto preserved = applyTirePerformanceBias(imported, dryBias);

    TirePerformanceBias invalidBias;
    invalidBias.dry = 1.01;
    const auto invalid = applyTirePerformanceBias(neutral.model, invalidBias);

    return dry.valid
        && wet.valid
        && winter.valid
        && mud.valid
        && sand.valid
        && gravel.valid
        && endurance.valid
        && summerWinterTuned.valid
        && dry.model.peakFriction > neutral.model.peakFriction
        && dry.model.longitudinalStiffness > neutral.model.longitudinalStiffness
        && wet.model.wetSurface.treadVoidRatio > neutral.model.wetSurface.treadVoidRatio
        && wet.model.wetSurface.drainageEfficiency > neutral.model.wetSurface.drainageEfficiency
        && wet.model.wetSurface.thinFilmMaximumFrictionLoss < neutral.model.wetSurface.thinFilmMaximumFrictionLoss
        && winter.model.winterSurface.winterCompoundEffectiveness > neutral.model.winterSurface.winterCompoundEffectiveness
        && winter.model.winterSurface.sipingDensity > neutral.model.winterSurface.sipingDensity
        && !summerWinterTuned.model.winterSurface.studsEnabled
        && mud.model.deformableTerrainSurface.treadAggressiveness > neutral.model.deformableTerrainSurface.treadAggressiveness
        && mud.model.contamination.mudRetention < neutral.model.contamination.mudRetention
        && sand.model.deformableTerrainSurface.flotationCoupling > neutral.model.deformableTerrainSurface.flotationCoupling
        && sand.model.contactGeometry.verticalStiffnessNPerM < neutral.model.contactGeometry.verticalStiffnessNPerM
        && gravel.model.shallowGranularSurface.treadEdgeDensity > neutral.model.shallowGranularSurface.treadEdgeDensity
        && gravel.model.shallowGranularSurface.granularShearCoupling > neutral.model.shallowGranularSurface.granularShearCoupling
        && endurance.model.wear.wearDepthPerJoule < neutral.model.wear.wearDepthPerJoule
        && endurance.model.wear.rubberSheddingPropensity < neutral.model.wear.rubberSheddingPropensity
        && endurance.model.thermal.hotTemperatureSpanC > neutral.model.thermal.hotTemperatureSpanC
        && endurance.model.peakFriction < neutral.model.peakFriction
        && dry.model.parameterProvenance == "heritage_estimated_family_baseline+bias_v1"
        && preserved.valid
        && !preserved.applied
        && preserved.authoritativeDataPreserved
        && preserved.model.peakFriction == imported.peakFriction
        && preserved.model.parameterProvenance == "measured_regression"
        && !invalid.valid;
}


bool tirePartsResolveAndAssignReusableFitments()
{
    using heritage::vehicles::tires::TireFamily;
    using heritage::vehicles::tires::TirePartAssignmentInfo;
    using heritage::vehicles::tires::TirePartDefinition;
    using heritage::vehicles::tires::TirePartFitment;
    using heritage::vehicles::tires::TirePartResolutionSource;
    using heritage::vehicles::tires::resolveTirePart;

    TirePartDefinition part;
    part.id = "road_225_45_r17";
    part.displayName = "Reusable 225/45 R17 road tire";
    part.manufacturer = "Metadata Only";
    part.model = "Neutral Runtime Regression";
    part.family = TireFamily::RoadSummerPerformance;
    part.engineering.sectionWidthM = 0.225;
    part.engineering.aspectRatio = 0.45;
    part.engineering.rimRadiusM = 0.2159;
    part.engineering.nominalLoadN = 4100.0;
    part.engineering.referenceInflationPressurePa = 230000.0;
    part.performanceBias.wet = 0.30;
    part.performanceBias.wearEndurance = 0.20;

    TirePartFitment frontFitment;
    frontFitment.coldInflationPressurePa = 245000.0;
    TirePartFitment rearFitment;
    rearFitment.coldInflationPressurePa = 215000.0;

    const auto frontResolved = resolveTirePart(part, {}, frontFitment);
    const auto rearResolved = resolveTirePart(part, {}, rearFitment);
    if (!frontResolved.valid || !rearResolved.valid)
        return false;

    const VehicleScalar expectedRadius =
        part.engineering.rimRadiusM
        + part.engineering.sectionWidthM * part.engineering.aspectRatio;

    TirePartDefinition missingProperty = part;
    missingProperty.id = "missing_property";
    missingProperty.displayName = "Missing property file";
    missingProperty.propertyFile = "does/not/exist.tir";
    const auto missingResolved = resolveTirePart(missingProperty);

    TirePartFitment invalidFitment;
    invalidFitment.coldInflationPressurePa = 1000.0;
    const auto invalidResolved = resolveTirePart(part, {}, invalidFitment);

    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
        return false;

    if (!world.vehicles.assignWheelTirePart(world.vehicle, 0, part, {}, frontFitment)
        || !world.vehicles.assignWheelTirePart(world.vehicle, 1, part, {}, rearFitment))
    {
        return false;
    }

    TireModelDescription frontModel;
    TireModelDescription rearModel;
    TirePartAssignmentInfo frontAssignment;
    TirePartAssignmentInfo rearAssignment;
    if (!world.vehicles.wheelTireModel(world.vehicle, 0, frontModel)
        || !world.vehicles.wheelTireModel(world.vehicle, 1, rearModel)
        || !world.vehicles.wheelTirePartAssignment(world.vehicle, 0, frontAssignment)
        || !world.vehicles.wheelTirePartAssignment(world.vehicle, 1, rearAssignment))
    {
        return false;
    }

    VehicleScalar minimumPressurePa = 0.0;
    VehicleScalar maximumPressurePa = 0.0;
    VehicleScalar representativePressurePa = 0.0;
    const bool pressureRangeAvailable =
        world.vehicles.tireColdInflationPressureRange(
            world.vehicle, minimumPressurePa, maximumPressurePa, representativePressurePa);
    const VehicleScalar testPressurePa = std::clamp(
        VehicleScalar{180000.0}, minimumPressurePa, maximumPressurePa);
    TireModelDescription pressureAdjustedModel;
    TirePartAssignmentInfo pressureAdjustedAssignment;
    const bool pressureAdjustmentWorks = pressureRangeAvailable
        && world.vehicles.setWheelTireColdInflationPressure(
            world.vehicle, 0, testPressurePa)
        && world.vehicles.wheelTireModel(
            world.vehicle, 0, pressureAdjustedModel)
        && world.vehicles.wheelTirePartAssignment(
            world.vehicle, 0, pressureAdjustedAssignment)
        && std::abs(pressureAdjustedModel.inflationPressurePa - testPressurePa) < 1.0e-12
        && std::abs(pressureAdjustedModel.referenceInflationPressurePa
            - part.engineering.referenceInflationPressurePa) < 1.0e-12
        && std::abs(pressureAdjustedModel.thermal.referenceGaugePressurePa - testPressurePa) < 1.0e-12
        && std::abs(pressureAdjustedAssignment.coldInflationPressurePa - testPressurePa) < 1.0e-12;

    TirePartAssignmentInfo clearedAssignment;
    const bool manualOverrideClearsAssignment =
        world.vehicles.setWheelTireProvider(world.vehicle, 0, pressureAdjustedModel.provider)
        && world.vehicles.wheelTirePartAssignment(world.vehicle, 0, clearedAssignment)
        && !clearedAssignment.assigned;

    return frontResolved.source == TirePartResolutionSource::EstimatedFamilyBaseline
        && rearResolved.source == TirePartResolutionSource::EstimatedFamilyBaseline
        && frontResolved.fitmentPressureApplied
        && rearResolved.fitmentPressureApplied
        && std::abs(frontResolved.model.contactGeometry.nominalWidthM - 0.225) < 1.0e-12
        && std::abs(frontResolved.model.contactGeometry.unloadedRadiusM - expectedRadius) < 1.0e-12
        && std::abs(frontResolved.model.inflationPressurePa - 245000.0) < 1.0e-12
        && std::abs(rearResolved.model.inflationPressurePa - 215000.0) < 1.0e-12
        && std::abs(frontResolved.model.referenceInflationPressurePa - 230000.0) < 1.0e-12
        && std::abs(rearResolved.model.referenceInflationPressurePa - 230000.0) < 1.0e-12
        && std::abs(part.engineering.referenceInflationPressurePa - 230000.0) < 1.0e-12
        && frontResolved.model.parameterProvenance == "heritage_estimated_tire_part"
        && !missingResolved.valid
        && !invalidResolved.valid
        && frontAssignment.assigned
        && rearAssignment.assigned
        && frontAssignment.partId == part.id
        && rearAssignment.partId == part.id
        && frontAssignment.source == TirePartResolutionSource::EstimatedFamilyBaseline
        && rearAssignment.source == TirePartResolutionSource::EstimatedFamilyBaseline
        && std::abs(frontAssignment.coldInflationPressurePa - 245000.0) < 1.0e-12
        && std::abs(rearAssignment.coldInflationPressurePa - 215000.0) < 1.0e-12
        && std::abs(frontModel.contactGeometry.nominalWidthM - 0.225) < 1.0e-12
        && std::abs(rearModel.contactGeometry.nominalWidthM - 0.225) < 1.0e-12
        && std::abs(frontModel.inflationPressurePa - 245000.0) < 1.0e-12
        && std::abs(rearModel.inflationPressurePa - 215000.0) < 1.0e-12
        && std::abs(frontModel.referenceInflationPressurePa - 230000.0) < 1.0e-12
        && std::abs(rearModel.referenceInflationPressurePa - 230000.0) < 1.0e-12
        && pressureRangeAvailable
        && minimumPressurePa <= 80000.0 + 1.0e-9
        && maximumPressurePa >= 500000.0 - 1.0e-9
        && representativePressurePa > minimumPressurePa
        && representativePressurePa < maximumPressurePa
        && pressureAdjustmentWorks
        && manualOverrideClearsAssignment;
}

} // namespace heritage::tests
