#pragma once

namespace heritage::graphics::sky_renderer_shaders {

extern const char* kSkyVertexShader;
extern const char* kPbrSkyFragmentShader;
extern const char* kPbrAtmosphereTransmittanceFragmentShader;
extern const char* kPbrAtmosphereMultiScatteringFragmentShader;
extern const char* kPbrAtmosphereSkyViewFragmentShader;
extern const char* kFullscreenVertexShader;
extern const char* kCloudRaymarchFragmentShader;
extern const char* kCloudCombineFragmentShader;
extern const char* kCloudTemporalFragmentShader;
extern const char* kCloudPresentFragmentShader;
extern const char* kCloudGroundShadowFragmentShader;
extern const char* kCloudDepthMergeFragmentShader;
extern const char* kCloudShadowFragmentShader;
extern const char* kCloudShadowFilterFragmentShader;

} // namespace heritage::graphics::sky_renderer_shaders
