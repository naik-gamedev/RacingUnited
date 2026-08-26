#include "SkyRenderer.hpp"
#include "SkyRendererShaders.hpp"

#include "../ShaderProgram.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::graphics {
using namespace sky_renderer_shaders;
namespace {

constexpr int kTransmittanceWidth = 256;
constexpr int kTransmittanceHeight = 64;
constexpr int kMultiScatteringWidth = 32;
constexpr int kMultiScatteringHeight = 32;
constexpr int kSkyViewWidth = 256;
constexpr int kSkyViewHeight = 144;

bool programLinked(GLuint program)
{
    if (!program)
        return false;
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    return linked == GL_TRUE;
}

void allocateLutTexture(GLuint& texture, int width, int height, GLint wrapS)
{
    if (!texture)
        glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace

bool SkyRenderer::initializePhysicallyBasedAtmosphere()
{
    shutdownPhysicallyBasedAtmosphere();

    m_pbrAtmosphereTransmittanceProgram = buildShaderProgram(
        kFullscreenVertexShader,
        kPbrAtmosphereTransmittanceFragmentShader);
    m_pbrAtmosphereMultiScatteringProgram = buildShaderProgram(
        kFullscreenVertexShader,
        kPbrAtmosphereMultiScatteringFragmentShader);
    m_pbrAtmosphereSkyViewProgram = buildShaderProgram(
        kFullscreenVertexShader,
        kPbrAtmosphereSkyViewFragmentShader);

    // PERF05 rule applies here as well: link status is queried exactly once at
    // initialization and never from a frame hot path.
    m_pbrAtmosphereProgramsLinked =
        programLinked(m_pbrAtmosphereTransmittanceProgram)
        && programLinked(m_pbrAtmosphereMultiScatteringProgram)
        && programLinked(m_pbrAtmosphereSkyViewProgram);
    if (!m_pbrAtmosphereProgramsLinked)
        return false;

    glGenFramebuffers(1, &m_pbrAtmosphereFbo);
    allocateLutTexture(m_pbrTransmittanceTexture, kTransmittanceWidth, kTransmittanceHeight, GL_CLAMP_TO_EDGE);
    allocateLutTexture(m_pbrMultiScatteringTexture, kMultiScatteringWidth, kMultiScatteringHeight, GL_CLAMP_TO_EDGE);
    allocateLutTexture(m_pbrSkyViewTexture, kSkyViewWidth, kSkyViewHeight, GL_REPEAT);

    glBindFramebuffer(GL_FRAMEBUFFER, m_pbrAtmosphereFbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        m_pbrSkyViewTexture,
        0);
    const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &drawBuffer);
    const bool framebufferReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!framebufferReady)
    {
        shutdownPhysicallyBasedAtmosphere();
        return false;
    }

    m_pbrTransmittance.aerosolMultiplier = glGetUniformLocation(
        m_pbrAtmosphereTransmittanceProgram,
        "uAerosolMultiplier");

    m_pbrMultiScattering.transmittance = glGetUniformLocation(
        m_pbrAtmosphereMultiScatteringProgram,
        "uTransmittanceLut");
    m_pbrMultiScattering.aerosolMultiplier = glGetUniformLocation(
        m_pbrAtmosphereMultiScatteringProgram,
        "uAerosolMultiplier");
    glUseProgram(m_pbrAtmosphereMultiScatteringProgram);
    glUniform1i(m_pbrMultiScattering.transmittance, 0);

    m_pbrSkyView.transmittance = glGetUniformLocation(
        m_pbrAtmosphereSkyViewProgram,
        "uTransmittanceLut");
    m_pbrSkyView.multiScattering = glGetUniformLocation(
        m_pbrAtmosphereSkyViewProgram,
        "uMultiScatteringLut");
    m_pbrSkyView.sunDirection = glGetUniformLocation(
        m_pbrAtmosphereSkyViewProgram,
        "uSunDirection");
    m_pbrSkyView.sunColor = glGetUniformLocation(
        m_pbrAtmosphereSkyViewProgram,
        "uSunColor");
    m_pbrSkyView.sunIntensity = glGetUniformLocation(
        m_pbrAtmosphereSkyViewProgram,
        "uSunIntensity");
    m_pbrSkyView.cameraAltitude = glGetUniformLocation(
        m_pbrAtmosphereSkyViewProgram,
        "uCameraAltitudeM");
    m_pbrSkyView.aerosolMultiplier = glGetUniformLocation(
        m_pbrAtmosphereSkyViewProgram,
        "uAerosolMultiplier");
    glUseProgram(m_pbrAtmosphereSkyViewProgram);
    glUniform1i(m_pbrSkyView.transmittance, 0);
    glUniform1i(m_pbrSkyView.multiScattering, 1);
    glUseProgram(0);

    m_pbrLastAerosolMultiplier = -1.0f;
    m_pbrAtmosphereReady = true;
    return true;
}

