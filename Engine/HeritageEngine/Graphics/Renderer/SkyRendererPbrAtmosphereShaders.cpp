#include "SkyRendererShaders.hpp"

namespace heritage::graphics::sky_renderer_shaders {

#define HERITAGE_SKY_GLSL_VERSION "#version 460 core\n"

// PBSKY01 is a Heritage-native GLSL translation of the architecture used by
// jiaozi158/UnityPhysicallyBasedSkyURP (MIT): Earth-scale Rayleigh, Mie and
// ozone extinction, precomputed transmittance + multiple-scattering tables,
// and a sun-dependent sky-view LUT.  The implementation is intentionally
// native GLSL/OpenGL rather than Unity/URP macro emulation.

const char* kPbrAtmosphereTransmittanceFragmentShader = HERITAGE_SKY_GLSL_VERSION R"glsl(
in vec2 vUv;
out vec4 FragColor;
uniform float uAerosolMultiplier;

const float PI = 3.14159265358979323846;
const float PLANET_R = 6378100.0;
const float ATM_H = 60000.0;
const float ATM_R = PLANET_R + ATM_H;
const float HR = 8000.0;
const float HM = 1200.0;
const vec3 BETA_R = vec3(5.8e-6, 13.5e-6, 33.1e-6);
const vec3 BETA_O = vec3(0.65e-6, 1.88e-6, 0.08e-6);

float ozoneDensity(float h)
{
    // UnityPhysicallyBasedSkyURP's Earth preset: 20-40 km triangular ozone layer.
    return clamp(1.0 - abs((h - 30000.0) / 10000.0), 0.0, 1.0);
}
vec3 extinctionAt(float h)
{
    float r = exp(-max(h,0.0) / HR);
    float m = exp(-max(h,0.0) / HM);
    float mieExt = 10.0e-6 * max(uAerosolMultiplier, 0.0);
    return BETA_R * r + vec3(mieExt * m) + BETA_O * ozoneDensity(h);
}
vec2 raySphere(vec3 o, vec3 d, float radius)
{
    float b = dot(o,d);
    float c = dot(o,o) - radius*radius;
    float h = b*b-c;
    if(h < 0.0) return vec2(-1.0);
    h = sqrt(h);
    return vec2(-b-h,-b+h);
}
void main()
{
    vec2 uv = clamp(vUv, vec2(0.0), vec2(1.0));
    float height = uv.y*uv.y*ATM_H;
    float mu = uv.x*2.0-1.0;
    float sinTheta = sqrt(max(1.0-mu*mu,0.0));
    vec3 o = vec3(0.0, PLANET_R+height, 0.0);
    vec3 d = vec3(sinTheta, mu, 0.0);

    vec2 topHit = raySphere(o,d,ATM_R);
    float tMax = max(topHit.y,0.0);
    vec2 groundHit = raySphere(o,d,PLANET_R);
    if(groundHit.x > 0.0 && groundHit.x < tMax)
    {
        FragColor = vec4(0.0,0.0,0.0,1.0);
        return;
    }

    vec3 opticalDepth = vec3(0.0);
    const int N=40;
    for(int i=0;i<N;++i)
    {
        float a=float(i)/float(N);
        float b=float(i+1)/float(N);
        // Quadratic spacing preserves the near-ground density gradient.
        float t0=a*a*tMax;
        float t1=b*b*tMax;
        float tm=0.5*(t0+t1);
        float dt=t1-t0;
        float h=max(length(o+d*tm)-PLANET_R,0.0);
        opticalDepth += extinctionAt(h)*dt;
    }
    FragColor=vec4(exp(-opticalDepth),1.0);
}
)glsl";

const char* kPbrAtmosphereMultiScatteringFragmentShader = HERITAGE_SKY_GLSL_VERSION R"glsl(
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTransmittanceLut;
uniform float uAerosolMultiplier;

const float PI=3.14159265358979323846;
const float PLANET_R=6378100.0;
const float ATM_H=60000.0;
const float ATM_R=PLANET_R+ATM_H;
const float HR=8000.0;
const float HM=1200.0;
const vec3 BETA_R=vec3(5.8e-6,13.5e-6,33.1e-6);
const vec3 BETA_O=vec3(0.65e-6,1.88e-6,0.08e-6);

