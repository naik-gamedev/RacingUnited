#pragma once

#include "SurfaceField.hpp"
#include "SurfaceMaterialProperties.hpp"
#include "Presentation/SurfacePresentation.hpp"
#include "Rubber/TrackRubberState.hpp"

namespace heritage::physics {

struct SurfaceWorldEnvironment
{
    // Global rain/water contribution. Authored wetness remains a local base;
    // the effective contact wetness combines both without exceeding one.
    double wetness = 0.0;

    // Air temperature is always available to tire thermal state. A separate
    // optional road-temperature override lets weather/day-night systems drive
    // the road without erasing a scene-authored local temperature when no
    // override is active.
    double ambientTemperatureC = 20.0;
    bool surfaceTemperatureOverrideEnabled = false;
    double surfaceTemperatureC = 20.0;
};


struct SurfaceWorldDevelopmentControls
{
    // Development/lab acceleration only. These are never serialized into a
    // vehicle or tire part and default to physically timed 1x behavior.
    double tireWearRateMultiplier = 1.0;
    double rubberGenerationMultiplier = 1.0;
    double marbleMaturationMultiplier = 1.0;
};

struct SurfaceLocalConditions
{
    double wetness = 0.0;
    double ambientTemperatureC = 20.0;
    double surfaceTemperatureC = 20.0;
};

// World-owned authoritative dynamic surface state. Vehicle/tire simulation
// consumes this service; it does not own it. SurfaceWorld is also the single
// local-FP32 -> global-FP64 addressing boundary, keeping driven-surface state
// stable when PhysicsWorld rebases its floating origin.
//
// Deformable terrain and track rubber are distinct world-owned layers.
// TrackRubberState remains specialized under Physics/Surfaces/Rubber rather
// than pretending loose tire marbles are generic terrain deformation.
class SurfaceWorld
{
public:
    SurfaceWorld() = default;

    void clear();

    void setGlobalOrigin(const heritage::math::DVec3& globalOrigin);
    const heritage::math::DVec3& globalOrigin() const { return m_globalOrigin; }
    heritage::math::DVec3 localToGlobal(
        const heritage::math::Vec3& localPosition) const;

    bool setEnvironment(const SurfaceWorldEnvironment& environment);
    const SurfaceWorldEnvironment& environment() const { return m_environment; }

    bool setDevelopmentControls(const SurfaceWorldDevelopmentControls& controls);
    const SurfaceWorldDevelopmentControls& developmentControls() const
    {
        return m_developmentControls;
    }
    void resetDevelopmentControls() { m_developmentControls = {}; }
    void clearTrackRubber() { m_trackRubber.clear(); }

    SurfaceLocalConditions localConditions(
        const heritage::math::Vec3& localPosition,
        SurfaceMaterial material,
        double authoredWetness,
        const SurfaceMaterialProperties& properties) const;

    SurfaceField& deformableTerrain() { return m_deformableTerrain; }
    const SurfaceField& deformableTerrain() const { return m_deformableTerrain; }

    SurfacePresentation& presentation() { return m_presentation; }
    const SurfacePresentation& presentation() const { return m_presentation; }

    rubber::TrackRubberState& trackRubber() { return m_trackRubber; }
    const rubber::TrackRubberState& trackRubber() const { return m_trackRubber; }

    rubber::TrackRubberSample sampleTrackRubber(
        const heritage::math::Vec3& localPosition,
        SurfaceMaterial material,
        float localWetness) const;
    rubber::TrackRubberSample applyTrackRubberContact(
        const heritage::math::Vec3& localPosition,
        const rubber::TrackRubberContactInput& input);
    rubber::TrackRubberWakeResult applyTrackRubberWake(
        const heritage::math::Vec3& localPosition,
        const rubber::TrackRubberWakeInput& input);

    void advancePresentation(float deltaTimeSeconds);
    void recordContactPresentation(
        const heritage::math::Vec3& localPosition,
        const SurfacePresentationContact& contact);

    SurfaceFieldSample sampleDeformable(
        const heritage::math::Vec3& localPosition,
        SurfaceMaterial material,
        const SurfaceFieldInitialState& initialState) const;

    SurfaceFieldSample applyDeformable(
        const heritage::math::Vec3& localPosition,
        const SurfaceFieldUpdate& update);

private:
    heritage::math::DVec3 m_globalOrigin{ 0.0, 0.0, 0.0 };
    SurfaceWorldEnvironment m_environment{};
    SurfaceWorldDevelopmentControls m_developmentControls{};
    SurfaceField m_deformableTerrain;
    rubber::TrackRubberState m_trackRubber;
    SurfacePresentation m_presentation;
};

} // namespace heritage::physics
