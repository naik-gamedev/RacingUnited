#include "SurfaceWorld.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::physics {

void SurfaceWorld::setJobSystem(heritage::jobs::JobSystem* jobs)
{
    m_jobSystem = jobs;
    m_hydrology.setJobSystem(jobs);
}

void SurfaceWorld::clear()
{
    m_environment = {};
    m_weather = {};
    m_weatherState = {};
    m_precipitation.clear();
    m_hydrology.clear();
    m_dynamicSurface.clear();
    m_gpuDynamicSurfaceAuthorityEnabled = false;
    m_gpuDynamicSurfaceTireEvents.clear();
    m_gpuDynamicSurfaceTireEventByCell.clear();
    m_developmentControls = {};
    m_deformableTerrain.clear();
    m_trackRubber.clear();
    m_presentation.clear();
}

void SurfaceWorld::setGlobalOrigin(
    const heritage::math::DVec3& globalOrigin)
{
    if (!std::isfinite(globalOrigin.x)
        || !std::isfinite(globalOrigin.y)
        || !std::isfinite(globalOrigin.z))
    {
        return;
    }
    m_globalOrigin = globalOrigin;
}

heritage::math::DVec3 SurfaceWorld::localToGlobal(
    const heritage::math::Vec3& localPosition) const
{
    return {
        m_globalOrigin.x + static_cast<double>(localPosition.x),
        m_globalOrigin.y + static_cast<double>(localPosition.y),
        m_globalOrigin.z + static_cast<double>(localPosition.z)
    };
}

bool SurfaceWorld::setEnvironment(const SurfaceWorldEnvironment& environment)
{
    if (!std::isfinite(environment.wetness)
        || environment.wetness < 0.0 || environment.wetness > 1.0
        || !std::isfinite(environment.ambientTemperatureC)
        || environment.ambientTemperatureC < -100.0
        || environment.ambientTemperatureC > 100.0
        || !std::isfinite(environment.surfaceTemperatureC)
        || environment.surfaceTemperatureC < -100.0
        || environment.surfaceTemperatureC > 150.0)
    {
        return false;
    }

    m_environment = environment;
    return true;
}

bool SurfaceWorld::setWeather(const SurfaceWeatherDescription& weather)
{
    if (!validSurfaceWeatherDescription(weather))
        return false;
    m_weather = weather;
    m_precipitation.configureRain(
        weather.enabled ? weather.precipitationRateMmPerHour : 0.0,
        weather.windSpeedMps,
        weather.windDirectionDegrees);
    return true;
}

SurfaceWeatherOutput SurfaceWorld::weatherOutput() const
{
    return evaluateSurfaceWeather(m_weather, m_weatherState);
}

void SurfaceWorld::resetWeatherState()
{
    m_weatherState = {};
    // Resetting accumulated road water must not rewind the world precipitation
    // field or make every visible drop jump back to its t=0 phase.
    m_hydrology.resetWater();
    m_dynamicSurface.resetHydroWater();
    m_dynamicSurface.resetThermalState();
}

bool SurfaceWorld::loadOrBakeHydrology(
    const std::vector<StaticSceneTriangle>& localTriangles,
    const std::filesystem::path& cachePath,
    water::SurfaceHydrologyBakeReport& report)
{
    return m_hydrology.loadOrBake(
        localTriangles, m_globalOrigin, cachePath, report);
}

bool SurfaceWorld::loadOrBakeDynamicSurface(
    const std::vector<StaticSceneTriangle>& localTriangles,
    const std::filesystem::path& cachePath,
    dynamicsurface::DynamicSurfaceStaticBakeReport& report)
{
    return m_dynamicSurface.loadOrBakeStaticScene(
        localTriangles, m_globalOrigin, cachePath, report);
}

