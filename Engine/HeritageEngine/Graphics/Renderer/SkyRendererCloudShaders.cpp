#include "SkyRendererShaders.hpp"

namespace heritage::graphics::sky_renderer_shaders {

const char* kFullscreenVertexShader = R"glsl(#version 460 core
out vec2 vUv;
void main()
{
    vec2 p = gl_VertexID == 0 ? vec2(-1.0, -1.0)
        : (gl_VertexID == 1 ? vec2(3.0, -1.0) : vec2(-1.0, 3.0));
    vUv = p * 0.5 + 0.5;
    // Heritage uses reversed Z; NDC -1 maps to the far depth value 0.
    gl_Position = vec4(p, -1.0, 1.0);
}
)glsl";

// Heritage-native GLSL translation of the active HDRP-derived model in
// VolumetricClouds.hlsl + VolumetricCloudsUtilities.hlsl. Heritage regional
// weather interpolates the upstream Sparse/Cloudy/Overcast/Stormy families.
// Alpha is TRANSMITTANCE, exactly as in the source shader.
const char* kCloudRaymarchFragmentShader = R"glsl(#version 460 core
in vec2 vUv;
layout(location=0) out vec4 FragColor;
layout(location=1) out float CloudDepth;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCameraGlobal;
uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform vec3 uMoonDirection;
uniform vec3 uMoonColor;
uniform float uMoonIntensity;
uniform vec3 uSkyHorizon;
uniform vec3 uSkyZenith;
uniform float uTime;
uniform uint uTemporalFrameIndex;
uniform float uCloudCover;
uniform float uHumidity;
uniform float uPrecipitationMmPerHour;
uniform vec2 uWindVelocityXZ;
uniform vec2 uCloudBaseWindVelocityXZ;
uniform vec2 uCloudTopWindVelocityXZ;
uniform sampler2D uRegionalWeatherMap;
uniform bool uRegionalWeatherMapValid;
uniform vec2 uRegionalWeatherCameraOffsetXZ;
uniform vec2 uRegionalWeatherAdvectionXZ;
uniform float uRegionalWeatherHalfRangeM;
uniform sampler3D uShapeNoise;
uniform sampler3D uErosionNoise;
uniform sampler2D uCurveLut;
uniform samplerCube uEnvironmentMap;
uniform sampler2D uPbrTransmittanceLut;
uniform bool uPbrAtmosphereValid;
uniform bool uMicroErosion;
uniform bool uPhysicallyBasedSun;
uniform bool uLocalClouds;
uniform sampler2D uSceneDepth;
uniform sampler2DMS uSceneDepthMS;
uniform int uSceneDepthSamples;
uniform sampler2D uSceneColor;
uniform bool uPerceptualBlending;

const float PI=3.14159265358979323846;
const float EARTH_RADIUS=6378100.0;
// VCLOUD01: the broad shell contains all four upstream presets. Local density
// uses the preset-specific bottom/range, so this intersection is only a cheap
// bounding volume and does not flatten all cloud types into one slab.
const float CLOUD_SHELL_MIN_ALTITUDE_M=900.0;
const float CLOUD_SHELL_MAX_ALTITUDE_M=6500.0;
const float LOWEST=EARTH_RADIUS+CLOUD_SHELL_MIN_ALTITUDE_M;
const float HIGHEST=EARTH_RADIUS+CLOUD_SHELL_MAX_ALTITUDE_M;
const float NOISE_TEXTURE_NORMALIZATION_FACTOR=100000.0;
const float MAX_SKYBOX_VOLUMETRIC_CLOUDS_DISTANCE=200000.0;
const float CLOUD_DENSITY_THRESHOLD=0.001;
const int PRIMARY_STEPS=32;
const int NUM_LIGHT_STEPS=2;
const int EMPTY_STEPS_BEFORE_LARGE_STEPS=8;
const int NUM_MULTI_SCATTERING_OCTAVES=2;
const float LIGHT_STEP_MAXIMAL_SIZE=1000.0;
const float FORWARD_ECCENTRICITY=0.7;
const float BACKWARD_ECCENTRICITY=0.7;
const float MULTI_SCATTERING=0.525;
const float POWDER_EFFECT_INTENSITY=0.25;
const float MOON_INTERIOR_SCATTERING_STRENGTH=1.35;
const float MOON_INTERIOR_DENSITY_SCALE=5.0;
const float MOON_FORWARD_ECCENTRICITY=0.90;
const float MOON_FORWARD_AUREOLE_STRENGTH=1.10;
const float MOON_CLOUD_SCATTER_EXPOSURE=5.0;
const float EROSION_OCCLUSION=0.1;
const float MIN_EROSION_DISTANCE=3000.0;
const float MAX_EROSION_DISTANCE=100000.0;
const float FADE_IN_START=0.0;
const float FADE_IN_DISTANCE=400.0;

struct CloudCoverageData { float coverage; float rainClouds; float cloudType; float maxCloudHeight; };
struct CloudProperties { float density; float ambientOcclusion; float height; float sigmaT; };
struct VolumetricRayResult { vec3 sunScattering; vec3 moonScattering; float ambient; float transmittance; float meanDistance; bool invalidRay; };

float sat(float x){return clamp(x,0.0,1.0);}
float densityRemap(float x,float a,float b,float c,float d){return ((x-a)/max(b-a,1e-5))*(d-c)+c;}
float luminance(vec3 c){return dot(c,vec3(0.2126,0.7152,0.0722));}
float presetValue(vec4 values,float cloudType)
{
    float t=sat(cloudType)*3.0;
    if(t<1.0)return mix(values.x,values.y,t);
    if(t<2.0)return mix(values.y,values.z,t-1.0);
    return mix(values.z,values.w,t-2.0);
}

uint jenkinsHash(uint x){x+=x<<10u;x^=x>>6u;x+=x<<3u;x^=x>>11u;x+=x<<15u;return x;}
uint jenkinsHash(uvec2 v){return jenkinsHash(v.x^jenkinsHash(v.y));}
uint jenkinsHash(uvec3 v){return jenkinsHash(v.x^jenkinsHash(v.yz));}
float constructFloat(uint m){m=(m&0x007fffffu)|0x3f800000u;return uintBitsToFloat(m)-1.0;}
float integrationNoise()
{
    // CLOUDURP15E6: stochastic integration is indexed by rendered cloud frame,
    // not simulation time. Pausing/slowing the clock must not freeze the dither
    // sequence or prevent temporal convergence.
    uvec2 px=uvec2(max(gl_FragCoord.xy,vec2(0.0)));
    return constructFloat(jenkinsHash(uvec3(px,uTemporalFrameIndex)));
}

vec2 sphereRoots(float radius,vec3 origin,vec3 dir)
{
    float b=dot(origin,dir);float c=dot(origin,origin)-radius*radius;float disc=b*b-c;
    if(disc<0.0)return vec2(-1.0);float s=sqrt(disc);return vec2(-b-s,-b+s);
}
float horizonCos(float r){float q=EARTH_RADIUS/max(r,EARTH_RADIUS);return -sqrt(sat(1.0-q*q));}
bool intersectCloudVolume(vec3 origin,vec3 dir,out float entry,out float exitD)
{
    vec2 inner=sphereRoots(LOWEST,origin,dir);vec2 outer=sphereRoots(HIGHEST,origin,dir);
    if(outer.y<0.0){entry=exitD=0.0;return false;}
    float radial=length(origin);float cosChi=dot(origin,dir)/max(radial,1e-6);
    if(radial<LOWEST)
    {
        if(inner.y<0.0||cosChi<horizonCos(radial)){entry=exitD=0.0;return false;}
        entry=max(inner.y,0.0);exitD=outer.y;
    }
    else if(radial<=HIGHEST)
    {
        entry=0.0;exitD=inner.x>=0.0?inner.x:outer.y;
    }
    else
    {
        if(outer.x<0.0){entry=exitD=0.0;return false;}
        entry=outer.x;exitD=inner.x>=entry?inner.x:outer.y;
    }
    return exitD>entry;
}
bool exitCloudVolume(vec3 origin,vec3 dir,out float tExit)
{
    vec2 roots=sphereRoots(HIGHEST,origin,dir);tExit=roots.y;
    float r=length(origin);float c=dot(origin,dir)/max(r,1e-6);
    return tExit>=0.0&&c>=horizonCos(r);
}

