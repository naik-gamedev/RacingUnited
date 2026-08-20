#pragma once

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "SurfaceField.hpp"
#include "SurfaceMaterialProperties.hpp"
#include "SurfaceWeather.hpp"
#include "DynamicSurface/DynamicSurfaceSystem.hpp"
#include "Presentation/SurfacePresentation.hpp"
#include "Rubber/TrackRubberState.hpp"
#include "Water/SurfaceHydrology.hpp"
#include "../Weather/PrecipitationField.hpp"

namespace heritage::jobs { class JobSystem; }

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


struct GpuDynamicSurfaceTireEvent
{
    heritage::math::DVec3 globalPosition{};
    float patchLengthM = 0.0f;
    float patchWidthM = 0.0f;
    float forwardX = 0.0f;
    float forwardZ = 1.0f;
    float rightX = 1.0f;
    float rightZ = 0.0f;
    float normalLoadN = 0.0f;
    float speedMps = 0.0f;
    float accumulatedDtSeconds = 0.0f;
    bool mudDeformable = false;
};

struct SurfaceLocalConditions
{
    double wetness = 0.0;
    double weatherWetness = 0.0;
    bool waterFilmDepthValid = false;
    double waterFilmDepthM = 0.0;
    double ambientAirSpeedMps = 0.0;
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

    void setJobSystem(heritage::jobs::JobSystem* jobs);

    void clear();

    void setGlobalOrigin(const heritage::math::DVec3& globalOrigin);
    const heritage::math::DVec3& globalOrigin() const { return m_globalOrigin; }
    heritage::math::DVec3 localToGlobal(
        const heritage::math::Vec3& localPosition) const;

    bool setEnvironment(const SurfaceWorldEnvironment& environment);
    const SurfaceWorldEnvironment& environment() const { return m_environment; }

    bool setWeather(const SurfaceWeatherDescription& weather);
    const SurfaceWeatherDescription& weather() const { return m_weather; }
    const SurfaceWeatherState& weatherState() const { return m_weatherState; }
    SurfaceWeatherOutput weatherOutput() const;
    void resetWeatherState();

    const weather::PrecipitationField& precipitation() const
    {
        return m_precipitation;
    }

    bool loadOrBakeHydrology(
        const std::vector<StaticSceneTriangle>& localTriangles,
        const std::filesystem::path& cachePath,
        water::SurfaceHydrologyBakeReport& report);
    bool loadOrBakeDynamicSurface(
        const std::vector<StaticSceneTriangle>& localTriangles,
        const std::filesystem::path& cachePath,
        dynamicsurface::DynamicSurfaceStaticBakeReport& report);
    void clearHydrology()
    {
        m_hydrology.clear();
        m_dynamicSurface.clearHydroState();
    }
    void resetHydrologyWater()
    {
        m_hydrology.resetWater();
        m_dynamicSurface.resetHydroWater();
    }
    water::SurfaceHydrology& hydrology() { return m_hydrology; }
    const water::SurfaceHydrology& hydrology() const { return m_hydrology; }
    void clearHydrologyInterestSources()
    {
        m_hydrology.clearInterestSources();
        m_dynamicSurface.setInterestSources({});
    }
    void setHydrologyInterestSource(const heritage::math::DVec3& source)
    {
        m_hydrology.setInterestSource(source);
        m_dynamicSurface.setInterestSources({ source });
    }
    void setHydrologyInterestSources(
        const std::vector<heritage::math::DVec3>& sources)
    {
        m_hydrology.setInterestSources(sources);
        m_dynamicSurface.setInterestSources(sources);
    }
    water::SurfaceHydrologyTireResult applyHydrologyTireContact(
        const heritage::math::Vec3& localPosition,
        const water::SurfaceHydrologyTireInput& input);

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

    dynamicsurface::DynamicSurfaceSystem& dynamicSurface() { return m_dynamicSurface; }
    const dynamicsurface::DynamicSurfaceSystem& dynamicSurface() const { return m_dynamicSurface; }

    // DSURF04F: the renderer-owned compute field is the single water authority
    // while active. The old CPU Hydro state remains compiled for regression and
    // static support/sheet infrastructure, but it is not advanced in live play.
    void setGpuDynamicSurfaceAuthorityEnabled(bool enabled)
    {
        m_gpuDynamicSurfaceAuthorityEnabled = enabled;
    }
    bool gpuDynamicSurfaceAuthorityEnabled() const
    {
        return m_gpuDynamicSurfaceAuthorityEnabled;
    }
    void consumeGpuDynamicSurfaceTireEvents(
        std::vector<GpuDynamicSurfaceTireEvent>& outEvents);

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
    heritage::jobs::JobSystem* m_jobSystem = nullptr;
    heritage::math::DVec3 m_globalOrigin{ 0.0, 0.0, 0.0 };
    SurfaceWorldEnvironment m_environment{};
    SurfaceWeatherDescription m_weather{};
    SurfaceWeatherState m_weatherState{};
    weather::PrecipitationField m_precipitation;
    water::SurfaceHydrology m_hydrology;
    dynamicsurface::DynamicSurfaceSystem m_dynamicSurface;
    bool m_gpuDynamicSurfaceAuthorityEnabled = false;
    std::vector<GpuDynamicSurfaceTireEvent> m_gpuDynamicSurfaceTireEvents;
    std::unordered_map<std::uint64_t, std::size_t> m_gpuDynamicSurfaceTireEventByCell;
    SurfaceWorldDevelopmentControls m_developmentControls{};
    SurfaceField m_deformableTerrain;
    rubber::TrackRubberState m_trackRubber;
    SurfacePresentation m_presentation;
};

} // namespace heritage::physics
