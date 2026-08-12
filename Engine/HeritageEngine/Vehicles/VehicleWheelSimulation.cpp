#include "VehicleSystem.hpp"
#include "VehicleSystemInternal.hpp"
#include "Tires/TireSlipDynamics.hpp"
#include "Tires/TireContactPatch.hpp"
#include "../Physics/Surfaces/SurfaceWorld.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;

void VehicleSystem::simulateWheelSubstep(
    Record& vehicle,
    std::size_t wheelIndex,
    const SteeringSubstepState& steering,
    const DrivelineSubstepState& driveline,
    const heritage::physics::RigidBodyPose& chassisPose,
    const heritage::math::Vec3& chassisCenterOfMassLocal,
    heritage::math::Vec3& chassisLinearVelocity,
    heritage::math::Vec3& chassisAngularVelocityDegrees,
    heritage::physics::RigidBodySystem& bodies,
    const heritage::physics::CollisionSystem& collisions,
    heritage::physics::SurfaceWorld& surfaces,
    float substepDeltaTime,
    VehicleScalar antiRollBarForceN,
    VehicleScalar chassisSectionTwistRadiansValue)
{
    // CLEAN03B: the authoritative 1 kHz wheel path is physically partitioned into
    // ordered function-scope phases. These fragments deliberately share this lexical
    // scope so this cleanup cannot perturb evaluation order or FP behavior.
    // Establish per-wheel references, previous-state snapshot, support ray, and zeroed telemetry state.
#include "Simulation/WheelSubstep/00_PrepareWheelAndSupportQuery.inl"
    // Define telemetry writers, airborne state advancement, missing-support classification, and contact transition policy.
#include "Simulation/WheelSubstep/01_TelemetryAndAirbornePolicy.inl"
    // Resolve per-wheel steering/upright geometry, driver-aid modulation, actuation torques, and free-wheel fallback.
#include "Simulation/WheelSubstep/02_SteeringBrakingAndFreeWheel.inl"
    // Run adaptive 2D road enveloping and cache footprint material/wetness/provider blends.
#include "Simulation/WheelSubstep/03_RoadEnvelopeAndFootprintSampling.inl"
    // Advance rigid-ring structure and preview granular/deformable support sinkage before suspension resolution.
#include "Simulation/WheelSubstep/04_TireStructureAndTerrainSupport.inl"
    // Solve massless or unsprung-mass suspension/contact support, apply normal/link forces, and finalize grounded state.
#include "Simulation/WheelSubstep/05_SuspensionAndContactResolution.inl"
    // Refresh upright/contact kinematics, derive wheel basis and local velocities, and solve physical contact geometry/slip inputs.
#include "Simulation/WheelSubstep/06_ContactKinematicsAndPatchGeometry.inl"
    // Build surface/provider state, thermal/wet/winter/granular/deformable inputs, transient slip, and turn-slip contact-patch state.
#include "Simulation/WheelSubstep/07_SurfaceProvidersAndContactPatch.inl"
    // Evaluate wear/contamination and tire forces, then add bounded granular/deformable/fluid reactions and publish force telemetry.
#include "Simulation/WheelSubstep/08_TireForcesAndSurfaceReactions.inl"
    // Advance thermal, wear, contamination, wet/winter retained state, and persistent deformable SurfaceField state.
#include "Simulation/WheelSubstep/09_TirePhysicalStateUpdate.inl"
    // Apply tire impulse to the chassis and integrate wheel torque, brake constraint, angular velocity, and visual rotation.
#include "Simulation/WheelSubstep/10_ApplyForcesAndIntegrateWheel.inl"
}


} // namespace heritage::vehicles