water::SurfaceHydrologyTireResult SurfaceWorld::applyHydrologyTireContact(
    const heritage::math::Vec3& localPosition,
    const water::SurfaceHydrologyTireInput& input)
{
    // DSURF04F: when GPU authority is live, do not mutate the retired CPU Hydro
    // lattice. Tire physics temporarily consumes the smooth weather-film depth
    // until the tiny asynchronous GPU contact-sample bridge is promoted; this
    // preserves the one-water-authority rule instead of running two solvers.
    if (m_gpuDynamicSurfaceAuthorityEnabled)
    {
        const heritage::math::DVec3 globalPosition = localToGlobal(localPosition);
        // Aggregate 1000Hz tire contacts into 10cm world cells until the render
        // thread consumes them. This keeps the GPU contact path bounded while
        // preserving exact world anchoring and accumulated contact time.
        const std::int32_t qx = static_cast<std::int32_t>(std::floor(globalPosition.x * 10.0));
        const std::int32_t qz = static_cast<std::int32_t>(std::floor(globalPosition.z * 10.0));
        const std::uint64_t eventKey =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(qx)) << 32u)
            | static_cast<std::uint32_t>(qz);
        auto existing = m_gpuDynamicSurfaceTireEventByCell.find(eventKey);
        if (existing == m_gpuDynamicSurfaceTireEventByCell.end())
        {
            if (m_gpuDynamicSurfaceTireEvents.size() < 1024u)
            {
                GpuDynamicSurfaceTireEvent event;
                event.globalPosition = globalPosition;
                event.patchLengthM = static_cast<float>(std::max(input.contactPatchLengthM, 0.02));
                event.patchWidthM = static_cast<float>(std::max(input.contactPatchWidthM, 0.02));
                event.forwardX = input.forward.x;
                event.forwardZ = input.forward.z;
                event.rightX = input.right.x;
                event.rightZ = input.right.z;
                event.normalLoadN = static_cast<float>(std::max(input.normalLoadN, 0.0));
                event.speedMps = static_cast<float>(std::hypot(input.forwardSpeedMps, input.lateralSpeedMps));
                event.accumulatedDtSeconds = static_cast<float>(std::max(input.deltaTimeSeconds, 0.0));
                event.mudDeformable = input.surfaceMaterial == SurfaceMaterial::Mud
                    || input.surfaceMaterial == SurfaceMaterial::SoftSoil;
                const std::size_t index = m_gpuDynamicSurfaceTireEvents.size();
                m_gpuDynamicSurfaceTireEvents.push_back(event);
                m_gpuDynamicSurfaceTireEventByCell.emplace(eventKey, index);
            }
        }
        else
        {
            auto& event = m_gpuDynamicSurfaceTireEvents[existing->second];
            event.globalPosition = globalPosition;
            event.patchLengthM = std::max(event.patchLengthM, static_cast<float>(input.contactPatchLengthM));
            event.patchWidthM = std::max(event.patchWidthM, static_cast<float>(input.contactPatchWidthM));
            event.forwardX = input.forward.x;
            event.forwardZ = input.forward.z;
            event.rightX = input.right.x;
            event.rightZ = input.right.z;
            event.normalLoadN = std::max(event.normalLoadN, static_cast<float>(input.normalLoadN));
            event.speedMps = std::max(event.speedMps, static_cast<float>(std::hypot(input.forwardSpeedMps, input.lateralSpeedMps)));
            event.accumulatedDtSeconds += static_cast<float>(std::max(input.deltaTimeSeconds, 0.0));
            event.mudDeformable = event.mudDeformable
                || input.surfaceMaterial == SurfaceMaterial::Mud
                || input.surfaceMaterial == SurfaceMaterial::SoftSoil;
        }

        const auto weather = weatherOutput();
        water::SurfaceHydrologyTireResult result;
        result.valid = weather.valid;
        result.initialWaterDepthM = weather.valid
            ? std::max(weather.waterFilmDepthM, 0.0) : 0.0;
        result.finalWaterDepthM = result.initialWaterDepthM;

        dynamicsurface::DynamicSurfaceThermalTireInput thermalInput;
        thermalInput.deltaTimeSeconds = input.deltaTimeSeconds;
        thermalInput.contactPatchAreaM2 = input.contactPatchAreaM2;
        thermalInput.slipDissipationWatts = input.slipDissipationWatts;
        m_dynamicSurface.applyThermalTireContact(
            localToGlobal(localPosition), thermalInput);
        return result;
    }

    dynamicsurface::DynamicSurfaceHydroTireInput migrated;
    migrated.deltaTimeSeconds = input.deltaTimeSeconds;
    migrated.contactPatchLengthM = input.contactPatchLengthM;
    migrated.contactPatchWidthM = input.contactPatchWidthM;
    migrated.contactPatchAreaM2 = input.contactPatchAreaM2;
    migrated.normalLoadN = input.normalLoadN;
    migrated.nominalLoadN = input.nominalLoadN;
    migrated.forwardSpeedMps = input.forwardSpeedMps;
    migrated.lateralSpeedMps = input.lateralSpeedMps;
    migrated.treadVoidRatio = input.treadVoidRatio;
    migrated.slipDissipationWatts = input.slipDissipationWatts;
    migrated.forward = input.forward;
    migrated.right = input.right;

    const auto dynamicResult = m_dynamicSurface.applyHydroTireContact(
        localToGlobal(localPosition), migrated);

    dynamicsurface::DynamicSurfaceThermalTireInput thermalInput;
    thermalInput.deltaTimeSeconds = input.deltaTimeSeconds;
    thermalInput.contactPatchAreaM2 = input.contactPatchAreaM2;
    thermalInput.slipDissipationWatts = input.slipDissipationWatts;
    m_dynamicSurface.applyThermalTireContact(
        localToGlobal(localPosition), thermalInput);

    water::SurfaceHydrologyTireResult result;
    result.valid = dynamicResult.valid;
    result.initialWaterDepthM = dynamicResult.initialWaterDepthM;
    result.finalWaterDepthM = dynamicResult.finalWaterDepthM;
    result.removedVolumeM3 = dynamicResult.removedVolumeM3;
    result.redistributedVolumeM3 = dynamicResult.redistributedVolumeM3;
    result.sprayVolumeM3 = dynamicResult.sprayVolumeM3;
    result.frictionEvaporatedVolumeM3 = dynamicResult.frictionEvaporatedVolumeM3;
    return result;
}


