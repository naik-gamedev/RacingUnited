#include "SkyRendererShaders.hpp"

namespace heritage::graphics::sky_renderer_shaders {

#define HERITAGE_SKY_GLSL_VERSION "#version 460 core\n"
const char* kSkyVertexShader = HERITAGE_SKY_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aPos;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vDirection;

void main()
{
    mat4 rotationOnlyView = mat4(mat3(uView));
    vec4 clip = uProjection * rotationOnlyView * vec4(aPos, 1.0);
    gl_Position = vec4(clip.xy, -clip.w, clip.w);
    vDirection = aPos;
}
)glsl";

} // namespace heritage::graphics::sky_renderer_shaders
