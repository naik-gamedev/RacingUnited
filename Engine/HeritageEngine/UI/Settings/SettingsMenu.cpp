#include "SettingsMenu.hpp"

#include <imgui.h>

#include "AudioSettingsPage.hpp"
#include "GameplaySettingsPage.hpp"
#include "InputSettingsPage.hpp"

namespace heritage::ui::settings {

void drawSettingsMenu(
    GLFWwindow* window,
    heritage::graphics::DisplaySystem& display,
    heritage::graphics::WindowSystem& windowSystem,
    heritage::settings::VideoSettings& videoSettings,
    heritage::audio::AudioSystem& audio,
    heritage::input::InputSystem& input,
    int desktopWidth,
    int desktopHeight,
    const DisplayChangeHandler& initiateDisplayChange)
{
    if (!ImGui::BeginTabBar("MenuSettings"))
        return;

    if (ImGui::BeginTabItem("Video"))
    {
        drawVideoSettingsPage(
            window,
            display,
            windowSystem,
            videoSettings,
            desktopWidth,
            desktopHeight,
            initiateDisplayChange);

        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Audio"))
    {
        drawAudioSettingsPage(audio);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Input"))
    {
        drawInputSettingsPage(input);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Gameplay"))
    {
        drawGameplaySettingsPage();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

} // namespace heritage::ui::settings
