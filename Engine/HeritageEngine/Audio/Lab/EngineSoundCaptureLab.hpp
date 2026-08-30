#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "EngineSoundLabDsp.hpp"
#include "../AudioSystem.hpp"

namespace heritage::audio::lab {

struct EngineSoundCaptureStatus
{
    bool available = false;
    bool capturing = false;
    bool previewPlaying = false;
    bool lastCaptureBank = false;
    int lastCaptureRpm = 0;
    int lastCaptureThrottlePercent = 0;
    float progress = 0.0f;
    float requestedDurationSeconds = 0.0f;
    float capturedDurationSeconds = 0.0f;
    float peak = 0.0f;
    float rms = 0.0f;
    std::uint32_t sampleRate = 48000;
    std::filesystem::path lastRawPath;
    std::filesystem::path lastPreviewPath;
    std::string profileName = "Peugeot206RC_EW10J4S";
    std::string lastError;
};

class EngineSoundCaptureLab
{
public:
    EngineSoundCaptureLab(
        AudioSystem& audio,
        const std::filesystem::path& moduleUserRoot);
    ~EngineSoundCaptureLab();

    EngineSoundCaptureLab(const EngineSoundCaptureLab&) = delete;
    EngineSoundCaptureLab& operator=(const EngineSoundCaptureLab&) = delete;

    void update();

    bool startCalibrationCapture(float durationSeconds);
    bool startBankCapture(
        const std::string& vehicleId,
        const std::string& engineId,
        int rpm,
        int throttlePercent,
        float durationSeconds);
    void stopCapture();

    bool playPreview(EngineSoundPerspective perspective);
    void stopPreview();

    const EngineSoundAcousticProfile& profile() const { return m_profile; }
    void setProfile(const EngineSoundAcousticProfile& profile);
    bool saveProfile(const std::string& name);
    bool loadProfile(const std::string& name);

    EngineSoundCaptureStatus status() const;
    EngineSoundAnalysis analysis() const;
    bool bankCaptureExists(
        const std::string& vehicleId,
        const std::string& engineId,
        int rpm,
        int throttlePercent) const;
    const std::filesystem::path& root() const { return m_root; }

private:
    struct CaptureRequest
    {
        bool bank = false;
        std::string vehicleId;
        std::string engineId;
        int rpm = 0;
        int throttlePercent = 0;
        float durationSeconds = 4.0f;
        std::filesystem::path outputPath;
    };

    struct CompletedCapture
    {
        bool ready = false;
        bool success = false;
        std::string error;
        CaptureRequest request;
        std::vector<float> stereo;
        std::uint32_t sampleRate = 48000;
        float peak = 0.0f;
        float rms = 0.0f;
    };

    bool startCapture(CaptureRequest request);
    void captureThreadMain(CaptureRequest request);
    void finalizeCompletedCapture();
    bool writeFloatStereoWav(
        const std::filesystem::path& path,
        const std::vector<float>& stereo,
        std::uint32_t sampleRate,
        std::string& error) const;
    bool appendManifest(const CompletedCapture& completed, std::string& error) const;
    std::filesystem::path bankCapturePath(
        const std::string& vehicleId,
        const std::string& engineId,
        int rpm,
        int throttlePercent) const;
    static std::string safeToken(const std::string& value);
    static void measure(
        const std::vector<float>& samples,
        float& peak,
        float& rms);

    AudioSystem& m_audio;
    std::filesystem::path m_root;
    std::filesystem::path m_previewRoot;
    std::filesystem::path m_bankRoot;
    std::filesystem::path m_profileRoot;

    EngineSoundAcousticProfile m_profile;
    std::string m_profileName = "Peugeot206RC_EW10J4S";

    mutable std::mutex m_mutex;
    std::thread m_captureThread;
    std::atomic<bool> m_capturing{ false };
    std::atomic<bool> m_stopRequested{ false };
    std::atomic<float> m_progress{ 0.0f };
    float m_requestedDurationSeconds = 0.0f;
    float m_previousMasterVolume = 1.0f;
    bool m_masterMutedForCapture = false;

    CompletedCapture m_completed;
    std::vector<float> m_lastRawStereo;
    std::uint32_t m_lastRawSampleRate = 48000;
    std::filesystem::path m_lastRawPath;
    std::filesystem::path m_lastPreviewPath;
    bool m_lastCaptureWasBank = false;
    int m_lastCaptureRpm = 0;
    int m_lastCaptureThrottlePercent = 0;
    float m_lastPeak = 0.0f;
    float m_lastRms = 0.0f;
    EngineSoundAnalysis m_lastAnalysis;
    float m_lastDurationSeconds = 0.0f;
    std::string m_lastError;
    AudioHandle m_previewHandle = kInvalidAudioHandle;
};

} // namespace heritage::audio::lab
