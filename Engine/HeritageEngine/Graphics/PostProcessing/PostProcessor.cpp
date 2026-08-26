#include "PostProcessor.hpp"
#include "../ShaderProgram.hpp"

namespace heritage::graphics {

#ifdef _WIN32
#define HERITAGE_POST_GLSL_VERSION "#version 460 core\n"
#else
#define HERITAGE_POST_GLSL_VERSION "#version 330 core\n"
#endif

static const char* QUAD_VS = HERITAGE_POST_GLSL_VERSION R"glsl(
out vec2 vUV;
void main()
{
    const vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0));
    vec2 pos = positions[gl_VertexID];
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)glsl";

static const char* FXAA_FS = HERITAGE_POST_GLSL_VERSION R"glsl(
in vec2 vUV;
uniform sampler2D uScene;
uniform vec2 uTexelSize;
out vec4 FragColor;
void main()
{
    vec3 rgbNW = texture(uScene, vUV + vec2(-1.0,-1.0)*uTexelSize).rgb;
    vec3 rgbNE = texture(uScene, vUV + vec2( 1.0,-1.0)*uTexelSize).rgb;
    vec3 rgbSW = texture(uScene, vUV + vec2(-1.0, 1.0)*uTexelSize).rgb;
    vec3 rgbSE = texture(uScene, vUV + vec2( 1.0, 1.0)*uTexelSize).rgb;
    vec3 rgbM  = texture(uScene, vUV).rgb;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    if (lumaRange < max(0.0833, lumaMax * 0.125))
    {
        FragColor = vec4(rgbM, 1.0);
        return;
    }

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW+lumaNE+lumaSW+lumaSE)*0.03125, 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -8.0, 8.0) * uTexelSize;

    vec3 rgbA = 0.5 * (
        texture(uScene, vUV + dir * (1.0/3.0 - 0.5)).rgb +
        texture(uScene, vUV + dir * (2.0/3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(uScene, vUV + dir * -0.5).rgb +
        texture(uScene, vUV + dir *  0.5).rgb);

    float lumaB = dot(rgbB, luma);
    if (lumaB < lumaMin || lumaB > lumaMax)
        FragColor = vec4(rgbA, 1.0);
    else
        FragColor = vec4(rgbB, 1.0);
}
)glsl";

static const char* BLIT_FS = HERITAGE_POST_GLSL_VERSION R"glsl(
in vec2 vUV;
uniform sampler2D uScene;
uniform bool uNearestNeighbour;
out vec4 FragColor;
void main()
{
    FragColor = texture(uScene, vUV);
}
)glsl";

bool PostProcessor::initialize()
{
    shutdown();

    m_fxaaProgram = buildShaderProgram(QUAD_VS, FXAA_FS);
    m_blitProgram = buildShaderProgram(QUAD_VS, BLIT_FS);
    glGenVertexArrays(1, &m_fullscreenVao);

    if (m_fxaaProgram)
    {
        m_fxaaUniformScene = glGetUniformLocation(m_fxaaProgram, "uScene");
        m_fxaaUniformTexelSize = glGetUniformLocation(m_fxaaProgram, "uTexelSize");
    }
    if (m_blitProgram)
    {
        m_blitUniformScene = glGetUniformLocation(m_blitProgram, "uScene");
        m_blitUniformNearestNeighbour =
            glGetUniformLocation(m_blitProgram, "uNearestNeighbour");
    }

    return m_fxaaProgram != 0 && m_blitProgram != 0 && m_fullscreenVao != 0;
}

void PostProcessor::shutdown()
{
    if (m_fullscreenVao) {
        glDeleteVertexArrays(1, &m_fullscreenVao);
        m_fullscreenVao = 0;
    }
    if (m_fxaaProgram) {
        glDeleteProgram(m_fxaaProgram);
        m_fxaaProgram = 0;
    }
    if (m_blitProgram) {
        glDeleteProgram(m_blitProgram);
        m_blitProgram = 0;
    }
    m_fxaaUniformScene = -1;
    m_fxaaUniformTexelSize = -1;
    m_blitUniformScene = -1;
    m_blitUniformNearestNeighbour = -1;
}

void PostProcessor::applyFxaa(GLuint sceneTexture, int sourceWidth, int sourceHeight,
                              GLuint destinationFramebuffer, int destinationWidth, int destinationHeight) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, destinationFramebuffer);
    glViewport(0, 0, destinationWidth, destinationHeight);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(m_fxaaProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    glUniform1i(m_fxaaUniformScene, 0);
    glUniform2f(m_fxaaUniformTexelSize,
                1.0f / static_cast<float>(sourceWidth),
                1.0f / static_cast<float>(sourceHeight));
    glBindVertexArray(m_fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void PostProcessor::blit(GLuint sceneTexture, GLuint destinationFramebuffer,
                         int destinationWidth, int destinationHeight, bool nearestNeighbour) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, destinationFramebuffer);
    glViewport(0, 0, destinationWidth, destinationHeight);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(m_blitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);

    const GLenum filter = nearestNeighbour ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    glUniform1i(m_blitUniformScene, 0);
    glUniform1i(m_blitUniformNearestNeighbour, nearestNeighbour ? 1 : 0);
    glBindVertexArray(m_fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

} // namespace heritage::graphics
