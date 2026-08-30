#include "AudioSystem.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <array>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
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
#include <xaudio2fx.h>
#include "../../../ThirdParty/stb/stb_vorbis.c"
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
    return std::clamp(value, 0.125f, 8.0f);
}

float dot(const AudioVector3& left, const AudioVector3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

AudioVector3 subtract(const AudioVector3& left, const AudioVector3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

float length(const AudioVector3& value)
{
    return std::sqrt((std::max)(dot(value, value), 0.0f));
}

AudioVector3 normalized(const AudioVector3& value)
{
    const float magnitude = length(value);
    return magnitude > 1.0e-5f
        ? AudioVector3{ value.x / magnitude, value.y / magnitude, value.z / magnitude }
        : AudioVector3{ 0.0f, 0.0f, 1.0f };
}

AudioVector3 cross(const AudioVector3& left, const AudioVector3& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
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

bool loadOggFile(
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
    if (length <= 0 || length > static_cast<std::streamoff>((std::numeric_limits<int>::max)()))
    {
        error = "OGG file length is invalid: " + path.string();
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    file.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!file)
    {
        error = "Could not read OGG file: " + path.string();
        return false;
    }

    int channels = 0;
    int sampleRate = 0;
    short* decoded = nullptr;
    const int samplesPerChannel = stb_vorbis_decode_memory(
        bytes.data(), static_cast<int>(bytes.size()),
        &channels, &sampleRate, &decoded);
    if (samplesPerChannel <= 0 || !decoded || channels <= 0
        || channels > XAUDIO2_MAX_AUDIO_CHANNELS || sampleRate <= 0)
    {
        std::free(decoded);
        error = "Could not decode OGG/Vorbis audio file: " + path.string();
        return false;
    }

    const std::size_t sampleCount = static_cast<std::size_t>(samplesPerChannel)
        * static_cast<std::size_t>(channels);
    output.format = {};
    output.format.wFormatTag = WAVE_FORMAT_PCM;
    output.format.nChannels = static_cast<WORD>(channels);
    output.format.nSamplesPerSec = static_cast<DWORD>(sampleRate);
    output.format.wBitsPerSample = 16;
    output.format.nBlockAlign = static_cast<WORD>(channels * sizeof(short));
    output.format.nAvgBytesPerSec = output.format.nSamplesPerSec
        * output.format.nBlockAlign;
    output.pcm.resize(sampleCount * sizeof(short));
    std::memcpy(output.pcm.data(), decoded, output.pcm.size());
    std::free(decoded);
    error.clear();
    return true;
}

bool loadAudioFile(
    const std::filesystem::path& path,
    LoadedWav& output,
    std::string& error)
{
    const std::string extension = lowerCopy(path.extension().string());
    if (extension == ".wav" || extension == ".wave")
        return loadWavFile(path, output, error);
    if (extension == ".ogg" || extension == ".oga")
        return loadOggFile(path, output, error);
    error = "Unsupported audio file extension (use WAV or OGG/Vorbis): "
        + path.string();
    return false;
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
    AudioListenerState listener;

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
        float lowPassOpenness = 1.0f;
        bool looping = false;
        bool spatial = false;
        bool acousticRouting = false;
        AudioEmitterState emitter;
        AcousticPropagationState acoustics;
    };

    struct CachedClip
    {
        std::shared_ptr<Clip> clip;
        std::uint64_t lastUse = 0;
    };

    IXAudio2* engine = nullptr;
    IXAudio2MasteringVoice* masteringVoice = nullptr;
    static constexpr std::size_t reverbBusCount = 3;
    std::array<IXAudio2SubmixVoice*, reverbBusCount> reverbBuses{};
    std::uint32_t outputChannels = 2;
    std::uint32_t outputSampleRate = 48000;
    std::unordered_map<std::string, CachedClip> clipCache;
    std::size_t clipCacheBytes = 0;
    std::uint64_t clipCacheClock = 0;
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
    void trimClipCache()
    {
        constexpr std::size_t maximumBytes = 256U * 1024U * 1024U;
        while (clipCacheBytes > maximumBytes)
        {
            auto oldest = clipCache.end();
            for (auto candidate = clipCache.begin(); candidate != clipCache.end(); ++candidate)
            {
                if (!candidate->second.clip
                    || candidate->second.clip.use_count() != 1)
                    continue;
                if (oldest == clipCache.end()
                    || candidate->second.lastUse < oldest->second.lastUse)
                    oldest = candidate;
            }
            if (oldest == clipCache.end())
                break;
            clipCacheBytes -= oldest->second.clip->pcm.size();
            clipCache.erase(oldest);
        }
    }

    void cacheClip(const std::string& key, const std::shared_ptr<Clip>& clip)
    {
        if (!clip)
            return;
        if (const auto existing = clipCache.find(key); existing != clipCache.end())
        {
            if (existing->second.clip)
                clipCacheBytes -= existing->second.clip->pcm.size();
            clipCache.erase(existing);
        }
        clipCacheBytes += clip->pcm.size();
        clipCache.emplace(key, CachedClip{ clip, ++clipCacheClock });
        trimClipCache();
    }

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

    void destroyReverbBuses()
    {
        for (IXAudio2SubmixVoice*& bus : reverbBuses)
        {
            if (bus)
                bus->DestroyVoice();
            bus = nullptr;
        }
    }

    bool createReverbBuses()
    {
        if (!engine || !masteringVoice)
            return false;

        const std::array<float, reverbBusCount> reflectionDelays{
            0.008f, 0.032f, 0.074f
        };
        const std::array<float, reverbBusCount> decayTimes{
            0.42f, 1.05f, 2.10f
        };
        const UINT32 effectRate = std::clamp<std::uint32_t>(
            outputSampleRate, XAUDIO2FX_REVERB_MIN_FRAMERATE,
            XAUDIO2FX_REVERB_MAX_FRAMERATE);

        for (std::size_t index = 0; index < reverbBusCount; ++index)
        {
            IUnknown* reverb = nullptr;
            HRESULT result = XAudio2CreateReverb(&reverb, 0);
            if (FAILED(result) || !reverb)
            {
                destroyReverbBuses();
                return false;
            }

            XAUDIO2_EFFECT_DESCRIPTOR descriptor{};
            descriptor.InitialState = TRUE;
            descriptor.OutputChannels = 2;
            descriptor.pEffect = reverb;
            XAUDIO2_EFFECT_CHAIN chain{};
            chain.EffectCount = 1;
            chain.pEffectDescriptors = &descriptor;
            result = engine->CreateSubmixVoice(
                &reverbBuses[index], 2, effectRate, 0, 0, nullptr, &chain);
            reverb->Release();
            if (FAILED(result) || !reverbBuses[index])
            {
                destroyReverbBuses();
                return false;
            }

            XAUDIO2FX_REVERB_I3DL2_PARAMETERS authored{};
            authored.WetDryMix = 100.0f;
            authored.Room = index == 0 ? -2200 : (index == 1 ? -1700 : -1400);
            authored.RoomHF = index == 0 ? -3000 : (index == 1 ? -2500 : -2100);
            authored.RoomRolloffFactor = 0.0f;
            authored.DecayTime = decayTimes[index];
            authored.DecayHFRatio = 0.62f;
            authored.Reflections = index == 0 ? -500 : -800;
            authored.ReflectionsDelay = reflectionDelays[index];
            authored.Reverb = index == 0 ? -1100 : -650;
            authored.ReverbDelay = index == 0 ? 0.004f : 0.012f;
            authored.Diffusion = index == 0 ? 45.0f : 78.0f;
            authored.Density = index == 0 ? 58.0f : 92.0f;
            authored.HFReference = 6200.0f;
            XAUDIO2FX_REVERB_PARAMETERS native{};
            ReverbConvertI3DL2ToNative(&authored, &native, FALSE);
            result = reverbBuses[index]->SetEffectParameters(
                0, &native, sizeof(native), XAUDIO2_COMMIT_NOW);
            if (FAILED(result))
            {
                destroyReverbBuses();
                return false;
            }
        }
        return true;
    }

    bool ensureAcousticRouting(Voice& voice)
    {
        if (voice.acousticRouting)
            return true;
        if (!voice.source || !voice.clip
            || voice.clip->format.nChannels != 1
            || !masteringVoice
            || std::any_of(
                reverbBuses.begin(), reverbBuses.end(),
                [](IXAudio2SubmixVoice* value) { return value == nullptr; }))
        {
            return false;
        }

        std::array<XAUDIO2_SEND_DESCRIPTOR, 1 + reverbBusCount> descriptors{};
        descriptors[0] = { 0, masteringVoice };
        for (std::size_t index = 0; index < reverbBusCount; ++index)
            descriptors[index + 1] = { 0, reverbBuses[index] };
        XAUDIO2_VOICE_SENDS sends{};
        sends.SendCount = static_cast<UINT32>(descriptors.size());
        sends.pSends = descriptors.data();
        if (FAILED(voice.source->SetOutputVoices(&sends)))
            return false;
        voice.acousticRouting = true;
        return true;
    }

    void releaseAcousticRouting(Voice& voice)
    {
        if (!voice.acousticRouting || !voice.source)
            return;
        // A null send list restores XAudio2's default mastering-voice route.
        // This removes three inactive reverb sends when a vehicle leaves the
        // 20-source geometry budget instead of making the full field pay for
        // zero-valued matrices forever.
        if (SUCCEEDED(voice.source->SetOutputVoices(nullptr)))
            voice.acousticRouting = false;
    }

    std::shared_ptr<Clip> loadClipUncached(const std::filesystem::path& requestedPath)
    {
        std::error_code pathError;
        const std::filesystem::path absolutePath = std::filesystem::absolute(
            requestedPath, pathError).lexically_normal();
        LoadedWav loaded;
        if (!loadAudioFile(pathError ? requestedPath : absolutePath, loaded, error))
            return {};
        auto clip = std::make_shared<Clip>();
        clip->format = loaded.format;
        clip->pcm = std::move(loaded.pcm);
        return clip;
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
            found->second.lastUse = ++clipCacheClock;
            return found->second.clip;
        }

        LoadedWav loaded;
        if (!loadAudioFile(pathError ? requestedPath : absolutePath, loaded, error))
            return {};

        auto clip = std::make_shared<Clip>();
        clip->format = loaded.format;
        clip->pcm = std::move(loaded.pcm);
        cacheClip(cacheKey, clip);
        return clip;
    }

    std::shared_ptr<Clip> generatedClip(
        const std::string& cacheKey,
        const GeneratedMonoAudio& generated)
    {
        const std::string key = "generated:" + cacheKey;
        if (const auto found = clipCache.find(key); found != clipCache.end())
        {
            found->second.lastUse = ++clipCacheClock;
            return found->second.clip;
        }

        if (generated.sampleRate < 8000
            || generated.sampleRate > 192000
            || generated.samples.size() < 64)
        {
            error = "Generated audio must contain at least 64 mono samples at 8-192 kHz.";
            return {};
        }

        auto clip = std::make_shared<Clip>();
        clip->format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        clip->format.nChannels = 1;
        clip->format.nSamplesPerSec = generated.sampleRate;
        clip->format.wBitsPerSample = 32;
        clip->format.nBlockAlign = sizeof(float);
        clip->format.nAvgBytesPerSec = generated.sampleRate * sizeof(float);
        clip->pcm.resize(generated.samples.size() * sizeof(float));
        std::memcpy(
            clip->pcm.data(),
            generated.samples.data(),
            clip->pcm.size());
        cacheClip(key, clip);
        return clip;
    }

    std::shared_ptr<Clip> generatedClip(
        const std::string& cacheKey,
        const std::function<GeneratedMonoAudio()>& factory)
    {
        const std::string key = "generated:" + cacheKey;
        if (const auto found = clipCache.find(key); found != clipCache.end())
        {
            found->second.lastUse = ++clipCacheClock;
            return found->second.clip;
        }
        return generatedClip(cacheKey, factory());
    }

    AudioHandle playClip(
        std::shared_ptr<Clip> clip,
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
        if (!clip)
            return kInvalidAudioHandle;

        IXAudio2SourceVoice* source = nullptr;
        HRESULT result = engine->CreateSourceVoice(
            &source,
            &clip->format,
            XAUDIO2_VOICE_USEFILTER,
            8.0f,
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

    AudioHandle play(
        const std::filesystem::path& path,
        AudioBus bus,
        float volume,
        float pitch,
        bool loop)
    {
        std::shared_ptr<Clip> clip = loadClip(path);
        return playClip(std::move(clip), bus, volume, pitch, loop);
    }

    void updateSpatialVoice(Voice& voice)
    {
        if (!voice.source || !voice.spatial)
            return;

        const AudioVector3 delta = subtract(voice.emitter.position, listener.position);
        const float distance = length(delta);
        const AudioVector3 direction = normalized(delta);
        const float minimum = (std::max)(voice.emitter.minimumDistanceMeters, 0.1f);
        const float maximum = (std::max)(voice.emitter.maximumDistanceMeters, minimum + 0.1f);
        const float normalizedDistance = std::clamp(
            (distance - minimum) / (maximum - minimum), 0.0f, 1.0f);
        const float attenuation = (1.0f - normalizedDistance)
            * (1.0f - normalizedDistance);

        const AudioVector3 listenerRight = normalized(cross(listener.up, listener.forward));
        const float pan = std::clamp(dot(direction, listenerRight), -1.0f, 1.0f);
        if (voice.clip && voice.clip->format.nChannels == 1 && outputChannels == 2)
        {
            const float direct = attenuation * std::clamp(
                voice.acoustics.directGain, 0.0f, 1.0f);
            const float left = direct * std::sqrt(0.5f * (1.0f - pan));
            const float right = direct * std::sqrt(0.5f * (1.0f + pan));
            const float matrix[2]{ left, right };
            voice.source->SetOutputMatrix(
                masteringVoice, 1, 2, matrix, XAUDIO2_COMMIT_NOW);

            if (voice.acousticRouting)
            {
                constexpr std::array<float, reverbBusCount> delayCenters{
                    0.008f, 0.032f, 0.074f
                };
                std::array<float, reverbBusCount> delayWeights{};
                const float delay = std::clamp(
                    voice.acoustics.earlyReflectionDelaySeconds,
                    delayCenters.front(), delayCenters.back());
                if (delay <= delayCenters[1])
                {
                    const float blend = (delay - delayCenters[0])
                        / (delayCenters[1] - delayCenters[0]);
                    delayWeights[0] = std::sqrt(std::max(1.0f - blend, 0.0f));
                    delayWeights[1] = std::sqrt(std::max(blend, 0.0f));
                }
                else
                {
                    const float blend = (delay - delayCenters[1])
                        / (delayCenters[2] - delayCenters[1]);
                    delayWeights[1] = std::sqrt(std::max(1.0f - blend, 0.0f));
                    delayWeights[2] = std::sqrt(std::max(blend, 0.0f));
                }

                const float early = std::clamp(
                    voice.acoustics.earlyReflectionGain, 0.0f, 0.75f);
                const float late = std::clamp(
                    voice.acoustics.lateReverbGain, 0.0f, 0.50f);
                for (std::size_t index = 0; index < reverbBusCount; ++index)
                {
                    const float earlySend = early * delayWeights[index];
                    const float lateSend = late * (0.16f + 0.24f * index);
                    const float wet = attenuation * std::clamp(
                        earlySend + lateSend, 0.0f, 0.72f);
                    // Reflections are deliberately wider than the direct ray;
                    // the reverb APO diffuses this stereo seed further.
                    const float wetLeft = wet * (0.82f - 0.18f * pan);
                    const float wetRight = wet * (0.82f + 0.18f * pan);
                    const float wetMatrix[2]{ wetLeft, wetRight };
                    voice.source->SetOutputMatrix(
                        reverbBuses[index], 1, 2,
                        wetMatrix, XAUDIO2_COMMIT_NOW);
                }
            }
        }
        else
        {
            voice.source->SetVolume(effectiveVolume(voice) * attenuation, XAUDIO2_COMMIT_NOW);
        }

        constexpr float speedOfSound = 343.0f;
        const float listenerRadial = dot(listener.velocity, direction);
        const float emitterRadial = dot(voice.emitter.velocity, direction);
        const float doppler = std::clamp(
            (speedOfSound + listenerRadial) / (speedOfSound + emitterRadial),
            0.5f,
            2.0f);
        voice.source->SetFrequencyRatio(
            clampPitch(voice.pitch * doppler), XAUDIO2_COMMIT_NOW);

        XAUDIO2_FILTER_PARAMETERS filter{};
        filter.Type = LowPassFilter;
        const float openness = std::clamp(
            voice.lowPassOpenness * voice.acoustics.directOpenness,
            0.0f, 1.0f);
        filter.Frequency = 0.035f + 0.965f * openness * openness;
        filter.OneOverQ = 1.0f;
        voice.source->SetFilterParameters(&filter, XAUDIO2_COMMIT_NOW);
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

    XAUDIO2_VOICE_DETAILS masteringDetails{};
    m_impl->masteringVoice->GetVoiceDetails(&masteringDetails);
    m_impl->outputChannels = masteringDetails.InputChannels;
    m_impl->outputSampleRate = masteringDetails.InputSampleRate;
    // Reverb is optional: basic playback remains available on devices which
    // reject an effect format. Geometry tracing then still supplies direct
    // occlusion and low-pass filtering.
    m_impl->createReverbBuses();

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
    m_impl->clipCacheBytes = 0;
    m_impl->clipCacheClock = 0;

    m_impl->destroyReverbBuses();

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
    for (auto& [handle, voice] : m_impl->voices)
    {
        m_impl->updateSpatialVoice(voice);
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

AudioHandle AudioSystem::playOneShotUncached(
    const std::filesystem::path& path,
    AudioBus bus,
    float volume,
    float pitch)
{
#ifdef _WIN32
    return m_impl->playClip(
        m_impl->loadClipUncached(path),
        bus, volume, pitch, false);
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

AudioHandle AudioSystem::playGeneratedLoop(
    const std::string& cacheKey,
    const GeneratedMonoAudio& audio,
    AudioBus bus,
    float volume,
    float pitch)
{
#ifdef _WIN32
    return m_impl->playClip(
        m_impl->generatedClip(cacheKey, audio),
        bus,
        volume,
        pitch,
        true);
#else
    (void)cacheKey; (void)audio; (void)bus; (void)volume; (void)pitch;
    m_impl->error = "Generated audio playback is unavailable on this platform.";
    return kInvalidAudioHandle;
#endif
}

AudioHandle AudioSystem::playGeneratedLoopCached(
    const std::string& cacheKey,
    const std::function<GeneratedMonoAudio()>& factory,
    AudioBus bus,
    float volume,
    float pitch)
{
#ifdef _WIN32
    if (!factory)
    {
        m_impl->error = "Generated audio factory is missing.";
        return kInvalidAudioHandle;
    }
    return m_impl->playClip(
        m_impl->generatedClip(cacheKey, factory),
        bus,
        volume,
        pitch,
        true);
#else
    (void)cacheKey; (void)factory; (void)bus; (void)volume; (void)pitch;
    m_impl->error = "Generated audio playback is unavailable on this platform.";
    return kInvalidAudioHandle;
#endif
}

AudioHandle AudioSystem::playGeneratedOneShotCached(
    const std::string& cacheKey,
    const std::function<GeneratedMonoAudio()>& factory,
    AudioBus bus,
    float volume,
    float pitch)
{
#ifdef _WIN32
    if (!factory)
    {
        m_impl->error = "Generated audio factory is missing.";
        return kInvalidAudioHandle;
    }
    return m_impl->playClip(
        m_impl->generatedClip(cacheKey, factory),
        bus,
        volume,
        pitch,
        false);
#else
    (void)cacheKey; (void)factory; (void)bus; (void)volume; (void)pitch;
    m_impl->error = "Generated audio playback is unavailable on this platform.";
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

std::size_t AudioSystem::activeVoiceCount() const
{
#ifdef _WIN32
    return m_impl ? m_impl->voices.size() : 0;
#else
    return 0;
#endif
}

std::size_t AudioSystem::cachedAudioBytes() const
{
#ifdef _WIN32
    return m_impl ? m_impl->clipCacheBytes : 0;
#else
    return 0;
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

bool AudioSystem::setHandleSpatial(
    AudioHandle handle,
    const AudioEmitterState& emitter)
{
#ifdef _WIN32
    const auto found = m_impl->voices.find(handle);
    if (found == m_impl->voices.end() || !found->second.source)
        return false;

    found->second.spatial = true;
    found->second.emitter = emitter;
    m_impl->updateSpatialVoice(found->second);
    return true;
#else
    (void)handle; (void)emitter;
    return false;
#endif
}

bool AudioSystem::setHandleLowPass(AudioHandle handle, float openness)
{
#ifdef _WIN32
    const auto found = m_impl->voices.find(handle);
    if (found == m_impl->voices.end() || !found->second.source)
        return false;

    found->second.lowPassOpenness = std::clamp(openness, 0.0f, 1.0f);
    m_impl->updateSpatialVoice(found->second);
    return true;
#else
    (void)handle; (void)openness;
    return false;
#endif
}

bool AudioSystem::setHandleAcoustics(
    AudioHandle handle,
    const AcousticPropagationState& acoustics)
{
#ifdef _WIN32
    const auto found = m_impl->voices.find(handle);
    if (found == m_impl->voices.end() || !found->second.source)
        return false;

    auto& voice = found->second;
    voice.acoustics.directGain = std::clamp(acoustics.directGain, 0.0f, 1.0f);
    voice.acoustics.directOpenness = std::clamp(
        acoustics.directOpenness, 0.0f, 1.0f);
    voice.acoustics.earlyReflectionGain = std::clamp(
        acoustics.earlyReflectionGain, 0.0f, 0.75f);
    voice.acoustics.earlyReflectionDelaySeconds = std::clamp(
        acoustics.earlyReflectionDelaySeconds, 0.0f, 0.150f);
    voice.acoustics.lateReverbGain = std::clamp(
        acoustics.lateReverbGain, 0.0f, 0.50f);
    const bool needsAcousticRoute = voice.acoustics.directGain < 0.999f
        || voice.acoustics.directOpenness < 0.999f
        || voice.acoustics.earlyReflectionGain > 0.001f
        || voice.acoustics.lateReverbGain > 0.001f;
    if (needsAcousticRoute)
        m_impl->ensureAcousticRouting(voice);
    else
        m_impl->releaseAcousticRouting(voice);
    m_impl->updateSpatialVoice(voice);
    return true;
#else
    (void)handle; (void)acoustics;
    return false;
#endif
}

void AudioSystem::setListener(const AudioListenerState& listener)
{
    m_impl->listener = listener;
#ifdef _WIN32
    m_impl->listener.forward = normalized(listener.forward);
    m_impl->listener.up = normalized(listener.up);
#endif
}

const AudioListenerState& AudioSystem::listener() const
{
    return m_impl->listener;
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
