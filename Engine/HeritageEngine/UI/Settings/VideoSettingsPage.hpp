#pragma once

#include <functional>

#include <GLFW/glfw3.h>

#include "../../Core/Settings/VideoSettings.hpp"
#include "../../Graphics/DisplaySystem.hpp"
#include "../../Graphics/WindowSystem.hpp"

namespace heritage::ui::settings {

using DisplayChangeHandler = std::function<void(
    heritage::graphics::WindowMode newMode,
    int desiredWidth,
    int desiredHeight,
    int desiredRefreshRate)>;

void drawVideoSettingsPage(
    GLFWwindow* window,
    heritage::graphics::DisplaySystem& display,
    heritage::graphics::WindowSystem& windowSystem,
    heritage::settings::VideoSettings& videoSettings,
    int desktopWidth,
    int desktopHeight,
    const DisplayChangeHandler& initiateDisplayChange);

} // namespace heritage::ui::settings