void SurfaceWorld::consumeGpuDynamicSurfaceTireEvents(
    std::vector<GpuDynamicSurfaceTireEvent>& outEvents)
{
    outEvents.clear();
    outEvents.swap(m_gpuDynamicSurfaceTireEvents);
    m_gpuDynamicSurfaceTireEventByCell.clear();
}

bool SurfaceWorld::setDevelopmentControls(
    const SurfaceWorldDevelopmentControls& controls)
{
    const auto validMultiplier = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 1000.0;
    };
    if (!validMultiplier(controls.tireWearRateMultiplier)
        || !validMultiplier(controls.rubberGenerationMultiplier)
        || !validMultiplier(controls.marbleMaturationMultiplier))
    {
        return false;
    }
    m_developmentControls = controls;
    return true;
}

SurfaceLocalConditions SurfaceWorld::localConditions(
    const heritage::math::Vec3& localPosition,
    SurfaceMaterial material,
    double authoredWetness,
    const SurfaceMaterialProperties& properties) const
{
    SurfaceLocalConditions result;
    const double baseWetness = std::clamp(
        std::isfinite(authoredWetness) ? authoredWetness : 0.0,
        0.0,
        1.0);
    const SurfaceWeatherOutput dynamicWeather = weatherOutput();
    const dynamicsurface::DynamicSurfaceHydroSample hydrology =
        m_gpuDynamicSurfaceAuthorityEnabled
            ? dynamicsurface::DynamicSurfaceHydroSample{}
            : m_dynamicSurface.sampleHydro(localToGlobal(localPosition));
    const dynamicsurface::DynamicSurfaceThermalSample thermal =
        m_dynamicSurface.sampleThermal(localToGlobal(localPosition));
    const bool spatialWaterValid = hydrology.valid && dynamicWeather.valid;
    const double dynamicWetness = spatialWaterValid
        ? hydrology.wetness
        : (dynamicWeather.valid ? dynamicWeather.effectiveWetness : 0.0);
    const double weatherWetness = 1.0
        - (1.0 - std::clamp(m_environment.wetness, 0.0, 1.0))
            * (1.0 - dynamicWetness);
    // Union of two [0,1] coverages: pre-wet authored road remains wet while
    // global rain can wet an otherwise dry scene surface.
    result.wetness = 1.0 - (1.0 - baseWetness) * (1.0 - weatherWetness);
    result.weatherWetness = dynamicWetness;
    result.waterFilmDepthValid = dynamicWeather.valid;
    result.ambientAirSpeedMps = dynamicWeather.valid
        ? dynamicWeather.windSpeedMps : 0.0;
    switch (material)
    {
    case SurfaceMaterial::Default:
    case SurfaceMaterial::Asphalt:
    case SurfaceMaterial::Kerb:
    case SurfaceMaterial::PaintedLine:
        result.waterFilmDepthM = spatialWaterValid
            ? hydrology.waterDepthM
            : (dynamicWeather.valid ? dynamicWeather.waterFilmDepthM : 0.0);
        break;
    default:
        result.waterFilmDepthM = 0.0;
        break;
    }
    result.ambientTemperatureC = m_environment.ambientTemperatureC;

    if (m_environment.surfaceTemperatureOverrideEnabled)
    {
        result.surfaceTemperatureC = m_environment.surfaceTemperatureC;
    }
    else if (thermal.valid)
    {
        result.surfaceTemperatureC = thermal.surfaceTemperatureC;
    }
    else if (dynamicWeather.valid)
    {
        result.surfaceTemperatureC = dynamicWeather.roadTemperatureC;
    }
    else if (properties.hasAuthoredSurfaceTemperature
        && std::isfinite(properties.authoredSurfaceTemperatureC))
    {
        result.surfaceTemperatureC = properties.authoredSurfaceTemperatureC;
    }
    else
    {
        result.surfaceTemperatureC = defaultSurfaceTemperatureC(material);
    }
    return result;
}