vec4 regionalWeather(vec2 cameraRelativeXZ)
{
    vec4 w;
    if(!uRegionalWeatherMapValid||uRegionalWeatherHalfRangeM<=1.0)
        w=vec4(uCloudCover,sat(uPrecipitationMmPerHour/80.0),uHumidity,0.0);
    else
    {
        vec2 uv=vec2(0.5)+(cameraRelativeXZ+uRegionalWeatherCameraOffsetXZ+uRegionalWeatherAdvectionXZ)/(2.0*uRegionalWeatherHalfRangeM);
        if(any(lessThan(uv,vec2(0.0)))||any(greaterThan(uv,vec2(1.0))))
            w=vec4(uCloudCover,sat(uPrecipitationMmPerHour/80.0),uHumidity,0.0);
        else
            w=texture(uRegionalWeatherMap,uv);
    }
    return clamp(w,vec4(0.0),vec4(1.0));
}
CloudCoverageData getCloudCoverageData(vec3 positionPS)
{
    vec2 relativeXZ=positionPS.xz-uCameraGlobal.xz;
    vec4 w=regionalWeather(relativeXZ);
    float manualRain=sat(uPrecipitationMmPerHour/80.0);
    float rainClouds=max(max(w.g,w.a),manualRain);
    float coverage=max(w.r,smoothstep(0.12,0.75,rainClouds)*0.35);
    // The Heritage regional map already owns weather. Its R/G/B/A channels are
    // coverage/rain/humidity/storm, so derive the upstream cloud-type coordinate
    // without creating a second weather simulation.
    float cloudType=sat(0.05+0.45*coverage+0.20*w.b+0.55*rainClouds);
    CloudCoverageData d;
    d.coverage=sat(coverage);
    d.rainClouds=sat(rainClouds);
    d.cloudType=cloudType;
    d.maxCloudHeight=1.0;
    return d;
}

void presetLayer(float cloudType,out float bottomAltitude,out float altitudeRange)
{
    // Upstream presets: Sparse / Cloudy / Overcast / Stormy.
    bottomAltitude=presetValue(vec4(3000.0,1200.0,1500.0,1000.0),cloudType);
    altitudeRange=presetValue(vec4(1000.0,2000.0,2500.0,5000.0),cloudType);
}
float normalizedCloudHeight(vec3 positionPS,float cloudType)
{
    float bottomAltitude,altitudeRange;presetLayer(cloudType,bottomAltitude,altitudeRange);
    float altitude=length(positionPS)-EARTH_RADIUS;
    return (altitude-bottomAltitude)/max(altitudeRange,1.0);
}
float densityFadeValue(float distanceToCamera)
{
    return sat((distanceToCamera-FADE_IN_START)/max(FADE_IN_DISTANCE,1.0));
}
float erosionMipOffset(float distanceToCamera)
{
    return mix(0.0,4.0,sat((distanceToCamera-MIN_EROSION_DISTANCE)/(MAX_EROSION_DISTANCE-MIN_EROSION_DISTANCE)));
}
vec2 layerWind(float height)
{
    vec2 wind=mix(uCloudBaseWindVelocityXZ,uCloudTopWindVelocityXZ,sat(height));
    if(dot(wind,wind)<1e-5)wind=uWindVelocityXZ;
    return wind;
}
vec3 sampleCurveLut(float cloudType,float height)
{
    // Four 64-sample preset columns, linearly interpolated by cloud type.
    float u=(0.5+3.0*sat(cloudType))/4.0;
    return textureLod(uCurveLut,vec2(u,sat(height)),0.0).rgb;
}

void evaluateCloudProperties(vec3 positionPS,float noiseMipOffset,float erosionMipOffsetValue,bool cheapVersion,bool lightSampling,out CloudProperties properties)
{
    properties.density=0.0;properties.ambientOcclusion=1.0;properties.height=0.0;properties.sigmaT=0.04;
    if(!uLocalClouds&&positionPS.y<EARTH_RADIUS)return;

    CloudCoverageData cloudCoverageData=getCloudCoverageData(positionPS);
    if(cloudCoverageData.coverage<=CLOUD_DENSITY_THRESHOLD)return;

    properties.height=normalizedCloudHeight(positionPS,cloudCoverageData.cloudType);
    if(properties.height<0.0||properties.height>cloudCoverageData.maxCloudHeight)return;

    vec2 wind=layerWind(properties.height);
    vec2 windDir=dot(wind,wind)>1e-5?normalize(wind):vec2(-1.0,0.0);
    vec3 shapePosition=positionPS;
    shapePosition.y+=shapePosition.x/3.0+shapePosition.z/7.0;
    shapePosition+=vec3(wind.x,0.0,wind.y)*uTime;

    const float shapeScale=5.0;
    vec3 baseNoiseSamplingCoordinates=shapePosition.xzy/NOISE_TEXTURE_NORMALIZATION_FACTOR*shapeScale;
    baseNoiseSamplingCoordinates+=properties.height*vec3(windDir.x,windDir.y,0.0)*0.0625;
    float lowFrequencyNoise=textureLod(uShapeNoise,baseNoiseSamplingCoordinates,noiseMipOffset).r;

    vec3 densityErosionAO=sampleCurveLut(cloudCoverageData.cloudType,properties.height);
    float shapeFactorPreset=presetValue(vec4(0.95,0.90,0.50,0.85),cloudCoverageData.cloudType);
    float erosionFactorPreset=presetValue(vec4(0.80,0.80,0.50,0.75),cloudCoverageData.cloudType);
    float densityMultiplier=presetValue(vec4(0.32,0.32,0.18,0.245),cloudCoverageData.cloudType);
    float microErosionFactor=presetValue(vec4(0.65,0.65,0.65,0.65),cloudCoverageData.cloudType);
    float microErosionScale=presetValue(vec4(300.0,300.0,300.0,300.0),cloudCoverageData.cloudType);
    const float erosionScale=107.0;

    float shapeFactor=mix(0.1,1.0,shapeFactorPreset)*densityErosionAO.g;
    float erosionFactor=erosionFactorPreset*densityErosionAO.g;
    lowFrequencyNoise=mix(1.0,lowFrequencyNoise,shapeFactor);
    float baseCloud=1.0-densityErosionAO.r*cloudCoverageData.coverage*(1.0-shapeFactor);
    baseCloud=sat(densityRemap(lowFrequencyNoise,baseCloud,1.0,0.0,1.0))*cloudCoverageData.coverage*cloudCoverageData.coverage;

    properties.ambientOcclusion=densityErosionAO.b;
    properties.sigmaT=mix(0.04,0.12,cloudCoverageData.rainClouds);
    float ambientOcclusionBlend=sat(1.0-max(erosionFactor,shapeFactor)*0.5);
    properties.ambientOcclusion=mix(1.0,properties.ambientOcclusion,ambientOcclusionBlend);

    if(!cheapVersion)
    {
        vec3 erosionPosition=positionPS+vec3(wind.x,0.0,wind.y)*uTime*1.35;
        vec3 erosionCoords=erosionPosition/NOISE_TEXTURE_NORMALIZATION_FACTOR*erosionScale;
        float erosionNoise=1.0-textureLod(uErosionNoise,erosionCoords,erosionMipOffsetValue).r;
        erosionNoise=mix(0.0,erosionNoise,erosionFactor*0.75*cloudCoverageData.coverage);
        properties.ambientOcclusion=sat(properties.ambientOcclusion-sqrt(max(erosionNoise*EROSION_OCCLUSION,0.0)));
        baseCloud=densityRemap(baseCloud,erosionNoise,1.0,0.0,1.0);
        if(uMicroErosion)
        {
            vec3 fineCoords=erosionPosition/NOISE_TEXTURE_NORMALIZATION_FACTOR*microErosionScale;
            float fineNoise=1.0-textureLod(uErosionNoise,fineCoords,erosionMipOffsetValue).r;
            fineNoise=mix(0.0,fineNoise,microErosionFactor*densityErosionAO.g*0.5*cloudCoverageData.coverage);
            baseCloud=densityRemap(baseCloud,fineNoise,1.0,0.0,1.0);
        }
    }
    if(lightSampling)
    {
        baseCloud-=erosionFactor*0.1;
        if(uMicroErosion)baseCloud-=microErosionFactor*densityErosionAO.g*0.15;
    }
    properties.density=max(0.0,baseCloud)*densityMultiplier;
}

