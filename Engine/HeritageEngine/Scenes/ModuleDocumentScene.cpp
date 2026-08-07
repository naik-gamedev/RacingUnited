#include "ModuleDocumentScene.hpp"

#include <algorithm>
#include <utility>

#include <imgui.h>

namespace heritage::scenes {

ModuleDocumentScene::ModuleDocumentScene(
    std::string sceneId,
    std::filesystem::path documentPath,
    heritage::entities::EntityRegistry* entityRegistry)
    : m_sceneId(std::move(sceneId)),
      m_documentPath(std::move(documentPath)),
      m_entityRegistry(entityRegistry)
{
}

bool ModuleDocumentScene::initialize(
    GLFWwindow*,
    const heritage::modules::ModuleContext&,
    std::string& errorMessage)
{
    shutdown();

    return heritage::entities::EntitySceneDocument::load(
        m_documentPath,
        m_sceneId,
        m_entityRegistry,
        m_documentInfo,
        m_sceneEntities,
        errorMessage);
}

void ModuleDocumentScene::shutdown()
{
    if (m_entityRegistry)
    {
        // Destroy in reverse declaration order. Destroying a root recursively
        // removes its descendants; existence checks make later entries safe.
        for (auto iterator = m_sceneEntities.rbegin();
             iterator != m_sceneEntities.rend();
             ++iterator)
        {
            if (m_entityRegistry->exists(*iterator))
                m_entityRegistry->destroy(*iterator);
        }
    }

    m_sceneEntities.clear();
    m_documentInfo = {};
}

void ModuleDocumentScene::update(float, bool)
{
}

heritage::math::Vec3 ModuleDocumentScene::clearColor() const
{
    return m_documentInfo.clearColor;
}

void ModuleDocumentScene::draw(
    const heritage::math::Mat4&,
    const heritage::settings::VideoSettings&) const
{
    // Entity components are rendered centrally after the active scene. This
    // keeps scene documents data-only and lets future mesh/physics components
    // share the same renderer without embedding rendering code in .hscene.
}

void ModuleDocumentScene::drawOverlay(
    int framebufferWidth,
    int framebufferHeight) const
{
    if (!m_documentInfo.showOverlay)
        return;

    const float availableWidth =
        (std::max)(260.0f, static_cast<float>(framebufferWidth) - 40.0f);
    const float panelWidth = (std::min)(640.0f, availableWidth);
    const float availableHeight =
        (std::max)(180.0f, static_cast<float>(framebufferHeight) - 40.0f);
    const float panelHeight = (std::min)(320.0f, availableHeight);

    ImGui::SetNextWindowPos(
        ImVec2(framebufferWidth * 0.5f, framebufferHeight * 0.5f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

    ImGui::PushStyleColor(
        ImGuiCol_WindowBg,
        ImVec4(0.035f, 0.040f, 0.048f, 0.96f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));

    ImGui::Begin(
        ("##module_document_scene_" + m_sceneId).c_str(),
        nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs);

    if (!m_documentInfo.title.empty())
    {
        ImGui::SetWindowFontScale(1.30f);
        ImGui::TextUnformatted(m_documentInfo.title.c_str());
        ImGui::SetWindowFontScale(1.0f);
    }

    if (!m_documentInfo.subtitle.empty())
        ImGui::TextDisabled("%s", m_documentInfo.subtitle.c_str());

    if (!m_documentInfo.title.empty() || !m_documentInfo.subtitle.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    if (!m_documentInfo.text.empty())
        ImGui::TextWrapped("%s", m_documentInfo.text.c_str());

    if (m_documentInfo.type == "entities")
    {
        ImGui::Spacing();
        ImGui::TextDisabled(
            "Scene-owned entities: %zu",
            m_documentInfo.entityCount);
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace heritage::scenes
