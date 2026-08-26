#pragma once

#include "../Core/Math/Math.hpp"

namespace heritage::graphics {

// CLOUDURP15Q: one compact, renderer-facing description of the atmosphere
// used by the fullscreen aerial-perspective composite.  EnvironmentSystem
// remains the astronomical/sky authority; EntityMeshRenderer only snapshots
// the already-resolved lighting + local weather state for post processing.
struct AerialPerspectiveState
{
    bool enabled = true;

    heritage::math::Vec3 horizonColor{ 0.10f, 0.15f, 0.24f };
    heritage::math::Vec3 zenithColor{ 0.02f, 0.08f, 0.24f };
    heritage::math::Vec3 sunDirection{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 sunColor{ 1.0f, 0.96f, 0.88f };

    // Sea-level extinction coefficient. The post shader applies an altitude
    // density falloff and a small horizon-path correction on top of this.
    float extinctionPerMeter = 0.000045f;
    float daylightFactor = 1.0f;
    float sunIntensity = 1.0f;
    float atmosphereThickness = 0.40f;
    float relativeHumidity = 0.55f;
    float cloudCover = 0.0f;
    float precipitation01 = 0.0f;
    float cameraAltitudeM = 0.0f;
    float skyExposure = 1.0f;

    // The scene texture is already display-referred by Heritage's material and
    // sky shaders. These values let the post pass transform atmospheric source
    // colours into the same display space before compositing them over geometry.
    float gamma = 2.2f;
    float brightness = 0.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
};

} // namespace heritage::graphics
