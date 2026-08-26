#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace heritage::diagnostics {

enum class PerformanceSection : std::size_t
{
    EventsInput = 0,
    Audio,
    Housekeeping,
    Physics,
    GameUpdate,
    RenderCpu,
    UiCpu,
    Present,
    FrameActive,
    Count
};

// PERF08: RenderCpu is the top-level render bucket. These sub-sections are
// diagnostic children and are deliberately NOT added to frame attribution a
// second time. They let the overlay identify where an OpenGL/driver stall is
// being charged when VSync/back-pressure blocks before SwapBuffers.
enum class RenderPerformanceSection : std::size_t
{
    ModuleRender = 0,
    MeshRenderer,
    SurfacePresentation,
    WeatherPresentation,
    DebugRenderer,
    FramebufferSetup,
    MsaaResolve,
    PostProcess,
    SpanComposite,
    Count
};

// OPT00: asynchronous timestamp children of the frame-wide GPU timer. These
// represent durable render passes and are diagnostic children only; they are
// not added to gpuFrameMs a second time. Multi-monitor spanning keeps the
// frame-wide timer authoritative and temporarily omits these per-pass values.
enum class GpuPerformanceSection : std::size_t
{
    ModuleRender = 0,
    MeshRenderer,
    SurfacePresentation,
    WeatherPresentation,
    DebugRenderer,
    MsaaResolve,
    PostProcess,
    Count
};

struct PerformanceSnapshot
{
    static constexpr std::size_t kFrameTimeGraphCapacity = 360;

    double fps = 0.0;
    double frameMs = 0.0;
    double gpuFrameMs = 0.0;
    double eventsInputMs = 0.0;
    double audioMs = 0.0;
    double housekeepingMs = 0.0;
    double physicsMs = 0.0;
    double gameUpdateMs = 0.0;
    double renderCpuMs = 0.0;
    double uiCpuMs = 0.0;
    double presentMs = 0.0;
    double frameActiveMs = 0.0;

    // PERF08 rolling render-submission children. These sum to most (but not
    // necessarily all) of renderCpuMs; residualRenderCpuMs catches framebuffer
    // setup/state work not wrapped by a named child timer.
    double renderModuleMs = 0.0;
    double renderMeshMs = 0.0;
    double renderSurfaceMs = 0.0;
    double renderWeatherMs = 0.0;
    double renderDebugMs = 0.0;
    double renderFramebufferSetupMs = 0.0;
    double renderMsaaResolveMs = 0.0;
    double renderPostProcessMs = 0.0;
    double renderSpanCompositeMs = 0.0;
    double residualRenderCpuMs = 0.0;

    // OPT00 rolling asynchronous GPU pass timings. gpuFrameMs remains the
    // authoritative total. gpuResidualMs is the portion not represented by
    // the named top-level passes (driver work, clears, copies, etc.).
    double gpuModuleMs = 0.0;
    double gpuMeshMs = 0.0;
    double gpuSurfaceMs = 0.0;
    double gpuWeatherMs = 0.0;
    double gpuDebugMs = 0.0;
    double gpuMsaaResolveMs = 0.0;
    double gpuPostProcessMs = 0.0;
    double gpuNamedMs = 0.0;
    double gpuResidualMs = 0.0;

    // PERF04 rolling frame-pacing statistics. "1% low" / "0.1% low" are
    // reported from the 99th / 99.9th frame-time percentiles respectively.
    // This makes a hitch visible even when the smoothed FPS number looks fine.
    double onePercentLowFps = 0.0;
    double pointOnePercentLowFps = 0.0;
    double p99FrameMs = 0.0;
    double p999FrameMs = 0.0;
    double worstRollingFrameMs = 0.0;
    std::size_t statisticsSampleCount = 0;

    std::array<float, kFrameTimeGraphCapacity> frameTimeGraphMs{};
    std::size_t frameTimeGraphCount = 0;

    double peakFrameMs = 0.0;
    std::uint64_t frameNumber = 0;

