#pragma once

#include "../Core/Math/Math.hpp"

namespace heritage::graphics {

struct EnvironmentLighting
{
    float timeOfDayHours = 14.0f;
    heritage::math::Vec3 sunDirection{ -0.35f, 0.86f, 0.38f };
    heritage::math::Vec3 sunColor{ 1.0f, 0.96f, 0.88f };
    float sunIntensity = 1.0f;
    heritage::math::Vec3 skyHorizon{ 0.58f, 0.68f, 0.78f };
    heritage::math::Vec3 skyZenith{ 0.16f, 0.34f, 0.62f };
    heritage::math::Vec3 groundHorizon{ 0.30f, 0.29f, 0.25f };
    heritage::math::Vec3 groundNadir{ 0.085f, 0.10f, 0.075f };
    float starIntensity = 0.0f;
    float daylightFactor = 1.0f;
};

// Renderer-independent time-of-day state shared by the runtime and graphics.
//
// timeScale is expressed as simulated seconds per real second:
//   1     = real-time day/night progression
//   60    = one simulated minute per real second
//   240   = a full day in six real minutes
//   1440  = a full day in one real minute
class EnvironmentSystem
{
public:
    void reset();
    void update(float realDeltaSeconds);

    void setTimeOfDayHours(float hours);
    float timeOfDayHours() const { return m_timeOfDayHours; }

    void setCycleEnabled(bool enabled) { m_cycleEnabled = enabled; }
    bool cycleEnabled() const { return m_cycleEnabled; }

    void setTimeScale(float simulatedSecondsPerRealSecond);
    float timeScale() const { return m_timeScale; }

    const EnvironmentLighting& lighting() const { return m_lighting; }

private:
    void rebuildLighting();

    float m_timeOfDayHours = 14.0f;
    float m_timeScale = 240.0f;
    bool m_cycleEnabled = true;
    EnvironmentLighting m_lighting;
};

} // namespace heritage::graphics
