#include "WindowSystem.hpp"
#include <algorithm>
#include <cmath>

namespace heritage::graphics {

void WindowSystem::initialize(GLFWwindow* window)
{
    m_window = window;

    m_cursorArrow   = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    m_cursorHResize = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    m_cursorVResize = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);

#ifdef _WIN32
    m_hCursorArrow = LoadCursor(NULL, IDC_ARROW);
    m_hCursorH     = LoadCursor(NULL, IDC_SIZEWE);
    m_hCursorV     = LoadCursor(NULL, IDC_SIZENS);
    m_hCursorNWSE  = LoadCursor(NULL, IDC_SIZENWSE);
    m_hCursorNESW  = LoadCursor(NULL, IDC_SIZENESW);
#endif

    // Store initial size/pos
    glfwGetWindowPos(window, &m_savedX, &m_savedY);
    glfwGetWindowSize(window, &m_savedW, &m_savedH);
}

void WindowSystem::shutdown()
{
    // Cursors are destroyed automatically by GLFW on terminate
}

void WindowSystem::setCursor(CursorType t)
{
#ifdef _WIN32
    HCURSOR hc = nullptr;
    switch (t) {
    case CursorType::Arrow:    hc = m_hCursorArrow; break;
    case CursorType::HResize:  hc = m_hCursorH; break;
    case CursorType::VResize:  hc = m_hCursorV; break;
    case CursorType::DiagNWSE: hc = m_hCursorNWSE; break;
    case CursorType::DiagNESW: hc = m_hCursorNESW; break;
    }
    if (hc) SetCursor(hc);
#else
    switch (t) {
    case CursorType::Arrow:    glfwSetCursor(m_window, m_cursorArrow); break;
    case CursorType::HResize:  glfwSetCursor(m_window, m_cursorHResize); break;
    case CursorType::VResize:  glfwSetCursor(m_window, m_cursorVResize); break;
    default:                   glfwSetCursor(m_window, m_cursorArrow); break;
    }
#endif
}

void WindowSystem::saveCurrentRect(GLFWwindow* window)
{
    glfwGetWindowPos(window, &m_savedX, &m_savedY);
    glfwGetWindowSize(window, &m_savedW, &m_savedH);
}

void WindowSystem::setMode(
    GLFWwindow* window,
    WindowMode mode,
    int desiredW,
    int desiredH,
    int desiredRefresh,
    GLFWmonitor* targetMonitor)
{
    GLFWmonitor* monitor = targetMonitor ? targetMonitor : glfwGetPrimaryMonitor();
    if (!monitor)
        return;

    const GLFWvidmode* desktopMode = glfwGetVideoMode(monitor);
    if (!desktopMode)
        return;

    if (mode == WindowMode::Windowed)
    {
        const bool windowSizeChanged = desiredW > 0 || desiredH > 0;

        if (desiredW > 0)
            m_savedW = desiredW;
        if (desiredH > 0)
            m_savedH = desiredH;

        // A resolution chosen from the Video menu supplies a new window size.
        // Re-center that size inside the monitor work area so the window does
        // not drift down and to the right after repeated mode changes.
        // Manual freeform resizing does not pass a requested size, so it keeps
        // the position chosen by the user.
        if (windowSizeChanged)
        {
            int workX = 0;
            int workY = 0;
            int workW = desktopMode->width;
            int workH = desktopMode->height;
            glfwGetMonitorWorkarea(monitor, &workX, &workY, &workW, &workH);

            m_savedX = workX + (std::max)(0, (workW - m_savedW) / 2);
            m_savedY = workY + (std::max)(0, (workH - m_savedH) / 2);
        }

        glfwSetWindowMonitor(
            window,
            nullptr,
            m_savedX,
            m_savedY,
            m_savedW,
            m_savedH,
            GLFW_DONT_CARE);
    }
    else if (mode == WindowMode::Borderless)
    {
        int monitorX = 0;
        int monitorY = 0;
        glfwGetMonitorPos(monitor, &monitorX, &monitorY);

        glfwSetWindowMonitor(
            window,
            nullptr,
            monitorX,
            monitorY,
            desktopMode->width,
            desktopMode->height,
            GLFW_DONT_CARE);
    }
    else
    {
        const int width = (desiredW > 0) ? desiredW : desktopMode->width;
        const int height = (desiredH > 0) ? desiredH : desktopMode->height;
        const int refresh = (desiredRefresh != GLFW_DONT_CARE)
            ? desiredRefresh
            : desktopMode->refreshRate;

        glfwSetWindowMonitor(window, monitor, 0, 0, width, height, refresh);
    }

    m_mode = mode;
}

