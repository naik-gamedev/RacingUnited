#include "AudioSystem.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xaudio2.h>
#endif

namespace heritage::audio {
namespace {

float clampVolume(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

std::string lowerCopy(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

#ifdef _WIN32

float clampPitch(float value)
{
    return std::clamp(value, 0.25f, 4.0f);
}

std::uint16_t readU16(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>(data[0])
        | static_cast<std::uint16_t>(data[1] << 8);
}

std::uint32_t readU32(const std::uint8_t* data)
{
    return static_cast<std::uint32_t>(data[0])
        | (static_cast<std::uint32_t>(data[1]) << 8)
        | (static_cast<std::uint32_t>(data[2]) << 16)
        | (static_cast<std::uint32_t>(data[3]) << 24);
}

std::string hresultText(HRESULT result)
{
    std::ostringstream stream;
    stream << "HRESULT 0x" << std::hex << std::uppercase
        << static_cast<unsigned long>(result);
    return stream.str();
}

struct LoadedWav
{
    WAVEFORMATEX format{};
    std::vector<std::uint8_t> pcm;
};

bool loadWavFile(
    const std::filesystem::path& path,
    LoadedWav& output,
    std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        error = "Audio file was not found: " + path.string();
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    file.seekg(0, std::ios::beg);
    if (length < 44)
    {
        error = "WAV file is too small: " + path.string();
        return false;
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    file.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!file)
    {
        error = "Could not read WAV file: " + path.string();
        return false;
    }

    if (std::memcmp(bytes.data(), "RIFF", 4) != 0
        || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
    {
        error = "Audio file is not a RIFF/WAVE file: " + path.string();
        return false;
    }

    bool foundFormat = false;
    bool foundData = false;
    std::size_t cursor = 12;

    while (cursor + 8 <= bytes.size())
    {
        const char* chunkId = reinterpret_cast<const char*>(bytes.data() + cursor);
        const std::uint32_t chunkSize = readU32(bytes.data() + cursor + 4);
        const std::size_t payload = cursor + 8;
        if (payload + chunkSize > bytes.size())
        {
            error = "WAV chunk extends beyond the file: " + path.string();
            return false;
        }

        if (std::memcmp(chunkId, "fmt ", 4) == 0)
        {
            if (chunkSize < 16)
            {
                error = "WAV format chunk is incomplete: " + path.string();
                return false;
            }

            output.format = {};
            output.format.wFormatTag = readU16(bytes.data() + payload + 0);
            output.format.nChannels = readU16(bytes.data() + payload + 2);
            output.format.nSamplesPerSec = readU32(bytes.data() + payload + 4);
            output.format.nAvgBytesPerSec = readU32(bytes.data() + payload + 8);
            output.format.nBlockAlign = readU16(bytes.data() + payload + 12);
            output.format.wBitsPerSample = readU16(bytes.data() + payload + 14);
            output.format.cbSize = chunkSize >= 18
                ? readU16(bytes.data() + payload + 16)
                : 0;

            if (output.format.wFormatTag != WAVE_FORMAT_PCM
                && output.format.wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
            {
                error = "WAV encoding is not supported yet (use PCM or IEEE float): "
                    + path.string();
                return false;
            }

            if (output.format.nChannels == 0
                || output.format.nChannels > XAUDIO2_MAX_AUDIO_CHANNELS
                || output.format.nSamplesPerSec == 0
                || output.format.nBlockAlign == 0)
            {
                error = "WAV format values are invalid: " + path.string();
                return false;
            }

            foundFormat = true;
        }
        else if (std::memcmp(chunkId, "data", 4) == 0)
        {
            output.pcm.assign(
                bytes.begin() + static_cast<std::ptrdiff_t>(payload),
                bytes.begin() + static_cast<std::ptrdiff_t>(payload + chunkSize));
            foundData = !output.pcm.empty();
        }

        cursor = payload + chunkSize + (chunkSize & 1U);
    }

    if (!foundFormat || !foundData)
    {
        error = "WAV file is missing its fmt or data chunk: " + path.string();
        return false;
    }

    error.clear();
    return true;
}

#endif

} // namespace

const char* audioBusName(AudioBus bus)
{
    switch (bus)
    {
    case AudioBus::Music: return "Music";
    case AudioBus::Effects: return "Effects";
    case AudioBus::Ambience: return "Ambience";
    case AudioBus::UI: return "UI";
    case AudioBus::Voice: return "Voice";
    }
    return "Effects";
}

std::optional<AudioBus> parseAudioBus(const std::string& name)
{
    const std::string lowered = lowerCopy(name);
    if (lowered == "music") return AudioBus::Music;
    if (lowered == "effects" || lowered == "effect" || lowered == "sfx") return AudioBus::Effects;
    if (lowered == "ambience" || lowered == "ambient") return AudioBus::Ambience;
    if (lowered == "ui" || lowered == "interface") return AudioBus::UI;
    if (lowered == "voice" || lowered == "radio") return AudioBus::Voice;
    return std::nullopt;
}

struct AudioSystem::Impl
{
    heritage::settings::AudioSettings settings;
    bool applicationFocused = true;
    bool initialized = false;
    std::string error;
    AudioHandle nextHandle = 1;

#ifdef _WIN32
    struct Clip
    {
        WAVEFORMATEX format{};
        std::vector<std::uint8_t> pcm;
    };

    struct Voice
    {
        IXAudio2SourceVoice* source = nullptr;
        std::shared_ptr<Clip> clip;
        AudioBus bus = AudioBus::Effects;
        float localVolume = 1.0f;
        float pitch = 1.0f;
        bool looping = false;
    };

    IXAudio2* engine = nullptr;
    IXAudio2MasteringVoice* masteringVoice = nullptr;
    std::unordered_map<std::string, std::weak_ptr<Clip>> clipCache;
    std::unordered_map<AudioHandle, Voice> voices;
#endif

    float busVolume(AudioBus bus) const
    {
        switch (bus)
        {
        case AudioBus::Music: return settings.musicVolume;
        case AudioBus::Effects: return settings.effectsVolume;
        case AudioBus::Ambience: return settings.ambienceVolume;
        case AudioBus::UI: return settings.uiVolume;
        case AudioBus::Voice: return settings.voiceVolume;
        }
        return 1.0f;
    }

#ifdef _WIN32
    float effectiveVolume(const Voice& voice) const
    {
        if (settings.muteWhenUnfocused && !applicationFocused)
            return 0.0f;

        return clampVolume(settings.masterVolume)
            * clampVolume(busVolume(voice.bus))
            * clampVolume(voice.localVolume);
    }

    void applyVoiceVolume(Voice& voice)
    {
        if (voice.source)
            voice.source->SetVolume(effectiveVolume(voice), XAUDIO2_COMMIT_NOW);
    }

    void applyAllVolumes()
    {
        for (auto& [handle, voice] : voices)
        {
            (void)handle;
            applyVoiceVolume(voice);
        }
    }

    std::shared_ptr<Clip> loadClip(const std::filesystem::path& requestedPath)
    {
        std::error_code pathError;
        const std::filesystem::path absolutePath = std::filesystem::absolute(
            requestedPath,
            pathError).lexically_normal();
        const std::string cacheKey = (pathError ? requestedPath : absolutePath).string();

        if (const auto found = clipCache.find(cacheKey); found != clipCache.end())
        {
            if (auto cached = found->second.lock())
                return cached;
        }

        LoadedWav loaded;
        if (!loadWavFile(pathError ? requestedPath : absolutePath, loaded, error))
            return {};

        auto clip = std::make_shared<Clip>();
        clip->format = loaded.format;
        clip->pcm = std::move(loaded.pcm);
        clipCache[cacheKey] = clip;
        return clip;
    }

    AudioHandle play(
        const std::filesystem::path& path,
        AudioBus bus,
        float volume,
        float pitch,
        bool loop)
    {
        if (!initialized || !engine)
        {
            error = "Audio system is not initialized.";
            return kInvalidAudioHandle;
        }

        std::shared_ptr<Clip> clip = loadClip(path);
        if (!clip)
            return kInvalidAudioHandle;

        IXAudio2SourceVoice* source = nullptr;
        HRESULT result = engine->CreateSourceVoice(
            &source,
            &clip->format,
            0,
            4.0f,
            nullptr,
            nullptr,
            nullptr);
        if (FAILED(result) || !source)
        {
            error = "XAudio2 could not create a source voice ("
                + hresultText(result) + ").";
            return kInvalidAudioHandle;
        }

        XAUDIO2_BUFFER buffer{};
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.AudioBytes = static_cast<UINT32>(clip->pcm.size());
        buffer.pAudioData = clip->pcm.data();
        buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

        result = source->SubmitSourceBuffer(&buffer);
        if (FAILED(result))
        {
            source->DestroyVoice();
            error = "XAudio2 could not submit the sound buffer ("
                + hresultText(result) + ").";
            return kInvalidAudioHandle;
        }

        const AudioHandle handle = nextHandle++;
        Voice voice;
        voice.source = source;
        voice.clip = std::move(clip);
        voice.bus = bus;
        voice.localVolume = clampVolume(volume);
        voice.pitch = clampPitch(pitch);
        voice.looping = loop;

        source->SetFrequencyRatio(voice.pitch, XAUDIO2_COMMIT_NOW);
        voices.emplace(handle, std::move(voice));
        applyVoiceVolume(voices.at(handle));

        result = source->Start(0, XAUDIO2_COMMIT_NOW);
        if (FAILED(result))
        {
            source->DestroyVoice();
            voices.erase(handle);
            error = "XAudio2 could not start playback ("
                + hresultText(result) + ").";
            return kInvalidAudioHandle;
        }

        error.clear();
        return handle;
    }

    bool stopVoice(AudioHandle handle)
    {
        const auto found = voices.find(handle);
        if (found == voices.end())
            return false;

        if (found->second.source)
        {
            found->second.source->Stop(0, XAUDIO2_COMMIT_NOW);
            found->second.source->FlushSourceBuffers();
            found->second.source->DestroyVoice();
        }
        voices.erase(found);
        return true;
    }
#endif
};

AudioSystem::AudioSystem()
    : m_impl(std::make_unique<Impl>())
{
}

AudioSystem::~AudioSystem()
{
    shutdown();
}

bool AudioSystem::initialize(std::string& message)
{
    shutdown();

#ifdef _WIN32
    HRESULT result = XAudio2Create(
        &m_impl->engine,
        0,
        XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(result) || !m_impl->engine)
    {
        m_impl->error = "XAudio2 initialization failed ("
            + hresultText(result) + ").";
        message = m_impl->error;
        return false;
    }

    result = m_impl->engine->CreateMasteringVoice(
        &m_impl->masteringVoice,
        XAUDIO2_DEFAULT_CHANNELS,
        XAUDIO2_DEFAULT_SAMPLERATE,
        0,
        nullptr,
        nullptr,
        AudioCategory_GameEffects);
    if (FAILED(result) || !m_impl->masteringVoice)
    {
        m_impl->error = "XAudio2 could not create the mastering voice ("
            + hresultText(result) + ").";
        m_impl->engine->Release();
        m_impl->engine = nullptr;
        message = m_impl->error;
        return false;
    }

    m_impl->initialized = true;
    m_impl->error.clear();
    message.clear();
    return true;
#else
    m_impl->error = "No native audio backend is implemented for this operating system yet.";
    message = m_impl->error;
    return false;
#endif
}

void AudioSystem::shutdown()
{
    if (!m_impl)
        return;

#ifdef _WIN32
    for (auto& [handle, voice] : m_impl->voices)
    {
        (void)handle;
        if (voice.source)
        {
            voice.source->Stop(0, XAUDIO2_COMMIT_NOW);
            voice.source->FlushSourceBuffers();
            voice.source->DestroyVoice();
        }
    }
    m_impl->voices.clear();
    m_impl->clipCache.clear();

    if (m_impl->masteringVoice)
    {
        m_impl->masteringVoice->DestroyVoice();
        m_impl->masteringVoice = nullptr;
    }

    if (m_impl->engine)
    {
        m_impl->engine->StopEngine();
        m_impl->engine->Release();
        m_impl->engine = nullptr;
    }
#endif

    m_impl->initialized = false;
    m_impl->nextHandle = 1;
}

void AudioSystem::update(bool applicationFocused)
{
    if (!m_impl)
        return;

    if (m_impl->applicationFocused != applicationFocused)
    {
        m_impl->applicationFocused = applicationFocused;
#ifdef _WIN32
        m_impl->applyAllVolumes();
#endif
    }

#ifdef _WIN32
    std::vector<AudioHandle> completed;
    for (const auto& [handle, voice] : m_impl->voices)
    {
        if (!voice.source || voice.looping)
            continue;

        XAUDIO2_VOICE_STATE state{};
        voice.source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued == 0)
            completed.push_back(handle);
    }

    for (const AudioHandle handle : completed)
        m_impl->stopVoice(handle);
#endif
}

bool AudioSystem::isAvailable() const
{
    return m_impl && m_impl->initialized;
}

std::string AudioSystem::backendName() const
{
#ifdef _WIN32
    return isAvailable() ? "XAudio2 (Windows)" : "XAudio2 unavailable";
#else
    return "No backend";
#endif
}

const std::string& AudioSystem::lastError() const
{
    return m_impl->error;
}

AudioHandle AudioSystem::playOneShot(
    const std::filesystem::path& path,
    AudioBus bus,
    float volume,
    float pitch)
{
#ifdef _WIN32
    return m_impl->play(path, bus, volume, pitch, false);
#else
    (void)path; (void)bus; (void)volume; (void)pitch;
    m_impl->error = "Audio playback is unavailable on this platform.";
    return kInvalidAudioHandle;
#endif
}

AudioHandle AudioSystem::playLoop(
    const std::filesystem::path& path,
    AudioBus bus,
    float volume,
    float pitch)
{
#ifdef _WIN32
    return m_impl->play(path, bus, volume, pitch, true);
#else
    (void)path; (void)bus; (void)volume; (void)pitch;
    m_impl->error = "Audio playback is unavailable on this platform.";
    return kInvalidAudioHandle;
#endif
}

bool AudioSystem::stop(AudioHandle handle)
{
#ifdef _WIN32
    return handle != kInvalidAudioHandle && m_impl->stopVoice(handle);
#else
    (void)handle;
    return false;
#endif
}

void AudioSystem::stopAll()
{
#ifdef _WIN32
    std::vector<AudioHandle> handles;
    handles.reserve(m_impl->voices.size());
    for (const auto& [handle, voice] : m_impl->voices)
    {
        (void)voice;
        handles.push_back(handle);
    }
    for (const AudioHandle handle : handles)
        m_impl->stopVoice(handle);
#endif
}

void AudioSystem::stopBus(AudioBus bus)
{
#ifdef _WIN32
    std::vector<AudioHandle> handles;
    for (const auto& [handle, voice] : m_impl->voices)
    {
        if (voice.bus == bus)
            handles.push_back(handle);
    }
    for (const AudioHandle handle : handles)
        m_impl->stopVoice(handle);
#else
    (void)bus;
#endif
}

bool AudioSystem::isPlaying(AudioHandle handle) const
{
#ifdef _WIN32
    const auto found = m_impl->voices.find(handle);
    if (found == m_impl->voices.end() || !found->second.source)
        return false;

    XAUDIO2_VOICE_STATE state{};
    found->second.source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    return state.BuffersQueued > 0;
#else
    (void)handle;
    return false;
#endif
}

bool AudioSystem::setHandleVolume(AudioHandle handle, float volume)
{
#ifdef _WIN32
    const auto found = m_impl->voices.find(handle);
    if (found == m_impl->voices.end())
        return false;

    found->second.localVolume = clampVolume(volume);
    m_impl->applyVoiceVolume(found->second);
    return true;
#else
    (void)handle; (void)volume;
    return false;
#endif
}

bool AudioSystem::setHandlePitch(AudioHandle handle, float pitch)
{
#ifdef _WIN32
    const auto found = m_impl->voices.find(handle);
    if (found == m_impl->voices.end() || !found->second.source)
        return false;

    found->second.pitch = clampPitch(pitch);
    return SUCCEEDED(found->second.source->SetFrequencyRatio(
        found->second.pitch,
        XAUDIO2_COMMIT_NOW));
#else
    (void)handle; (void)pitch;
    return false;
#endif
}

void AudioSystem::applySettings(
    const heritage::settings::AudioSettings& settings)
{
    m_impl->settings.masterVolume = clampVolume(settings.masterVolume);
    m_impl->settings.musicVolume = clampVolume(settings.musicVolume);
    m_impl->settings.effectsVolume = clampVolume(settings.effectsVolume);
    m_impl->settings.ambienceVolume = clampVolume(settings.ambienceVolume);
    m_impl->settings.uiVolume = clampVolume(settings.uiVolume);
    m_impl->settings.voiceVolume = clampVolume(settings.voiceVolume);
    m_impl->settings.muteWhenUnfocused = settings.muteWhenUnfocused;
#ifdef _WIN32
    m_impl->applyAllVolumes();
#endif
}

const heritage::settings::AudioSettings& AudioSystem::settings() const
{
    return m_impl->settings;
}

void AudioSystem::setMasterVolume(float volume)
{
    m_impl->settings.masterVolume = clampVolume(volume);
#ifdef _WIN32
    m_impl->applyAllVolumes();
#endif
}

float AudioSystem::masterVolume() const
{
    return m_impl->settings.masterVolume;
}

void AudioSystem::setBusVolume(AudioBus bus, float volume)
{
    const float clamped = clampVolume(volume);
    switch (bus)
    {
    case AudioBus::Music: m_impl->settings.musicVolume = clamped; break;
    case AudioBus::Effects: m_impl->settings.effectsVolume = clamped; break;
    case AudioBus::Ambience: m_impl->settings.ambienceVolume = clamped; break;
    case AudioBus::UI: m_impl->settings.uiVolume = clamped; break;
    case AudioBus::Voice: m_impl->settings.voiceVolume = clamped; break;
    }
#ifdef _WIN32
    m_impl->applyAllVolumes();
#endif
}

float AudioSystem::busVolume(AudioBus bus) const
{
    return m_impl->busVolume(bus);
}

void AudioSystem::setMuteWhenUnfocused(bool enabled)
{
    m_impl->settings.muteWhenUnfocused = enabled;
#ifdef _WIN32
    m_impl->applyAllVolumes();
#endif
}

bool AudioSystem::muteWhenUnfocused() const
{
    return m_impl->settings.muteWhenUnfocused;
}

} // namespace heritage::audio
