#include "VehicleAudioFleetBudgetModel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::audio::vehicles {

VehicleAudioDetailLevel distanceBasedVehicleAudioDetail(
    const VehicleAudioDefinition& definition,
    float distanceMeters)
{
    const float distance = std::isfinite(distanceMeters)
        ? std::max(distanceMeters, 0.0f)
        : (std::numeric_limits<float>::max)();
    if (distance <= definition.fullDetailDistanceMeters)
        return VehicleAudioDetailLevel::Full;
    if (distance <= definition.reducedDetailDistanceMeters)
        return VehicleAudioDetailLevel::Reduced;
    if (distance <= definition.maximumDistanceMeters)
        return VehicleAudioDetailLevel::Crowd;
    return VehicleAudioDetailLevel::Silent;
}

int estimatedVehicleAudioVoiceCost(VehicleAudioDetailLevel detail)
{
    switch (detail)
    {
    case VehicleAudioDetailLevel::Full: return 9;    // seven model + two RPM loops
    case VehicleAudioDetailLevel::Reduced: return 4; // three model + one RPM loop
    case VehicleAudioDetailLevel::Crowd: return 1;
    case VehicleAudioDetailLevel::Silent: return 0;
    }
    return 0;
}

std::vector<VehicleAudioFleetAssignment> allocateVehicleAudioFleet(
    std::vector<VehicleAudioFleetCandidate> candidates,
    const VehicleAudioFleetBudget& budget)
{
    std::sort(candidates.begin(), candidates.end(),
        [](const VehicleAudioFleetCandidate& left,
           const VehicleAudioFleetCandidate& right)
        {
            const float leftDistance = std::isfinite(left.distanceMeters)
                ? left.distanceMeters : (std::numeric_limits<float>::max)();
            const float rightDistance = std::isfinite(right.distanceMeters)
                ? right.distanceMeters : (std::numeric_limits<float>::max)();
            if (leftDistance != rightDistance)
                return leftDistance < rightDistance;
            return left.handle < right.handle;
        });

    const int voiceLimit = std::max(budget.maximumContinuousVoices, 0);
    const int fullLimit = std::max(budget.maximumFullVehicles, 0);
    const int reducedLimit = std::max(budget.maximumReducedVehicles, 0);
    const int crowdLimit = std::max(budget.maximumCrowdVehicles, 0);
    int voices = 0;
    int fullCount = 0;
    int reducedCount = 0;
    int crowdCount = 0;
    std::vector<VehicleAudioFleetAssignment> assignments;
    assignments.reserve(candidates.size());

    for (const VehicleAudioFleetCandidate& candidate : candidates)
    {
        VehicleAudioDetailLevel detail = candidate.requestedDetail;
        while (detail != VehicleAudioDetailLevel::Silent)
        {
            const bool countAvailable =
                (detail == VehicleAudioDetailLevel::Full && fullCount < fullLimit)
                || (detail == VehicleAudioDetailLevel::Reduced
                    && reducedCount < reducedLimit)
                || (detail == VehicleAudioDetailLevel::Crowd
                    && crowdCount < crowdLimit);
            const int cost = estimatedVehicleAudioVoiceCost(detail);
            if (countAvailable && voices + cost <= voiceLimit)
                break;
            detail = detail == VehicleAudioDetailLevel::Full
                ? VehicleAudioDetailLevel::Reduced
                : (detail == VehicleAudioDetailLevel::Reduced
                    ? VehicleAudioDetailLevel::Crowd
                    : VehicleAudioDetailLevel::Silent);
        }

        const int cost = estimatedVehicleAudioVoiceCost(detail);
        voices += cost;
        if (detail == VehicleAudioDetailLevel::Full) ++fullCount;
        else if (detail == VehicleAudioDetailLevel::Reduced) ++reducedCount;
        else if (detail == VehicleAudioDetailLevel::Crowd) ++crowdCount;
        assignments.push_back({ candidate.handle, detail, cost });
    }
    return assignments;
}

} // namespace heritage::audio::vehicles
