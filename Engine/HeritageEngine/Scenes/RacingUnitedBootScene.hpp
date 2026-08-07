#pragma once

#include "Scene.hpp"

namespace heritage::scenes {

class RacingUnitedBootScene final : public Scene
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

    void drawOverlay(int framebufferWidth, int framebufferHeight) const override;

private:
    std::string m_moduleName = "Racing United";
    float m_elapsedTime = 0.0f;
};

} // namespace heritage::scenes
