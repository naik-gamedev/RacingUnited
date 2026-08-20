#include "SkyRenderer.hpp"

#include "../ShaderProgram.hpp"

#include <algorithm>
#include <cmath>

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

// WEATHER06A: clouds are integrated at one-third view resolution and then
// bilinearly reconstructed over the normal procedural sky. Density is a
// deterministic world-space field advected by weather wind; camera movement
// changes the sampled rays rather than translating the cloud volume.
const char* kCloudVertexShader = HERITAGE_SKY_GLSL_VERSION R"glsl(
out vec2 vUv;
void main()
{
    vec2 p;
    if (gl_VertexID == 0) p = vec2(-1.0, -1.0);
    else if (gl_VertexID == 1) p = vec2(3.0, -1.0);
    else p = vec2(-1.0, 3.0);
    vUv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
)glsl";

const char* kCloudFragmentShader = HERITAGE_SKY_GLSL_VERSION R"glsl(
in vec2 vUv;
out vec4 FragColor;

uniform mat4 uView;
uniform vec2 uProjectionScale;
uniform vec2 uCameraGlobalXZ;
uniform float uCameraGlobalY;
uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform float uDaylight;
uniform float uCloudCover;
uniform float uHumidity;
uniform float uPrecipitationMmPerHour;
uniform vec2 uWindVelocityXZ;
uniform float uTime;

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.56;
    mat2 rotation = mat2(0.80, -0.60, 0.60, 0.80);
    for (int octave = 0; octave < 4; ++octave)
    {
        value += valueNoise(p) * amplitude;
        p = rotation * p * 2.03 + vec2(13.7, 7.9);
        amplitude *= 0.48;
    }
    return value;
}

float cloudDensity(vec3 worldPosition, float cloudBase, float cloudTop,
    vec2 windOffset, float cover, float storm)
{
    float span = max(cloudTop - cloudBase, 1.0);
    float h = clamp((worldPosition.y - cloudBase) / span, 0.0, 1.0);
    float vertical = smoothstep(0.0, 0.16, h)
        * (1.0 - smoothstep(0.68, 1.0, h));

    vec2 q = worldPosition.xz * 0.00042
        + vec2(worldPosition.y * 0.000055, -worldPosition.y * 0.000041)
        + windOffset;
    float macro = fbm(q);
    float detail = fbm(q * 3.1 + vec2(19.4, -8.7));
    float structure = macro * 0.78 + detail * 0.22;
    // WEATHER06B: bias coverage toward clearly readable cloud bodies. The
    // original threshold produced mostly transparent volume on the live scene,
    // even while storm lighting correctly darkened the sky.
    float threshold = mix(0.74, 0.24, cover);
    threshold -= storm * 0.085;
    return smoothstep(threshold - 0.08, threshold + 0.12, structure)
        * vertical;
}

