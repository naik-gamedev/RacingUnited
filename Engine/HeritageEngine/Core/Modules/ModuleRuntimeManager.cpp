#include "ModuleRuntimeManager.hpp"

#include "BuiltInSceneRuntime.hpp"
#include "LuaModuleRuntime.hpp"
#include "ScriptedUiRuntime.hpp"

namespace heritage::modules {

bool ModuleRuntimeManager::initialize(
    GLFWwindow* window,
    const ModuleContext& context,
    const ModuleRuntimeServices& services,
    std::string& message)
{
    shutdown();

    std::string creationError;
    m_runtime = createRuntime(context, creationError);
    if (!m_runtime)
    {
        message = creationError.empty()
            ? "Heritage Engine could not create the requested module runtime."
            : creationError;
        return false;
    }

    std::string loadMessage;
    if (!m_runtime->onLoad(window, context, services, loadMessage))
    {
        message = loadMessage.empty()
            ? "Module runtime '" + std::string(m_runtime->runtimeId())
                + "' failed during onLoad."
            : loadMessage;
        m_runtime->onShutdown();
        m_runtime.reset();
        return false;
    }

    m_runtime->onStart();
    m_started = true;

    // A built-in scene can recover by displaying an ErrorScene. Preserve that
    // message so main.cpp can log it while the runtime remains usable.
    message = loadMessage;
    return true;
}

void ModuleRuntimeManager::shutdown()
{
    if (m_runtime)
        m_runtime->onShutdown();

    m_runtime.reset();
    m_started = false;
}

void ModuleRuntimeManager::fixedUpdate(float fixedDeltaTime)
{
    if (m_started && m_runtime)
        m_runtime->onFixedUpdate(fixedDeltaTime);
}

void ModuleRuntimeManager::update(float deltaTime, bool allowInteraction)
{
    if (m_started && m_runtime)
        m_runtime->onUpdate(deltaTime, allowInteraction);
}

heritage::math::Vec3 ModuleRuntimeManager::clearColor() const
{
    return (m_started && m_runtime)
        ? m_runtime->clearColor()
        : heritage::math::Vec3{ 0.0f, 0.0f, 0.0f };
}

void ModuleRuntimeManager::render(
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings) const
{
    if (m_started && m_runtime)
        m_runtime->onRender(projection, videoSettings);
}

void ModuleRuntimeManager::drawUI(
    int framebufferWidth,
    int framebufferHeight)
{
    if (m_started && m_runtime)
        m_runtime->onDrawUI(framebufferWidth, framebufferHeight);
}

bool ModuleRuntimeManager::pollAction(ModuleRuntimeAction& action)
{
    return m_started && m_runtime && m_runtime->pollAction(action);
}

std::string ModuleRuntimeManager::runtimeId() const
{
    return m_runtime ? m_runtime->runtimeId() : "<none>";
}

std::string ModuleRuntimeManager::activeContentId() const
{
    return m_runtime ? m_runtime->activeContentId() : "<none>";
}

std::unique_ptr<ModuleRuntime> ModuleRuntimeManager::createRuntime(
    const ModuleContext& context,
    std::string& errorMessage) const
{
    const std::string runtime = context.module().runtime.empty()
        ? "builtin_scene"
        : context.module().runtime;

    if (runtime == "builtin_scene")
    {
        errorMessage.clear();
        return std::make_unique<BuiltInSceneRuntime>();
    }

    if (runtime == "scripted_ui")
    {
        errorMessage.clear();
        return std::make_unique<ScriptedUiRuntime>();
    }

    if (runtime == "lua")
    {
        errorMessage.clear();
        return std::make_unique<LuaModuleRuntime>();
    }

    errorMessage = "Module '" + context.module().id
        + "' requested unsupported runtime '" + runtime + "'.\n\n"
        + "Supported today: builtin_scene, scripted_ui, lua";
    return {};
}

} // namespace heritage::modules
