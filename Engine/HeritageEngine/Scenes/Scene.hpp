#pragma once

#include <string>

#include "../Core/Math/Math.hpp"
#include "../Core/Modules/ModuleContext.hpp"
#include "../Core/Settings/VideoSettings.hpp"

struct GLFWwindow;

namespace heritage::scenes {

// Stable engine-facing interface for one running module scene. Built-in scenes
// implement it today; a later script runtime can provide another implementation
// without changing the renderer, settings or launcher.
class Scene
{
public:
    virtual ~Scene() = default;

    virtual bool initialize(
        GLFWwindow* window,
        const heritage::modules::ModuleContext& moduleContext,
        std::string& errorMessage) = 0;

    virtual void shutdown() = 0;
    virtual void update(float deltaTime, bool allowInteraction) = 0;

    virtual heritage::math::Vec3 clearColor() const = 0;

    virtual void draw(
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings) const = 0;

    // Called after ImGui::NewFrame and before the permanent engine UI.
    virtual void drawOverlay(int framebufferWidth, int framebufferHeight) const {}
};

} // namespace heritage::scenes
