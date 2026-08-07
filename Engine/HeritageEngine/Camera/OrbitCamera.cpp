#include "OrbitCamera.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>

#include <algorithm>
#include <cmath>

namespace heritage::camera {

using heritage::math::Mat4;
using heritage::math::Vec3;

OrbitCamera* OrbitCamera::s_activeCamera = nullptr;

OrbitCamera::OrbitCamera()
{
    rebuildViewMatrix();
}

void OrbitCamera::installCallbacks(GLFWwindow* window)
{
    s_activeCamera = this;
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetScrollCallback(window, scrollCallback);
}

void OrbitCamera::uninstallCallbacks()
{
    if (s_activeCamera == this)
        s_activeCamera = nullptr;
}

void OrbitCamera::update(float deltaTime, bool allowAutomaticRotation)
{
    if (!m_dragging && allowAutomaticRotation)
        m_automaticYaw += deltaTime * 0.6f;

    rebuildViewMatrix();
}

void OrbitCamera::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

    if (!s_activeCamera || ImGui::GetIO().WantCaptureMouse)
        return;

    s_activeCamera->handleMouseButton(button, action);
}

void OrbitCamera::cursorPositionCallback(GLFWwindow* window, double x, double y)
{
    ImGui_ImplGlfw_CursorPosCallback(window, x, y);

    if (!s_activeCamera)
        return;

    if (!ImGui::GetIO().WantCaptureMouse)
        s_activeCamera->handleCursorPosition(x, y);
    else
    {
        // Preserve the previous callback's last-position tracking even while
        // ImGui owns the mouse, preventing a jump when camera input resumes.
        s_activeCamera->m_lastX = x;
        s_activeCamera->m_lastY = y;
    }
}

void OrbitCamera::scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    ImGui_ImplGlfw_ScrollCallback(window, xOffset, yOffset);

    if (!s_activeCamera || ImGui::GetIO().WantCaptureMouse)
        return;

    s_activeCamera->handleScroll(yOffset);
}

void OrbitCamera::handleMouseButton(int button, int action)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        m_dragging = (action == GLFW_PRESS);
}

void OrbitCamera::handleCursorPosition(double x, double y)
{
    if (m_dragging)
    {
        m_orbitY += static_cast<float>(x - m_lastX) * 0.01f;
        m_orbitX += static_cast<float>(y - m_lastY) * 0.01f;
        m_orbitX = std::clamp(m_orbitX, -1.4f, 1.4f);
    }

    m_lastX = x;
    m_lastY = y;
}

void OrbitCamera::handleScroll(double yOffset)
{
    m_zoom -= static_cast<float>(yOffset) * 0.3f;
    m_zoom = std::clamp(m_zoom, 2.0f, 12.0f);
}

void OrbitCamera::rebuildViewMatrix()
{
    const float yaw = m_orbitY + m_automaticYaw;
    const float camX = std::sin(yaw) * std::cos(m_orbitX) * m_zoom;
    const float camY = std::sin(m_orbitX) * m_zoom;
    const float camZ = std::cos(yaw) * std::cos(m_orbitX) * m_zoom;

    m_eye = { camX, camY, camZ };
    const Vec3 up{ 0.0f, 1.0f, 0.0f };

    Vec3 forward{ -camX, -camY, -camZ };
    const float forwardLength = std::sqrt(
        forward.x * forward.x +
        forward.y * forward.y +
        forward.z * forward.z);
    forward = {
        forward.x / forwardLength,
        forward.y / forwardLength,
        forward.z / forwardLength
    };

    Vec3 right{
        forward.y * up.z - forward.z * up.y,
        forward.z * up.x - forward.x * up.z,
        forward.x * up.y - forward.y * up.x
    };
    const float rightLength = std::sqrt(
        right.x * right.x +
        right.y * right.y +
        right.z * right.z);
    right = {
        right.x / rightLength,
        right.y / rightLength,
        right.z / rightLength
    };

    const Vec3 cameraUp{
        right.y * forward.z - right.z * forward.y,
        right.z * forward.x - right.x * forward.z,
        right.x * forward.y - right.y * forward.x
    };

    m_view = Mat4{};
    m_view.m[0] = right.x;
    m_view.m[4] = right.y;
    m_view.m[8] = right.z;

    m_view.m[1] = cameraUp.x;
    m_view.m[5] = cameraUp.y;
    m_view.m[9] = cameraUp.z;

    m_view.m[2] = -forward.x;
    m_view.m[6] = -forward.y;
    m_view.m[10] = -forward.z;
    m_view.m[15] = 1.0f;

    m_view.m[12] = -(right.x * m_eye.x + right.y * m_eye.y + right.z * m_eye.z);
    m_view.m[13] = -(cameraUp.x * m_eye.x + cameraUp.y * m_eye.y + cameraUp.z * m_eye.z);
    m_view.m[14] = forward.x * m_eye.x + forward.y * m_eye.y + forward.z * m_eye.z;
}

} // namespace heritage::camera
