#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "../../../Camera/VehicleCameraController.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <imgui.h>

namespace heritage::modules {
namespace {

heritage::math::Vec3 subtractVec3(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

float dotVec3(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

heritage::math::Vec3 crossVec3(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

heritage::math::Vec3 normalizeVec3(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const double lengthSquared = static_cast<double>(dotVec3(value, value));
    if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12)
        return fallback;
    const float inverseLength = static_cast<float>(1.0 / std::sqrt(lengthSquared));
    return {
        value.x * inverseLength,
        value.y * inverseLength,
        value.z * inverseLength
    };
}

bool projectWorldPointToUi(
    const heritage::camera::CameraFrame& frame,
    int framebufferWidth,
    int framebufferHeight,
    const heritage::math::Vec3& world,
    ImVec2& screen)
{
    if (!frame.valid || framebufferWidth <= 0 || framebufferHeight <= 0)
        return false;

    const heritage::math::Vec3 forward = normalizeVec3(
        subtractVec3(frame.targetLocal, frame.eyeLocal),
        { 0.0f, 0.0f, 1.0f });
    const heritage::math::Vec3 right = normalizeVec3(
        crossVec3(forward, frame.up),
        { 1.0f, 0.0f, 0.0f });
    const heritage::math::Vec3 correctedUp = normalizeVec3(
        crossVec3(right, forward),
        { 0.0f, 1.0f, 0.0f });
    const heritage::math::Vec3 fromEye = subtractVec3(world, frame.eyeLocal);
    const float depth = dotVec3(fromEye, forward);
    if (!std::isfinite(depth) || depth <= 0.02f)
        return false;

    constexpr float kRenderVerticalFovRadians = 0.6f;
    const float aspect = static_cast<float>(framebufferWidth)
        / static_cast<float>(framebufferHeight);
    const float focalScale = 1.0f / std::tan(kRenderVerticalFovRadians * 0.5f);
    const float ndcX = dotVec3(fromEye, right) * (focalScale / aspect) / depth;
    const float ndcY = dotVec3(fromEye, correctedUp) * focalScale / depth;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)
        || ndcX < -1.10f || ndcX > 1.10f
        || ndcY < -1.10f || ndcY > 1.10f)
    {
        return false;
    }

    screen.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(framebufferWidth);
    screen.y = (0.5f - ndcY * 0.5f) * static_cast<float>(framebufferHeight);
    return true;
}

} // namespace

int LuaCoreBindingHandlers::luaUiWorldLabel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleCamera)
        return 0;

    const std::string text = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const heritage::math::Vec3 world{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const auto bounded = [](double value) {
        return std::clamp(static_cast<float>(value), 0.0f, 1.0f);
    };
    const float red = bounded(LuaModuleRuntime::numberArgument(*runtime, state, 5, 1.0));
    const float green = bounded(LuaModuleRuntime::numberArgument(*runtime, state, 6, 1.0));
    const float blue = bounded(LuaModuleRuntime::numberArgument(*runtime, state, 7, 1.0));
    const float alpha = bounded(LuaModuleRuntime::numberArgument(*runtime, state, 8, 1.0));
    const float verticalOffset = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 9, 8.0));

    ImVec2 anchor{};
    if (text.empty()
        || !projectWorldPointToUi(
            runtime->m_vehicleCamera->presentedFrame(),
            runtime->m_framebufferWidth,
            runtime->m_framebufferHeight,
            world,
            anchor))
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    // World labels belong above the rendered scene but below ordinary module
    // windows, so a laboratory panel naturally occludes annotations behind it.
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
    const ImVec2 textPosition(
        anchor.x - textSize.x * 0.5f,
        anchor.y - textSize.y - verticalOffset);
    const ImVec2 padding(5.0f, 3.0f);
    const ImVec2 backgroundMinimum(
        textPosition.x - padding.x,
        textPosition.y - padding.y);
    const ImVec2 backgroundMaximum(
        textPosition.x + textSize.x + padding.x,
        textPosition.y + textSize.y + padding.y);
    drawList->AddRectFilled(
        backgroundMinimum,
        backgroundMaximum,
        ImGui::GetColorU32(ImVec4(0.015f, 0.020f, 0.025f, 0.78f)),
        3.0f);
    drawList->AddRect(
        backgroundMinimum,
        backgroundMaximum,
        ImGui::GetColorU32(ImVec4(red, green, blue, alpha * 0.85f)),
        3.0f,
        0,
        1.0f);
    drawList->AddText(
        textPosition,
        ImGui::GetColorU32(ImVec4(red, green, blue, alpha)),
        text.c_str());

    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

} // namespace heritage::modules
