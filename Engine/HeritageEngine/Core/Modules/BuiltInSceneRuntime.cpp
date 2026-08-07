#include "BuiltInSceneRuntime.hpp"

namespace heritage::modules {

bool BuiltInSceneRuntime::onLoad(
    GLFWwindow* window,
    const ModuleContext& context,
    const ModuleRuntimeServices& services,
    std::string& message)
{
    onShutdown();

    // SceneManager installs a visible ErrorScene for unknown or failed scenes.
    // That is recoverable, so the module runtime itself remains active and can
    // explain the problem instead of crashing or borrowing another module.
    const bool sceneLoaded = m_sceneManager.load(
        context.module().scene,
        window,
        context,
        services.entities,
        message);

    m_loaded = true;
    m_started = false;
    return sceneLoaded || !m_sceneManager.activeSceneId().empty();
}

void BuiltInSceneRuntime::onStart()
{
    if (m_loaded)
        m_started = true;
}

void BuiltInSceneRuntime::onUpdate(float deltaTime, bool allowInteraction)
{
    if (m_started)
        m_sceneManager.update(deltaTime, allowInteraction);
}

heritage::math::Vec3 BuiltInSceneRuntime::clearColor() const
{
    return m_loaded
        ? m_sceneManager.clearColor()
        : heritage::math::Vec3{ 0.0f, 0.0f, 0.0f };
}

void BuiltInSceneRuntime::onRender(
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings) const
{
    if (m_started)
        m_sceneManager.draw(projection, videoSettings);
}

void BuiltInSceneRuntime::onDrawUI(
    int framebufferWidth,
    int framebufferHeight)
{
    if (m_started)
        m_sceneManager.drawOverlay(framebufferWidth, framebufferHeight);
}

void BuiltInSceneRuntime::onShutdown()
{
    if (m_loaded)
        m_sceneManager.shutdown();

    m_loaded = false;
    m_started = false;
}

std::string BuiltInSceneRuntime::activeContentId() const
{
    return m_sceneManager.activeSceneId();
}

} // namespace heritage::modules
