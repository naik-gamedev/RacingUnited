#include "../../LuaModuleRuntime.hpp"
#include "LuaPhysicsBindingHandlers.hpp"
#include "../LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../../../../Physics/PhysicsWorld.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaPhysicsBindingHandlers::luaPhysicsIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetFixedDelta(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->fixedDeltaTime())
            : 0.0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetTickRate(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->tickRate())
            : 0.0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetTickRate(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const float hertz = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 120.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->setTickRate(hertz);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetGravity(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 gravity = runtime->m_physics
        ? runtime->m_physics->gravity()
        : heritage::math::Vec3{ 0.0f, 0.0f, 0.0f };
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(gravity.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(gravity.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(gravity.z));
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetGravity(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (!runtime->m_physics)
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_physics->setGravity({
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, -9.80665)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0))
    });
    runtime->m_api.lua_pushboolean(
        state,
        runtime->m_physics->lastError().empty() ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetSurfaceEnvironment(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::physics::SurfaceWorldEnvironment environment =
        runtime->m_physics
            ? runtime->m_physics->surfaces().environment()
            : heritage::physics::SurfaceWorldEnvironment{};
    runtime->m_api.lua_pushnumber(
        state, static_cast<LuaNumber>(environment.wetness));
    runtime->m_api.lua_pushnumber(
        state, static_cast<LuaNumber>(environment.ambientTemperatureC));
    runtime->m_api.lua_pushnumber(
        state, static_cast<LuaNumber>(environment.surfaceTemperatureC));
    runtime->m_api.lua_pushboolean(
        state, environment.surfaceTemperatureOverrideEnabled ? 1 : 0);
    return 4;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetSurfaceWeather(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::physics::SurfaceWeatherDescription description =
        runtime->m_physics
            ? runtime->m_physics->surfaces().weather()
            : heritage::physics::SurfaceWeatherDescription{};
    const heritage::physics::SurfaceWeatherState weatherState =
        runtime->m_physics
            ? runtime->m_physics->surfaces().weatherState()
            : heritage::physics::SurfaceWeatherState{};
    const heritage::physics::SurfaceWeatherOutput output =
        runtime->m_physics
            ? runtime->m_physics->surfaces().weatherOutput()
            : heritage::physics::SurfaceWeatherOutput{};

    const auto precipitation = runtime->m_physics
        ? runtime->m_physics->surfaces().precipitation().rainPopulation()
        : heritage::physics::weather::RainDropPopulation{};

    runtime->m_api.lua_createtable(state, 0, 36);
    const auto setNumber = [&](const char* name, double value) {
        runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
        runtime->m_api.lua_setfield(state, -2, name);
    };
    const auto setBoolean = [&](const char* name, bool value) {
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
        runtime->m_api.lua_setfield(state, -2, name);
    };
    setBoolean("enabled", description.enabled);
    setBoolean("initialized", weatherState.initialized);
    setBoolean("valid", output.valid);
    setNumber("rain_mm_per_hour", description.precipitationRateMmPerHour);
    setNumber("relative_humidity", description.relativeHumidity);
    setNumber("wind_mps", description.windSpeedMps);
    setNumber("wind_direction_deg", description.windDirectionDegrees);
    setNumber("wind_x_mps", output.windVelocityXMps);
    setNumber("wind_z_mps", output.windVelocityZMps);
    setNumber("cloud_cover", description.cloudCover);
    setNumber("drainage_capacity_mm_per_hour", description.drainageRateMmPerHour);
    setNumber("evaporation_reference_mm_per_hour",
        description.referenceEvaporationRateMmPerHour);
    setNumber("maximum_water_film_mm",
        description.maximumWaterFilmDepthM * 1000.0);
    setNumber("water_film_mm", output.waterFilmDepthM * 1000.0);
    setNumber("effective_wetness", output.effectiveWetness);
    setNumber("road_temperature_c", output.roadTemperatureC);
    setNumber("current_drainage_mm_per_hour", output.drainageRateMmPerHour);
    setNumber("current_evaporation_mm_per_hour", output.evaporationRateMmPerHour);
    setNumber("elapsed_seconds", weatherState.elapsedSeconds);
    setNumber("cumulative_rain_mm", weatherState.cumulativePrecipitationMm);
    setNumber("cumulative_drainage_mm", weatherState.cumulativeDrainageMm);
    setNumber("cumulative_evaporation_mm", weatherState.cumulativeEvaporationMm);
    setNumber("cumulative_overflow_mm", weatherState.cumulativeOverflowMm);
    setNumber("rain_drop_number_concentration_m3",
        precipitation.numberConcentrationPerM3);
    setNumber("rain_drop_mean_diameter_mm",
        precipitation.numberWeightedMeanDiameterMm);
    setNumber("rain_drop_volume_mean_diameter_mm",
        precipitation.volumeWeightedMeanDiameterMm);
    setNumber("rain_drop_flux_terminal_mps",
        precipitation.fluxWeightedMeanTerminalVelocityMps);
    setNumber("rain_mass_flux_kg_m2_s",
        precipitation.massFluxKgPerM2PerSecond);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetSurfaceWeather(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_physics)
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    heritage::physics::SurfaceWeatherDescription weather =
        runtime->m_physics->surfaces().weather();
    weather.enabled = LuaModuleRuntime::booleanArgument(
        *runtime, state, 1, weather.enabled);
    weather.precipitationRateMmPerHour = LuaModuleRuntime::numberArgument(
        *runtime, state, 2, weather.precipitationRateMmPerHour);
    weather.relativeHumidity = LuaModuleRuntime::numberArgument(
        *runtime, state, 3, weather.relativeHumidity);
    weather.windSpeedMps = LuaModuleRuntime::numberArgument(
        *runtime, state, 4, weather.windSpeedMps);
    weather.cloudCover = LuaModuleRuntime::numberArgument(
        *runtime, state, 5, weather.cloudCover);
    weather.drainageRateMmPerHour = LuaModuleRuntime::numberArgument(
        *runtime, state, 6, weather.drainageRateMmPerHour);
    weather.referenceEvaporationRateMmPerHour = LuaModuleRuntime::numberArgument(
        *runtime, state, 7, weather.referenceEvaporationRateMmPerHour);
    weather.windDirectionDegrees = LuaModuleRuntime::numberArgument(
        *runtime, state, 8, weather.windDirectionDegrees);
    const bool result = runtime->m_physics->surfaces().setWeather(weather);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsResetSurfaceWeather(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (runtime->m_physics)
        runtime->m_physics->surfaces().resetWeatherState();
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetSurfaceHydrology(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::physics::water::SurfaceHydrologyStats stats =
        runtime->m_physics
            ? runtime->m_physics->surfaces().hydrology().stats()
            : heritage::physics::water::SurfaceHydrologyStats{};
    const heritage::physics::water::SurfaceHydrologyBakeReport bake =
        runtime->m_physics
            ? runtime->m_physics->surfaces().hydrology().lastBakeReport()
            : heritage::physics::water::SurfaceHydrologyBakeReport{};
    runtime->m_api.lua_createtable(state, 0, 32);
    const auto setNumber = [&](const char* name, double value) {
        runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
        runtime->m_api.lua_setfield(state, -2, name);
    };
    const auto setBoolean = [&](const char* name, bool value) {
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
        runtime->m_api.lua_setfield(state, -2, name);
    };
    const auto setString = [&](const char* name, const std::string& value) {
        runtime->m_api.lua_pushstring(state, value.c_str());
        runtime->m_api.lua_setfield(state, -2, name);
    };
    setBoolean("available", stats.available);
    setBoolean("loaded_from_cache", stats.loadedFromCache);
    setBoolean("debug_visualization", stats.debugVisualizationEnabled);
    setNumber("source_triangles", static_cast<double>(stats.sourceTriangleCount));
    setNumber("cells", static_cast<double>(stats.supportCellCount));
    setNumber("support_cells", static_cast<double>(stats.supportCellCount));
    setNumber("connected_cells", 0.0);
    setNumber("prebaked_world_tiles", static_cast<double>(stats.prebakedWorldTileCount));
    setNumber("prebaked_far_payload_bytes", static_cast<double>(stats.prebakedFarPayloadBytes));
    setNumber("adaptive_minimum_cell_m", 0.0);
    setNumber("adaptive_maximum_cell_m", 0.0);
    setNumber("adaptive_0_1m_cells", 0.0);
    setNumber("adaptive_large_cells", 0.0);
    setNumber("wet_cells", 0.0);
    setNumber("simulation_steps", 0.0);
    setNumber("tire_contacts", 0.0);
    setNumber("update_rate_hz", 0.0);
    setNumber("water_volume_m3", 0.0);
    setNumber("maximum_water_depth_mm", 0.0);
    setNumber("rain_volume_m3", 0.0);
    setNumber("infiltration_volume_m3", 0.0);
    setNumber("drainage_volume_m3", 0.0);
    setNumber("evaporation_volume_m3", 0.0);
    setNumber("runoff_volume_m3", 0.0);
    setNumber("tire_cleared_volume_l", 0.0);
    setNumber("tire_spray_volume_l", 0.0);
    setNumber("last_step_ms", 0.0);
    setString("solver", "gpu_prebaked_hhyd_v15");
    setNumber("active_virtual_pipes", 0.0);
    setNumber("maximum_virtual_pipe_flux_lps", 0.0);
    setNumber("bake_ms", bake.elapsedMilliseconds);
    setString("bake_message", bake.message);
    setString("cache_path", bake.cachePath.string());
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsResetSurfaceHydrology(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (runtime->m_physics)
        runtime->m_physics->surfaces().resetHydrologyWater();
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetSurfaceHydrologyDebug(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_physics)
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }
    runtime->m_physics->surfaces().hydrology().setDebugVisualizationEnabled(
        LuaModuleRuntime::booleanArgument(*runtime, state, 1, false));
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetSurfacePresentation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::physics::SurfacePresentationStats stats =
        runtime->m_physics
            ? runtime->m_physics->surfaces().presentation().stats()
            : heritage::physics::SurfacePresentationStats{};
    const heritage::physics::rubber::TrackRubberStats rubberStats =
        runtime->m_physics
            ? runtime->m_physics->surfaces().trackRubber().stats()
            : heritage::physics::rubber::TrackRubberStats{};

    runtime->m_api.lua_createtable(state, 0, 16);
    const auto setNumber = [&](const char* name, double value) {
        runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
        runtime->m_api.lua_setfield(state, -2, name);
    };
    setNumber("active_track_marks", static_cast<double>(stats.activeTrackMarks));
    setNumber("active_tire_mark_segments", static_cast<double>(stats.activeTireMarkSegments));
    setNumber("active_particles", static_cast<double>(stats.activeParticles));
    setNumber("emitted_particles", static_cast<double>(stats.emittedParticles));
    setNumber("contact_samples", static_cast<double>(stats.contactSamples));
    setNumber("rut_intensity", stats.rutIntensity);
    setNumber("rolling_audio", stats.audio.rolling);
    setNumber("spray_audio", stats.audio.spray);
    setNumber("dust_audio", stats.audio.dust);
    setNumber("debris_audio", stats.audio.debris);
    setNumber("active_rubber_cells", static_cast<double>(rubberStats.activeCells));
    setNumber("resident_rubber_chunks", static_cast<double>(rubberStats.residentChunks));
    setNumber("rubber_contact_samples", static_cast<double>(rubberStats.contactSamples));
    setNumber("rubber_deposited_generation", rubberStats.depositedGeneration);
    setNumber("rubber_loose_generation", rubberStats.looseGeneration);
    setNumber("persistent_rubber_pieces", static_cast<double>(rubberStats.persistentPieces));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetTireDevelopmentControls(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::physics::SurfaceWorldDevelopmentControls controls =
        runtime->m_physics
            ? runtime->m_physics->surfaces().developmentControls()
            : heritage::physics::SurfaceWorldDevelopmentControls{};
    runtime->m_api.lua_createtable(state, 0, 3);
    const auto setNumber = [&](const char* name, double value) {
        runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
        runtime->m_api.lua_setfield(state, -2, name);
    };
    setNumber("wear_speed", controls.tireWearRateMultiplier);
    setNumber("rubber_generation", controls.rubberGenerationMultiplier);
    setNumber("marble_maturity", controls.marbleMaturationMultiplier);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetTireDevelopmentControls(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_physics)
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    heritage::physics::SurfaceWorldDevelopmentControls controls =
        runtime->m_physics->surfaces().developmentControls();
    controls.tireWearRateMultiplier = static_cast<double>(
        LuaModuleRuntime::numberArgument(*runtime, state, 1, controls.tireWearRateMultiplier));
    controls.rubberGenerationMultiplier = static_cast<double>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, controls.rubberGenerationMultiplier));
    controls.marbleMaturationMultiplier = static_cast<double>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, controls.marbleMaturationMultiplier));
    const bool result = runtime->m_physics->surfaces().setDevelopmentControls(controls);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsResetTrackRubber(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (runtime->m_physics)
        runtime->m_physics->surfaces().clearTrackRubber();
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetSurfaceEnvironment(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (!runtime->m_physics)
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    heritage::physics::SurfaceWorldEnvironment environment =
        runtime->m_physics->surfaces().environment();
    environment.wetness = static_cast<double>(
        LuaModuleRuntime::numberArgument(
            *runtime, state, 1, environment.wetness));
    environment.ambientTemperatureC = static_cast<double>(
        LuaModuleRuntime::numberArgument(
            *runtime, state, 2, environment.ambientTemperatureC));
    environment.surfaceTemperatureC = static_cast<double>(
        LuaModuleRuntime::numberArgument(
            *runtime, state, 3, environment.surfaceTemperatureC));
    environment.surfaceTemperatureOverrideEnabled =
        LuaModuleRuntime::booleanArgument(
            *runtime, state, 4, environment.surfaceTemperatureOverrideEnabled);

    const bool result = runtime->m_physics->surfaces().setEnvironment(environment);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsIsPaused(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(
        state,
        runtime->m_physics && runtime->m_physics->paused() ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetPaused(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (runtime->m_physics)
        runtime->m_physics->setPaused(LuaModuleRuntime::booleanArgument(*runtime, state, 1, false));
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsRequestSingleStep(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (runtime->m_physics)
        runtime->m_physics->requestSingleStep();
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetTimeScale(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->timeScale())
            : 0.0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetTimeScale(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const float scale = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 1.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->setTimeScale(scale);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetStepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->stepCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetSimulationTime(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->simulationTime())
            : 0.0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetInterpolationAlpha(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->interpolationAlpha())
            : 0.0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetLastSubstepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->lastSubstepCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetMaximumWorldStepsPerFrame(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->maximumWorldStepsPerFrame())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetPendingWorldStepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->pendingWorldStepCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBacklogTime(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->backlogTime())
            : 0.0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetPeakBacklogTime(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->peakBacklogTime())
            : 0.0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsWasOverloadedLastFrame(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(
        state,
        runtime->m_physics
            && runtime->m_physics->overloadedLastFrame()
            ? 1
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetOverloadFrameCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->overloadFrameCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetDroppedTime(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->droppedSimulationTime())
            : 0.0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetClampedTime(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->clampedSimulationTime())
            : 0.0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsResetClock(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (runtime->m_physics)
        runtime->m_physics->resetClock();
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetFloatingOriginAnchor(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::physics::BodyHandle body =
        LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1);
    const float threshold = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, 4096.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->setFloatingOriginAnchor(body, threshold);
    if (!result && runtime->m_physics)
        runtime->m_lastPhysicsError = runtime->m_physics->lastError();
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsClearFloatingOriginAnchor(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (runtime->m_physics)
        runtime->m_physics->clearFloatingOriginAnchor();
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetWorldOrigin(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const heritage::math::DVec3 origin = runtime->m_physics
        ? runtime->m_physics->globalOrigin()
        : heritage::math::DVec3{};
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(origin.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(origin.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(origin.z));
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetOriginRebaseCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->originRebaseCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsResetWorldOrigin(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->resetWorldOrigin();
    if (!result && runtime->m_physics)
        runtime->m_lastPhysicsError = runtime->m_physics->lastError();
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsLocalToGlobal(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const heritage::math::Vec3 local{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0))
    };
    const heritage::math::DVec3 global = runtime->m_physics
        ? runtime->m_physics->localToGlobal(local)
        : heritage::math::DVec3{
            static_cast<double>(local.x),
            static_cast<double>(local.y),
            static_cast<double>(local.z) };
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(global.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(global.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(global.z));
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsGlobalToLocal(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const heritage::math::DVec3 global{
        static_cast<double>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
        static_cast<double>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<double>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0))
    };
    heritage::math::Vec3 local{};
    if (!runtime->m_physics
        || !runtime->m_physics->globalToLocal(global, local))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }
    runtime->m_api.lua_pushnumber(state, local.x);
    runtime->m_api.lua_pushnumber(state, local.y);
    runtime->m_api.lua_pushnumber(state, local.z);
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyGlobalPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    heritage::math::DVec3 global{};
    if (!runtime->m_physics
        || !runtime->m_physics->bodyGlobalPosition(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), global))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(global.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(global.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(global.z));
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyGlobalPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const heritage::math::DVec3 global{
        static_cast<double>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<double>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<double>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->setBodyGlobalPosition(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), global);
    if (!result && runtime->m_physics)
        runtime->m_lastPhysicsError = runtime->m_physics->lastError();
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

} // namespace heritage::modules
