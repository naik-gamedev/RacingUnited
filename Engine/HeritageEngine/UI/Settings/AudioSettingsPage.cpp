#include "AudioSettingsPage.hpp"

#include <imgui.h>

namespace heritage::ui::settings {
namespace {

bool volumeSlider(const char* label, float current, float& changedValue)
{
    float percent = current * 100.0f;
    ImGui::SetNextItemWidth(220.0f);
    if (!ImGui::SliderFloat(label, &percent, 0.0f, 100.0f, "%.0f%%"))
        return false;

    changedValue = percent / 100.0f;
    return true;
}

} // namespace

void drawAudioSettingsPage(heritage::audio::AudioSystem& audio)
{
    ImGui::Spacing();
    ImGui::TextDisabled("Audio backend");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Backend: %s", audio.backendName().c_str());
    if (!audio.isAvailable())
    {
        ImGui::TextWrapped(
            "Audio is unavailable. The engine will continue silently. %s",
            audio.lastError().c_str());
    }
    else
    {
        ImGui::TextDisabled("Current format support: PCM or IEEE-float WAV.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Volume buses");
    ImGui::Separator();
    ImGui::Spacing();

    float value = 0.0f;
    if (volumeSlider("Master Volume", audio.masterVolume(), value))
        audio.setMasterVolume(value);
    if (volumeSlider("Music Volume", audio.busVolume(heritage::audio::AudioBus::Music), value))
        audio.setBusVolume(heritage::audio::AudioBus::Music, value);
    if (volumeSlider("Effects Volume", audio.busVolume(heritage::audio::AudioBus::Effects), value))
        audio.setBusVolume(heritage::audio::AudioBus::Effects, value);
    if (volumeSlider("Ambience Volume", audio.busVolume(heritage::audio::AudioBus::Ambience), value))
        audio.setBusVolume(heritage::audio::AudioBus::Ambience, value);
    if (volumeSlider("UI Volume", audio.busVolume(heritage::audio::AudioBus::UI), value))
        audio.setBusVolume(heritage::audio::AudioBus::UI, value);
    if (volumeSlider("Voice / Radio", audio.busVolume(heritage::audio::AudioBus::Voice), value))
        audio.setBusVolume(heritage::audio::AudioBus::Voice, value);

    ImGui::Spacing();
    bool muteWhenUnfocused = audio.muteWhenUnfocused();
    if (ImGui::Checkbox("Mute when unfocused", &muteWhenUnfocused))
        audio.setMuteWhenUnfocused(muteWhenUnfocused);

    ImGui::Spacing();
    ImGui::TextDisabled(
        "These values are saved independently for the active module.");
}

} // namespace heritage::ui::settings
