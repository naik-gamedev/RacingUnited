#pragma once

#include <string>
#include "../../Core/Math/Math.hpp"
#include "../Mesh.hpp"

namespace heritage::graphics {

// Draws the current engine scene. At this stage the scene consists of the
// Heritage Engine logo, but the class deliberately owns only scene drawing:
// it does not own anti-aliasing, scaling, framebuffers, display spanning,
// window management, or post-processing coordination.
class SceneRenderer
{
public:
    bool initialize(const std::string& logoMeshPath);
    void shutdown();

    void draw(const heritage::math::Mat4& model,
              const heritage::math::Mat4& view,
              const heritage::math::Mat4& projection,
              const heritage::math::Vec3& eyePosition,
              float gamma,
              float brightness,
              float contrast,
              float saturation) const;

private:
    GLuint m_program = 0;
    Mesh m_logo;
    GLint m_uniformModel = -1;
    GLint m_uniformView = -1;
    GLint m_uniformProjection = -1;
    GLint m_uniformLightPosition = -1;
    GLint m_uniformViewPosition = -1;
    GLint m_uniformColor = -1;
    GLint m_uniformGamma = -1;
    GLint m_uniformBrightness = -1;
    GLint m_uniformContrast = -1;
    GLint m_uniformSaturation = -1;
};

} // namespace heritage::graphics
