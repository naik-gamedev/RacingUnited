#pragma once

#include "Scene.hpp"
#include "../Camera/OrbitCamera.hpp"
#include "../Graphics/Renderer/SceneRenderer.hpp"

namespace heritage::scenes {

class LogoShowcaseScene final : public Scene
{
public:
    bool initialize(
        GLFWwindow* window,
        const heritage::modules::ModuleContext& moduleContext,
        std::string& errorMessage) override;

    void shutdown() override;
    void update(float deltaTime, bool allowInteraction) override;
    heritage::math::Vec3 clearColor() const override;

    void draw(
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings) const override;

private:
    heritage::graphics::SceneRenderer m_renderer;
    heritage::camera::OrbitCamera m_camera;
    bool m_initialized = false;
};

} // namespace heritage::scenes
