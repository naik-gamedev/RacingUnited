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

} // namespace heritage::vehicles::tires