void WindowSystem::cycleMode(GLFWwindow* window)
{
    if (m_mode == WindowMode::Windowed) {
        saveCurrentRect(window);
        setMode(window, WindowMode::Borderless);
    }
    else if (m_mode == WindowMode::Borderless) {
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        const GLFWvidmode* vid = glfwGetVideoMode(mon);
        setMode(window, WindowMode::Exclusive, vid->width, vid->height, vid->refreshRate);
    }
    else {
        setMode(window, WindowMode::Windowed);
    }
}

void WindowSystem::update(GLFWwindow* window)
{
    // Handle pending restore after maximize
    if (m_pendingRestore) {
        int maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);
        if (!maximized) {
            glfwSetWindowPos(window, m_savedX, m_savedY);
            glfwSetWindowSize(window, m_savedW, m_savedH);
            m_isMaximized = false;
            m_pendingRestore = false;
        }
    }
}

void WindowSystem::drawTitlebar(GLFWwindow* window, int fbW, int fbH, bool& shouldClose, bool& shouldMin)
{
    bool showTitlebar = (m_mode == WindowMode::Windowed);
    if (!showTitlebar) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        if (my < 50) showTitlebar = true;
    }
    if (!showTitlebar) return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)fbW, (float)TITLEBAR_H));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.05f, 0.95f));
    ImGui::Begin("##titlebar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);

    // Drag region
    if (m_mode == WindowMode::Windowed) {
        ImGui::SetCursorPos(ImVec2(RESIZE_BORDER, 0));
        ImGui::InvisibleButton("##drag", ImVec2((float)fbW - 120.f - RESIZE_BORDER * 2.f, (float)TITLEBAR_H));
        if (ImGui::IsItemActive()) {
            if (!m_dragging) {
                m_dragging = true;
                glfwGetWindowPos(window, &m_dragWinX, &m_dragWinY);
                double cx, cy;
                glfwGetCursorPos(window, &cx, &cy);
                m_dragStartX = m_dragWinX + cx;
                m_dragStartY = m_dragWinY + cy;
            }
            int curX, curY;
            double cx, cy;
            glfwGetWindowPos(window, &curX, &curY);
            glfwGetCursorPos(window, &cx, &cy);
            glfwSetWindowPos(window,
                m_dragWinX + (int)((curX + cx) - m_dragStartX),
                m_dragWinY + (int)((curY + cy) - m_dragStartY));
        }
        else {
            m_dragging = false;
        }
    }

    // Title text
    ImGui::SetCursorPos(ImVec2(10, 7));
    ImGui::TextDisabled("HERITAGE ENGINE");
    ImGui::SameLine();
    ImGui::SetCursorPosY(7);
    const char* modeName = (m_mode == WindowMode::Windowed) ? "Windowed" :
                           (m_mode == WindowMode::Borderless) ? "Borderless" : "Exclusive";
    ImGui::TextDisabled("|  %s  |  F6 Sky  |  F7 Time  |  F9 Wire  |  F10 Radar  |  Insert TireDbg  |  F11  |  ESC = Menu", modeName);

    // Buttons
    ImGui::SetCursorPos(ImVec2((float)fbW - 112, 1));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1));

    if (ImGui::Button(" _ ##min", ImVec2(36, 26))) shouldMin = true;

    // Maximize / Restore
    ImGui::PushID("maxbtn");
    ImGui::SetCursorPos(ImVec2((float)fbW - 76, 1));
    ImGui::InvisibleButton("##maxbtn", ImVec2(36, 26));
    ImVec2 rmin = ImGui::GetItemRectMin();
    ImVec2 rmax = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 center = ImVec2((rmin.x + rmax.x) * 0.5f, (rmin.y + rmax.y) * 0.5f);
    float iw = 12.f, ih = 10.f;
    ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);

    if (!m_isMaximized) {
        ImVec2 a(center.x - iw * 0.5f, center.y - ih * 0.5f);
        dl->AddRect(a, ImVec2(a.x + iw, a.y + ih), col, 2.0f);
    } else {
        ImVec2 a1(center.x - iw * 0.6f, center.y - ih * 0.4f);
        ImVec2 a2(a1.x + 3.f, a1.y + 3.f);
        dl->AddRect(a1, ImVec2(a1.x + iw, a1.y + ih), col, 2.0f);
        dl->AddRect(a2, ImVec2(a2.x + iw, a2.y + ih), col, 2.0f);
    }

    if (ImGui::IsItemClicked()) {
        if (!m_isMaximized) {
            saveCurrentRect(window);
            glfwMaximizeWindow(window);
            m_isMaximized = true;
            m_pendingRestore = false;
        } else {
            glfwRestoreWindow(window);
            m_pendingRestore = true;
        }
    }
    ImGui::PopID();

    ImGui::SetCursorPos(ImVec2((float)fbW - 40, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.1f, 0.1f, 1));
    if (ImGui::Button(" X ##cls", ImVec2(36, 26))) {
        shouldClose = true;
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    ImGui::PopStyleColor(4);

    ImGui::End();
    ImGui::PopStyleColor();
}

