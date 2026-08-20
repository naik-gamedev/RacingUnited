#pragma once

#include <glad/glad.h>

#include "../../Core/Math/Math.hpp"
#include "../EnvironmentMap.hpp"

namespace heritage::graphics {

struct SkyWeatherParameters
{
    bool enabled = false;
    float cloudCover = 0.0f;
    float relativeHumidity = 0.55f;
    float precipitationRateMmPerHour = 0.0f;
    float windVelocityXMps = 0.0f;
    float windVelocityZMps = 0.0f;
    float elapsedSeconds = 0.0f;
    heritage::math::DVec3 cameraGlobal{ 0.0, 0.0, 0.0 };
};

class SkyRenderer
{
public:
    bool initialize();
    void shutdown();

    void draw(
        const heritage::math::Mat4& view,
        const heritage::math::Mat4& projection,
        const EnvironmentMap& environmentMap,
        const EnvironmentLighting& lighting,
        const SkyWeatherParameters& weather,
        float gamma,
        float brightness,
        float contrast,
        float saturation);

    bool valid() const { return m_program != 0 && m_vao != 0; }

private:
    bool ensureCloudTarget(int viewportWidth, int viewportHeight);
    void drawVolumetricClouds(
        const heritage::math::Mat4& view,
        const heritage::math::Mat4& projection,
        const EnvironmentLighting& lighting,
        const SkyWeatherParameters& weather,
        int viewportX,
        int viewportY,
        int viewportWidth,
        int viewportHeight,
        GLint targetFramebuffer);

    GLuint m_program = 0;
    GLuint m_cloudProgram = 0;
    GLuint m_cloudCompositeProgram = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_cloudFbo = 0;
    GLuint m_cloudTexture = 0;
    int m_cloudWidth = 0;
    int m_cloudHeight = 0;

    GLint m_cloudUniformView = -1;
    GLint m_cloudUniformProjectionScale = -1;
    GLint m_cloudUniformCameraGlobalXZ = -1;
    GLint m_cloudUniformCameraGlobalY = -1;
    GLint m_cloudUniformSunDirection = -1;
    GLint m_cloudUniformSunColor = -1;
    GLint m_cloudUniformSunIntensity = -1;
    GLint m_cloudUniformDaylight = -1;
    GLint m_cloudUniformCloudCover = -1;
    GLint m_cloudUniformHumidity = -1;
    GLint m_cloudUniformPrecipitation = -1;
    GLint m_cloudUniformWindVelocityXZ = -1;
    GLint m_cloudUniformTime = -1;
};

} // namespace heritage::graphics