float ozoneDensity(float h){return clamp(1.0-abs((h-30000.0)/10000.0),0.0,1.0);}
void mediumAt(float h,out vec3 sigmaS,out vec3 sigmaE)
{
    float dr=exp(-max(h,0.0)/HR);
    float dm=exp(-max(h,0.0)/HM);
    float mieExt=10.0e-6*max(uAerosolMultiplier,0.0);
    vec3 ray=BETA_R*dr;
    vec3 mieS=vec3(mieExt*0.90*dm);
    sigmaS=ray+mieS;
    sigmaE=ray+vec3(mieExt*dm)+BETA_O*ozoneDensity(h);
}
vec2 raySphere(vec3 o,vec3 d,float radius)
{
    float b=dot(o,d),c=dot(o,o)-radius*radius,h=b*b-c;
    if(h<0.0)return vec2(-1.0);h=sqrt(h);return vec2(-b-h,-b+h);
}
vec3 transToSun(float h,float mu)
{
    vec2 uv=vec2(mu*0.5+0.5,sqrt(clamp(h/ATM_H,0.0,1.0)));
    return texture(uTransmittanceLut,uv).rgb;
}
void main()
{
    vec2 uv=clamp(vUv,vec2(0.0),vec2(1.0));
    float muS=uv.x*2.0-1.0;
    float height=uv.y*uv.y*ATM_H;
    vec3 o=vec3(0.0,PLANET_R+height,0.0);
    vec3 up=normalize(o);
    vec3 sunDir=normalize(vec3(sqrt(max(1.0-muS*muS,0.0)),muS,0.0));

    vec3 luminance=vec3(0.0);
    vec3 scatterFraction=vec3(0.0);
    const int DIRS=64;
    const int STEPS=16;
    const float golden=2.39996322972865332;
    for(int j=0;j<DIRS;++j)
    {
        float y=1.0-2.0*(float(j)+0.5)/float(DIRS);
        float rxy=sqrt(max(1.0-y*y,0.0));
        float phi=golden*float(j);
        vec3 d=normalize(vec3(cos(phi)*rxy,y,sin(phi)*rxy));
        vec2 top=raySphere(o,d,ATM_R);
        float tMax=max(top.y,0.0);
        vec2 ground=raySphere(o,d,PLANET_R);
        bool hitGround=ground.x>0.0&&ground.x<tMax;
        if(hitGround)tMax=ground.x;

        vec3 throughput=vec3(1.0);
        vec3 L=vec3(0.0),F=vec3(0.0);
        for(int i=0;i<STEPS;++i)
        {
            float t0=float(i)/float(STEPS)*tMax;
            float t1=float(i+1)/float(STEPS)*tMax;
            float tm=0.5*(t0+t1),dt=t1-t0;
            vec3 p=o+d*tm;
            float rr=max(length(p),PLANET_R);
            float h=max(rr-PLANET_R,0.0);
            vec3 n=p/rr;
            vec3 sigmaS,sigmaE;mediumAt(h,sigmaS,sigmaE);
            vec3 segT=exp(-sigmaE*dt);
            vec3 integral=(vec3(1.0)-segT)/max(sigmaE,vec3(1.0e-8));
            vec3 sunT=transToSun(h,dot(n,sunDir));
            vec3 source=sunT*sigmaS*(1.0/(4.0*PI));
            L += throughput*source*integral;
            F += throughput*sigmaS*integral;
            throughput*=segT;
        }
        luminance+=L;
        scatterFraction+=F;
    }
    luminance/=float(DIRS);
    scatterFraction/=float(DIRS);
    // Geometric-series closure used by modern real-time atmosphere models.
    vec3 multiple=luminance/max(vec3(1.0)-0.65*scatterFraction,vec3(0.08));
    FragColor=vec4(max(multiple,vec3(0.0)),1.0);
}
)glsl";

const char* kPbrAtmosphereSkyViewFragmentShader = HERITAGE_SKY_GLSL_VERSION R"glsl(
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTransmittanceLut;
uniform sampler2D uMultiScatteringLut;
uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform float uCameraAltitudeM;
uniform float uAerosolMultiplier;

const float PI=3.14159265358979323846;
const float PLANET_R=6378100.0;
const float ATM_H=60000.0;
const float ATM_R=PLANET_R+ATM_H;
const float HR=8000.0;
const float HM=1200.0;
const vec3 BETA_R=vec3(5.8e-6,13.5e-6,33.1e-6);
const vec3 BETA_O=vec3(0.65e-6,1.88e-6,0.08e-6);

