#include "EnvironmentSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace heritage::graphics {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDegreesToRadians = kPi / 180.0;
constexpr int kDerivedUtcOffset = (std::numeric_limits<int>::min)();

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


bool leapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month)
{
    static constexpr int kDays[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (month < 1 || month > 12)
        return 0;
    if (month == 2 && leapYear(year))
        return 29;
    return kDays[month - 1];
}

int dayOfWeekSundayZero(int year, int month, int day)
{
    // Sakamoto Gregorian calendar algorithm.
    static constexpr int table[12] = { 0,3,2,5,0,3,5,1,4,6,2,4 };
    if (month < 3)
        --year;
    const int value = year + year / 4 - year / 100 + year / 400
        + table[month - 1] + day;
    return ((value % 7) + 7) % 7;
}

int lastSundayOfMonth(int year, int month)
{
    const int last = daysInMonth(year, month);
    return last - dayOfWeekSundayZero(year, month, last);
}

bool europeCentralDstLocal(int year, int month, int day, float localHours)
{
    if (month < 3 || month > 10)
        return false;
    if (month > 3 && month < 10)
        return true;
    if (month == 3)
    {
        const int transitionDay = lastSundayOfMonth(year, 3);
        return day > transitionDay || (day == transitionDay && localHours >= 2.0f);
    }
    const int transitionDay = lastSundayOfMonth(year, 10);
    return day < transitionDay || (day == transitionDay && localHours < 3.0f);
}

double wrapDegrees(double degrees)
{
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0)
        degrees += 360.0;
    return degrees;
}

double wrapRadians(double radians)
{
    radians = std::fmod(radians, kTwoPi);
    if (radians < 0.0)
        radians += kTwoPi;
    return radians;
}

double gregorianJulianDate(
    int year,
    int month,
    int day,
    double utcHours)
{
    int y = year;
    int m = month;
    if (m <= 2)
    {
        --y;
        m += 12;
    }
    const int a = y / 100;
    const int b = 2 - a + a / 4;
    return std::floor(365.25 * static_cast<double>(y + 4716))
        + std::floor(30.6001 * static_cast<double>(m + 1))
        + static_cast<double>(day) + static_cast<double>(b)
        - 1524.5 + utcHours / 24.0;
}

double greenwichMeanSiderealRadians(double julianDate)
{
    const double d = julianDate - 2451545.0;
    const double t = d / 36525.0;
    const double degrees = 280.46061837
        + 360.98564736629 * d
        + 0.000387933 * t * t
        - (t * t * t) / 38710000.0;
    return wrapDegrees(degrees) * kDegreesToRadians;
}

struct EquatorialCoordinates
{
    double rightAscension = 0.0;
    double declination = 0.0;
};

EquatorialCoordinates solarEquatorial(double julianDate)
{
    const double n = julianDate - 2451545.0;
    const double meanLongitude = wrapDegrees(280.460 + 0.9856474 * n) * kDegreesToRadians;
    const double meanAnomaly = wrapDegrees(357.528 + 0.9856003 * n) * kDegreesToRadians;
    const double eclipticLongitude = meanLongitude
        + (1.915 * std::sin(meanAnomaly) + 0.020 * std::sin(2.0 * meanAnomaly)) * kDegreesToRadians;
    const double obliquity = (23.439 - 0.0000004 * n) * kDegreesToRadians;

    EquatorialCoordinates result;
    result.rightAscension = wrapRadians(std::atan2(
        std::cos(obliquity) * std::sin(eclipticLongitude),
        std::cos(eclipticLongitude)));
    result.declination = std::asin(
        std::sin(obliquity) * std::sin(eclipticLongitude));
    return result;
}

EquatorialCoordinates lunarEquatorial(double julianDate)
{
    // Compact low-cost lunar ephemeris. This is intentionally positional only:
    // Racing United currently renders the moon as a permanently full disc.
    const double n = julianDate - 2451545.0;
    const double meanLongitude = wrapDegrees(218.316 + 13.176396 * n) * kDegreesToRadians;
    const double meanAnomaly = wrapDegrees(134.963 + 13.064993 * n) * kDegreesToRadians;
    const double argumentLatitude = wrapDegrees(93.272 + 13.229350 * n) * kDegreesToRadians;
    const double lambda = meanLongitude + 6.289 * kDegreesToRadians * std::sin(meanAnomaly);
    const double beta = 5.128 * kDegreesToRadians * std::sin(argumentLatitude);
    const double obliquity = (23.439 - 0.0000004 * n) * kDegreesToRadians;

    const double x = std::cos(beta) * std::cos(lambda);
    const double y = std::cos(beta) * std::sin(lambda) * std::cos(obliquity)
        - std::sin(beta) * std::sin(obliquity);
    const double z = std::cos(beta) * std::sin(lambda) * std::sin(obliquity)
        + std::sin(beta) * std::cos(obliquity);

    EquatorialCoordinates result;
    result.rightAscension = wrapRadians(std::atan2(y, x));
    result.declination = std::asin(std::clamp(z, -1.0, 1.0));
    return result;
}

