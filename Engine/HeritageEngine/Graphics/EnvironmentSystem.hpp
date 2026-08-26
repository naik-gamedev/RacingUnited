#pragma once

#include <string>

#include "../Core/Math/Math.hpp"

namespace heritage::graphics {

// Visual Moon-disc strength remains independent from scene illumination.
// CLOUDURP15L2 halves the Moon's contribution to world/cloud lighting
// without dimming Moon.png itself.
inline constexpr float kMoonSceneIlluminationScale = 0.204f;

struct EnvironmentCalendarDate
{
    int year = 2026;
    int month = 8;
    int day = 24;
};

struct EnvironmentLocation
{
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double elevationM = 0.0;
    std::string timezone = "UTC";
    // INT32_MIN means derive a reasonable civil-time offset from timezone/name
    // or longitude. Scene metadata may explicitly author this when desired.
    int utcOffsetMinutes = -2147483647 - 1;
};

struct EnvironmentLighting
{
    float timeOfDayHours = 14.0f;
    heritage::math::Vec3 sunDirection{ -0.35f, 0.86f, 0.38f };
    heritage::math::Vec3 sunColor{ 1.0f, 0.96f, 0.88f };
    float sunIntensity = 1.0f;
    heritage::math::Vec3 moonDirection{ 0.35f, 0.42f, -0.84f };
    heritage::math::Vec3 moonColor{ 0.62f, 0.68f, 0.78f };
    float moonIntensity = 0.0f;
    float moonPhase = 0.5f;
    heritage::math::Vec3 keyLightDirection{ -0.35f, 0.86f, 0.38f };
    heritage::math::Vec3 keyLightColor{ 1.0f, 0.96f, 0.88f };
    float keyLightIntensity = 1.0f;
    heritage::math::Vec3 skyHorizon{ 0.58f, 0.68f, 0.78f };
    heritage::math::Vec3 skyZenith{ 0.16f, 0.34f, 0.62f };
    heritage::math::Vec3 groundHorizon{ 0.30f, 0.29f, 0.25f };
    heritage::math::Vec3 groundNadir{ 0.085f, 0.10f, 0.075f };
    float starIntensity = 0.0f;
    float daylightFactor = 1.0f;
    float skyExposure = 1.0f;
    float atmosphereThickness = 0.40f;

    // Rows transforming Heritage local sky direction (east, up, north in
    // x/y/z) into the J2000-like equatorial Cartesian frame sampled by the
    // NASA plate-carree star map. The shader uses dot(row, direction).
    heritage::math::Vec3 worldToCelestialRow0{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 worldToCelestialRow1{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 worldToCelestialRow2{ 0.0f, 0.0f, 1.0f };
};

// Resolve the renderer's single directional celestial key from the physically
// overlapping Sun and Moon contributions. Keeping this in the astronomical
// environment authority prevents individual render paths from reintroducing
// hard dawn/dusk ownership switches.
void resolveCelestialKeyLight(EnvironmentLighting& lighting);

// Single authoritative environment clock + astronomy state.
// Local scene date/time and geographic location determine the sun, moon and
// celestial-sphere orientation. Scene rendering never owns a second hidden
// day/night cycle.
class EnvironmentSystem
{
public:
    void reset();
    void update(float realDeltaSeconds);

    void setTimeOfDayHours(float hours);
    float timeOfDayHours() const { return m_timeOfDayHours; }

    bool setDate(int year, int month, int day);
    const EnvironmentCalendarDate& date() const { return m_date; }

    void setLocation(
        double latitudeDeg,
        double longitudeDeg,
        double elevationM,
        std::string timezone = {},
        int utcOffsetMinutes = -2147483647 - 1);
    const EnvironmentLocation& location() const { return m_location; }
    int effectiveUtcOffsetMinutes() const;

    void setCycleEnabled(bool enabled) { m_cycleEnabled = enabled; }
    bool cycleEnabled() const { return m_cycleEnabled; }

    void setTimeScale(float simulatedSecondsPerRealSecond);
    float timeScale() const { return m_timeScale; }

    const EnvironmentLighting& lighting() const { return m_lighting; }

private:
    void rebuildLighting();
    void advanceCalendarDay(int direction);

    float m_timeOfDayHours = 14.0f;
    double m_secondsOfDay = 14.0 * 3600.0;
    float m_timeScale = 240.0f;
    bool m_cycleEnabled = true;
    EnvironmentCalendarDate m_date;
    EnvironmentLocation m_location;
    EnvironmentLighting m_lighting;
};

} // namespace heritage::graphics
