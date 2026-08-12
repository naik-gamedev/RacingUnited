#include <glad/glad.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "DisplaySystem.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace heritage::graphics {

void DisplaySystem::initialize()
{
    m_monitors.clear();
    selected.clear();
    bezelMm.clear();
    m_primaryMonitorIndex = 0;

    int count = 0;
    GLFWmonitor** mons = glfwGetMonitors(&count);
    if (!mons || count <= 0) return;

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();

    for (int i = 0; i < count; ++i)
    {
        GLFWmonitor* monitor = mons[i];
        MonitorInfo info;
        info.handle = monitor;
        info.primary = (monitor == primaryMonitor);

        if (info.primary)
            m_primaryMonitorIndex = static_cast<std::size_t>(i);

        const char* name = glfwGetMonitorName(monitor);
        info.name = name ? name : ("Monitor " + std::to_string(i));

        glfwGetMonitorPos(monitor, &info.xpos, &info.ypos);
        glfwGetMonitorWorkarea(
            monitor,
            &info.workX,
            &info.workY,
            &info.workWidth,
            &info.workHeight);

        const GLFWvidmode* currentMode = glfwGetVideoMode(monitor);
        if (currentMode)
        {
            info.width = currentMode->width;
            info.height = currentMode->height;
            info.refreshRate = currentMode->refreshRate;
        }

        glfwGetMonitorPhysicalSize(monitor, &info.physWidthMM, &info.physHeightMM);

        int modeCount = 0;
        const GLFWvidmode* modes = glfwGetVideoModes(monitor, &modeCount);
        if (modes && modeCount > 0)
        {
            std::vector<std::tuple<int, int, int>> seen;
            for (int modeIndex = 0; modeIndex < modeCount; ++modeIndex)
            {
                const int width = modes[modeIndex].width;
                const int height = modes[modeIndex].height;
                const int refresh = modes[modeIndex].refreshRate;
                const auto key = std::make_tuple(width, height, refresh);

                if (std::find(seen.begin(), seen.end(), key) != seen.end())
                    continue;

                seen.push_back(key);

                MonitorMode mode;
                mode.w = width;
                mode.h = height;
                mode.refresh = refresh;
                mode.label = std::to_string(width) + "x" + std::to_string(height)
                    + " @ " + std::to_string(refresh) + "Hz";
                info.modes.push_back(std::move(mode));
            }

            std::sort(
                info.modes.begin(),
                info.modes.end(),
                [](const MonitorMode& a, const MonitorMode& b)
                {
                    if (a.w != b.w) return a.w < b.w;
                    if (a.h != b.h) return a.h < b.h;
                    return a.refresh > b.refresh;
                });
        }

        m_monitors.push_back(std::move(info));
    }

    selected.resize(m_monitors.size(), true);
    bezelMm.resize(m_monitors.size(), globalBezelMm);
}

void DisplaySystem::shutdown()
{
    destroySpanFBO();
    m_monitors.clear();
    selected.clear();
    bezelMm.clear();
    m_primaryMonitorIndex = 0;
}

void DisplaySystem::destroySpanFBO()
{
    if (m_spanFBO) {
        glDeleteFramebuffers(1, &m_spanFBO);
        m_spanFBO = 0;
    }
    if (m_spanColor) {
        glDeleteTextures(1, &m_spanColor);
        m_spanColor = 0;
    }
    if (m_spanDepth) {
        glDeleteRenderbuffers(1, &m_spanDepth);
        m_spanDepth = 0;
    }
    m_spanW = m_spanH = 0;
    m_spanScale = 1.0f;
}

