#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "PauseMenu.hpp"

#include <algorithm>

namespace heritage::ui {

void drawPauseMenu(
    GLFWwindow* window,
    bool menuOpen,
    bool& menuShowSettings,
    bool& shouldClose,
    int framebufferWidth,
    int framebufferHeight,
    ImFont* fontLarge,
    ImFont* fontNormal,
    heritage::graphics::DisplaySystem& display,
    heritage::graphics::WindowSystem& windowSystem,
    heritage::settings::VideoSettings& videoSettings,
    heritage::audio::AudioSystem& audio,
    heritage::input::InputSystem& input,
    int desktopWidth,
    int desktopHeight,
    const heritage::ui::settings::DisplayChangeHandler& initiateDisplayChange)
{
    if (!menuOpen)
        return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(
        static_cast<float>(framebufferWidth),
        static_cast<float>(framebufferHeight)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.55f));
    ImGui::Begin("##dim", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
    ImGui::End();
    ImGui::PopStyleColor();

    // Keep the normal 420 x 620 layout when space allows, but constrain the
    // menu to the current framebuffer at low resolutions. Once constrained,
    // ImGui's vertical scrollbar exposes the content that no longer fits.
    constexpr float kMenuMargin = 10.0f;
    constexpr float kNormalMenuWidth = 420.0f;
    constexpr float kCollapsedMenuHeight = 180.0f;
    constexpr float kSettingsMenuHeight = 620.0f;

    const float framebufferWidthF = static_cast<float>((std::max)(framebufferWidth, 1));
    const float framebufferHeightF = static_cast<float>((std::max)(framebufferHeight, 1));

    const float maximumMenuWidth = (std::max)(1.0f, framebufferWidthF - kMenuMargin * 2.0f);
    const float maximumMenuHeight = (std::max)(1.0f, framebufferHeightF - kMenuMargin * 2.0f);

    const float desiredMenuHeight = menuShowSettings
        ? kSettingsMenuHeight
        : kCollapsedMenuHeight;

    const float menuWidth = (std::min)(kNormalMenuWidth, maximumMenuWidth);
    const float menuHeight = (std::min)(desiredMenuHeight, maximumMenuHeight);

    const float menuX = (std::max)(0.0f, (framebufferWidthF - menuWidth) * 0.5f);
    const float menuY = (std::max)(0.0f, (framebufferHeightF - menuHeight) * 0.5f);

    ImGui::SetNextWindowPos(ImVec2(menuX, menuY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.07f, 0.98f));

    ImGuiWindowFlags menuFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    if (desiredMenuHeight > menuHeight)
        menuFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;

    ImGui::Begin("##menu", nullptr, menuFlags);

    ImGui::PushFont(fontLarge);
    const float titleWidth = ImGui::CalcTextSize("HERITAGE ENGINE").x;
    ImGui::SetCursorPosX((std::max)(0.0f, (menuWidth - titleWidth) * 0.5f));
    ImGui::SetCursorPosY(16.0f);
    ImGui::Text("HERITAGE ENGINE");
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushFont(fontNormal);
    const float buttonWidth = (std::min)(200.0f, (std::max)(1.0f, menuWidth - 40.0f));
    constexpr float buttonHeight = 38.0f;

    ImGui::SetCursorPosX((std::max)(0.0f, (menuWidth - buttonWidth) * 0.5f));
    if (ImGui::Button(
        menuShowSettings ? "HIDE SETTINGS" : "SETTINGS",
        ImVec2(buttonWidth, buttonHeight)))
    {
        menuShowSettings = !menuShowSettings;
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX((std::max)(0.0f, (menuWidth - buttonWidth) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.08f, 0.08f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.10f, 0.10f, 1));
    if (ImGui::Button("EXIT", ImVec2(buttonWidth, buttonHeight)))
    {
        shouldClose = true;
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    ImGui::PopStyleColor(2);
    ImGui::PopFont();

    if (menuShowSettings)
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushFont(fontNormal);

        heritage::ui::settings::drawSettingsMenu(
            window,
            display,
            windowSystem,
            videoSettings,
            audio,
            input,
            desktopWidth,
            desktopHeight,
            initiateDisplayChange);

        ImGui::PopFont();
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

} // namespace heritage::ui
