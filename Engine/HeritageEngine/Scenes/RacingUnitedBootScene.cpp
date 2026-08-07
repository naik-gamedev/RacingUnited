#include "RacingUnitedBootScene.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace heritage::scenes {

bool RacingUnitedBootScene::initialize(
    GLFWwindow*,
    const heritage::modules::ModuleContext& moduleContext,
    std::string& errorMessage)
{
    m_moduleName = moduleContext.module().name.empty()
        ? "Racing United"
        : moduleContext.module().name;
    m_elapsedTime = 0.0f;
    errorMessage.clear();
    return true;
}

void RacingUnitedBootScene::shutdown()
{
    m_elapsedTime = 0.0f;
}

void RacingUnitedBootScene::update(float deltaTime, bool)
{
    m_elapsedTime += deltaTime;
}

heritage::math::Vec3 RacingUnitedBootScene::clearColor() const
{
    const float pulse = 0.006f * (0.5f + 0.5f * std::sin(m_elapsedTime * 0.7f));
    return { 0.012f + pulse, 0.016f + pulse, 0.025f + pulse };
}

void RacingUnitedBootScene::draw(
    const heritage::math::Mat4&,
    const heritage::settings::VideoSettings&) const
{
    // This deliberately has no borrowed 3D asset. It proves that selecting the
    // RacingUnited module routes to its own scene rather than the logo scene.
}

void RacingUnitedBootScene::drawOverlay(
    int framebufferWidth,
    int framebufferHeight) const
{
    const float availableWidth = (std::max)(220.0f, static_cast<float>(framebufferWidth) - 40.0f);
    const float panelWidth = (std::min)(520.0f, availableWidth);
    const float panelHeight = 176.0f;

    ImGui::SetNextWindowPos(
        ImVec2(framebufferWidth * 0.5f, framebufferHeight * 0.5f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.040f, 0.052f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.24f, 0.27f, 0.34f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGui::Begin(
        "##racing_united_boot_scene",
        nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs);

    ImGui::SetWindowFontScale(1.45f);
    ImGui::TextUnformatted(m_moduleName.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextDisabled("MODULE-OWNED BOOT SCENE");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped(
        "This scene was selected by entry_scene = racing_united_boot. "
        "It intentionally does not load LogoShowcase's 3D mesh.");
    ImGui::Spacing();
    ImGui::TextDisabled("Gameplay content will be supplied by this module later.");

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

} // namespace heritage::scenes
