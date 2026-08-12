#include "SceneManager.hpp"

#include "LogoShowcaseScene.hpp"
#include "ModuleDocumentScene.hpp"

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace heritage::scenes {
namespace {

class EmptyScene final : public Scene
{
public:
    bool initialize(
        GLFWwindow*,
        const heritage::modules::ModuleContext&,
        std::string& errorMessage) override
    {
        errorMessage.clear();
        return true;
    }

    void shutdown() override {}
    void update(float, bool) override {}

    heritage::math::Vec3 clearColor() const override
    {
        return { 0.0f, 0.0f, 0.0f };
    }

    void draw(
        const heritage::math::Mat4&,
        const heritage::settings::VideoSettings&) const override
    {
        // Intentional pitch-black nothingness. A module without an entry scene
        // never borrows LogoShowcase or any other module's content.
    }
};

class ErrorScene final : public Scene
{
public:
    explicit ErrorScene(std::string message)
        : m_message(std::move(message))
    {
    }

    bool initialize(
        GLFWwindow*,
        const heritage::modules::ModuleContext&,
        std::string& errorMessage) override
    {
        errorMessage.clear();
        return true;
    }

    void shutdown() override {}
    void update(float, bool) override {}

    heritage::math::Vec3 clearColor() const override
    {
        return { 0.055f, 0.008f, 0.010f };
    }

    void draw(
        const heritage::math::Mat4&,
        const heritage::settings::VideoSettings&) const override
    {
    }

    void drawOverlay(int framebufferWidth, int framebufferHeight) const override
    {
        const float availableWidth = (std::max)(260.0f, static_cast<float>(framebufferWidth) - 40.0f);
        const float panelWidth = (std::min)(640.0f, availableWidth);
        const float availableHeight = (std::max)(180.0f, static_cast<float>(framebufferHeight) - 40.0f);
        const float panelHeight = (std::min)(280.0f, availableHeight);

        ImGui::SetNextWindowPos(
            ImVec2(framebufferWidth * 0.5f, framebufferHeight * 0.5f),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.015f, 0.018f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.75f, 0.12f, 0.14f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        ImGui::Begin(
            "##module_scene_error",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs);

        ImGui::SetWindowFontScale(1.35f);
        ImGui::TextUnformatted("MODULE SCENE ERROR");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_message.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled(
            "Heritage Engine did not fall back to another module's content.");

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

private:
    std::string m_message;
};

bool isBuiltInScene(const std::string& sceneId)
{
    return sceneId == "logo_showcase";
}

std::filesystem::path documentPathFor(
    const std::string& sceneId,
    const heritage::modules::ModuleContext& moduleContext)
{
    std::filesystem::path relative(sceneId);
    if (relative.extension().empty())
        relative += ".hscene";

    return moduleContext.resolveScenePath(relative);
}

std::unique_ptr<Scene> createBuiltInScene(const std::string& sceneId)
{
    if (sceneId == "logo_showcase")
        return std::make_unique<LogoShowcaseScene>();

    return {};
}

} // namespace

bool SceneManager::load(
    const std::string& entryScene,
    GLFWwindow* window,
    const heritage::modules::ModuleContext& moduleContext,
    heritage::entities::EntityRegistry* entityRegistry,
    std::string& errorMessage)
{
    shutdown();

    if (entryScene.empty())
    {
        m_activeSceneId = "<empty>";
        m_activeScene = std::make_unique<EmptyScene>();
        std::string ignored;
        m_activeScene->initialize(window, moduleContext, ignored);
        errorMessage.clear();
        return true;
    }

    m_activeSceneId = entryScene;
    std::unique_ptr<Scene> requestedScene;

    const std::filesystem::path documentPath = documentPathFor(entryScene, moduleContext);
    if (!documentPath.empty() && std::filesystem::is_regular_file(documentPath))
    {
        requestedScene = std::make_unique<ModuleDocumentScene>(
            entryScene,
            documentPath,
            entityRegistry);
    }
    else
    {
        requestedScene = createBuiltInScene(entryScene);
    }

    if (!requestedScene)
    {
        errorMessage = "Module '" + moduleContext.module().id
            + "' requested unknown scene '" + entryScene + "'.\n\n"
            + "Heritage Engine looked for:\n"
            + (documentPath.empty() ? std::string("<unsafe scene path>") : documentPath.string())
            + "\n\nand for a registered built-in scene with that ID.";
        m_activeScene = std::make_unique<ErrorScene>(errorMessage);
        std::string ignored;
        m_activeScene->initialize(window, moduleContext, ignored);
        return false;
    }

    std::string initializationError;
    if (!requestedScene->initialize(window, moduleContext, initializationError))
    {
        errorMessage = initializationError.empty()
            ? "Scene '" + entryScene + "' failed to initialize."
            : initializationError;

        requestedScene->shutdown();
        m_activeScene = std::make_unique<ErrorScene>(errorMessage);
        std::string ignored;
        m_activeScene->initialize(window, moduleContext, ignored);
        return false;
    }

    m_activeScene = std::move(requestedScene);
    errorMessage.clear();
    return true;
}

bool SceneManager::exists(
    const std::string& sceneId,
    const heritage::modules::ModuleContext& moduleContext) const
{
    if (sceneId.empty())
        return false;

    if (isBuiltInScene(sceneId))
        return true;

    const std::filesystem::path documentPath = documentPathFor(sceneId, moduleContext);
    return !documentPath.empty() && std::filesystem::is_regular_file(documentPath);
}

void SceneManager::shutdown()
{
    if (m_activeScene)
        m_activeScene->shutdown();

    m_activeScene.reset();
    m_activeSceneId.clear();
}

void SceneManager::update(float deltaTime, bool allowInteraction)
{
    if (m_activeScene)
        m_activeScene->update(deltaTime, allowInteraction);
}

heritage::math::Vec3 SceneManager::clearColor() const
{
    return m_activeScene
        ? m_activeScene->clearColor()
        : heritage::math::Vec3{ 0.0f, 0.0f, 0.0f };
}

void SceneManager::draw(
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings) const
{
    if (m_activeScene)
        m_activeScene->draw(projection, videoSettings);
}

void SceneManager::drawOverlay(
    int framebufferWidth,
    int framebufferHeight) const
{
    if (m_activeScene)
        m_activeScene->drawOverlay(framebufferWidth, framebufferHeight);
}

} // namespace heritage::scenes