void main()
{
    float cover = clamp(uCloudCover, 0.0, 1.0);
    float rain = clamp(uPrecipitationMmPerHour / 80.0, 0.0, 1.0);
    float storm = clamp(max(rain, cover * rain * 1.25), 0.0, 1.0);
    if (cover <= 0.01 && rain <= 0.001)
    {
        FragColor = vec4(0.0);
        return;
    }

    vec2 ndc = vUv * 2.0 - 1.0;
    vec3 rayView = normalize(vec3(
        ndc.x / max(uProjectionScale.x, 0.0001),
        ndc.y / max(uProjectionScale.y, 0.0001),
        -1.0));
    vec3 rayDirection = normalize(transpose(mat3(uView)) * rayView);

    // The first weather authoring tier uses a world-altitude cloud slab. Its
    // base lowers naturally with humidity/storm intensity; a later scene datum
    // can replace these defaults without changing the renderer contract.
    float cloudBase = mix(1550.0, 620.0, clamp(uHumidity, 0.0, 1.0))
        - storm * 240.0;
    float cloudTop = cloudBase + mix(950.0, 2600.0,
        clamp(max(cover, storm), 0.0, 1.0));

    float startDistance = 0.0;
    float endDistance = 0.0;
    if (abs(rayDirection.y) > 0.0001)
    {
        float t0 = (cloudBase - uCameraGlobalY) / rayDirection.y;
        float t1 = (cloudTop - uCameraGlobalY) / rayDirection.y;
        startDistance = min(t0, t1);
        endDistance = max(t0, t1);
    }

    startDistance = max(startDistance, 0.0);
    endDistance = min(endDistance, 65000.0);
    vec3 accumulated = vec3(0.0);
    float transmittance = 1.0;

    if (endDistance > startDistance + 1.0)
    {
        const int kSteps = 10;
        float stepLength = (endDistance - startDistance) / float(kSteps);
        float jitter = hash21(gl_FragCoord.xy) - 0.5;
        float t = startDistance + stepLength * (0.5 + jitter * 0.35);

        // WEATHER07A: cloud advection consumes the same explicit world wind
        // vector as precipitation instead of inventing a renderer-only heading.
        vec2 windOffset = uWindVelocityXZ * uTime * 0.00042;

        for (int stepIndex = 0; stepIndex < kSteps; ++stepIndex)
        {
            vec2 worldXZ = uCameraGlobalXZ + rayDirection.xz * t;
            vec3 worldPosition = vec3(
                worldXZ.x,
                uCameraGlobalY + rayDirection.y * t,
                worldXZ.y);

            float density = cloudDensity(
                worldPosition, cloudBase, cloudTop, windOffset, cover, storm);
            if (density > 0.002)
            {
                float sunFacing = clamp(dot(rayDirection, uSunDirection), 0.0, 1.0);
                float silver = pow(sunFacing, 10.0) * (1.0 - storm * 0.65);
                float daylight = clamp(uDaylight, 0.0, 1.0);
                vec3 brightCloud = mix(vec3(0.08, 0.095, 0.12),
                    vec3(0.82, 0.86, 0.90), daylight);
                vec3 stormCloud = mix(vec3(0.025, 0.030, 0.040),
                    vec3(0.20, 0.23, 0.27), daylight);
                vec3 cloudColor = mix(brightCloud, stormCloud,
                    clamp(storm * 0.88 + density * storm * 0.25, 0.0, 1.0));
                cloudColor += uSunColor * silver * uSunIntensity * 0.12;

                float extinction = density * stepLength
                    * mix(0.00082, 0.00145, storm);
                float sampleAlpha = 1.0 - exp(-extinction);
                accumulated += transmittance * cloudColor * sampleAlpha;
                transmittance *= (1.0 - sampleAlpha);
                if (transmittance < 0.025)
                    break;
            }
            t += stepLength;
        }
    }

    float alpha = 1.0 - transmittance;

    // Far precipitation becomes a soft rain curtain at the horizon rather than
    // requiring millions of streak particles. Near/mid streaks are handled by
    // WeatherPresentationRenderer after scene geometry.
    if (rain > 0.001 && rayDirection.y > -0.05)
    {
        float horizon = 1.0 - smoothstep(0.03, 0.44, max(rayDirection.y, 0.0));
        vec2 curtainCoord = uCameraGlobalXZ * 0.00028
            + rayDirection.xz * 8.0
            + uWindVelocityXZ * uTime * 0.00018;
        float curtainNoise = mix(0.58, 1.0, fbm(curtainCoord));
        float veilAlpha = rain * horizon * curtainNoise * 0.24;
        vec3 veilColor = mix(vec3(0.07, 0.085, 0.10),
            vec3(0.36, 0.40, 0.44), clamp(uDaylight, 0.0, 1.0));
        accumulated += transmittance * veilColor * veilAlpha;
        alpha += transmittance * veilAlpha;
        alpha = clamp(alpha, 0.0, 0.98);
    }

    FragColor = vec4(accumulated, alpha);
}
)glsl";