float ozoneDensity(float h){return clamp(1.0-abs((h-30000.0)/10000.0),0.0,1.0);}
void mediumAt(float h,out vec3 rayS,out vec3 mieS,out vec3 sigmaE)
{
    float dr=exp(-max(h,0.0)/HR);
    float dm=exp(-max(h,0.0)/HM);
    float mieExt=10.0e-6*max(uAerosolMultiplier,0.0);
    rayS=BETA_R*dr;
    mieS=vec3(mieExt*0.90*dm);
    sigmaE=rayS+vec3(mieExt*dm)+BETA_O*ozoneDensity(h);
}
vec2 raySphere(vec3 o,vec3 d,float radius)
{
    float b=dot(o,d),c=dot(o,o)-radius*radius,h=b*b-c;
    if(h<0.0)return vec2(-1.0);h=sqrt(h);return vec2(-b-h,-b+h);
}
vec3 transToSun(float h,float mu)
{
    vec2 uv=vec2(mu*0.5+0.5,sqrt(clamp(h/ATM_H,0.0,1.0)));
    return texture(uTransmittanceLut,uv).rgb;
}
vec3 multiScatter(float h,float mu)
{
    vec2 uv=vec2(mu*0.5+0.5,sqrt(clamp(h/ATM_H,0.0,1.0)));
    return texture(uMultiScatteringLut,uv).rgb;
}
float rayleighPhase(float mu){return 3.0*(1.0+mu*mu)/(16.0*PI);}
float miePhase(float mu)
{
    const float g=0.80;
    float g2=g*g;
    float denom=max(1.0+g2-2.0*g*mu,1.0e-4);
    return 3.0*(1.0-g2)*(1.0+mu*mu)/(8.0*PI*(2.0+g2)*pow(denom,1.5));
}
vec3 unmapSkyView(vec2 uv)
{
    float remapped=1.0-uv.y*uv.y;
    float cosChi=clamp(cos(remapped*0.5*PI),0.0,1.0);
    float sinChi=sqrt(max(1.0-cosChi*cosChi,0.0));
    float phi=2.0*PI*uv.x;
    return vec3(sinChi*cos(phi),cosChi,sinChi*sin(phi));
}
void main()
{
    vec2 uv=clamp(vUv,vec2(0.0),vec2(1.0));
    vec3 V=normalize(unmapSkyView(uv));
    float cameraAltitude=clamp(uCameraAltitudeM,0.0,ATM_H-10.0);
    vec3 O=vec3(0.0,PLANET_R+cameraAltitude,0.0);
    vec2 top=raySphere(O,V,ATM_R);
    float tMax=max(top.y,0.0);
    vec2 ground=raySphere(O,V,PLANET_R);
    if(ground.x>0.0&&ground.x<tMax)tMax=ground.x;

    vec3 L=normalize(uSunDirection);
    float mu=dot(V,L);
    vec3 radiance=vec3(0.0);
    vec3 throughput=vec3(1.0);
    const int N=16;
    for(int i=0;i<N;++i)
    {
        float a=float(i)/float(N),b=float(i+1)/float(N);
        float t0=a*a*tMax,t1=b*b*tMax;
        float tm=0.5*(t0+t1),dt=t1-t0;
        vec3 P=O+V*tm;
        float rr=max(length(P),PLANET_R);
        vec3 Np=P/rr;
        float h=max(rr-PLANET_R,0.0);
        vec3 rayS,mieS,sigmaE;mediumAt(h,rayS,mieS,sigmaE);
        vec3 segT=exp(-sigmaE*dt);
        vec3 integral=(vec3(1.0)-segT)/max(sigmaE,vec3(1.0e-8));
        float muS=dot(Np,L);
        vec3 sunT=transToSun(h,muS);
        vec3 ms=multiScatter(h,muS);
        vec3 source=sunT*(rayS*rayleighPhase(mu)+mieS*miePhase(mu));
        source+=(rayS+mieS)*ms*0.72;
        radiance+=throughput*source*integral;
        throughput*=segT;
    }

    // Unity's recommendation starts around intensity 3.03. Heritage's
    // astronomical sun already spans approximately 0..3.4, so this factor maps
    // the dimensionless scattering integral into the engine's HDR sky range.
    // Heritage's legacy sunColor contains artistic dawn/sunset tinting.  The
    // physical atmosphere must not tint that spectrum a second time; molecular
    // and aerosol extinction generate the warm low-Sun colour themselves.
    vec3 solarRadiance=vec3(max(uSunIntensity,0.0)*18.0);
    FragColor=vec4(max(radiance*solarRadiance,vec3(0.0)),1.0);
}
)glsl";

