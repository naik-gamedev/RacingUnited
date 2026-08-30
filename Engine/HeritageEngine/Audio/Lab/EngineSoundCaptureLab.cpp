#include "EngineSoundCaptureLab.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <ksmedia.h>
#endif

namespace heritage::audio::lab {
namespace {

constexpr std::uint32_t kCaptureRate = 48000;

void writeU16(std::ofstream& file, std::uint16_t value)
{
    const char bytes[2]{
        static_cast<char>(value & 0xffU),
        static_cast<char>((value >> 8U) & 0xffU)
    };
    file.write(bytes, 2);
}

void writeU32(std::ofstream& file, std::uint32_t value)
{
    const char bytes[4]{
        static_cast<char>(value & 0xffU),
        static_cast<char>((value >> 8U) & 0xffU),
        static_cast<char>((value >> 16U) & 0xffU),
        static_cast<char>((value >> 24U) & 0xffU)
    };
    file.write(bytes, 4);
}

std::vector<float> resampleStereoLinear(
    const std::vector<float>& source,
    std::uint32_t sourceRate,
    std::uint32_t targetRate)
{
    if (source.empty() || sourceRate == 0 || targetRate == 0)
        return {};
    if (sourceRate == targetRate)
        return source;

    const std::size_t sourceFrames = source.size() / 2;
    if (sourceFrames < 2)
        return source;
    const double ratio = static_cast<double>(sourceRate)
        / static_cast<double>(targetRate);
    const std::size_t targetFrames = static_cast<std::size_t>(
        std::floor(static_cast<double>(sourceFrames) / ratio));
    std::vector<float> output(targetFrames * 2, 0.0f);
    for (std::size_t frame = 0; frame < targetFrames; ++frame)
    {
        const double sourcePosition = static_cast<double>(frame) * ratio;
        const std::size_t i0 = (std::min)(
            static_cast<std::size_t>(sourcePosition), sourceFrames - 1);
        const std::size_t i1 = (std::min)(i0 + 1, sourceFrames - 1);
        const float fraction = static_cast<float>(sourcePosition - static_cast<double>(i0));
        for (std::size_t channel = 0; channel < 2; ++channel)
        {
            const float a = source[i0 * 2 + channel];
            const float b = source[i1 * 2 + channel];
            output[frame * 2 + channel] = a + (b - a) * fraction;
        }
    }
    return output;
}

#ifdef _WIN32

bool isFloatFormat(const WAVEFORMATEX* format)
{
    if (!format)
        return false;
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        return true;
    if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE
        || format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
        return false;
    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    return IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != FALSE;
}

bool isPcmFormat(const WAVEFORMATEX* format)
{
    if (!format)
        return false;
    if (format->wFormatTag == WAVE_FORMAT_PCM)
        return true;
    if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE
        || format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
        return false;
    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    return IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) != FALSE;
}

float readPcmSample(const BYTE* data, std::uint16_t bits)
{
    if (bits == 16)
    {
        std::int16_t value = 0;
        std::memcpy(&value, data, sizeof(value));
        return static_cast<float>(value) / 32768.0f;
    }
    if (bits == 24)
    {
        std::int32_t value = static_cast<std::int32_t>(data[0])
            | (static_cast<std::int32_t>(data[1]) << 8)
            | (static_cast<std::int32_t>(data[2]) << 16);
        if (value & 0x00800000)
            value |= ~0x00ffffff;
        return static_cast<float>(value) / 8388608.0f;
    }
    if (bits == 32)
    {
        std::int32_t value = 0;
        std::memcpy(&value, data, sizeof(value));
        return static_cast<float>(static_cast<double>(value) / 2147483648.0);
    }
    return 0.0f;
}

void appendPacketAsStereo(
    std::vector<float>& output,
    const BYTE* data,
    UINT32 frames,
    DWORD flags,
    const WAVEFORMATEX* format)
{
    if (!format || format->nChannels == 0 || format->nBlockAlign == 0)
        return;
    const std::uint16_t channels = format->nChannels;
    const std::uint16_t bits = format->wBitsPerSample;
    const std::size_t bytesPerSample = bits / 8U;
    const bool floating = isFloatFormat(format) && bits == 32;
    const bool pcm = isPcmFormat(format) && (bits == 16 || bits == 24 || bits == 32);

    output.reserve(output.size() + static_cast<std::size_t>(frames) * 2);
    for (UINT32 frame = 0; frame < frames; ++frame)
    {
        float left = 0.0f;
        float right = 0.0f;
        if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0 && data && (floating || pcm))
        {
            const BYTE* frameData = data + static_cast<std::size_t>(frame)
                * format->nBlockAlign;
            const auto sampleAt = [&](std::uint16_t channel)
            {
                channel = (std::min)(channel, static_cast<std::uint16_t>(channels - 1));
                const BYTE* sample = frameData + static_cast<std::size_t>(channel)
                    * bytesPerSample;
                if (floating)
                {
                    float value = 0.0f;
                    std::memcpy(&value, sample, sizeof(value));
                    return std::isfinite(value) ? value : 0.0f;
                }
                return readPcmSample(sample, bits);
            };
            left = sampleAt(0);
            right = channels >= 2 ? sampleAt(1) : left;
        }
        output.push_back(left);
        output.push_back(right);
    }
}

