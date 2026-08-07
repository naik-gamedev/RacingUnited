#pragma once

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "../Core/Settings/VideoSettings.hpp"
#include "../Audio/AudioSystem.hpp"
#include "../Input/InputSystem.hpp"
#include "Settings/SettingsMenu.hpp"

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
    const heritage::ui::settings::DisplayChangeHandler& initiateDisplayChange);

} // namespace heritage::ui
