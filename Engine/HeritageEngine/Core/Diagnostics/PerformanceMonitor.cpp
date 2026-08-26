#include "PerformanceMonitor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace heritage::diagnostics {

void PerformanceMonitor::SmoothedValue::reset()
{
    latest = 0.0;
    average = 0.0;
    initialized = false;
}

void PerformanceMonitor::SmoothedValue::push(double value, double alpha)
{
    if (!std::isfinite(value) || value < 0.0)
        return;
    latest = value;
    if (!initialized)
    {
        average = value;
        initialized = true;
        return;
    }
    average += (value - average) * alpha;
}

void PerformanceMonitor::reset()
{
    for (SmoothedValue& value : m_sections)
        value.reset();
    for (SmoothedValue& value : m_renderSections)
        value.reset();
    for (SmoothedValue& value : m_gpuSections)
        value.reset();
    m_frameMs.reset();
    m_gpuMs.reset();
    m_frameTimeGraph.fill(0.0f);
    m_frameTimeGraphWrite = 0;
    m_frameTimeGraphCount = 0;
    m_statisticsFrames.fill(0.0);
    m_statisticsScratch.fill(0.0);
    m_statisticsWrite = 0;
    m_statisticsCount = 0;
    m_statisticsRefreshCountdown = 0;
    m_onePercentLowFps = 0.0;
    m_pointOnePercentLowFps = 0.0;
    m_p99FrameMs = 0.0;
    m_p999FrameMs = 0.0;
    m_worstRollingFrameMs = 0.0;
    m_peakFrameMs = 0.0;
    m_frameNumber = 0;
    m_hasCpuHitch = false;
    m_hitchFrameNumber = 0;
    m_hitchActiveMs = 0.0;
    m_hitchSections.fill(0.0);
    m_hitchRenderSections.fill(0.0);
    m_hitchUnattributedMs = 0.0;
    m_hitchResidualRenderCpuMs = 0.0;
}

void PerformanceMonitor::pushRawFrameTime(double milliseconds)
{
    if (!std::isfinite(milliseconds) || milliseconds < 0.0)
        return;

    m_frameTimeGraph[m_frameTimeGraphWrite] = static_cast<float>(milliseconds);
    m_frameTimeGraphWrite = (m_frameTimeGraphWrite + 1) % kFrameTimeGraphCapacity;
    m_frameTimeGraphCount = (std::min)(m_frameTimeGraphCount + 1, kFrameTimeGraphCapacity);

    m_statisticsFrames[m_statisticsWrite] = milliseconds;
    m_statisticsWrite = (m_statisticsWrite + 1) % kStatisticsCapacity;
    m_statisticsCount = (std::min)(m_statisticsCount + 1, kStatisticsCapacity);
}

void PerformanceMonitor::refreshFrameTimeStatistics()
{
    if (m_statisticsCount == 0)
        return;

    // Copy only the valid samples. Order does not matter because the copy is sorted.
    for (std::size_t index = 0; index < m_statisticsCount; ++index)
        m_statisticsScratch[index] = m_statisticsFrames[index];

    std::sort(
        m_statisticsScratch.begin(),
        m_statisticsScratch.begin() + static_cast<std::ptrdiff_t>(m_statisticsCount));

    const auto percentile = [&](double fraction) -> double {
        if (m_statisticsCount == 1)
            return m_statisticsScratch[0];
        const double position = fraction * static_cast<double>(m_statisticsCount - 1);
        const std::size_t lower = static_cast<std::size_t>(std::floor(position));
        const std::size_t upper = (std::min)(lower + 1, m_statisticsCount - 1);
        const double blend = position - static_cast<double>(lower);
        return m_statisticsScratch[lower]
            + (m_statisticsScratch[upper] - m_statisticsScratch[lower]) * blend;
    };

    m_p99FrameMs = percentile(0.99);
    m_p999FrameMs = percentile(0.999);
    m_onePercentLowFps = m_p99FrameMs > 0.000001 ? 1000.0 / m_p99FrameMs : 0.0;
    m_pointOnePercentLowFps = m_p999FrameMs > 0.000001 ? 1000.0 / m_p999FrameMs : 0.0;
    m_worstRollingFrameMs = m_statisticsScratch[m_statisticsCount - 1];
}

void PerformanceMonitor::beginFrame(double frameDeltaSeconds)
{
    ++m_frameNumber;
    const double milliseconds = (std::max)(0.0, frameDeltaSeconds * 1000.0);
    m_frameMs.push(milliseconds, kSmoothingAlpha);
    pushRawFrameTime(milliseconds);
    m_peakFrameMs = (std::max)(m_peakFrameMs, milliseconds);

    if (m_statisticsRefreshCountdown == 0)
    {
        refreshFrameTimeStatistics();
        m_statisticsRefreshCountdown = kStatisticsRefreshFrames;
    }
    else
    {
        --m_statisticsRefreshCountdown;
    }
}

