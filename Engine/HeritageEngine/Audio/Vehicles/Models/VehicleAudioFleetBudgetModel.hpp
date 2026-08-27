#pragma once

#include <vector>

#include "../VehicleAudioTypes.hpp"

namespace heritage::audio::vehicles {

struct VehicleAudioFleetCandidate
{
    VehicleSoundHandle handle = kInvalidVehicleSoundHandle;
    float distanceMeters = 0.0f;
    VehicleAudioDetailLevel requestedDetail = VehicleAudioDetailLevel::Silent;
};

struct VehicleAudioFleetAssignment
{
    VehicleSoundHandle handle = kInvalidVehicleSoundHandle;
    VehicleAudioDetailLevel detail = VehicleAudioDetailLevel::Silent;
    int estimatedContinuousVoices = 0;
};

struct VehicleAudioFleetBudget
{
    int maximumContinuousVoices = 192;
    int maximumFullVehicles = 8;
    int maximumReducedVehicles = 24;
    int maximumCrowdVehicles = 48;
    int maximumTransientVoices = 48;
};

VehicleAudioDetailLevel distanceBasedVehicleAudioDetail(
    const VehicleAudioDefinition& definition,
    float distanceMeters);

int estimatedVehicleAudioVoiceCost(VehicleAudioDetailLevel detail);

// Returns deterministic nearest-first assignments. Requested detail is an
// upper bound; candidates degrade through reduced/crowd/silent as fleet caps
// are reached rather than exceeding the native voice budget.
std::vector<VehicleAudioFleetAssignment> allocateVehicleAudioFleet(
    std::vector<VehicleAudioFleetCandidate> candidates,
    const VehicleAudioFleetBudget& budget = {});

} // namespace heritage::audio::vehicles
