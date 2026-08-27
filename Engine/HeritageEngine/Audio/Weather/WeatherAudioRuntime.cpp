#include "WeatherAudioRuntime.hpp"

#include "../../Physics/PhysicsWorld.hpp"

#include <algorithm>
#include <array>

namespace heritage::audio::weather {

struct WeatherAudioRuntime::Instance
{
    WeatherAudioDefinition definition;
    bool enabled = true;
    AudioHandle lightRain = kInvalidAudioHandle;
    AudioHandle mediumRain = kInvalidAudioHandle;
    AudioHandle heavyRain = kInvalidAudioHandle;
    AudioHandle stormRain = kInvalidAudioHandle;
    AudioHandle wind = kInvalidAudioHandle;
    WeatherAudioModelState model;
    WeatherAudioTelemetry telemetry;
};

WeatherAudioRuntime::WeatherAudioRuntime(
    AudioSystem& audio,
    heritage::physics::PhysicsWorld& physics)
    : m_audio(&audio), m_physics(&physics)
{
}

WeatherAudioRuntime::~WeatherAudioRuntime()
{
    clear();
}

WeatherSoundHandle WeatherAudioRuntime::create(
    const WeatherAudioDefinition& definition)
{
    if (!m_audio || !m_audio->isAvailable() || !m_physics)
        return kInvalidWeatherSoundHandle;

    Instance instance;
    instance.definition = definition;
    const float rainGain = std::clamp(definition.rainGain, 0.0f, 2.0f);
    const float windGain = std::clamp(definition.windGain, 0.0f, 2.0f);
    const auto start = [&](const std::filesystem::path& path, float gain)
    {
        return path.empty()
            ? kInvalidAudioHandle
            : m_audio->playLoop(path, AudioBus::Ambience, gain, 1.0f);
    };
    instance.lightRain = start(definition.lightRainPath, 0.0f * rainGain);
    instance.mediumRain = start(definition.mediumRainPath, 0.0f * rainGain);
    instance.heavyRain = start(definition.heavyRainPath, 0.0f * rainGain);
    instance.stormRain = start(definition.stormRainPath, 0.0f * rainGain);
    instance.wind = start(definition.windPath, 0.0f * windGain);

    if (instance.lightRain == kInvalidAudioHandle
        && instance.mediumRain == kInvalidAudioHandle
        && instance.heavyRain == kInvalidAudioHandle
        && instance.stormRain == kInvalidAudioHandle
        && instance.wind == kInvalidAudioHandle)
        return kInvalidWeatherSoundHandle;

    const WeatherSoundHandle handle = m_nextHandle++;
    m_instances.emplace(handle, std::move(instance));
    return handle;
}

bool WeatherAudioRuntime::destroy(WeatherSoundHandle handle)
{
    const auto found = m_instances.find(handle);
    if (found == m_instances.end())
        return false;
    stop(found->second);
    m_instances.erase(found);
    return true;
}

void WeatherAudioRuntime::clear()
{
    for (auto& [handle, instance] : m_instances)
    {
        (void)handle;
        stop(instance);
    }
    m_instances.clear();
    m_nextHandle = 1;
}

void WeatherAudioRuntime::update(float deltaSeconds)
{
    if (!m_audio || !m_physics)
        return;
    const auto weather = m_physics->surfaces().weather();

    for (auto& [handle, instance] : m_instances)
    {
        (void)handle;
        const WeatherAudioModelInput input{
            instance.enabled && weather.enabled,
            static_cast<float>(weather.precipitationRateMmPerHour),
            static_cast<float>(weather.windSpeedMps),
            deltaSeconds
        };
        const WeatherAudioMix mix = evaluateWeatherAudio(input, instance.model);
        const float rainGain = std::clamp(instance.definition.rainGain, 0.0f, 2.0f);
        const float windGain = std::clamp(instance.definition.windGain, 0.0f, 2.0f);
        const auto volume = [&](AudioHandle voice, float value)
        {
            if (voice != kInvalidAudioHandle)
                m_audio->setHandleVolume(voice, value);
        };
        volume(instance.lightRain, mix.lightRain * rainGain);
        volume(instance.mediumRain, mix.mediumRain * rainGain);
        volume(instance.heavyRain, mix.heavyRain * rainGain);
        volume(instance.stormRain, mix.stormRain * rainGain);
        volume(instance.wind, mix.wind * windGain);
        if (instance.wind != kInvalidAudioHandle)
            m_audio->setHandlePitch(instance.wind, mix.windPitch);

        instance.telemetry.enabled = instance.enabled && weather.enabled;
        instance.telemetry.rainMmPerHour = input.rainMmPerHour;
        instance.telemetry.windMetersPerSecond = input.windMetersPerSecond;
        instance.telemetry.lightRainGain = mix.lightRain * rainGain;
        instance.telemetry.mediumRainGain = mix.mediumRain * rainGain;
        instance.telemetry.heavyRainGain = mix.heavyRain * rainGain;
        instance.telemetry.stormRainGain = mix.stormRain * rainGain;
        instance.telemetry.windGain = mix.wind * windGain;
        const std::array voices{
            instance.lightRain, instance.mediumRain, instance.heavyRain,
            instance.stormRain, instance.wind };
        instance.telemetry.activeVoiceCount = static_cast<int>(std::count_if(
            voices.begin(), voices.end(), [&](AudioHandle voice)
            {
                return voice != kInvalidAudioHandle && m_audio->isPlaying(voice);
            }));
    }
}

bool WeatherAudioRuntime::setEnabled(WeatherSoundHandle handle, bool enabled)
{
    const auto found = m_instances.find(handle);
    if (found == m_instances.end())
        return false;
    found->second.enabled = enabled;
    return true;
}

bool WeatherAudioRuntime::telemetry(
    WeatherSoundHandle handle,
    WeatherAudioTelemetry& value) const
{
    const auto found = m_instances.find(handle);
    if (found == m_instances.end())
        return false;
    value = found->second.telemetry;
    return true;
}

void WeatherAudioRuntime::stop(Instance& instance)
{
    if (!m_audio)
        return;
    const std::array voices{
        instance.lightRain, instance.mediumRain, instance.heavyRain,
        instance.stormRain, instance.wind };
    for (const AudioHandle voice : voices)
    {
        if (voice != kInvalidAudioHandle)
            m_audio->stop(voice);
    }
}

} // namespace heritage::audio::weather