    // PERF07 forensic snapshot of the most recent CPU-active hitch. This uses
    // unsmoothed section timings from the exact hitch frame so rolling averages
    // cannot hide a one-frame spike.
    bool hasCpuHitch = false;
    std::uint64_t hitchFrameNumber = 0;
    double hitchActiveMs = 0.0;
    double hitchEventsInputMs = 0.0;
    double hitchAudioMs = 0.0;
    double hitchHousekeepingMs = 0.0;
    double hitchPhysicsMs = 0.0;
    double hitchGameUpdateMs = 0.0;
    double hitchRenderCpuMs = 0.0;
    double hitchRenderModuleMs = 0.0;
    double hitchRenderMeshMs = 0.0;
    double hitchRenderSurfaceMs = 0.0;
    double hitchRenderWeatherMs = 0.0;
    double hitchRenderDebugMs = 0.0;
    double hitchRenderFramebufferSetupMs = 0.0;
    double hitchRenderMsaaResolveMs = 0.0;
    double hitchRenderPostProcessMs = 0.0;
    double hitchRenderSpanCompositeMs = 0.0;
    double hitchResidualRenderCpuMs = 0.0;
    double hitchUiCpuMs = 0.0;
    double hitchPresentMs = 0.0;
    double hitchUnattributedMs = 0.0;
};

// PERF01 low-overhead rolling CPU/GPU performance state. CPU sections are
// fed by main.cpp. GPU time comes from asynchronous OpenGL timer queries and
// is recorded only when a completed result is available.
class PerformanceMonitor
{
public:
    void reset();
    void beginFrame(double frameDeltaSeconds);
    void recordSection(PerformanceSection section, double milliseconds);
    void recordRenderSection(RenderPerformanceSection section, double milliseconds);
    void recordGpuSection(GpuPerformanceSection section, double milliseconds);
    void recordGpuFrame(double milliseconds);
    void endFrame(double activeMilliseconds);

    PerformanceSnapshot snapshot() const;

private:
    struct SmoothedValue
    {
        double latest = 0.0;
        double average = 0.0;
        bool initialized = false;

        void reset();
        void push(double value, double alpha);
    };

    static constexpr double kSmoothingAlpha = 0.10;
    static constexpr std::size_t kFrameTimeGraphCapacity = PerformanceSnapshot::kFrameTimeGraphCapacity;
    static constexpr std::size_t kStatisticsCapacity = 4096;
    static constexpr std::uint32_t kStatisticsRefreshFrames = 30;
    static constexpr double kCpuHitchThresholdMs = 20.0;

    void pushRawFrameTime(double milliseconds);
    void refreshFrameTimeStatistics();

    std::array<SmoothedValue, static_cast<std::size_t>(PerformanceSection::Count)> m_sections{};
    std::array<SmoothedValue, static_cast<std::size_t>(RenderPerformanceSection::Count)> m_renderSections{};
    std::array<SmoothedValue, static_cast<std::size_t>(GpuPerformanceSection::Count)> m_gpuSections{};
    SmoothedValue m_frameMs{};
    SmoothedValue m_gpuMs{};

    std::array<float, kFrameTimeGraphCapacity> m_frameTimeGraph{};
    std::size_t m_frameTimeGraphWrite = 0;
    std::size_t m_frameTimeGraphCount = 0;

    std::array<double, kStatisticsCapacity> m_statisticsFrames{};
    std::array<double, kStatisticsCapacity> m_statisticsScratch{};
    std::size_t m_statisticsWrite = 0;
    std::size_t m_statisticsCount = 0;
    std::uint32_t m_statisticsRefreshCountdown = 0;
    double m_onePercentLowFps = 0.0;
    double m_pointOnePercentLowFps = 0.0;
    double m_p99FrameMs = 0.0;
    double m_p999FrameMs = 0.0;
    double m_worstRollingFrameMs = 0.0;

    double m_peakFrameMs = 0.0;
    std::uint64_t m_frameNumber = 0;

    bool m_hasCpuHitch = false;
    std::uint64_t m_hitchFrameNumber = 0;
    double m_hitchActiveMs = 0.0;
    std::array<double, static_cast<std::size_t>(PerformanceSection::Count)> m_hitchSections{};
    std::array<double, static_cast<std::size_t>(RenderPerformanceSection::Count)> m_hitchRenderSections{};
    double m_hitchUnattributedMs = 0.0;
    double m_hitchResidualRenderCpuMs = 0.0;
};

} // namespace heritage::diagnostics
