#pragma once

#include <array>
#include <cstddef>

#include <glad/glad.h>

#include "../../Graphics/Framebuffer/PostFramebuffer.hpp"
#include "../../Graphics/PostProcessing/PostProcessor.hpp"
#include "../../Graphics/RenderScaler.hpp"

namespace heritage::camera { struct CameraFrame; }
namespace heritage::diagnostics { class PerformanceMonitor; }
namespace heritage::entities { class EntityRegistry; }
namespace heritage::graphics {
class DisplaySystem;
class EntityDebugRenderer;
class EntityMeshRenderer;
class SurfacePresentationRenderer;
}
namespace heritage::modules { class ModuleRuntimeManager; }
namespace heritage::physics { class SurfaceWorld; }
namespace heritage::settings { struct VideoSettings; }

namespace heritage::engine {

struct EngineRenderingState final
{
    heritage::graphics::PostProcessor postProcessor;
    heritage::graphics::PostFramebuffer msaaFBO;
    heritage::graphics::PostFramebuffer resolveFBO;

    std::array<GLuint, 3> gpuTimerQueries{ 0, 0, 0 };
    std::array<bool, 3> gpuTimerIssued{ false, false, false };
    std::size_t gpuTimerCursor = 0;

    int previousFramebufferWidth = 0;
    int previousFramebufferHeight = 0;
    int previousAntiAliasingIndex = -1;
    int previousScaleModeIndex = -1;
};

struct EngineRenderFrame final
{
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    heritage::graphics::RenderSize renderSize;
    double now = 0.0;
};

bool initializeEngineRendering(EngineRenderingState& state);
void shutdownEngineRendering(EngineRenderingState& state);

EngineRenderFrame prepareEngineRendering(
    EngineRenderingState& state,
    heritage::graphics::DisplaySystem& display,
    const heritage::settings::VideoSettings& videoSettings,
    int framebufferWidth,
    int framebufferHeight);

bool renderEngineScene(
    EngineRenderingState& state,
    const EngineRenderFrame& frame,
    heritage::graphics::DisplaySystem& display,
    const heritage::settings::VideoSettings& videoSettings,
    heritage::modules::ModuleRuntimeManager& moduleRuntime,
    heritage::graphics::EntityMeshRenderer& entityMeshRenderer,
    heritage::graphics::EntityDebugRenderer& entityDebugRenderer,
    heritage::graphics::SurfacePresentationRenderer& surfacePresentationRenderer,
    const heritage::physics::SurfaceWorld& surfaces,
    heritage::entities::EntityRegistry& entityRegistry,
    const heritage::camera::CameraFrame& entityCameraFrame,
    heritage::diagnostics::PerformanceMonitor& performanceMonitor,
    bool wireframeVisible);

void endEngineGpuTimer(EngineRenderingState& state, bool timerActiveThisFrame);

} // namespace heritage::engine
