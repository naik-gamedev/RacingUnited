#pragma once

#include <glad/glad.h>

#include "../../Core/Math/Math.hpp"
#include "../EnvironmentMap.hpp"

namespace heritage::graphics {

class SkyRenderer
{
public:
    bool initialize();
    void shutdown();

    void draw(
        const heritage::math::Mat4& view,
        const heritage::math::Mat4& projection,
        const EnvironmentMap& environmentMap,
        float gamma,
        float brightness,
        float contrast,
        float saturation) const;

    bool valid() const { return m_program != 0 && m_vao != 0; }

private:
    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
};

} // namespace heritage::graphics
