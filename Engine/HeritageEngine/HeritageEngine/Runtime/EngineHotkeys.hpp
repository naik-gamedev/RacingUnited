#pragma once

struct GLFWwindow;

namespace heritage::graphics {
class EntityMeshRenderer;
class EnvironmentSystem;
class WindowSystem;
}

namespace heritage::engine {
class DisplayModeController;

struct EngineHotkeyState final
{
    bool f5Prev = false;
    bool f6Prev = false;
    bool f7Prev = false;
    bool f8Prev = false;
    bool f9Prev = false;
    bool f11Prev = false;
    bool insertPrev = false;
    bool printScreenPrev = false;
    bool f12Prev = false;
    bool escPrev = false;

    int screenshotClipboardRefreshFrames = 0;
    bool menuOpen = false;
    bool menuShowSettings = false;
    bool performanceOverlayVisible = true;
    bool wireframeVisible = false;
    bool tireProbeDebugVisible = false;
};

void processEngineHotkeys(
    GLFWwindow* window,
    heritage::graphics::WindowSystem& windowSystem,
    DisplayModeController& displayModeController,
    heritage::graphics::EntityMeshRenderer& entityMeshRenderer,
    heritage::graphics::EnvironmentSystem& environmentSystem,
    EngineHotkeyState& state);

} // namespace heritage::engine