float henyeyGreenstein(float g,float cosTheta)
{
    float g2=g*g;
    return (1.0-g2)/(4.0*PI*pow(max(1.0+g2-2.0*g*cosTheta,1e-4),1.5));
}
float powderEffect(float cloudDensity,float cosAngle,float intensity)
{
    float p=1.0-exp(-cloudDensity*4.0);p=sat(p*2.0);
    return mix(1.0,mix(1.0,p,smoothstep(0.5,-0.5,cosAngle)),intensity);
}
vec2 phaseFunction(float cosAngle)
{
    vec2 phase=vec2(0.0);
    phase.x=henyeyGreenstein(FORWARD_ECCENTRICITY,cosAngle)+henyeyGreenstein(-BACKWARD_ECCENTRICITY,cosAngle);
    float gScale=MULTI_SCATTERING;
    phase.y=henyeyGreenstein(FORWARD_ECCENTRICITY*gScale,cosAngle)+henyeyGreenstein(-BACKWARD_ECCENTRICITY*gScale,cosAngle);
    return phase;
}
vec3 evaluateCelestialTransmittance(vec3 positionPS,vec3 lightDirection,vec2 phase)
{
    float totalLightDistance=0.0;vec3 transmittance=vec3(0.0);
    if(exitCloudVolume(positionPS,lightDirection,totalLightDistance))
    {
        totalLightDistance=clamp(totalLightDistance,0.0,float(NUM_LIGHT_STEPS)*LIGHT_STEP_MAXIMAL_SIZE)+5.0;
        float intervalSize=totalLightDistance/float(NUM_LIGHT_STEPS);
        float opticalDepth=0.0;
        for(int j=0;j<NUM_LIGHT_STEPS;++j)
        {
            float dist=intervalSize*(0.25+float(j));
            CloudProperties lightProperties;
            evaluateCloudProperties(positionPS+lightDirection*dist,3.0*float(j)/float(NUM_LIGHT_STEPS),0.0,true,true,lightProperties);
            opticalDepth+=lightProperties.density*lightProperties.sigmaT;
        }
        vec3 extinction=vec3(intervalSize*opticalDepth);
        for(int o=0;o<NUM_MULTI_SCATTERING_OCTAVES;++o)
        {
            float msFactor=pow(MULTI_SCATTERING,float(o));
            transmittance+=exp(-extinction*msFactor)*(phase[o]*msFactor);
        }
    }
    return transmittance;
}
// Keep the upstream-derived Sun-named helper visible for VCLOUD01 validation,
// while both celestial sources now share the same physical cloud transport.
vec3 evaluateSunTransmittance(vec3 positionPS,vec3 sunDirection,vec2 phase)
{
    return evaluateCelestialTransmittance(positionPS,sunDirection,phase);
}
vec3 evaluateMoonTransmittance(vec3 positionPS,vec3 moonDirection,vec2 phase)
{
    return evaluateCelestialTransmittance(positionPS,moonDirection,phase);
}
void evaluateCloud(CloudProperties p,vec3 rayDirection,vec3 currentPositionPS,float stepSize,inout VolumetricRayResult result)
{
    float extinction=p.density*p.sigmaT;
    float stepTransmittance=exp(-extinction*stepSize);
    float integrated=result.transmittance-result.transmittance*stepTransmittance;

    if(uSunIntensity>0.00001)
    {
        vec3 sunDirection=normalize(uSunDirection);
        float sunCosAngle=dot(rayDirection,sunDirection);
        vec2 sunPhase=phaseFunction(sunCosAngle);
        float sunPowder=powderEffect(p.density,sunCosAngle,POWDER_EFFECT_INTENSITY);
        vec3 sunTransmittance=evaluateSunTransmittance(currentPositionPS,sunDirection,sunPhase);
        result.sunScattering+=sunTransmittance*sunPowder*integrated;
    }

    if(uMoonIntensity>0.00001)
    {
        vec3 moonDirection=normalize(uMoonDirection);
        float moonCosAngle=dot(rayDirection,moonDirection);
        vec2 moonPhase=phaseFunction(moonCosAngle);
        // CELESTIAL03: real water droplets produce a much narrower forward
        // aureole around the Moon than the generic HDRP-style g=0.7 phase can
        // show at game exposure. Add a physically shaped g=0.90 lobe only to
        // lunar transport; this brightens cloud structure *around the Moon*
        // without globally turning the night deck white.
        float moonForwardAureole=henyeyGreenstein(MOON_FORWARD_ECCENTRICITY,moonCosAngle);
        moonPhase.x+=moonForwardAureole*MOON_FORWARD_AUREOLE_STRENGTH;
        moonPhase.y+=moonForwardAureole*(MOON_FORWARD_AUREOLE_STRENGTH*0.22);
        float moonPowder=powderEffect(p.density,moonCosAngle,POWDER_EFFECT_INTENSITY);
        vec3 moonTransmittance=evaluateMoonTransmittance(currentPositionPS,moonDirection,moonPhase);
        // Keep a broad higher-order fill for dense cloud interiors. Lowering the
        // density scale lets thinner cloud around the lunar disc participate,
        // matching the luminous body/halo seen in real moonlit overcast.
        float moonInteriorDensity=1.0-exp(-max(p.density,0.0)*MOON_INTERIOR_DENSITY_SCALE);
        float moonInteriorOcclusion=mix(0.68,1.0,sat(p.ambientOcclusion));
        vec3 moonInteriorFill=vec3(
            MOON_INTERIOR_SCATTERING_STRENGTH*moonInteriorDensity*moonInteriorOcclusion);
        result.moonScattering+=(moonTransmittance*moonPowder+moonInteriorFill)*integrated;
    }

    result.ambient+=p.ambientOcclusion*integrated;
    result.transmittance*=stepTransmittance;
}

vec3 physicalCelestialTransmission(vec3 positionPS,vec3 lightDirection)
{
    if(!uPbrAtmosphereValid||!uPhysicallyBasedSun)return vec3(1.0);
    float radius=max(length(positionPS),EARTH_RADIUS);
    float height=clamp(radius-EARTH_RADIUS,0.0,59990.0);
    float mu=dot(positionPS/radius,lightDirection);
    vec2 uv=vec2(mu*0.5+0.5,sqrt(height/60000.0));
    return texture(uPbrTransmittanceLut,clamp(uv,vec2(0.0),vec2(1.0))).rgb;
}

