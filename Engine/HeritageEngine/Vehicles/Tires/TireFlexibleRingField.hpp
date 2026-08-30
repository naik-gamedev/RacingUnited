#pragma once

#include "../VehiclePrecision.hpp"

#include <array>
#include <cstddef>

namespace heritage::vehicles::tires {

// A bounded, real-time flexible-ring representation.  Collision sampling is an
// input to this solver; it is never a second render-time deformation authority.
// The solver returns one final displacement field in the wheel's
// forward/down/right basis.
inline constexpr std::size_t TireFlexibleRingContactStations = 21;
inline constexpr std::size_t TireFlexibleRingContactBands = 13;
inline constexpr std::size_t TireFlexibleRingContactCount =
    TireFlexibleRingContactStations * TireFlexibleRingContactBands;

// The complete belt is represented, not merely a separately flattened bottom.
// Twenty-four circumferential controls are a deterministic reduced-order model;
// they are not free rigid bodies or an unrestricted soft-body mesh.
inline constexpr std::size_t TireFlexibleRingFieldStations = 24;
inline constexpr std::size_t TireFlexibleRingFieldBands = 13;
inline constexpr std::size_t TireFlexibleRingFieldCount =
    TireFlexibleRingFieldStations * TireFlexibleRingFieldBands;

inline constexpr std::array<VehicleScalar, TireFlexibleRingContactStations>
    TireFlexibleRingContactPhiRadians{
        0.0,
        0.392699082, 0.698131701, 0.959931089, 1.134464014,
        1.265363708, 1.361356817, 1.439896633, 1.500983157,
        1.544616389, 1.570796327, 1.596976265, 1.640609496,
        1.701696021, 1.780235837, 1.876228945, 2.007128640,
        2.181661565, 2.443460953, 2.748893572, 3.141592654
    };

inline constexpr std::array<VehicleScalar, TireFlexibleRingContactBands>
    TireFlexibleRingWidthCoordinates{
        -1.00, -0.82, -0.65, -0.49, -0.34, -0.18, 0.00,
         0.18,  0.34,  0.49,  0.65,  0.82, 1.00
    };

struct TireFlexibleRingFieldDescription
{
    VehicleScalar unloadedRadiusM = 0.30;
    VehicleScalar rimRadiusM = 0.2159;
    VehicleScalar sectionWidthM = 0.205;
    VehicleScalar maximumDeflectionM = 0.08;
    VehicleScalar referencePressurePa = 220000.0;
    // Asset convention: the imported tire mesh is the fully supported shape
    // at 150 PSI. The solved displacement field fades continuously to zero at
    // this endpoint and grows as the common physics pressure is reduced.
    VehicleScalar authoredShapePressurePa = 150.0 * 6894.757293168;
    VehicleScalar verticalStiffnessNPerM = 220000.0;

    // Reduced-order structural parameters.  These describe coupling of the
    // belt/carcass control field, not artistic displacement amplitudes.
    VehicleScalar circumferentialCoupling = 4.5;
    VehicleScalar lateralCoupling = 3.0;
    VehicleScalar foundationStiffness = 1.35;
    VehicleScalar contactConstraintStiffness = 52.0;
    VehicleScalar effectivePoissonRatio = 0.48;
};

struct TireFlexibleRingFieldInput
{
    bool grounded = false;
    VehicleScalar verticalDeflectionM = 0.0;
    VehicleScalar contactPatchLengthM = 0.0;
    VehicleScalar contactPatchWidthM = 0.0;
    VehicleScalar normalLoadN = 0.0;
    VehicleScalar inflationPressurePa = 220000.0;

    // The road-envelope radial mode belongs to wheel support physics.  It is
    // deliberately not a presentation input: translating the complete belt
    // relative to the rim double-counts contact, balloons the crown and can
    // collapse the lower sidewall.  Local radial shape comes from the single
    // contact field above and its pressure/carcass constraints.
    VehicleScalar ringLongitudinalOffsetM = 0.0;
    VehicleScalar ringLateralOffsetM = 0.0;
    VehicleScalar ringYawRadians = 0.0;
    VehicleScalar ringWindupRadians = 0.0;
    // Tread-road torsion accumulated by the low-speed/turn-slip contact patch.
    // Unlike rigid-ring yaw this mode is anchored at the footprint and fades
    // around the carcass toward the unloaded crown.
    VehicleScalar contactPatchTwistRadians = 0.0;
    VehicleScalar flatSpotDepthM = 0.0;
    VehicleScalar flatSpotSector = 0.0;
    VehicleScalar wheelRotationRadians = 0.0;