std::string hresultString(HRESULT result)
{
    std::ostringstream stream;
    stream << "HRESULT 0x" << std::hex << std::uppercase
        << static_cast<unsigned long>(result);
    return stream.str();
}

#endif

} // namespace

EngineSoundCaptureLab::EngineSoundCaptureLab(
    AudioSystem& audio,
    const std::filesystem::path& moduleUserRoot)
    : m_audio(audio),
      m_root(moduleUserRoot / "EngineSoundLab"),
      m_previewRoot(m_root / "Preview"),
      m_bankRoot(m_root / "Banks"),
      m_profileRoot(m_root / "Profiles")
{
    std::error_code error;
    std::filesystem::create_directories(m_previewRoot, error);
    std::filesystem::create_directories(m_bankRoot, error);
    std::filesystem::create_directories(m_profileRoot, error);
    if (error)
        m_lastError = "Could not prepare Engine Sound Lab directories: " + error.message();
}

EngineSoundCaptureLab::~EngineSoundCaptureLab()
{
    stopCapture();
    stopPreview();
    if (m_masterMutedForCapture)
    {
        m_audio.setMasterVolume(m_previousMasterVolume);
        m_masterMutedForCapture = false;
    }
}

void EngineSoundCaptureLab::update()
{
    if (!m_capturing.load(std::memory_order_acquire) && m_captureThread.joinable())
        m_captureThread.join();
    finalizeCompletedCapture();
}

bool EngineSoundCaptureLab::startCalibrationCapture(float durationSeconds)
{
    CaptureRequest request;
    request.durationSeconds = std::clamp(durationSeconds, 1.0f, 20.0f);
    request.outputPath = m_previewRoot / "calibration_raw.wav";
    return startCapture(std::move(request));
}

bool EngineSoundCaptureLab::startBankCapture(
    const std::string& vehicleId,
    const std::string& engineId,
    int rpm,
    int throttlePercent,
    float durationSeconds)
{
    CaptureRequest request;
    request.bank = true;
    request.vehicleId = safeToken(vehicleId);
    request.engineId = safeToken(engineId);
    request.rpm = std::clamp(rpm, 0, 20000);
    request.throttlePercent = std::clamp(throttlePercent, 0, 100);
    request.durationSeconds = std::clamp(durationSeconds, 1.0f, 20.0f);

    request.outputPath = bankCapturePath(
        request.vehicleId,
        request.engineId,
        request.rpm,
        request.throttlePercent);
    return startCapture(std::move(request));
}

bool EngineSoundCaptureLab::startCapture(CaptureRequest request)
{
    update();
    if (m_capturing.load(std::memory_order_acquire))
    {
        m_lastError = "An Engine Sound Lab capture is already running.";
        return false;
    }
    if (!m_audio.isAvailable())
    {
        m_lastError = "Native audio is unavailable; WASAPI loopback cannot capture.";
        return false;
    }

#ifdef _WIN32
    std::error_code directoryError;
    std::filesystem::create_directories(request.outputPath.parent_path(), directoryError);
    if (directoryError)
    {
        m_lastError = "Could not create capture directory: " + directoryError.message();
        return false;
    }

    stopPreview();
    m_stopRequested.store(false, std::memory_order_release);
    m_progress.store(0.0f, std::memory_order_release);
    m_requestedDurationSeconds = request.durationSeconds;
    m_previousMasterVolume = m_audio.masterVolume();
    m_audio.setMasterVolume(0.0f);
    m_masterMutedForCapture = true;
    {
        std::lock_guard lock(m_mutex);
        m_completed = {};
    }
    m_capturing.store(true, std::memory_order_release);
    m_lastError.clear();
    m_captureThread = std::thread(
        &EngineSoundCaptureLab::captureThreadMain,
        this,
        std::move(request));
    return true;
#else
    (void)request;
    m_lastError = "WASAPI loopback capture is only available on Windows.";
    return false;
#endif
}