void DisplaySystem::updateSpanFBO()
{
    if (!spanAllMonitors || m_monitors.empty()) {
        destroySpanFBO();
        return;
    }

    // Compute selected desktop bounds
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    bool any = false;
    for (size_t i = 0; i < m_monitors.size(); ++i) {
        if (!selected[i]) continue;
        any = true;
        const auto& mi = m_monitors[i];
        minX = std::min(minX, mi.xpos);
        minY = std::min(minY, mi.ypos);
        maxX = std::max(maxX, mi.xpos + mi.width);
        maxY = std::max(maxY, mi.ypos + mi.height);
    }
    if (!any) {
        destroySpanFBO();
        return;
    }

    int desktopW = maxX - minX;
    int desktopH = maxY - minY;

    GLint maxTex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    float scale = 1.0f;
    if (desktopW > maxTex || desktopH > maxTex) {
        scale = std::min((float)maxTex / desktopW, (float)maxTex / desktopH);
    }

    int fboW = std::max(1, (int)std::floor(desktopW * scale));
    int fboH = std::max(1, (int)std::floor(desktopH * scale));

    if (m_spanFBO && m_spanW == fboW && m_spanH == fboH) return; // already good

    destroySpanFBO();

    m_spanW = fboW;
    m_spanH = fboH;
    m_spanScale = scale;

    glGenFramebuffers(1, &m_spanFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_spanFBO);

    glGenTextures(1, &m_spanColor);
    glBindTexture(GL_TEXTURE_2D, m_spanColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fboW, fboH, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_spanColor, 0);

    glGenRenderbuffers(1, &m_spanDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_spanDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, fboW, fboH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_spanDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "DisplaySystem: span FBO incomplete!\n";
        destroySpanFBO();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Mat4 DisplaySystem::buildFrustumReversedZ(float l, float r, float b, float t, float n, float f) const
{
    Mat4 m{};
    const float A = (2.0f * n) / (r - l);
    const float B = (2.0f * n) / (t - b);
    const float C = (r + l) / (r - l);
    const float D = (t + b) / (t - b);
    m.m[0] = A;  m.m[5] = B;
    m.m[8] = C;  m.m[9] = D;
    m.m[10] = (f + n) / (f - n);
    m.m[14] = (2.0f * f * n) / (f - n);
    m.m[11] = -1.0f;
    return m;
}

Mat4 DisplaySystem::getOffAxisProjection(size_t monitorIndex, float nearZ, float farZ) const
{
    if (monitorIndex >= m_monitors.size() || !selected[monitorIndex])
        return heritage::math::perspectiveReversedZ(0.6f, 16.0f/9.0f, nearZ, farZ);

    // Compute selected desktop bounds & physical totals
    int minX = INT_MAX, maxX = INT_MIN;
    float totalSelectedMM = 0.0f;
    int selDesktopW = 0;
    for (size_t i = 0; i < m_monitors.size(); ++i) {
        if (!selected[i]) continue;
        minX = std::min(minX, m_monitors[i].xpos);
        maxX = std::max(maxX, m_monitors[i].xpos + m_monitors[i].width);
        totalSelectedMM += (float)m_monitors[i].physWidthMM;
        selDesktopW += m_monitors[i].width;
    }

    const auto& mi = m_monitors[monitorIndex];
    float physWm = (float)mi.physWidthMM / 1000.0f;
    float physHm = (float)mi.physHeightMM / 1000.0f;
    float bezelM = (float)bezelMm[monitorIndex] / 1000.0f;

    // Approximate center offset along the selected horizontal span
    float accum = 0.0f;
    for (size_t i = 0; i < monitorIndex; ++i) {
        if (selected[i]) accum += m_monitors[i].width;
    }
    float monitorCenterX = (accum + mi.width * 0.5f) / (float)selDesktopW * (totalSelectedMM / 1000.0f);
    float spanCenterX    = 0.5f * (totalSelectedMM / 1000.0f);
    float centerOffsetX  = monitorCenterX - spanCenterX;
    float centerOffsetY  = 0.0f;

    float eyeM = std::max(0.01f, eyeDistanceCm / 100.0f);
    float halfW = std::max(0.001f, physWm - bezelM) * 0.5f;
    float halfH = physHm * 0.5f;

    float l = (-halfW - centerOffsetX) * (nearZ / eyeM);
    float r = ( halfW - centerOffsetX) * (nearZ / eyeM);
    float b = (-halfH - centerOffsetY) * (nearZ / eyeM);
    float t = ( halfH - centerOffsetY) * (nearZ / eyeM);

    return buildFrustumReversedZ(l, r, b, t, nearZ, farZ);
}

float DisplaySystem::getCombinedHFOVDegrees() const
{
    float totalMM = 0.0f;
    for (size_t i = 0; i < m_monitors.size(); ++i)
        if (selected[i]) totalMM += (float)m_monitors[i].physWidthMM;

    float totalM = totalMM / 1000.0f;
    float eyeM = std::max(0.01f, eyeDistanceCm / 100.0f);
    float hfov = 2.0f * std::atan((totalM * 0.5f) / eyeM);
    return hfov * 180.0f / 3.14159265f;
}

void DisplaySystem::save(const std::string& path) const
{
    std::ofstream f(path);
    if (!f) return;
    f << "spanAll=" << (spanAllMonitors ? "1" : "0") << "\n";
    f << "eyeCm=" << eyeDistanceCm << "\n";
    f << "bezelMm=" << globalBezelMm << "\n";
    f << "monitorSelected=";
    for (size_t i = 0; i < selected.size(); ++i) {
        f << (selected[i] ? "1" : "0");
        if (i + 1 < selected.size()) f << ",";
    }
    f << "\nmonitorBezels=";
    for (size_t i = 0; i < bezelMm.size(); ++i) {
        f << bezelMm[i];
        if (i + 1 < bezelMm.size()) f << ",";
    }
    f << "\n";
}

void DisplaySystem::load(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "spanAll") spanAllMonitors = (val == "1");
        else if (key == "eyeCm") eyeDistanceCm = (float)std::atof(val.c_str());
        else if (key == "bezelMm") globalBezelMm = std::atoi(val.c_str());
        else if (key == "monitorSelected") {
            std::stringstream ss(val);
            std::string tok;
            size_t idx = 0;
            while (std::getline(ss, tok, ',') && idx < selected.size()) {
                selected[idx++] = (tok == "1");
            }
        }
        else if (key == "monitorBezels") {
            std::stringstream ss(val);
            std::string tok;
            size_t idx = 0;
            while (std::getline(ss, tok, ',') && idx < bezelMm.size()) {
                bezelMm[idx++] = std::atoi(tok.c_str());
            }
        }
    }

    eyeDistanceCm = std::clamp(eyeDistanceCm, 10.0f, 500.0f);
    globalBezelMm = std::clamp(globalBezelMm, 0, 100);

    for (int& bezel : bezelMm)
        bezel = std::clamp(bezel, 0, 100);

    // A stale settings file must not leave spanning enabled with no output
    // monitors selected. Prefer the current primary monitor as a safe fallback.
    const bool anySelected = std::any_of(
        selected.begin(),
        selected.end(),
        [](bool value) { return value; });

    if (!anySelected && !selected.empty())
    {
        const std::size_t safePrimary = std::min(
            m_primaryMonitorIndex,
            selected.size() - 1);
        selected[safePrimary] = true;
    }
}

} // namespace heritage::graphics