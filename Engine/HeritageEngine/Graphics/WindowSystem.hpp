#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace heritage::graphics {

enum class WindowMode { Windowed, Borderless, Exclusive };

class WindowSystem {
public:
    void initialize(GLFWwindow* window);
    void shutdown();

    // Call once per frame (before ImGui)
    void update(GLFWwindow* window);

    // Call inside the ImGui frame (after NewFrame, before Render)
    void drawTitlebar(GLFWwindow* window, int fbW, int fbH, bool& shouldClose, bool& shouldMin);
    void drawResizeHandles(GLFWwindow* window);

    // Window mode
    WindowMode mode() const { return m_mode; }
    void setMode(
        GLFWwindow* window,
        WindowMode mode,
        int desiredW = 0,
        int desiredH = 0,
        int desiredRefresh = GLFW_DONT_CARE,
        GLFWmonitor* targetMonitor = nullptr);
    void cycleMode(GLFWwindow* window);   // F11 behaviour

    // Saved rect
    void saveCurrentRect(GLFWwindow* window);
    int  savedX() const { return m_savedX; }
    int  savedY() const { return m_savedY; }
    int  savedW() const { return m_savedW; }
    int  savedH() const { return m_savedH; }

    bool isMaximized() const { return m_isMaximized; }

private:
    GLFWwindow* m_window = nullptr;

    WindowMode m_mode = WindowMode::Windowed;
    int m_savedX = 100, m_savedY = 100, m_savedW = 1280, m_savedH = 720;
    bool m_isMaximized = false;
    bool m_pendingRestore = false;

    // Titlebar drag
    bool   m_dragging = false;
    double m_dragStartX = 0, m_dragStartY = 0;
    int    m_dragWinX = 0, m_dragWinY = 0;

    // Resize
    bool  m_resizing = false;
    int   m_resizeDir = 0;
    int   m_resizeStartX = 0, m_resizeStartY = 0;
    int   m_resizeStartW = 0, m_resizeStartH = 0;
    float m_resizeStartMouseX = 0.f, m_resizeStartMouseY = 0.f;

    // Cursors
    GLFWcursor* m_cursorArrow = nullptr;
    GLFWcursor* m_cursorHResize = nullptr;
    GLFWcursor* m_cursorVResize = nullptr;
#ifdef _WIN32
    HCURSOR m_hCursorArrow = nullptr;
    HCURSOR m_hCursorH = nullptr;
    HCURSOR m_hCursorV = nullptr;
    HCURSOR m_hCursorNWSE = nullptr;
    HCURSOR m_hCursorNESW = nullptr;
#endif

    enum class CursorType { Arrow, HResize, VResize, DiagNWSE, DiagNESW };
    void setCursor(CursorType t);

    static constexpr int TITLEBAR_H = 28;
    static constexpr float RESIZE_BORDER = 8.0f;
};

} // namespace heritage::graphics