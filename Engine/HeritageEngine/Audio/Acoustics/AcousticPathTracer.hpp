#pragma once

#include "../AudioSystem.hpp"
#include "../../Physics/CollisionSystem.hpp"

namespace heritage::audio::acoustics {

struct AcousticPathTraceInput
{
    AudioVector3 source;
    AudioVector3 listener;
    heritage::physics::BodyHandle ignoredEmitterBody =
        heritage::physics::InvalidBody;
    float maximumDistanceMeters = 400.0f;
};

struct AcousticPathTraceResult
{
    bool directOccluded = false;
    float directGain = 1.0f;
    float directOpenness = 1.0f;
    float earlyReflectionGain = 0.0f;
    float earlyReflectionDelaySeconds = 0.0f;
    float lateReverbGain = 0.0f;
    int validReflectionPathCount = 0;
    int tracedRayCount = 0;
};

// A bounded deterministic geometric-acoustics solver. It traces a direct path
// and image-source, one-bounce specular paths against the authoritative
// collision scene. It intentionally does not pretend to solve diffraction or
// full wave acoustics; those can be added behind the same result contract.
class AcousticPathTracer
{
public:
    static AcousticPathTraceResult trace(
        const AcousticPathTraceInput& input,
        const heritage::physics::CollisionSystem& collisions,
        const heritage::physics::RigidBodySystem& bodies);
};

} // namespace heritage::audio::acoustics