const char* kCloudCompositeFragmentShader = HERITAGE_SKY_GLSL_VERSION R"glsl(
in vec2 vUv;
uniform sampler2D uCloudTexture;
out vec4 FragColor;
void main()
{
    FragColor = texture(uCloudTexture, vUv);
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

float wrappedWorldCoordinate(double value)
{
    constexpr double kWrap = 65536.0;
    double wrapped = std::fmod(value, kWrap);
    if (wrapped < 0.0)
        wrapped += kWrap;
    return static_cast<float>(wrapped);
}

} // namespace

bool SkyRenderer::initialize()
{
    shutdown();

    m_program = buildShaderProgram(kSkyVertexShader, kSkyFragmentShader);
    m_cloudProgram = buildShaderProgram(kCloudVertexShader, kCloudFragmentShader);
    m_cloudCompositeProgram = buildShaderProgram(
        kCloudVertexShader, kCloudCompositeFragmentShader);
    if (!m_program || !m_cloudProgram || !m_cloudCompositeProgram)
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
    glUseProgram(m_cloudCompositeProgram);
    glUniform1i(glGetUniformLocation(m_cloudCompositeProgram, "uCloudTexture"), 0);

    m_cloudUniformView = glGetUniformLocation(m_cloudProgram, "uView");
    m_cloudUniformProjectionScale = glGetUniformLocation(
        m_cloudProgram, "uProjectionScale");
    m_cloudUniformCameraGlobalXZ = glGetUniformLocation(
        m_cloudProgram, "uCameraGlobalXZ");
    m_cloudUniformCameraGlobalY = glGetUniformLocation(
        m_cloudProgram, "uCameraGlobalY");
    m_cloudUniformSunDirection = glGetUniformLocation(
        m_cloudProgram, "uSunDirection");
    m_cloudUniformSunColor = glGetUniformLocation(m_cloudProgram, "uSunColor");
    m_cloudUniformSunIntensity = glGetUniformLocation(
        m_cloudProgram, "uSunIntensity");
    m_cloudUniformDaylight = glGetUniformLocation(m_cloudProgram, "uDaylight");
    m_cloudUniformCloudCover = glGetUniformLocation(
        m_cloudProgram, "uCloudCover");
    m_cloudUniformHumidity = glGetUniformLocation(m_cloudProgram, "uHumidity");
    m_cloudUniformPrecipitation = glGetUniformLocation(
        m_cloudProgram, "uPrecipitationMmPerHour");
    m_cloudUniformWindVelocityXZ = glGetUniformLocation(
        m_cloudProgram, "uWindVelocityXZ");
    m_cloudUniformTime = glGetUniformLocation(m_cloudProgram, "uTime");

    return m_vao != 0 && m_vbo != 0;
}

void SkyRenderer::shutdown()
{
    if (m_cloudFbo)
        glDeleteFramebuffers(1, &m_cloudFbo);
    if (m_cloudTexture)
        glDeleteTextures(1, &m_cloudTexture);
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_program)
        glDeleteProgram(m_program);
    if (m_cloudProgram)
        glDeleteProgram(m_cloudProgram);
    if (m_cloudCompositeProgram)
        glDeleteProgram(m_cloudCompositeProgram);
    m_cloudFbo = 0;
    m_cloudTexture = 0;
    m_cloudWidth = 0;
    m_cloudHeight = 0;
    m_vao = 0;
    m_vbo = 0;
    m_program = 0;
    m_cloudProgram = 0;
    m_cloudCompositeProgram = 0;
}

