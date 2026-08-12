#include "EngineRendering.hpp"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <climits>
#include <cmath>

#include "../../Camera/ChaseCamera.hpp"
#include "../../Core/Diagnostics/PerformanceMonitor.hpp"
#include "../../Core/Entities/EntityRegistry.hpp"
#include "../../Core/Math/Math.hpp"
#include "../../Core/Modules/ModuleRuntimeManager.hpp"
#include "../../Core/Settings/VideoSettings.hpp"
#include "../../Graphics/AntiAliasing.hpp"
#include "../../Graphics/DisplaySystem.hpp"
#include "../../Graphics/Renderer/EntityDebugRenderer.hpp"
#include "../../Graphics/Renderer/EntityMeshRenderer.hpp"
#include "../../Graphics/Renderer/SurfacePresentationRenderer.hpp"
#include "../../Physics/Surfaces/SurfaceWorld.hpp"

namespace heritage::engine {

bool initializeEngineRendering(EngineRenderingState& state)
{
    if (!state.postProcessor.initialize())
        return false;

    glGenQueries(
        static_cast<GLsizei>(state.gpuTimerQueries.size()),
        state.gpuTimerQueries.data());
    return true;
}

void shutdownEngineRendering(EngineRenderingState& state)
{
    state.msaaFBO.destroy();
    state.resolveFBO.destroy();
    state.postProcessor.shutdown();
    if (state.gpuTimerQueries[0] != 0)
    {
        glDeleteQueries(
            static_cast<GLsizei>(state.gpuTimerQueries.size()),
            state.gpuTimerQueries.data());
    }
    state.gpuTimerQueries = { 0, 0, 0 };
    state.gpuTimerIssued = { false, false, false };
    state.gpuTimerCursor = 0;
}

EngineRenderFrame prepareEngineRendering(
    EngineRenderingState& state,
    heritage::graphics::DisplaySystem& display,
    const heritage::settings::VideoSettings& videoSettings,
    int framebufferWidth,
    int framebufferHeight)
{
    using heritage::graphics::AntiAliasingSettings;
    using heritage::graphics::RenderScaler;
    using heritage::graphics::resolveAntiAliasing;

    EngineRenderFrame frame;
    frame.framebufferWidth = framebufferWidth;
    frame.framebufferHeight = framebufferHeight;
    frame.renderSize = RenderScaler::calculateRenderSize(
        framebufferWidth,
        framebufferHeight,
        videoSettings.scaleModeIndex);
    if (framebufferWidth != state.previousFramebufferWidth
        || framebufferHeight != state.previousFramebufferHeight
        || videoSettings.antiAliasingIndex != state.previousAntiAliasingIndex
        || videoSettings.scaleModeIndex != state.previousScaleModeIndex)
    {
        const AntiAliasingSettings antiAliasing =
            resolveAntiAliasing(videoSettings.antiAliasingIndex);
        if (antiAliasing.msaaSamples > 1)
        {
            state.msaaFBO.init(
                frame.renderSize.width,
                frame.renderSize.height,
                antiAliasing.msaaSamples);
        }
        // The normal scene always renders to an off-screen texture.
        // This gives every non-spanning frame one consistent presentation
        // path and prepares the pipeline for global post-processing passes.
        state.resolveFBO.init(
            frame.renderSize.width,
            frame.renderSize.height,
            1);
        state.previousFramebufferWidth = framebufferWidth;
        state.previousFramebufferHeight = framebufferHeight;
        state.previousAntiAliasingIndex = videoSettings.antiAliasingIndex;
        state.previousScaleModeIndex = videoSettings.scaleModeIndex;
    }

    display.updateSpanFBO();
    return frame;
}

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
    bool wireframeVisible)
{
    using heritage::diagnostics::PerformanceSection;
    using heritage::diagnostics::RenderPerformanceSection;
    using heritage::graphics::AntiAliasingSettings;
    using heritage::graphics::RenderScaler;
    using heritage::graphics::kDefaultFarClipMeters;
    using heritage::graphics::kDefaultNearClipMeters;
    using heritage::graphics::resolveAntiAliasing;
    using heritage::math::Mat4;
    using heritage::math::Vec3;
    using heritage::math::perspectiveReversedZ;

    const int fbW = frame.framebufferWidth;
    const int fbH = frame.framebufferHeight;
    const int rW = frame.renderSize.width;
    const int rH = frame.renderSize.height;
    const double now = frame.now;

    const AntiAliasingSettings antiAliasing =
        resolveAntiAliasing(videoSettings.antiAliasingIndex);
    const bool needMSAA = antiAliasing.msaaSamples > 1;
    const bool needFXAA = antiAliasing.useFxaa;
    if (needMSAA) glEnable(GL_MULTISAMPLE);
    else glDisable(GL_MULTISAMPLE);
    const bool needScale = RenderScaler::requiresScaling(fbW, fbH, frame.renderSize);
    const bool nearestUp = RenderScaler::usesNearestNeighbour(videoSettings.scaleModeIndex);

    entityMeshRenderer.beginFrameStats();
    entityDebugRenderer.beginFrameStats();
    surfacePresentationRenderer.beginFrameStats();

    bool gpuTimerActiveThisFrame = false;
    if (!state.gpuTimerQueries.empty())
    {
        const std::size_t queryIndex = state.gpuTimerCursor;
        if (state.gpuTimerIssued[queryIndex])
        {
            GLint available = GL_FALSE;
            glGetQueryObjectiv(
                state.gpuTimerQueries[queryIndex],
                GL_QUERY_RESULT_AVAILABLE,
                &available);
            if (available == GL_TRUE)
            {
                GLuint64 nanoseconds = 0;
                glGetQueryObjectui64v(
                    state.gpuTimerQueries[queryIndex],
                    GL_QUERY_RESULT,
                    &nanoseconds);
                performanceMonitor.recordGpuFrame(
                    static_cast<double>(nanoseconds) / 1000000.0);
                state.gpuTimerIssued[queryIndex] = false;
            }
        }
        if (state.gpuTimerQueries[queryIndex] != 0
            && !state.gpuTimerIssued[queryIndex])
        {
            glBeginQuery(GL_TIME_ELAPSED, state.gpuTimerQueries[queryIndex]);
            gpuTimerActiveThisFrame = true;
        }
    }

    const double renderCpuStart = glfwGetTime();
    double renderModuleMs = 0.0;
    double renderMeshMs = 0.0;
    double renderDebugMs = 0.0;
    double renderMsaaResolveMs = 0.0;
    double renderPostProcessMs = 0.0;
    double renderSpanCompositeMs = 0.0;

    // ========== RENDER ==========
    const Vec3 sceneClearColor = moduleRuntime.clearColor();
    const bool spanning = display.isSpanning() && display.spanFBO() != 0;

    if (spanning)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, display.spanFBO());
        glViewport(0, 0, display.spanWidth(), display.spanHeight());
        glClearColor(sceneClearColor.x, sceneClearColor.y, sceneClearColor.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);

        int minX = INT_MAX, minY = INT_MAX, maxY = INT_MIN;
        for (std::size_t i = 0; i < display.monitors().size(); ++i)
        {
            if (!display.selected[i]) continue;
            const auto& mi = display.monitors()[i];
            minX = std::min(minX, mi.xpos);
            minY = std::min(minY, mi.ypos);
            maxY = std::max(maxY, mi.ypos + mi.height);
        }
        const int desktopH = maxY - minY;

        for (std::size_t i = 0; i < display.monitors().size(); ++i)
        {
            if (!display.selected[i]) continue;
            const auto& mi = display.monitors()[i];

            const int relX = mi.xpos - minX;
            const int relY = mi.ypos - minY;
            const int monW = mi.width;
            const int monH = mi.height;

            const int fboX = static_cast<int>(floorf(relX * display.spanScale()));
            const int fboY = static_cast<int>(floorf((desktopH - (relY + monH)) * display.spanScale()));
            const int fboW = static_cast<int>(floorf(monW * display.spanScale()));
            const int fboH = static_cast<int>(floorf(monH * display.spanScale()));

            glViewport(fboX, fboY, fboW, fboH);
            glScissor(fboX, fboY, fboW, fboH);
            glClear(GL_DEPTH_BUFFER_BIT);

            const Mat4 projOff = display.getOffAxisProjection(i);

            {
                const double sectionStart = glfwGetTime();
                if (wireframeVisible)
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                moduleRuntime.render(projOff, videoSettings);
                if (wireframeVisible)
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                renderModuleMs += (glfwGetTime() - sectionStart) * 1000.0;
            }
            {
                const double sectionStart = glfwGetTime();
                entityMeshRenderer.draw(
                    entityRegistry,
                    projOff,
                    videoSettings,
                    static_cast<float>(now),
                    entityCameraFrame,
                    wireframeVisible);
                renderMeshMs += (glfwGetTime() - sectionStart) * 1000.0;
            }
            surfacePresentationRenderer.draw(
                surfaces,
                projOff,
                videoSettings,
                entityCameraFrame);
            {
                const double sectionStart = glfwGetTime();
                entityDebugRenderer.draw(
                    entityRegistry,
                    projOff,
                    videoSettings,
                    static_cast<float>(now),
                    entityCameraFrame);
                renderDebugMs += (glfwGetTime() - sectionStart) * 1000.0;
            }
        }

        glDisable(GL_SCISSOR_TEST);

        {
            const double sectionStart = glfwGetTime();
            glBindFramebuffer(GL_READ_FRAMEBUFFER, display.spanFBO());
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(
                0, 0, display.spanWidth(), display.spanHeight(),
                0, 0, fbW, fbH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            renderSpanCompositeMs += (glfwGetTime() - sectionStart) * 1000.0;
        }
    }
    else
    {
        if (needMSAA && state.msaaFBO.fbo)
            glBindFramebuffer(GL_FRAMEBUFFER, state.msaaFBO.fbo);
        else
            glBindFramebuffer(GL_FRAMEBUFFER, state.resolveFBO.fbo);

        glViewport(0, 0, rW, rH);
        glClearColor(sceneClearColor.x, sceneClearColor.y, sceneClearColor.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        const Mat4 proj = perspectiveReversedZ(
            0.6f,
            static_cast<float>(rW) / static_cast<float>(rH),
            kDefaultNearClipMeters,
            kDefaultFarClipMeters);

        {
            const double sectionStart = glfwGetTime();
            if (wireframeVisible)
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            moduleRuntime.render(proj, videoSettings);
            if (wireframeVisible)
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            renderModuleMs += (glfwGetTime() - sectionStart) * 1000.0;
        }
        {
            const double sectionStart = glfwGetTime();
            entityMeshRenderer.draw(
                entityRegistry,
                proj,
                videoSettings,
                static_cast<float>(now),
                entityCameraFrame,
                wireframeVisible);
            renderMeshMs += (glfwGetTime() - sectionStart) * 1000.0;
        }
        surfacePresentationRenderer.draw(
            surfaces,
            proj,
            videoSettings,
            entityCameraFrame);
        {
            const double sectionStart = glfwGetTime();
            entityDebugRenderer.draw(
                entityRegistry,
                proj,
                videoSettings,
                static_cast<float>(now),
                entityCameraFrame);
            renderDebugMs += (glfwGetTime() - sectionStart) * 1000.0;
        }

        if (needMSAA && state.msaaFBO.fbo && state.resolveFBO.fbo)
        {
            const double sectionStart = glfwGetTime();
            glBindFramebuffer(GL_READ_FRAMEBUFFER, state.msaaFBO.fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.resolveFBO.fbo);
            glBlitFramebuffer(
                0, 0, rW, rH,
                0, 0, rW, rH,
                GL_COLOR_BUFFER_BIT,
                GL_NEAREST);
            renderMsaaResolveMs += (glfwGetTime() - sectionStart) * 1000.0;
        }

        if (state.resolveFBO.fbo)
        {
            const double sectionStart = glfwGetTime();
            if (needFXAA)
            {
                // FXAA samples the internal-resolution scene texture and
                // writes directly to the final framebuffer. Scaling, when
                // enabled, happens naturally in this fullscreen pass.
                state.postProcessor.applyFxaa(
                    state.resolveFBO.tex,
                    rW,
                    rH,
                    0,
                    fbW,
                    fbH);
            }
            else
            {
                // Even native-resolution frames now use the same final
                // presentation path. This is intentionally a simple blit
                // today; later color grading and vignette can be inserted
                // here without changing how the scene renderer works.
                state.postProcessor.blit(
                    state.resolveFBO.tex,
                    0,
                    fbW,
                    fbH,
                    !needScale || nearestUp);
            }
            renderPostProcessMs += (glfwGetTime() - sectionStart) * 1000.0;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    performanceMonitor.recordRenderSection(
        RenderPerformanceSection::ModuleRender,
        renderModuleMs);
    performanceMonitor.recordRenderSection(
        RenderPerformanceSection::MeshRenderer,
        renderMeshMs);
    performanceMonitor.recordRenderSection(
        RenderPerformanceSection::DebugRenderer,
        renderDebugMs);
    performanceMonitor.recordRenderSection(
        RenderPerformanceSection::MsaaResolve,
        renderMsaaResolveMs);
    performanceMonitor.recordRenderSection(
        RenderPerformanceSection::PostProcess,
        renderPostProcessMs);
    performanceMonitor.recordRenderSection(
        RenderPerformanceSection::SpanComposite,
        renderSpanCompositeMs);
    performanceMonitor.recordSection(
        PerformanceSection::RenderCpu,
        (glfwGetTime() - renderCpuStart) * 1000.0);

    return gpuTimerActiveThisFrame;
}

void endEngineGpuTimer(EngineRenderingState& state, bool timerActiveThisFrame)
{
    if (!timerActiveThisFrame)
        return;

    glEndQuery(GL_TIME_ELAPSED);
    state.gpuTimerIssued[state.gpuTimerCursor] = true;
    state.gpuTimerCursor =
        (state.gpuTimerCursor + 1) % state.gpuTimerQueries.size();
}

} // namespace heritage::engine
