#pragma once

namespace heritage::audio::vehicles {

struct VehicleAudioEventState
{
    bool initialized = false;
    int previousGear = 0;
    float previousSuspensionActivity = 0.0f;
    float suspensionCooldownSeconds = 0.0f;
    float limiterCooldownSeconds = 0.0f;
    float overrunCooldownSeconds = 0.0f;
    float previousEngineTorqueNm = 0.0f;
};

struct VehicleAudioEventInput
{
    int gear = 0;
    float suspensionActivity = 0.0f;
    float deltaSeconds = 0.0f;
    float engineRpm = 0.0f;
    float engineTorqueNm = 0.0f;
    float redlineRpm = 0.0f;
};

struct VehicleAudioEventFrame
{
    bool gearShift = false;
    bool suspensionLight = false;
    bool suspensionHeavy = false;
    bool revLimiterCut = false;
    bool overrunPop = false;
};

// Advances deterministic event history and reports rising-edge events. The
// model owns no voices and performs no playback, which makes its thresholds
// repeatable and independently testable.
VehicleAudioEventFrame evaluateVehicleAudioEvents(
    const VehicleAudioEventInput& input,
    VehicleAudioEventState& state);

} // namespace heritage::audio::vehicles