    // Direct contact demand sampled against the real collision scene.  The
    // compression is a non-negative magnitude; the three signed components
    // are the resolving displacement along the actual collider normal in the
    // wheel forward/down/right basis.  Keeping the normal matters: a road top
    // must compress radially, while only a genuine kerb face may press a
    // sidewall laterally inward.
    std::array<VehicleScalar, TireFlexibleRingContactCount>
        directContactCompressionM{};
    std::array<VehicleScalar, TireFlexibleRingContactCount>
        directContactForwardDisplacementM{};
    std::array<VehicleScalar, TireFlexibleRingContactCount>
        directContactDownDisplacementM{};
    std::array<VehicleScalar, TireFlexibleRingContactCount>
        directContactLateralDisplacementM{};
};


// TIRE44: physics-owned dynamic carcass state.  The renderer consumes this
// field but does not solve tire shape.  Road and rim contact enter as
// unilateral structural constraints inside advanceTireFlexibleRingDynamics().
inline constexpr std::size_t TireFlexibleRingMaximumRoadSamples = 25;

struct TireFlexibleRingRoadSample
{
    // queried=false means this slot is unused. queried=true/supported=false is
    // important for partial-support edges: the nearest road-envelope query
    // explicitly found no collider there.
    bool queried = false;
    bool supported = false;
    VehicleScalar queryForwardM = 0.0;
    VehicleScalar queryLateralM = 0.0;
    VehicleScalar pointForwardM = 0.0;
    VehicleScalar pointDownM = 0.0;
    VehicleScalar pointLateralM = 0.0;
    VehicleScalar normalForward = 0.0;
    VehicleScalar normalDown = -1.0;
    VehicleScalar normalLateral = 0.0;
};

// TIRE45 development-only solver override bank. Most controls retain the TIRE44
// production defaults. TIRE45E intentionally defaults the synthetic Poisson
// lateral-bulge heuristic to zero because flat radial support is not a lateral
// excitation source; the Megalab can still opt back into it explicitly.
// The large station/band banks are intentional: they let the in-game lab
// isolate whether an artefact comes from lower-carcass topology, tread/shoulder
// stiffness, contact response or the rigid-ring anchor mapping instead of
// forcing another guessed global "fix".
struct TireFlexibleRingDevelopmentTuning
{
    bool enabled = false;

    VehicleScalar effectiveMassScale = 1.0;
    VehicleScalar foundationScale = 1.0;
    VehicleScalar circumferentialScale = 1.0;
    VehicleScalar secondNeighborScale = 1.0;
    VehicleScalar lateralScale = 1.0;
    VehicleScalar contactScale = 1.0;
    VehicleScalar rimContactScale = 1.0;
    VehicleScalar dampingScale = 1.0;
    VehicleScalar velocityRetention = 0.86;
    VehicleScalar pressureExponent = 0.50;
    VehicleScalar pneumaticMinimumScale = 0.10;
    VehicleScalar pneumaticMaximumScale = 2.25;
    VehicleScalar thermalInfluence = 1.0;
    VehicleScalar longitudinalAnchorScale = 1.0;
    VehicleScalar lateralAnchorScale = 1.0;
    // TIRE45J: rigid-ring yaw and lower-footprint torsion are visible again,
    // but only through the physical cornering-authority gate in the dynamic
    // solver. This preserves real cornering deflection/torsion without bringing
    // back the old straight-line inward wedge or parking-state twist artefacts.
    VehicleScalar yawAnchorScale = 1.0;
    VehicleScalar windupAnchorScale = 1.0;
    VehicleScalar contactTwistAnchorScale = 1.0;
    // TIRE45E: pure vertical support must not manufacture visible lateral
    // tread/sidewall motion. Real lateral tire load and side-contact normals
    // still deform the carcass through the rigid ring and unilateral contacts.
    // Keep the old heuristic exposed in Megalab, but production defaults off.
    VehicleScalar poissonBulgeScale = 0.0;
    VehicleScalar flatSpotScale = 1.0;
    VehicleScalar flangeHeightScale = 1.0;
    VehicleScalar flangeClearanceScale = 1.0;
    VehicleScalar shoulderAllowanceScale = 1.0;
    VehicleScalar maximumMagnitudeScale = 1.0;
    VehicleScalar associationForwardScale = 1.0;
    VehicleScalar associationLateralScale = 1.0;
    VehicleScalar groundStationThreshold = -0.10;
    VehicleScalar contactSlopM = 0.0;
    VehicleScalar radialCompressionScale = 1.0;
    VehicleScalar lowerHemisphereAnchorScale = 1.0;
    VehicleScalar structuralRateScale = 1.0;
    int implicitIterations = 8;

    std::array<VehicleScalar, TireFlexibleRingFieldStations>
        stationFoundationScale{};
    std::array<VehicleScalar, TireFlexibleRingFieldStations>
        stationContactScale{};
    std::array<VehicleScalar, TireFlexibleRingFieldStations>
        stationDampingScale{};
    std::array<VehicleScalar, TireFlexibleRingFieldStations>
        stationAnchorScale{};
    std::array<VehicleScalar, TireFlexibleRingFieldStations>
        stationCircumferentialScale{};

