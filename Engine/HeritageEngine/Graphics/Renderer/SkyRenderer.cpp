#include "SkyRenderer.hpp"
#include "SkyRendererShaders.hpp"

#include "../ShaderProgram.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace heritage::graphics {
using namespace sky_renderer_shaders;
namespace {

constexpr float kCubeVertices[] = {
-1,1,-1,-1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,
-1,-1,1,-1,-1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1,-1,1,
1,-1,-1,1,-1,1,1,1,1,1,1,1,1,1,-1,1,-1,-1,
-1,-1,1,-1,1,1,1,1,1,1,1,1,1,-1,1,-1,-1,1,
-1,1,-1,1,1,-1,1,1,1,1,1,1,-1,1,1,-1,1,-1,
-1,-1,-1,-1,-1,1,1,-1,-1,1,-1,-1,-1,-1,1,1,-1,1};

struct HvolHeader{char magic[4];std::uint32_t version,width,height,depth,channels,byteType,reserved;};
bool linked(GLuint p){if(!p)return false;GLint ok=GL_FALSE;glGetProgramiv(p,GL_LINK_STATUS,&ok);return ok==GL_TRUE;}
void setup2D(GLenum internalFormat,int w,int h,GLuint& tex,GLenum filter=GL_LINEAR)
{
    if(!tex)glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    GLenum format=(internalFormat==GL_R16F||internalFormat==GL_R32F)?GL_RED:GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D,0,internalFormat,w,h,0,format,GL_FLOAT,nullptr);
}
void setupDepthTexture(int w,int h,int samples,GLuint& tex,GLenum& target)
{
    if(!tex)glGenTextures(1,&tex);
    if(samples>1)
    {
        target=GL_TEXTURE_2D_MULTISAMPLE;
        glBindTexture(target,tex);
        glTexImage2DMultisample(target,samples,GL_DEPTH_COMPONENT32F,w,h,GL_TRUE);
    }
    else
    {
        target=GL_TEXTURE_2D;
        glBindTexture(target,tex);
        glTexParameteri(target,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(target,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(target,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(target,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(target,0,GL_DEPTH_COMPONENT32F,w,h,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
    }
    glBindTexture(target,0);
}
float hermite(float a,float b,float x){float t=std::clamp((x-a)/(b-a),0.0f,1.0f);return t*t*(3-2*t);}

struct ScreenDirection
{
    float u = 0.5f;
    float v = 0.5f;
    bool visible = false;
};

ScreenDirection projectSkyDirection(
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const heritage::math::Vec3& direction)
{
    // Sky directions have no translation. Project the rotation-only view-space
    // vector exactly as the skybox vertex shader does so the post-cloud Sun
    // presentation remains registered with the astronomical Sun disc.
    const float viewX = view.m[0] * direction.x
        + view.m[4] * direction.y + view.m[8] * direction.z;
    const float viewY = view.m[1] * direction.x
        + view.m[5] * direction.y + view.m[9] * direction.z;
    const float viewZ = view.m[2] * direction.x
        + view.m[6] * direction.y + view.m[10] * direction.z;

    const float clipX = projection.m[0] * viewX
        + projection.m[4] * viewY + projection.m[8] * viewZ;
    const float clipY = projection.m[1] * viewX
        + projection.m[5] * viewY + projection.m[9] * viewZ;
    const float clipW = projection.m[3] * viewX
        + projection.m[7] * viewY + projection.m[11] * viewZ;
    if (clipW <= 1.0e-5f)
        return {};

    ScreenDirection result;
    result.u = clipX / clipW * 0.5f + 0.5f;
    result.v = clipY / clipW * 0.5f + 0.5f;
    // Keep a small guard band so rays leave smoothly as the Sun crosses an edge.
    result.visible = result.u >= -0.08f && result.u <= 1.08f
        && result.v >= -0.08f && result.v <= 1.08f;
    return result;
}
} // namespace

bool SkyRenderer::volumetricCloudsValid() const{return m_volumetricCloudProgramsLinked&&m_cloudShapeTexture&&m_cloudErosionTexture&&m_cloudLutTexture;}

bool SkyRenderer::loadCloudVolumeTexture(const std::filesystem::path& path,GLuint& texture,int ew,int eh,int ed)
{
    std::ifstream f(path,std::ios::binary);if(!f){std::cerr<<"Heritage clouds: missing upstream volume asset: "<<path<<'\n';return false;}HvolHeader h{};f.read(reinterpret_cast<char*>(&h),sizeof(h));if(!f||std::memcmp(h.magic,"HVOL",4)||h.version!=1||h.channels!=1||h.byteType!=1||int(h.width)!=ew||int(h.height)!=eh||int(h.depth)!=ed)return false;std::vector<std::uint8_t> p(size_t(ew)*eh*ed);f.read(reinterpret_cast<char*>(p.data()),std::streamsize(p.size()));if(!f)return false;if(!texture)glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_3D,texture);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_REPEAT);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_REPEAT);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage3D(GL_TEXTURE_3D,0,GL_R8,ew,eh,ed,0,GL_RED,GL_UNSIGNED_BYTE,p.data());glGenerateMipmap(GL_TEXTURE_3D);glBindTexture(GL_TEXTURE_3D,0);return true;
}

bool SkyRenderer::ensureCloudLut()
{
    if(m_cloudLutTexture)return true;
    // VCLOUD01: reproduce the four upstream preset curve families in one
    // 4x256 RGB16F texture. X selects Sparse/Cloudy/Overcast/Stormy and the
    // shader linearly interpolates between them from Heritage regional weather.
    constexpr int kPresetCount=4,kSamples=256;
    std::array<float,kPresetCount*kSamples*3> data{};
    const auto sampleCurve=[](float x,const float* keys,int keyCount)->float
    {
        if(x<=keys[0])return keys[1];
        for(int k=0;k<keyCount-1;++k)
        {
            const float x0=keys[k*2],v0=keys[k*2+1],x1=keys[(k+1)*2],v1=keys[(k+1)*2+1];
            if(x<=x1)return v0+(v1-v0)*hermite(x0,x1,x);
        }
        return keys[(keyCount-1)*2+1];
    };
    static constexpr float densityKeys[][8]={
        {0.0f,0.0f,0.05f,1.0f,0.75f,1.0f,1.0f,0.0f},
        {0.0f,0.0f,0.15f,1.0f,1.0f,0.10f,1.0f,0.10f},
        {0.0f,0.0f,0.05f,1.0f,0.90f,0.0f,1.0f,0.0f},
        {0.0f,0.0f,0.037f,1.0f,0.60f,1.0f,1.0f,0.0f}};
    static constexpr int densityCounts[]={4,3,4,4};
    static constexpr float erosionSparseCloudyOvercast[]={0.0f,1.0f,0.10f,0.90f,1.0f,1.0f};
    static constexpr float erosionStormy[]={0.0f,1.0f,0.05f,0.80f,0.2438f,0.9498f,0.50f,1.0f,0.93f,0.9268f,1.0f,1.0f};
    static constexpr float aoKeys[][6]={
        {0.0f,0.0f,0.25f,0.50f,1.0f,0.0f},
        {0.0f,0.0f,0.25f,0.40f,1.0f,0.0f},
        {0.0f,0.0f,0.50f,0.00f,1.0f,0.0f},
        {0.0f,0.0f,0.10f,0.40f,1.0f,0.0f}};
    for(int y=0;y<kSamples;++y)
    {
        const float h=float(y)/float(kSamples-1);
        for(int preset=0;preset<kPresetCount;++preset)
        {
            float density=sampleCurve(h,densityKeys[preset],densityCounts[preset]);
            if(y==0||y==kSamples-1)density=0.0f;
            const float erosion=preset==3?sampleCurve(h,erosionStormy,6):sampleCurve(h,erosionSparseCloudyOvercast,3);
            const float ambientOcclusion=sampleCurve(h,aoKeys[preset],3);
            const std::size_t offset=std::size_t((y*kPresetCount+preset)*3);
            data[offset+0]=density;
            data[offset+1]=erosion;
            data[offset+2]=1.0f-ambientOcclusion;
        }
    }
    glGenTextures(1,&m_cloudLutTexture);glBindTexture(GL_TEXTURE_2D,m_cloudLutTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,kPresetCount,kSamples,0,GL_RGB,GL_FLOAT,data.data());glBindTexture(GL_TEXTURE_2D,0);return true;
}

bool SkyRenderer::initialize(const std::filesystem::path& moduleAssetRoot)
{
    shutdown();m_assetRoot=moduleAssetRoot;m_program=buildShaderProgram(kSkyVertexShader,kPbrSkyFragmentShader);m_cloudRaymarchProgram=buildShaderProgram(kFullscreenVertexShader,kCloudRaymarchFragmentShader);m_cloudCombineProgram=buildShaderProgram(kFullscreenVertexShader,kCloudCombineFragmentShader);m_cloudTemporalProgram=buildShaderProgram(kFullscreenVertexShader,kCloudTemporalFragmentShader);m_cloudPresentProgram=buildShaderProgram(kFullscreenVertexShader,kCloudPresentFragmentShader);m_cloudGroundShadowProgram=buildShaderProgram(kFullscreenVertexShader,kCloudGroundShadowFragmentShader);m_cloudDepthMergeProgram=buildShaderProgram(kFullscreenVertexShader,kCloudDepthMergeFragmentShader);m_cloudShadowProgram=buildShaderProgram(kFullscreenVertexShader,kCloudShadowFragmentShader);m_cloudShadowFilterProgram=buildShaderProgram(kFullscreenVertexShader,kCloudShadowFilterFragmentShader);
    const bool skyProgramLinked=linked(m_program);
    m_volumetricCloudProgramsLinked=linked(m_cloudRaymarchProgram)&&linked(m_cloudCombineProgram)&&linked(m_cloudTemporalProgram)&&linked(m_cloudPresentProgram)&&linked(m_cloudGroundShadowProgram);
    m_cloudDepthMergeProgramLinked=linked(m_cloudDepthMergeProgram);
    m_cloudShadowProgramsLinked=linked(m_cloudShadowProgram)&&linked(m_cloudShadowFilterProgram);
    if(!skyProgramLinked)return false;
    glGenVertexArrays(1,&m_vao);glGenBuffers(1,&m_vbo);glBindVertexArray(m_vao);glBindBuffer(GL_ARRAY_BUFFER,m_vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(kCubeVertices),kCubeVertices,GL_STATIC_DRAW);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);glEnableVertexAttribArray(0);glBindVertexArray(0);
    if(!initializePhysicallyBasedAtmosphere()){shutdown();return false;}
    // CLOUDURP15E6: UnityVolumetricCloudsURP allocates its temporal history with
    // FilterMode.Point and samples it through a point-clamp sampler. Keep that
    // exact history policy; the current frame is already full-resolution before
    // temporal denoising.
    glGenSamplers(1,&m_cloudHistorySampler);glSamplerParameteri(m_cloudHistorySampler,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glSamplerParameteri(m_cloudHistorySampler,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glSamplerParameteri(m_cloudHistorySampler,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glSamplerParameteri(m_cloudHistorySampler,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glUseProgram(m_program);glUniform1i(glGetUniformLocation(m_program,"uEnvironmentMap"),0);glUniform1i(glGetUniformLocation(m_program,"uMoonTexture"),1);glUniform1i(glGetUniformLocation(m_program,"uStarMapTexture"),2);glUniform1i(glGetUniformLocation(m_program,"uPbrSkyViewLut"),3);glUniform1i(glGetUniformLocation(m_program,"uPbrTransmittanceLut"),4);m_uniformView=glGetUniformLocation(m_program,"uView");m_uniformProjection=glGetUniformLocation(m_program,"uProjection");m_uniformGamma=glGetUniformLocation(m_program,"uGamma");m_uniformBrightness=glGetUniformLocation(m_program,"uBrightness");m_uniformContrast=glGetUniformLocation(m_program,"uContrast");m_uniformSaturation=glGetUniformLocation(m_program,"uSaturation");m_uniformSunDirection=glGetUniformLocation(m_program,"uSunDirection");m_uniformSunColor=glGetUniformLocation(m_program,"uSunColor");m_uniformSunIntensity=glGetUniformLocation(m_program,"uSunIntensity");m_uniformDaylightFactor=glGetUniformLocation(m_program,"uDaylightFactor");m_uniformSkyHorizon=glGetUniformLocation(m_program,"uSkyHorizon");m_uniformSkyZenith=glGetUniformLocation(m_program,"uSkyZenith");m_uniformSkyExposure=glGetUniformLocation(m_program,"uSkyExposure");m_uniformAtmosphereThickness=glGetUniformLocation(m_program,"uAtmosphereThickness");m_uniformStarIntensity=glGetUniformLocation(m_program,"uStarIntensity");m_uniformWorldToCelestialRow0=glGetUniformLocation(m_program,"uWorldToCelestialRow0");m_uniformWorldToCelestialRow1=glGetUniformLocation(m_program,"uWorldToCelestialRow1");m_uniformWorldToCelestialRow2=glGetUniformLocation(m_program,"uWorldToCelestialRow2");m_uniformMoonDirection=glGetUniformLocation(m_program,"uMoonDirection");m_uniformMoonIntensity=glGetUniformLocation(m_program,"uMoonIntensity");m_uniformMoonPhase=glGetUniformLocation(m_program,"uMoonPhase");m_uniformWeatherCloudCover=glGetUniformLocation(m_program,"uWeatherCloudCover");m_uniformWeatherHumidity=glGetUniformLocation(m_program,"uWeatherHumidity");m_uniformWeatherPrecipitation=glGetUniformLocation(m_program,"uWeatherPrecipitationMmPerHour");m_uniformPbrSkyView=glGetUniformLocation(m_program,"uPbrSkyViewLut");m_uniformPbrSkyValid=glGetUniformLocation(m_program,"uPbrSkyValid");m_uniformPbrCameraAltitude=glGetUniformLocation(m_program,"uPbrCameraAltitudeM");
    auto R=[&](GLint& x,const char*n){x=glGetUniformLocation(m_cloudRaymarchProgram,n);};R(m_ray.view,"uView");R(m_ray.projection,"uProjection");R(m_ray.cameraGlobalXZ,"uCameraGlobal");R(m_ray.sunDirection,"uSunDirection");R(m_ray.sunColor,"uSunColor");R(m_ray.sunIntensity,"uSunIntensity");R(m_ray.moonDirection,"uMoonDirection");R(m_ray.moonColor,"uMoonColor");R(m_ray.moonIntensity,"uMoonIntensity");R(m_ray.skyHorizon,"uSkyHorizon");R(m_ray.skyZenith,"uSkyZenith");R(m_ray.time,"uTime");R(m_ray.temporalFrameIndex,"uTemporalFrameIndex");R(m_ray.cloudCover,"uCloudCover");R(m_ray.humidity,"uHumidity");R(m_ray.precipitation,"uPrecipitationMmPerHour");R(m_ray.windVelocityXZ,"uWindVelocityXZ");R(m_ray.baseWindXZ,"uCloudBaseWindVelocityXZ");R(m_ray.topWindXZ,"uCloudTopWindVelocityXZ");R(m_ray.regionalMap,"uRegionalWeatherMap");R(m_ray.regionalMapValid,"uRegionalWeatherMapValid");R(m_ray.regionalCameraOffsetXZ,"uRegionalWeatherCameraOffsetXZ");R(m_ray.regionalAdvectionXZ,"uRegionalWeatherAdvectionXZ");R(m_ray.regionalHalfRange,"uRegionalWeatherHalfRangeM");R(m_ray.shapeNoise,"uShapeNoise");R(m_ray.erosionNoise,"uErosionNoise");R(m_ray.curveLut,"uCurveLut");R(m_ray.environmentMap,"uEnvironmentMap");R(m_ray.pbrTransmittance,"uPbrTransmittanceLut");R(m_ray.pbrAtmosphereValid,"uPbrAtmosphereValid");R(m_ray.microErosion,"uMicroErosion");R(m_ray.physicallyBasedSun,"uPhysicallyBasedSun");R(m_ray.localClouds,"uLocalClouds");R(m_ray.sceneDepth,"uSceneDepth");R(m_ray.sceneDepthMs,"uSceneDepthMS");R(m_ray.sceneDepthSamples,"uSceneDepthSamples");R(m_ray.sceneColor,"uSceneColor");R(m_ray.perceptual,"uPerceptualBlending");
    auto C=[&](GLint& x,const char*n){x=glGetUniformLocation(m_cloudCombineProgram,n);};C(m_combine.cloud,"uCloudTexture");C(m_combine.scene,"uSceneTexture");C(m_combine.bilateral,"uBilateral");auto T=[&](GLint& x,const char*n){x=glGetUniformLocation(m_cloudTemporalProgram,n);};T(m_temporal.current,"uCurrentTexture");T(m_temporal.history,"uHistoryTexture");T(m_temporal.historyValid,"uHistoryValid");T(m_temporal.currentView,"uCurrentView");T(m_temporal.previousView,"uPreviousView");T(m_temporal.currentProjection,"uCurrentProjection");T(m_temporal.previousProjection,"uPreviousProjection");T(m_temporal.cameraDelta,"uCameraDelta");T(m_temporal.sceneDepth,"uSceneDepth");T(m_temporal.sceneDepthMs,"uSceneDepthMS");T(m_temporal.sceneDepthSamples,"uSceneDepthSamples");T(m_temporal.localClouds,"uLocalClouds");auto G=[&](GLint& x,const char*n){x=glGetUniformLocation(m_cloudGroundShadowProgram,n);};G(m_groundShadow.sceneDepth,"uSceneDepth");G(m_groundShadow.sceneDepthMs,"uSceneDepthMS");G(m_groundShadow.sceneDepthSamples,"uSceneDepthSamples");G(m_groundShadow.cloudShadow,"uCloudShadow");G(m_groundShadow.cloudShadowValid,"uCloudShadowValid");G(m_groundShadow.cloudShadowHalfRangeM,"uCloudShadowHalfRangeM");G(m_groundShadow.view,"uView");G(m_groundShadow.projection,"uProjection");G(m_groundShadow.celestialLightDirection,"uCelestialLightDirection");G(m_groundShadow.daylightFactor,"uDaylightFactor");m_present.source=glGetUniformLocation(m_cloudPresentProgram,"uSourceTexture");m_present.sceneDepth=glGetUniformLocation(m_cloudPresentProgram,"uSceneDepth");m_present.sceneDepthMs=glGetUniformLocation(m_cloudPresentProgram,"uSceneDepthMS");m_present.sceneDepthSamples=glGetUniformLocation(m_cloudPresentProgram,"uSceneDepthSamples");m_present.sunScreenUv=glGetUniformLocation(m_cloudPresentProgram,"uSunScreenUv");m_present.sunColor=glGetUniformLocation(m_cloudPresentProgram,"uSunColor");m_present.sunIntensity=glGetUniformLocation(m_cloudPresentProgram,"uSunIntensity");m_present.sunElevation=glGetUniformLocation(m_cloudPresentProgram,"uSunElevation");m_present.sunScreenVisible=glGetUniformLocation(m_cloudPresentProgram,"uSunScreenVisible");m_present.cloudCover=glGetUniformLocation(m_cloudPresentProgram,"uCloudCover");m_present.humidity=glGetUniformLocation(m_cloudPresentProgram,"uHumidity");m_present.precipitation=glGetUniformLocation(m_cloudPresentProgram,"uPrecipitation");
    auto D=[&](GLint& x,const char*n){x=glGetUniformLocation(m_cloudDepthMergeProgram,n);};D(m_depthMerge.sceneDepth,"uSceneDepth");D(m_depthMerge.sceneDepthMs,"uSceneDepthMS");D(m_depthMerge.sceneDepthSamples,"uSceneDepthSamples");D(m_depthMerge.cloudDepth,"uCloudDepth");
    auto S=[&](GLint& x,const char*n){x=glGetUniformLocation(m_cloudShadowProgram,n);};S(m_shadow.cameraGlobalXZ,"uCameraGlobal");S(m_shadow.sunDirection,"uCelestialLightDirection");S(m_shadow.time,"uTime");S(m_shadow.cloudCover,"uCloudCover");S(m_shadow.humidity,"uHumidity");S(m_shadow.precipitation,"uPrecipitation");S(m_shadow.baseWindXZ,"uBaseWindXZ");S(m_shadow.topWindXZ,"uTopWindXZ");S(m_shadow.regionalMap,"uRegionalWeatherMap");S(m_shadow.regionalMapValid,"uRegionalWeatherMapValid");S(m_shadow.regionalCameraOffsetXZ,"uRegionalCameraOffsetXZ");S(m_shadow.regionalAdvectionXZ,"uRegionalAdvectionXZ");S(m_shadow.regionalHalfRange,"uRegionalHalfRange");S(m_shadow.shapeNoise,"uShapeNoise");S(m_shadow.erosionNoise,"uErosionNoise");S(m_shadow.curveLut,"uCurveLut");S(m_shadow.microErosion,"uMicroErosion");S(m_shadow.halfRange,"uHalfRange");m_shadowFilter.source=glGetUniformLocation(m_cloudShadowFilterProgram,"uSource");m_shadowFilter.texelSize=glGetUniformLocation(m_cloudShadowFilterProgram,"uTexelSize");
    glUseProgram(m_cloudRaymarchProgram);glUniform1i(m_ray.regionalMap,0);glUniform1i(m_ray.shapeNoise,1);glUniform1i(m_ray.erosionNoise,2);glUniform1i(m_ray.curveLut,3);glUniform1i(m_ray.environmentMap,4);glUniform1i(m_ray.sceneDepth,5);glUniform1i(m_ray.sceneDepthMs,6);glUniform1i(m_ray.sceneColor,7);glUniform1i(m_ray.pbrTransmittance,8);glUseProgram(m_cloudCombineProgram);glUniform1i(m_combine.cloud,0);glUniform1i(m_combine.scene,1);glUseProgram(m_cloudTemporalProgram);glUniform1i(m_temporal.current,0);glUniform1i(m_temporal.history,1);glUniform1i(m_temporal.sceneDepth,2);glUniform1i(m_temporal.sceneDepthMs,3);glUseProgram(m_cloudPresentProgram);glUniform1i(m_present.source,0);glUniform1i(m_present.sceneDepth,2);glUniform1i(m_present.sceneDepthMs,3);glUseProgram(m_cloudGroundShadowProgram);glUniform1i(m_groundShadow.sceneDepth,0);glUniform1i(m_groundShadow.sceneDepthMs,1);glUniform1i(m_groundShadow.cloudShadow,2);glUseProgram(m_cloudDepthMergeProgram);glUniform1i(m_depthMerge.sceneDepth,0);glUniform1i(m_depthMerge.sceneDepthMs,1);glUniform1i(m_depthMerge.cloudDepth,2);glUseProgram(m_cloudShadowProgram);glUniform1i(m_shadow.regionalMap,0);glUniform1i(m_shadow.shapeNoise,1);glUniform1i(m_shadow.erosionNoise,2);glUniform1i(m_shadow.curveLut,3);glUseProgram(m_cloudShadowFilterProgram);glUniform1i(m_shadowFilter.source,0);glUseProgram(0);
    std::string moonError; if(const Texture2D* moon=m_textureCache.acquire(m_assetRoot/"Scenes"/"Moon.png",TextureColorSpace::SRgb,2,false,moonError)) m_moonTexture=moon->id;
    std::string starError; if(const Texture2D* stars=m_textureCache.acquire(m_assetRoot/"Scenes"/"Sky"/"Scene_NightSky.ktx2",TextureColorSpace::Linear,2,false,starError)) m_starMapTexture=stars->id; else if(!starError.empty()) std::cerr<<"Heritage sky: "<<starError<<'\n';
    const auto cloudRoot=m_assetRoot/"Weather"/"Clouds"/"UnityVolumetricCloudsURP";loadCloudVolumeTexture(cloudRoot/"WorleyNoise128R.hvol",m_cloudShapeTexture,128,128,128);loadCloudVolumeTexture(cloudRoot/"PerlinNoise32R.hvol",m_cloudErosionTexture,32,32,32);ensureCloudLut();
    // OPT00 pass timers are diagnostics only; renderer availability does not
    // depend on query allocation succeeding.
    m_backgroundGpuTimer.initialize();
    m_cloudShadowGpuTimer.initialize();
    m_cloudSceneCopyGpuTimer.initialize();
    m_cloudRaymarchGpuTimer.initialize();
    m_cloudUpscaleGpuTimer.initialize();
    m_cloudTemporalGpuTimer.initialize();
    m_cloudPresentGpuTimer.initialize();
    m_gpuStats={};
    return true;
}

void SkyRenderer::resetCloudHistory(){m_cloudHistoryValid=false;m_cloudTemporalFrameIndex=0;m_previousCloudView=heritage::math::identity();m_previousCloudProjection=heritage::math::identity();m_previousCloudCameraGlobal={0,0,0};}
void SkyRenderer::destroyCloudTargets()
{
    GLuint ts[]={m_cloudRaymarchTexture,m_cloudRaymarchDepthTexture,m_cloudSceneTexture,m_cloudSceneDepthTexture,m_cloudCombinedTexture,m_cloudTemporalTexture,m_cloudHistoryTexture};
    glDeleteTextures(7,ts);
    GLuint fs[]={m_cloudRaymarchFbo,m_cloudSceneFbo,m_cloudSceneDepthFbo,m_cloudCombinedFbo,m_cloudTemporalFbo,m_cloudHistoryFbo};
    glDeleteFramebuffers(6,fs);
    m_cloudRaymarchTexture=m_cloudRaymarchDepthTexture=m_cloudSceneTexture=m_cloudSceneDepthTexture=m_cloudCombinedTexture=m_cloudTemporalTexture=m_cloudHistoryTexture=0;
    m_cloudRaymarchFbo=m_cloudSceneFbo=m_cloudSceneDepthFbo=m_cloudCombinedFbo=m_cloudTemporalFbo=m_cloudHistoryFbo=0;
    m_cloudSceneDepthTarget=GL_TEXTURE_2D;m_cloudSceneDepthSamples=1;
    m_cloudHalfWidth=m_cloudHalfHeight=m_cloudFullWidth=m_cloudFullHeight=0;
    resetCloudHistory();
}
void SkyRenderer::destroyCloudShadowTargets(){GLuint t[]={m_cloudShadowRawTexture,m_cloudShadowTempTexture,m_cloudShadowTexture};glDeleteTextures(3,t);if(m_cloudShadowFbo)glDeleteFramebuffers(1,&m_cloudShadowFbo);m_cloudShadowRawTexture=m_cloudShadowTempTexture=m_cloudShadowTexture=m_cloudShadowFbo=0;m_cloudShadowReady=false;}

bool SkyRenderer::ensureCloudTargets(int w,int h,int sceneDepthSamples)
{
    w=std::max(w,1);h=std::max(h,1);sceneDepthSamples=std::max(sceneDepthSamples,1);
    // CLOUDURP15EC: raise the cloud raymarch from 0.80x to 0.90x linear
    // resolution. This gives silhouettes and internal lighting materially more
    // real samples before any denoising, without the 1.56x pixel cost of full-res.
    int hw=std::max(w,1),hh=std::max(h,1);
    if(m_cloudFullWidth==w&&m_cloudFullHeight==h&&m_cloudSceneDepthSamples==sceneDepthSamples&&m_cloudRaymarchTexture&&m_cloudSceneDepthTexture)return true;

    destroyCloudTargets();
    m_cloudFullWidth=w;m_cloudFullHeight=h;m_cloudHalfWidth=hw;m_cloudHalfHeight=hh;m_cloudSceneDepthSamples=sceneDepthSamples;
    glGenFramebuffers(1,&m_cloudRaymarchFbo);glGenFramebuffers(1,&m_cloudSceneFbo);glGenFramebuffers(1,&m_cloudSceneDepthFbo);glGenFramebuffers(1,&m_cloudCombinedFbo);glGenFramebuffers(1,&m_cloudTemporalFbo);glGenFramebuffers(1,&m_cloudHistoryFbo);
    setup2D(GL_RGBA16F,hw,hh,m_cloudRaymarchTexture);
    setup2D(GL_R32F,hw,hh,m_cloudRaymarchDepthTexture,GL_NEAREST);
    // CLOUDURP15E6: the upstream temporal denoiser operates on the full-resolution
    // camera colour after clouds are combined. Keep the staged scene colour at
    // full resolution so the prepare pass and 5-pixel TAA neighbourhood match
    // that architecture. RGBA8 still preserves the source scene's available
    // colour precision without inflating this staging target to RGBA16F.
    setup2D(GL_RGBA8,w,h,m_cloudSceneTexture);
    setup2D(GL_RGBA16F,w,h,m_cloudCombinedTexture);
    setup2D(GL_RGBA16F,w,h,m_cloudTemporalTexture);
    setup2D(GL_RGBA16F,w,h,m_cloudHistoryTexture);
    setupDepthTexture(w,h,sceneDepthSamples,m_cloudSceneDepthTexture,m_cloudSceneDepthTarget);

    glBindFramebuffer(GL_FRAMEBUFFER,m_cloudRaymarchFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,m_cloudRaymarchTexture,0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,m_cloudRaymarchDepthTexture,0);
    const GLenum rayBuffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,rayBuffers);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){glBindFramebuffer(GL_FRAMEBUFFER,0);destroyCloudTargets();return false;}

    const GLuint colorFbos[]={m_cloudSceneFbo,m_cloudCombinedFbo,m_cloudTemporalFbo,m_cloudHistoryFbo};
    const GLuint colorTextures[]={m_cloudSceneTexture,m_cloudCombinedTexture,m_cloudTemporalTexture,m_cloudHistoryTexture};
    for(int i=0;i<4;++i){glBindFramebuffer(GL_FRAMEBUFFER,colorFbos[i]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colorTextures[i],0);GLenum draw=GL_COLOR_ATTACHMENT0;glDrawBuffers(1,&draw);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){glBindFramebuffer(GL_FRAMEBUFFER,0);destroyCloudTargets();return false;}}

    glBindFramebuffer(GL_FRAMEBUFFER,m_cloudSceneDepthFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,m_cloudSceneDepthTarget,m_cloudSceneDepthTexture,0);
    glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){glBindFramebuffer(GL_FRAMEBUFFER,0);destroyCloudTargets();return false;}
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    return true;
}

