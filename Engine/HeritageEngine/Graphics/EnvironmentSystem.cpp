#include "EnvironmentSystem.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::graphics {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep(float edge0, float edge1, float value)
{
    if (std::abs(edge1 - edge0) <= 1.0e-6f)
        return value >= edge1 ? 1.0f : 0.0f;
    const float t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 multiply(
    const heritage::math::Vec3& value,
    float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

heritage::math::Vec3 mix(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b,
    float t)
{
    t = clamp01(t);
    return add(multiply(a, 1.0f - t), multiply(b, t));
}

heritage::math::Vec3 normalize(const heritage::math::Vec3& value)
{
    const float lengthSquared =
        value.x * value.x + value.y * value.y + value.z * value.z;
    if (lengthSquared <= 1.0e-12f)
        return { 0.0f, 1.0f, 0.0f };
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return multiply(value, inverseLength);
}

float wrapHours(float hours)
{
    if (!std::isfinite(hours))
        return 12.0f;
    hours = std::fmod(hours, 24.0f);
    if (hours < 0.0f)
        hours += 24.0f;
    return hours;
}

} // namespace

void EnvironmentSystem::reset()
{
    m_timeOfDayHours = 14.0f;
    m_timeScale = 240.0f;
    m_cycleEnabled = true;
    rebuildLighting();
}

void EnvironmentSystem::update(float realDeltaSeconds)
{
    if (m_cycleEnabled && std::isfinite(realDeltaSeconds) && realDeltaSeconds > 0.0f)
    {
        const float clampedDelta = std::min(realDeltaSeconds, 0.25f);
        m_timeOfDayHours = wrapHours(
            m_timeOfDayHours + clampedDelta * m_timeScale / 3600.0f);
    }
    rebuildLighting();
}

void EnvironmentSystem::setTimeOfDayHours(float hours)
{
    m_timeOfDayHours = wrapHours(hours);
    rebuildLighting();
}

void EnvironmentSystem::setTimeScale(float simulatedSecondsPerRealSecond)
{
    if (!std::isfinite(simulatedSecondsPerRealSecond))
        return;
    m_timeScale = std::clamp(simulatedSecondsPerRealSecond, 0.0f, 86400.0f);
}

void EnvironmentSystem::rebuildLighting()
{
    EnvironmentLighting lighting;
    lighting.timeOfDayHours = m_timeOfDayHours;

    // Sunrise ~= 06:00, solar noon ~= 12:00, sunset ~= 18:00.
    const float solarAngle =
        (m_timeOfDayHours - 6.0f) / 24.0f * (2.0f * kPi);
    const float elevation = std::sin(solarAngle);
    const float horizontal = std::cos(solarAngle);
    lighting.sunDirection = normalize({
        -horizontal * 0.72f,
        elevation,
        horizontal * 0.69f
    });

    const float daylight = smoothstep(-0.08f, 0.14f, elevation);
    const float twilight =
        (1.0f - smoothstep(-0.18f, -0.02f, elevation))
        * smoothstep(-0.34f, -0.10f, elevation);
    const float warmHorizon =
        (1.0f - smoothstep(0.02f, 0.42f, std::max(elevation, 0.0f)))
        * daylight;

    const heritage::math::Vec3 nightZenith{ 0.004f, 0.010f, 0.032f };
    const heritage::math::Vec3 nightHorizon{ 0.012f, 0.018f, 0.045f };
    const heritage::math::Vec3 twilightZenith{ 0.11f, 0.12f, 0.25f };
    const heritage::math::Vec3 twilightHorizon{ 0.72f, 0.28f, 0.12f };
    const heritage::math::Vec3 dayZenith{ 0.12f, 0.31f, 0.72f };
    const heritage::math::Vec3 dayHorizon{ 0.53f, 0.70f, 0.92f };

    lighting.skyZenith = mix(nightZenith, dayZenith, daylight);
    lighting.skyHorizon = mix(nightHorizon, dayHorizon, daylight);
    if (twilight > 0.0f)
    {
        lighting.skyZenith = mix(lighting.skyZenith, twilightZenith, twilight * 0.72f);
        lighting.skyHorizon = mix(lighting.skyHorizon, twilightHorizon, twilight);
    }
    if (warmHorizon > 0.0f)
    {
        lighting.skyHorizon = mix(
            lighting.skyHorizon,
            heritage::math::Vec3{ 0.93f, 0.46f, 0.19f },
            warmHorizon * 0.48f);
    }

    const heritage::math::Vec3 nightGround{ 0.006f, 0.008f, 0.010f };
    const heritage::math::Vec3 dayGroundHorizon{ 0.28f, 0.28f, 0.24f };
    const heritage::math::Vec3 dayGroundNadir{ 0.055f, 0.070f, 0.050f };
    lighting.groundHorizon = mix(nightGround, dayGroundHorizon, daylight);
    lighting.groundNadir = mix(nightGround, dayGroundNadir, daylight);

    const heritage::math::Vec3 warmSun{ 1.0f, 0.33f, 0.10f };
    const heritage::math::Vec3 daySun{ 1.0f, 0.95f, 0.82f };
    lighting.sunColor = mix(warmSun, daySun, smoothstep(0.03f, 0.38f, elevation));
    lighting.sunIntensity = daylight * (0.22f + (3.2f - 0.22f) * smoothstep(0.0f, 0.65f, elevation));
    lighting.starIntensity = 1.0f - smoothstep(-0.12f, 0.02f, elevation);
    lighting.daylightFactor = daylight;

    m_lighting = lighting;
}

} // namespace heritage::graphics