bool SkyRenderer::ensureCloudTarget(int viewportWidth, int viewportHeight)
{
    const int width = std::max(160, viewportWidth / 3);
    const int height = std::max(90, viewportHeight / 3);
    if (m_cloudFbo && m_cloudTexture
        && width == m_cloudWidth && height == m_cloudHeight)
    {
        return true;
    }

    if (!m_cloudFbo)
        glGenFramebuffers(1, &m_cloudFbo);
    if (!m_cloudTexture)
        glGenTextures(1, &m_cloudTexture);
    if (!m_cloudFbo || !m_cloudTexture)
        return false;

    glBindTexture(GL_TEXTURE_2D, m_cloudTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA16F,
        width,
        height,
        0,
        GL_RGBA,
        GL_FLOAT,
        nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, m_cloudFbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        m_cloudTexture,
        0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER)
        == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete)
        return false;

    m_cloudWidth = width;
    m_cloudHeight = height;
    return true;
}

void SkyRenderer::drawVolumetricClouds(
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const EnvironmentLighting& lighting,
    const SkyWeatherParameters& weather,
    int viewportX,
    int viewportY,
    int viewportWidth,
    int viewportHeight,
    GLint targetFramebuffer)
{
    if (!weather.enabled
        || (weather.cloudCover <= 0.01f
            && weather.precipitationRateMmPerHour <= 0.001f)
        || !ensureCloudTarget(viewportWidth, viewportHeight))
    {
        return;
    }

    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean oldDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);

    glBindFramebuffer(GL_FRAMEBUFFER, m_cloudFbo);
    glViewport(0, 0, m_cloudWidth, m_cloudHeight);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_cloudProgram);
    glUniformMatrix4fv(m_cloudUniformView, 1, GL_FALSE, view.m);
    glUniform2f(
        m_cloudUniformProjectionScale,
        projection.m[0],
        projection.m[5]);
    glUniform2f(
        m_cloudUniformCameraGlobalXZ,
        wrappedWorldCoordinate(weather.cameraGlobal.x),
        wrappedWorldCoordinate(weather.cameraGlobal.z));
    glUniform1f(
        m_cloudUniformCameraGlobalY,
        static_cast<float>(weather.cameraGlobal.y));
    glUniform3f(
        m_cloudUniformSunDirection,
        lighting.sunDirection.x,
        lighting.sunDirection.y,
        lighting.sunDirection.z);
    glUniform3f(
        m_cloudUniformSunColor,
        lighting.sunColor.x,
        lighting.sunColor.y,
        lighting.sunColor.z);
    glUniform1f(m_cloudUniformSunIntensity, lighting.sunIntensity);
    glUniform1f(m_cloudUniformDaylight, lighting.daylightFactor);
    glUniform1f(m_cloudUniformCloudCover, weather.cloudCover);
    glUniform1f(m_cloudUniformHumidity, weather.relativeHumidity);
    glUniform1f(
        m_cloudUniformPrecipitation,
        weather.precipitationRateMmPerHour);
    glUniform2f(
        m_cloudUniformWindVelocityXZ,
        weather.windVelocityXMps,
        weather.windVelocityZMps);
    glUniform1f(m_cloudUniformTime, weather.elapsedSeconds);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(targetFramebuffer));
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_cloudCompositeProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_cloudTexture);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);

    glDepthMask(oldDepthMask);
    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (!blendWasEnabled)
        glDisable(GL_BLEND);
}

void SkyRenderer::draw(
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const EnvironmentMap& environmentMap,
    const EnvironmentLighting& lighting,
    const SkyWeatherParameters& weather,
    float gamma,
    float brightness,
    float contrast,
    float saturation)
{
    if (!valid() || !environmentMap.valid())
        return;

    GLint oldViewport[4] = { 0, 0, 1, 1 };
    GLint oldFramebuffer = 0;
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFramebuffer);

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

    drawVolumetricClouds(
        view,
        projection,
        lighting,
        weather,
        oldViewport[0],
        oldViewport[1],
        oldViewport[2],
        oldViewport[3],
        oldFramebuffer);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldFramebuffer));
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    glDepthMask(oldDepthMask);
    glDepthFunc(GL_GREATER);
    if (!depthTestWasEnabled)
        glDisable(GL_DEPTH_TEST);
    else
        glEnable(GL_DEPTH_TEST);
    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
}

} // namespace heritage::graphics