void EngineSoundCaptureLab::stopCapture()
{
    m_stopRequested.store(true, std::memory_order_release);
    if (m_captureThread.joinable())
        m_captureThread.join();
    m_capturing.store(false, std::memory_order_release);
    finalizeCompletedCapture();
    if (m_masterMutedForCapture)
    {
        m_audio.setMasterVolume(m_previousMasterVolume);
        m_masterMutedForCapture = false;
    }
}

void EngineSoundCaptureLab::captureThreadMain(CaptureRequest request)
{
    CompletedCapture completed;
    completed.request = request;

#ifdef _WIN32
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = SUCCEEDED(result);
    if (FAILED(result) && result != RPC_E_CHANGED_MODE)
    {
        completed.error = "WASAPI COM initialization failed (" + hresultString(result) + ").";
    }
    else
    {
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDevice* device = nullptr;
        IAudioClient* audioClient = nullptr;
        IAudioCaptureClient* captureClient = nullptr;
        WAVEFORMATEX* mixFormat = nullptr;

        result = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
        if (SUCCEEDED(result))
            result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (SUCCEEDED(result))
            result = device->Activate(
                __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                reinterpret_cast<void**>(&audioClient));
        if (SUCCEEDED(result))
            result = audioClient->GetMixFormat(&mixFormat);
        if (SUCCEEDED(result))
        {
            result = audioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_LOOPBACK,
                10000000,
                0,
                mixFormat,
                nullptr);
        }
        if (SUCCEEDED(result))
            result = audioClient->GetService(
                __uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&captureClient));

        if (FAILED(result) || !mixFormat || !captureClient)
        {
            completed.error = "Could not initialize default-output WASAPI loopback ("
                + hresultString(result) + ").";
        }
        else if ((!isFloatFormat(mixFormat) || mixFormat->wBitsPerSample != 32)
            && !isPcmFormat(mixFormat))
        {
            completed.error = "Default output uses a WASAPI mix format the capture lab does not support.";
        }
        else
        {
            std::vector<float> nativeStereo;
            const std::uint64_t targetFrames = static_cast<std::uint64_t>(
                request.durationSeconds * static_cast<float>(mixFormat->nSamplesPerSec));
            nativeStereo.reserve(static_cast<std::size_t>(targetFrames) * 2);

            result = audioClient->Start();
            if (FAILED(result))
            {
                completed.error = "WASAPI loopback could not start (" + hresultString(result) + ").";
            }
            else
            {
                while (!m_stopRequested.load(std::memory_order_acquire)
                    && nativeStereo.size() / 2 < targetFrames)
                {
                    UINT32 packetFrames = 0;
                    result = captureClient->GetNextPacketSize(&packetFrames);
                    if (FAILED(result))
                        break;
                    if (packetFrames == 0)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(4));
                        continue;
                    }

                    BYTE* packet = nullptr;
                    UINT32 frames = 0;
                    DWORD flags = 0;
                    result = captureClient->GetBuffer(
                        &packet, &frames, &flags, nullptr, nullptr);
                    if (FAILED(result))
                        break;
                    appendPacketAsStereo(nativeStereo, packet, frames, flags, mixFormat);
                    captureClient->ReleaseBuffer(frames);

                    const float progress = targetFrames > 0
                        ? static_cast<float>(nativeStereo.size() / 2)
                            / static_cast<float>(targetFrames)
                        : 1.0f;
                    m_progress.store(std::clamp(progress, 0.0f, 1.0f), std::memory_order_release);
                }
                audioClient->Stop();

                if (FAILED(result))
                {
                    completed.error = "WASAPI loopback capture failed (" + hresultString(result) + ").";
                }
                else if (nativeStereo.size() < 128)
                {
                    completed.error = "WASAPI loopback returned no useful audio frames.";
                }
                else
                {
                    completed.stereo = resampleStereoLinear(
                        nativeStereo, mixFormat->nSamplesPerSec, kCaptureRate);
                    completed.sampleRate = kCaptureRate;
                    measure(completed.stereo, completed.peak, completed.rms);
                    std::string writeError;
                    if (!writeFloatStereoWav(
                        request.outputPath,
                        completed.stereo,
                        completed.sampleRate,
                        writeError))
                    {
                        completed.error = writeError;
                    }
                    else
                    {
                        completed.success = true;
                    }
                }
            }
        }

        if (mixFormat)
            CoTaskMemFree(mixFormat);
        if (captureClient)
            captureClient->Release();
        if (audioClient)
            audioClient->Release();
        if (device)
            device->Release();
        if (enumerator)
            enumerator->Release();
        if (shouldUninitialize)
            CoUninitialize();
    }