vec3 reconstructRay(vec2 uv)
{
    vec4 v=inverse(uProjection)*vec4(uv*2.0-1.0,-1.0,1.0);
    vec3 viewPos=v.xyz/max(abs(v.w),1e-8);
    return normalize((inverse(uView)*vec4(viewPos,0.0)).xyz);
}
float sceneDepthAt(vec2 uv)
{
    if(uSceneDepthSamples<=1)return texture(uSceneDepth,uv).r;
    ivec2 sizeMs=textureSize(uSceneDepthMS);ivec2 p=clamp(ivec2(uv*vec2(sizeMs)),ivec2(0),sizeMs-1);
    float d=0.0;int n=min(uSceneDepthSamples,16);for(int i=0;i<n;++i)d=max(d,texelFetch(uSceneDepthMS,p,i).r);return d;
}
vec3 reconstructRelativeAtDepth(vec2 uv,float depth)
{
    vec4 v=inverse(uProjection)*vec4(uv*2.0-1.0,depth*2.0-1.0,1.0);
    vec3 viewPos=v.xyz/max(abs(v.w),1e-8);
    return (inverse(uView)*vec4(viewPos,1.0)).xyz;
}
float perceptualTransmittance(vec3 scene,float t)
{
    float lum=luminance(scene);
    if(lum>0.0){float r=lum/(1.0+lum)*t;r=r/max(1.0-r,1e-6);float f=max(r/lum,pow(t,6.0));t=f;}
    return sat(t);
}

void main()
{
    CloudDepth=0.0;
    vec3 rayDirection=reconstructRay(vUv);
    vec3 origin=vec3(uCameraGlobal.x,EARTH_RADIUS+uCameraGlobal.y,uCameraGlobal.z);
    float entry,exitD;
    if(!intersectCloudVolume(origin,rayDirection,entry,exitD)){FragColor=vec4(0.0,0.0,0.0,1.0);return;}

    float sceneDepth=sceneDepthAt(vUv);
    bool occluded=sceneDepth>1e-6;
    float maxRayLength=MAX_SKYBOX_VOLUMETRIC_CLOUDS_DISTANCE;
    if(uLocalClouds&&occluded)
    {
        vec3 relativeHit=reconstructRelativeAtDepth(vUv,sceneDepth);
        maxRayLength=min(maxRayLength,length(relativeHit));
    }
    if(maxRayLength<entry){FragColor=vec4(0.0,0.0,0.0,1.0);return;}

    float totalDistance=min(exitD,maxRayLength)-entry;
    if(totalDistance<=0.0){FragColor=vec4(0.0,0.0,0.0,1.0);return;}
    float maxStepSize=max((CLOUD_SHELL_MAX_ALTITUDE_M-CLOUD_SHELL_MIN_ALTITUDE_M)/8.0,1.0);
    float stepS=min(totalDistance/float(PRIMARY_STEPS),maxStepSize);
    totalDistance=stepS*float(PRIMARY_STEPS);

    VolumetricRayResult result;
    result.sunScattering=vec3(0.0);result.moonScattering=vec3(0.0);result.ambient=0.0;result.transmittance=1.0;result.meanDistance=0.0;result.invalidRay=true;
    float meanDistanceDivider=0.0;
    // CLOUDURP15E6: match the upstream HDRP-derived marcher. One stochastic
    // scalar is generated per pixel/frame; the initial ray offset is that raw
    // 0..1 value (not an entire step), and the same scalar modulates only the
    // first step length. This avoids turning temporal jitter into hundreds of
    // metres of per-pixel ray displacement.
    float integrationJitter=integrationNoise();
    float currentDistance=integrationJitter;
    vec3 currentPosition=origin+(entry+currentDistance)*rayDirection;
    bool activeSampling=true;int sequentialEmptySamples=0;int currentIndex=0;
    while(currentIndex<PRIMARY_STEPS&&currentDistance<totalDistance)
    {
        float distanceFromCamera=entry+currentDistance;
        float densityAttenuation=densityFadeValue(distanceFromCamera);
        float erosionMip=erosionMipOffset(distanceFromCamera);
        if(activeSampling)
        {
            CloudProperties properties;
            evaluateCloudProperties(currentPosition,0.0,erosionMip,false,false,properties);
            properties.density*=densityAttenuation;
            if(properties.density>CLOUD_DENSITY_THRESHOLD)
            {
                float weightedDensity=result.transmittance*properties.density;
                result.meanDistance+=distanceFromCamera*weightedDensity;
                meanDistanceDivider+=weightedDensity;
                evaluateCloud(properties,rayDirection,currentPosition,stepS,result);
                if(result.transmittance<0.003){result.transmittance=0.0;break;}
                sequentialEmptySamples=0;
            }
            else ++sequentialEmptySamples;
            if(sequentialEmptySamples==EMPTY_STEPS_BEFORE_LARGE_STEPS)activeSampling=false;
            float relativeStepSize=mix(integrationJitter,1.0,sat(float(currentIndex)));
            currentPosition+=rayDirection*stepS*relativeStepSize;
            currentDistance+=stepS*relativeStepSize;
        }
        else
        {
            CloudProperties properties;
            evaluateCloudProperties(currentPosition,1.0,0.0,true,false,properties);
            properties.density*=densityAttenuation;
            if(properties.density<CLOUD_DENSITY_THRESHOLD)
            {
                currentPosition+=rayDirection*stepS*2.0;
                currentDistance+=stepS*2.0;
            }
            else
            {
                currentPosition-=rayDirection*stepS;
                currentDistance-=stepS;
                --currentIndex;activeSampling=true;sequentialEmptySamples=0;
            }
        }
        ++currentIndex;
    }

    if(meanDistanceDivider<=0.0){FragColor=vec4(0.0,0.0,0.0,1.0);return;}
    result.invalidRay=false;result.meanDistance=min(result.meanDistance/meanDistanceDivider,maxRayLength);
    vec3 meanPosition=origin+result.meanDistance*rayDirection;
    CloudCoverageData meanCoverage=getCloudCoverageData(meanPosition);
    float relativeHeight=sat(normalizedCloudHeight(meanPosition,meanCoverage.cloudType));

    vec3 sunDirection=normalize(uSunDirection);
    vec3 moonDirection=normalize(uMoonDirection);
    vec3 sunRadiance=uSunColor*PI*max(uSunIntensity,0.0)*physicalCelestialTransmission(meanPosition,sunDirection);
    vec3 moonRadiance=uMoonColor*PI*max(uMoonIntensity,0.0)*MOON_CLOUD_SCATTER_EXPOSURE*physicalCelestialTransmission(meanPosition,moonDirection);
    vec3 ambientTop=max(textureLod(uEnvironmentMap,vec3(0.0,1.0,0.0),4.0).rgb,vec3(0.0));
    vec3 ambientBottom=max(textureLod(uEnvironmentMap,vec3(0.0,-1.0,0.0),4.0).rgb,vec3(0.0));
    vec3 ambient=mix(ambientBottom,ambientTop,relativeHeight);
    // The PBSKY sky colors are a live-frame fallback for a reflection cubemap
    // that may update asynchronously; this keeps sunrise/sunset cloud ambient
    // synchronized with the current physical atmosphere without art-tint hacks.
    ambient=mix(ambient,max(mix(uSkyHorizon,uSkyZenith,relativeHeight),vec3(0.0)),0.55);
    // CELESTIAL01: Sun and Moon illuminate the same physical cloud volume
    // independently. Twilight can therefore contain both warm solar and cool
    // lunar scattering without synthesizing a single cloud-light direction.
    vec3 color=sunRadiance*result.sunScattering
        +moonRadiance*result.moonScattering
        +ambient*result.ambient;

    vec4 clip=uProjection*uView*vec4(rayDirection*result.meanDistance,1.0);
    CloudDepth=clamp(clip.z/max(abs(clip.w),1e-8)*0.5+0.5,0.0,1.0);
    float finalTransmittance=uPerceptualBlending?perceptualTransmittance(texture(uSceneColor,vUv).rgb,result.transmittance):result.transmittance;
    FragColor=vec4(max(color,vec3(0.0)),sat(finalTransmittance));
}
)glsl";

