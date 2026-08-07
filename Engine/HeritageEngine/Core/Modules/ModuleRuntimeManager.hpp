#pragma once

#include <memory>
#include <string>

#include "ModuleRuntime.hpp"

namespace heritage::modules {

// Owns the active module runtime and enforces a predictable lifecycle:
// onLoad -> onStart -> onUpdate/onRender/onDrawUI -> onShutdown.
class ModuleRuntimeManager
{
public:
    bool initialize(
        GLFWwindow* window,
        const ModuleContext& context,
        const ModuleRuntimeServices& services,
        std::string& message);

    void shutdown();
    void fixedUpdate(float fixedDeltaTime);
    void update(float deltaTime, bool allowInteraction);

    heritage::math::Vec3 clearColor() const;

    void render(
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings) const;

    void drawUI(int framebufferWidth, int framebufferHeight);
    bool pollAction(ModuleRuntimeAction& action);

    bool isRunning() const { return m_started && m_runtime != nullptr; }
    std::string runtimeId() const;
    std::string activeContentId() const;

private:
    std::unique_ptr<ModuleRuntime> createRuntime(
        const ModuleContext& context,
        std::string& errorMessage) const;

    std::unique_ptr<ModuleRuntime> m_runtime;
    bool m_started = false;
};

} // namespace heritage::modules
