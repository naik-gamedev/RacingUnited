#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "../Core/Settings/AudioSettings.hpp"

namespace heritage::audio {

using AudioHandle = std::uint64_t;
inline constexpr AudioHandle kInvalidAudioHandle = 0;

enum class AudioBus
{
    Music,
    Effects,
    Ambience,
    UI,
    Voice
};

const char* audioBusName(AudioBus bus);
std::optional<AudioBus> parseAudioBus(const std::string& name);

// Native audio service used by Heritage Engine and exposed to module Lua.
//
// The current Windows backend uses XAudio2 and intentionally begins with
// uncompressed PCM/IEEE-float WAV files. The public API is platform-neutral so
// another backend can be added later without changing module scripts.
class AudioSystem
{
public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool initialize(std::string& message);
    void shutdown();
    void update(bool applicationFocused);

    bool isAvailable() const;
    std::string backendName() const;
    const std::string& lastError() const;

    AudioHandle playOneShot(
        const std::filesystem::path& path,
        AudioBus bus = AudioBus::Effects,
        float volume = 1.0f,
        float pitch = 1.0f);

    AudioHandle playLoop(
        const std::filesystem::path& path,
        AudioBus bus = AudioBus::Ambience,
        float volume = 1.0f,
        float pitch = 1.0f);

    bool stop(AudioHandle handle);
    void stopAll();
    void stopBus(AudioBus bus);
    bool isPlaying(AudioHandle handle) const;

    bool setHandleVolume(AudioHandle handle, float volume);
    bool setHandlePitch(AudioHandle handle, float pitch);

    void applySettings(const heritage::settings::AudioSettings& settings);
    const heritage::settings::AudioSettings& settings() const;

    void setMasterVolume(float volume);
    float masterVolume() const;
    void setBusVolume(AudioBus bus, float volume);
    float busVolume(AudioBus bus) const;
    void setMuteWhenUnfocused(bool enabled);
    bool muteWhenUnfocused() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace heritage::audio