#else
    completed.error = "WASAPI loopback capture is only available on Windows.";
#endif

    {
        std::lock_guard lock(m_mutex);
        completed.ready = true;
        m_completed = std::move(completed);
    }
    m_progress.store(1.0f, std::memory_order_release);
    m_capturing.store(false, std::memory_order_release);
}

void EngineSoundCaptureLab::finalizeCompletedCapture()
{
    CompletedCapture completed;
    {
        std::lock_guard lock(m_mutex);
        if (!m_completed.ready)
            return;
        completed = std::move(m_completed);
        m_completed = {};
    }

    if (m_masterMutedForCapture)
    {
        m_audio.setMasterVolume(m_previousMasterVolume);
        m_masterMutedForCapture = false;
    }

    if (!completed.success)
    {
        m_lastError = completed.error.empty()
            ? "Engine Sound Lab capture failed."
            : completed.error;
        return;
    }

    m_lastRawStereo = std::move(completed.stereo);
    m_lastRawSampleRate = completed.sampleRate;
    m_lastRawPath = completed.request.outputPath;
    m_lastCaptureWasBank = completed.request.bank;
    m_lastCaptureRpm = completed.request.rpm;
    m_lastCaptureThrottlePercent = completed.request.throttlePercent;
    m_lastPeak = completed.peak;
    m_lastRms = completed.rms;
    m_lastAnalysis = analyzeEngineSoundStereo(m_lastRawStereo, m_lastRawSampleRate);
    m_lastDurationSeconds = m_lastRawSampleRate > 0
        ? static_cast<float>(m_lastRawStereo.size() / 2)
            / static_cast<float>(m_lastRawSampleRate)
        : 0.0f;
    m_lastError.clear();
    if (m_lastRms < 1.0e-4f)
    {
        m_lastError = "Capture is effectively silent. Check that Engine Simulator is playing through the Windows default output device.";
        return;
    }

    if (completed.request.bank)
    {
        std::string manifestError;
        if (!appendManifest(completed, manifestError))
            m_lastError = manifestError;
    }
}

bool EngineSoundCaptureLab::playPreview(EngineSoundPerspective perspective)
{
    update();
    if (m_capturing.load(std::memory_order_acquire))
    {
        m_lastError = "Finish the current loopback capture before auditioning it.";
        return false;
    }
    if (m_lastRawStereo.empty())
    {
        m_lastError = "Capture a calibration sample or bank cell first.";
        return false;
    }

    stopPreview();
    std::vector<float> processed = processEngineSoundStereo(
        m_lastRawStereo,
        m_lastRawSampleRate,
        perspective,
        m_profile);
    if (processed.empty())
    {
        m_lastError = "The Engine Sound Lab DSP could not process the current sample.";
        return false;
    }

    std::string token = engineSoundPerspectiveName(perspective);
    for (char& c : token)
    {
        if (c == ' ' || c == '/')
            c = '_';
    }
    m_lastPreviewPath = m_previewRoot / ("processed_" + safeToken(token) + ".wav");
    std::string writeError;
    if (!writeFloatStereoWav(m_lastPreviewPath, processed, m_lastRawSampleRate, writeError))
    {
        m_lastError = writeError;
        return false;
    }

    m_previewHandle = m_audio.playOneShotUncached(
        m_lastPreviewPath,
        AudioBus::Effects,
        1.0f,
        1.0f);
    if (m_previewHandle == kInvalidAudioHandle)
    {
        m_lastError = m_audio.lastError();
        return false;
    }
    m_lastError.clear();
    return true;
}

void EngineSoundCaptureLab::stopPreview()
{
    if (m_previewHandle != kInvalidAudioHandle)
        m_audio.stop(m_previewHandle);
    m_previewHandle = kInvalidAudioHandle;
}

