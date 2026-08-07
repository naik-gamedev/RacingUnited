#pragma once

#include "ModuleRuntime.hpp"
#include "../../Scenes/SceneManager.hpp"

namespace heritage::modules {

// Current native runtime used by the built-in technology-demo scenes.
// It adapts SceneManager to the generic ModuleRuntime lifecycle.
class BuiltInSceneRuntime final : public ModuleRuntime
{
public:
    bool onLoad(
        GLFWwindow* window,
        const ModuleContext& context,
        const ModuleRuntimeServices& services,
        std::string& message) override;

    void onStart() override;
    void onUpdate(float deltaTime, bool allowInteraction) override;
    heritage::math::Vec3 clearColor() const override;

    void onRender(
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings) const override;

    void onDrawUI(int framebufferWidth, int framebufferHeight) override;
    void onShutdown() override;

    const char* runtimeId() const override { return "builtin_scene"; }
    std::string activeContentId() const override;

private:
    heritage::scenes::SceneManager m_sceneManager;
    bool m_loaded = false;
    bool m_started = false;
};

} // namespace heritage::modules