heritage::math::Vec3 horizontalDirection(
    double localSiderealRadians,
    double latitudeRadians,
    const EquatorialCoordinates& equatorial)
{
    const double hourAngle = localSiderealRadians - equatorial.rightAscension;
    const double cosDecl = std::cos(equatorial.declination);
    const double sinDecl = std::sin(equatorial.declination);
    const double cosLat = std::cos(latitudeRadians);
    const double sinLat = std::sin(latitudeRadians);

    const double east = -cosDecl * std::sin(hourAngle);
    const double north = cosLat * sinDecl
        - sinLat * cosDecl * std::cos(hourAngle);
    const double up = sinLat * sinDecl
        + cosLat * cosDecl * std::cos(hourAngle);
    return normalize({
        static_cast<float>(east),
        static_cast<float>(up),
        static_cast<float>(north)
    });
}

void buildWorldToCelestialRows(
    double localSiderealRadians,
    double latitudeRadians,
    heritage::math::Vec3& row0,
    heritage::math::Vec3& row1,
    heritage::math::Vec3& row2)
{
    const double s = std::sin(localSiderealRadians);
    const double c = std::cos(localSiderealRadians);
    const double sinLat = std::sin(latitudeRadians);
    const double cosLat = std::cos(latitudeRadians);

    // Inverse (transpose) of equatorial -> local ENU basis, where Heritage
    // world sky axes are x=east, y=up, z=north.
    row0 = {
        static_cast<float>(-s),
        static_cast<float>(cosLat * c),
        static_cast<float>(-sinLat * c)
    };
    row1 = {
        static_cast<float>(c),
        static_cast<float>(cosLat * s),
        static_cast<float>(-sinLat * s)
    };
    row2 = {
        0.0f,
        static_cast<float>(sinLat),
        static_cast<float>(cosLat)
    };
}

} // namespace

void resolveCelestialKeyLight(EnvironmentLighting& lighting)
{
    // CELESTIAL07: the legacy renderer can cast only one ordinary directional
    // shadow map at a time. Never invent a direction between the Sun and Moon:
    // around twilight they are often close to opposite and even a mathematically
    // continuous spherical interpolation can sweep tens of degrees per simulated
    // minute. Fade the outgoing physical source to zero, switch ownership only
    // inside a zero-direct-light bridge, then fade the incoming source up.
    const float day = std::clamp(lighting.daylightFactor, 0.0f, 1.0f);
    const float sunPower = std::max(lighting.sunIntensity, 0.0f);
    const float moonPower = std::max(
        lighting.moonIntensity * kMoonSceneIlluminationScale,
        0.0f);

    const float sunOwnership = smoothstep(0.155f, 0.195f, day);
    const float moonOwnership = 1.0f - smoothstep(0.085f, 0.125f, day);
    const float sunAltitudeSafety = smoothstep(0.005f, 0.055f, lighting.sunDirection.y);
    const float moonAltitudeSafety = smoothstep(0.010f, 0.070f, lighting.moonDirection.y);
    const float stableSunPower = sunPower * sunOwnership * sunAltitudeSafety;
    const float stableMoonPower = moonPower * moonOwnership * moonAltitudeSafety;

    if (stableSunPower > 1.0e-6f)
    {
        lighting.keyLightDirection = lighting.sunDirection;
        lighting.keyLightColor = lighting.sunColor;
        lighting.keyLightIntensity = stableSunPower;
        return;
    }
    if (stableMoonPower > 1.0e-6f)
    {
        lighting.keyLightDirection = lighting.moonDirection;
        lighting.keyLightColor = lighting.moonColor;
        lighting.keyLightIntensity = stableMoonPower;
        return;
    }

    // During the ownership bridge the direct key is intentionally zero. Ambient
    // sky/cloud transport still contains both physical celestial sources, so the
    // scene remains continuously lit without a synthetic shadow direction.
    const bool preferSunDirection = day >= 0.14f;
    lighting.keyLightDirection = preferSunDirection
        ? lighting.sunDirection : lighting.moonDirection;
    lighting.keyLightColor = preferSunDirection
        ? lighting.sunColor : lighting.moonColor;
    lighting.keyLightIntensity = 0.0f;
}