bool SkyRenderer::ensureCloudShadowTargets()
{
    if(m_cloudShadowFbo&&m_cloudShadowRawTexture)return true;glGenFramebuffers(1,&m_cloudShadowFbo);setup2D(GL_R16F,m_cloudShadowResolution,m_cloudShadowResolution,m_cloudShadowRawTexture);setup2D(GL_R16F,m_cloudShadowResolution,m_cloudShadowResolution,m_cloudShadowTempTexture);setup2D(GL_R16F,m_cloudShadowResolution,m_cloudShadowResolution,m_cloudShadowTexture);return true;
}

void SkyRenderer::shutdown()
{
    shutdownPhysicallyBasedAtmosphere();
    m_backgroundGpuTimer.shutdown();
    m_cloudShadowGpuTimer.shutdown();
    m_cloudSceneCopyGpuTimer.shutdown();
    m_cloudRaymarchGpuTimer.shutdown();
    m_cloudUpscaleGpuTimer.shutdown();
    m_cloudTemporalGpuTimer.shutdown();
    m_cloudPresentGpuTimer.shutdown();
    m_gpuStats={};
    destroyCloudTargets();destroyCloudShadowTargets();GLuint ps[]={m_program,m_cloudRaymarchProgram,m_cloudCombineProgram,m_cloudTemporalProgram,m_cloudPresentProgram,m_cloudGroundShadowProgram,m_cloudDepthMergeProgram,m_cloudShadowProgram,m_cloudShadowFilterProgram};for(GLuint p:ps)if(p)glDeleteProgram(p);m_program=m_cloudRaymarchProgram=m_cloudCombineProgram=m_cloudTemporalProgram=m_cloudPresentProgram=m_cloudGroundShadowProgram=m_cloudDepthMergeProgram=m_cloudShadowProgram=m_cloudShadowFilterProgram=0;m_volumetricCloudProgramsLinked=m_cloudDepthMergeProgramLinked=m_cloudShadowProgramsLinked=false;if(m_cloudShapeTexture)glDeleteTextures(1,&m_cloudShapeTexture);if(m_cloudErosionTexture)glDeleteTextures(1,&m_cloudErosionTexture);if(m_cloudLutTexture)glDeleteTextures(1,&m_cloudLutTexture);m_cloudShapeTexture=m_cloudErosionTexture=m_cloudLutTexture=0;m_textureCache.clear();m_moonTexture=0;m_starMapTexture=0;if(m_cloudHistorySampler)glDeleteSamplers(1,&m_cloudHistorySampler);m_cloudHistorySampler=0;if(m_vbo)glDeleteBuffers(1,&m_vbo);if(m_vao)glDeleteVertexArrays(1,&m_vao);m_vbo=m_vao=0;m_environmentMapTexture=0;
}