void PerformanceMonitor::recordSection(
    PerformanceSection section,
    double milliseconds)
{
    const std::size_t index = static_cast<std::size_t>(section);
    if (index >= m_sections.size())
        return;
    m_sections[index].push(milliseconds, kSmoothingAlpha);
}

void PerformanceMonitor::recordRenderSection(
    RenderPerformanceSection section,
    double milliseconds)
{
    const std::size_t index = static_cast<std::size_t>(section);
    if (index >= m_renderSections.size())
        return;
    m_renderSections[index].push(milliseconds, kSmoothingAlpha);
}

void PerformanceMonitor::recordGpuSection(
    GpuPerformanceSection section,
    double milliseconds)
{
    const std::size_t index = static_cast<std::size_t>(section);
    if (index >= m_gpuSections.size())
        return;
    m_gpuSections[index].push(milliseconds, kSmoothingAlpha);
}

void PerformanceMonitor::recordGpuFrame(double milliseconds)
{
    m_gpuMs.push(milliseconds, kSmoothingAlpha);
}

void PerformanceMonitor::endFrame(double activeMilliseconds)
{
    recordSection(PerformanceSection::FrameActive, activeMilliseconds);

    // Capture exact unsmoothed section timings for meaningful CPU hitches.
    // This intentionally records the most recent hitch rather than only the
    // absolute worst one so a creator can reproduce a ditch/impact hitch and
    // immediately see which subsystem consumed the frame.
    if (std::isfinite(activeMilliseconds)
        && activeMilliseconds >= kCpuHitchThresholdMs)
    {
        m_hasCpuHitch = true;
        m_hitchFrameNumber = m_frameNumber;
        m_hitchActiveMs = activeMilliseconds;

        double attributed = 0.0;
        for (std::size_t index = 0; index < m_hitchSections.size(); ++index)
        {
            const PerformanceSection section = static_cast<PerformanceSection>(index);
            if (section == PerformanceSection::FrameActive)
            {
                m_hitchSections[index] = activeMilliseconds;
                continue;
            }

            const double latest = m_sections[index].latest;
            m_hitchSections[index] = latest;
            attributed += latest;
        }
        m_hitchUnattributedMs = (std::max)(0.0, activeMilliseconds - attributed);

        double namedRenderMs = 0.0;
        for (std::size_t index = 0; index < m_hitchRenderSections.size(); ++index)
        {
            const double latest = m_renderSections[index].latest;
            m_hitchRenderSections[index] = latest;
            namedRenderMs += latest;
        }
        m_hitchResidualRenderCpuMs = (std::max)(
            0.0,
            m_hitchSections[static_cast<std::size_t>(PerformanceSection::RenderCpu)]
                - namedRenderMs);
    }
}

