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

float dot(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
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

heritage::math::Vec3 sphericalMix(
    const heritage::math::Vec3& aValue,
    const heritage::math::Vec3& bValue,
    float t)
{
    const heritage::math::Vec3 a = normalize(aValue);
    const heritage::math::Vec3 b = normalize(bValue);
    t = clamp01(t);
    const float cosine = std::clamp(dot(a, b), -1.0f, 1.0f);

    // Ordinary directions use a true great-circle interpolation.  This is
    // important for dawn/dusk: linearly blending a Moon direction and an
    // almost-opposite Sun direction can collapse toward a zero vector and then
    // flip hemispheres as the power ratio crosses 50/50.
    if (cosine > -0.9990f)
    {
        const float angle = std::acos(cosine);
        if (angle <= 1.0e-5f)
            return normalize(mix(a, b, t));
        const float sine = std::sin(angle);
        const float wa = std::sin((1.0f - t) * angle) / sine;
        const float wb = std::sin(t * angle) / sine;
        return normalize(add(multiply(a, wa), multiply(b, wb)));
    }

    // Exact/near antipodes have no unique great circle. Pick a stable axis so
    // the key light still rotates continuously instead of disappearing.
    heritage::math::Vec3 reference = std::abs(a.y) < 0.90f
        ? heritage::math::Vec3{ 0.0f, 1.0f, 0.0f }
        : heritage::math::Vec3{ 1.0f, 0.0f, 0.0f };
    const heritage::math::Vec3 axis = normalize(cross(a, reference));
    const float angle = static_cast<float>(kPi) * t;
    const float c = std::cos(angle);
    const float si = std::sin(angle);
    return normalize(add(multiply(a, c), multiply(cross(axis, a), si)));
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
    const float sunPower = std::max(lighting.sunIntensity, 0.0f);
    const float moonPower = std::max(
        lighting.moonIntensity * kMoonSceneIlluminationScale,
        0.0f);
    const float totalPower = sunPower + moonPower;

    if (totalPower <= 1.0e-6f)
    {
        lighting.keyLightDirection = lighting.sunDirection;
        lighting.keyLightColor = lighting.sunColor;
        lighting.keyLightIntensity = 0.0f;
        return;
    }

    // CLOUDURP15M: dawn/dusk continuity.  The old renderer made a binary
    // Moon -> Sun handoff at unrelated thresholds.  Around dawn that could
    // replace a still-strong moon key with a much weaker newborn sun, making
    // the whole scene suddenly go dark before sunrise recovered.  Blend the
    // celestial ownership from their actual scene powers instead.
    const float sunShare = sunPower / totalPower;
    const float sunBlend = smoothstep(0.18f, 0.82f, sunShare);

    // CLOUDURP15N: never linearly interpolate celestial directions. Around
    // sunrise/sunset the Sun and Moon can be almost opposite; normalized lerp
    // then approaches zero and can swing/flip the scene light during the very
    // period where the user is watching the transition. Great-circle mixing
    // keeps the single legacy key-light direction continuous. Volumetric
    // clouds below no longer use this synthetic key at all: they receive the
    // physical Sun and Moon channels separately.
    lighting.keyLightDirection = sphericalMix(
        lighting.moonDirection, lighting.sunDirection, sunBlend);
    lighting.keyLightColor = mix(lighting.moonColor, lighting.sunColor, sunBlend);

    // Add the two celestial contributions instead of choosing one.  There is
    // still only one directional renderer light, but its energy cannot form
    // the artificial pre-sunrise trough caused by the old hard handoff.
    lighting.keyLightIntensity = totalPower;
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

    const float daylight = smoothstep(-0.09f, 0.12f, solarElevation);

    // CLOUDURP15N: one continuous low-Sun warmth envelope. Earlier revisions
    // used separate twilight and post-horizon bands with different endpoints;
    // at accelerated time scale their overlap could read as blue -> warm ->
    // blue -> warm pulses even though every individual curve was continuous.
    // This envelope rises during astronomical/civil twilight, stays strong
    // through the horizon crossing, then decays gradually as the Sun climbs.
    const float twilightArrival = smoothstep(-0.30f, -0.08f, solarElevation);
    const float twilightDeparture = 1.0f - smoothstep(0.10f, 0.42f, solarElevation);
    const float twilight = twilightArrival * twilightDeparture;
    const float warmHorizon = smoothstep(-0.20f, -0.035f, solarElevation)
        * (1.0f - smoothstep(0.08f, 0.45f, solarElevation));

    const float astronomicalDay = smoothstep(-0.08f, 0.16f, solarElevation);
    const float deepNightHold = 1.0f - smoothstep(-0.16f, -0.03f, solarElevation);
    const float twilightExposure = smoothstep(-0.20f, 0.08f, solarElevation);
    const float uncappedSkyExposure = std::clamp(
        0.045f + 0.955f * astronomicalDay + 0.12f * twilightExposure * (1.0f - astronomicalDay),
        0.045f, 1.0f);

    // CLOUDURP15M: the former `if (deepNight > 0)` held exposure at 0.055
    // until solar elevation crossed exactly -0.03, then released it to about
    // 0.22 in one frame.  Blend the night hold out continuously instead.
    const float nightHeldExposure = std::min(uncappedSkyExposure, 0.055f);
    lighting.skyExposure = nightHeldExposure
        + (uncappedSkyExposure - nightHeldExposure) * (1.0f - deepNightHold);
    lighting.atmosphereThickness = 0.87f + (0.40f - 0.87f) * astronomicalDay;

    const heritage::math::Vec3 nightZenith{ 0.0030f, 0.0060f, 0.020f };
    const heritage::math::Vec3 nightHorizon{ 0.010f, 0.016f, 0.038f };
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

    const heritage::math::Vec3 nightGround{ 0.0120f, 0.0140f, 0.020f };
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
    lighting.sunColor = mix(warmSun, daySun, smoothstep(0.02f, 0.42f, solarElevation));
    lighting.sunIntensity = daylight * (0.18f + (3.40f - 0.18f) * smoothstep(0.0f, 0.70f, solarElevation));

    const float moonVisibility = smoothstep(-0.16f, 0.08f, moonDirection.y);
    lighting.moonDirection = moonDirection;
    lighting.moonColor = { 0.62f, 0.68f, 0.78f };
    lighting.moonPhase = 0.5f;
    lighting.moonIntensity = 0.54f * moonVisibility * (1.0f - daylight);

    resolveCelestialKeyLight(lighting);

    // Keep the visible full Moon unchanged, but its diffuse/ambient lift now
    // fades continuously with actual Moon visibility/daylight.  The previous
    // branch applied the full lift and then removed it abruptly at the
    // Sun/Moon key-light threshold, creating another dawn discontinuity.
    const float moonIllumination01 = clamp01(lighting.moonIntensity / 0.54f);
    const float moonSkyLift = 0.48f * kMoonSceneIlluminationScale * moonIllumination01;
    if (moonSkyLift > 0.0001f)
    {
        lighting.skyZenith = mix(lighting.skyZenith, heritage::math::Vec3{ 0.030f, 0.040f, 0.080f }, moonSkyLift);
        lighting.skyHorizon = mix(lighting.skyHorizon, heritage::math::Vec3{ 0.036f, 0.042f, 0.080f }, moonSkyLift);
        lighting.groundHorizon = mix(lighting.groundHorizon, heritage::math::Vec3{ 0.016f, 0.018f, 0.025f }, moonSkyLift * 0.78f);
        lighting.groundNadir = mix(lighting.groundNadir, heritage::math::Vec3{ 0.014f, 0.016f, 0.022f }, moonSkyLift * 0.66f);
    }

    // The NASA HDR star map is always geometrically present. Atmospheric
    // daylight controls visibility continuously; there is no procedural pixel
    // star field and no binary on/off transition at sunset.
    lighting.starIntensity = 1.0f - smoothstep(-0.31f, -0.10f, solarElevation);
    lighting.daylightFactor = daylight;

    m_lighting = lighting;
}

} // namespace heritage::graphics