void SkyRenderer::pollGpuTimers()
{
    using PerfClock = std::chrono::steady_clock;
    const auto millisecondsSince = [](PerfClock::time_point start) -> double {
        return std::chrono::duration<double, std::milli>(PerfClock::now() - start).count();
    };

    const auto totalStart = PerfClock::now();
    auto pollOne = [&](AsyncGpuTimer& timer, double& gpuDestination, double& cpuDestination)
    {
        const auto pollStart = PerfClock::now();
        double milliseconds = 0.0;
        if (timer.poll(milliseconds))
            gpuDestination = milliseconds;
        cpuDestination = millisecondsSince(pollStart);
    };

    pollOne(m_backgroundGpuTimer, m_gpuStats.backgroundMs, m_cpuStats.gpuTimerPollBackgroundMs);
    pollOne(m_cloudShadowGpuTimer, m_gpuStats.cloudShadowMs, m_cpuStats.gpuTimerPollCloudShadowMs);
    pollOne(m_cloudSceneCopyGpuTimer, m_gpuStats.cloudSceneCopyMs, m_cpuStats.gpuTimerPollSceneCopyMs);
    pollOne(m_cloudRaymarchGpuTimer, m_gpuStats.cloudRaymarchMs, m_cpuStats.gpuTimerPollRaymarchMs);
    pollOne(m_cloudUpscaleGpuTimer, m_gpuStats.cloudUpscaleMs, m_cpuStats.gpuTimerPollUpscaleMs);
    pollOne(m_cloudTemporalGpuTimer, m_gpuStats.cloudTemporalMs, m_cpuStats.gpuTimerPollTemporalMs);
    pollOne(m_cloudPresentGpuTimer, m_gpuStats.cloudPresentMs, m_cpuStats.gpuTimerPollPresentMs);
    m_cpuStats.gpuTimerPollTotalMs = millisecondsSince(totalStart);
}