void EngineSoundCaptureLab::setProfile(const EngineSoundAcousticProfile& profile)
{
    m_profile = profile;
    m_profile.inputGainDb = std::clamp(m_profile.inputGainDb, -18.0f, 12.0f);
    m_profile.highPassHz = std::clamp(m_profile.highPassHz, 10.0f, 800.0f);
    m_profile.lowPassHz = std::clamp(m_profile.lowPassHz, 1000.0f, 22000.0f);
    m_profile.bodyGainDb = std::clamp(m_profile.bodyGainDb, -12.0f, 12.0f);
    m_profile.bodyFrequencyHz = std::clamp(m_profile.bodyFrequencyHz, 40.0f, 500.0f);
    m_profile.bodyQ = std::clamp(m_profile.bodyQ, 0.2f, 4.0f);
    m_profile.presenceCutDb = std::clamp(m_profile.presenceCutDb, 0.0f, 18.0f);
    m_profile.presenceFrequencyHz = std::clamp(m_profile.presenceFrequencyHz, 500.0f, 8000.0f);
    m_profile.presenceQ = std::clamp(m_profile.presenceQ, 0.2f, 4.0f);
    m_profile.highShelfDb = std::clamp(m_profile.highShelfDb, -18.0f, 12.0f);
    m_profile.pulseSoftening = std::clamp(m_profile.pulseSoftening, 0.0f, 1.0f);
    m_profile.saturation = std::clamp(m_profile.saturation, 0.0f, 1.0f);
    m_profile.combustionPunch = std::clamp(m_profile.combustionPunch, 0.0f, 1.0f);
    m_profile.metallicCharacter = std::clamp(m_profile.metallicCharacter, 0.0f, 1.0f);
    m_profile.metallicFrequencyHz = std::clamp(m_profile.metallicFrequencyHz, 800.0f, 7000.0f);
    m_profile.mechanicalPresence = std::clamp(m_profile.mechanicalPresence, 0.0f, 1.0f);
    m_profile.intakePresence = std::clamp(m_profile.intakePresence, 0.0f, 1.0f);
    m_profile.intakeFrequencyHz = std::clamp(m_profile.intakeFrequencyHz, 250.0f, 4500.0f);
    m_profile.intakeThroat = std::clamp(m_profile.intakeThroat, 0.0f, 1.0f);
    m_profile.airboxDamping = std::clamp(m_profile.airboxDamping, 0.0f, 1.0f);
    m_profile.exhaustMuffling = std::clamp(m_profile.exhaustMuffling, 0.0f, 1.0f);
    m_profile.exhaustBodyGainDb = std::clamp(m_profile.exhaustBodyGainDb, -12.0f, 12.0f);
    m_profile.exhaustBodyFrequencyHz = std::clamp(m_profile.exhaustBodyFrequencyHz, 40.0f, 350.0f);
    m_profile.exhaustBodyQ = std::clamp(m_profile.exhaustBodyQ, 0.2f, 4.0f);
    m_profile.exhaustRasp = std::clamp(m_profile.exhaustRasp, 0.0f, 1.0f);
    m_profile.exhaustDrone = std::clamp(m_profile.exhaustDrone, 0.0f, 1.0f);
    m_profile.tailpipeBrightness = std::clamp(m_profile.tailpipeBrightness, 0.0f, 1.0f);
    m_profile.cabinDamping = std::clamp(m_profile.cabinDamping, 0.0f, 1.0f);
    m_profile.firewallDamping = std::clamp(m_profile.firewallDamping, 0.0f, 1.0f);
    m_profile.glassLeak = std::clamp(m_profile.glassLeak, 0.0f, 1.0f);
    m_profile.cabinBoom = std::clamp(m_profile.cabinBoom, 0.0f, 1.0f);
    m_profile.windowOpenPreview = std::clamp(m_profile.windowOpenPreview, 0.0f, 1.0f);
    m_profile.cabinLowFrequencyLeak = std::clamp(m_profile.cabinLowFrequencyLeak, 0.0f, 1.0f);
    m_profile.cabinResonance = std::clamp(m_profile.cabinResonance, 0.0f, 1.0f);
    m_profile.cabinResonanceHz = std::clamp(m_profile.cabinResonanceHz, 40.0f, 300.0f);
    m_profile.reverbPreview = std::clamp(m_profile.reverbPreview, 0.0f, 0.6f);
    m_profile.occlusionPreview = std::clamp(m_profile.occlusionPreview, 0.0f, 1.0f);
    m_profile.outputGainDb = std::clamp(m_profile.outputGainDb, -18.0f, 12.0f);
}

