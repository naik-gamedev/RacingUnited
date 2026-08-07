#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Scene.hpp"
#include "../Core/Entities/EntitySceneDocument.hpp"

namespace heritage::scenes {

// Module-owned .hscene document. Empty documents still provide clear colour
// and optional overlay text; type=entities additionally instantiates a complete
// scene-owned entity hierarchy in the active module's EntityRegistry.
class ModuleDocumentScene final : public Scene
{
public:
    ModuleDocumentScene(
        std::string sceneId,
        std::filesystem::path documentPath,
        heritage::entities::EntityRegistry* entityRegistry);

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
    std::string m_sceneId;
    std::filesystem::path m_documentPath;
    heritage::entities::EntityRegistry* m_entityRegistry = nullptr;
    heritage::entities::EntitySceneDocumentInfo m_documentInfo;
    std::vector<heritage::entities::EntityHandle> m_sceneEntities;
};

} // namespace heritage::scenes
