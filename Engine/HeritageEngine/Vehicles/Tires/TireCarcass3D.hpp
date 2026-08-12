#pragma once

#include "../VehiclePrecision.hpp"
#include "../../Physics/CollisionSystem.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace heritage::vehicles::tires {

// TIRE20: unified lower-hemisphere 3D tire/carcass contact.
//
// Heritage deliberately spends physical contact resolution where a vehicle tire
// can realistically carry vehicle load: the complete lower half of the tire,
// including the forward/rearward equators, tread, both shoulders and both
// sidewalls.  The upper half is not part of the expensive tire-contact domain.
//
// A 9 x 7 reduced-order structural lattice spans front-equator -> bottom ->
// rear-equator and the full tire width.  Collision itself remains continuous: a
// thin kerb/rock edge may contact between lattice stations, after which the
// contact is mapped into this structural basis for deformation/presentation.
// The same physical manifold supplies local traction and visual deformation.
inline constexpr std::size_t TireCarcassCircumferentialStations = 9;
// Backward-compatible name used by existing diagnostics/tests.  It now means
// lower-half stations, not a full 360-degree ring.
inline constexpr std::size_t TireCarcassCircumferentialSectors =
    TireCarcassCircumferentialStations;
inline constexpr std::size_t TireCarcassCrossSectionBands = 7;
inline constexpr std::size_t TireCarcassStructuralNodeCount =
    TireCarcassCircumferentialStations * TireCarcassCrossSectionBands;
inline constexpr std::size_t TireCarcassMaximumContacts = 12;
inline constexpr std::size_t TireCarcassMaximumCandidateTriangles = 64;
inline constexpr std::size_t TireCarcassMaximumCandidatePrimitives = 12;
// Static candidates intentionally match the 64-triangle BVH neighborhood.
// Do not truncate that already-nearest-first cache to a smaller prefix: a
// densely tessellated flat road can otherwise consume every slot and starve a
// vertical kerb/wall only millimetres farther from the wheel centre. The 3D
// narrow phase is gated off for ordinary flat-road tires, so preserving all 64
// candidates costs CPU only when complex lower-shell geometry is actually near.

// Cross-section band classification.  Magic Formula is meaningful for tread
// and shoulder contact patches; sidewall collision still has pneumatic/
// carcass normal compliance and surface friction, but is not misrepresented as
// a normal road-running MF contact patch.
enum class TireCarcassRegion : std::uint8_t
{
    Tread = 0,
    Shoulder = 1,
    Sidewall = 2
};

struct TireCarcassContact3DDescription
{
    bool enabled = true;
    VehicleScalar unloadedRadiusM = 0.30;
    VehicleScalar rimRadiusM = 0.18;
    VehicleScalar widthM = 0.20;

    // Whole-tire radial stiffness/damping datum.  The provider distributes it
    // across clustered local contacts rather than assigning full tire
    // stiffness to every creator triangle.
    VehicleScalar radialStiffnessNPerM = 220000.0;
    VehicleScalar radialDampingNSecondsPerM = 1800.0;
    VehicleScalar maximumCompressionM = 0.080;
    VehicleScalar maximumNormalForceN = 250000.0;

    // TIRE21 penetration stabilization. The lower tire shell is an
    // authoritative physical boundary, not merely a soft graphics probe.
    // Local carcass spring/damper compliance handles realistic deflection;
    // this bounded velocity-level term prevents a wheel centre from being
    // driven deeply through a kerb/wall before the compliant force catches up.
    VehicleScalar contactEffectiveMassKg = 35.0;
    VehicleScalar penetrationCorrectionFraction = 0.18;
    VehicleScalar maximumCorrectionSpeedMps = 4.0;

    // Small metric skin/probe radius gives robust edge/corner contact without
    // turning the tire into a coarse sphere/cylinder.
    VehicleScalar surfaceSkinM = 0.0010;
    VehicleScalar nodeProbeRadiusM = 0.0045;

    // Similar candidate contacts are collapsed into one physical manifold
    // point.  This is essential on tessellated kerbs: triangle count must not
    // multiply tire stiffness.
    VehicleScalar clusterDistanceM = 0.050;
    VehicleScalar clusterNormalCosine = 0.90;

    // Normal contacts substantially aligned with the already-authoritative
    // lower road patch are skipped to avoid double-solving the same flat-road
    // load.  Distinct lower-half lateral, longitudinal and diagonal contacts remain.
    VehicleScalar supportDuplicateNormalCosine = 0.88;
};

