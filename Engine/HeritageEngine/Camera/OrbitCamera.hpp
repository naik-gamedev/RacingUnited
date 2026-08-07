#pragma once

struct GLFWwindow;

#include "../Core/Math/Math.hpp"

namespace heritage::camera {

// Temporary orbit camera used by the Heritage Engine logo scene.
// Owns mouse orbit, scroll zoom, automatic rotation, camera position,
// and construction of the view matrix.
class OrbitCamera
{
public:
    OrbitCamera();

    // Installs the same GLFW callbacks that were previously in main.cpp.
    // The callbacks continue forwarding events to ImGui before handling
    // camera input, preserving the existing behavior.
    void installCallbacks(GLFWwindow* window);
    void uninstallCallbacks();

    // Advances automatic yaw when allowed and rebuilds the camera matrices.
    void update(float deltaTime, bool allowAutomaticRotation);

    const heritage::math::Mat4& viewMatrix() const { return m_view; }
    const heritage::math::Vec3& eye() const { return m_eye; }
    bool isDragging() const { return m_dragging; }

private:
    static OrbitCamera* s_activeCamera;

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPositionCallback(GLFWwindow* window, double x, double y);
    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);

    void handleMouseButton(int button, int action);
    void handleCursorPosition(double x, double y);
    void handleScroll(double yOffset);
    void rebuildViewMatrix();

    float m_orbitX = 0.3f;
    float m_orbitY = 0.0f;
    float m_zoom = 4.5f;
    float m_automaticYaw = 0.0f;

    bool m_dragging = false;
    double m_lastX = 0.0;
    double m_lastY = 0.0;

    heritage::math::Vec3 m_eye{};
    heritage::math::Mat4 m_view{};
};

} // namespace heritage::camera
