#include "SurfacePresentation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::physics {
namespace {

constexpr double kTrackCellSizeM = 0.25;
constexpr double kVerticalLayerSizeM = 2.0;

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float length(const heritage::math::Vec3& value)
{
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitude = length(value);
    if (magnitude <= 1.0e-6f || !std::isfinite(magnitude))
        return fallback;
    return { value.x / magnitude, value.y / magnitude, value.z / magnitude };
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

bool deformablePresentationMaterial(SurfaceMaterial material)
{
    return material == SurfaceMaterial::Mud
        || material == SurfaceMaterial::Sand
        || material == SurfaceMaterial::SoftSoil
        || material == SurfaceMaterial::DeepSnow;
}

bool tireMarkMaterial(SurfaceMaterial material)
{
    return material == SurfaceMaterial::Default
        || material == SurfaceMaterial::Asphalt
        || material == SurfaceMaterial::Kerb
        || material == SurfaceMaterial::PaintedLine;
}

float smoothStep(float edge0, float edge1, float value)
{
    if (edge1 <= edge0)
        return value >= edge1 ? 1.0f : 0.0f;
    const float t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

double distanceBetween(
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool dustyMaterial(SurfaceMaterial material)
{
    return material == SurfaceMaterial::Gravel
        || material == SurfaceMaterial::Dirt
        || material == SurfaceMaterial::Sand
        || material == SurfaceMaterial::SoftSoil;
}

bool looseDebrisMaterial(SurfaceMaterial material)
{
    return material == SurfaceMaterial::Gravel
        || material == SurfaceMaterial::Dirt
        || material == SurfaceMaterial::Mud
        || material == SurfaceMaterial::Sand
        || material == SurfaceMaterial::SoftSoil
        || material == SurfaceMaterial::Snow
        || material == SurfaceMaterial::DeepSnow;
}

std::uint32_t mixSeed(std::uint64_t serial, std::uint32_t sequence)
{
    std::uint64_t value = serial + 0x9e3779b97f4a7c15ULL
        + static_cast<std::uint64_t>(sequence) * 0x85ebca6bULL;
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31;
    return static_cast<std::uint32_t>(value ^ (value >> 32));
}

float random01(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>((state >> 8) & 0x00ffffffu)
        / static_cast<float>(0x01000000u);
}

float signedRandom(std::uint32_t& state)
{
    return random01(state) * 2.0f - 1.0f;
}

void smoothToward(float& value, float target, float rate, float dt)
{
    const float blend = 1.0f - std::exp(-std::max(rate, 0.0f) * dt);
    value += (target - value) * blend;
}

} // namespace

std::size_t SurfacePresentation::TrackKeyHash::operator()(
    const TrackKey& key) const
{
    std::size_t seed = std::hash<std::int64_t>{}(key.x);
    const auto combine = [&](std::size_t value) {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    };
    combine(std::hash<std::int64_t>{}(key.z));
    combine(std::hash<std::int64_t>{}(key.verticalLayer));
    combine(std::hash<std::uint8_t>{}(key.material));
    return seed;
}

void SurfacePresentation::clear()
{
    m_trackSlots.clear();
    m_trackLookup.clear();
    m_trackMarks.clear();
    m_tireMarkSegments.clear();
    m_tireMarkTrails.clear();
    m_tireFailureEmitters.clear();
    m_particles.clear();
    m_nextTrackReplacement = 0;
    m_nextTireMarkReplacement = 0;
    m_nextTireMarkSerial = 1;
    m_nextParticleReplacement = 0;
    m_audio = {};
    m_audioTarget = {};
    m_updateSerial = 0;
    m_emittedParticles = 0;
    m_contactSamples = 0;
    m_rutIntensity = 0.0f;
    m_elapsedSeconds = 0.0;
}

SurfacePresentation::TrackKey SurfacePresentation::trackKey(
    const heritage::math::DVec3& globalPosition,
    SurfaceMaterial material) const
{
    TrackKey key;
    key.x = static_cast<std::int64_t>(std::floor(globalPosition.x / kTrackCellSizeM));
    key.z = static_cast<std::int64_t>(std::floor(globalPosition.z / kTrackCellSizeM));
    key.verticalLayer = static_cast<std::int64_t>(
        std::floor(globalPosition.y / kVerticalLayerSizeM));
    key.material = static_cast<std::uint8_t>(material);
    return key;
}

SurfacePresentation::TrackSlot& SurfacePresentation::acquireTrackSlot(
    const TrackKey& key)
{
    const auto existing = m_trackLookup.find(key);
    if (existing != m_trackLookup.end())
        return m_trackSlots[existing->second];

    if (m_trackSlots.size() < kMaximumTrackMarks)
    {
        const std::size_t index = m_trackSlots.size();
        m_trackSlots.push_back({});
        m_trackSlots.back().key = key;
        m_trackLookup.emplace(key, index);
        return m_trackSlots.back();
    }

    const std::size_t index = m_nextTrackReplacement % m_trackSlots.size();
    m_nextTrackReplacement = (m_nextTrackReplacement + 1) % m_trackSlots.size();
    m_trackLookup.erase(m_trackSlots[index].key);
    m_trackSlots[index] = {};
    m_trackSlots[index].key = key;
    m_trackLookup.emplace(key, index);
    return m_trackSlots[index];
}

std::size_t SurfacePresentation::appendTireMarkSegment(
    const TireMarkSample& start,
    const TireMarkSample& end,
    bool startFeather,
    bool endFeather,
    std::uint64_t previousSegmentSerial,
    std::uint64_t& serialOut)
{
    SurfaceTireMarkSegment segment;
    segment.startGlobalPosition = start.globalPosition;
    segment.endGlobalPosition = end.globalPosition;
    segment.startNormal = start.normal;
    segment.endNormal = end.normal;
    segment.startRight = start.right;
    segment.endRight = end.right;
    segment.material = end.material;
    segment.startWidthM = start.widthM;
    segment.endWidthM = end.widthM;
    segment.startIntensity = start.intensity;
    segment.endIntensity = end.intensity;
    segment.startLoadFractions = start.loadFractions;
    segment.endLoadFractions = end.loadFractions;
    segment.birthTimeSeconds = m_elapsedSeconds;
    segment.serial = m_nextTireMarkSerial++;
    if (segment.serial == 0)
        segment.serial = m_nextTireMarkSerial++;
    segment.previousSegmentSerial = previousSegmentSerial;
    segment.startFeather = startFeather;
    segment.endFeather = endFeather;
    serialOut = segment.serial;

    // TIRE16H: production history is large but bounded. Because marks are
    // chronological, dropping from the front preserves continuity and makes
    // the one-million-segment ceiling O(1) rather than shifting a huge vector.
    if (m_tireMarkSegments.size() >= kMaximumTireMarkSegments)
    {
        m_tireMarkSegments.pop_front();
        for (auto& [streamId, trail] : m_tireMarkTrails)
        {
            (void)streamId;
            if (trail.lastSegmentSerial == 0)
                continue;
            if (trail.lastSegmentIndex == 0)
            {
                trail.lastSegmentSerial = 0;
            }
            else
            {
                --trail.lastSegmentIndex;
            }
        }
    }
    m_tireMarkSegments.push_back(segment);
    return m_tireMarkSegments.size() - 1;
}

void SurfacePresentation::recordTireMark(
    const heritage::math::DVec3& globalPosition,
    const SurfacePresentationContact& contact)
{
    if (contact.sourceStreamId == 0 || !tireMarkMaterial(contact.material))
        return;

    const float slipSpeed = std::sqrt(
        contact.longitudinalSlipSpeedMps * contact.longitudinalSlipSpeedMps
        + contact.lateralSpeedMps * contact.lateralSpeedMps);
    const float slipPower = std::max(contact.slipDissipationWatts, 0.0f);
    // TIRE16B: visible pavement marking requires genuine sliding kinematics,
    // not merely a tire carrying substantial cornering force. Normal rolling
    // cornering can have meaningful slip work and high grip utilization while
    // staying at a modest slip angle; that must not paint a black roundabout.
    // Longitudinal lock/wheelspin and high-angle lateral sliding independently
    // open the transfer gate, and the response remains continuous around onset.
    const float longitudinalSlideActivation = smoothStep(
        0.12f, 0.30f, std::abs(contact.slipRatio));
    // TIRE16G: slip angle becomes numerically large when a nearly stationary
    // chassis is slowly dragged sideways down a slope. That is not the same
    // physical event as a high-energy drift and must not paint an opaque skid.
    // Gate lateral transfer by actual ground-relative motion, while leaving
    // longitudinal lock/wheelspin free to mark even during a stationary burnout.
    const float groundMotionSpeedMps = std::sqrt(
        contact.forwardSpeedMps * contact.forwardSpeedMps
        + contact.lateralSpeedMps * contact.lateralSpeedMps);
    const float lateralMotionActivation = smoothStep(
        1.75f, 4.50f, groundMotionSpeedMps);
    const float lateralSlideActivation = smoothStep(
        7.0f, 15.0f, std::abs(contact.slipAngleDegrees))
        * lateralMotionActivation;
    const float kinematicSlideActivation = 1.0f
        - (1.0f - longitudinalSlideActivation)
            * (1.0f - lateralSlideActivation);

    // TIRE16C keeps a deliberately tiny pre-slide transfer band. Real tires can
    // polish/smear one shoulder before they reach an obvious lockup or drift,
    // especially when camber/load transfer concentrates pressure. This trace is
    // capped far below a genuine skid so normal driving stays visually clean.
    const float longitudinalTraceActivation = smoothStep(
        0.065f, 0.12f, std::abs(contact.slipRatio));
    const float lateralTraceActivation = smoothStep(
        4.25f, 7.0f, std::abs(contact.slipAngleDegrees))
        * lateralMotionActivation;
    const float preSlideKinematicActivation = 1.0f
        - (1.0f - longitudinalTraceActivation)
            * (1.0f - lateralTraceActivation);

    const float slipActivation = smoothStep(0.65f, 2.20f, slipSpeed);
    const float meaningfulSlipPower = std::max(slipPower - 1800.0f, 0.0f);
    const float energyIntensity = 1.0f - std::exp(-meaningfulSlipPower / 28000.0f);
    const float gripActivation = smoothStep(
        0.55f, 0.92f, clamp01(contact.gripUtilization));
    const float traceGripActivation = smoothStep(
        0.74f, 0.96f, clamp01(contact.gripUtilization));
    const float traceEnergyIntensity = 1.0f - std::exp(
        -std::max(slipPower - 4200.0f, 0.0f) / 36000.0f);
    const float loadIntensity = clamp01(contact.normalLoadN / 5200.0f);
    const float temperatureFactor = 0.72f
        + 0.28f * smoothStep(28.0f, 92.0f, contact.treadTemperatureC);
    // LIVETRACK22: a water-saturated contact does not lay down a dry-looking
    // opaque skid. Keep a small residual transfer for hot rubber smearing, but
    // make full-wet deposition only ~8% of dry instead of the previous 28%.
    const float wetTransfer = 1.0f - 0.92f * clamp01(contact.wetness);
    float materialTransfer = 1.0f;
    if (contact.material == SurfaceMaterial::PaintedLine)
        materialTransfer = 0.78f;
    else if (contact.material == SurfaceMaterial::Kerb)
        materialTransfer = 0.72f;

    const float commonTransfer =
        (0.42f + 0.58f * loadIntensity)
        * temperatureFactor
        * wetTransfer
        * materialTransfer;

    // Genuine lockup/wheelspin/high-angle sliding owns the visible black trace.
    // The pre-slide contribution can only add a whisper (<= 2.2% intensity),
    // allowing a heavily loaded shoulder to be barely readable without turning
    // an ordinary roundabout into a painted skid pad.
    const float slideIntensity =
        kinematicSlideActivation
        * slipActivation
        * energyIntensity
        * gripActivation
        * commonTransfer;
    const float preSlideTraceIntensity = std::min(
        0.022f,
        preSlideKinematicActivation
            * traceGripActivation
            * traceEnergyIntensity
            * commonTransfer
            * 0.055f);
    const float intensity = clamp01(slideIntensity + preSlideTraceIntensity);

    TireMarkSample sample;
    sample.valid = true;
    sample.globalPosition = globalPosition;
    sample.normal = normalized(contact.normal, { 0.0f, 1.0f, 0.0f });
    heritage::math::Vec3 forward = normalized(
        contact.forward, { 0.0f, 0.0f, 1.0f });
    sample.right = normalized(cross(sample.normal, forward), { 1.0f, 0.0f, 0.0f });
    forward = normalized(cross(sample.right, sample.normal), forward);
    sample.right = normalized(cross(sample.normal, forward), sample.right);
    sample.material = contact.material;
    // TIRE16J: preserve the authored/configured tire width for the visual
    // contact trace. The previous 0.97 multiplier trimmed another 3% from an
    // already physical tire-width input and made the mark read slightly too
    // narrow beside the actual tread in close-up testing.
    sample.widthM = std::clamp(contact.tireWidthM, 0.075f, 0.65f);
    sample.intensity = intensity;

    std::array<float, 3> loads{
        std::max(contact.insideLoadFraction, 0.0f),
        std::max(contact.centerLoadFraction, 0.0f),
        std::max(contact.outsideLoadFraction, 0.0f)
    };
    const float loadSum = loads[0] + loads[1] + loads[2];
    if (loadSum > 1.0e-5f && std::isfinite(loadSum))
    {
        for (float& load : loads)
            load /= loadSum;
    }
    else
    {
        loads = { 0.30f, 0.40f, 0.30f };
    }
    sample.loadFractions = loads;

    TireMarkTrail& trail = m_tireMarkTrails[contact.sourceStreamId];
    // A wheel that has been airborne or otherwise out of hard-surface contact
    // must not reconnect its old skid to the landing point. Simulation time is
    // advanced once per world step, so this remains independent of the tire
    // substep frequency.
    constexpr double kMaximumContactGapSeconds = 0.080;
    if (trail.lastContactTimeSeconds >= 0.0
        && m_elapsedSeconds - trail.lastContactTimeSeconds > kMaximumContactGapSeconds)
    {
        trail = {};
    }
    trail.lastContactTimeSeconds = m_elapsedSeconds;

    if (!trail.anchor.valid)
    {
        trail.anchor = sample;
        return;
    }

    const double distance = distanceBetween(trail.anchor.globalPosition, sample.globalPosition);
    const double deltaX = sample.globalPosition.x - trail.anchor.globalPosition.x;
    const double deltaY = sample.globalPosition.y - trail.anchor.globalPosition.y;
    const double deltaZ = sample.globalPosition.z - trail.anchor.globalPosition.z;
    const float normalAlignment =
        trail.anchor.normal.x * sample.normal.x
        + trail.anchor.normal.y * sample.normal.y
        + trail.anchor.normal.z * sample.normal.z;
    const double averageNormalX = static_cast<double>(
        trail.anchor.normal.x + sample.normal.x) * 0.5;
    const double averageNormalY = static_cast<double>(
        trail.anchor.normal.y + sample.normal.y) * 0.5;
    const double averageNormalZ = static_cast<double>(
        trail.anchor.normal.z + sample.normal.z) * 0.5;
    const double supportStepM = std::abs(
        deltaX * averageNormalX
        + deltaY * averageNormalY
        + deltaZ * averageNormalZ);
    // TIRE16G: do not bridge a single flat ribbon across an abrupt kerb/chamfer
    // transition. A sharp support-normal change or a true support-plane step
    // breaks the trail; the next contact starts a new conforming strip instead
    // of visibly floating through the air between road and sidewalk.
    const bool supportDiscontinuity = normalAlignment < 0.90f
        || (distance < 0.35 && supportStepM > 0.022);
    const bool discontinuity = trail.anchor.material != sample.material
        || distance > 1.25
        || supportDiscontinuity;
    if (discontinuity)
    {
        trail = {};
        trail.anchor = sample;
        trail.lastContactTimeSeconds = m_elapsedSeconds;
        return;
    }

    // A real trail is distance-resampled rather than stamped at the 1 kHz tire
    // rate. Intensity changes can force an earlier control point so the mark
    // smoothly darkens/fades longitudinally as slip work rises and falls.
    constexpr double kTargetSegmentLengthM = 0.115;
    constexpr float kIntensityChangeThreshold = 0.075f;
    constexpr float kMinimumVisibleIntensity = 0.0015f;
    const float intensityChange = std::abs(sample.intensity - trail.anchor.intensity);
    const bool shouldEmit = distance >= kTargetSegmentLengthM
        || (distance >= 0.025 && intensityChange >= kIntensityChangeThreshold);

    if (shouldEmit)
    {
        if (std::max(sample.intensity, trail.anchor.intensity)
            >= kMinimumVisibleIntensity)
        {
            bool startFeather = true;
            if (trail.lastSegmentSerial != 0
                && trail.lastSegmentIndex < m_tireMarkSegments.size()
                && m_tireMarkSegments[trail.lastSegmentIndex].serial
                    == trail.lastSegmentSerial)
            {
                m_tireMarkSegments[trail.lastSegmentIndex].endFeather = false;
                startFeather = false;
            }

            std::uint64_t serial = 0;
            const std::size_t index = appendTireMarkSegment(
                trail.anchor, sample, startFeather, true,
                startFeather ? 0 : trail.lastSegmentSerial, serial);
            trail.lastSegmentIndex = index;
            trail.lastSegmentSerial = serial;
        }
        else
        {
            // Once both ends are effectively clean rolling, break the visual
            // chain. The next real skid begins with a soft longitudinal cap.
            trail.lastSegmentSerial = 0;
        }
        trail.anchor = sample;
        trail.stationarySeconds = 0.0f;
        return;
    }

    // Stationary/near-stationary wheelspin still burns a footprint. Emit a
    // sparse short ribbon with soft caps; repeated physical slip can layer it
    // darker without allocating at the high-rate contact frequency.
    if (distance < 0.025 && sample.intensity > 0.10f)
    {
        trail.stationarySeconds += std::max(contact.deltaTimeSeconds, 0.0f);
        if (trail.stationarySeconds >= 0.080f)
        {
            TireMarkSample back = sample;
            TireMarkSample front = sample;
            const heritage::math::Vec3 localForward = normalized(
                cross(sample.right, sample.normal), { 0.0f, 0.0f, 1.0f });
            constexpr double kStationaryHalfLengthM = 0.055;
            back.globalPosition.x -= static_cast<double>(localForward.x) * kStationaryHalfLengthM;
            back.globalPosition.y -= static_cast<double>(localForward.y) * kStationaryHalfLengthM;
            back.globalPosition.z -= static_cast<double>(localForward.z) * kStationaryHalfLengthM;
            front.globalPosition.x += static_cast<double>(localForward.x) * kStationaryHalfLengthM;
            front.globalPosition.y += static_cast<double>(localForward.y) * kStationaryHalfLengthM;
            front.globalPosition.z += static_cast<double>(localForward.z) * kStationaryHalfLengthM;
            std::uint64_t serial = 0;
            appendTireMarkSegment(back, front, true, true, 0, serial);
            trail.stationarySeconds = 0.0f;
        }
    }
}

void SurfacePresentation::emitParticle(
    const heritage::math::DVec3& globalPosition,
    const SurfacePresentationContact& contact,
    SurfaceParticleKind kind,
    float intensity,
    std::uint32_t sequence)
{
    if (intensity <= 0.0f)
        return;

    SurfacePresentationParticle particle;
    particle.globalPosition = globalPosition;
    particle.kind = kind;
    particle.material = contact.material;
    particle.seed = mixSeed(m_updateSerial, sequence);
    std::uint32_t randomState = particle.seed;

    const heritage::math::Vec3 normal = normalized(
        contact.normal, { 0.0f, 1.0f, 0.0f });
    heritage::math::Vec3 forward = normalized(
        contact.forward, { 0.0f, 0.0f, 1.0f });
    const heritage::math::Vec3 right = normalized(
        cross(normal, forward), { 1.0f, 0.0f, 0.0f });
    forward = normalized(cross(right, normal), forward);

    const float speed = std::abs(contact.forwardSpeedMps);
    const float rearwardSign = contact.forwardSpeedMps >= 0.0f ? -1.0f : 1.0f;
    const float lateralJitter = signedRandom(randomState);
    const float forwardJitter = signedRandom(randomState);
    const float upwardJitter = random01(randomState);
    float tireFailureSideSign = 0.0f;

    float rearwardSpeed = 0.8f + speed * 0.16f;
    float upwardSpeed = 0.8f + speed * 0.06f;
    float lateralSpeed = 0.4f + speed * 0.025f;
    particle.lifetimeSeconds = 0.75f;
    particle.sizeM = 0.04f;
    particle.opacity = 0.65f;

    switch (kind)
    {
    case SurfaceParticleKind::WaterSpray:
        rearwardSpeed = 1.5f + speed * 0.28f;
        upwardSpeed = 0.65f + speed * 0.055f;
        lateralSpeed = 0.8f + speed * 0.045f;
        particle.lifetimeSeconds = 0.55f + 0.35f * random01(randomState);
        particle.sizeM = 0.025f + 0.035f * random01(randomState);
        particle.opacity = 0.42f + 0.22f * intensity;
        break;
    case SurfaceParticleKind::Dust:
        rearwardSpeed = 0.6f + speed * 0.12f;
        upwardSpeed = 0.45f + speed * 0.035f;
        lateralSpeed = 0.6f + speed * 0.03f;
        particle.lifetimeSeconds = 1.1f + 1.2f * random01(randomState);
        particle.sizeM = 0.05f + 0.09f * random01(randomState);
        particle.opacity = 0.22f + 0.30f * intensity;
        break;
    case SurfaceParticleKind::Mud:
        particle.lifetimeSeconds = 0.65f + 0.55f * random01(randomState);
        particle.sizeM = 0.025f + 0.055f * random01(randomState);
        particle.opacity = 0.75f;
        break;
    case SurfaceParticleKind::Snow:
        upwardSpeed += 0.5f;
        lateralSpeed += 0.5f;
        particle.lifetimeSeconds = 0.9f + 0.8f * random01(randomState);
        particle.sizeM = 0.035f + 0.075f * random01(randomState);
        particle.opacity = 0.68f;
        break;
    case SurfaceParticleKind::RubberShred:
    {
        // Physically driven but intentionally modest toss: a fragment leaves
        // with some tire-surface velocity, scrub direction and a small normal
        // component. It is a transient representative of persistent TrackRubber
        // state, not an independent physics object.
        const float severity = clamp01(contact.rubberFragmentSeverity);
        const float surfaceSpeed = std::abs(contact.tireSurfaceSpeedMps);
        rearwardSpeed = 0.20f + surfaceSpeed * (0.035f + 0.020f * severity);
        upwardSpeed = 0.08f + severity * 0.42f + speed * 0.006f;
        lateralSpeed = 0.10f
            + std::abs(contact.lateralSpeedMps) * (0.10f + 0.08f * severity);
        particle.lifetimeSeconds = 0.55f + 0.75f * random01(randomState);
        particle.sizeM = (0.006f + 0.010f * random01(randomState))
            * (0.85f + 1.15f * severity);
        particle.opacity = 0.96f;
        particle.supportPlanePoint = globalPosition;
        particle.supportPlaneNormal = normal;
        particle.settlesOnSupportPlane = true;
        break;
    }
    case SurfaceParticleKind::TireFailureSmoke:
        // A blowout initially expels a cool aerosol of air, rubber dust and
        // road dirt. This is intentionally a short translucent puff, not fire.
        rearwardSpeed = 0.35f + speed * 0.065f;
        upwardSpeed = 0.35f + speed * 0.018f;
        lateralSpeed = 1.35f + 2.65f * clamp01(intensity)
            + speed * 0.018f;
        particle.lifetimeSeconds = 0.80f + 1.10f * random01(randomState);
        particle.sizeM = (0.075f + 0.120f * random01(randomState))
            * (0.85f + 0.65f * clamp01(intensity));
        particle.opacity = 0.28f + 0.32f * clamp01(intensity);
        break;
    case SurfaceParticleKind::TireFailureDebris:
    {
        // Presentation fragments represent bounded pieces from the authored
        // tire. They never become vehicle-physics bodies or affect contact.
        const float severity = clamp01(intensity);
        rearwardSpeed = 0.35f + speed * (0.06f + 0.045f * severity);
        upwardSpeed = 0.18f + severity * 0.95f + speed * 0.008f;
        lateralSpeed = 0.24f + speed * 0.025f;
        particle.lifetimeSeconds = 1.15f + 1.75f * random01(randomState);
        particle.sizeM = (0.028f + 0.070f * random01(randomState))
            * (0.75f + 1.20f * severity);
        particle.opacity = 0.98f;
        particle.supportPlanePoint = globalPosition;
        particle.supportPlaneNormal = normal;
        particle.settlesOnSupportPlane = true;
        break;
    }
    case SurfaceParticleKind::LooseDebris:
    default:
        particle.lifetimeSeconds = 0.55f + 0.65f * random01(randomState);
        particle.sizeM = 0.018f + 0.035f * random01(randomState);
        particle.opacity = 0.80f;
        break;
    }

    // Contact presentation is recorded at the support point. Tire-failure
    // particles instead originate around the loaded tire ring so a blowout
    // visibly vents at the bead/sidewall and then across the cavity rather
    // than looking like road dust emitted from one point beneath the wheel.
    if (kind == SurfaceParticleKind::TireFailureSmoke
        || kind == SurfaceParticleKind::TireFailureDebris)
    {
        const float radiusM = std::clamp(contact.tireRadiusM, 0.05f, 1.20f);
        const float theta = random01(randomState) * 6.28318530718f;
        const float radialScale = kind == SurfaceParticleKind::TireFailureSmoke
            ? (0.62f + 0.25f * random01(randomState))
            : (0.88f + 0.10f * random01(randomState));
        tireFailureSideSign = signedRandom(randomState) >= 0.0f ? 1.0f : -1.0f;
        const float axialOffset = tireFailureSideSign
            * std::max(contact.tireWidthM, 0.04f)
            * (kind == SurfaceParticleKind::TireFailureSmoke ? 0.52f : 0.44f);
        particle.globalPosition.x += static_cast<double>(
            normal.x * radiusM
            + (forward.x * std::cos(theta) + normal.x * std::sin(theta))
                * radiusM * radialScale
            + right.x * axialOffset);
        particle.globalPosition.y += static_cast<double>(
            normal.y * radiusM
            + (forward.y * std::cos(theta) + normal.y * std::sin(theta))
                * radiusM * radialScale
            + right.y * axialOffset);
        particle.globalPosition.z += static_cast<double>(
            normal.z * radiusM
            + (forward.z * std::cos(theta) + normal.z * std::sin(theta))
                * radiusM * radialScale
            + right.z * axialOffset);
    }

    particle.velocityMps = add(
        scale(forward, rearwardSign * (rearwardSpeed + forwardJitter * 0.25f)),
        add(
            scale(right,
                tireFailureSideSign != 0.0f
                    ? tireFailureSideSign * lateralSpeed
                    : lateralJitter * lateralSpeed),
            scale(normal, upwardSpeed * (0.55f + upwardJitter * 0.8f))));

    // Spawn a few millimetres above the support plane to avoid immediate
    // depth fighting with the road/terrain.
    particle.globalPosition.x += static_cast<double>(normal.x) * 0.012;
    particle.globalPosition.y += static_cast<double>(normal.y) * 0.012;
    particle.globalPosition.z += static_cast<double>(normal.z) * 0.012;

    if (m_particles.size() < kMaximumParticles)
    {
        m_particles.push_back(particle);
    }
    else
    {
        m_particles[m_nextParticleReplacement] = particle;
        m_nextParticleReplacement =
            (m_nextParticleReplacement + 1) % m_particles.size();
    }
    ++m_emittedParticles;
}

void SurfacePresentation::accumulateAudio(
    const SurfacePresentationContact& contact,
    float sprayIntensity,
    float dustIntensity,
    float debrisIntensity)
{
    const float speedIntensity = clamp01(
        (std::abs(contact.forwardSpeedMps) - 0.5f) / 24.0f);
    const float loadIntensity = clamp01(contact.normalLoadN / 5000.0f);
    m_audioTarget.rolling = std::max(
        m_audioTarget.rolling,
        speedIntensity * (0.35f + 0.65f * loadIntensity));
    m_audioTarget.spray = std::max(m_audioTarget.spray, sprayIntensity);
    m_audioTarget.dust = std::max(m_audioTarget.dust, dustIntensity);
    m_audioTarget.debris = std::max(m_audioTarget.debris, debrisIntensity);
}

void SurfacePresentation::recordContact(
    const heritage::math::DVec3& globalPosition,
    const SurfacePresentationContact& contact)
{
    if (!std::isfinite(globalPosition.x)
        || !std::isfinite(globalPosition.y)
        || !std::isfinite(globalPosition.z))
    {
        return;
    }

    ++m_contactSamples;
    ++m_updateSerial;

    recordTireMark(globalPosition, contact);

    const float speed = std::abs(contact.forwardSpeedMps);
    const float slipSpeed = std::sqrt(
        contact.longitudinalSlipSpeedMps * contact.longitudinalSlipSpeedMps
        + contact.lateralSpeedMps * contact.lateralSpeedMps);
    const float motionIntensity = clamp01((speed + slipSpeed * 0.65f - 1.0f) / 18.0f);
    const float loadIntensity = clamp01(contact.normalLoadN / 5000.0f);
    const float wetness = clamp01(contact.wetness);
    const float displacementIntensity = clamp01(
        contact.displacedVolumeDeltaM3 / 0.000004f
        + contact.rutDepthDeltaM / 0.0015f);
    const float looseIntensity = clamp01(contact.looseDepthM / 0.10f);

    float particleAccumulatorContribution = 0.0f;
    TrackSlot* deformableTrack = nullptr;
    if (deformablePresentationMaterial(contact.material))
    {
        TrackSlot& slot = acquireTrackSlot(trackKey(globalPosition, contact.material));
        deformableTrack = &slot;
        SurfaceTrackMark& mark = slot.mark;
        mark.globalPosition = globalPosition;
        mark.normal = normalized(contact.normal, { 0.0f, 1.0f, 0.0f });
        mark.forward = normalized(contact.forward, { 0.0f, 0.0f, 1.0f });
        mark.material = contact.material;
        mark.widthM = std::clamp(contact.tireWidthM * 1.02f, 0.08f, 0.65f);
        mark.lengthM = std::clamp(0.28f + speed * 0.006f, 0.28f, 0.55f);
        mark.rutDepthM = std::max(mark.rutDepthM, std::max(contact.rutDepthM, 0.0f));
        mark.displacedVolumeM3 += std::max(contact.displacedVolumeDeltaM3, 0.0f);
        mark.intensity = clamp01(
            mark.rutDepthM / 0.10f
            + clamp01(mark.displacedVolumeM3 / 0.0025f) * 0.45f);
        mark.updateSerial = m_updateSerial;
        m_rutIntensity = std::max(m_rutIntensity, mark.intensity);

        if (m_trackMarks.size() != m_trackSlots.size())
            m_trackMarks.resize(m_trackSlots.size());
        const std::size_t index = static_cast<std::size_t>(&slot - m_trackSlots.data());
        m_trackMarks[index] = mark;

        particleAccumulatorContribution =
            (0.20f + displacementIntensity * 2.8f + looseIntensity * 0.75f)
            * motionIntensity * (0.35f + loadIntensity)
            * std::clamp(contact.deltaTimeSeconds * 120.0f, 0.0f, 0.25f);
        slot.particleAccumulator += particleAccumulatorContribution;
    }

    const bool waterSurface = wetness > 0.08f && speed > 2.0f;
    const float sprayIntensity = waterSurface
        ? clamp01(wetness * motionIntensity * (0.35f + loadIntensity * 0.65f))
        : 0.0f;
    const float dustIntensity = dustyMaterial(contact.material)
        && wetness < 0.38f && speed > 1.5f
        ? clamp01((1.0f - wetness) * motionIntensity
            * (0.45f + looseIntensity * 0.55f))
        : 0.0f;
    const float debrisIntensity = looseDebrisMaterial(contact.material)
        ? clamp01(motionIntensity * (0.25f + looseIntensity * 0.35f
            + displacementIntensity * 0.75f))
        : 0.0f;

    accumulateAudio(contact, sprayIntensity, dustIntensity, debrisIntensity);

    // Keep high-rate tire simulation from becoming high-rate particle spawn.
    // Each deformable presentation cell retains a fractional emission budget;
    // hard-surface spray uses a tiny stateless probability-like budget tied to
    // dt so 1000 Hz and 250 Hz remain broadly comparable.
    std::uint32_t emissionSequence = 0;
    auto emitBudget = [&](SurfaceParticleKind kind, float intensity, float& budget) {
        budget += intensity
            * std::clamp(contact.deltaTimeSeconds * 42.0f, 0.0f, 0.22f);
        int emittedThisSample = 0;
        while (budget >= 1.0f && emittedThisSample < 2)
        {
            emitParticle(globalPosition, contact, kind, intensity, emissionSequence++);
            budget -= 1.0f;
            ++emittedThisSample;
        }
    };

    SurfaceParticleKind primaryParticleKind = SurfaceParticleKind::LooseDebris;
    float primaryParticleIntensity = 0.0f;
    if (contact.material == SurfaceMaterial::Mud && debrisIntensity > 0.0f)
    {
        primaryParticleKind = SurfaceParticleKind::Mud;
        primaryParticleIntensity = debrisIntensity;
    }
    else if ((contact.material == SurfaceMaterial::Snow
        || contact.material == SurfaceMaterial::DeepSnow)
        && debrisIntensity > 0.0f)
    {
        primaryParticleKind = SurfaceParticleKind::Snow;
        primaryParticleIntensity = debrisIntensity;
    }
    else if (dustIntensity > 0.0f)
    {
        primaryParticleKind = SurfaceParticleKind::Dust;
        primaryParticleIntensity = dustIntensity;
    }
    else if (debrisIntensity > 0.0f)
    {
        primaryParticleKind = SurfaceParticleKind::LooseDebris;
        primaryParticleIntensity = debrisIntensity;
    }

    if (primaryParticleIntensity > 0.0f)
    {
        if (deformableTrack)
        {
            float budget = deformableTrack->particleAccumulator;
            emitBudget(primaryParticleKind, primaryParticleIntensity, budget);
            deformableTrack->particleAccumulator = budget;
        }
        else
        {
            // Gravel and hard dirt do not own deformable track cells, so they
            // cannot retain a fractional particle budget there. Use the same
            // deterministic dt-scaled sparse-emission approach as pavement
            // spray. This keeps loose-surface dust/debris visible without
            // adding presentation-only state to the authoritative SurfaceField.
            const float threshold = primaryParticleIntensity
                * std::clamp(contact.deltaTimeSeconds * 24.0f, 0.0f, 0.16f);
            const std::uint32_t seed = mixSeed(m_updateSerial, 0x2du);
            if (static_cast<float>(seed & 0xffffu) / 65535.0f < threshold)
            {
                emitParticle(
                    globalPosition,
                    contact,
                    primaryParticleKind,
                    primaryParticleIntensity,
                    emissionSequence++);
            }
        }
    }

    // Water spray may happen on hard pavement where there is no deformable
    // track slot. A deterministic high-rate accumulator derived from the
    // global serial keeps it sparse without allocating a second field.
    if (sprayIntensity > 0.0f)
    {
        const float threshold = sprayIntensity
            * std::clamp(contact.deltaTimeSeconds * 28.0f, 0.0f, 0.18f);
        const std::uint32_t seed = mixSeed(m_updateSerial, 0x51u);
        if (static_cast<float>(seed & 0xffffu) / 65535.0f < threshold)
            emitParticle(
                globalPosition,
                contact,
                SurfaceParticleKind::WaterSpray,
                sprayIntensity,
                emissionSequence++);
    }

    // TIRE19: one wheel owns one bounded emitter keyed by the same persistent
    // stream used for its tire mark. Direct trigger serials produce a one-shot
    // burst; sustained venting/carcass or tread destruction accrue a small dt-
    // scaled budget. Thus 1000 Hz player tires and lower-rate AI presentation
    // produce comparable output without spawning thousands of particles.
    if (contact.sourceStreamId != 0)
    {
        TireFailureEmitter& failureEmitter =
            m_tireFailureEmitters[contact.sourceStreamId];
        failureEmitter.lastContactTimeSeconds = m_elapsedSeconds;
        if (contact.tireFailureStage == 0)
        {
            failureEmitter.lastEventSerial = contact.tireFailureEventSerial;
            failureEmitter.lastStage = 0;
            failureEmitter.smokeBudget = 0.0f;
            failureEmitter.debrisBudget = 0.0f;
        }
        else
        {
        const bool newEvent = contact.tireFailureEventSerial != 0
            && contact.tireFailureEventSerial != failureEmitter.lastEventSerial;
        const bool newStage = contact.tireFailureStage > failureEmitter.lastStage;
        if (newEvent)
            failureEmitter.lastEventSerial = contact.tireFailureEventSerial;
        if (newStage)
            failureEmitter.lastStage = contact.tireFailureStage;

        const float leakIntensity = clamp01(
            contact.tireFailureLeakMassFlowKgPerSecond / 0.020f);
        const float carcassDamage = clamp01(
            1.0f - contact.tireFailureStructuralIntegrity);
        const float treadLoss = clamp01(
            1.0f - contact.tireFailureTreadAttachment);
        const float rimContact = clamp01(contact.tireFailureRimContactFraction);
        const float speedActivation = smoothStep(1.5f, 18.0f, speed);
        const float dt = std::clamp(contact.deltaTimeSeconds, 0.0f, 0.05f);

        failureEmitter.smokeBudget += dt
            * (2.0f * leakIntensity
                + speedActivation * (1.8f * carcassDamage + 2.6f * rimContact));
        failureEmitter.debrisBudget += dt * speedActivation
            * (0.7f * carcassDamage + 4.0f * treadLoss + 1.5f * rimContact);
        if (newEvent || newStage)
        {
            // A new incident must be legible even at rest. Air is transparent,
            // so the stage-1/2 wisps are explicitly sparse diagnostic aerosol;
            // stages 3+ add the much larger dust/condensation burst and rubber.
            if (contact.tireFailureStage == 1)
                failureEmitter.smokeBudget += 2.0f;
            else if (contact.tireFailureStage == 2)
                failureEmitter.smokeBudget += 4.0f;
            else if (contact.tireFailureStage == 3)
            {
                failureEmitter.smokeBudget += 12.0f;
                failureEmitter.debrisBudget += 4.0f;
            }
            else if (contact.tireFailureStage == 4)
            {
                failureEmitter.smokeBudget += 5.0f;
                failureEmitter.debrisBudget += 11.0f;
            }
            else if (contact.tireFailureStage == 5)
            {
                failureEmitter.smokeBudget += 8.0f;
                failureEmitter.debrisBudget += 12.0f;
            }
            else if (contact.tireFailureStage >= 6)
            {
                failureEmitter.smokeBudget += 8.0f;
                failureEmitter.debrisBudget += 22.0f;
            }
        }

        int failureEmissions = 0;
        while (failureEmitter.smokeBudget >= 1.0f && failureEmissions < 3)
        {
            emitParticle(globalPosition, contact,
                SurfaceParticleKind::TireFailureSmoke,
                std::max(leakIntensity, std::max(carcassDamage, rimContact)),
                0x1900u + emissionSequence++);
            failureEmitter.smokeBudget -= 1.0f;
            ++failureEmissions;
        }
        failureEmissions = 0;
        while (failureEmitter.debrisBudget >= 1.0f && failureEmissions < 3)
        {
            emitParticle(globalPosition, contact,
                SurfaceParticleKind::TireFailureDebris,
                std::max(treadLoss, std::max(carcassDamage, rimContact)),
                0x1940u + emissionSequence++);
            failureEmitter.debrisBudget -= 1.0f;
            ++failureEmissions;
        }
        failureEmitter.smokeBudget = std::min(failureEmitter.smokeBudget, 24.0f);
        failureEmitter.debrisBudget = std::min(failureEmitter.debrisBudget, 32.0f);
        }
    }

    // TIRE15C5: rubber flight is no longer a lossy generic presentation
    // particle. TrackRubberState owns authoritative AIRBORNE/MOBILE_GROUND
    // packets and the renderer draws those packets as deformable two-triangle
    // flakes. Keep freshRubberShed in the contact payload for telemetry/API
    // compatibility, but do not duplicate it here as a point sprite.

}

void SurfacePresentation::advance(float deltaTimeSeconds)
{
    if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0f)
        return;

    const float dt = std::min(deltaTimeSeconds, 0.1f);
    m_elapsedSeconds += static_cast<double>(dt);

    // TIRE16K: age never changes tire-mark geometry. The renderer keeps the
    // six-control pressure profile for near marks and performs the full twenty-
    // minute opacity fade in the GPU shader. Chronological deque storage keeps
    // retirement O(1), while renderer-side serial access lets only newly added
    // segments enter the persistent GPU cache.
    std::size_t retiredTireMarks = 0;
    while (!m_tireMarkSegments.empty()
        && m_elapsedSeconds - m_tireMarkSegments.front().birthTimeSeconds
            >= kTireMarkRetirementSeconds)
    {
        m_tireMarkSegments.pop_front();
        ++retiredTireMarks;
    }
    if (retiredTireMarks > 0)
    {
        for (auto& [streamId, trail] : m_tireMarkTrails)
        {
            (void)streamId;
            if (trail.lastSegmentSerial == 0)
                continue;
            if (trail.lastSegmentIndex < retiredTireMarks)
            {
                trail.lastSegmentIndex = 0;
                trail.lastSegmentSerial = 0;
            }
            else
            {
                trail.lastSegmentIndex -= retiredTireMarks;
            }
        }
    }

    for (SurfacePresentationParticle& particle : m_particles)
    {
        if (particle.ageSeconds >= particle.lifetimeSeconds)
            continue;

        particle.ageSeconds += dt;
        if (particle.ageSeconds >= particle.lifetimeSeconds)
            continue;
        if (particle.settled)
            continue;

        const bool failureSmoke =
            particle.kind == SurfaceParticleKind::TireFailureSmoke;
        const bool heavyRubber = particle.kind == SurfaceParticleKind::RubberShred
            || particle.kind == SurfaceParticleKind::TireFailureDebris;
        const float gravityScale = particle.kind == SurfaceParticleKind::Dust
            || particle.kind == SurfaceParticleKind::WaterSpray
            || failureSmoke
            ? 0.28f
            : (heavyRubber ? 1.0f : 0.85f);
        particle.velocityMps.y -= 9.80665f * gravityScale * dt;
        const float dragRate = particle.kind == SurfaceParticleKind::Dust
            ? 1.6f
            : (particle.kind == SurfaceParticleKind::WaterSpray
                ? 2.8f
                : (failureSmoke ? 1.9f : (heavyRubber ? 0.55f : 0.9f)));
        const float drag = std::exp(-dragRate * dt);
        particle.velocityMps.x *= drag;
        particle.velocityMps.y *= drag;
        particle.velocityMps.z *= drag;
        particle.globalPosition.x += static_cast<double>(particle.velocityMps.x * dt);
        particle.globalPosition.y += static_cast<double>(particle.velocityMps.y * dt);
        particle.globalPosition.z += static_cast<double>(particle.velocityMps.z * dt);

        if (particle.settlesOnSupportPlane)
        {
            const heritage::math::Vec3 supportNormal = normalized(
                particle.supportPlaneNormal, { 0.0f, 1.0f, 0.0f });
            const heritage::math::DVec3 delta{
                particle.globalPosition.x - particle.supportPlanePoint.x,
                particle.globalPosition.y - particle.supportPlanePoint.y,
                particle.globalPosition.z - particle.supportPlanePoint.z
            };
            const double signedHeight =
                delta.x * static_cast<double>(supportNormal.x)
                + delta.y * static_cast<double>(supportNormal.y)
                + delta.z * static_cast<double>(supportNormal.z);
            const float normalVelocity =
                particle.velocityMps.x * supportNormal.x
                + particle.velocityMps.y * supportNormal.y
                + particle.velocityMps.z * supportNormal.z;
            if (signedHeight <= 0.004 && normalVelocity < 0.0f)
            {
                const double correction = 0.004 - signedHeight;
                particle.globalPosition.x +=
                    static_cast<double>(supportNormal.x) * correction;
                particle.globalPosition.y +=
                    static_cast<double>(supportNormal.y) * correction;
                particle.globalPosition.z +=
                    static_cast<double>(supportNormal.z) * correction;
                particle.velocityMps = {};
                particle.settled = true;
                // Leave the landed representative visible briefly; the
                // persistent TrackRubberState geometry remains thereafter.
                particle.lifetimeSeconds = std::max(
                    particle.lifetimeSeconds, particle.ageSeconds + 0.30f);
            }
        }
    }

    smoothToward(m_audio.rolling, m_audioTarget.rolling, 12.0f, dt);
    smoothToward(m_audio.spray, m_audioTarget.spray, 14.0f, dt);
    smoothToward(m_audio.dust, m_audioTarget.dust, 10.0f, dt);
    smoothToward(m_audio.debris, m_audioTarget.debris, 12.0f, dt);

    const float targetDecay = std::exp(-8.0f * dt);
    m_audioTarget.rolling *= targetDecay;
    m_audioTarget.spray *= targetDecay;
    m_audioTarget.dust *= targetDecay;
    m_audioTarget.debris *= targetDecay;
    m_rutIntensity *= std::exp(-0.35f * dt);

    // Reclaim emitter state for wheels no longer producing contact samples.
    // This cache is presentation-only and must remain bounded in long sessions.
    for (auto it = m_tireFailureEmitters.begin(); it != m_tireFailureEmitters.end();)
    {
        if (it->second.lastContactTimeSeconds >= 0.0
            && m_elapsedSeconds - it->second.lastContactTimeSeconds > 30.0)
        {
            it = m_tireFailureEmitters.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

SurfacePresentationStats SurfacePresentation::stats() const
{
    SurfacePresentationStats result;
    result.activeTrackMarks = m_trackSlots.size();
    result.activeTireMarkSegments = m_tireMarkSegments.size();
    for (const SurfacePresentationParticle& particle : m_particles)
    {
        if (particle.ageSeconds < particle.lifetimeSeconds)
            ++result.activeParticles;
    }
    result.emittedParticles = m_emittedParticles;
    result.contactSamples = m_contactSamples;
    result.rutIntensity = clamp01(m_rutIntensity);
    result.audio = m_audio;
    return result;
}

} // namespace heritage::physics
