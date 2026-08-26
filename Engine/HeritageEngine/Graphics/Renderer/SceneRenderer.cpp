#include <glad/glad.h>
#include "SceneRenderer.hpp"
#include "../ShaderProgram.hpp"

namespace heritage::graphics {

#ifdef _WIN32
#define HERITAGE_SCENE_GLSL_VERSION "#version 460 core\n"
#else
#define HERITAGE_SCENE_GLSL_VERSION "#version 330 core\n"
#endif

static const char* SCENE_VS = HERITAGE_SCENE_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uModel, uView, uProj;
out vec3 vNormal, vFragPos;
void main()
{
    vec4 wp  = uModel * vec4(aPos, 1.0);
    vFragPos = wp.xyz;
    vNormal  = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uProj * uView * wp;
}
)glsl";

static const char* SCENE_FS = HERITAGE_SCENE_GLSL_VERSION R"glsl(
in vec3 vNormal, vFragPos;
uniform vec3 uLightPos, uViewPos, uColor;
uniform float uGamma, uBrightness, uContrast, uSaturation;
out vec4 FragColor;
void main()
{
    vec3 norm     = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);
    vec3 viewDir  = normalize(uViewPos  - vFragPos);
    vec3 halfway  = normalize(lightDir + viewDir);
    float ambient = 0.18;
    float diff    = max(dot(norm, lightDir), 0.0);
    float spec    = pow(max(dot(norm, halfway), 0.0), 64.0) * 0.7;
    float rim     = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.0) * 0.25;
    vec3 col = uColor*(ambient+diff) + vec3(spec) + uColor*rim;
    col = pow(clamp(col,0.0,1.0), vec3(1.0 / max(uGamma, 0.01)));
    col = (col - 0.5) * uContrast + 0.5 + uBrightness;
    float luminance = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(vec3(luminance), col, uSaturation);
    FragColor = vec4(col, 1.0);
}
)glsl";

bool SceneRenderer::initialize(const std::string& logoMeshPath)
{
    shutdown();

    m_program = buildShaderProgram(SCENE_VS, SCENE_FS);
    if (!m_program)
        return false;

    m_uniformModel = glGetUniformLocation(m_program, "uModel");
    m_uniformView = glGetUniformLocation(m_program, "uView");
    m_uniformProjection = glGetUniformLocation(m_program, "uProj");
    m_uniformLightPosition = glGetUniformLocation(m_program, "uLightPos");
    m_uniformViewPosition = glGetUniformLocation(m_program, "uViewPos");
    m_uniformColor = glGetUniformLocation(m_program, "uColor");
    m_uniformGamma = glGetUniformLocation(m_program, "uGamma");
    m_uniformBrightness = glGetUniformLocation(m_program, "uBrightness");
    m_uniformContrast = glGetUniformLocation(m_program, "uContrast");
    m_uniformSaturation = glGetUniformLocation(m_program, "uSaturation");

    m_logo = loadObjMesh(logoMeshPath);
    uploadMesh(m_logo);
    return m_logo.vao != 0 && !m_logo.indices.empty();
}

void SceneRenderer::shutdown()
{
    if (m_logo.vao) { glDeleteVertexArrays(1, &m_logo.vao); m_logo.vao = 0; }
    if (m_logo.vbo) { glDeleteBuffers(1, &m_logo.vbo); m_logo.vbo = 0; }
    if (m_logo.ebo) { glDeleteBuffers(1, &m_logo.ebo); m_logo.ebo = 0; }
    m_logo.indices.clear();

    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_uniformModel = -1;
    m_uniformView = -1;
    m_uniformProjection = -1;
    m_uniformLightPosition = -1;
    m_uniformViewPosition = -1;
    m_uniformColor = -1;
    m_uniformGamma = -1;
    m_uniformBrightness = -1;
    m_uniformContrast = -1;
    m_uniformSaturation = -1;
}

void SceneRenderer::draw(const heritage::math::Mat4& model,
                         const heritage::math::Mat4& view,
                         const heritage::math::Mat4& projection,
                         const heritage::math::Vec3& eyePosition,
                         float gamma,
                         float brightness,
                         float contrast,
                         float saturation) const
{
    glUseProgram(m_program);
    glUniformMatrix4fv(m_uniformModel, 1, GL_FALSE, model.m);
    glUniformMatrix4fv(m_uniformView, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(m_uniformProjection, 1, GL_FALSE, projection.m);
    glUniform3f(m_uniformLightPosition, 4.0f, 6.0f, 5.0f);
    glUniform3f(m_uniformViewPosition, eyePosition.x, eyePosition.y, eyePosition.z);
    glUniform3f(m_uniformColor, 1.0f, 1.0f, 1.0f);
    glUniform1f(m_uniformGamma, gamma);
    glUniform1f(m_uniformBrightness, brightness);
    glUniform1f(m_uniformContrast, contrast);
    glUniform1f(m_uniformSaturation, saturation);

    glBindVertexArray(m_logo.vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_logo.indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

} // namespace heritage::graphics
