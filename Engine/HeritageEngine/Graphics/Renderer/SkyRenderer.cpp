#include "SkyRenderer.hpp"

#include "../ShaderProgram.hpp"

namespace heritage::graphics {
namespace {

#ifdef _WIN32
#define HERITAGE_SKY_GLSL_VERSION "#version 460 core\n"
#else
#define HERITAGE_SKY_GLSL_VERSION "#version 330 core\n"
#endif

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

const char* kSkyFragmentShader = HERITAGE_SKY_GLSL_VERSION R"glsl(
in vec3 vDirection;

uniform samplerCube uEnvironmentMap;
uniform float uGamma;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;

out vec4 FragColor;

void main()
{
    vec3 color = texture(uEnvironmentMap, normalize(vDirection)).rgb;

    // Mild filmic compression keeps the HDR sun disc from simply clipping to
    // a giant white patch while leaving the procedural sky vivid.
    color = color / (color + vec3(1.0));
    color = pow(
        clamp(color, 0.0, 1.0),
        vec3(1.0 / max(uGamma, 0.01)));
    color = (color - 0.5) * uContrast + 0.5 + uBrightness;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, uSaturation);

    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)glsl";

constexpr float kCubeVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

} // namespace

bool SkyRenderer::initialize()
{
    shutdown();

    m_program = buildShaderProgram(kSkyVertexShader, kSkyFragmentShader);
    if (!m_program)
        return false;

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(kCubeVertices),
        kCubeVertices,
        GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glUseProgram(m_program);
    glUniform1i(glGetUniformLocation(m_program, "uEnvironmentMap"), 0);
    return m_vao != 0 && m_vbo != 0;
}

void SkyRenderer::shutdown()
{
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_program)
        glDeleteProgram(m_program);
    m_vao = 0;
    m_vbo = 0;
    m_program = 0;
}

void SkyRenderer::draw(
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const EnvironmentMap& environmentMap,
    float gamma,
    float brightness,
    float contrast,
    float saturation) const
{
    if (!valid() || !environmentMap.valid())
        return;

    const GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean oldDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glUseProgram(m_program);
    glUniformMatrix4fv(
        glGetUniformLocation(m_program, "uView"),
        1,
        GL_FALSE,
        view.m);
    glUniformMatrix4fv(
        glGetUniformLocation(m_program, "uProjection"),
        1,
        GL_FALSE,
        projection.m);
    glUniform1f(glGetUniformLocation(m_program, "uGamma"), gamma);
    glUniform1f(glGetUniformLocation(m_program, "uBrightness"), brightness);
    glUniform1f(glGetUniformLocation(m_program, "uContrast"), contrast);
    glUniform1f(glGetUniformLocation(m_program, "uSaturation"), saturation);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap.textureId());
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    glDepthMask(oldDepthMask);
    glDepthFunc(GL_GREATER);
    if (!depthTestWasEnabled)
        glDisable(GL_DEPTH_TEST);
    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
}

} // namespace heritage::graphics
