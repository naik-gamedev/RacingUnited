#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <deque>
#include <unordered_map>
#include <vector>

#include "../../../Core/Math/Math.hpp"
#include "../../CollisionSystem.hpp"

namespace heritage::physics {

enum class SurfaceParticleKind : std::uint8_t
{
    WaterSpray = 0,
    Dust,
    LooseDebris,
    Mud,
    Snow,
    RubberShred
};

struct SurfacePresentationContact
{
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    SurfaceMaterial material = SurfaceMaterial::Default;

    float deltaTimeSeconds = 0.001f;
    float forwardSpeedMps = 0.0f;
    float lateralSpeedMps = 0.0f;
    float longitudinalSlipSpeedMps = 0.0f;
    float normalLoadN = 0.0f;
    float wetness = 0.0f;
    float tireWidthM = 0.20f;

    // TIRE16 pressure-resolved tire-mark inputs. sourceStreamId identifies one
    // physical wheel/contact stream so successive high-rate samples can be
    // joined into a continuous ribbon instead of independent decal stamps.
    std::uint64_t sourceStreamId = 0;
    float slipDissipationWatts = 0.0f;
    float gripUtilization = 0.0f;
    float slipRatio = 0.0f;
    float slipAngleDegrees = 0.0f;
    float treadTemperatureC = 20.0f;
    float camberAngleRadians = 0.0f;
    float insideLoadFraction = 0.30f;
    float centerLoadFraction = 0.40f;
    float outsideLoadFraction = 0.30f;

    // Authoritative track-rubber event output. The transient particle is only
    // a visual representative of material already committed to TrackRubberState.
    // When it visually lands/expires, the persistent aggregate state remains.
    float freshRubberShed = 0.0f;
    float rubberFragmentSeverity = 0.0f;
    float tireSurfaceSpeedMps = 0.0f;