void SkyRenderer::updateCloudShadows(const EnvironmentLighting& lighting,const SkyWeatherParameters& w)
{
    using PerfClock=std::chrono::steady_clock;
    const auto millisecondsSince=[](PerfClock::time_point start)->double{return std::chrono::duration<double,std::milli>(PerfClock::now()-start).count();};
    const auto internalTotalStart=PerfClock::now();

    // PERF04: reset only the cloud-shadow update breakdown. The enclosing
    // SkyRenderer::draw() still owns cloudShadowUpdateCpuMs so the internal
    // sum can be compared with the independently measured outer wall time.
    m_cpuStats.cloudShadowInternalTotalMs=0.0;
    m_cpuStats.cloudShadowEligibilityMs=0.0;
    m_cpuStats.cloudShadowTargetEnsureMs=0.0;
    m_cpuStats.cloudShadowTimerBeginMs=0.0;
    m_cpuStats.cloudShadowStateSetupMs=0.0;
    m_cpuStats.cloudShadowRawAttachmentMs=0.0;
    m_cpuStats.cloudShadowProgramBindMs=0.0;
    m_cpuStats.cloudShadowUniformUploadMs=0.0;
    m_cpuStats.cloudShadowTextureBindMs=0.0;
    m_cpuStats.cloudShadowRawDrawCallMs=0.0;
    m_cpuStats.cloudShadowFilterSetupMs=0.0;
    m_cpuStats.cloudShadowFilterAttachmentMs=0.0;
    m_cpuStats.cloudShadowFilterTextureBindMs=0.0;
    m_cpuStats.cloudShadowFilterDrawCallMs=0.0;
    m_cpuStats.cloudShadowCopyImageMs=0.0;
    m_cpuStats.cloudShadowFinalizeMs=0.0;
    m_cpuStats.cloudShadowTimerEndMs=0.0;
    m_cpuStats.cloudShadowResidualMs=0.0;

    const auto eligibilityStart=PerfClock::now();
    m_cloudShadowReady=false;
    const bool eligible=w.cloudShadows&&w.enabled&&volumetricCloudsValid()
        &&m_cloudShadowProgramsLinked;
    m_cpuStats.cloudShadowEligibilityMs=millisecondsSince(eligibilityStart);
    if(!eligible)
    {
        m_cpuStats.cloudShadowInternalTotalMs=millisecondsSince(internalTotalStart);
        return;
    }

    // Keep the original short-circuit order: shadow targets are ensured before
    // the sun-direction/intensity early-outs, exactly as before PERF04.
    const auto targetStart=PerfClock::now();
    const bool targetsReady=ensureCloudShadowTargets();
    m_cpuStats.cloudShadowTargetEnsureMs=millisecondsSince(targetStart);
    // CELESTIAL01: cloud-ground attenuation follows the same continuous
    // astronomical key used by scene direct lighting. Daytime therefore traces
    // toward the Sun; nighttime traces toward the Moon without a second cookie.
    if(!targetsReady||lighting.keyLightDirection.y<=0.01f||lighting.keyLightIntensity<=0.0001f)
    {
        m_cpuStats.cloudShadowInternalTotalMs=millisecondsSince(internalTotalStart);
        return;
    }

    const auto timerBeginStart=PerfClock::now();
    const bool gpuTimerActive=m_cloudShadowGpuTimer.begin();
    m_cpuStats.cloudShadowTimerBeginMs=millisecondsSince(timerBeginStart);

    const auto stateStart=PerfClock::now();
    glBindVertexArray(m_vao);
    glBindFramebuffer(GL_FRAMEBUFFER,m_cloudShadowFbo);
    glViewport(0,0,m_cloudShadowResolution,m_cloudShadowResolution);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_BLEND);
    m_cpuStats.cloudShadowStateSetupMs=millisecondsSince(stateStart);

    const auto rawAttachmentStart=PerfClock::now();
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,m_cloudShadowRawTexture,0);
    m_cpuStats.cloudShadowRawAttachmentMs=millisecondsSince(rawAttachmentStart);

    const auto programBindStart=PerfClock::now();
    glUseProgram(m_cloudShadowProgram);
    m_cpuStats.cloudShadowProgramBindMs=millisecondsSince(programBindStart);

    const auto uniformStart=PerfClock::now();
    glUniform3f(m_shadow.cameraGlobalXZ,float(w.cameraGlobal.x),float(w.cameraGlobal.y),float(w.cameraGlobal.z));
    glUniform3f(m_shadow.sunDirection,lighting.keyLightDirection.x,lighting.keyLightDirection.y,lighting.keyLightDirection.z);
    glUniform1f(m_shadow.time,w.elapsedSeconds);glUniform1f(m_shadow.cloudCover,w.authoredCloudCover);
    glUniform1f(m_shadow.humidity,w.relativeHumidity);glUniform1f(m_shadow.precipitation,w.precipitationRateMmPerHour);
    glUniform2f(m_shadow.baseWindXZ,w.cloudBaseWindVelocityXMps,w.cloudBaseWindVelocityZMps);
    glUniform2f(m_shadow.topWindXZ,w.cloudTopWindVelocityXMps,w.cloudTopWindVelocityZMps);
    glUniform1i(m_shadow.regionalMapValid,w.regionalWeatherTexture?1:0);
    glUniform2f(m_shadow.regionalCameraOffsetXZ,w.regionalWeatherCameraOffsetX,w.regionalWeatherCameraOffsetZ);
    glUniform2f(m_shadow.regionalAdvectionXZ,w.regionalWeatherAdvectionOffsetX,w.regionalWeatherAdvectionOffsetZ);
    glUniform1f(m_shadow.regionalHalfRange,w.regionalWeatherHalfRangeM);
    glUniform1i(m_shadow.microErosion,w.microErosion?1:0);glUniform1f(m_shadow.halfRange,m_cloudShadowHalfRangeM);
    m_cpuStats.cloudShadowUniformUploadMs=millisecondsSince(uniformStart);

    const auto textureStart=PerfClock::now();
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,w.regionalWeatherTexture);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_3D,m_cloudShapeTexture);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_3D,m_cloudErosionTexture);
    glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,m_cloudLutTexture);
    m_cpuStats.cloudShadowTextureBindMs=millisecondsSince(textureStart);

    const auto rawDrawStart=PerfClock::now();
    glDrawArrays(GL_TRIANGLES,0,3);
    m_cpuStats.cloudShadowRawDrawCallMs=millisecondsSince(rawDrawStart);

    const auto filterSetupStart=PerfClock::now();
    glUseProgram(m_cloudShadowFilterProgram);
    glUniform2f(m_shadowFilter.texelSize,1.0f/m_cloudShadowResolution,1.0f/m_cloudShadowResolution);
    GLuint src=m_cloudShadowRawTexture,dst=m_cloudShadowTempTexture;
    m_cpuStats.cloudShadowFilterSetupMs=millisecondsSince(filterSetupStart);

    for(int pass=0;pass<1;++pass)
    {
        const auto filterAttachmentStart=PerfClock::now();
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,dst,0);
        m_cpuStats.cloudShadowFilterAttachmentMs+=millisecondsSince(filterAttachmentStart);

        const auto filterTextureStart=PerfClock::now();
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,src);
        m_cpuStats.cloudShadowFilterTextureBindMs+=millisecondsSince(filterTextureStart);

        const auto filterDrawStart=PerfClock::now();
        glDrawArrays(GL_TRIANGLES,0,3);
        m_cpuStats.cloudShadowFilterDrawCallMs+=millisecondsSince(filterDrawStart);
        std::swap(src,dst);
    }

    if(src!=m_cloudShadowTexture)
    {
        const auto copyStart=PerfClock::now();
        glCopyImageSubData(src,GL_TEXTURE_2D,0,0,0,0,m_cloudShadowTexture,GL_TEXTURE_2D,0,0,0,0,m_cloudShadowResolution,m_cloudShadowResolution,1);
        m_cpuStats.cloudShadowCopyImageMs=millisecondsSince(copyStart);
    }

    const auto finalizeStart=PerfClock::now();
    m_cloudShadowReady=true;
    glBindVertexArray(0);
    m_cpuStats.cloudShadowFinalizeMs=millisecondsSince(finalizeStart);

    const auto timerEndStart=PerfClock::now();
    m_cloudShadowGpuTimer.end(gpuTimerActive);
    m_cpuStats.cloudShadowTimerEndMs=millisecondsSince(timerEndStart);
    m_cpuStats.cloudShadowInternalTotalMs=millisecondsSince(internalTotalStart);

    const double attributedMs=
        m_cpuStats.cloudShadowEligibilityMs+
        m_cpuStats.cloudShadowTargetEnsureMs+
        m_cpuStats.cloudShadowTimerBeginMs+
        m_cpuStats.cloudShadowStateSetupMs+
        m_cpuStats.cloudShadowRawAttachmentMs+
        m_cpuStats.cloudShadowProgramBindMs+
        m_cpuStats.cloudShadowUniformUploadMs+
        m_cpuStats.cloudShadowTextureBindMs+
        m_cpuStats.cloudShadowRawDrawCallMs+
        m_cpuStats.cloudShadowFilterSetupMs+
        m_cpuStats.cloudShadowFilterAttachmentMs+
        m_cpuStats.cloudShadowFilterTextureBindMs+
        m_cpuStats.cloudShadowFilterDrawCallMs+
        m_cpuStats.cloudShadowCopyImageMs+
        m_cpuStats.cloudShadowFinalizeMs+
        m_cpuStats.cloudShadowTimerEndMs;
    m_cpuStats.cloudShadowResidualMs=(std::max)(0.0,m_cpuStats.cloudShadowInternalTotalMs-attributedMs);
}

