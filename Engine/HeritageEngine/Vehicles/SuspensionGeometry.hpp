#pragma once

#include "SuspensionModel.hpp"
#include "Suspension/Geometry/MacPherson/MacPhersonKinematics.hpp"
#include "Suspension/Geometry/DoubleWishbone/DoubleWishboneKinematics.hpp"
#include "Suspension/Geometry/PushrodDoubleWishbone/PushrodDoubleWishboneKinematics.hpp"
#include "Suspension/Geometry/TrailingArm/TrailingArmKinematics.hpp"
#include "Suspension/Geometry/LiveAxle/LiveAxleKinematics.hpp"
#include "Suspension/Springs/LeafSpring/LeafSpringLiveAxle.hpp"
#include "Suspension/Geometry/MotorcycleFork/MotorcycleForkKinematics.hpp"
#include "Suspension/Geometry/MotorcycleSwingarm/MotorcycleSwingarmKinematics.hpp"
#include "Suspension/Geometry/Kart/KartChassisKinematics.hpp"
#include "Suspension/Geometry/MultiLink/MultiLinkKinematics.hpp"
#include "Suspension/Geometry/SemiTrailingArm/SemiTrailingArmKinematics.hpp"
#include "Suspension/Geometry/TwistBeam/TwistBeamKinematics.hpp"

#include "../Core/Math/Math.hpp"

namespace heritage::vehicles {

// Healthy upright kinematics evaluated independently from spring/damper force.
// The current provider uses authored travel curves. Future hardpoint providers
// can replace those curves while retaining the same authoritative pose output.
struct SuspensionGeometryDescription
{
    SuspensionProviderKind provider = SuspensionProviderKind::LinearRaycastV1;
    // A point on the steering axis. WheelSystem supplies the authored/reference
    // mount for virtual/legacy providers; hardpoint providers replace it with
    // their actual moving linkage point.
    heritage::math::Vec3 localSteeringAxisPoint{};
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    float staticCamberDegrees = 0.0f;
    float camberGainDegreesPerM = 0.0f;
    float camberProgressionDegreesPerM2 = 0.0f;
    float staticToeDegrees = 0.0f;
    bool casterOverrideEnabled = false;
    float staticCasterDegrees = 0.0f;
    float toeGainDegreesPerM = 0.0f;
    float toeProgressionDegreesPerM2 = 0.0f;
    MacPhersonHardpoints macPherson;
    DoubleWishboneHardpoints doubleWishbone;
    PushrodDoubleWishboneHardpoints pushrodDoubleWishbone;
    TrailingArmHardpoints trailingArm;
    LiveAxleHardpoints liveAxle;
    LeafSpringLiveAxleHardpoints leafSpringLiveAxle;
    MotorcycleForkHardpoints motorcycleFork;
    MotorcycleSwingarmHardpoints motorcycleSwingarm;
    KartChassisHardpoints kartChassis;
    MultiLinkHardpoints multiLink;
    SemiTrailingArmHardpoints semiTrailingArm;
    TwistBeamHardpoints twistBeam;
};

struct SuspensionGeometryInput
{
    float compressionM = 0.0f;
    float steeringDegrees = 0.0f;
    heritage::math::Vec3 localSuspensionDirection{ 0.0f, -1.0f, 0.0f };
    bool pairedCompressionValid = false;
    float pairedCompressionM = 0.0f;
    bool pairedCompressionVelocityValid = false;
    float compressionVelocityMps = 0.0f;
    float pairedCompressionVelocityMps = 0.0f;
};

struct SuspensionGeometryOutput
{
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    bool steeringAxisPointValid = false;
    heritage::math::Vec3 localSteeringAxisPoint{};
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    // Intrinsic X-Y-Z Euler angles matching Entity.SetLocalRotation.
    heritage::math::Vec3 localUprightRotationDegrees{};
    // Hardpoint providers can move the wheel centre laterally/longitudinally
    // through travel and expose the instantaneous spring motion ratio.
    bool kinematicsValid = true;
    bool travelClamped = false;
    float bumpSteerDegrees = 0.0f;
    float casterDegrees = 0.0f;
    float kingpinInclinationDegrees = 0.0f;
    float strutCompressionM = 0.0f;
    float springCompressionM = 0.0f;
    float springMotionRatio = 1.0f;
    // Separate-damper and rotational-spring mechanisms expose their own
    // generalized coordinates. MacPherson keeps both ratios equal because the
    // spring/damper share the strut.
    float damperCompressionM = 0.0f;
    float damperMotionRatio = 1.0f;
    float springTwistRadians = 0.0f;
    float springAngularMotionRatioRadPerM = 0.0f;
    float referenceSpringAngularMotionRatioRadPerM = 0.0f;
    // SUSP10 motorcycle-specific diagnostics/force-coupling geometry.
    float motorcycleRakeDegreesFromVertical = 0.0f;
    float motorcycleSwingarmAngleRadians = 0.0f;
    float motorcycleRockerAngleRadians = 0.0f;
    float motorcycleChainDistanceMotionRatio = 0.0f;
    float wheelbaseDeltaM = 0.0f;
    // SUSP11 kart-specific steering/frame diagnostics. The provider has no
    // conventional spring/damper travel; steering jacking is an actual wheel-
    // centre displacement and the chassis torsion mode supplies frame compliance.
    float kartSteeringJackingM = 0.0f;
    float kartKingpinRadialOffsetM = 0.0f;
    float multiLinkSteeringRackDisplacementM = 0.0f;
    float twistBeamTwistRadians = 0.0f;
    float twistBeamTwistRateRadiansPerSecond = 0.0f;
    float twistBeamAngularMotionRatioRadPerM = 0.0f;
    heritage::math::Vec3 localWheelCenter{};
};

SuspensionGeometryOutput evaluateSuspensionGeometry(
    const SuspensionGeometryDescription& description,
    const SuspensionGeometryInput& input);

// SUSP05: hardpoint-derived wheel-centre motion is also contact-query authority.
// The high-rate ray solver already owns travel along the suspension axis, so
// linkage providers contribute only the component perpendicular to that axis.
// This makes real lateral/longitudinal wheel scrub and steering-axis offset move
// the tire contact query without double-counting suspension compression.
struct SuspensionSupportOffsetOutput
{
    bool valid = false;
    heritage::math::Vec3 localTransverseOffset{};
};

SuspensionSupportOffsetOutput evaluateSuspensionSupportOffset(
    const SuspensionGeometryDescription& description,
    const SuspensionGeometryInput& input);

} // namespace heritage::vehicles