    // Authoritative deformable-surface state/delta when available. These are
    // presentation inputs only; this system never feeds forces back to tires.
    float rutDepthM = 0.0f;
    float rutDepthDeltaM = 0.0f;
    float displacedVolumeDeltaM3 = 0.0f;
    float looseDepthM = 0.0f;
};

struct SurfaceTrackMark
{
    heritage::math::DVec3 globalPosition{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    SurfaceMaterial material = SurfaceMaterial::Default;
    float widthM = 0.20f;
    float lengthM = 0.30f;
    float rutDepthM = 0.0f;
    float displacedVolumeM3 = 0.0f;
    float intensity = 0.0f;
    std::uint64_t updateSerial = 0;
};


struct SurfaceTireMarkSegment
{
    heritage::math::DVec3 startGlobalPosition{ 0.0, 0.0, 0.0 };
    heritage::math::DVec3 endGlobalPosition{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 startNormal{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 endNormal{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 startRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 endRight{ 1.0f, 0.0f, 0.0f };
    SurfaceMaterial material = SurfaceMaterial::Asphalt;
    float startWidthM = 0.20f;
    float endWidthM = 0.20f;
    float startIntensity = 0.0f;
    float endIntensity = 0.0f;
    std::array<float, 3> startLoadFractions{ 0.30f, 0.40f, 0.30f };
    std::array<float, 3> endLoadFractions{ 0.30f, 0.40f, 0.30f };
    double birthTimeSeconds = 0.0;
    std::uint64_t serial = 0;
    // TIRE16K1: append-only GPU caches need the exact predecessor so the
    // previous tail can clear its provisional end feather with one tiny flag update.
    std::uint64_t previousSegmentSerial = 0;
    bool startFeather = true;
    bool endFeather = true;
};

struct SurfacePresentationParticle
{
    heritage::math::DVec3 globalPosition{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 velocityMps{ 0.0f, 0.0f, 0.0f };
    SurfaceParticleKind kind = SurfaceParticleKind::LooseDebris;
    SurfaceMaterial material = SurfaceMaterial::Default;
    float ageSeconds = 0.0f;
    float lifetimeSeconds = 1.0f;
    float sizeM = 0.04f;
    float opacity = 1.0f;
    std::uint32_t seed = 0;

    // Rubber shreds can visibly complete a short ballistic arc and settle on
    // the support plane before their transient representative expires.
    heritage::math::DVec3 supportPlanePoint{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 supportPlaneNormal{ 0.0f, 1.0f, 0.0f };
    bool settlesOnSupportPlane = false;
    bool settled = false;
};

struct SurfacePresentationAudioMix
{
    // Bounded 0..1 mechanism intensities. TIRE15B2 deliberately does not
    // ship fake placeholder samples; module/SDK audio can consume these values
    // to choose authored rolling/spray/dust/debris loops later.
    float rolling = 0.0f;
    float spray = 0.0f;
    float dust = 0.0f;
    float debris = 0.0f;
};

struct SurfacePresentationStats
{
    std::size_t activeTrackMarks = 0;
    std::size_t activeTireMarkSegments = 0;
    std::size_t activeParticles = 0;
    std::uint64_t emittedParticles = 0;
    std::uint64_t contactSamples = 0;
    float rutIntensity = 0.0f;
    SurfacePresentationAudioMix audio{};
};

// TIRE15B2 bounded presentation cache. Physics remains authoritative in
// SurfaceWorld/SurfaceField; this cache converts wheel contact state into
// render/audio-friendly track marks and transient particles. It is intentionally
// lossy and bounded, and changing/clearing it cannot change vehicle physics.
class SurfacePresentation
{
public:
    static constexpr std::size_t kMaximumTrackMarks = 8192;
    // TIRE16K production history policy. Up to one million authoritative FP64
    // segments are retained. The renderer consumes new serials incrementally
    // into invisible 100 m GPU-cache chunks, so old history no longer needs to
    // be scanned or retessellated every frame. Time only fades opacity; the
    // uniform strip remains strictly a distance LOD.
    static constexpr std::size_t kMaximumTireMarkSegments = 1000000;
    static constexpr double kTireMarkRetirementSeconds = 1200.0;
    static constexpr std::size_t kMaximumParticles = 2048;

    void clear();
    void advance(float deltaTimeSeconds);

    void recordContact(
        const heritage::math::DVec3& globalPosition,
        const SurfacePresentationContact& contact);

    const std::vector<SurfaceTrackMark>& trackMarks() const { return m_trackMarks; }
    const std::deque<SurfaceTireMarkSegment>& tireMarkSegments() const { return m_tireMarkSegments; }
    std::uint64_t firstTireMarkSerial() const
    {
        return m_tireMarkSegments.empty() ? 0 : m_tireMarkSegments.front().serial;
    }
    std::uint64_t lastTireMarkSerial() const
    {
        return m_tireMarkSegments.empty() ? 0 : m_tireMarkSegments.back().serial;
    }
    const SurfaceTireMarkSegment* tireMarkSegmentBySerial(std::uint64_t serial) const
    {
        if (m_tireMarkSegments.empty() || serial == 0)
            return nullptr;
        const std::uint64_t first = m_tireMarkSegments.front().serial;
        const std::uint64_t last = m_tireMarkSegments.back().serial;
        if (serial < first || serial > last)
            return nullptr;
        const std::size_t index = static_cast<std::size_t>(serial - first);
        if (index >= m_tireMarkSegments.size())
            return nullptr;
        const SurfaceTireMarkSegment& candidate = m_tireMarkSegments[index];
        return candidate.serial == serial ? &candidate : nullptr;
    }
    const std::vector<SurfacePresentationParticle>& particles() const { return m_particles; }
    double elapsedSeconds() const { return m_elapsedSeconds; }
    SurfacePresentationStats stats() const;

private:
    struct TrackKey
    {
        std::int64_t x = 0;
        std::int64_t z = 0;
        std::int64_t verticalLayer = 0;
        std::uint8_t material = 0;

        bool operator==(const TrackKey& other) const
        {
            return x == other.x
                && z == other.z
                && verticalLayer == other.verticalLayer
                && material == other.material;
        }
    };

    struct TrackKeyHash
    {
        std::size_t operator()(const TrackKey& key) const;
    };

    struct TrackSlot
    {
        TrackKey key{};
        SurfaceTrackMark mark{};
        float particleAccumulator = 0.0f;
    };

    struct TireMarkSample
    {
        bool valid = false;
        heritage::math::DVec3 globalPosition{ 0.0, 0.0, 0.0 };
        heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
        heritage::math::Vec3 right{ 1.0f, 0.0f, 0.0f };
        SurfaceMaterial material = SurfaceMaterial::Asphalt;
        float widthM = 0.20f;
        float intensity = 0.0f;
        std::array<float, 3> loadFractions{ 0.30f, 0.40f, 0.30f };
    };

    struct TireMarkTrail
    {
        TireMarkSample anchor{};
        std::size_t lastSegmentIndex = 0;
        std::uint64_t lastSegmentSerial = 0;
        float stationarySeconds = 0.0f;
        double lastContactTimeSeconds = -1.0;
    };

    TrackKey trackKey(
        const heritage::math::DVec3& globalPosition,
        SurfaceMaterial material) const;
    TrackSlot& acquireTrackSlot(const TrackKey& key);
    void recordTireMark(
        const heritage::math::DVec3& globalPosition,
        const SurfacePresentationContact& contact);
    std::size_t appendTireMarkSegment(
        const TireMarkSample& start,
        const TireMarkSample& end,
        bool startFeather,
        bool endFeather,
        std::uint64_t previousSegmentSerial,
        std::uint64_t& serialOut);
    void emitParticle(
        const heritage::math::DVec3& globalPosition,
        const SurfacePresentationContact& contact,
        SurfaceParticleKind kind,
        float intensity,
        std::uint32_t sequence);
    void accumulateAudio(
        const SurfacePresentationContact& contact,
        float sprayIntensity,
        float dustIntensity,
        float debrisIntensity);

    std::vector<TrackSlot> m_trackSlots;
    std::unordered_map<TrackKey, std::size_t, TrackKeyHash> m_trackLookup;
    std::size_t m_nextTrackReplacement = 0;

    std::vector<SurfaceTrackMark> m_trackMarks;

    std::deque<SurfaceTireMarkSegment> m_tireMarkSegments;
    std::unordered_map<std::uint64_t, TireMarkTrail> m_tireMarkTrails;
    std::size_t m_nextTireMarkReplacement = 0;
    std::uint64_t m_nextTireMarkSerial = 1;

    std::vector<SurfacePresentationParticle> m_particles;
    std::size_t m_nextParticleReplacement = 0;

    SurfacePresentationAudioMix m_audio{};
    SurfacePresentationAudioMix m_audioTarget{};
    std::uint64_t m_updateSerial = 0;
    std::uint64_t m_emittedParticles = 0;
    std::uint64_t m_contactSamples = 0;
    float m_rutIntensity = 0.0f;
    double m_elapsedSeconds = 0.0;
};

} // namespace heritage::physics