const char* kCloudCombineFragmentShader = R"glsl(#version 460 core
in vec2 vUv;out vec4 FragColor;
uniform sampler2D uCloudTexture;uniform sampler2D uSceneTexture;uniform bool uBilateral;
// CLOUDURP15E6: upstream Pass 1 + Pass 2 translation. The cloud lighting is
// upscaled, composed over the already-rendered scene, and the cloud
// transmittance is stored in alpha as the temporal mask. The temporal resolve
// therefore receives the same kind of full-resolution current buffer as
// UnityVolumetricCloudsURP's accumulation target: scene+cloud RGB, cloud T in A.
float gaussianWeight(vec2 o){const float sigma=2.75;return exp(-0.5*dot(o,o)/(sigma*sigma));}
float edgeSoftness(float transmittance)
{
    float opacity=1.0-transmittance;
    return smoothstep(0.015,0.18,opacity)*(1.0-smoothstep(0.68,0.96,opacity));
}
vec4 upscaleCloud(vec2 uv)
{
    if(!uBilateral)return texture(uCloudTexture,uv);
    vec2 lowSize=vec2(textureSize(uCloudTexture,0));vec2 texel=1.0/lowSize;
    vec2 offset=(floor(uv*lowSize)+0.5)*texel;vec4 center=texture(uCloudTexture,uv);vec4 result=vec4(0.0);float normalization=0.0;
    for(int i=-3;i<=3;++i)for(int j=-3;j<=3;++j)
    {
        vec2 o=vec2(i,j);vec4 neighbor=texture(uCloudTexture,offset+o*texel);
        float w=gaussianWeight(o);result+=neighbor*w;normalization+=w;
    }
    vec4 blurred=result/max(normalization,1e-6);
    float softness=edgeSoftness(center.a);
    vec4 cloud=mix(center,blurred,0.14+0.72*softness);
    // Transmittance uses the slightly stronger edge softening retained from the
    // existing Heritage bilateral upscale, but there is no temporal classifier.
    cloud.a=mix(center.a,blurred.a,0.20+0.72*softness);
    return cloud;
}
void main()
{
    vec4 cloud=upscaleCloud(vUv);
    vec3 scene=texture(uSceneTexture,vUv).rgb;
    FragColor=vec4(cloud.rgb+scene*cloud.a,cloud.a);
}
)glsl";

// Faithful translation of UnityVolumetricCloudsURP shader Pass 3
// "Volumetric Clouds Denoise" (itself a half-precision HDRP/Playdead-style TAA
// resolve). All prior Heritage adaptive cloud-TAA classifiers are retired.
const char* kCloudTemporalFragmentShader = R"glsl(#version 460 core
in vec2 vUv;out vec4 FragColor;
uniform sampler2D uCurrentTexture;uniform sampler2D uHistoryTexture;uniform bool uHistoryValid;
uniform sampler2D uSceneDepth;uniform sampler2DMS uSceneDepthMS;uniform int uSceneDepthSamples;uniform bool uLocalClouds;
uniform mat4 uCurrentView,uPreviousView,uCurrentProjection,uPreviousProjection;uniform vec3 uCameraDelta;
const float accumulationFactor=0.95;
float sceneDepthAt(vec2 uv)
{
    if(uSceneDepthSamples<=1)return texture(uSceneDepth,uv).r;
    ivec2 sz=textureSize(uSceneDepthMS);ivec2 p=clamp(ivec2(uv*vec2(sz)),ivec2(0),sz-1);
    float d=0.0;int n=min(uSceneDepthSamples,16);
    for(int i=0;i<n;++i)d=max(d,texelFetch(uSceneDepthMS,p,i).r);
    return d;
}
vec3 relativeAtDepth(vec2 uv,float depth)
{
    vec4 p=inverse(uCurrentProjection)*vec4(uv*2.0-1.0,depth*2.0-1.0,1.0);
    vec3 v=p.xyz/max(abs(p.w),1e-8);
    return (inverse(uCurrentView)*vec4(v,1.0)).xyz;
}
vec4 currentPoint(ivec2 p,ivec2 size)
{
    return texelFetch(uCurrentTexture,clamp(p,ivec2(0),size-1),0);
}
vec3 historyPoint(vec2 uv)
{
    ivec2 size=textureSize(uHistoryTexture,0);
    vec2 c=clamp(uv,vec2(0.0),vec2(1.0));
    ivec2 p=clamp(ivec2(c*vec2(size)),ivec2(0),size-1);
    return texelFetch(uHistoryTexture,p,0).rgb;
}
void main()
{
    ivec2 size=textureSize(uCurrentTexture,0);ivec2 px=clamp(ivec2(gl_FragCoord.xy),ivec2(0),size-1);
    vec4 cur=currentPoint(px,size);
    if(!uHistoryValid){FragColor=cur;return;}

    // CLOUDURP15E7 keeps the upstream 5-pixel neighbourhood as the sole TAA
    // authority, but evaluates RGBA instead of discarding a center pixel merely
    // because this stochastic frame happened to miss the cloud there.  Only a
    // genuinely clear 5-pixel neighbourhood bypasses history.  This specifically
    // lets temporal accumulation heal the salt-and-pepper clear holes visible at
    // sparsely sampled cloud boundaries without accumulating over empty sky.
    vec4 s1=currentPoint(px+ivec2(0,-1),size);
    vec4 s2=currentPoint(px+ivec2(-1,0),size);
    vec4 s3=currentPoint(px+ivec2(1,0),size);
    vec4 s4=currentPoint(px+ivec2(0,1),size);
    float minTransmittance=min(cur.a,min(min(s1.a,s2.a),min(s3.a,s4.a)));
    if(minTransmittance>=0.999999){FragColor=cur;return;}

    // Upstream 5-pixel current-frame colour box: center + four cardinals.
    vec3 colorCenter=cur.rgb;
    vec3 boxMin=colorCenter,boxMax=colorCenter;
    vec3 c1=s1.rgb,c2=s2.rgb,c3=s3.rgb,c4=s4.rgb;
    boxMin=min(boxMin,min(min(c1,c2),min(c3,c4)));
    boxMax=max(boxMax,max(max(c1,c2),max(c3,c4)));

    // Match the source shader: local clouds reconstruct from opaque scene depth;
    // global clouds reproject the far plane. Heritage's reversed-Z far value is 0.
    float depth=uLocalClouds?sceneDepthAt(vUv):0.0;
    vec3 rel=relativeAtDepth(vUv,depth);
    vec4 curClip=uCurrentProjection*uCurrentView*vec4(rel,1.0);
    vec4 prevClip=uPreviousProjection*uPreviousView*vec4(rel+uCameraDelta,1.0);
    vec2 curNdc=curClip.xy/max(abs(curClip.w),1e-8);
    vec2 prevNdc=prevClip.xy/max(abs(prevClip.w),1e-8);
    vec2 velocity=(prevNdc-curNdc)*0.5;
    vec2 prevUv=vUv+velocity;
    if(any(lessThan(prevUv,vec2(0.0)))||any(greaterThan(prevUv,vec2(1.0)))){FragColor=cur;return;}

    // Upstream history is point-clamped, then clipped to the current 5-pixel AABB.
    vec3 prevColor=clamp(historyPoint(prevUv),boxMin,boxMax);

    // Preserve the upstream 0.95 accumulation as the baseline for coherent cloud
    // structure.  The additional persistence is derived only from current-frame
    // high-frequency disagreement inside the SAME upstream 5-pixel neighbourhood.
    // This is not a second/legacy TAA path: it only modulates the upstream blend.
    const vec3 lumaWeights=vec3(0.2126,0.7152,0.0722);
    float l0=dot(cur.rgb,lumaWeights),l1=dot(c1,lumaWeights),l2=dot(c2,lumaWeights);
    float l3=dot(c3,lumaWeights),l4=dot(c4,lumaWeights);
    float lumaMean=(l0+l1+l2+l3+l4)*0.2;
    float lumaMin=min(l0,min(min(l1,l2),min(l3,l4)));
    float lumaMax=max(l0,max(max(l1,l2),max(l3,l4)));
    float lumaResidual=abs(l0-lumaMean)/(0.055+0.30*abs(lumaMean));
    float lumaSpread=(lumaMax-lumaMin)/(0.075+0.35*abs(lumaMean));

    float transmittanceMean=(cur.a+s1.a+s2.a+s3.a+s4.a)*0.2;
    float transmittanceMax=max(cur.a,max(max(s1.a,s2.a),max(s3.a,s4.a)));
    float transmittanceResidual=abs(cur.a-transmittanceMean);
    float transmittanceSpread=transmittanceMax-minTransmittance;

    float rgbGrain=smoothstep(0.12,0.48,lumaResidual)*smoothstep(0.12,0.60,lumaSpread);
    float alphaGrain=smoothstep(0.012,0.12,transmittanceResidual)*smoothstep(0.035,0.32,transmittanceSpread);
    float stochasticGrain=max(rgbGrain,alphaGrain);

    // Dense interiors are where over-accumulation looks waxy/cartoonish.  Keep
    // them close to the source 95% resolve while allowing exposed/partial volume
    // samples to integrate much longer.  98.5% ~= 67-frame memory; 99.75% ~=
    // 400-frame memory, but the current AABB clamp still bounds stale history.
    float exposedSample=smoothstep(0.035,0.32,transmittanceMean);
    float selectiveGrain=stochasticGrain*mix(0.18,1.0,exposedSample);
    float mildGrain=smoothstep(0.10,0.42,selectiveGrain);
    float strongGrain=smoothstep(0.48,0.88,selectiveGrain);
    float adaptiveAccumulation=mix(accumulationFactor,0.985,mildGrain);
    adaptiveAccumulation=mix(adaptiveAccumulation,0.9975,strongGrain);

    float intensity=clamp(min(accumulationFactor-abs(velocity.x)*accumulationFactor,
                              accumulationFactor-abs(velocity.y)*accumulationFactor),0.0,1.0);
    float adaptiveIntensity=clamp(min(adaptiveAccumulation-abs(velocity.x)*adaptiveAccumulation,
                                      adaptiveAccumulation-abs(velocity.y)*adaptiveAccumulation),0.0,1.0);
    intensity=max(intensity,adaptiveIntensity);

    // The Unity shader emits history RGB + intensity and lets fixed-function
    // SrcAlpha/OneMinusSrcAlpha blend it over current camera colour. Heritage
    // writes the mathematically identical resolved RGB directly and keeps the
    // current cloud transmittance in alpha for the next frame's clear-pixel mask.
    FragColor=vec4(mix(cur.rgb,prevColor,intensity),cur.a);
}
)glsl";