struct TireCarcassContact3DInput
{
    heritage::math::Vec3 wheelCenterWorld{};
    heritage::math::Vec3 wheelForwardWorld{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 wheelRightWorld{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 wheelUpWorld{ 0.0f, 1.0f, 0.0f };

    // Approximate world velocity of the wheel centre, plus spin around the
    // authored axle.  This is sufficient for normal damping and for deriving
    // local tread slip at arbitrary carcass contacts.
    heritage::math::Vec3 wheelCenterVelocityWorld{};
    VehicleScalar wheelAngularVelocityRadPerS = 0.0;
    VehicleScalar deltaTimeSeconds = 0.001;

    VehicleScalar inflationPressurePa = 220000.0;
    VehicleScalar referencePressurePa = 220000.0;

    bool supportContactValid = false;
    heritage::math::Vec3 supportNormalWorld{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 supportPointWorld{};
    // TIRE20: the ordinary road support is explicitly represented as the
    // primary lower carcass contact so the same physical contact point drives
    // deformation and the established road/MF traction path.  Its normal load
    // is already solved by suspension/unsprung-mass support and must therefore
    // not be applied a second time by the secondary-contact impulse path.
    VehicleScalar supportCompressionM = 0.0;
    VehicleScalar supportNormalForceN = 0.0;
    heritage::physics::SurfaceMaterial supportSurfaceMaterial =
        heritage::physics::SurfaceMaterial::Default;
    VehicleScalar supportWetness = 0.0;

    const heritage::physics::StaticSceneTriangle* candidateTriangles = nullptr;
    std::size_t candidateTriangleCount = 0;

    // Dynamic/static primitive colliders share the same lower-hemisphere tire
    // envelope.  Front, rear, side, shoulder and diagonal obstacle contacts are
    // therefore physical even when no downward support ray can see them.
    const heritage::physics::NearbyColliderSurface* candidatePrimitives = nullptr;
    std::size_t candidatePrimitiveCount = 0;
};

struct TireCarcassContact3D
{
    bool valid = false;
    heritage::math::Vec3 pointWorld{};
    heritage::math::Vec3 tireSurfacePointWorld{};
    heritage::math::Vec3 normalWorld{ 0.0f, 1.0f, 0.0f }; // surface -> tire centre

    VehicleScalar penetrationM = 0.0;
    VehicleScalar compressionM = 0.0;
    VehicleScalar normalVelocityMps = 0.0; // positive = separating
    VehicleScalar normalForceN = 0.0;
    // Primary support is the exact lower road contact already owned by the
    // suspension/unsprung support solve.  It participates in deformation and
    // traction coordinates but its normal impulse is not re-applied here.
    bool primarySupport = false;

    // Local tangent basis at the actual contact, not at the bottom of the
    // wheel.  localRollingTangent follows the tire circumference; localLateral
    // tangent follows the axle after projection into the contact plane.
    heritage::math::Vec3 localRollingTangentWorld{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localLateralTangentWorld{ 1.0f, 0.0f, 0.0f };
    VehicleScalar rollingSurfaceSpeedMps = 0.0;
    VehicleScalar lateralSurfaceSpeedMps = 0.0;

    TireCarcassRegion region = TireCarcassRegion::Tread;
    std::uint16_t structuralSector = 0;
    std::uint8_t structuralBand = 3;

    // TIRE20: continuous reduced-order shell coordinates.  Contact detection is
    // not quantized to the 9 x 7 lattice; the lattice is the structural
    // state basis only.  axialNormalized is -1..+1 across tire width, while
    // radialUp/radialForward locate the contact continuously around the
    // circumference in the authoritative wheel frame.  normalWheelLocal stores
    // the obstacle reaction normal in (right, up, forward) coordinates.  These
    // coordinates let the renderer reconstruct the same physical contact on the
    // current render pose without depending on a one-world-step-stale absolute
    // wheel-centre position.
    VehicleScalar structuralAxialNormalized = 0.0;
    VehicleScalar structuralRadialUp = -1.0;
    VehicleScalar structuralRadialForward = 0.0;
    heritage::math::Vec3 normalWheelLocal{ 0.0f, 1.0f, 0.0f };

    heritage::physics::SurfaceMaterial surfaceMaterial =
        heritage::physics::SurfaceMaterial::Default;
    VehicleScalar surfaceWetness = 0.0;

    // Invalid for immutable creator/world triangles.  Primitive contacts retain
    // their source so the caller can apply the equal-and-opposite impulse to a
    // dynamic object contacting the lower tire (e.g. a rock or obstacle).
    heritage::physics::ColliderHandle sourceCollider = heritage::physics::InvalidCollider;
    heritage::physics::BodyHandle sourceBody = heritage::physics::InvalidBody;
    heritage::physics::BodyMotionType sourceMotionType = heritage::physics::BodyMotionType::Static;
};

struct TireCarcassContact3DOutput
{
    bool valid = false;
    std::size_t rawCandidateCount = 0;
    std::size_t contactCount = 0;
    VehicleScalar totalNormalForceN = 0.0;
    VehicleScalar maximumCompressionM = 0.0;
    std::array<TireCarcassContact3D, TireCarcassMaximumContacts> contacts{};
};

TireCarcassContact3DOutput evaluateTireCarcassContact3D(
    const TireCarcassContact3DDescription& description,
    const TireCarcassContact3DInput& input);

// Applies a solved tire contact force to the wheel/chassis carrier and, when
// the source is a dynamic primitive collider, the equal-and-opposite impulse
// to that body.  Keeping this pair operation beside the carcass contract makes
// momentum conservation explicit and regression-testable.
void applyTireCarcassContactImpulsePair(
    heritage::physics::RigidBodySystem& bodies,
    heritage::physics::BodyHandle tireCarrierBody,
    const TireCarcassContact3D& contact,
    const heritage::math::Vec3& forceWorld,
    VehicleScalar deltaTimeSeconds);

} // namespace heritage::vehicles::tires
