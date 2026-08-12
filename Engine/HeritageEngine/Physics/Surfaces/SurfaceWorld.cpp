#include "SurfaceWorld.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::physics {

void SurfaceWorld::clear()
{
    m_environment = {};
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
    // Keep the position in the signature now: future TIRE15B/TIRE15C layers
    // can sample spatial water/temperature without changing tire call sites.
    (void)localPosition;

    SurfaceLocalConditions result;
    const double baseWetness = std::clamp(
        std::isfinite(authoredWetness) ? authoredWetness : 0.0,
        0.0,
        1.0);
    const double weatherWetness = std::clamp(m_environment.wetness, 0.0, 1.0);
    // Union of two [0,1] coverages: pre-wet authored road remains wet while
    // global rain can wet an otherwise dry scene surface.
    result.wetness = 1.0 - (1.0 - baseWetness) * (1.0 - weatherWetness);
    result.ambientTemperatureC = m_environment.ambientTemperatureC;

    if (m_environment.surfaceTemperatureOverrideEnabled)
    {
        result.surfaceTemperatureC = m_environment.surfaceTemperatureC;
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
    m_trackRubber.advance(deltaTimeSeconds, static_cast<float>(m_environment.wetness));
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