const char* kCloudGroundShadowFragmentShader = R"glsl(#version 460 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uSceneDepth;
uniform sampler2DMS uSceneDepthMS;
uniform int uSceneDepthSamples;
uniform sampler2D uCloudShadow;
uniform bool uCloudShadowValid;
uniform float uCloudShadowHalfRangeM;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCelestialLightDirection;
uniform float uDaylightFactor;

float sceneDepthAt(vec2 uv)
{
    if(uSceneDepthSamples<=1)return texture(uSceneDepth,uv).r;
    ivec2 sz=textureSize(uSceneDepthMS);
    ivec2 p=clamp(ivec2(uv*vec2(sz)),ivec2(0),sz-1);
    float d=0.0;int n=min(uSceneDepthSamples,16);
    for(int i=0;i<n;++i)d=max(d,texelFetch(uSceneDepthMS,p,i).r);
    return d;
}
vec3 relativeAtDepth(vec2 uv,float depth)
{
    vec4 p=inverse(uProjection)*vec4(uv*2.0-1.0,depth*2.0-1.0,1.0);
    vec3 v=p.xyz/max(abs(p.w),1e-8);
    return (inverse(uView)*vec4(v,1.0)).xyz;
}
void main()
{
    // CELESTIAL04: ground cloud shadows are a dedicated receiver pass rather
    // than an optional per-material side path.  The scene depth reconstructs
    // the opaque receiver in the same camera-relative world coordinates used
    // by the camera-centred cloud-shadow cookie.
    if(!uCloudShadowValid||uCloudShadowHalfRangeM<=1.0||uCelestialLightDirection.y<=0.01)
    {
        FragColor=vec4(1.0);return;
    }
    float depth=sceneDepthAt(vUv);
    // Heritage reversed-Z clears to zero. Sky pixels therefore remain untouched.
    if(depth<=1e-6){FragColor=vec4(1.0);return;}
    vec3 surface=relativeAtDepth(vUv,depth);
    float safeY=max(uCelestialLightDirection.y,0.06);
    vec2 receiverXZ=surface.xz-uCelestialLightDirection.xz*(surface.y/safeY);
    vec2 shadowUv=vec2(0.5)+receiverXZ/(2.0*uCloudShadowHalfRangeM);
    if(any(lessThan(shadowUv,vec2(0.0)))||any(greaterThan(shadowUv,vec2(1.0))))
    {
        FragColor=vec4(1.0);return;
    }
    float transmission=clamp(texture(uCloudShadow,shadowUv).r,0.0,1.0);
    float daylight=smoothstep(0.04,0.30,clamp(uDaylightFactor,0.0,1.0));
    // Sun shadows are stronger; moon shadows remain visible without crushing
    // the already-dark nighttime scene. Ambient sky still survives because
    // this pass intentionally applies only a bounded fraction of optical depth.
    float receiverStrength=mix(0.44,0.64,daylight);
    float luminanceTransmission=mix(1.0,transmission,receiverStrength);
    vec3 coolTint=mix(vec3(0.88,0.93,1.00),vec3(1.0),transmission);
    FragColor=vec4(clamp(coolTint*luminanceTransmission,vec3(0.0),vec3(1.0)),1.0);
}
)glsl";

const char* kCloudPresentFragmentShader = R"glsl(#version 460 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uSourceTexture;
uniform sampler2D uSceneDepth;
uniform sampler2DMS uSceneDepthMS;
uniform int uSceneDepthSamples;
uniform vec2 uSunScreenUv;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform float uSunElevation;
uniform bool uSunScreenVisible;
uniform float uCloudCover;
uniform float uHumidity;
uniform float uPrecipitation;

float sceneDepthAt(vec2 uv)
{
    if(uSceneDepthSamples<=1)
        return texture(uSceneDepth,uv).r;
    ivec2 size=textureSize(uSceneDepthMS);
    ivec2 pixel=clamp(ivec2(uv*vec2(size)),ivec2(0),size-1);
    float nearest=0.0;
    int count=min(uSceneDepthSamples,16);
    for(int sampleIndex=0;sampleIndex<count;++sampleIndex)
        nearest=max(nearest,texelFetch(uSceneDepthMS,pixel,sampleIndex).r);
    return nearest;
}

