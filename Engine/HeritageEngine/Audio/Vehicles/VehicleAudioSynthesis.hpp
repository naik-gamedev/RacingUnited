#pragma once

#include "VehicleAudioTypes.hpp"

namespace heritage::audio::vehicles {

enum class SynthesizedVehicleLayer
{
    Exhaust,
    Intake,
    Mechanical,
    Transmission,
    Tire,
    Wind,
    Chassis
};

enum class SynthesizedVehicleTransient
{
    RevLimiterCut,
    OverrunPop
};

GeneratedMonoAudio synthesizeVehicleLayer(
    const VehicleAudioDefinition& definition,
    SynthesizedVehicleLayer layer);

GeneratedMonoAudio synthesizeVehicleTransient(
    const VehicleAudioDefinition& definition,
    SynthesizedVehicleTransient transient);

float signalPeak(const GeneratedMonoAudio& audio);
float signalRms(const GeneratedMonoAudio& audio);

} // namespace heritage::audio::vehicles
