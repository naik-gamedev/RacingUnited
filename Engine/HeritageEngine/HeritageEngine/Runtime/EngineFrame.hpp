#pragma once

struct GLFWwindow;

namespace heritage::audio { class AudioSystem; }
namespace heritage::diagnostics { class PerformanceMonitor; }
namespace heritage::graphics { class WindowSystem; }
namespace heritage::input { class InputSystem; }
namespace heritage::settings { struct VideoSettings; }
namespace heritage::timing { class FrameLimiter; }
namespace heritage::engine { class DisplayModeController; }

namespace heritage::engine {

struct EngineFrameState final
{
    bool appliedVSync = false;
    double prevTime = 0.0;
    double previousScreenshotCaptureSeconds = 0.0;
};

struct EngineFrameData final
{
    double activeFrameStart = 0.0;
    double housekeepingCpuStart = 0.0;
    double now = 0.0;
    float dt = 0.0f;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
};

void beginEngineFrame(
    EngineFrameState& state,
    EngineFrameData& frame,
    GLFWwindow* window,
    heritage::settings::VideoSettings& videoSettings,
    heritage::input::InputSystem& input,
    heritage::audio::AudioSystem& audio,
    heritage::timing::FrameLimiter& frameLimiter,
    heritage::diagnostics::PerformanceMonitor& performanceMonitor,
    bool& shouldMinimize);

bool completeEngineFrameSetup(
    EngineFrameData& frame,
    GLFWwindow* window,
    heritage::graphics::WindowSystem& windowSystem,
    DisplayModeController& displayModeController,
    const heritage::settings::VideoSettings& videoSettings,
    heritage::timing::FrameLimiter& frameLimiter);

void finalizeEngineFrameTiming(
    EngineFrameState& state,
    EngineFrameData& frame,
    heritage::diagnostics::PerformanceMonitor& performanceMonitor);

double captureRequestedScreenshot(
    EngineFrameState& state,
    GLFWwindow* window,
    int framebufferWidth,
    int framebufferHeight,
    int& screenshotClipboardRefreshFrames);

void presentEngineFrame(
    const EngineFrameData& frame,
    GLFWwindow* window,
    const heritage::settings::VideoSettings& videoSettings,
    heritage::timing::FrameLimiter& frameLimiter,
    heritage::diagnostics::PerformanceMonitor& performanceMonitor,
    double screenshotCaptureMsThisFrame);

} // namespace heritage::engine
