#include "LogoShowcaseScene.hpp"

#include <filesystem>

namespace heritage::scenes {

bool LogoShowcaseScene::initialize(
    GLFWwindow* window,
    const heritage::modules::ModuleContext& moduleContext,
    std::string& errorMessage)
{
    shutdown();

    const std::filesystem::path meshPath =
        moduleContext.resolveModulePath(moduleContext.module().logoMesh);

    if (meshPath.empty())
    {
        errorMessage = "LogoShowcase contains an unsafe logo_mesh path: "
            + moduleContext.module().logoMesh;
        return false;
    }

    if (!std::filesystem::is_regular_file(meshPath))
    {
        errorMessage = "LogoShowcase could not find its module-owned mesh:\n"
            + meshPath.string()
            + "\n\nNo global Assets fallback was used.";
        return false;
    }

    if (!m_renderer.initialize(meshPath.string()))
    {
        errorMessage = "LogoShowcase failed to load its mesh:\n"
            + meshPath.string();
        return false;
    }

    m_camera.installCallbacks(window);
    m_initialized = true;
    errorMessage.clear();
    return true;
}

void LogoShowcaseScene::shutdown()
{
    if (m_initialized)
        m_camera.uninstallCallbacks();

    m_renderer.shutdown();
    m_initialized = false;
}

void LogoShowcaseScene::update(float deltaTime, bool allowInteraction)
{
    if (m_initialized)
        m_camera.update(deltaTime, allowInteraction);
}

heritage::math::Vec3 LogoShowcaseScene::clearColor() const
{
    return { 0.0f, 0.0f, 0.0f };
}

void LogoShowcaseScene::draw(
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings) const
{
    if (!m_initialized)
        return;

    m_renderer.draw(
        heritage::math::identity(),
        m_camera.viewMatrix(),
        projection,
        m_camera.eye(),
        videoSettings.gamma,
        videoSettings.brightness,
        videoSettings.contrast,
        videoSettings.saturation);
}

} // namespace heritage::scenes
