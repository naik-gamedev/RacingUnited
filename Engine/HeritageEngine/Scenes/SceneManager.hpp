#pragma once

#include <memory>
#include <string>

#include "Scene.hpp"

namespace heritage::entities {
class EntityRegistry;
}

namespace heritage::scenes {

class SceneManager
{
public:
    bool load(
        const std::string& entryScene,
        GLFWwindow* window,
        const heritage::modules::ModuleContext& moduleContext,
        heritage::entities::EntityRegistry* entityRegistry,
        std::string& errorMessage);


    bool exists(
        const std::string& sceneId,
        const heritage::modules::ModuleContext& moduleContext) const;

    void shutdown();
    void update(float deltaTime, bool allowInteraction);

    heritage::math::Vec3 clearColor() const;

    void draw(
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings) const;

    void drawOverlay(int framebufferWidth, int framebufferHeight) const;

    const std::string& activeSceneId() const { return m_activeSceneId; }

private:
    std::unique_ptr<Scene> m_activeScene;
    std::string m_activeSceneId;
};

} // namespace heritage::scenes
