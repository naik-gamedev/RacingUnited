#pragma once

#include <string>

#include "../Math/Math.hpp"
#include "../Settings/VideoSettings.hpp"
#include "ModuleContext.hpp"
#include "ModuleRuntimeAction.hpp"
#include "ModuleRuntimeServices.hpp"

struct GLFWwindow;

namespace heritage::modules {

// Engine-facing lifecycle contract for one active module runtime.
//
// Native scenes, the lightweight module UI runtime, and future Lua modules all
// implement this same interface. The rest of Heritage Engine does not need to
// know which implementation is active.
class ModuleRuntime
{
public:
    virtual ~ModuleRuntime() = default;

    virtual bool onLoad(
        GLFWwindow* window,
        const ModuleContext& context,
        const ModuleRuntimeServices& services,
        std::string& message) = 0;

    virtual void onStart() = 0;
    virtual void onFixedUpdate(float fixedDeltaTime) { (void)fixedDeltaTime; }
    virtual void onUpdate(float deltaTime, bool allowInteraction) = 0;
    virtual heritage::math::Vec3 clearColor() const = 0;

    virtual void onRender(
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings) const = 0;

    // Non-const because module-owned buttons can change screens and queue
    // requests such as opening the permanent engine settings menu.
    virtual void onDrawUI(int framebufferWidth, int framebufferHeight) = 0;

    virtual void onShutdown() = 0;

    // Runtimes can request a small, controlled set of engine-level actions.
    // Modules do not receive direct access to main.cpp or WindowSystem.
    virtual bool pollAction(ModuleRuntimeAction&) { return false; }

    virtual const char* runtimeId() const = 0;
    virtual std::string activeContentId() const = 0;
};

} // namespace heritage::modules