void SkyRenderer::shutdownPhysicallyBasedAtmosphere()
{
    const GLuint programs[] = {
        m_pbrAtmosphereTransmittanceProgram,
        m_pbrAtmosphereMultiScatteringProgram,
        m_pbrAtmosphereSkyViewProgram
    };
    for (GLuint program : programs)
    {
        if (program)
            glDeleteProgram(program);
    }
    m_pbrAtmosphereTransmittanceProgram = 0;
    m_pbrAtmosphereMultiScatteringProgram = 0;
    m_pbrAtmosphereSkyViewProgram = 0;

    const GLuint textures[] = {
        m_pbrTransmittanceTexture,
        m_pbrMultiScatteringTexture,
        m_pbrSkyViewTexture
    };
    glDeleteTextures(3, textures);
    m_pbrTransmittanceTexture = 0;
    m_pbrMultiScatteringTexture = 0;
    m_pbrSkyViewTexture = 0;

    if (m_pbrAtmosphereFbo)
        glDeleteFramebuffers(1, &m_pbrAtmosphereFbo);
    m_pbrAtmosphereFbo = 0;
    m_pbrAtmosphereProgramsLinked = false;
    m_pbrAtmosphereReady = false;
    m_pbrLastAerosolMultiplier = -1.0f;
}

bool SkyRenderer::ensurePhysicallyBasedAtmosphereTargets()
{
    return m_pbrAtmosphereReady
        && m_pbrAtmosphereProgramsLinked
        && m_pbrAtmosphereFbo
        && m_pbrTransmittanceTexture
        && m_pbrMultiScatteringTexture
        && m_pbrSkyViewTexture;
}

void SkyRenderer::updatePhysicallyBasedAtmosphereLuts(
    const EnvironmentLighting& lighting,
    const SkyWeatherParameters& weather)
{
    if (!ensurePhysicallyBasedAtmosphereTargets())
        return;

    const float rain01 = std::clamp(weather.precipitationRateMmPerHour / 80.0f, 0.0f, 1.0f);
    const float humidity01 = std::clamp(weather.relativeHumidity, 0.0f, 1.0f);
    // The upstream Earth preset uses 10e-6 /m aerosol extinction. Heritage's
    // regional weather modulates only that aerosol amount; molecular air and
    // ozone remain immutable physical atmosphere coefficients.
    const float aerosolMultiplier = std::clamp(
        0.82f + humidity01 * 0.42f + rain01 * 0.36f,
        0.65f,
        1.60f);

    glBindVertexArray(m_vao);
    glBindFramebuffer(GL_FRAMEBUFFER, m_pbrAtmosphereFbo);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    // Transmittance and multiple-scattering depend on atmospheric composition,
    // not Sun azimuth. Refresh them only when weather materially changes.
    if (m_pbrLastAerosolMultiplier < 0.0f
        || std::abs(aerosolMultiplier - m_pbrLastAerosolMultiplier) > 0.01f)
    {
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            m_pbrTransmittanceTexture,
            0);
        glViewport(0, 0, kTransmittanceWidth, kTransmittanceHeight);
        glUseProgram(m_pbrAtmosphereTransmittanceProgram);
        glUniform1f(m_pbrTransmittance.aerosolMultiplier, aerosolMultiplier);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            m_pbrMultiScatteringTexture,
            0);
        glViewport(0, 0, kMultiScatteringWidth, kMultiScatteringHeight);
        glUseProgram(m_pbrAtmosphereMultiScatteringProgram);
        glUniform1f(m_pbrMultiScattering.aerosolMultiplier, aerosolMultiplier);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_pbrTransmittanceTexture);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        m_pbrLastAerosolMultiplier = aerosolMultiplier;
    }

    // The sky-view LUT is deliberately tiny (256x144, matching the upstream
    // project) and follows Heritage's continuously moving astronomical Sun.
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        m_pbrSkyViewTexture,
        0);
    glViewport(0, 0, kSkyViewWidth, kSkyViewHeight);
    glUseProgram(m_pbrAtmosphereSkyViewProgram);
    glUniform3f(
        m_pbrSkyView.sunDirection,
        lighting.sunDirection.x,
        lighting.sunDirection.y,
        lighting.sunDirection.z);
    glUniform3f(
        m_pbrSkyView.sunColor,
        lighting.sunColor.x,
        lighting.sunColor.y,
        lighting.sunColor.z);
    glUniform1f(m_pbrSkyView.sunIntensity, lighting.sunIntensity);
    glUniform1f(
        m_pbrSkyView.cameraAltitude,
        std::clamp(static_cast<float>(weather.cameraGlobal.y), 0.0f, 59990.0f));
    glUniform1f(m_pbrSkyView.aerosolMultiplier, aerosolMultiplier);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_pbrTransmittanceTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_pbrMultiScatteringTexture);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(0);
}

} // namespace heritage::graphics
