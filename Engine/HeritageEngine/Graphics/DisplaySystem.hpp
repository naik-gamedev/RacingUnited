#pragma once

#include <vector>
#include <string>
#include <GLFW/glfw3.h>
#include "../Core/Math/Math.hpp"

namespace heritage::graphics {

using heritage::math::Mat4;

struct MonitorMode {
    int w = 0, h = 0, refresh = 0;
    std::string label;
};

struct MonitorInfo {
    std::string name;
    int xpos = 0, ypos = 0;
    int width = 0, height = 0;
    int physWidthMM = 0, physHeightMM = 0;
    std::vector<MonitorMode> modes;
};

class DisplaySystem {
public:
    void initialize();                          // call once after glfwInit + window created
    void shutdown();

    // Settings
    bool  spanAllMonitors = false;
    float eyeDistanceCm   = 60.0f;
    int   globalBezelMm   = 5;

    // Per-monitor state (same size as monitors())
    std::vector<bool> selected;
    std::vector<int>  bezelMm;

    // Read-only access
    const std::vector<MonitorInfo>& monitors() const { return m_monitors; }
    bool isSpanning() const { return spanAllMonitors && !m_monitors.empty(); }

    // Call every frame if spanning (or when settings change)
    void updateSpanFBO();                       // creates/resizes the big FBO if needed

    // Rendering helpers
    GLuint spanFBO() const { return m_spanFBO; }
    int    spanWidth() const { return m_spanW; }
    int    spanHeight() const { return m_spanH; }
    float  spanScale() const { return m_spanScale; }

    // Off-axis projection for a specific monitor index (only valid when spanning)
    Mat4 getOffAxisProjection(size_t monitorIndex, float nearZ = 0.1f, float farZ = 100.0f) const;

    // Simple combined HFOV (for UI preview)
    float getCombinedHFOVDegrees() const;

    // Persistence
    void save(const std::string& path) const;
    void load(const std::string& path);

private:
    std::vector<MonitorInfo> m_monitors;

    GLuint m_spanFBO = 0;
    GLuint m_spanColor = 0;
    GLuint m_spanDepth = 0;
    int    m_spanW = 0;
    int    m_spanH = 0;
    float  m_spanScale = 1.0f;

    void destroySpanFBO();
    Mat4 buildFrustum(float l, float r, float b, float t, float n, float f) const;
};

} // namespace heritage::graphics