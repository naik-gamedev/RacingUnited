#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>

#include "../Core/Math/Math.hpp"

namespace heritage::graphics {

using heritage::math::Mat4;

struct MonitorMode
{
    int w = 0;
    int h = 0;
    int refresh = 0;
    std::string label;
};

struct MonitorInfo
{
    GLFWmonitor* handle = nullptr;
    std::string name;
    bool primary = false;

    int xpos = 0;
    int ypos = 0;
    int width = 0;
    int height = 0;

    // Usable desktop area after taskbars / docks are excluded.
    int workX = 0;
    int workY = 0;
    int workWidth = 0;
    int workHeight = 0;
    int refreshRate = 0;
    int physWidthMM = 0;
    int physHeightMM = 0;

    std::vector<MonitorMode> modes;
};

class DisplaySystem
{
public:
    void initialize();
    void shutdown();

    // Settings
    bool spanAllMonitors = false;
    float eyeDistanceCm = 60.0f;
    int globalBezelMm = 5;

    // Per-monitor state (same size as monitors())
    std::vector<bool> selected;
    std::vector<int> bezelMm;

    // Read-only access
    const std::vector<MonitorInfo>& monitors() const { return m_monitors; }
    std::size_t primaryMonitorIndex() const { return m_primaryMonitorIndex; }
    bool isSpanning() const { return spanAllMonitors && !m_monitors.empty(); }

    // Call every frame if spanning (or when settings change)
    void updateSpanFBO();

    // Rendering helpers
    GLuint spanFBO() const { return m_spanFBO; }
    int spanWidth() const { return m_spanW; }
    int spanHeight() const { return m_spanH; }
    float spanScale() const { return m_spanScale; }

    // Off-axis projection for a specific monitor index (only valid when spanning)
    Mat4 getOffAxisProjection(std::size_t monitorIndex, float nearZ = 0.1f, float farZ = 100.0f) const;

    // Simple combined HFOV (for UI preview)
    float getCombinedHFOVDegrees() const;

    // Persistence
    void save(const std::string& path) const;
    void load(const std::string& path);

private:
    std::vector<MonitorInfo> m_monitors;
    std::size_t m_primaryMonitorIndex = 0;

    GLuint m_spanFBO = 0;
    GLuint m_spanColor = 0;
    GLuint m_spanDepth = 0;
    int m_spanW = 0;
    int m_spanH = 0;
    float m_spanScale = 1.0f;

    void destroySpanFBO();
    Mat4 buildFrustum(float l, float r, float b, float t, float n, float f) const;
};

} // namespace heritage::graphics
