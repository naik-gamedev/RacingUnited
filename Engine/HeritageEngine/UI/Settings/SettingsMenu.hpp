#pragma once

#include <GLFW/glfw3.h>

#include "VideoSettingsPage.hpp"
#include "../../Audio/AudioSystem.hpp"
#include "../../Input/InputSystem.hpp"

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
    const DisplayChangeHandler& initiateDisplayChange);

} // namespace heritage::ui::settings
