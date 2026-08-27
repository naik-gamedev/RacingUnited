#pragma once

#include <unordered_map>

#include "WeatherAudioTypes.hpp"
#include "Models/WeatherAudioModel.hpp"
#include "../AudioSystem.hpp"

namespace heritage::physics { class PhysicsWorld; }

namespace heritage::audio::weather {

// A module-owned weather ambience graph. It consumes SurfaceWorld weather as
// immutable authority and never feeds values back into weather or physics.
class WeatherAudioRuntime
{
public:
    WeatherAudioRuntime(AudioSystem& audio, heritage::physics::PhysicsWorld& physics);
    ~WeatherAudioRuntime();

    WeatherAudioRuntime(const WeatherAudioRuntime&) = delete;
    WeatherAudioRuntime& operator=(const WeatherAudioRuntime&) = delete;

    WeatherSoundHandle create(const WeatherAudioDefinition& definition);
    bool destroy(WeatherSoundHandle handle);
    void clear();
    void update(float deltaSeconds);
    bool setEnabled(WeatherSoundHandle handle, bool enabled);
    bool telemetry(WeatherSoundHandle handle, WeatherAudioTelemetry& value) const;

private:
    struct Instance;
    void stop(Instance& instance);

    AudioSystem* m_audio = nullptr;
    heritage::physics::PhysicsWorld* m_physics = nullptr;
    WeatherSoundHandle m_nextHandle = 1;
    std::unordered_map<WeatherSoundHandle, Instance> m_instances;
};

} // namespace heritage::audio::weather
