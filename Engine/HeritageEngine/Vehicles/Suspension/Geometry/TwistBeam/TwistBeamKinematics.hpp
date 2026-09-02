#pragma once
#include "../SemiTrailingArm/SemiTrailingArmKinematics.hpp"

namespace heritage::vehicles {

// SUSP13: two semi-trailing arms joined by one torsionally compliant crossbeam.
// Wheel paths stay arm-owned; the beam contributes relative-arm torsion rather
// than pretending to be a generic anti-roll bar detached from the mechanism.
struct TwistBeamHardpoints
{
    bool authored=false;
    SemiTrailingArmHardpoints leftArm{};
    SemiTrailingArmHardpoints rightArm{};
    heritage::math::Vec3 beamLeftAttachment{};
    heritage::math::Vec3 beamRightAttachment{};
};
struct TwistBeamKinematicsInput
{
    float leftCompressionM=0.0f,rightCompressionM=0.0f;
    float leftCompressionVelocityMps=0.0f,rightCompressionVelocityMps=0.0f;
    bool leftSide=true;
    heritage::math::Vec3 suspensionDirection{0,-1,0};
    float staticCamberDegrees=0.0f,staticToeDegrees=0.0f;
};
struct TwistBeamKinematicsOutput
{
    bool valid=false,travelClamped=false;
    SemiTrailingArmKinematicsOutput arm{};
    float beamTwistRadians=0.0f;
    float beamTwistRateRadiansPerSecond=0.0f;
    float beamAngularMotionRatioRadPerM=0.0f; // signed selected-wheel d(delta theta)/dx
    float beamSpanM=0.0f;
};
bool validTwistBeamHardpoints(const TwistBeamHardpoints& hardpoints);
TwistBeamKinematicsOutput evaluateTwistBeamKinematics(const TwistBeamHardpoints&,const TwistBeamKinematicsInput&);

} // namespace heritage::vehicles