void SkyRenderer::draw(
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const EnvironmentMap& env,
    const EnvironmentLighting& lighting,
    const SkyWeatherParameters& w,
    const SkyRenderTargetState& t,
    float gamma,
    float brightness,
    float contrast,
    float saturation)
{
    using PerfClock = std::chrono::steady_clock;
    const auto millisecondsSince = [](PerfClock::time_point start) -> double {
        return std::chrono::duration<double, std::milli>(PerfClock::now() - start).count();
    };

    // PERF03: reset only the background-stage fields here. Cloud-stage fields
    // are reset by drawVolumetricCloudsAfterOpaque(), which runs later in the
    // same frame and must not erase this attribution before F8 copies it.
    m_cpuStats.backgroundTotalMs = 0.0;
    m_cpuStats.gpuTimerPollTotalMs = 0.0;
    m_cpuStats.gpuTimerPollBackgroundMs = 0.0;
    m_cpuStats.gpuTimerPollCloudShadowMs = 0.0;
    m_cpuStats.gpuTimerPollSceneCopyMs = 0.0;
    m_cpuStats.gpuTimerPollRaymarchMs = 0.0;
    m_cpuStats.gpuTimerPollUpscaleMs = 0.0;
    m_cpuStats.gpuTimerPollTemporalMs = 0.0;
    m_cpuStats.gpuTimerPollPresentMs = 0.0;
    m_cpuStats.backgroundTimerBeginMs = 0.0;
    m_cpuStats.backgroundStateSetupMs = 0.0;
    m_cpuStats.backgroundUniformUploadMs = 0.0;
    m_cpuStats.backgroundTextureBindMs = 0.0;
    m_cpuStats.backgroundDrawCallMs = 0.0;
    m_cpuStats.backgroundTimerEndMs = 0.0;
    m_cpuStats.cloudShadowUpdateCpuMs = 0.0;
    m_cpuStats.cloudShadowInternalTotalMs = 0.0;
    m_cpuStats.cloudShadowEligibilityMs = 0.0;
    m_cpuStats.cloudShadowTargetEnsureMs = 0.0;
    m_cpuStats.cloudShadowTimerBeginMs = 0.0;
    m_cpuStats.cloudShadowStateSetupMs = 0.0;
    m_cpuStats.cloudShadowRawAttachmentMs = 0.0;
    m_cpuStats.cloudShadowProgramBindMs = 0.0;
    m_cpuStats.cloudShadowUniformUploadMs = 0.0;
    m_cpuStats.cloudShadowTextureBindMs = 0.0;
    m_cpuStats.cloudShadowRawDrawCallMs = 0.0;
    m_cpuStats.cloudShadowFilterSetupMs = 0.0;
    m_cpuStats.cloudShadowFilterAttachmentMs = 0.0;
    m_cpuStats.cloudShadowFilterTextureBindMs = 0.0;
    m_cpuStats.cloudShadowFilterDrawCallMs = 0.0;
    m_cpuStats.cloudShadowCopyImageMs = 0.0;
    m_cpuStats.cloudShadowFinalizeMs = 0.0;
    m_cpuStats.cloudShadowTimerEndMs = 0.0;
    m_cpuStats.cloudShadowResidualMs = 0.0;
    m_cpuStats.backgroundRestoreMs = 0.0;

    const auto totalStart = PerfClock::now();
    pollGpuTimers();
    if (!valid() || !env.valid())
    {
        m_cpuStats.backgroundTotalMs = millisecondsSince(totalStart);
        return;
    }

    const auto timerBeginStart = PerfClock::now();
    const bool backgroundGpuTimerActive = m_backgroundGpuTimer.begin();
    m_cpuStats.backgroundTimerBeginMs = millisecondsSince(timerBeginStart);

    // PBSKY01: update the tiny physical atmosphere LUTs before drawing the sky.
    // This remains inside the existing background GPU timer and does not add a
    // synchronous readback/query path.
    updatePhysicallyBasedAtmosphereLuts(lighting, w);

    const auto stateSetupStart = PerfClock::now();
    m_environmentMapTexture = env.textureId();
    glBindFramebuffer(GL_FRAMEBUFFER, t.framebuffer);
    glViewport(t.viewportX, t.viewportY, t.viewportWidth, t.viewportHeight);
    if (t.scissorEnabled)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(t.scissorX, t.scissorY, t.scissorWidth, t.scissorHeight);
    }
    else
    {
        glDisable(GL_SCISSOR_TEST);
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glUseProgram(m_program);
    m_cpuStats.backgroundStateSetupMs = millisecondsSince(stateSetupStart);

    const auto uniformStart = PerfClock::now();
    glUniformMatrix4fv(m_uniformView, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(m_uniformProjection, 1, GL_FALSE, projection.m);
    glUniform1f(m_uniformGamma, gamma);
    glUniform1f(m_uniformBrightness, brightness);
    glUniform1f(m_uniformContrast, contrast);
    glUniform1f(m_uniformSaturation, saturation);
    glUniform3f(m_uniformSunDirection, lighting.sunDirection.x, lighting.sunDirection.y, lighting.sunDirection.z);
    glUniform3f(m_uniformSunColor, lighting.sunColor.x, lighting.sunColor.y, lighting.sunColor.z);
    glUniform1f(m_uniformSunIntensity, lighting.sunIntensity);
    glUniform1f(m_uniformDaylightFactor, lighting.daylightFactor);
    glUniform3f(m_uniformSkyHorizon, lighting.skyHorizon.x, lighting.skyHorizon.y, lighting.skyHorizon.z);
    glUniform3f(m_uniformSkyZenith, lighting.skyZenith.x, lighting.skyZenith.y, lighting.skyZenith.z);
    glUniform1f(m_uniformSkyExposure, lighting.skyExposure);
    glUniform1f(m_uniformAtmosphereThickness, lighting.atmosphereThickness);
    glUniform1f(m_uniformStarIntensity, lighting.starIntensity);
    glUniform3f(m_uniformWorldToCelestialRow0, lighting.worldToCelestialRow0.x, lighting.worldToCelestialRow0.y, lighting.worldToCelestialRow0.z);
    glUniform3f(m_uniformWorldToCelestialRow1, lighting.worldToCelestialRow1.x, lighting.worldToCelestialRow1.y, lighting.worldToCelestialRow1.z);
    glUniform3f(m_uniformWorldToCelestialRow2, lighting.worldToCelestialRow2.x, lighting.worldToCelestialRow2.y, lighting.worldToCelestialRow2.z);
    glUniform3f(m_uniformMoonDirection, lighting.moonDirection.x, lighting.moonDirection.y, lighting.moonDirection.z);
    glUniform1f(m_uniformMoonIntensity, lighting.moonIntensity);
    glUniform1f(m_uniformMoonPhase, lighting.moonPhase);
    glUniform1f(m_uniformWeatherCloudCover, w.enabled ? w.cloudCover : 0.0f);
    glUniform1f(m_uniformWeatherHumidity, w.enabled ? w.relativeHumidity : 0.0f);
    glUniform1f(m_uniformWeatherPrecipitation, w.enabled ? w.precipitationRateMmPerHour : 0.0f);
    glUniform1i(m_uniformPbrSkyValid, m_pbrAtmosphereReady ? 1 : 0);
    glUniform1f(m_uniformPbrCameraAltitude, std::clamp(static_cast<float>(w.cameraGlobal.y), 0.0f, 59990.0f));
    m_cpuStats.backgroundUniformUploadMs = millisecondsSince(uniformStart);

    const auto textureStart = PerfClock::now();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.textureId());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_moonTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_starMapTexture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_pbrSkyViewTexture);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_pbrTransmittanceTexture);
    glBindVertexArray(m_vao);
    m_cpuStats.backgroundTextureBindMs = millisecondsSince(textureStart);

    const auto drawStart = PerfClock::now();
    glDrawArrays(GL_TRIANGLES, 0, 36);
    m_cpuStats.backgroundDrawCallMs = millisecondsSince(drawStart);

    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);

    const auto timerEndStart = PerfClock::now();
    m_backgroundGpuTimer.end(backgroundGpuTimerActive);
    m_cpuStats.backgroundTimerEndMs = millisecondsSince(timerEndStart);

    const auto cloudShadowStart = PerfClock::now();
    updateCloudShadows(lighting, w);
    m_cpuStats.cloudShadowUpdateCpuMs = millisecondsSince(cloudShadowStart);

    const auto restoreStart = PerfClock::now();
    glBindFramebuffer(GL_FRAMEBUFFER, t.framebuffer);
    glViewport(t.viewportX, t.viewportY, t.viewportWidth, t.viewportHeight);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_GREATER);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    m_cpuStats.backgroundRestoreMs = millisecondsSince(restoreStart);
    m_cpuStats.backgroundTotalMs = millisecondsSince(totalStart);
}