void WindowSystem::drawResizeHandles(GLFWwindow* window)
{
    if (m_mode != WindowMode::Windowed) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs;

    int winW = 0, winH = 0;
    glfwGetWindowSize(window, &winW, &winH);

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)winW, (float)winH), ImGuiCond_Always);
    ImGui::Begin("##resize_overlay", nullptr, flags);

    double cx_d, cy_d;
    glfwGetCursorPos(window, &cx_d, &cy_d);
    int cx = (int)cx_d, cy = (int)cy_d;
    const int corner = 12;
    const float th = RESIZE_BORDER;

    int hover = 0;
    if (cx >= 0 && cy >= winH - corner && cx <= corner && cy <= winH) hover = 1 | 8;
    else if (cx >= winW - corner && cy >= winH - corner && cx <= winW && cy <= winH) hover = 2 | 8;
    else if (cx >= 0 && cx <= (int)th) hover = 1;
    else if (cx >= winW - (int)th && cx <= winW) hover = 2;
    else if (cy >= winH - (int)th && cy <= winH) hover = 8;

    ImGuiIO& io = ImGui::GetIO();
    bool mouseDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

    if (!m_resizing) {
        if (!io.WantCaptureMouse && hover != 0) {
            if ((hover & (1 | 8)) == (1 | 8))
                setCursor(CursorType::DiagNESW);
            else if ((hover & (2 | 8)) == (2 | 8))
                setCursor(CursorType::DiagNWSE);
            else if (hover & (1 | 2))
                setCursor(CursorType::HResize);
            else if (hover & 8)
                setCursor(CursorType::VResize);

            if (mouseDown) {
                m_resizing = true;
                m_resizeDir = hover;
                glfwGetWindowPos(window, &m_resizeStartX, &m_resizeStartY);
                glfwGetWindowSize(window, &m_resizeStartW, &m_resizeStartH);
                m_resizeStartMouseX = (float)cx;
                m_resizeStartMouseY = (float)cy;
            }
        } else {
            setCursor(CursorType::Arrow);
        }
    } else {
        // Active resizing
        if ((m_resizeDir & 1) && (m_resizeDir & 8)) setCursor(CursorType::DiagNESW);
        else if ((m_resizeDir & 2) && (m_resizeDir & 8)) setCursor(CursorType::DiagNWSE);
        else if (m_resizeDir & (1 | 2)) setCursor(CursorType::HResize);
        else if (m_resizeDir & 8) setCursor(CursorType::VResize);

        float ddx = (float)cx - m_resizeStartMouseX;
        float ddy = (float)cy - m_resizeStartMouseY;

        int newX = m_resizeStartX;
        int newY = m_resizeStartY;
        int newW = m_resizeStartW;
        int newH = m_resizeStartH;

        if (m_resizeDir & 1) { newX = (int)(m_resizeStartX + ddx); newW = m_resizeStartW - (newX - m_resizeStartX); }
        if (m_resizeDir & 2) { newW = (int)(m_resizeStartW + ddx); }
        if (m_resizeDir & 8) { newH = (int)(m_resizeStartH + ddy); }

        newW = (std::max)(newW, 200);
        newH = (std::max)(newH, 120);

        glfwSetWindowPos(window, newX, newY);
        glfwSetWindowSize(window, newW, newH);
        m_savedX = newX; m_savedY = newY; m_savedW = newW; m_savedH = newH;

        if (!mouseDown) {
            m_resizing = false;
            m_resizeDir = 0;
            setCursor(CursorType::Arrow);
        }
    }

    ImGui::End();
}

} // namespace heritage::graphics