bool EngineSoundCaptureLab::saveProfile(const std::string& name)
{
    const std::string token = safeToken(name.empty() ? m_profileName : name);
    const std::filesystem::path path = m_profileRoot / (token + ".hacoustic");
    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        m_lastError = "Could not write acoustic profile: " + path.string();
        return false;
    }
    file << std::setprecision(9);
    const auto write = [&](const char* key, float value) { file << key << '=' << value << '\n'; };
    write("inputGainDb", m_profile.inputGainDb);
    write("highPassHz", m_profile.highPassHz);
    write("lowPassHz", m_profile.lowPassHz);
    write("bodyGainDb", m_profile.bodyGainDb);
    write("bodyFrequencyHz", m_profile.bodyFrequencyHz);
    write("bodyQ", m_profile.bodyQ);
    write("presenceCutDb", m_profile.presenceCutDb);
    write("presenceFrequencyHz", m_profile.presenceFrequencyHz);
    write("presenceQ", m_profile.presenceQ);
    write("highShelfDb", m_profile.highShelfDb);
    write("pulseSoftening", m_profile.pulseSoftening);
    write("saturation", m_profile.saturation);
    write("combustionPunch", m_profile.combustionPunch);
    write("metallicCharacter", m_profile.metallicCharacter);
    write("metallicFrequencyHz", m_profile.metallicFrequencyHz);
    write("mechanicalPresence", m_profile.mechanicalPresence);
    write("intakePresence", m_profile.intakePresence);
    write("intakeFrequencyHz", m_profile.intakeFrequencyHz);
    write("intakeThroat", m_profile.intakeThroat);
    write("airboxDamping", m_profile.airboxDamping);
    write("exhaustMuffling", m_profile.exhaustMuffling);
    write("exhaustBodyGainDb", m_profile.exhaustBodyGainDb);
    write("exhaustBodyFrequencyHz", m_profile.exhaustBodyFrequencyHz);
    write("exhaustBodyQ", m_profile.exhaustBodyQ);
    write("exhaustRasp", m_profile.exhaustRasp);
    write("exhaustDrone", m_profile.exhaustDrone);
    write("tailpipeBrightness", m_profile.tailpipeBrightness);
    write("cabinDamping", m_profile.cabinDamping);
    write("firewallDamping", m_profile.firewallDamping);
    write("glassLeak", m_profile.glassLeak);
    write("cabinBoom", m_profile.cabinBoom);
    write("windowOpenPreview", m_profile.windowOpenPreview);
    write("cabinLowFrequencyLeak", m_profile.cabinLowFrequencyLeak);
    write("cabinResonance", m_profile.cabinResonance);
    write("cabinResonanceHz", m_profile.cabinResonanceHz);
    write("reverbPreview", m_profile.reverbPreview);
    write("occlusionPreview", m_profile.occlusionPreview);
    write("outputGainDb", m_profile.outputGainDb);
    if (!file)
    {
        m_lastError = "Could not finish acoustic profile: " + path.string();
        return false;
    }
    m_profileName = token;
    m_lastError.clear();
    return true;
}