void EnvironmentSystem::reset()
{
    m_timeOfDayHours = 14.0f;
    m_secondsOfDay = 14.0 * 3600.0;
    m_timeScale = 240.0f;
    m_cycleEnabled = true;
    m_date = { 2026, 8, 24 };
    m_location = {};
    rebuildLighting();
}

void EnvironmentSystem::update(float realDeltaSeconds)
{
    if (m_cycleEnabled && std::isfinite(realDeltaSeconds) && realDeltaSeconds > 0.0f)
    {
        const float clampedDelta = std::min(realDeltaSeconds, 0.25f);
        m_secondsOfDay += static_cast<double>(clampedDelta)
            * static_cast<double>(m_timeScale);
        while (m_secondsOfDay >= 86400.0)
        {
            m_secondsOfDay -= 86400.0;
            advanceCalendarDay(1);
        }
        m_timeOfDayHours = static_cast<float>(m_secondsOfDay / 3600.0);
    }
    rebuildLighting();
}

void EnvironmentSystem::setTimeOfDayHours(float hours)
{
    if (!std::isfinite(hours))
        return;
    double wrapped = std::fmod(static_cast<double>(hours), 24.0);
    if (wrapped < 0.0)
        wrapped += 24.0;
    m_secondsOfDay = wrapped * 3600.0;
    m_timeOfDayHours = static_cast<float>(wrapped);
    rebuildLighting();
}

bool EnvironmentSystem::setDate(int year, int month, int day)
{
    if (year < 1600 || year > 9999 || month < 1 || month > 12)
        return false;
    const int maximumDay = daysInMonth(year, month);
    if (day < 1 || day > maximumDay)
        return false;
    m_date = { year, month, day };
    rebuildLighting();
    return true;
}

void EnvironmentSystem::setLocation(
    double latitudeDeg,
    double longitudeDeg,
    double elevationM,
    std::string timezone,
    int utcOffsetMinutes)
{
    if (!std::isfinite(latitudeDeg)) latitudeDeg = 0.0;
    if (!std::isfinite(longitudeDeg)) longitudeDeg = 0.0;
    if (!std::isfinite(elevationM)) elevationM = 0.0;
    m_location.latitudeDeg = std::clamp(latitudeDeg, -90.0, 90.0);
    longitudeDeg = std::fmod(longitudeDeg + 180.0, 360.0);
    if (longitudeDeg < 0.0)
        longitudeDeg += 360.0;
    m_location.longitudeDeg = longitudeDeg - 180.0;
    m_location.elevationM = std::clamp(elevationM, -500.0, 10000.0);
    m_location.timezone = timezone.empty() ? "AUTO" : std::move(timezone);
    m_location.utcOffsetMinutes = utcOffsetMinutes;
    rebuildLighting();
}

int EnvironmentSystem::effectiveUtcOffsetMinutes() const
{
    if (m_location.utcOffsetMinutes != kDerivedUtcOffset)
        return std::clamp(m_location.utcOffsetMinutes, -14 * 60, 14 * 60);

    if (m_location.timezone == "Europe/Ljubljana")
        return europeCentralDstLocal(m_date.year, m_date.month, m_date.day, m_timeOfDayHours) ? 120 : 60;
    if (m_location.timezone == "Asia/Tokyo")
        return 540;
    if (m_location.timezone == "UTC" || m_location.timezone == "Etc/UTC")
        return 0;

    // Generic scene fallback: nearest civil-time meridian. Astronomy remains
    // geographically driven by longitude; this only interprets the local clock.
    return std::clamp(
        static_cast<int>(std::lround(m_location.longitudeDeg / 15.0)) * 60,
        -12 * 60,
        14 * 60);
}

void EnvironmentSystem::setTimeScale(float simulatedSecondsPerRealSecond)
{
    if (!std::isfinite(simulatedSecondsPerRealSecond))
        return;
    m_timeScale = std::clamp(simulatedSecondsPerRealSecond, 0.0f, 86400.0f);
}

void EnvironmentSystem::advanceCalendarDay(int direction)
{
    if (direction >= 0)
    {
        ++m_date.day;
        if (m_date.day > daysInMonth(m_date.year, m_date.month))
        {
            m_date.day = 1;
            ++m_date.month;
            if (m_date.month > 12)
            {
                m_date.month = 1;
                ++m_date.year;
            }
        }
    }
    else
    {
        --m_date.day;
        if (m_date.day < 1)
        {
            --m_date.month;
            if (m_date.month < 1)
            {
                m_date.month = 12;
                --m_date.year;
            }
            m_date.day = daysInMonth(m_date.year, m_date.month);
        }
    }
}