void SkyRenderer::drawVolumetricCloudsAfterOpaque(
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const EnvironmentLighting& lighting,
    const SkyWeatherParameters& w,
    const SkyRenderTargetState& t)
{
    using PerfClock=std::chrono::steady_clock;
    const auto millisecondsSince=[](PerfClock::time_point start)->double{return std::chrono::duration<double,std::milli>(PerfClock::now()-start).count();};
    // PERF03: preserve the background-stage attribution written earlier in
    // this frame; reset only the post-opaque cloud fields.
    m_cpuStats.cloudAfterOpaqueTotalMs = 0.0;
    m_cpuStats.cloudTargetEnsureMs = 0.0;
    m_cpuStats.cloudSceneCopyMs = 0.0;
    m_cpuStats.cloudSceneColorBlitMs = 0.0;
    m_cpuStats.cloudSceneDepthBlitMs = 0.0;
    m_cpuStats.cloudRaymarchMs = 0.0;
    m_cpuStats.cloudRaymarchDrawCallMs = 0.0;
    m_cpuStats.cloudUpscaleMs = 0.0;
    m_cpuStats.cloudUpscaleDrawCallMs = 0.0;
    m_cpuStats.cloudTemporalMs = 0.0;
    m_cpuStats.cloudTemporalDrawCallMs = 0.0;
    m_cpuStats.cloudPresentMs = 0.0;
    m_cpuStats.cloudPresentDrawCallMs = 0.0;
    m_cpuStats.cloudDepthMergeDrawCallMs = 0.0;
    m_cpuStats.cloudRestoreMs = 0.0;
    const auto cloudTotalStart=PerfClock::now();
    if(!w.enabled||(w.authoredCloudCover<=.000001f&&w.precipitationRateMmPerHour<=.001f)||!volumetricCloudsValid())
    {
        resetCloudHistory();
        return;
    }

    const auto targetEnsureStart=PerfClock::now();
    glBindFramebuffer(GL_READ_FRAMEBUFFER,t.framebuffer);
    // OPT04B: EngineRendering already knows the target sample count. Avoid a
    // synchronous GL_SAMPLES driver query on every volumetric-cloud draw.
    const int sceneDepthSamples=std::max<int>(t.samples,1);
    if(!ensureCloudTargets(t.viewportWidth,t.viewportHeight,sceneDepthSamples))
    {
        m_cpuStats.cloudTargetEnsureMs=millisecondsSince(targetEnsureStart);
        resetCloudHistory();
        glBindFramebuffer(GL_FRAMEBUFFER,t.framebuffer);
        m_cpuStats.cloudAfterOpaqueTotalMs=millisecondsSince(cloudTotalStart);
        return;
    }
    m_cpuStats.cloudTargetEnsureMs=millisecondsSince(targetEnsureStart);

    glBindVertexArray(m_vao);

    const auto sceneCopyStart=PerfClock::now();
    const bool sceneCopyGpuTimerActive=m_cloudSceneCopyGpuTimer.begin();
    // Match URP's camera color + depth inputs. Color resolves into a full-resolution
    // single-sample RGBA8 staging target; depth stays multisampled when the source is multisampled so
    // local-cloud intersection can inspect the nearest reversed-Z sample.
    glBindFramebuffer(GL_READ_FRAMEBUFFER,t.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,m_cloudSceneFbo);
    const auto sceneColorBlitStart=PerfClock::now();
    glBlitFramebuffer(t.viewportX,t.viewportY,t.viewportX+t.viewportWidth,t.viewportY+t.viewportHeight,
        0,0,m_cloudFullWidth,m_cloudFullHeight,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    m_cpuStats.cloudSceneColorBlitMs=millisecondsSince(sceneColorBlitStart);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,t.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,m_cloudSceneDepthFbo);
    const auto sceneDepthBlitStart=PerfClock::now();
    glBlitFramebuffer(t.viewportX,t.viewportY,t.viewportX+t.viewportWidth,t.viewportY+t.viewportHeight,
        0,0,m_cloudFullWidth,m_cloudFullHeight,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
    m_cpuStats.cloudSceneDepthBlitMs=millisecondsSince(sceneDepthBlitStart);
    m_cloudSceneCopyGpuTimer.end(sceneCopyGpuTimerActive);
    m_cpuStats.cloudSceneCopyMs=millisecondsSince(sceneCopyStart);

    // Ray trace at Heritage's modestly raised cloud resolution. Attachment 0
    // stores scattering + TRANSMITTANCE; attachment 1 stores the optional cloud depth.
    const auto raymarchStart=PerfClock::now();
    const bool raymarchGpuTimerActive=m_cloudRaymarchGpuTimer.begin();
    glBindFramebuffer(GL_FRAMEBUFFER,m_cloudRaymarchFbo);
    glViewport(0,0,m_cloudHalfWidth,m_cloudHalfHeight);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_BLEND);
    glUseProgram(m_cloudRaymarchProgram);
    glUniformMatrix4fv(m_ray.view,1,GL_FALSE,view.m);glUniformMatrix4fv(m_ray.projection,1,GL_FALSE,projection.m);
    glUniform3f(m_ray.cameraGlobalXZ,float(w.cameraGlobal.x),float(w.cameraGlobal.y),float(w.cameraGlobal.z));
    glUniform3f(m_ray.sunDirection,lighting.sunDirection.x,lighting.sunDirection.y,lighting.sunDirection.z);
    glUniform3f(m_ray.sunColor,lighting.sunColor.x,lighting.sunColor.y,lighting.sunColor.z);glUniform1f(m_ray.sunIntensity,lighting.sunIntensity);
    glUniform3f(m_ray.moonDirection,lighting.moonDirection.x,lighting.moonDirection.y,lighting.moonDirection.z);
    glUniform3f(m_ray.moonColor,lighting.moonColor.x,lighting.moonColor.y,lighting.moonColor.z);glUniform1f(m_ray.moonIntensity,lighting.moonIntensity*kMoonSceneIlluminationScale);
    glUniform3f(m_ray.skyHorizon,lighting.skyHorizon.x,lighting.skyHorizon.y,lighting.skyHorizon.z);
    glUniform3f(m_ray.skyZenith,lighting.skyZenith.x,lighting.skyZenith.y,lighting.skyZenith.z);
    glUniform1f(m_ray.time,w.elapsedSeconds);glUniform1ui(m_ray.temporalFrameIndex,m_cloudTemporalFrameIndex++);glUniform1f(m_ray.cloudCover,w.authoredCloudCover);glUniform1f(m_ray.humidity,w.relativeHumidity);glUniform1f(m_ray.precipitation,w.precipitationRateMmPerHour);
    glUniform2f(m_ray.windVelocityXZ,w.windVelocityXMps,w.windVelocityZMps);glUniform2f(m_ray.baseWindXZ,w.cloudBaseWindVelocityXMps,w.cloudBaseWindVelocityZMps);glUniform2f(m_ray.topWindXZ,w.cloudTopWindVelocityXMps,w.cloudTopWindVelocityZMps);
    glUniform1i(m_ray.regionalMapValid,w.regionalWeatherTexture?1:0);glUniform2f(m_ray.regionalCameraOffsetXZ,w.regionalWeatherCameraOffsetX,w.regionalWeatherCameraOffsetZ);glUniform2f(m_ray.regionalAdvectionXZ,w.regionalWeatherAdvectionOffsetX,w.regionalWeatherAdvectionOffsetZ);glUniform1f(m_ray.regionalHalfRange,w.regionalWeatherHalfRangeM);
    glUniform1i(m_ray.microErosion,w.microErosion?1:0);glUniform1i(m_ray.physicallyBasedSun,w.physicallyBasedSun?1:0);glUniform1i(m_ray.pbrAtmosphereValid,m_pbrAtmosphereReady?1:0);glUniform1i(m_ray.localClouds,w.localVolumetricClouds?1:0);glUniform1i(m_ray.sceneDepthSamples,sceneDepthSamples);glUniform1i(m_ray.perceptual,w.perceptualBlending?1:0);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,w.regionalWeatherTexture);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_3D,m_cloudShapeTexture);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_3D,m_cloudErosionTexture);
    glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,m_cloudLutTexture);
    glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_CUBE_MAP,m_environmentMapTexture);
    glActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,m_cloudSceneDepthTarget==GL_TEXTURE_2D?m_cloudSceneDepthTexture:0);
    glActiveTexture(GL_TEXTURE6);glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,m_cloudSceneDepthTarget==GL_TEXTURE_2D_MULTISAMPLE?m_cloudSceneDepthTexture:0);
    glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,m_cloudSceneTexture);
    glActiveTexture(GL_TEXTURE8);glBindTexture(GL_TEXTURE_2D,m_pbrTransmittanceTexture);
    const auto raymarchDrawStart=PerfClock::now();
    glDrawArrays(GL_TRIANGLES,0,3);
    m_cpuStats.cloudRaymarchDrawCallMs=millisecondsSince(raymarchDrawStart);
    m_cloudRaymarchGpuTimer.end(raymarchGpuTimerActive);
    m_cpuStats.cloudRaymarchMs=millisecondsSince(raymarchStart);

    // CLOUDURP15E6: faithfully restore the upstream order: first upscale/combine
    // clouds into a full-resolution current camera-colour buffer whose alpha is
    // cloud transmittance, then run the temporal denoiser. There is no alternate
    // low-resolution/fused temporal path and no adaptive dither classifier.
    const auto upscaleStart=PerfClock::now();
    const bool upscaleGpuTimerActive=m_cloudUpscaleGpuTimer.begin();
    glBindFramebuffer(GL_FRAMEBUFFER,m_cloudCombinedFbo);glViewport(0,0,m_cloudFullWidth,m_cloudFullHeight);
    glUseProgram(m_cloudCombineProgram);glUniform1i(m_combine.bilateral,w.bilateralUpscale?1:0);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m_cloudRaymarchTexture);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,m_cloudSceneTexture);
    const auto upscaleDrawStart=PerfClock::now();glDrawArrays(GL_TRIANGLES,0,3);m_cpuStats.cloudUpscaleDrawCallMs=millisecondsSince(upscaleDrawStart);
    m_cloudUpscaleGpuTimer.end(upscaleGpuTimerActive);
    m_cpuStats.cloudUpscaleMs=millisecondsSince(upscaleStart);

    // UnityVolumetricCloudsURP Pass 3 / HDRP-derived temporal denoise:
    // full-resolution current scene+cloud RGB, cloud transmittance mask, 5-pixel
    // current colour AABB, point-clamped reprojected history, 0.95 accumulation
    // reduced by camera velocity. CLOUDURP15E8 strengthens coherent accumulation and raises high-frequency partial
    // samples toward 99.2-99.85% while coherent dense interiors use a stronger 97%
    // baseline. Heritage writes the equivalent resolved RGB
    // directly rather than relying on Unity's fixed-function blend.
    const auto temporalStart=PerfClock::now();
    const bool temporalGpuTimerActive=m_cloudTemporalGpuTimer.begin();
    glBindFramebuffer(GL_FRAMEBUFFER,m_cloudTemporalFbo);glViewport(0,0,m_cloudFullWidth,m_cloudFullHeight);glUseProgram(m_cloudTemporalProgram);
    glUniform1i(m_temporal.historyValid,m_cloudHistoryValid?1:0);glUniform1i(m_temporal.localClouds,w.localVolumetricClouds?1:0);glUniform1i(m_temporal.sceneDepthSamples,sceneDepthSamples);
    glUniformMatrix4fv(m_temporal.currentView,1,GL_FALSE,view.m);glUniformMatrix4fv(m_temporal.previousView,1,GL_FALSE,m_previousCloudView.m);glUniformMatrix4fv(m_temporal.currentProjection,1,GL_FALSE,projection.m);glUniformMatrix4fv(m_temporal.previousProjection,1,GL_FALSE,m_previousCloudProjection.m);
    glUniform3f(m_temporal.cameraDelta,float(w.cameraGlobal.x-m_previousCloudCameraGlobal.x),float(w.cameraGlobal.y-m_previousCloudCameraGlobal.y),float(w.cameraGlobal.z-m_previousCloudCameraGlobal.z));
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m_cloudCombinedTexture);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,m_cloudHistoryTexture);glBindSampler(1,m_cloudHistorySampler);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,m_cloudSceneDepthTarget==GL_TEXTURE_2D?m_cloudSceneDepthTexture:0);
    glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,m_cloudSceneDepthTarget==GL_TEXTURE_2D_MULTISAMPLE?m_cloudSceneDepthTexture:0);
    const auto temporalDrawStart=PerfClock::now();glDrawArrays(GL_TRIANGLES,0,3);m_cpuStats.cloudTemporalDrawCallMs=millisecondsSince(temporalDrawStart);
    glBindSampler(1,0);
    m_cloudTemporalGpuTimer.end(temporalGpuTimerActive);
    m_cpuStats.cloudTemporalMs=millisecondsSince(temporalStart);
    m_cloudHistoryValid=true;m_previousCloudView=view;m_previousCloudProjection=projection;m_previousCloudCameraGlobal=w.cameraGlobal;

    // CLOUDURP15E6: the temporal target is now the fully resolved scene+cloud RGB,
    // matching the upstream denoiser's camera-colour result. Copy RGB back to the
    // scene while preserving the destination alpha channel.
    const auto presentStart=PerfClock::now();
    const bool presentGpuTimerActive=m_cloudPresentGpuTimer.begin();
    glBindFramebuffer(GL_FRAMEBUFFER,t.framebuffer);glViewport(t.viewportX,t.viewportY,t.viewportWidth,t.viewportHeight);
    if(t.scissorEnabled){glEnable(GL_SCISSOR_TEST);glScissor(t.scissorX,t.scissorY,t.scissorWidth,t.scissorHeight);}else glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_ZERO,GL_ONE);glUseProgram(m_cloudPresentProgram);
    const ScreenDirection sunScreen = projectSkyDirection(
        view, projection, lighting.sunDirection);
    glUniform1i(m_present.sceneDepthSamples,sceneDepthSamples);
    glUniform2f(m_present.sunScreenUv,sunScreen.u,sunScreen.v);
    glUniform3f(m_present.sunColor,
        lighting.sunColor.x,lighting.sunColor.y,lighting.sunColor.z);
    glUniform1f(m_present.sunIntensity,lighting.sunIntensity);
    glUniform1f(m_present.sunElevation,lighting.sunDirection.y);
    glUniform1i(m_present.sunScreenVisible,sunScreen.visible?1:0);
    glUniform1f(m_present.cloudCover,w.authoredCloudCover);
    glUniform1f(m_present.humidity,w.relativeHumidity);
    glUniform1f(m_present.precipitation,w.precipitationRateMmPerHour);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m_cloudTemporalTexture);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,m_cloudSceneDepthTarget==GL_TEXTURE_2D?m_cloudSceneDepthTexture:0);
    glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,m_cloudSceneDepthTarget==GL_TEXTURE_2D_MULTISAMPLE?m_cloudSceneDepthTexture:0);
    const auto presentDrawStart=PerfClock::now();glDrawArrays(GL_TRIANGLES,0,3);m_cpuStats.cloudPresentDrawCallMs=millisecondsSince(presentDrawStart);

    // CELESTIAL04 remains a separate opaque-receiver multiplier. Apply it after
    // the resolved camera colour is restored; sky pixels have zero reversed-Z
    // depth and are therefore untouched. This keeps cloud-shadow state out of
    // temporal history and avoids ghosting a moving ground cookie.
    if(m_cloudShadowReady&&m_cloudShadowTexture&&lighting.keyLightDirection.y>0.01f)
    {
        glBlendFuncSeparate(GL_ZERO,GL_SRC_COLOR,GL_ZERO,GL_ONE);
        glUseProgram(m_cloudGroundShadowProgram);
        glUniform1i(m_groundShadow.sceneDepthSamples,sceneDepthSamples);
        glUniform1i(m_groundShadow.cloudShadowValid,1);
        glUniform1f(m_groundShadow.cloudShadowHalfRangeM,m_cloudShadowHalfRangeM);
        glUniformMatrix4fv(m_groundShadow.view,1,GL_FALSE,view.m);
        glUniformMatrix4fv(m_groundShadow.projection,1,GL_FALSE,projection.m);
        glUniform3f(m_groundShadow.celestialLightDirection,lighting.keyLightDirection.x,lighting.keyLightDirection.y,lighting.keyLightDirection.z);
        glUniform1f(m_groundShadow.daylightFactor,lighting.daylightFactor);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m_cloudSceneDepthTarget==GL_TEXTURE_2D?m_cloudSceneDepthTexture:0);
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,m_cloudSceneDepthTarget==GL_TEXTURE_2D_MULTISAMPLE?m_cloudSceneDepthTexture:0);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,m_cloudShadowTexture);
        glDrawArrays(GL_TRIANGLES,0,3);
    }

    // Experimental upstream OUTPUT_DEPTH_TO_SCENE_DEPTH path. Keep it opt-in,
    // matching the source feature's experimental nature, while always exposing the
    // generated cloud-depth texture through cloudDepthTexture().
    if(w.outputCloudDepth&&w.outputCloudDepthToScene&&m_cloudDepthMergeProgramLinked)
    {
        glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glDepthMask(GL_TRUE);glDisable(GL_DEPTH_TEST);glUseProgram(m_cloudDepthMergeProgram);glUniform1i(m_depthMerge.sceneDepthSamples,sceneDepthSamples);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m_cloudSceneDepthTarget==GL_TEXTURE_2D?m_cloudSceneDepthTexture:0);
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,m_cloudSceneDepthTarget==GL_TEXTURE_2D_MULTISAMPLE?m_cloudSceneDepthTexture:0);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,m_cloudRaymarchDepthTexture);
        const auto depthMergeDrawStart=PerfClock::now();glDrawArrays(GL_TRIANGLES,0,3);m_cpuStats.cloudDepthMergeDrawCallMs=millisecondsSince(depthMergeDrawStart);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    }
    m_cloudPresentGpuTimer.end(presentGpuTimerActive);
    m_cpuStats.cloudPresentMs=millisecondsSince(presentStart);

    const auto restoreStart=PerfClock::now();
    // The texture just presented becomes next frame's history. Swap both texture
    // and FBO ownership so the next temporal pass writes into the other texture
    // without a reattachment call and, crucially, without a full-image copy.
    std::swap(m_cloudTemporalTexture,m_cloudHistoryTexture);
    std::swap(m_cloudTemporalFbo,m_cloudHistoryFbo);

    // OPT04B: following passes explicitly bind every texture target/unit they
    // consume. Do not spend 40 GL calls zeroing four texture targets across
    // eight units after every cloud frame; only normalize the active unit.
    glActiveTexture(GL_TEXTURE0);glUseProgram(0);glBindVertexArray(0);glDepthMask(GL_TRUE);glDepthFunc(GL_GREATER);glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);
    m_cpuStats.cloudRestoreMs=millisecondsSince(restoreStart);
    m_cpuStats.cloudAfterOpaqueTotalMs=millisecondsSince(cloudTotalStart);
}


} // namespace heritage::graphics
