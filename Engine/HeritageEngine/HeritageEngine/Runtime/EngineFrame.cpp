#include "EngineFrame.hpp"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>

#include "../Display/DisplayModeController.hpp"
#include "../../Audio/AudioSystem.hpp"
#include "../../Core/Diagnostics/PerformanceMonitor.hpp"
#include "../../Core/Settings/VideoSettings.hpp"
#include "../../Core/Timing/FrameLimiter.hpp"
#include "../../Graphics/WindowSystem.hpp"
#include "../../Input/InputSystem.hpp"
#include "../../Platform/Windows/BackbufferClipboard.hpp"

namespace heritage::engine {

void beginEngineFrame(
    EngineFrameState& state,
    EngineFrameData& frame,
    GLFWwindow* window,
    heritage::settings::VideoSettings& videoSettings,
    heritage::input::InputSystem& input,
    heritage::audio::AudioSystem& audio,
    heritage::timing::FrameLimiter& frameLimiter,
    heritage::diagnostics::PerformanceMonitor& performanceMonitor,
    bool& shouldMinimize)
{
    using heritage::diagnostics::PerformanceSection;

    if (state.appliedVSync != videoSettings.vsyncEnabled)
    {
        glfwSwapInterval(videoSettings.vsyncEnabled ? 1 : 0);
        state.appliedVSync = videoSettings.vsyncEnabled;
        frameLimiter.reset();
    }

    frameLimiter.beginFrame();
    frame.activeFrameStart = glfwGetTime();

    const double eventsInputCpuStart = glfwGetTime();
    glfwPollEvents();
    input.update();
    performanceMonitor.recordSection(
        PerformanceSection::EventsInput,
        (glfwGetTime() - eventsInputCpuStart) * 1000.0);

    const double audioCpuStart = glfwGetTime();
    audio.update(glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE);
    performanceMonitor.recordSection(
        PerformanceSection::Audio,
        (glfwGetTime() - audioCpuStart) * 1000.0);

    frame.housekeepingCpuStart = glfwGetTime();
    if (shouldMinimize)
    {
        glfwIconifyWindow(window);
        shouldMinimize = false;
    }
}

bool completeEngineFrameSetup(
    EngineFrameData& frame,
    GLFWwindow* window,
    heritage::graphics::WindowSystem& windowSystem,
    DisplayModeController& displayModeController,
    const heritage::settings::VideoSettings& videoSettings,
    heritage::timing::FrameLimiter& frameLimiter)
{
    using heritage::settings::selectedFpsCap;

    windowSystem.update(window);
    displayModeController.enforceSpanCompatibility();

    glfwGetFramebufferSize(
        window,
        &frame.framebufferWidth,
        &frame.framebufferHeight);
    if (frame.framebufferWidth == 0 || frame.framebufferHeight == 0)
    {
        glfwSwapBuffers(window);
        frameLimiter.endFrame(selectedFpsCap(videoSettings));
        return false;
    }
    return true;
}

void finalizeEngineFrameTiming(
    EngineFrameState& state,
    EngineFrameData& frame,
    heritage::diagnostics::PerformanceMonitor& performanceMonitor)
{
    using heritage::diagnostics::PerformanceSection;

    frame.now = glfwGetTime();
    double frameDeltaSeconds = frame.now - state.prevTime;
    state.prevTime = frame.now;

    // SCREEN03/PERF09: F12 intentionally performs a synchronous GPU readback
    // for an exact clipboard capture. Do not feed that author-requested pause
    // into gameplay time, frametime percentiles, or hitch diagnostics.
    if (state.previousScreenshotCaptureSeconds > 0.0)
    {
        frameDeltaSeconds = (std::max)(
            0.0,
            frameDeltaSeconds - state.previousScreenshotCaptureSeconds);
        state.previousScreenshotCaptureSeconds = 0.0;
    }

    frame.dt = static_cast<float>(frameDeltaSeconds);
    performanceMonitor.beginFrame(frameDeltaSeconds);
    performanceMonitor.recordSection(
        PerformanceSection::Housekeeping,
        (frame.now - frame.housekeepingCpuStart) * 1000.0);
}

double captureRequestedScreenshot(
    EngineFrameState& state,
    GLFWwindow* window,
    int framebufferWidth,
    int framebufferHeight,
    int& screenshotClipboardRefreshFrames)
{
    double screenshotCaptureMsThisFrame = 0.0;
#ifdef _WIN32
    if (screenshotClipboardRefreshFrames > 0)
    {
        const double screenshotCaptureStart = glfwGetTime();
        const bool screenshotCopied =
            heritage::platform::windows::copyBackbufferToClipboard(
                window,
                framebufferWidth,
                framebufferHeight);
        const double screenshotCaptureSeconds =
            glfwGetTime() - screenshotCaptureStart;
        screenshotCaptureMsThisFrame = screenshotCaptureSeconds * 1000.0;
        state.previousScreenshotCaptureSeconds = screenshotCaptureSeconds;

        if (screenshotCopied)
        {
            std::cout
                << "F12: exact Heritage frame copied to clipboard ("
                << screenshotCaptureMsThisFrame
                << " ms capture pause excluded from gameplay/perf diagnostics)\n";
        }
        --screenshotClipboardRefreshFrames;
    }
#else
    (void)state;
    (void)window;
    (void)framebufferWidth;
    (void)framebufferHeight;
    (void)screenshotClipboardRefreshFrames;
#endif
    return screenshotCaptureMsThisFrame;
}

void presentEngineFrame(
    const EngineFrameData& frame,
    GLFWwindow* window,
    const heritage::settings::VideoSettings& videoSettings,
    heritage::timing::FrameLimiter& frameLimiter,
    heritage::diagnostics::PerformanceMonitor& performanceMonitor,
    double screenshotCaptureMsThisFrame)
{
    using heritage::diagnostics::PerformanceSection;
    using heritage::settings::selectedFpsCap;

    const double presentCpuStart = glfwGetTime();
    glfwSwapBuffers(window);
    performanceMonitor.recordSection(
        PerformanceSection::Present,
        (glfwGetTime() - presentCpuStart) * 1000.0);
    performanceMonitor.endFrame(
        (std::max)(
            0.0,
            (glfwGetTime() - frame.activeFrameStart) * 1000.0
                - screenshotCaptureMsThisFrame));
    frameLimiter.endFrame(selectedFpsCap(videoSettings));
}

} // namespace heritage::engine