void main()
{
    vec4 resolved=texture(uSourceTexture,vUv);
    vec3 shafts=vec3(0.0);

    // CLOUDURP15AD: restore the depth-carved radial Sun shafts removed by the
    // dead-code cleanup. Clear reversed-Z samples contribute atmosphere while
    // visible depth-writing geometry interrupts it. Sampling the resolved cloud
    // colour also lets cloud gaps shape the rays without another cloud texture.
    if(uSunScreenVisible&&uSunIntensity>0.0001&&uSunElevation>-0.025)
    {
        vec2 aspect=vec2(float(textureSize(uSourceTexture,0).x)
            /float(max(textureSize(uSourceTexture,0).y,1)),1.0);
        float sunDistance=length((vUv-uSunScreenUv)*aspect);
        float radialEnvelope=1.0-smoothstep(0.035,0.72,sunDistance);
        if(radialEnvelope>0.0001)
        {
            const int sampleCount=16;
            vec2 stepUv=(uSunScreenUv-vUv)/float(sampleCount);
            vec2 sampleUv=vUv;
            float illumination=0.0;
            float decay=1.0;
            for(int sampleIndex=0;sampleIndex<sampleCount;++sampleIndex)
            {
                sampleUv+=stepUv;
                bool inside=all(greaterThanEqual(sampleUv,vec2(0.0)))
                    &&all(lessThanEqual(sampleUv,vec2(1.0)));
                if(inside)
                {
                    float clearSky=1.0-step(1.0e-6,sceneDepthAt(sampleUv));
                    vec3 sampleColor=texture(uSourceTexture,sampleUv).rgb;
                    float luminance=dot(sampleColor,vec3(0.2126,0.7152,0.0722));
                    float luminousSky=smoothstep(0.08,0.72,luminance);
                    illumination+=clearSky*(0.32+0.68*luminousSky)*decay;
                }
                decay*=0.945;
            }
            illumination/=10.55;
            float partialCloud=4.0*clamp(uCloudCover,0.0,1.0)
                *(1.0-clamp(uCloudCover,0.0,1.0));
            float haze=clamp(0.18+uHumidity*0.52
                +clamp(uPrecipitation/80.0,0.0,1.0)*0.18,0.0,1.0);
            float lowSun=mix(0.42,1.0,
                1.0-smoothstep(0.12,0.62,max(uSunElevation,0.0)));
            float strength=(0.035+0.095*partialCloud)*haze*lowSun
                *clamp(uSunIntensity/3.4,0.0,1.0);
            shafts=max(uSunColor,vec3(0.0))*illumination
                *radialEnvelope*strength;
        }
    }

    FragColor=vec4(resolved.rgb+shafts,resolved.a);
}
)glsl";

// Optional upstream OUTPUT_DEPTH_TO_SCENE_DEPTH equivalent. Heritage uses
// reversed Z, so the nearest of opaque and cloud depth is max(scene, cloud).
const char* kCloudDepthMergeFragmentShader = R"glsl(#version 460 core
in vec2 vUv;
uniform sampler2D uSceneDepth;uniform sampler2DMS uSceneDepthMS;uniform int uSceneDepthSamples;uniform sampler2D uCloudDepth;
float sceneDepthAt(vec2 uv){if(uSceneDepthSamples<=1)return texture(uSceneDepth,uv).r;ivec2 sz=textureSize(uSceneDepthMS);ivec2 p=clamp(ivec2(uv*vec2(sz)),ivec2(0),sz-1);float d=0.0;int n=min(uSceneDepthSamples,16);for(int i=0;i<n;++i)d=max(d,texelFetch(uSceneDepthMS,p,i).r);return d;}
void main(){float sceneDepth=sceneDepthAt(vUv);float cloudDepth=texture(uCloudDepth,vUv).r;gl_FragDepth=max(sceneDepth,cloudDepth);}
)glsl";

// VCLOUD01: restore the upstream 16-segment / 15-interior-sample cloud-shadow
// trace and retain the existing 3x3 sigma=.9 Gaussian filter. Heritage maps it onto
// a camera-centered ground receiver field and multiplies it with the existing
// cascaded shadow result instead of replacing the directional-light cookie.
const char* kCloudShadowFragmentShader = R"glsl(#version 460 core
in vec2 vUv;
out float FragColor;
uniform vec3 uCameraGlobal;
uniform vec3 uCelestialLightDirection;
uniform float uTime;
uniform float uCloudCover,uHumidity,uPrecipitation;
uniform vec2 uBaseWindXZ,uTopWindXZ;
uniform sampler2D uRegionalWeatherMap;
uniform bool uRegionalWeatherMapValid;
uniform vec2 uRegionalCameraOffsetXZ,uRegionalAdvectionXZ;
uniform float uRegionalHalfRange;
uniform sampler3D uShapeNoise,uErosionNoise;
uniform sampler2D uCurveLut;
uniform bool uMicroErosion;
uniform float uHalfRange;

const float EARTH_RADIUS=6378100.0;
const float CLOUD_SHELL_MIN_ALTITUDE_M=900.0;
const float CLOUD_SHELL_MAX_ALTITUDE_M=6500.0;
const float LOWEST=EARTH_RADIUS+CLOUD_SHELL_MIN_ALTITUDE_M;
const float HIGHEST=EARTH_RADIUS+CLOUD_SHELL_MAX_ALTITUDE_M;
const float NOISE_TEXTURE_NORMALIZATION_FACTOR=100000.0;
const float CLOUD_DENSITY_THRESHOLD=0.001;
const float EROSION_OCCLUSION=0.1;

