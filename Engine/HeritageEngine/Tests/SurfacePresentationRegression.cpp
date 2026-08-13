#include "PhysicsRegressionCommon.hpp"
#include "../Graphics/LodTransitionPolicy.hpp"
#include "../Graphics/PresentationPrecision.hpp"
#include "../Graphics/TireMarkChunking.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::tests {

bool surfacePresentationIsBoundedAndWorldAddressed()
{
    using heritage::math::DVec3;
    using heritage::math::Vec3;
    using heritage::physics::SurfaceMaterial;
    using heritage::physics::SurfacePresentationContact;
    using heritage::physics::SurfaceWorld;

    // TIRE16I/LOD01: Heritage's master LOD contract must never hard-pop at
    // either a representation boundary or a final visibility boundary. Tire
    // marks use the one-sided form so full near detail is preserved through
    // 200 m and morphs smoothly after it.
    const float lodBlendWidth = heritage::graphics::lod::lodBlendWidthMeters(200.0f);
    const auto lodStart = heritage::graphics::lod::crossfadeAfterBoundary(
        200.0f, 200.0f, lodBlendWidth);
    const auto lodMiddle = heritage::graphics::lod::crossfadeAfterBoundary(
        200.0f + lodBlendWidth * 0.5f, 200.0f, lodBlendWidth);
    const auto lodEnd = heritage::graphics::lod::crossfadeAfterBoundary(
        200.0f + lodBlendWidth, 200.0f, lodBlendWidth);
    const float visibilityNear = heritage::graphics::lod::visibilityWeight(400.0f, 500.0f);
    const float visibilityFade = heritage::graphics::lod::visibilityWeight(460.0f, 500.0f);
    const float visibilityGone = heritage::graphics::lod::visibilityWeight(500.0f, 500.0f);
    if (lodStart.nearWeight < 0.999f
        || lodMiddle.nearWeight <= 0.35f || lodMiddle.nearWeight >= 0.65f
        || lodEnd.farWeight < 0.999f
        || visibilityNear < 0.999f
        || visibilityFade <= 0.0f || visibilityFade >= 1.0f
        || visibilityGone != 0.0f)
    {
        return false;
    }

    // Surface presentation keeps persistent state precise, then converts the
    // FP64 difference to camera-relative FP32 exactly once for the renderer.
    const heritage::math::Vec3 relativeProbe =
        heritage::graphics::presentation::cameraRelativeFp32(
            { 100000.125, 20.5, -49999.75 },
            { 99999.875, 20.0, -50000.0 });
    if (std::abs(relativeProbe.x - 0.25f) > 1.0e-6f
        || std::abs(relativeProbe.y - 0.5f) > 1.0e-6f
        || std::abs(relativeProbe.z - 0.25f) > 1.0e-6f)
    {
        return false;
    }

    // TIRE16K: 100 m chunks are presentation-only. FP32 local coordinates
    // must reconstruct the same FP64 road position to far below visible tire-
    // mark precision, including immediately on opposite sides of a chunk seam.
    const DVec3 chunkProbeA{ 199.99997, 1234.56789, -300.00003 };
    const DVec3 chunkProbeB{ 200.00003, 1234.56791, -299.99997 };
    const auto chunkAddressA = heritage::graphics::tiremarks::chunkAddress(chunkProbeA);
    const auto chunkAddressB = heritage::graphics::tiremarks::chunkAddress(chunkProbeB);
    const DVec3 chunkOriginA = heritage::graphics::tiremarks::chunkOrigin(chunkAddressA);
    const DVec3 chunkOriginB = heritage::graphics::tiremarks::chunkOrigin(chunkAddressB);
    const DVec3 reconstructedA = heritage::graphics::tiremarks::reconstructGlobal(
        chunkOriginA, heritage::graphics::tiremarks::localFp32(chunkProbeA, chunkOriginA));
    const DVec3 reconstructedB = heritage::graphics::tiremarks::reconstructGlobal(
        chunkOriginB, heritage::graphics::tiremarks::localFp32(chunkProbeB, chunkOriginB));
    const auto reconstructionError = [](const DVec3& expected, const DVec3& actual) {
        return std::max({
            std::abs(expected.x - actual.x),
            std::abs(expected.y - actual.y),
            std::abs(expected.z - actual.z)
        });
    };
    if (reconstructionError(chunkProbeA, reconstructedA) > 1.0e-4
        || reconstructionError(chunkProbeB, reconstructedB) > 1.0e-4
        || chunkAddressA.x == chunkAddressB.x
        || chunkAddressA.z == chunkAddressB.z)
    {
        return false;
    }

    SurfaceWorld world;
    world.setGlobalOrigin({ 100000.0, 20.0, -50000.0 });

    const Vec3 firstLocal{ 10.125f, 0.5f, -3.875f };
    SurfacePresentationContact mud;
    mud.normal = { 0.0f, 1.0f, 0.0f };
    mud.forward = { 0.0f, 0.0f, 1.0f };
    mud.material = SurfaceMaterial::Mud;
    mud.deltaTimeSeconds = 0.001f;
    mud.forwardSpeedMps = 12.0f;
    mud.lateralSpeedMps = 1.5f;
    mud.longitudinalSlipSpeedMps = 3.0f;
    mud.normalLoadN = 3600.0f;
    mud.wetness = 0.30f;
    mud.tireWidthM = 0.205f;
    mud.rutDepthM = 0.045f;
    mud.rutDepthDeltaM = 0.0008f;
    mud.displacedVolumeDeltaM3 = 0.000006f;
    mud.looseDepthM = 0.09f;

    for (int sample = 0; sample < 300; ++sample)
        world.recordContactPresentation(firstLocal, mud);

    world.advancePresentation(1.0f / 60.0f);
    auto stats = world.presentation().stats();
    if (stats.activeTrackMarks != 1u
        || stats.contactSamples != 300u
        || stats.rutIntensity <= 0.0f
        || stats.audio.debris <= 0.0f)
    {
        return false;
    }

    // A floating-origin rebase must not create a second visual track for the
    // same absolute contact patch. Presentation keys are derived after the
    // same local->global boundary used by authoritative SurfaceField state.
    const DVec3 secondOrigin{ 104096.0, 20.0, -54096.0 };
    const Vec3 secondLocal{
        firstLocal.x - 4096.0f,
        firstLocal.y,
        firstLocal.z + 4096.0f
    };
    world.setGlobalOrigin(secondOrigin);
    world.recordContactPresentation(secondLocal, mud);
    if (world.presentation().stats().activeTrackMarks != 1u)
        return false;

    // Wet hard pavement does not create deformable ruts, but it does create
    // bounded spray/audio presentation from the same effective wetness state.
    SurfacePresentationContact wetRoad;
    wetRoad.normal = { 0.0f, 1.0f, 0.0f };
    wetRoad.forward = { 0.0f, 0.0f, 1.0f };
    wetRoad.material = SurfaceMaterial::Asphalt;
    wetRoad.deltaTimeSeconds = 0.001f;
    wetRoad.forwardSpeedMps = 28.0f;
    wetRoad.longitudinalSlipSpeedMps = 1.0f;
    wetRoad.normalLoadN = 3200.0f;
    wetRoad.wetness = 0.85f;
    wetRoad.tireWidthM = 0.205f;
    for (int sample = 0; sample < 2000; ++sample)
        world.recordContactPresentation({ secondLocal.x + 1.0f, secondLocal.y, secondLocal.z }, wetRoad);

    world.advancePresentation(1.0f / 60.0f);
    stats = world.presentation().stats();
    if (stats.activeTrackMarks != 1u
        || stats.audio.spray <= 0.0f
        || stats.emittedParticles == 0u
        || stats.activeParticles > heritage::physics::SurfacePresentation::kMaximumParticles)
    {
        return false;
    }

    for (int step = 0; step < 40; ++step)
        world.advancePresentation(0.1f);
    if (world.presentation().stats().activeParticles != 0u)
        return false;

    // TIRE19 failure presentation is keyed to the wheel stream and event
    // serial. Repeating a 1000 Hz contact must not replay the blowout burst,
    // while the initial blowout and later detached-tread stage are visible.
    world.presentation().clear();
    SurfacePresentationContact slowPuncture = wetRoad;
    slowPuncture.sourceStreamId = 0xF018u;
    slowPuncture.deltaTimeSeconds = 0.001f;
    slowPuncture.forwardSpeedMps = 0.0f;
    slowPuncture.wetness = 0.0f;
    slowPuncture.tireFailureStage = 1;
    slowPuncture.tireFailureEventSerial = 1;
    slowPuncture.tireFailureLeakMassFlowKgPerSecond = 0.00001f;
    world.recordContactPresentation(
        { secondLocal.x + 1.4f, secondLocal.y, secondLocal.z }, slowPuncture);
    const std::uint64_t slowPunctureParticles =
        world.presentation().stats().emittedParticles;
    if (slowPunctureParticles < 1u || slowPunctureParticles > 3u)
        return false;
    world.recordContactPresentation(
        { secondLocal.x + 1.4f, secondLocal.y, secondLocal.z }, slowPuncture);
    if (world.presentation().stats().emittedParticles != slowPunctureParticles)
        return false;

    world.presentation().clear();
    SurfacePresentationContact failedTire = wetRoad;
    failedTire.sourceStreamId = 0xF019u;
    failedTire.deltaTimeSeconds = 0.001f;
    failedTire.forwardSpeedMps = 22.0f;
    failedTire.wetness = 0.0f;
    failedTire.tireFailureStage = 3;
    failedTire.tireFailureEventSerial = 1;
    failedTire.tireFailureLeakMassFlowKgPerSecond = 0.080f;
    failedTire.tireFailureStructuralIntegrity = 0.70f;
    for (int sample = 0; sample < 5; ++sample)
    {
        world.recordContactPresentation(
            { secondLocal.x + 1.5f, secondLocal.y, secondLocal.z }, failedTire);
    }
    const std::uint64_t blowoutParticles =
        world.presentation().stats().emittedParticles;
    if (blowoutParticles < 9u || blowoutParticles > 18u)
        return false;
    for (int sample = 0; sample < 5; ++sample)
    {
        world.recordContactPresentation(
            { secondLocal.x + 1.5f, secondLocal.y, secondLocal.z }, failedTire);
    }
    if (world.presentation().stats().emittedParticles != blowoutParticles)
        return false;

    failedTire.tireFailureStage = 4;
    failedTire.tireFailureEventSerial = 2;
    failedTire.tireFailureTreadAttachment = 0.50f;
    world.recordContactPresentation(
        { secondLocal.x + 1.5f, secondLocal.y, secondLocal.z }, failedTire);
    if (world.presentation().stats().emittedParticles <= blowoutParticles)
        return false;

    world.presentation().clear();

    // Gravel/dirt are loose presentation surfaces but do not allocate
    // deformable terrain track cells. They still need deterministic sparse
    // dust/debris emission rather than silently losing a transient budget.
    const std::uint64_t emittedBeforeGravel = world.presentation().stats().emittedParticles;
    SurfacePresentationContact gravel = wetRoad;
    gravel.material = SurfaceMaterial::Gravel;
    gravel.forwardSpeedMps = 18.0f;
    gravel.longitudinalSlipSpeedMps = 2.5f;
    gravel.wetness = 0.05f;
    gravel.looseDepthM = 0.035f;
    for (int sample = 0; sample < 4000; ++sample)
    {
        world.recordContactPresentation(
            { secondLocal.x + 2.0f, secondLocal.y, secondLocal.z }, gravel);
    }
    if (world.presentation().stats().emittedParticles <= emittedBeforeGravel)
        return false;

    // TIRE16A: ordinary traction transmits force with small tire slip, but
    // that microscopic slip must not paint a visible black stripe. Visible
    // transfer is gated by both dissipated slip work and grip utilization.
    SurfacePresentationContact ordinaryTraction = wetRoad;
    ordinaryTraction.sourceStreamId = 0x11223344u;
    ordinaryTraction.wetness = 0.0f;
    ordinaryTraction.forwardSpeedMps = 18.0f;
    ordinaryTraction.longitudinalSlipSpeedMps = 0.42f;
    ordinaryTraction.lateralSpeedMps = 0.08f;
    ordinaryTraction.slipDissipationWatts = 2200.0f;
    ordinaryTraction.gripUtilization = 0.34f;
    ordinaryTraction.slipRatio = 0.03f;
    ordinaryTraction.slipAngleDegrees = 0.35f;
    ordinaryTraction.treadTemperatureC = 54.0f;
    ordinaryTraction.normalLoadN = 3600.0f;
    for (int sample = 0; sample < 48; ++sample)
    {
        const float z = secondLocal.z + static_cast<float>(sample) * 0.035f;
        world.recordContactPresentation(
            { secondLocal.x + 2.6f, secondLocal.y, z }, ordinaryTraction);
    }
    if (!world.presentation().tireMarkSegments().empty())
        return false;

    // TIRE16B: a normal road-speed corner may use substantial lateral grip and
    // dissipate several kW without actually sliding enough to smear visible
    // rubber. This is the roundabout false-positive caught during user testing.
    SurfacePresentationContact ordinaryRoundabout = ordinaryTraction;
    ordinaryRoundabout.sourceStreamId = 0x55667788u;
    ordinaryRoundabout.forwardSpeedMps = 13.5f;
    ordinaryRoundabout.longitudinalSlipSpeedMps = 0.18f;
    ordinaryRoundabout.lateralSpeedMps = 1.10f;
    ordinaryRoundabout.slipDissipationWatts = 10500.0f;
    ordinaryRoundabout.gripUtilization = 0.84f;
    ordinaryRoundabout.slipRatio = 0.018f;
    ordinaryRoundabout.slipAngleDegrees = 4.65f;
    ordinaryRoundabout.normalLoadN = 3950.0f;
    for (int sample = 0; sample < 56; ++sample)
    {
        const float z = secondLocal.z + static_cast<float>(sample) * 0.035f;
        world.recordContactPresentation(
            { secondLocal.x + 2.8f, secondLocal.y, z }, ordinaryRoundabout);
    }
    if (!world.presentation().tireMarkSegments().empty())
        return false;

    // TIRE16G: a nearly stationary chassis that is slowly dragged sideways down
    // a slope can report a huge mathematical slip angle, but the event lacks the
    // road-relative motion of a real drift. It must not paint an opaque skid.
    SurfacePresentationContact slowLateralDrag = ordinaryTraction;
    slowLateralDrag.sourceStreamId = 0xABCDEF01u;
    slowLateralDrag.forwardSpeedMps = 0.20f;
    slowLateralDrag.lateralSpeedMps = 0.85f;
    slowLateralDrag.longitudinalSlipSpeedMps = 0.05f;
    slowLateralDrag.slipDissipationWatts = 14000.0f;
    slowLateralDrag.gripUtilization = 0.99f;
    slowLateralDrag.slipRatio = 0.01f;
    slowLateralDrag.slipAngleDegrees = 78.0f;
    slowLateralDrag.normalLoadN = 3900.0f;
    for (int sample = 0; sample < 64; ++sample)
    {
        const float x = secondLocal.x + 3.2f + static_cast<float>(sample) * 0.020f;
        world.recordContactPresentation(
            { x, secondLocal.y, secondLocal.z + 0.5f }, slowLateralDrag);
    }
    if (!world.presentation().tireMarkSegments().empty())
        return false;

    // TIRE16: a high-slip hard-surface contact stream must become one
    // distance-resampled continuous trail. Endpoint intensity and the lateral
    // three-band pressure state remain attached to each segment so rendering can
    // make smooth gradients in both travel and tire-width directions.
    SurfacePresentationContact skid = wetRoad;
    skid.sourceStreamId = 0x12345678u;
    skid.wetness = 0.0f;
    skid.forwardSpeedMps = 24.0f;
    skid.longitudinalSlipSpeedMps = 9.0f;
    skid.lateralSpeedMps = 1.5f;
    skid.slipDissipationWatts = 62000.0f;
    skid.gripUtilization = 0.97f;
    skid.slipRatio = 0.58f;
    skid.slipAngleDegrees = 10.5f;
    skid.treadTemperatureC = 78.0f;
    skid.normalLoadN = 4100.0f;
    skid.insideLoadFraction = 0.52f;
    skid.centerLoadFraction = 0.36f;
    skid.outsideLoadFraction = 0.12f;

    for (int sample = 0; sample < 36; ++sample)
    {
        const float z = secondLocal.z + static_cast<float>(sample) * 0.035f;
        world.recordContactPresentation(
            { secondLocal.x + 3.0f, secondLocal.y, z }, skid);
    }
    const auto& strongMarks = world.presentation().tireMarkSegments();
    const std::size_t strongMarkCount = strongMarks.size();
    if (strongMarkCount < 5u
        || world.presentation().stats().activeTireMarkSegments != strongMarkCount)
    {
        return false;
    }
    const auto firstSkid = strongMarks.front();
    const std::uint64_t firstSkidSerial = world.presentation().firstTireMarkSerial();
    const std::uint64_t lastSkidSerial = world.presentation().lastTireMarkSerial();
    const auto* serialLookup = world.presentation().tireMarkSegmentBySerial(firstSkidSerial);
    if (firstSkidSerial == 0 || lastSkidSerial < firstSkidSerial
        || serialLookup == nullptr || serialLookup->serial != firstSkidSerial
        || world.presentation().tireMarkSegmentBySerial(lastSkidSerial + 1) != nullptr)
    {
        return false;
    }
    // TIRE16K1B: persistent GPU lookup is O(1) only because presentation serials
    // are contiguous. A skipped serial makes later records look absent and can
    // make the cached skid trail vanish after its first segment.
    for (std::size_t markIndex = 1; markIndex < strongMarks.size(); ++markIndex)
    {
        if (strongMarks[markIndex].serial != strongMarks[markIndex - 1].serial + 1)
            return false;
    }
    // TIRE16K1: connected skid segments retain an explicit predecessor serial so
    // the persistent GPU cache can clear only the previous tail's provisional
    // end feather instead of leaving a dark 11.5 cm barcode along the skid.
    if (!firstSkid.startFeather
        || firstSkid.endFeather
        || firstSkid.previousSegmentSerial != 0
        || strongMarks.size() < 2u
        || strongMarks[1].previousSegmentSerial != firstSkid.serial
        || strongMarks[1].startFeather
        || firstSkid.startIntensity <= 0.20f
        || firstSkid.endIntensity <= 0.20f
        || firstSkid.startLoadFractions[0] <= firstSkid.startLoadFractions[2]
        || std::abs(firstSkid.startGlobalPosition.x
            - (secondOrigin.x + static_cast<double>(secondLocal.x + 3.0f))) > 0.01)
    {
        return false;
    }

    // Reduce physical slip work while the same wheel continues moving. The
    // longitudinal endpoint should become progressively lighter rather than
    // spawning a separate uniformly dark rectangle.
    skid.longitudinalSlipSpeedMps = 0.55f;
    skid.lateralSpeedMps = 0.10f;
    skid.slipDissipationWatts = 1800.0f;
    skid.gripUtilization = 0.28f;
    skid.slipRatio = 0.025f;
    skid.slipAngleDegrees = 0.8f;
    for (int sample = 36; sample < 52; ++sample)
    {
        const float z = secondLocal.z + static_cast<float>(sample) * 0.035f;
        world.recordContactPresentation(
            { secondLocal.x + 3.0f, secondLocal.y, z }, skid);
    }
    const auto& fadedMarks = world.presentation().tireMarkSegments();
    if (fadedMarks.size() <= strongMarkCount
        || fadedMarks.back().endIntensity >= firstSkid.endIntensity * 0.35f
        || !fadedMarks.back().endFeather
        || heritage::physics::SurfacePresentation::kMaximumTireMarkSegments != 1000000u
        || heritage::physics::SurfacePresentation::kTireMarkRetirementSeconds != 1200.0
        || std::abs(firstSkid.startWidthM - skid.tireWidthM) > 1.0e-5f)
    {
        return false;
    }

    // TIRE16C: just below obvious sliding, a heavily worked/loaded shoulder may
    // leave a whisper trace rather than a binary on/off skid. It must remain far
    // weaker than the genuine skid and preserve the pressure asymmetry that lets
    // one side of the tire be barely more visible than the other.
    world.presentation().clear();
    SurfacePresentationContact shoulderTrace = ordinaryRoundabout;
    shoulderTrace.sourceStreamId = 0x99887766u;
    shoulderTrace.forwardSpeedMps = 15.0f;
    shoulderTrace.longitudinalSlipSpeedMps = 0.45f;
    shoulderTrace.lateralSpeedMps = 1.65f;
    shoulderTrace.slipDissipationWatts = 18000.0f;
    shoulderTrace.gripUtilization = 0.94f;
    shoulderTrace.slipRatio = 0.075f;
    shoulderTrace.slipAngleDegrees = 6.2f;
    shoulderTrace.normalLoadN = 4300.0f;
    shoulderTrace.insideLoadFraction = 0.56f;
    shoulderTrace.centerLoadFraction = 0.34f;
    shoulderTrace.outsideLoadFraction = 0.10f;
    for (int sample = 0; sample < 56; ++sample)
    {
        const float z = secondLocal.z + static_cast<float>(sample) * 0.035f;
        world.recordContactPresentation(
            { secondLocal.x + 4.0f, secondLocal.y, z }, shoulderTrace);
    }
    const auto& shoulderMarks = world.presentation().tireMarkSegments();
    if (shoulderMarks.empty()
        || shoulderMarks.front().startIntensity <= 0.001f
        || shoulderMarks.front().startIntensity >= 0.030f
        || shoulderMarks.front().startLoadFractions[0]
            <= shoulderMarks.front().startLoadFractions[2])
    {
        return false;
    }

    // TIRE16G: an abrupt support-normal/height transition (road to a curb
    // chamfer/sidewalk edge) must break the ribbon rather than bridge one flat
    // quad through open air. After the break, marking may resume on the new
    // support with a fresh cap.
    world.presentation().clear();
    SurfacePresentationContact curbSkid = skid;
    curbSkid.sourceStreamId = 0xA0B0C0D0u;
    curbSkid.forwardSpeedMps = 18.0f;
    curbSkid.longitudinalSlipSpeedMps = 7.0f;
    curbSkid.lateralSpeedMps = 1.5f;
    curbSkid.slipDissipationWatts = 52000.0f;
    curbSkid.gripUtilization = 0.97f;
    curbSkid.slipRatio = 0.52f;
    curbSkid.slipAngleDegrees = 10.0f;
    curbSkid.normal = { 0.0f, 1.0f, 0.0f };
    world.recordContactPresentation(
        { secondLocal.x + 5.0f, secondLocal.y, secondLocal.z }, curbSkid);
    curbSkid.normal = { 0.0f, 0.70f, 0.7141428f };
    world.recordContactPresentation(
        { secondLocal.x + 5.0f, secondLocal.y + 0.060f, secondLocal.z + 0.13f }, curbSkid);
    if (!world.presentation().tireMarkSegments().empty())
        return false;
    world.recordContactPresentation(
        { secondLocal.x + 5.13f, secondLocal.y + 0.060f, secondLocal.z + 0.13f }, curbSkid);
    const auto& curbMarks = world.presentation().tireMarkSegments();
    if (curbMarks.empty()
        || curbMarks.front().startGlobalPosition.y
            < secondOrigin.y + static_cast<double>(secondLocal.y + 0.045f))
    {
        return false;
    }

    // TIRE16J: age-driven history retirement is deterministic and independent
    // of camera distance or geometry LOD. Full six-control detail is a renderer
    // policy; stored FP64 history survives until just before twenty minutes,
    // then retires.
    for (int step = 0; step < 5999; ++step)
        world.presentation().advance(0.1f);
    if (world.presentation().tireMarkSegments().empty())
        return false;
    for (int step = 5999; step < 11999; ++step)
        world.presentation().advance(0.1f);
    if (world.presentation().tireMarkSegments().empty())
        return false;
    for (int step = 0; step < 3; ++step)
        world.presentation().advance(0.1f);
    if (!world.presentation().tireMarkSegments().empty())
        return false;

    // TIRE15C5: rubber flight moved out of the lossy generic presentation
    // cache. TrackRubberRegression now covers authoritative moving packets and
    // their eventual settlement into static TrackRubberState cells.

    world.presentation().clear();
    const auto cleared = world.presentation().stats();
    return cleared.activeTrackMarks == 0u
        && cleared.activeTireMarkSegments == 0u
        && cleared.activeParticles == 0u
        && cleared.contactSamples == 0u;
}

} // namespace heritage::tests