rubber::TrackRubberSample SurfaceWorld::sampleTrackRubber(
    const heritage::math::Vec3& localPosition,
    SurfaceMaterial material,
    float localWetness) const
{
    return m_trackRubber.sample(localToGlobal(localPosition), material, localWetness);
}

rubber::TrackRubberSample SurfaceWorld::applyTrackRubberContact(
    const heritage::math::Vec3& localPosition,
    const rubber::TrackRubberContactInput& input)
{
    return m_trackRubber.applyContact(localToGlobal(localPosition), input);
}

rubber::TrackRubberWakeResult SurfaceWorld::applyTrackRubberWake(
    const heritage::math::Vec3& localPosition,
    const rubber::TrackRubberWakeInput& input)
{
    return m_trackRubber.applyWake(localToGlobal(localPosition), input);
}

void SurfaceWorld::advancePresentation(float deltaTimeSeconds)
{
    // TIRE15C rubber physics ages lazily from this global clock/exposure. The
    // method name is retained for compatibility with the existing runtime; the
    // authoritative rubber state remains independent from visual presentation.
    if (m_weather.enabled)
    {
        const double initialRoadTemperatureC =
            m_environment.surfaceTemperatureOverrideEnabled
                ? m_environment.surfaceTemperatureC
                : m_environment.ambientTemperatureC;
        advanceSurfaceWeather(
            m_weather,
            m_environment.ambientTemperatureC,
            initialRoadTemperatureC,
            static_cast<double>(deltaTimeSeconds),
            m_weatherState);
    }
    m_precipitation.advance(static_cast<double>(deltaTimeSeconds));
    const SurfaceWeatherOutput dynamicWeather = weatherOutput();
    // DSURF04B: select the bounded nearest-real-surface working set before
    // advancing Hydro/Track. Simulation therefore follows residency instead
    // of materializing every covered page inside a broad distance band.
    m_dynamicSurface.refreshHydroResidency();
    // DSURF03B: Heritage Dynamic Surface is now the authoritative runtime
    // water/moisture/flow state. The old SurfaceHydrology object is retained
    // temporarily only for static precipitation-cover queries and historical
    // diagnostics while its remaining non-state responsibilities are migrated.
    if (!m_gpuDynamicSurfaceAuthorityEnabled)
    {
        m_dynamicSurface.advanceHydro(
            m_weather, dynamicWeather, static_cast<double>(deltaTimeSeconds));
    }
    // DSURF04: persistent Track-plane temperature is advanced from the same
    // real simulation-interest sources and surface sheets as Hydro. The
    // weather scalar remains compatibility telemetry/fallback only.
    m_dynamicSurface.advanceThermal(
        m_weather,
        dynamicWeather,
        m_environment.ambientTemperatureC,
        m_environment.surfaceTemperatureOverrideEnabled,
        m_environment.surfaceTemperatureC,
        static_cast<double>(deltaTimeSeconds));
    const double effectiveWetness = 1.0
        - (1.0 - std::clamp(m_environment.wetness, 0.0, 1.0))
            * (1.0 - (dynamicWeather.valid
                ? dynamicWeather.effectiveWetness : 0.0));
    m_trackRubber.advance(deltaTimeSeconds, static_cast<float>(effectiveWetness));
    m_presentation.advance(deltaTimeSeconds);
}

void SurfaceWorld::recordContactPresentation(
    const heritage::math::Vec3& localPosition,
    const SurfacePresentationContact& contact)
{
    m_presentation.recordContact(localToGlobal(localPosition), contact);
}

SurfaceFieldSample SurfaceWorld::sampleDeformable(
    const heritage::math::Vec3& localPosition,
    SurfaceMaterial material,
    const SurfaceFieldInitialState& initialState) const
{
    return m_deformableTerrain.sample(
        localToGlobal(localPosition), material, initialState);
}

SurfaceFieldSample SurfaceWorld::applyDeformable(
    const heritage::math::Vec3& localPosition,
    const SurfaceFieldUpdate& update)
{
    return m_deformableTerrain.apply(localToGlobal(localPosition), update);
}

} // namespace heritage::physics