struct CloudCoverageData{float coverage;float rainClouds;float cloudType;float maxCloudHeight;};
struct CloudProperties{float density;float ambientOcclusion;float height;float sigmaT;};
float sat(float x){return clamp(x,0.0,1.0);}
float densityRemap(float x,float a,float b,float c,float d){return ((x-a)/max(b-a,1e-5))*(d-c)+c;}
float presetValue(vec4 values,float cloudType){float t=sat(cloudType)*3.0;if(t<1.0)return mix(values.x,values.y,t);if(t<2.0)return mix(values.y,values.z,t-1.0);return mix(values.z,values.w,t-2.0);}
vec2 sphereRoots(float radius,vec3 origin,vec3 dir){float b=dot(origin,dir),c=dot(origin,origin)-radius*radius,disc=b*b-c;if(disc<0.0)return vec2(-1.0);float s=sqrt(disc);return vec2(-b-s,-b+s);}
vec4 regionalWeather(vec2 cameraRelativeXZ)
{
    vec4 w;
    if(!uRegionalWeatherMapValid||uRegionalHalfRange<=1.0)w=vec4(uCloudCover,sat(uPrecipitation/80.0),uHumidity,0.0);
    else
    {
        vec2 uv=vec2(0.5)+(cameraRelativeXZ+uRegionalCameraOffsetXZ+uRegionalAdvectionXZ)/(2.0*uRegionalHalfRange);
        if(any(lessThan(uv,vec2(0.0)))||any(greaterThan(uv,vec2(1.0))))w=vec4(uCloudCover,sat(uPrecipitation/80.0),uHumidity,0.0);
        else w=texture(uRegionalWeatherMap,uv);
    }
    return clamp(w,vec4(0.0),vec4(1.0));
}
CloudCoverageData getCloudCoverageData(vec3 positionPS)
{
    vec4 w=regionalWeather(positionPS.xz-uCameraGlobal.xz);
    float rainClouds=max(max(w.g,w.a),sat(uPrecipitation/80.0));
    float coverage=max(w.r,smoothstep(0.12,0.75,rainClouds)*0.35);
    CloudCoverageData d;d.coverage=sat(coverage);d.rainClouds=sat(rainClouds);d.cloudType=sat(0.05+0.45*d.coverage+0.20*w.b+0.55*d.rainClouds);d.maxCloudHeight=1.0;return d;
}
void presetLayer(float cloudType,out float bottomAltitude,out float altitudeRange){bottomAltitude=presetValue(vec4(3000.0,1200.0,1500.0,1000.0),cloudType);altitudeRange=presetValue(vec4(1000.0,2000.0,2500.0,5000.0),cloudType);}
float normalizedCloudHeight(vec3 p,float cloudType){float b,r;presetLayer(cloudType,b,r);return ((length(p)-EARTH_RADIUS)-b)/max(r,1.0);}
vec2 layerWind(float height){vec2 w=mix(uBaseWindXZ,uTopWindXZ,sat(height));return dot(w,w)>1e-5?w:vec2(0.0);}
vec3 sampleCurveLut(float cloudType,float height){float u=(0.5+3.0*sat(cloudType))/4.0;return textureLod(uCurveLut,vec2(u,sat(height)),0.0).rgb;}
void evaluateCloudProperties(vec3 positionPS,bool cheapVersion,bool lightSampling,out CloudProperties p)
{
    p.density=0.0;p.ambientOcclusion=1.0;p.height=0.0;p.sigmaT=0.04;
    CloudCoverageData c=getCloudCoverageData(positionPS);if(c.coverage<=CLOUD_DENSITY_THRESHOLD)return;
    p.height=normalizedCloudHeight(positionPS,c.cloudType);if(p.height<0.0||p.height>c.maxCloudHeight)return;
    vec2 wind=layerWind(p.height);vec2 windDir=dot(wind,wind)>1e-5?normalize(wind):vec2(-1.0,0.0);
    vec3 q=positionPS;q.y+=q.x/3.0+q.z/7.0;q+=vec3(wind.x,0.0,wind.y)*uTime;
    vec3 shape=q.xzy/NOISE_TEXTURE_NORMALIZATION_FACTOR*5.0+p.height*vec3(windDir.x,windDir.y,0.0)*0.0625;
    float low=textureLod(uShapeNoise,shape,0.0).r;vec3 lut=sampleCurveLut(c.cloudType,p.height);
    float shapePreset=presetValue(vec4(0.95,0.90,0.50,0.85),c.cloudType);float erosionPreset=presetValue(vec4(0.80,0.80,0.50,0.75),c.cloudType);float densityMultiplier=presetValue(vec4(0.32,0.32,0.18,0.245),c.cloudType);float microFactor=0.65;
    float shapeFactor=mix(0.1,1.0,shapePreset)*lut.g;float erosionFactor=erosionPreset*lut.g;low=mix(1.0,low,shapeFactor);
    float baseCloud=1.0-lut.r*c.coverage*(1.0-shapeFactor);baseCloud=sat(densityRemap(low,baseCloud,1.0,0.0,1.0))*c.coverage*c.coverage;
    p.ambientOcclusion=lut.b;p.sigmaT=mix(0.04,0.12,c.rainClouds);
    if(!cheapVersion)
    {
        vec3 ec=(positionPS+vec3(wind.x,0.0,wind.y)*uTime*1.35)/NOISE_TEXTURE_NORMALIZATION_FACTOR*107.0;
        float erosion=1.0-textureLod(uErosionNoise,ec,0.0).r;erosion=mix(0.0,erosion,erosionFactor*0.75*c.coverage);baseCloud=densityRemap(baseCloud,erosion,1.0,0.0,1.0);
        if(uMicroErosion){vec3 fc=(positionPS+vec3(wind.x,0.0,wind.y)*uTime*1.35)/NOISE_TEXTURE_NORMALIZATION_FACTOR*300.0;float fine=1.0-textureLod(uErosionNoise,fc,0.0).r;fine=mix(0.0,fine,microFactor*lut.g*0.5*c.coverage);baseCloud=densityRemap(baseCloud,fine,1.0,0.0,1.0);}
    }
    if(lightSampling){baseCloud-=erosionFactor*0.1;if(uMicroErosion)baseCloud-=microFactor*lut.g*0.15;}
    p.density=max(baseCloud,0.0)*densityMultiplier;
}
void main()
{
    // CELESTIAL01: one cloud optical-depth cookie follows Heritage's continuous
    // astronomical key light. It is Sun-directed by day, Moon-directed by night
    // and remains continuous through twilight. Trace the same 15 interior
    // samples used by VolumetricCloudsShadows.hlsl (i=1..15 of 16 segments).
    vec2 rel=(vUv-0.5)*(2.0*uHalfRange);
    vec3 rayOrigin=vec3(uCameraGlobal.x+rel.x,EARTH_RADIUS+uCameraGlobal.y,uCameraGlobal.z+rel.y);
    vec3 rayDirection=normalize(uCelestialLightDirection);
    vec2 low=sphereRoots(LOWEST,rayOrigin,rayDirection);vec2 high=sphereRoots(HIGHEST,rayOrigin,rayDirection);
    if(low.y<0.0||high.y<0.0){FragColor=1.0;return;}
    float startDistance=max(high.x,0.0);
    float endDistance=low.x>=startDistance?low.x:high.y;
    if(endDistance<=startDistance){FragColor=1.0;return;}
    float totalDistance=endDistance-startDistance;float stepSize=totalDistance/16.0;float transmittance=1.0;bool validShadow=false;
    vec3 startPoint=rayOrigin+rayDirection*startDistance;
    for(int i=1;i<16;++i)
    {
        vec3 positionPS=startPoint+rayDirection*(stepSize*float(i));CloudProperties p;evaluateCloudProperties(positionPS,true,true,p);
        if(p.density>CLOUD_DENSITY_THRESHOLD){transmittance*=exp(-p.density*p.sigmaT*stepSize);validShadow=true;}
    }
    // CELESTIAL02: the 256x256 cookie is a coarse integration of a many-km
    // cloud volume.  Scale optical depth rather than multiplying final scene
    // colour, so dense clouds remove substantially more *direct* celestial
    // light while skylight/IBL still keeps the ground naturally illuminated.
    // A super-linear transmission curve strengthens direct-light contrast while
    // the low-frequency regional floor below prevents coarse-ray false clears.
    float shadowBase=clamp(transmittance-0.02,0.0,1.0);
    float densityShadow=pow(shadowBase,2.6);

    // CELESTIAL03: the 15-sample density trace can miss a thin/eroded lobe in
    // a 5.6 km shell. The same regional field that births the visible cloud is
    // therefore a conservative low-frequency shadow floor, while the 3D trace
    // remains the detailed authority. This guarantees that a real cloud cell
    // cannot illuminate the ground as if the sky were clear merely because a
    // coarse shadow ray threaded an erosion gap.
    vec3 representativePoint=startPoint+rayDirection*(totalDistance*0.45);
    CloudCoverageData receiverCoverage=getCloudCoverageData(representativePoint);
    float coverageOcclusion=smoothstep(0.22,0.82,receiverCoverage.coverage);
    float regionalFloor=mix(1.0,mix(0.52,0.26,receiverCoverage.rainClouds),coverageOcclusion);
    float shadow=min(densityShadow,regionalFloor);
    FragColor=(validShadow||coverageOcclusion>0.001)?shadow:1.0;
}
)glsl";

const char* kCloudShadowFilterFragmentShader = R"glsl(#version 460 core
in vec2 vUv;out float FragColor;uniform sampler2D uSource;uniform vec2 uTexelSize;float g(float r){float v=r/.9;return exp(-v*v);}void main(){if(vUv.x<=uTexelSize.x||vUv.y<=uTexelSize.y||vUv.x>=1-uTexelSize.x||vUv.y>=1-uTexelSize.y){FragColor=1;return;}float s=0,w=0;for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){float ww=g(length(vec2(x,y)));s+=texture(uSource,vUv+vec2(x,y)*uTexelSize).r*ww;w+=ww;}FragColor=s/max(w,1e-6);}
)glsl";


} // namespace heritage::graphics::sky_renderer_shaders