bool EngineSoundCaptureLab::loadProfile(const std::string& name)
{
    const std::string token = safeToken(name.empty() ? m_profileName : name);
    const std::filesystem::path path = m_profileRoot / (token + ".hacoustic");
    std::ifstream file(path);
    if (!file)
    {
        m_lastError = "Acoustic profile was not found: " + path.string();
        return false;
    }

    EngineSoundAcousticProfile loaded = m_profile;
    std::string line;
    while (std::getline(file, line))
    {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos)
            continue;
        const std::string key = line.substr(0, separator);
        float value = 0.0f;
        try
        {
            value = std::stof(line.substr(separator + 1));
        }
        catch (...)
        {
            continue;
        }
#define HERITAGE_PROFILE_FIELD(name) if (key == #name) loaded.name = value; else
        HERITAGE_PROFILE_FIELD(inputGainDb)
        HERITAGE_PROFILE_FIELD(highPassHz)
        HERITAGE_PROFILE_FIELD(lowPassHz)
        HERITAGE_PROFILE_FIELD(bodyGainDb)
        HERITAGE_PROFILE_FIELD(bodyFrequencyHz)
        HERITAGE_PROFILE_FIELD(bodyQ)
        HERITAGE_PROFILE_FIELD(presenceCutDb)
        HERITAGE_PROFILE_FIELD(presenceFrequencyHz)
        HERITAGE_PROFILE_FIELD(presenceQ)
        HERITAGE_PROFILE_FIELD(highShelfDb)
        HERITAGE_PROFILE_FIELD(pulseSoftening)
        HERITAGE_PROFILE_FIELD(saturation)
        HERITAGE_PROFILE_FIELD(combustionPunch)
        HERITAGE_PROFILE_FIELD(metallicCharacter)
        HERITAGE_PROFILE_FIELD(metallicFrequencyHz)
        HERITAGE_PROFILE_FIELD(mechanicalPresence)
        HERITAGE_PROFILE_FIELD(intakePresence)
        HERITAGE_PROFILE_FIELD(intakeFrequencyHz)
        HERITAGE_PROFILE_FIELD(intakeThroat)
        HERITAGE_PROFILE_FIELD(airboxDamping)
        HERITAGE_PROFILE_FIELD(exhaustMuffling)
        HERITAGE_PROFILE_FIELD(exhaustBodyGainDb)
        HERITAGE_PROFILE_FIELD(exhaustBodyFrequencyHz)
        HERITAGE_PROFILE_FIELD(exhaustBodyQ)
        HERITAGE_PROFILE_FIELD(exhaustRasp)
        HERITAGE_PROFILE_FIELD(exhaustDrone)
        HERITAGE_PROFILE_FIELD(tailpipeBrightness)
        HERITAGE_PROFILE_FIELD(cabinDamping)
        HERITAGE_PROFILE_FIELD(firewallDamping)
        HERITAGE_PROFILE_FIELD(glassLeak)
        HERITAGE_PROFILE_FIELD(cabinBoom)
        HERITAGE_PROFILE_FIELD(windowOpenPreview)
        HERITAGE_PROFILE_FIELD(cabinLowFrequencyLeak)
        HERITAGE_PROFILE_FIELD(cabinResonance)
        HERITAGE_PROFILE_FIELD(cabinResonanceHz)
        HERITAGE_PROFILE_FIELD(reverbPreview)
        HERITAGE_PROFILE_FIELD(occlusionPreview)
        HERITAGE_PROFILE_FIELD(outputGainDb)
        { }
#undef HERITAGE_PROFILE_FIELD
    }
    setProfile(loaded);
    m_profileName = token;
    m_lastError.clear();
    return true;
}

EngineSoundCaptureStatus EngineSoundCaptureLab::status() const
{
    EngineSoundCaptureStatus value;
#ifdef _WIN32
    value.available = m_audio.isAvailable();
#else
    value.available = false;
#endif
    value.capturing = m_capturing.load(std::memory_order_acquire);
    value.previewPlaying = m_previewHandle != kInvalidAudioHandle
        && m_audio.isPlaying(m_previewHandle);
    value.lastCaptureBank = m_lastCaptureWasBank;
    value.lastCaptureRpm = m_lastCaptureRpm;
    value.lastCaptureThrottlePercent = m_lastCaptureThrottlePercent;
    value.progress = m_progress.load(std::memory_order_acquire);
    value.requestedDurationSeconds = m_requestedDurationSeconds;
    value.capturedDurationSeconds = m_lastDurationSeconds;
    value.peak = m_lastPeak;
    value.rms = m_lastRms;
    value.sampleRate = m_lastRawSampleRate;
    value.lastRawPath = m_lastRawPath;
    value.lastPreviewPath = m_lastPreviewPath;
    value.profileName = m_profileName;
    value.lastError = m_lastError;
    return value;
}


EngineSoundAnalysis EngineSoundCaptureLab::analysis() const
{
    return m_lastAnalysis;
}

std::filesystem::path EngineSoundCaptureLab::bankCapturePath(
    const std::string& vehicleId,
    const std::string& engineId,
    int rpm,
    int throttlePercent) const
{
    const std::string vehicle = safeToken(vehicleId);
    const std::string engine = safeToken(engineId);
    rpm = std::clamp(rpm, 0, 20000);
    throttlePercent = std::clamp(throttlePercent, 0, 100);
    const auto directory = m_bankRoot / vehicle / engine / "steady";
    std::ostringstream filename;
    if (rpm <= 900 && throttlePercent <= 25)
        filename << "idle_" << std::setw(4) << std::setfill('0') << rpm << "rpm.wav";
    else
        filename << "rpm_" << std::setw(4) << std::setfill('0') << rpm
            << "_throttle_" << std::setw(3) << throttlePercent << ".wav";
    return directory / filename.str();
}

