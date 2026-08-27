#pragma once

#include <cstdint>
#include <unordered_map>

#include "VehicleAudioTypes.hpp"
#include "../../Vehicles/VehicleSystem.hpp"

namespace heritage::physics { class PhysicsWorld; }

namespace heritage::audio::vehicles {

// Module-owned bridge between authoritative vehicle telemetry and the audio
// graph. Physics never depends on sound; audio consumes immutable snapshots.
class VehicleAudioRuntime
{
public:
    VehicleAudioRuntime(AudioSystem& audio, heritage::physics::PhysicsWorld& physics);
    ~VehicleAudioRuntime();

    VehicleAudioRuntime(const VehicleAudioRuntime&) = delete;
    VehicleAudioRuntime& operator=(const VehicleAudioRuntime&) = delete;

    VehicleSoundHandle create(
        heritage::vehicles::VehicleHandle vehicle,
        const VehicleAudioDefinition& definition);
    bool destroy(VehicleSoundHandle handle);
    void clear();
    void update(float deltaSeconds);

    bool setEnabled(VehicleSoundHandle handle, bool enabled);
    bool telemetry(VehicleSoundHandle handle, VehicleAudioTelemetry& value) const;

private:
    struct Instance;
    void updateInstance(Instance& instance, float deltaSeconds);
    void stopLayers(Instance& instance);

    AudioSystem* m_audio = nullptr;
    heritage::physics::PhysicsWorld* m_physics = nullptr;
    VehicleSoundHandle m_nextHandle = 1;
    std::unordered_map<VehicleSoundHandle, Instance> m_instances;
    int m_transientVoiceSlots = 0;
};

} // namespace heritage::audio::vehicles