const char* kPbrSkyFragmentShader = HERITAGE_SKY_GLSL_VERSION R"glsl(
in vec3 vDirection;
uniform samplerCube uEnvironmentMap;
uniform sampler2D uMoonTexture;
uniform sampler2D uStarMapTexture;
uniform sampler2D uPbrSkyViewLut;
uniform sampler2D uPbrTransmittanceLut;
uniform int uPbrSkyValid;
uniform float uPbrCameraAltitudeM;
uniform float uGamma;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform float uDaylightFactor;
uniform vec3 uSkyHorizon;
uniform vec3 uSkyZenith;
uniform float uSkyExposure;
uniform float uAtmosphereThickness;
uniform float uStarIntensity;
uniform vec3 uWorldToCelestialRow0;
uniform vec3 uWorldToCelestialRow1;
uniform vec3 uWorldToCelestialRow2;
uniform vec3 uMoonDirection;
uniform float uMoonIntensity;
uniform float uMoonPhase;
uniform float uWeatherCloudCover;
uniform float uWeatherHumidity;
uniform float uWeatherPrecipitationMmPerHour;
out vec4 FragColor;

const float PI=3.14159265358979323846;
const float ATM_H=60000.0;
float sat(float x){return clamp(x,0.0,1.0);}
vec2 mapSkyView(vec3 V)
{
    float cosChi=clamp(V.y,0.0,1.0);
    float coord=acos(cosChi)/(0.5*PI);
    float y=sqrt(max(1.0-coord,0.0));
    float phi=atan(V.z,V.x);
    float x=fract(phi/(2.0*PI));
    return vec2(x,y);
}
vec3 atmosphereTransmission(vec3 direction)
{
    float h=sqrt(clamp(uPbrCameraAltitudeM/ATM_H,0.0,1.0));
    float mu=clamp(direction.y,-1.0,1.0)*0.5+0.5;
    return texture(uPbrTransmittanceLut,vec2(mu,h)).rgb;
}
void main()
{
    vec3 direction=normalize(vDirection);
    vec3 lookupDir=normalize(vec3(direction.x,max(direction.y,0.0),direction.z));
    vec3 physicalSky=texture(uPbrSkyViewLut,mapSkyView(lookupDir)).rgb;
    vec3 fallback=mix(uSkyHorizon,uSkyZenith,pow(max(direction.y,0.0),0.45));
    // CELESTIAL10: keep the authored twilight fallback authoritative until the
    // physically based sky has real atmospheric solar scattering to contribute.
    // CELESTIAL06 previously let PBSKY approach full authority while its
    // ground-clamped Sun intensity was still zero, creating a black/haze-free
    // notch immediately before dawn (and the symmetric dusk notch).
    float physicalSkyAuthority=smoothstep(0.035,0.220,uDaylightFactor);
    vec3 color=(uPbrSkyValid!=0)?mix(fallback,physicalSky,physicalSkyAuthority):fallback;

    // Below the geometric horizon only a narrow atmospheric shoulder remains;
    // opaque world geometry normally covers this region.
    if(direction.y<0.0)
    {
        vec2 horizontal=direction.xz;
        float horizontalLength=max(length(horizontal),1.0e-5);
        horizontal/=horizontalLength;
        vec3 horizonDir=vec3(horizontal.x,0.0,horizontal.y);
        vec3 physicalHorizon=texture(uPbrSkyViewLut,mapSkyView(horizonDir)).rgb;
        vec3 horizon=(uPbrSkyValid!=0)?mix(uSkyHorizon,physicalHorizon,physicalSkyAuthority):uSkyHorizon;
        color=mix(horizon,vec3(0.018,0.022,0.030),smoothstep(0.0,0.18,-direction.y));
    }

    // Resolved solar disk plus the authored local optical presentation preserved
    // by CLOUDURP15AC..15AH. The physical sky-view LUT still owns atmospheric
    // scattering; these compact lobes restore the deliberately luminous body,
    // tight halo and softened procedural starburst that cleanup removed.
    vec3 sunDir=normalize(uSunDirection);
    float sunDot=dot(direction,sunDir);
    const float sunAngularRadius=0.00465;
    float disk=smoothstep(cos(sunAngularRadius*1.10),cos(sunAngularRadius*0.82),sunDot);
    float sunVisibility=smoothstep(-0.02,0.012,sunDir.y);
    vec3 sunTransmission=atmosphereTransmission(sunDir);
    float sunAngle=sqrt(max(2.0*(1.0-max(sunDot,0.0)),0.0));
    float lowSun=1.0-smoothstep(0.055,0.34,max(sunDir.y,0.0));
    vec3 sunPresentationTint=mix(vec3(1.035,1.015,0.975),
        vec3(1.30,0.33,0.055),lowSun);
    vec3 sunPresentationColor=sunTransmission*sunPresentationTint;
    float sunPower=max(uSunIntensity,0.0)*sunVisibility;
    float tightHalo=exp(-pow(sunAngle/0.0092,1.55));
    float softShoulder=exp(-pow(sunAngle/0.0165,2.0));

    vec3 sunReferenceUp=abs(sunDir.y)>0.98?vec3(1,0,0):vec3(0,1,0);
    vec3 sunRight=normalize(cross(sunReferenceUp,sunDir));
    vec3 sunUp=normalize(cross(sunDir,sunRight));
    vec2 sunLocal=vec2(dot(direction,sunRight),dot(direction,sunUp));
    vec2 sunDiagonal=vec2(sunLocal.x+sunLocal.y,sunLocal.x-sunLocal.y)*0.70710678;
    float axialSpikes=
        exp(-abs(sunLocal.y)/0.00062)*exp(-abs(sunLocal.x)/0.030)
        +exp(-abs(sunLocal.x)/0.00062)*exp(-abs(sunLocal.y)/0.030);
    float diagonalSpikes=
        exp(-abs(sunDiagonal.y)/0.00082)*exp(-abs(sunDiagonal.x)/0.020)
        +exp(-abs(sunDiagonal.x)/0.00082)*exp(-abs(sunDiagonal.y)/0.020);
    float starburst=(axialSpikes+0.62*diagonalSpikes)
        *exp(-sunAngle/0.034);
    color+=sunPresentationColor*sunPower
        *(25.0*disk+2.55*tightHalo+0.72*softShoulder+0.085*starburst);

    // Preserve Heritage's astronomically oriented HDR star map. PBSKY01A keeps
    // CLOUDURP15P's useful peak-only stellar micro-bloom, but extinction is now
    // owned by the physical atmosphere rather than the retired artistic sky.
    if(uStarIntensity>0.001&&direction.y>-0.02)
    {
        vec3 equatorial=normalize(vec3(dot(uWorldToCelestialRow0,direction),dot(uWorldToCelestialRow1,direction),dot(uWorldToCelestialRow2,direction)));
        float ra=atan(equatorial.y,equatorial.x);
        float dec=asin(clamp(equatorial.z,-1.0,1.0));
        vec2 starUv=vec2(fract(0.5-ra/(2.0*PI)),clamp(0.5-dec/PI,0.0,1.0));
        vec3 starRadiance=texture(uStarMapTexture,starUv).rgb;
        float horizonVisibility=smoothstep(-0.01,0.08,direction.y);
        vec3 starTransmission=atmosphereTransmission(direction);

        float starLuminance=dot(starRadiance,vec3(0.2126,0.7152,0.0722));
        float stellarPeak=smoothstep(0.38,1.35,starLuminance);
        vec3 emissiveStarRadiance=starRadiance*(1.0+stellarPeak*0.075);
        ivec2 starTextureSize=max(textureSize(uStarMapTexture,0),ivec2(1));
        vec2 starTexel=1.0/vec2(starTextureSize);
        vec2 starFootprint=min(fwidth(starUv),starTexel*6.0);
        vec2 bloomStep=max(starTexel,starFootprint)*1.10;
        bloomStep.x/=max(cos(dec),0.28);
        vec3 bloomSamples[4]=vec3[4](
            texture(uStarMapTexture,starUv+vec2( bloomStep.x,0.0)).rgb,
            texture(uStarMapTexture,starUv+vec2(-bloomStep.x,0.0)).rgb,
            texture(uStarMapTexture,starUv+vec2(0.0, bloomStep.y)).rgb,
            texture(uStarMapTexture,starUv+vec2(0.0,-bloomStep.y)).rgb);
        vec3 stellarBloom=vec3(0.0);
        for(int i=0;i<4;++i)
        {
            float sampleLuminance=dot(bloomSamples[i],vec3(0.2126,0.7152,0.0722));
            float samplePeak=smoothstep(0.45,1.45,sampleLuminance);
            float sourceScale=min(1.0,1.80/max(sampleLuminance,0.0001));
            stellarBloom+=bloomSamples[i]*samplePeak*sourceScale;
        }
        stellarBloom*=0.25*0.026;
        // CELESTIAL09: rural night visibility. Extinction still comes from the
        // atmosphere LUT, but the HDR plate should remain legible once solar
        // air-light is gone instead of being visually buried by the night sky.
        float nightStarBoost=mix(1.22,1.0,smoothstep(0.02,0.18,uDaylightFactor));
        color+=(emissiveStarRadiance+stellarBloom)*uStarIntensity*nightStarBoost*horizonVisibility*starTransmission;
    }

    // Keep the authored Moon texture and restore CLOUDURP15O's three persistent
    // optical halo lobes. The final 15Y..15AB tuning made the middle lobe tight,
    // tied the outer radius to ten times that middle radius, and left the outer
    // veil broad but very faint. Humidity, precipitation and low altitude make
    // all three more apparent without turning this into a whole-scene bloom.
    vec3 moonDir=normalize(uMoonDirection);
    float moonDot=max(dot(direction,moonDir),0.0);
    float moonVisibility=sat(uMoonIntensity/0.54);
    vec3 moonTransmission=atmosphereTransmission(moonDir);
    float moonTrans=dot(moonTransmission,vec3(0.2126,0.7152,0.0722));
    float moonAngle=sqrt(max(2.0*(1.0-moonDot),0.0));
    float moonLowAltitude=1.0-smoothstep(0.07,0.42,max(moonDir.y,0.0));
    float moonWeatherHalo=clamp(0.18+uWeatherHumidity*0.58
        +clamp(uWeatherPrecipitationMmPerHour/80.0,0.0,1.0)*0.34
        +moonLowAltitude*0.30,0.12,1.25);
    float moonNear=exp(-pow(moonAngle/0.0135,2.0))*0.105;
    const float moonMiddleRadius=0.021;
    float moonMiddle=exp(-pow(moonAngle/moonMiddleRadius,2.0))*0.043;
    float moonOuter=exp(-pow(moonAngle/(moonMiddleRadius*10.0),2.0))*0.0042;
    // CELESTIAL05: low Moon altitude may attenuate brightness, but it must not
    // reuse a sunset-like orange presentation. Keep the lunar halo neutral/cool.
    vec3 moonHaloColor=vec3(0.91,0.95,1.00);
    color+=moonTransmission*moonHaloColor
        *(moonNear+moonMiddle+moonOuter)*moonVisibility*moonWeatherHalo;
    vec3 refUp=abs(moonDir.y)>0.98?vec3(1,0,0):vec3(0,1,0);
    vec3 moonRight=normalize(cross(refUp,moonDir));
    vec3 moonUp=normalize(cross(moonDir,moonRight));
    vec2 local=vec2(dot(direction,moonRight),dot(direction,moonUp))/0.0100;
    float r2=dot(local,local);

    // Physical radiance remains HDR until the same display controls used by the
    // rest of Heritage's sky path are applied.
    color=color/(color+vec3(1.0));
    color=pow(clamp(color,0.0,1.0),vec3(1.0/max(uGamma,0.01)));
    color=(color-0.5)*uContrast+0.5+uBrightness;
    float l=dot(color,vec3(0.2126,0.7152,0.0722));
    color=mix(vec3(l),color,uSaturation);

    // Full moon is composited AFTER tone mapping for Moon.png texture clarity.
    // PBSKY01A keeps CLOUDURP15J7/CLOUDURP15O's local Moon ownership while the
    // halo and extinction remain physically integrated above.
    if(r2<=1.0&&moonDot>0.0&&moonVisibility>0.0001)
    {
        vec4 texel=texture(uMoonTexture,local*0.5+0.5);
        float edge=1.0-smoothstep(0.86,1.0,r2);
        float a=sat(texel.a*edge*moonVisibility*moonTrans);
        vec3 moonDisplay=pow(max(texel.rgb,vec3(0.0)),vec3(0.92));
        color=mix(color,moonDisplay,a);
    }
    FragColor=vec4(clamp(color,0.0,1.0),1.0);
}
)glsl";

} // namespace heritage::graphics::sky_renderer_shaders