bool EngineSoundCaptureLab::bankCaptureExists(
    const std::string& vehicleId,
    const std::string& engineId,
    int rpm,
    int throttlePercent) const
{
    std::error_code ec;
    return std::filesystem::exists(
        bankCapturePath(vehicleId, engineId, rpm, throttlePercent), ec);
}

bool EngineSoundCaptureLab::writeFloatStereoWav(
    const std::filesystem::path& path,
    const std::vector<float>& stereo,
    std::uint32_t sampleRate,
    std::string& error) const
{
    if (stereo.empty() || (stereo.size() & 1U) != 0 || sampleRate < 8000)
    {
        error = "Invalid stereo float data for WAV output.";
        return false;
    }
    const std::uint64_t dataBytes64 = stereo.size() * sizeof(float);
    if (dataBytes64 > 0xffffffffULL - 44ULL)
    {
        error = "Capture is too large for a RIFF/WAV file.";
        return false;
    }
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError)
    {
        error = "Could not create WAV directory: " + directoryError.message();
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        error = "Could not create WAV file: " + path.string();
        return false;
    }
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(dataBytes64);
    file.write("RIFF", 4);
    writeU32(file, 36U + dataBytes);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    writeU32(file, 16);
    writeU16(file, 3); // WAVE_FORMAT_IEEE_FLOAT
    writeU16(file, 2);
    writeU32(file, sampleRate);
    writeU32(file, sampleRate * 2U * sizeof(float));
    writeU16(file, static_cast<std::uint16_t>(2U * sizeof(float)));
    writeU16(file, 32);
    file.write("data", 4);
    writeU32(file, dataBytes);
    file.write(
        reinterpret_cast<const char*>(stereo.data()),
        static_cast<std::streamsize>(dataBytes));
    if (!file)
    {
        error = "Could not finish WAV file: " + path.string();
        return false;
    }
    error.clear();
    return true;
}

bool EngineSoundCaptureLab::appendManifest(
    const CompletedCapture& completed,
    std::string& error) const
{
    const auto directory = m_bankRoot
        / completed.request.vehicleId
        / completed.request.engineId;
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);
    if (directoryError)
    {
        error = "Could not create capture manifest directory: " + directoryError.message();
        return false;
    }
    const auto path = directory / "capture_manifest.csv";
    const bool needsHeader = !std::filesystem::exists(path);
    std::ofstream file(path, std::ios::app);
    if (!file)
    {
        error = "Could not append capture manifest: " + path.string();
        return false;
    }
    if (needsHeader)
        file << "vehicle,engine,rpm,throttle_percent,duration_seconds,sample_rate,peak,rms,file\n";
    const float duration = completed.sampleRate > 0
        ? static_cast<float>(completed.stereo.size() / 2)
            / static_cast<float>(completed.sampleRate)
        : 0.0f;
    file << completed.request.vehicleId << ','
        << completed.request.engineId << ','
        << completed.request.rpm << ','
        << completed.request.throttlePercent << ','
        << std::fixed << std::setprecision(4) << duration << ','
        << completed.sampleRate << ','
        << std::setprecision(6) << completed.peak << ','
        << completed.rms << ','
        << "steady/" << completed.request.outputPath.filename().string() << '\n';
    if (!file)
    {
        error = "Could not finish capture manifest: " + path.string();
        return false;
    }
    error.clear();
    return true;
}

std::string EngineSoundCaptureLab::safeToken(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    for (const unsigned char c : value)
    {
        if ((c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9')
            || c == '-' || c == '_')
        {
            result.push_back(static_cast<char>(c));
        }
        else if (c == ' ' || c == '/' || c == '\\')
        {
            result.push_back('_');
        }
    }
    if (result.empty())
        result = "unnamed";
    return result.substr(0, 80);
}

void EngineSoundCaptureLab::measure(
    const std::vector<float>& samples,
    float& peak,
    float& rms)
{
    peak = 0.0f;
    double squareSum = 0.0;
    for (const float sample : samples)
    {
        const float magnitude = std::abs(std::isfinite(sample) ? sample : 0.0f);
        peak = (std::max)(peak, magnitude);
        squareSum += static_cast<double>(magnitude) * static_cast<double>(magnitude);
    }
    rms = samples.empty()
        ? 0.0f
        : static_cast<float>(std::sqrt(squareSum / static_cast<double>(samples.size())));
}

} // namespace heritage::audio::lab