PerformanceSnapshot PerformanceMonitor::snapshot() const
{
    PerformanceSnapshot result;
    const auto average = [&](PerformanceSection section) {
        return m_sections[static_cast<std::size_t>(section)].average;
    };

    result.frameMs = m_frameMs.average;
    result.fps = result.frameMs > 0.000001 ? 1000.0 / result.frameMs : 0.0;
    result.gpuFrameMs = m_gpuMs.average;
    result.eventsInputMs = average(PerformanceSection::EventsInput);
    result.audioMs = average(PerformanceSection::Audio);
    result.housekeepingMs = average(PerformanceSection::Housekeeping);
    result.physicsMs = average(PerformanceSection::Physics);
    result.gameUpdateMs = average(PerformanceSection::GameUpdate);
    result.renderCpuMs = average(PerformanceSection::RenderCpu);
    result.uiCpuMs = average(PerformanceSection::UiCpu);
    result.presentMs = average(PerformanceSection::Present);
    result.frameActiveMs = average(PerformanceSection::FrameActive);

    const auto renderAverage = [&](RenderPerformanceSection section) {
        return m_renderSections[static_cast<std::size_t>(section)].average;
    };
    result.renderModuleMs = renderAverage(RenderPerformanceSection::ModuleRender);
    result.renderMeshMs = renderAverage(RenderPerformanceSection::MeshRenderer);
    result.renderSurfaceMs = renderAverage(RenderPerformanceSection::SurfacePresentation);
    result.renderWeatherMs = renderAverage(RenderPerformanceSection::WeatherPresentation);
    result.renderDebugMs = renderAverage(RenderPerformanceSection::DebugRenderer);
    result.renderFramebufferSetupMs = renderAverage(RenderPerformanceSection::FramebufferSetup);
    result.renderMsaaResolveMs = renderAverage(RenderPerformanceSection::MsaaResolve);
    result.renderPostProcessMs = renderAverage(RenderPerformanceSection::PostProcess);
    result.renderSpanCompositeMs = renderAverage(RenderPerformanceSection::SpanComposite);
    const double namedRenderMs =
        result.renderModuleMs
        + result.renderMeshMs
        + result.renderSurfaceMs
        + result.renderWeatherMs
        + result.renderDebugMs
        + result.renderFramebufferSetupMs
        + result.renderMsaaResolveMs
        + result.renderPostProcessMs
        + result.renderSpanCompositeMs;
    result.residualRenderCpuMs = (std::max)(0.0, result.renderCpuMs - namedRenderMs);

    const auto gpuAverage = [&](GpuPerformanceSection section) {
        return m_gpuSections[static_cast<std::size_t>(section)].average;
    };
    result.gpuModuleMs = gpuAverage(GpuPerformanceSection::ModuleRender);
    result.gpuMeshMs = gpuAverage(GpuPerformanceSection::MeshRenderer);
    result.gpuSurfaceMs = gpuAverage(GpuPerformanceSection::SurfacePresentation);
    result.gpuWeatherMs = gpuAverage(GpuPerformanceSection::WeatherPresentation);
    result.gpuDebugMs = gpuAverage(GpuPerformanceSection::DebugRenderer);
    result.gpuMsaaResolveMs = gpuAverage(GpuPerformanceSection::MsaaResolve);
    result.gpuPostProcessMs = gpuAverage(GpuPerformanceSection::PostProcess);
    result.gpuNamedMs =
        result.gpuModuleMs
        + result.gpuMeshMs
        + result.gpuSurfaceMs
        + result.gpuWeatherMs
        + result.gpuDebugMs
        + result.gpuMsaaResolveMs
        + result.gpuPostProcessMs;
    result.gpuResidualMs = (std::max)(0.0, result.gpuFrameMs - result.gpuNamedMs);

    result.onePercentLowFps = m_onePercentLowFps;
    result.pointOnePercentLowFps = m_pointOnePercentLowFps;
    result.p99FrameMs = m_p99FrameMs;
    result.p999FrameMs = m_p999FrameMs;
    result.worstRollingFrameMs = m_worstRollingFrameMs;
    result.statisticsSampleCount = m_statisticsCount;

    result.frameTimeGraphCount = m_frameTimeGraphCount;
    if (m_frameTimeGraphCount > 0)
    {
        const std::size_t oldest =
            (m_frameTimeGraphWrite + kFrameTimeGraphCapacity - m_frameTimeGraphCount)
            % kFrameTimeGraphCapacity;
        for (std::size_t index = 0; index < m_frameTimeGraphCount; ++index)
        {
            result.frameTimeGraphMs[index] =
                m_frameTimeGraph[(oldest + index) % kFrameTimeGraphCapacity];
        }
    }

    result.peakFrameMs = m_peakFrameMs;
    result.frameNumber = m_frameNumber;

    result.hasCpuHitch = m_hasCpuHitch;
    result.hitchFrameNumber = m_hitchFrameNumber;
    result.hitchActiveMs = m_hitchActiveMs;
    const auto hitch = [&](PerformanceSection section) {
        return m_hitchSections[static_cast<std::size_t>(section)];
    };
    result.hitchEventsInputMs = hitch(PerformanceSection::EventsInput);
    result.hitchAudioMs = hitch(PerformanceSection::Audio);
    result.hitchHousekeepingMs = hitch(PerformanceSection::Housekeeping);
    result.hitchPhysicsMs = hitch(PerformanceSection::Physics);
    result.hitchGameUpdateMs = hitch(PerformanceSection::GameUpdate);
    result.hitchRenderCpuMs = hitch(PerformanceSection::RenderCpu);
    const auto hitchRender = [&](RenderPerformanceSection section) {
        return m_hitchRenderSections[static_cast<std::size_t>(section)];
    };
    result.hitchRenderModuleMs = hitchRender(RenderPerformanceSection::ModuleRender);
    result.hitchRenderMeshMs = hitchRender(RenderPerformanceSection::MeshRenderer);
    result.hitchRenderSurfaceMs = hitchRender(RenderPerformanceSection::SurfacePresentation);
    result.hitchRenderWeatherMs = hitchRender(RenderPerformanceSection::WeatherPresentation);
    result.hitchRenderDebugMs = hitchRender(RenderPerformanceSection::DebugRenderer);
    result.hitchRenderFramebufferSetupMs = hitchRender(RenderPerformanceSection::FramebufferSetup);
    result.hitchRenderMsaaResolveMs = hitchRender(RenderPerformanceSection::MsaaResolve);
    result.hitchRenderPostProcessMs = hitchRender(RenderPerformanceSection::PostProcess);
    result.hitchRenderSpanCompositeMs = hitchRender(RenderPerformanceSection::SpanComposite);
    result.hitchResidualRenderCpuMs = m_hitchResidualRenderCpuMs;
    result.hitchUiCpuMs = hitch(PerformanceSection::UiCpu);
    result.hitchPresentMs = hitch(PerformanceSection::Present);
    result.hitchUnattributedMs = m_hitchUnattributedMs;
    return result;
}

} // namespace heritage::diagnostics