void EnvironmentSystem::rebuildLighting()
{
    EnvironmentLighting lighting;
    lighting.timeOfDayHours = m_timeOfDayHours;

    const int utcOffsetMinutes = effectiveUtcOffsetMinutes();
    double utcHours = static_cast<double>(m_timeOfDayHours)
        - static_cast<double>(utcOffsetMinutes) / 60.0;
    int utcDayShift = 0;
    while (utcHours < 0.0) { utcHours += 24.0; --utcDayShift; }
    while (utcHours >= 24.0) { utcHours -= 24.0; ++utcDayShift; }

    // Julian-date day-shift is applied directly; a +/- one-day local/UTC
    // boundary does not need a separate temporary Gregorian date.
    const double julianDate = gregorianJulianDate(
        m_date.year, m_date.month, m_date.day, utcHours)
        + static_cast<double>(utcDayShift);
    const double latitudeRadians = m_location.latitudeDeg * kDegreesToRadians;
    const double localSiderealRadians = wrapRadians(
        greenwichMeanSiderealRadians(julianDate)
        + m_location.longitudeDeg * kDegreesToRadians);

    buildWorldToCelestialRows(
        localSiderealRadians,
        latitudeRadians,
        lighting.worldToCelestialRow0,
        lighting.worldToCelestialRow1,
        lighting.worldToCelestialRow2);

    const EquatorialCoordinates solarEq = solarEquatorial(julianDate);
    const heritage::math::Vec3 solarDirection = horizontalDirection(
        localSiderealRadians, latitudeRadians, solarEq);
    const float solarElevation = std::clamp(solarDirection.y, -1.0f, 1.0f);

    const EquatorialCoordinates lunarEq = lunarEquatorial(julianDate);
    const heritage::math::Vec3 moonDirection = horizontalDirection(
        localSiderealRadians, latitudeRadians, lunarEq);

    // CELESTIAL06: EnricoMonese/DayNightCycle-style single authority. The
    // reference implementation derives every presentation curve from one
    // clamped Sun/up dot instead of maintaining separate day/night switches.
    // Heritage keeps astronomical Sun motion, but all *state* below consumes
    // this one normalized solar-cycle value. The -0.20 minimum matches the
    // reference's default minPoint and begins the transition in twilight.
    constexpr float kCycleMinimumSolarElevation = -0.20f;
    constexpr float kCycleRange = 1.0f - kCycleMinimumSolarElevation;
    constexpr float kGeometricHorizonCycle =
        (0.0f - kCycleMinimumSolarElevation) / kCycleRange;
    const float dayNightCycle = clamp01(
        (solarElevation - kCycleMinimumSolarElevation) / kCycleRange);
    // CELESTIAL07: publish the one solar-cycle authority before resolving the
    // legacy single directional key so its physical Sun/Moon ownership bridge
    // consumes the same day/night scalar as the rest of the renderer.
    lighting.daylightFactor = dayNightCycle;

    // Gradient-like envelopes evaluated from the same cycle scalar. No
    // astronomicalDay/deepNightHold/twilightArrival secondary authorities.
    const float daylight = smoothstep(0.035f, 0.72f, dayNightCycle);
    const float twilight = smoothstep(0.025f, 0.115f, dayNightCycle)
        * (1.0f - smoothstep(0.24f, 0.46f, dayNightCycle));
    const float warmHorizon = smoothstep(0.045f, 0.125f, dayNightCycle)
        * (1.0f - smoothstep(0.19f, 0.36f, dayNightCycle));
    const float directSunVisibility = smoothstep(
        kGeometricHorizonCycle - 0.025f,
        kGeometricHorizonCycle + 0.025f,
        dayNightCycle);

    lighting.skyExposure = 0.050f
        + 0.950f * smoothstep(0.015f, 0.68f, dayNightCycle);
    lighting.atmosphereThickness = 0.87f
        + (0.40f - 0.87f) * smoothstep(0.025f, 0.74f, dayNightCycle);

    // CELESTIAL12: darker rural night reference. Keep the star field
    // legible against a near-black sky instead of a persistent navy wash.
    const heritage::math::Vec3 nightZenith{ 0.00035f, 0.00060f, 0.00180f };
    const heritage::math::Vec3 nightHorizon{ 0.00120f, 0.00180f, 0.00420f };
    const heritage::math::Vec3 twilightZenith{ 0.09f, 0.12f, 0.26f };
    const heritage::math::Vec3 twilightHorizon{ 0.75f, 0.34f, 0.16f };
    const heritage::math::Vec3 dayZenith{ 0.00f, 0.30f, 0.84f };
    const heritage::math::Vec3 dayHorizon{ 0.30f, 0.63f, 0.97f };

    lighting.skyZenith = mix(nightZenith, dayZenith, daylight);
    lighting.skyHorizon = mix(nightHorizon, dayHorizon, daylight);
    if (twilight > 0.0f)
    {
        lighting.skyZenith = mix(lighting.skyZenith, twilightZenith, twilight * 0.62f);
        lighting.skyHorizon = mix(lighting.skyHorizon, twilightHorizon, twilight * 0.92f);
    }
    if (warmHorizon > 0.0f)
    {
        lighting.skyHorizon = mix(
            lighting.skyHorizon,
            heritage::math::Vec3{ 0.98f, 0.43f, 0.16f },
            warmHorizon * 0.58f);
    }

    const heritage::math::Vec3 nightGround{ 0.0035f, 0.0040f, 0.0060f };
    const heritage::math::Vec3 dayGroundHorizon{ 0.22f, 0.22f, 0.21f };
    const heritage::math::Vec3 dayGroundNadir{ 0.060f, 0.064f, 0.060f };
    lighting.groundHorizon = mix(nightGround, dayGroundHorizon, daylight);
    lighting.groundNadir = mix(nightGround, dayGroundNadir, daylight);
    if (warmHorizon > 0.0f)
    {
        lighting.groundHorizon = mix(
            lighting.groundHorizon,
            heritage::math::Vec3{ 0.30f, 0.12f, 0.055f },
            warmHorizon * 0.24f);
        lighting.groundNadir = mix(
            lighting.groundNadir,
            heritage::math::Vec3{ 0.085f, 0.050f, 0.040f },
            warmHorizon * 0.10f);
    }

    const heritage::math::Vec3 warmSun{ 1.0f, 0.44f, 0.20f };
    const heritage::math::Vec3 daySun{ 1.0f, 0.96f, 0.88f };
    lighting.sunDirection = solarDirection;
    lighting.sunColor = mix(warmSun, daySun, smoothstep(0.18f, 0.58f, dayNightCycle));
    lighting.sunIntensity = directSunVisibility
        * (0.18f + (3.40f - 0.18f) * smoothstep(0.17f, 0.78f, dayNightCycle));

    const float moonVisibility = smoothstep(-0.16f, 0.08f, moonDirection.y);
    lighting.moonDirection = moonDirection;
    lighting.moonColor = { 0.62f, 0.68f, 0.78f };
    lighting.moonPhase = 0.5f;
    lighting.moonIntensity = 0.54f * moonVisibility
        * (1.0f - smoothstep(0.10f, 0.30f, dayNightCycle));

    resolveCelestialKeyLight(lighting);

    // Keep the visible full Moon unchanged, but its diffuse/ambient lift now
    // fades continuously with actual Moon visibility/daylight.  The previous
    // branch applied the full lift and then removed it abruptly at the
    // Sun/Moon key-light threshold, creating another dawn discontinuity.
    const float moonIllumination01 = clamp01(lighting.moonIntensity / 0.54f);
    // Keep the visible Moon and its local halo, but do not let a full
    // Moon repaint the entire sky dome bright blue. The global lunar sky
    // lift is deliberately tiny so the overall night remains predominantly
    // black like the rural reference image.
    const float moonSkyLift = 0.09f * kMoonSceneIlluminationScale * moonIllumination01;
    if (moonSkyLift > 0.0001f)
    {
        lighting.skyZenith = mix(lighting.skyZenith, heritage::math::Vec3{ 0.008f, 0.010f, 0.018f }, moonSkyLift);
        lighting.skyHorizon = mix(lighting.skyHorizon, heritage::math::Vec3{ 0.010f, 0.012f, 0.020f }, moonSkyLift);
        lighting.groundHorizon = mix(lighting.groundHorizon, heritage::math::Vec3{ 0.006f, 0.007f, 0.010f }, moonSkyLift * 0.54f);
        lighting.groundNadir = mix(lighting.groundNadir, heritage::math::Vec3{ 0.005f, 0.006f, 0.009f }, moonSkyLift * 0.42f);
    }

    // The NASA HDR star map is always geometrically present. CELESTIAL06
    // evaluates its fade from the same cycle scalar as every other day/night
    // effect. Bright stars begin appearing while the Sun is still near the
    // orange horizon, then build continuously through civil twilight.
    lighting.starIntensity = 1.0f - smoothstep(0.050f, 0.190f, dayNightCycle);

    m_lighting = lighting;
}

} // namespace heritage::graphics
