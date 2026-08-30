#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glad/glad.h>

#include "Authoring/StudioAuthoringData.hpp"
#include "../Graphics/Mesh.hpp"
#include "../Graphics/Texture2D.hpp"

namespace heritage::studio {

struct StudioPreviewCamera
{
    authoring::Vec3 position{};
    authoring::Vec3 target{};
    authoring::Vec3 right{ 1.0f, 0.0f, 0.0f };
    authoring::Vec3 up{ 0.0f, 1.0f, 0.0f };
    authoring::Vec3 forward{ 0.0f, 0.0f, -1.0f };
    float fovYRadians = 0.9f;
    float aspect = 1.0f;
    bool orthographic = false;
    float orthoHalfHeight = 10.0f;
};

class StudioScenePreview
{
public:
    StudioScenePreview() = default;
    ~StudioScenePreview();

    StudioScenePreview(const StudioScenePreview&) = delete;
    StudioScenePreview& operator=(const StudioScenePreview&) = delete;

    bool initialize(const std::filesystem::path& assetRoot, std::string& message);
    void shutdown();

    bool discoverAndLoadLatest(std::string& message);
    bool loadScene(const std::filesystem::path& absolutePath, std::string& message);
    bool reload(std::string& message);

    GLuint render(int width, int height, const StudioPreviewCamera& camera, bool gridVisible);

    bool raycast(const authoring::Vec3& origin, const authoring::Vec3& direction,
        authoring::Vec3& hitPosition) const;

    bool loaded() const { return m_loaded; }
    const std::filesystem::path& scenePath() const { return m_scenePath; }
    const std::string& status() const { return m_status; }
    std::size_t triangleCount() const { return m_sceneMesh.indices.size() / 3; }
    std::size_t materialCount() const { return m_sceneMesh.materials.size(); }
    authoring::Vec3 boundsCenter() const { return m_boundsCenter; }
    float boundsRadius() const { return m_boundsRadius; }

    void setVisible(bool value) { m_visible = value; }
    bool visible() const { return m_visible; }
    void setWireframe(bool value) { m_wireframe = value; }
    bool wireframe() const { return m_wireframe; }
    void setExposure(float value) { m_exposure = value; }
    float exposure() const { return m_exposure; }

private:
    struct Mat4
    {
        float m[16]{};
    };

    struct WorldTriangle
    {
        authoring::Vec3 a{};
        authoring::Vec3 b{};
        authoring::Vec3 c{};
    };

    bool ensureProgram(std::string& message);
    bool ensureGridProgram(std::string& message);
    bool ensureFramebuffer(int width, int height);
    void destroyFramebuffer();
    void rebuildNodeTransforms();
    void rebuildRaycastTriangles();
    bool hiddenAuthoringRange(const heritage::graphics::MeshDrawRange& range) const;

    const heritage::graphics::Texture2D* acquireTexture(
        const heritage::graphics::MaterialTextureReference& reference,
        heritage::graphics::TextureColorSpace colorSpace,
        std::string& error);

    static Mat4 identity();
    static Mat4 multiply(const Mat4& a, const Mat4& b);
    static Mat4 trs(const heritage::graphics::MeshNode& node);
    static Mat4 viewMatrix(const StudioPreviewCamera& camera);
    static Mat4 perspectiveMatrix(float fovY, float aspect, float zNear, float zFar);
    static Mat4 orthographicMatrix(float halfHeight, float aspect, float zNear, float zFar);
    static authoring::Vec3 transformPoint(const Mat4& matrix, const authoring::Vec3& point);
    static float determinant3x3(const Mat4& matrix);

    std::filesystem::path m_assetRoot;
    std::filesystem::path m_scenePath;
    std::string m_status = "No Scene_*.glb loaded.";

    heritage::graphics::Mesh m_sceneMesh;
    heritage::graphics::Texture2DCache m_textureCache;
    std::vector<Mat4> m_nodeGlobals;
    std::vector<WorldTriangle> m_raycastTriangles;
    authoring::Vec3 m_boundsCenter{};
    float m_boundsRadius = 1.0f;

    GLuint m_program = 0;
    GLuint m_gridProgram = 0;
    GLuint m_gridVao = 0;
    GLuint m_framebuffer = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthRenderbuffer = 0;
    int m_framebufferWidth = 0;
    int m_framebufferHeight = 0;

    bool m_loaded = false;
    bool m_visible = true;
    bool m_wireframe = false;
    float m_exposure = 1.0f;
};

} // namespace heritage::studio