    std::array<VehicleScalar, TireFlexibleRingFieldBands>
        bandFoundationScale{};
    std::array<VehicleScalar, TireFlexibleRingFieldBands>
        bandContactScale{};
    std::array<VehicleScalar, TireFlexibleRingFieldBands>
        bandDampingScale{};
    std::array<VehicleScalar, TireFlexibleRingFieldBands>
        bandAnchorScale{};
    std::array<VehicleScalar, TireFlexibleRingFieldBands>
        bandLateralScale{};

    TireFlexibleRingDevelopmentTuning()
    {
        stationFoundationScale.fill(1.0);
        stationContactScale.fill(1.0);
        stationDampingScale.fill(1.0);
        stationAnchorScale.fill(1.0);
        stationCircumferentialScale.fill(1.0);
        bandFoundationScale.fill(1.0);
        bandContactScale.fill(1.0);
        bandDampingScale.fill(1.0);
        bandAnchorScale.fill(1.0);
        bandLateralScale.fill(1.0);
    }
};

struct TireFlexibleRingDynamicState
{
    bool initialized = false;
    std::array<VehicleScalar, TireFlexibleRingFieldCount> forwardDisplacementM{};
    std::array<VehicleScalar, TireFlexibleRingFieldCount> downDisplacementM{};
    std::array<VehicleScalar, TireFlexibleRingFieldCount> lateralDisplacementM{};
    std::array<VehicleScalar, TireFlexibleRingFieldCount> forwardVelocityMps{};
    std::array<VehicleScalar, TireFlexibleRingFieldCount> downVelocityMps{};
    std::array<VehicleScalar, TireFlexibleRingFieldCount> lateralVelocityMps{};
};

struct TireFlexibleRingDynamicsInput
{
    VehicleScalar deltaTimeSeconds = 0.0;
    bool grounded = false;
    VehicleScalar inflationPressurePa = 220000.0;
    VehicleScalar thermalStiffnessScale = 1.0;
    VehicleScalar normalLoadN = 0.0;

    // TIRE45J: presentation-side lateral/torsional carcass modes are allowed
    // only when the tire is under genuine cornering authority. These are
    // one-substep-old force/moment/slip values from the physical MF/contact
    // solver, not render heuristics. Straight rolling therefore stays neutral,
    // while real Fy/Mz/slip can bend and twist the carcass again.
    VehicleScalar lateralForceN = 0.0;
    VehicleScalar aligningMomentNm = 0.0;
    VehicleScalar slipAngleRadians = 0.0;
    VehicleScalar forwardSpeedMps = 0.0;

    // The 24 circumferential controls are EULERIAN wheel-frame stations.
    // They describe spatial carcass shape relative to wheel forward/down/right,
    // not named rubber parcels. Therefore wheel spin must NOT convect this
    // state around the array. Material-fixed effects (for example flat spots)
    // are mapped into the spatial lattice explicitly with wheelRotationRadians.
    // Kept as input telemetry for future constitutive-rate work only.
    VehicleScalar wheelAngularVelocityRadPerS = 0.0;

    VehicleScalar ringLongitudinalOffsetM = 0.0;
    VehicleScalar ringLateralOffsetM = 0.0;
    VehicleScalar ringYawRadians = 0.0;
    VehicleScalar ringWindupRadians = 0.0;
    VehicleScalar contactPatchTwistRadians = 0.0;
    VehicleScalar flatSpotDepthM = 0.0;
    VehicleScalar flatSpotSector = 0.0;
    VehicleScalar wheelRotationRadians = 0.0;

    std::array<TireFlexibleRingRoadSample, TireFlexibleRingMaximumRoadSamples>
        roadSamples{};
    std::size_t roadSampleCount = 0;

    // Optional development override. Production callers leave this null.
    const TireFlexibleRingDevelopmentTuning* developmentTuning = nullptr;
};

struct TireFlexibleRingFieldOutput
{
    bool valid = false;
    std::array<VehicleScalar, TireFlexibleRingFieldCount> forwardDisplacementM{};
    std::array<VehicleScalar, TireFlexibleRingFieldCount> downDisplacementM{};
    std::array<VehicleScalar, TireFlexibleRingFieldCount> lateralDisplacementM{};
};

bool validTireFlexibleRingFieldDescription(
    const TireFlexibleRingFieldDescription& description);

TireFlexibleRingFieldOutput evaluateTireFlexibleRingField(
    const TireFlexibleRingFieldDescription& description,
    const TireFlexibleRingFieldInput& input);

// Advances the stateful physics-owned flexible carcass.  This is the runtime
// authority from TIRE44 onward; evaluateTireFlexibleRingField() remains as a
// deterministic legacy/regression utility only.
TireFlexibleRingFieldOutput advanceTireFlexibleRingDynamics(
    const TireFlexibleRingFieldDescription& description,
    const TireFlexibleRingDynamicsInput& input,
    TireFlexibleRingDynamicState& state);

void relaxTireFlexibleRingDynamics(
    const TireFlexibleRingFieldDescription& description,
    VehicleScalar deltaTimeSeconds,
    TireFlexibleRingDynamicState& state);

} // namespace heritage::vehicles::tires
