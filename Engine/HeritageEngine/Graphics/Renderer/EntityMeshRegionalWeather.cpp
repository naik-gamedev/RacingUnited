#include "EntityMeshRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace heritage::graphics {
namespace {
heritage::math::Vec3 weatherMix(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b,
    float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

float weatherSmoothstep(float edge0, float edge1, float value)
{
    if (std::abs(edge1 - edge0) <= 1.0e-6f)
        return value >= edge1 ? 1.0f : 0.0f;
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}

EnvironmentLighting EntityMeshRenderer::weatherAdjustedLighting(
    EnvironmentLighting lighting,
    const heritage::physics::SurfaceWorld* surfaces,
    const heritage::math::DVec3& cameraGlobal) const
{
    if (!surfaces)
        return lighting;
    const auto& weather = surfaces->weather();
    if (!weather.enabled)
        return lighting;

    const auto regional = surfaces->precipitation().regionalWeatherSample(
        cameraGlobal.x, cameraGlobal.z);
    const float cloud = std::clamp(
        static_cast<float>(regional.valid ? regional.cloudCover : weather.cloudCover),
        0.0f, 1.0f);
    const float humidity = std::clamp(
        static_cast<float>(regional.valid ? regional.relativeHumidity : weather.relativeHumidity),
        0.0f, 1.0f);
    const float localRainRate = static_cast<float>(regional.valid
        ? regional.currentRateMmPerHour
        : weather.precipitationRateMmPerHour);
    const float rain = std::clamp(localRainRate / 80.0f, 0.0f, 1.0f);
    const float storm = std::clamp(
        static_cast<float>(regional.valid ? regional.stormIntensity
            : cloud * 0.60f + rain * 0.48f),
        0.0f, 1.0f);

    // CLOUDURP15J7: weather is no longer allowed to act as a second hidden
    // night/day cycle. Astronomical lighting remains the sole authority for
    // night darkness and moon visibility. Weather now only nudges colour and
    // transmission, and it does so primarily during the day/twilight.
    const float day = std::clamp(lighting.daylightFactor, 0.0f, 1.0f);
    // CELESTIAL06: weather evaluates the same normalized solar-cycle scalar as
    // sky/material presentation. It cannot create a second twilight state by
    // independently thresholding raw Sun elevation.
    const float twilight = weatherSmoothstep(0.025f, 0.115f, day)
        * (1.0f - weatherSmoothstep(0.24f, 0.46f, day));
    const float overcast = std::clamp(
        cloud * 0.72f + rain * 0.16f + storm * 0.08f + humidity * 0.04f,
        0.0f, 1.0f);
    const float clearDay = day * (1.0f - overcast);
    const float daytimeWeatherWeight = overcast * weatherSmoothstep(0.04f, 0.30f, day);
    const float twilightWeatherWeight = overcast * twilight * 0.22f;

    lighting.skyHorizon = weatherMix(
        lighting.skyHorizon,
        { 0.00f, 0.545f, 0.925f },
        clearDay * 0.05f);
    lighting.skyZenith = weatherMix(
        lighting.skyZenith,
        { 0.00f, 0.335f, 0.845f },
        clearDay * 0.035f);

    const heritage::math::Vec3 dayOvercastHorizon{ 0.63f, 0.67f, 0.73f };
    const heritage::math::Vec3 dayOvercastZenith{ 0.45f, 0.49f, 0.58f };
    const heritage::math::Vec3 twilightOvercastHorizon{ 0.44f, 0.35f, 0.31f };
    const heritage::math::Vec3 twilightOvercastZenith{ 0.18f, 0.17f, 0.20f };

    lighting.skyHorizon = weatherMix(
        lighting.skyHorizon, dayOvercastHorizon, daytimeWeatherWeight * 0.34f);
    lighting.skyZenith = weatherMix(
        lighting.skyZenith, dayOvercastZenith, daytimeWeatherWeight * 0.28f);
    lighting.skyHorizon = weatherMix(
        lighting.skyHorizon, twilightOvercastHorizon, twilightWeatherWeight);
    lighting.skyZenith = weatherMix(
        lighting.skyZenith, twilightOvercastZenith, twilightWeatherWeight * 0.75f);

    const heritage::math::Vec3 dayGroundHorizon{ 0.18f, 0.18f, 0.18f };
    const heritage::math::Vec3 dayGroundNadir{ 0.056f, 0.056f, 0.058f };
    lighting.groundHorizon = weatherMix(
        lighting.groundHorizon, dayGroundHorizon, daytimeWeatherWeight * 0.18f);
    lighting.groundNadir = weatherMix(
        lighting.groundNadir, dayGroundNadir, daytimeWeatherWeight * 0.14f);

    // CELESTIAL08: cloud/weather may soften direct Sun light, but the previous
    // broad attenuation was far too strong once the dedicated spatial cloud
    // shadow receiver was also active. Preserve the old response shape, then
    // apply exactly one tenth of its loss; visible cloud opacity is unchanged.
    const float legacyDaylightTransmission = std::clamp(
        1.0f - overcast * 0.26f - rain * 0.05f - storm * 0.03f,
        0.45f, 1.0f);
    const float daylightTransmission = 1.0f
        + (legacyDaylightTransmission - 1.0f) * 0.10f;
    const float weatherSunWeight = weatherSmoothstep(0.04f, 0.28f, day);
    lighting.sunIntensity *= 1.0f + (daylightTransmission - 1.0f) * weatherSunWeight;
    lighting.sunColor = weatherMix(
        lighting.sunColor,
        { 0.92f, 0.93f, 0.96f },
        daytimeWeatherWeight * 0.08f);

    // CELESTIAL01: apply the same large-scale cloud climatology to Moon
    // radiance that the Sun already receives. The detailed camera-local cloud
    // cookie is applied later in the material shader, so this scalar represents
    // broad overcast/haze while the cookie supplies moving spatial shadows.
    const float moonTransmission = std::clamp(
        1.0f - overcast * 0.20f - rain * 0.06f - storm * 0.04f,
        0.55f, 1.0f);
    lighting.moonIntensity *= moonTransmission;
    lighting.moonColor = weatherMix(
        lighting.moonColor,
        { 0.78f, 0.82f, 0.90f },
        overcast * 0.06f);

    lighting.starIntensity *= (1.0f - cloud * 0.78f);

    // Re-resolve the shared celestial key after weather has attenuated the
    // individual Sun/Moon channels.  Do not reintroduce the old daylight<0.22
    // binary handoff here: that was the main scene-darkening trough at dawn.
    resolveCelestialKeyLight(lighting);
    return lighting;
}

bool EntityMeshRenderer::updateRegionalWeatherMap(
    const heritage::physics::SurfaceWorld* surfaceWorld,
    const heritage::math::DVec3& cameraGlobal,
    float elapsedSeconds)
{
    if (!surfaceWorld || !surfaceWorld->weather().enabled)
        return false;

    // CLOUDURP15K: keep the CPU regional-weather texture on a fixed world
    // lattice. The old arbitrary 5 km recenter step did not match the texture
    // texel spacing (2,000 km / 512 = 3,906.25 m), so every recenter resampled
    // the entire occupancy field at a different phase and visible clouds could
    // pop when the camera crossed a 5 km boundary. Recenter by exactly one
    // weather texel instead: overlapping world samples then remain identical.
    const int resolution = std::max(m_regionalWeatherResolution, 16);
    const double diameterM = m_regionalWeatherHalfRangeM * 2.0;
    const double kCenterSnapM = diameterM / static_cast<double>(resolution);
    const double centerX = std::floor(cameraGlobal.x / kCenterSnapM) * kCenterSnapM;
    const double centerZ = std::floor(cameraGlobal.z / kCenterSnapM) * kCenterSnapM;
    const double movedM = std::hypot(
        centerX - m_regionalWeatherCenterX,
        centerZ - m_regionalWeatherCenterZ);
    const auto& authoredWeather = surfaceWorld->weather();
    const bool authoringChanged =
        std::abs(authoredWeather.precipitationRateMmPerHour
            - m_regionalWeatherAuthoredRainMmPerHour) > 1.0e-6
        || std::abs(authoredWeather.relativeHumidity
            - m_regionalWeatherAuthoredHumidity) > 1.0e-6
        || std::abs(authoredWeather.cloudCover
            - m_regionalWeatherAuthoredCloudCover) > 1.0e-6
        || std::abs(authoredWeather.windSpeedMps
            - m_regionalWeatherAuthoredWindSpeedMps) > 1.0e-6
        || std::abs(authoredWeather.windDirectionDegrees
            - m_regionalWeatherAuthoredWindDirectionDeg) > 1.0e-6;
    // The regional field is pure wind advection between authoring changes. The
    // shader applies that advection continuously from the upload timestamp, so
    // there is no reason to rebuild ~16k CPU weather samples every half-second.
    // Refresh only when the snapped world window moves or weather authoring changes.
    const bool due = !m_regionalWeatherTexture
        || m_regionalWeatherLastUpdateSeconds < 0.0
        || movedM >= kCenterSnapM
        || authoringChanged;
    if (!due)
        return true;

    const std::size_t texelCount = static_cast<std::size_t>(resolution) * resolution;
    m_regionalWeatherPixels.resize(texelCount * 4u);
    const double stepM = kCenterSnapM;
    const auto& field = surfaceWorld->precipitation();
    for (int z = 0; z < resolution; ++z)
    {
        for (int x = 0; x < resolution; ++x)
        {
            const double gx = centerX - m_regionalWeatherHalfRangeM
                + (static_cast<double>(x) + 0.5) * stepM;
            const double gz = centerZ - m_regionalWeatherHalfRangeM
                + (static_cast<double>(z) + 0.5) * stepM;
            const auto sample = field.regionalWeatherSample(gx, gz);
            const std::size_t index = (static_cast<std::size_t>(z) * resolution + x) * 4u;
            const auto byte = [](double value) -> std::uint8_t {
                return static_cast<std::uint8_t>(std::lround(
                    std::clamp(value, 0.0, 1.0) * 255.0));
            };
            m_regionalWeatherPixels[index + 0u] = byte(sample.cloudCover);
            // Stable physical rain encoding. Do not normalize by the authored
            // peak: doing so made any non-zero rain slider instantly jump G to
            // the cell mask and caused discontinuous cloud/sun darkening.
            m_regionalWeatherPixels[index + 1u] = byte(sample.currentRateMmPerHour / 80.0);
            m_regionalWeatherPixels[index + 2u] = byte(sample.relativeHumidity);
            m_regionalWeatherPixels[index + 3u] = byte(sample.stormIntensity);
        }
    }

    if (!m_regionalWeatherTexture)
    {
        glGenTextures(1, &m_regionalWeatherTexture);
        if (!m_regionalWeatherTexture)
            return false;
        glBindTexture(GL_TEXTURE_2D, m_regionalWeatherTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            resolution,
            resolution,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            m_regionalWeatherPixels.data());
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, m_regionalWeatherTexture);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            resolution,
            resolution,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            m_regionalWeatherPixels.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    m_regionalWeatherCenterX = centerX;
    m_regionalWeatherCenterZ = centerZ;
    m_regionalWeatherLastUpdateSeconds = static_cast<double>(elapsedSeconds);
    m_regionalWeatherFieldElapsedAtUpload = field.elapsedSeconds();
    m_regionalWeatherAuthoredRainMmPerHour = authoredWeather.precipitationRateMmPerHour;
    m_regionalWeatherAuthoredHumidity = authoredWeather.relativeHumidity;
    m_regionalWeatherAuthoredCloudCover = authoredWeather.cloudCover;
    m_regionalWeatherAuthoredWindSpeedMps = authoredWeather.windSpeedMps;
    m_regionalWeatherAuthoredWindDirectionDeg = authoredWeather.windDirectionDegrees;
    return true;
}

void EntityMeshRenderer::shutdownRegionalWeatherMap()
{
    if (m_regionalWeatherTexture)
        glDeleteTextures(1, &m_regionalWeatherTexture);
    m_regionalWeatherTexture = 0;
    m_regionalWeatherCenterX = 0.0;
    m_regionalWeatherCenterZ = 0.0;
    m_regionalWeatherLastUpdateSeconds = -1.0;
    m_regionalWeatherFieldElapsedAtUpload = 0.0;
    m_regionalWeatherAuthoredRainMmPerHour = -1.0;
    m_regionalWeatherAuthoredHumidity = -1.0;
    m_regionalWeatherAuthoredCloudCover = -1.0;
    m_regionalWeatherAuthoredWindSpeedMps = -1.0;
    m_regionalWeatherAuthoredWindDirectionDeg = -1.0;
    m_regionalWeatherPixels.clear();
    m_weatherHazeSmoothingInitialized = false;
    m_weatherHazeLastElapsedSeconds = -1.0f;
}


void EntityMeshRenderer::bindRegionalWeatherMaterialState(
    const heritage::physics::SurfaceWorld* surfaceWorld,
    const heritage::math::DVec3& cameraGlobalForSurface,
    const heritage::physics::weather::RegionalWeatherSample& cameraRegionalWeather,
    bool regionalWeatherMapReady,
    const EnvironmentLighting& lighting,
    float elapsedSeconds)
{
    const float day = std::clamp(lighting.daylightFactor, 0.0f, 1.0f);
    const float twilight = weatherSmoothstep(0.025f, 0.115f, day)
        * (1.0f - weatherSmoothstep(0.24f, 0.46f, day));

    float rain = 0.0f;
    float cloud = 0.0f;
    float humidity = 0.55f;
    float authoredCloud = 0.0f;
    float authoredHumidity = 0.55f;
    if (surfaceWorld && surfaceWorld->weather().enabled)
    {
        const auto& weather = surfaceWorld->weather();
        authoredCloud = std::clamp(static_cast<float>(weather.cloudCover), 0.0f, 1.0f);
        authoredHumidity = std::clamp(static_cast<float>(weather.relativeHumidity), 0.0f, 1.0f);
        rain = std::clamp(
            static_cast<float>((cameraRegionalWeather.valid
                ? cameraRegionalWeather.currentRateMmPerHour
                : weather.precipitationRateMmPerHour) / 80.0),
            0.0f, 1.0f);
        cloud = std::clamp(
            static_cast<float>(cameraRegionalWeather.valid
                ? cameraRegionalWeather.cloudCover
                : weather.cloudCover),
            0.0f, 1.0f);
        humidity = std::clamp(
            static_cast<float>(cameraRegionalWeather.valid
                ? cameraRegionalWeather.relativeHumidity
                : weather.relativeHumidity),
            0.0f, 1.0f);
    }

    // CELESTIAL08: J9 haze is background atmosphere, not a binary property of
    // whichever regional cloud cell happens to sit over the camera. Low-pass
    // camera-local rain/cloud/humidity before they can modulate aerial haze,
    // and let the authored scene climate own most of the permanent component.
    if (!m_weatherHazeSmoothingInitialized
        || !std::isfinite(elapsedSeconds)
        || elapsedSeconds < m_weatherHazeLastElapsedSeconds)
    {
        m_weatherHazeRain01 = rain;
        m_weatherHazeCloud01 = cloud;
        m_weatherHazeHumidity01 = humidity;
        m_weatherHazeSmoothingInitialized = true;
    }
    else
    {
        const float dt = std::clamp(elapsedSeconds - m_weatherHazeLastElapsedSeconds, 0.0f, 0.25f);
        const float response = 1.0f - std::exp(-dt / 1.35f);
        m_weatherHazeRain01 += (rain - m_weatherHazeRain01) * response;
        m_weatherHazeCloud01 += (cloud - m_weatherHazeCloud01) * response;
        m_weatherHazeHumidity01 += (humidity - m_weatherHazeHumidity01) * response;
    }
    m_weatherHazeLastElapsedSeconds = elapsedSeconds;

    const float hazeCloud = std::clamp(authoredCloud * 0.90f + m_weatherHazeCloud01 * 0.10f, 0.0f, 1.0f);
    const float hazeHumidity = std::clamp(authoredHumidity * 0.85f + m_weatherHazeHumidity01 * 0.15f, 0.0f, 1.0f);
    const float hazeRain = std::clamp(m_weatherHazeRain01, 0.0f, 1.0f);

    // J9: real-life atmospheric haze / aerial perspective. This permanent
    // baseline never switches off; local weather can only move it continuously.
    // CELESTIAL09: atmospheric particles still exist at night, but darkness
    // does not create extra aerosol. The old deep-night density bonus made the
    // nocturnal air look more opaque precisely when its visible air-light
    // should be weakest. Preserve extinction; remove that artificial bonus.
    const float atmosphericHazeDensity =
        0.000045f
        + 0.000050f * day
        + 0.000030f * twilight
        + 0.000065f * hazeHumidity
        + 0.000025f * hazeCloud;
    float weatherFogDensity = atmosphericHazeDensity
        + hazeRain * (0.00105f + hazeCloud * 0.00095f + hazeHumidity * 0.00025f);

    heritage::math::Vec3 atmosphericHazeColor = weatherMix(
        { 0.085f, 0.100f, 0.155f },
        { 0.69f, 0.78f, 0.90f },
        day);
    atmosphericHazeColor = weatherMix(
        atmosphericHazeColor,
        { 0.44f, 0.42f, 0.46f },
        twilight * 0.45f);
    atmosphericHazeColor = weatherMix(
        atmosphericHazeColor,
        { 0.80f, 0.82f, 0.84f },
        hazeHumidity * (0.25f + 0.45f * day));
    atmosphericHazeColor = weatherMix(
        atmosphericHazeColor,
        lighting.skyHorizon,
        0.42f + 0.28f * twilight);

    // CELESTIAL11: background nocturnal aerosol remains as extinction only.
    // Without Sun illumination there is no global blue/grey air-light veil;
    // deep night therefore fades distant geometry toward darkness instead of
    // painting it with luminous haze. The Moon keeps its local halo but no
    // longer lights the entire atmosphere as a uniform fog colour. Visible
    // night mist belongs to a future low-altitude local-light scattering path
    // (headlights/streetlights/floodlights), not this global aerial term.
    const float solarAirlight = weatherSmoothstep(0.015f, 0.30f, day);
    const float nightAirlight = solarAirlight;
    atmosphericHazeColor = {
        atmosphericHazeColor.x * nightAirlight,
        atmosphericHazeColor.y * nightAirlight,
        atmosphericHazeColor.z * nightAirlight };

    heritage::math::Vec3 weatherFogColor = atmosphericHazeColor;
    if (surfaceWorld && surfaceWorld->weather().enabled)
    {
        const heritage::math::Vec3 illuminatedRainFog{
            0.17f * nightAirlight,
            0.19f * nightAirlight,
            0.21f * nightAirlight };
        weatherFogColor = weatherMix(
            weatherFogColor,
            illuminatedRainFog,
            hazeRain * 0.52f);
    }
    glUniform1f(m_uniforms.weatherFogDensity, weatherFogDensity);
    glUniform3f(
        m_uniforms.weatherFogColor,
        weatherFogColor.x,
        weatherFogColor.y,
        weatherFogColor.z);
    glUniform1i(
        m_uniforms.regionalWeatherMapValid,
        regionalWeatherMapReady && m_regionalWeatherTexture != 0 ? 1 : 0);
    glUniform2f(
        m_uniforms.regionalWeatherCameraOffsetXZ,
        static_cast<float>(cameraGlobalForSurface.x - m_regionalWeatherCenterX),
        static_cast<float>(cameraGlobalForSurface.z - m_regionalWeatherCenterZ));
    float weatherAdvectionX = 0.0f;
    float weatherAdvectionZ = 0.0f;
    if (surfaceWorld)
    {
        const double weatherFieldAgeSeconds = std::max(
            surfaceWorld->precipitation().elapsedSeconds()
                - m_regionalWeatherFieldElapsedAtUpload,
            0.0);
        const auto weatherFieldWind = surfaceWorld->precipitation().weatherSteeringWindVelocityMps();
        weatherAdvectionX = static_cast<float>(
            -static_cast<double>(weatherFieldWind.x) * weatherFieldAgeSeconds * 0.38);
        weatherAdvectionZ = static_cast<float>(
            -static_cast<double>(weatherFieldWind.z) * weatherFieldAgeSeconds * 0.38);
    }
    glUniform2f(
        m_uniforms.regionalWeatherAdvectionXZ,
        weatherAdvectionX, weatherAdvectionZ);
    glUniform1f(
        m_uniforms.regionalWeatherHalfRangeM,
        static_cast<float>(m_regionalWeatherHalfRangeM));
    const float weatherStorm = cameraRegionalWeather.valid
        ? static_cast<float>(cameraRegionalWeather.stormIntensity)
        : 0.0f;
    const float weatherHumidity = cameraRegionalWeather.valid
        ? static_cast<float>(cameraRegionalWeather.relativeHumidity)
        : (surfaceWorld ? static_cast<float>(surfaceWorld->weather().relativeHumidity) : 0.55f);
    const float weatherCloudBaseGlobalM = 1550.0f
        + (620.0f - 1550.0f) * std::clamp(weatherHumidity, 0.0f, 1.0f)
        - weatherStorm * 240.0f;
    const float weatherCloudBaseRelativeM = weatherCloudBaseGlobalM
        - static_cast<float>(cameraGlobalForSurface.y);
    glUniform1f(m_uniforms.weatherCloudBaseM, weatherCloudBaseRelativeM);
    glActiveTexture(GL_TEXTURE0 + 15);
    glBindTexture(GL_TEXTURE_2D,
        regionalWeatherMapReady ? m_regionalWeatherTexture : 0);

}

} // namespace heritage::graphics
