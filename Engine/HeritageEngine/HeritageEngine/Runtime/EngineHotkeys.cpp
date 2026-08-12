#include "EngineHotkeys.hpp"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <iostream>

#include "../Display/DisplayModeController.hpp"
#include "../../Graphics/EnvironmentSystem.hpp"
#include "../../Graphics/Renderer/EntityMeshRenderer.hpp"
#include "../../Graphics/WindowSystem.hpp"

namespace heritage::engine {

void processEngineHotkeys(
    GLFWwindow* window,
    heritage::graphics::WindowSystem& windowSystem,
    DisplayModeController& displayModeController,
    heritage::graphics::EntityMeshRenderer& entityMeshRenderer,
    heritage::graphics::EnvironmentSystem& environmentSystem,
    EngineHotkeyState& state)
{
    // F5 is the explicit development hot-reload boundary. Keeping asset/Lua
    // filesystem scans off the gameplay thread removes periodic hitch pulses.
    const bool f5Now = (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS);
    if (f5Now && !state.f5Prev)
        entityMeshRenderer.requestHotReloadPoll();
    state.f5Prev = f5Now;

    // Developer environment preview shortcuts. These are intentionally
    // simple now; Heritage Editor can expose the same EnvironmentSystem
    // controls visually later.
    const bool f6Now = (glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS);
    if (f6Now && !state.f6Prev)
    {
        environmentSystem.setCycleEnabled(!environmentSystem.cycleEnabled());
        std::cout << "Environment cycle: "
            << (environmentSystem.cycleEnabled() ? "enabled" : "paused")
            << " at " << environmentSystem.timeOfDayHours() << " h\n";
    }
    state.f6Prev = f6Now;

    const bool f7Now = (glfwGetKey(window, GLFW_KEY_F7) == GLFW_PRESS);
    if (f7Now && !state.f7Prev)
    {
        constexpr std::array<float, 4> kPreviewSpeeds{ 1.0f, 60.0f, 240.0f, 1440.0f };
        std::size_t nextIndex = 0;
        float bestDistance = std::abs(environmentSystem.timeScale() - kPreviewSpeeds[0]);
        for (std::size_t index = 1; index < kPreviewSpeeds.size(); ++index)
        {
            const float distance = std::abs(environmentSystem.timeScale() - kPreviewSpeeds[index]);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                nextIndex = index;
            }
        }
        nextIndex = (nextIndex + 1) % kPreviewSpeeds.size();
        environmentSystem.setTimeScale(kPreviewSpeeds[nextIndex]);
        std::cout << "Environment time scale: "
            << environmentSystem.timeScale() << "x\n";
    }
    state.f7Prev = f7Now;

    const bool f8Now = (glfwGetKey(window, GLFW_KEY_F8) == GLFW_PRESS);
    if (f8Now && !state.f8Prev)
        state.performanceOverlayVisible = !state.performanceOverlayVisible;
    state.f8Prev = f8Now;

    // F9 toggles a scene-only wireframe inspection mode. It deliberately does
    // not change shadow-map rasterization, post processing, or ImGui so the
    // authored/deformed mesh topology can be inspected without making the
    // debugging UI itself unreadable.
    const bool f9Now = (glfwGetKey(window, GLFW_KEY_F9) == GLFW_PRESS);
    if (f9Now && !state.f9Prev)
    {
        state.wireframeVisible = !state.wireframeVisible;
        std::cout << "Wireframe inspection: "
            << (state.wireframeVisible ? "enabled" : "disabled")
            << " (F9)\n";
    }
    state.f9Prev = f9Now;

    // TIRE27/VIS20: INSERT toggles the tire-local dense 21x13 probe diagnostic
    // directly on the visible rubber. This intentionally avoids a buried UI
    // setting while tire deformation is under active development.
    const bool insertNow = (glfwGetKey(window, GLFW_KEY_INSERT) == GLFW_PRESS);
    if (insertNow && !state.insertPrev)
    {
        state.tireProbeDebugVisible = !state.tireProbeDebugVisible;
        entityMeshRenderer.setTireProbeDebugVisible(state.tireProbeDebugVisible);
        std::cout << "Tire probe debug: "
            << (state.tireProbeDebugVisible ? "enabled" : "disabled")
            << " (Insert)\n";
    }
    state.insertPrev = insertNow;

    // F12 is Heritage's authoritative screenshot key. It does not invoke the
    // Windows PrintScreen compositor path, so the OS cannot replace our fresh
    // OpenGL backbuffer capture with a stale desktop frame afterwards.
    const bool f12Now = (glfwGetKey(window, GLFW_KEY_F12) == GLFW_PRESS);
    if (f12Now && !state.f12Prev)
        state.screenshotClipboardRefreshFrames = 1;
    state.f12Prev = f12Now;

    // Keep PrintScreen detectable only for a console hint. Windows owns this
    // key and may overwrite the clipboard after Heritage, so it is intentionally
    // not used for engine-native capture anymore.
    const bool printScreenNow =
        (glfwGetKey(window, GLFW_KEY_PRINT_SCREEN) == GLFW_PRESS);
    if (printScreenNow && !state.printScreenPrev)
        std::cout << "PrintScreen is OS-controlled; use F12 for exact Heritage clipboard capture.\n";
    state.printScreenPrev = printScreenNow;

    // F11
    const bool f11Now = (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS);
    if (f11Now && !state.f11Prev)
    {
        windowSystem.cycleMode(window);
        displayModeController.enforceSpanCompatibility();
        displayModeController.syncVideoSettings(window);
    }
    state.f11Prev = f11Now;

    // ESC
    const bool escNow = (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS);
    if (escNow && !state.escPrev)
    {
        state.menuOpen = !state.menuOpen;
        if (!state.menuOpen)
            state.menuShowSettings = false;
    }
    state.escPrev = escNow;
}

} // namespace heritage::engine
