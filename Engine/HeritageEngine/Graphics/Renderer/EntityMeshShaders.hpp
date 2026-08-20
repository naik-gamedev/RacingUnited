#pragma once

// CLEAN05: embedded shader ownership remains compile-time C++ until Heritage has
// a real shader asset/deployment pipeline. Keeping it here avoids mixing large GLSL
// programs with mesh cache, animation, shadow-resource and draw orchestration code.

#ifdef _WIN32
#define HERITAGE_MESH_GLSL_VERSION "#version 460 core\n"
#else
#define HERITAGE_MESH_GLSL_VERSION "#version 330 core\n"
#endif

namespace heritage::graphics::entity_mesh_shaders {

inline const char* kVertexShader = HERITAGE_MESH_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
layout(location=3) in vec4 aTangent;
layout(location=4) in vec4 aJoints;
layout(location=5) in vec4 aWeights;
layout(location=6) in vec4 aColor;

const int HERITAGE_MAX_JOINTS = 128;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform bool uUseSkinning;
uniform mat4 uJointMatrices[HERITAGE_MAX_JOINTS];

// Sole tire presentation authority: a final native flexible-ring field.
uniform bool uTireVisualEnabled;
uniform vec3 uTireVisualCenter;
uniform int uTireVisualAxleAxis;
uniform float uTireVisualHalfWidth;
uniform float uTireVisualInnerRadius;
uniform float uTireVisualOuterRadius;
uniform float uTireReferenceRadiusM;
uniform vec3 uTireWheelForwardWorld;
uniform vec3 uTireWheelRightWorld;
uniform vec3 uTireWheelUpWorld;
const int HERITAGE_TIRE_FIELD_STATIONS = 24;
const int HERITAGE_TIRE_FIELD_BANDS = 13;
const int HERITAGE_TIRE_FIELD_COUNT = 312;
uniform bool uTireVisualDeformationFieldValid;
uniform vec3 uTireVisualDisplacementM[HERITAGE_TIRE_FIELD_COUNT];
// Persistent TIRE19 state. Pass 0 draws the surviving carcass; pass 1 draws
// only the same authored vertices belonging to the tethered/departing belt.
uniform int uTireFailureStage;
uniform float uTireFailureTreadAttachment;
uniform float uTireFailureStructuralIntegrity;
uniform float uTireFailureEventSeed;
uniform float uTireFailureEventAgeSeconds;
uniform float uTireFailureWheelAngularVelocity;
uniform float uTireFailureWheelRotationRadians;
uniform int uTireFailureRenderPass;
uniform bool uTireProbeDebugVisible;

out vec3 vNormal;
out vec3 vWorldPosition;
out vec2 vTexCoord;
out vec3 vTangent;
out float vTangentSign;
out vec4 vColor;
out float vViewDepth;
out vec3 vTireProbeDebugColor;
out float vTireProbeDebugMask;
out vec3 vTireFailureCoordinates;

const float HERITAGE_PI = 3.14159265358979323846;

vec3 tireAxisVector(int axis)
{
    if (axis == 1) return vec3(0.0, 1.0, 0.0);
    if (axis == 2) return vec3(0.0, 0.0, 1.0);
    return vec3(1.0, 0.0, 0.0);
}

vec3 tireRestDown(int axis)
{
    // Heritage/Blender vehicle authoring is Y-up. Only a pathological tire
    // whose axle itself is Y requires a fallback radial down axis.
    return axis == 1 ? vec3(0.0, 0.0, -1.0) : vec3(0.0, -1.0, 0.0);
}

vec3 rotateAroundAxis(vec3 value, vec3 axis, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return value * c + cross(axis, value) * s
        + axis * dot(axis, value) * (1.0 - c);
}


float tireFieldWidthCoordinate(float widthCoordinate)
{
    const float widths[HERITAGE_TIRE_FIELD_BANDS] = float[](
        -1.00, -0.82, -0.65, -0.49, -0.34, -0.18, 0.00,
         0.18,  0.34,  0.49,  0.65,  0.82, 1.00);
    float w = clamp(widthCoordinate, -1.0, 1.0);
    for (int band = 0; band < HERITAGE_TIRE_FIELD_BANDS - 1; ++band)
    {
        if (w <= widths[band + 1] || band == HERITAGE_TIRE_FIELD_BANDS - 2)
        {
            return float(band) + clamp(
                (w - widths[band])
                    / max(widths[band + 1] - widths[band], 1.0e-6),
                0.0, 1.0);
        }
    }
    return float(HERITAGE_TIRE_FIELD_BANDS - 1);
}

vec3 tireFieldControl(int station, int band)
{
    station = (station + HERITAGE_TIRE_FIELD_STATIONS)
        % HERITAGE_TIRE_FIELD_STATIONS;
    band = clamp(band, 0, HERITAGE_TIRE_FIELD_BANDS - 1);
    int index = station * HERITAGE_TIRE_FIELD_BANDS + band;
    return uTireVisualDisplacementM[index];
}

vec3 sampleTireFlexibleRingField(float theta, float widthCoordinate)
{
    float wrappedTheta = mod(theta + 2.0 * HERITAGE_PI, 2.0 * HERITAGE_PI);
    float stationCoordinate = wrappedTheta * float(HERITAGE_TIRE_FIELD_STATIONS)
        / (2.0 * HERITAGE_PI);
    int station0 = int(floor(stationCoordinate)) % HERITAGE_TIRE_FIELD_STATIONS;
    int station1 = (station0 + 1) % HERITAGE_TIRE_FIELD_STATIONS;
    float stationT = fract(stationCoordinate);
    stationT = stationT * stationT * (3.0 - 2.0 * stationT);

    float bandCoordinate = tireFieldWidthCoordinate(widthCoordinate);
    int band0 = min(int(floor(bandCoordinate)), HERITAGE_TIRE_FIELD_BANDS - 2);
    int band1 = band0 + 1;
    float bandT = bandCoordinate - float(band0);
    bandT = bandT * bandT * (3.0 - 2.0 * bandT);
    return mix(
        mix(tireFieldControl(station0, band0),
            tireFieldControl(station0, band1), bandT),
        mix(tireFieldControl(station1, band0),
            tireFieldControl(station1, band1), bandT),
        stationT);
}

vec3 tireFlexibleRingDisplacementLocal(vec3 position)
{
    if (!uTireVisualDeformationFieldValid)
        return vec3(0.0);

    vec3 axle = tireAxisVector(uTireVisualAxleAxis);
    vec3 relative = position - uTireVisualCenter;
    float axial = dot(relative, axle);
    vec3 radial = relative - axle * axial;
    float radius = length(radial);
    float radialSpan = max(
        uTireVisualOuterRadius - uTireVisualInnerRadius, 0.0001);
    if (radius <= 0.000001 || uTireVisualOuterRadius <= 0.0001)
        return vec3(0.0);

    mat3 worldToLocal = inverse(mat3(uModel));
    vec3 forward = worldToLocal * uTireWheelForwardWorld;
    forward -= axle * dot(forward, axle);
    if (dot(forward, forward) < 0.000001)
        forward = normalize(cross(tireRestDown(uTireVisualAxleAxis), axle));
    else
        forward = normalize(forward);

    vec3 lateral = worldToLocal * uTireWheelRightWorld;
    lateral -= forward * dot(lateral, forward);
    if (dot(lateral, lateral) < 0.000001)
        lateral = axle;
    else
        lateral = normalize(lateral);
    // Use the authoritative suspension up vector instead of deriving height from
    // a cross product. Mirrored left/right wheel nodes reverse that product's
    // handedness even though their physical up direction remains identical.
    vec3 up = worldToLocal * uTireWheelUpWorld;
    up -= forward * dot(up, forward);
    up -= lateral * dot(up, lateral);
    vec3 down = dot(up, up) > 0.000001
        ? -normalize(up)
        : normalize(cross(forward, lateral));

    vec3 radialDirection = radial / radius;
    float theta = atan(
        dot(radialDirection, down),
        dot(radialDirection, forward));
    float widthCoordinate = clamp(
        dot(relative, lateral) / max(uTireVisualHalfWidth, 0.0001),
        -1.0, 1.0);
    vec3 fieldM = sampleTireFlexibleRingField(theta, widthCoordinate);

    // The bead remains fixed to the rim. This interpolates the one solved
    // field through carcass depth; it does not introduce another shape model.
    float radialFraction = clamp(
        (radius - uTireVisualInnerRadius) / radialSpan, 0.0, 1.0);
    float carcassAttachment = smoothstep(0.015, 0.62, radialFraction);
    float metersToLocal = uTireVisualOuterRadius
        / max(uTireReferenceRadiusM, 0.02);
    return metersToLocal * carcassAttachment
        * (forward * fieldM.x + down * fieldM.y + lateral * fieldM.z);
}

vec4 tireProbeDebugOverlay(vec3 position)
{
    if (!uTireProbeDebugVisible || !uTireVisualEnabled)
        return vec4(0.0);
    vec3 displacement = tireFlexibleRingDisplacementLocal(position);
    float magnitudeM = length(displacement)
        * max(uTireReferenceRadiusM, 0.02)
        / max(uTireVisualOuterRadius, 0.0001);
    vec3 color = mix(vec3(0.05, 0.75, 1.0), vec3(1.0, 0.05, 0.02),
        smoothstep(0.004, 0.030, magnitudeM));
    return vec4(color, smoothstep(0.0001, 0.0010, magnitudeM) * 0.92);
}

void applyTireVisualDeformation(inout vec3 position, inout vec3 normal)
{
    if (!uTireVisualEnabled)
        return;
    position += tireFlexibleRingDisplacementLocal(position);
}

float tireWrappedAngle(float value)
{
    return mod(value + HERITAGE_PI, 2.0 * HERITAGE_PI) - HERITAGE_PI;
}

vec3 tireFailureCoordinates(vec3 position)
{
    if (!uTireVisualEnabled)
        return vec3(0.0);
    vec3 axle = tireAxisVector(uTireVisualAxleAxis);
    vec3 relative = position - uTireVisualCenter;
    float axial = dot(relative, axle);
    vec3 radial = relative - axle * axial;
    float radius = length(radial);
    if (radius <= 0.000001)
        return vec3(0.0);

    mat3 worldToLocal = inverse(mat3(uModel));
    vec3 forward = worldToLocal * uTireWheelForwardWorld;
    forward -= axle * dot(forward, axle);
    forward = dot(forward, forward) > 0.000001
        ? normalize(forward)
        : normalize(cross(tireRestDown(uTireVisualAxleAxis), axle));
    vec3 lateral = worldToLocal * uTireWheelRightWorld;
    lateral -= forward * dot(lateral, forward);
    lateral = dot(lateral, lateral) > 0.000001
        ? normalize(lateral) : axle;
    vec3 up = worldToLocal * uTireWheelUpWorld;
    up -= forward * dot(up, forward);
    up -= lateral * dot(up, lateral);
    vec3 down = dot(up, up) > 0.000001
        ? -normalize(up) : normalize(cross(forward, lateral));
    vec3 radialDirection = radial / radius;
    float theta = atan(dot(radialDirection, down), dot(radialDirection, forward));
    float width = clamp(dot(relative, lateral)
        / max(uTireVisualHalfWidth, 0.0001), -1.0, 1.0);
    float radialFraction = clamp((radius - uTireVisualInnerRadius)
        / max(uTireVisualOuterRadius - uTireVisualInnerRadius, 0.0001),
        0.0, 1.0);
    return vec3(theta, width, radialFraction);
}

void applyTireFailureStrip(inout vec3 position, vec3 coordinates)
{
    if (!uTireVisualEnabled || uTireFailureRenderPass != 1
        || (uTireFailureTreadAttachment >= 0.90
            && uTireFailureStage < 6))
        return;

    vec3 axle = tireAxisVector(uTireVisualAxleAxis);
    vec3 relative = position - uTireVisualCenter;
    float axial = dot(relative, axle);
    vec3 radial = relative - axle * axial;
    float radius = max(length(radial), 0.0001);
    vec3 radialDirection = radial / radius;
    vec3 tangentDirection = normalize(cross(axle, radialDirection));

    // The event seed chooses one material sector and never changes afterward.
    // Wheel spin is already contained by the authored node transform, while
    // angular velocity controls the amount of centrifugal/tangential slap.
    float sectorCenter = tireWrappedAngle(
        uTireFailureEventSeed * 1.61803398875);
    float attachmentLoss = clamp(1.0 - uTireFailureTreadAttachment, 0.0, 1.0);
    float halfSpan = mix(0.32, 1.22, attachmentLoss);
    float sectorDelta = tireWrappedAngle(coordinates.x - sectorCenter);
    float along = clamp(sectorDelta / max(halfSpan, 0.01) * 0.5 + 0.5,
        0.0, 1.0);
    float tether = smoothstep(0.03, 0.42, along);
    float speed = clamp(abs(uTireFailureWheelAngularVelocity) / 65.0, 0.0, 1.0);
    float flutter = sin(uTireFailureEventAgeSeconds
        * mix(7.0, 24.0, speed) + uTireFailureEventSeed * 0.71);
    float structuralLoss = clamp(1.0 - uTireFailureStructuralIntegrity, 0.0, 1.0);
    float stripLengthLocal = uTireVisualOuterRadius
        * (0.18 + 0.38 * attachmentLoss + 0.22 * speed);
    float departure = uTireFailureStage >= 6
        ? smoothstep(0.10, 1.75, uTireFailureEventAgeSeconds) : 0.0;
    float hang = tether * stripLengthLocal
        * (0.48 + 0.40 * structuralLoss + 0.16 * flutter);
    position += radialDirection * hang * (0.38 + 0.32 * speed);
    position += tangentDirection * hang * (0.65 + 0.30 * speed);

    // When the last belt section leaves a bare rim it follows one deterministic
    // ballistic-looking presentation arc; authoritative vehicle forces remain
    // with the reduced-order failure state, never with these vertices.
    if (departure > 0.0)
    {
        float flightM = departure * departure
            * uTireVisualOuterRadius * (1.4 + speed);
        float metersToLocal = uTireVisualOuterRadius
            / max(uTireReferenceRadiusM, 0.02);
        mat3 worldToLocal = inverse(mat3(uModel));
        vec3 worldUpLocal = normalize(worldToLocal * vec3(0.0, 1.0, 0.0));
        position += tangentDirection * flightM;
        position += worldUpLocal * metersToLocal
            * (0.75 * uTireFailureEventAgeSeconds
                - 1.15 * uTireFailureEventAgeSeconds
                    * uTireFailureEventAgeSeconds);
    }
}

void main()
{
    mat4 skin = mat4(1.0);
    if (uUseSkinning)
    {
        ivec4 joints = ivec4(aJoints + vec4(0.5));
        skin = aWeights.x * uJointMatrices[clamp(joints.x, 0, HERITAGE_MAX_JOINTS - 1)]
             + aWeights.y * uJointMatrices[clamp(joints.y, 0, HERITAGE_MAX_JOINTS - 1)]
             + aWeights.z * uJointMatrices[clamp(joints.z, 0, HERITAGE_MAX_JOINTS - 1)]
             + aWeights.w * uJointMatrices[clamp(joints.w, 0, HERITAGE_MAX_JOINTS - 1)];
    }

    vec4 localPosition = skin * vec4(aPos, 1.0);
    vec3 localNormal = mat3(skin) * aNormal;
    vec3 localTangent = mat3(skin) * aTangent.xyz;
    vec4 tireProbeDebug = tireProbeDebugOverlay(localPosition.xyz);
    vTireFailureCoordinates = tireFailureCoordinates(localPosition.xyz);
    vec3 deformedPosition = localPosition.xyz;
    applyTireVisualDeformation(deformedPosition, localNormal);
    applyTireFailureStrip(deformedPosition, vTireFailureCoordinates);
    localPosition = vec4(deformedPosition, 1.0);

    vec4 world = uModel * localPosition;
    mat3 model3 = mat3(uModel);
    mat3 normalMatrix = mat3(transpose(inverse(uModel)));

    vWorldPosition = world.xyz;
    vNormal = normalMatrix * localNormal;
    vTexCoord = aTexCoord;
    vTangent = model3 * localTangent;

    float modelHandedness = determinant(model3) < 0.0 ? -1.0 : 1.0;
    vTangentSign = aTangent.w * modelHandedness;
    vColor = aColor;
    vTireProbeDebugColor = tireProbeDebug.rgb;
    vTireProbeDebugMask = tireProbeDebug.a;
    vec4 viewPosition = uView * world;
    vViewDepth = max(-viewPosition.z, 0.0);
    gl_Position = uProjection * viewPosition;
}
)glsl";

inline const char* kFragmentShader = HERITAGE_MESH_GLSL_VERSION R"glsl(
in vec3 vNormal;
in vec3 vWorldPosition;
in vec2 vTexCoord;
in vec3 vTangent;
in float vTangentSign;
in vec4 vColor;
in float vViewDepth;
in vec3 vTireProbeDebugColor;
in float vTireProbeDebugMask;
in vec3 vTireFailureCoordinates;

uniform bool uTireVisualEnabled;
uniform int uTireFailureStage;
uniform float uTireFailureTreadAttachment;
uniform float uTireFailureEventSeed;
uniform float uTireFailureEventAgeSeconds;
uniform int uTireFailureRenderPass;

uniform vec3 uTint;
uniform vec3 uMaterialBaseColor;
uniform vec3 uMaterialSpecularColor;
uniform vec3 uMaterialEmissiveColor;
uniform float uMaterialRoughness;
uniform float uMaterialMetallic;
uniform float uMaterialSpecularFactor;
uniform float uMaterialOpacity;

uniform sampler2D uBaseColorMap;
uniform sampler2D uNormalMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uMetallicMap;
uniform sampler2D uSpecularMap;
uniform sampler2D uSpecularFactorMap;
uniform sampler2D uAmbientOcclusionMap;
uniform sampler2D uEmissiveMap;
uniform sampler2D uOpacityMap;
uniform samplerCube uEnvironmentMap;
uniform sampler2DArrayShadow uShadowMap;
uniform sampler2DArray uShadowDepthMap;

// LIVETRACK03: persistent Hydro is one X/Z RGBA4 page per 100m chunk. The camera-
// local indirection texture stores exactly one physical Hydro slot per tile;
// water rendering has no vertical surface-sheet list or height resolver.
uniform sampler2DArray uDynamicSurfaceHydroPages;
uniform isampler2D uDynamicSurfacePageIndirection;
// DSURF04G2: one fixed-resolution 10m WaterState authority. Every resident
// tile is 512x512 (~1.953125cm/cell) regardless of distance. Physics remains
// R32UI; presentation may sample a same-resolution R8 cache derived from that
// authority by barrier-aware GPU filtering. The cache never feeds simulation.
uniform sampler2D uGpuWaterAtlas;
uniform sampler2D uGpuWaterPresentationAtlas;
uniform usampler2D uGpuSnowAtlas;
uniform usampler2D uGpuMudAtlas;
uniform usampler2D uGpuTileIndirection;
uniform sampler2D uSurfaceWetnessBreakupMask;

uniform bool uHasBaseColorMap;
uniform bool uHasNormalMap;
uniform bool uHasRoughnessMap;
uniform bool uHasMetallicMap;
uniform bool uHasSpecularMap;
uniform bool uHasSpecularFactorMap;
uniform bool uHasAmbientOcclusionMap;
uniform bool uHasEmissiveMap;
uniform bool uHasOpacityMap;
uniform bool uHasEnvironmentMap;
uniform bool uHasShadowMap;
uniform bool uSurfaceWetnessReceiver;
uniform bool uHasSurfaceWetnessBreakupMask;
uniform bool uDynamicSurfaceActive;
uniform bool uGpuDynamicSurfaceAuthorityActive;
uniform bool uGpuWaterPresentationReady;
uniform bool uGpuDynamicSurfaceSnowReady;
uniform bool uGpuDynamicSurfaceMudReady;
uniform vec2 uGpuDynamicSurfaceCenterOriginRelativeXZ;
uniform ivec2 uGpuDynamicSurfaceTileMapCenter;
uniform int uGpuDynamicSurfaceTileResolution;
uniform int uGpuDynamicSurfaceAtlasColumns;
uniform vec2 uDynamicSurfaceIndirectionOriginRelativeXZ;
uniform float uDynamicSurfacePageWorldSizeM;
uniform int uDynamicSurfaceIndirectionResolution;
uniform int uDynamicSurfaceHydroBaseMip;
uniform float uDynamicSurfaceCameraGlobalY;
uniform vec2 uSurfacePatternCameraModuloXZ;
uniform float uSurfacePresentationTime;
// Compatibility weather-film fallback. Once DSURF04F GPU authority is live,
 // local wetness/free-surface coverage is derived only from WaterState depth.
uniform float uSurfaceWeatherFilmWetness;
uniform float uSurfaceWeatherFilmDepthM;

uniform int uRoughnessChannel;
uniform int uMetallicChannel;
uniform int uAmbientOcclusionChannel;
uniform int uOpacityChannel;
uniform int uSpecularFactorChannel;
uniform bool uUseVertexColor;
uniform float uEnvironmentMaxLod;
uniform mat4 uShadowMatrices[4];
uniform vec4 uShadowSplits;
uniform float uShadowStrength;
uniform int uShadowFilterMode;

uniform vec3 uEye;
uniform vec3 uSunDirection;
uniform vec3 uSunRadiance;
uniform float uGamma;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uWeatherFogDensity;
uniform vec3 uWeatherFogColor;

out vec4 FragColor;

const float PI = 3.14159265358979323846;

float sampleChannel(vec4 texel, int channel)
{
    if (channel == 1)
        return texel.g;
    if (channel == 2)
        return texel.b;
    if (channel == 3)
        return texel.a;
    return texel.r;
}

float distributionGGX(vec3 normal, vec3 halfwayDirection, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(normal, halfwayDirection), 0.0);
    float nDotH2 = nDotH * nDotH;
    float denominator = nDotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 0.000001);
}

float geometrySchlickGGX(float nDotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.000001);
}

float geometrySmith(vec3 normal, vec3 viewDirection, vec3 lightDirection, float roughness)
{
    float nDotV = max(dot(normal, viewDirection), 0.0);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    return geometrySchlickGGX(nDotV, roughness)
         * geometrySchlickGGX(nDotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (vec3(1.0) - f0)
        * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 f0, float roughness)
{
    return f0 + (max(vec3(1.0 - roughness), f0) - f0)
        * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

const vec2 kShadowPoissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.14383161, -0.14100790)
);

vec2 rotateShadowPoisson(vec2 sampleOffset, int cascade)
{
    // A stable per-cascade rotation avoids a single repeated disk orientation
    // without temporal randomization/shimmer.
    float angle = 0.754877666 * float(cascade);
    float c = cos(angle);
    float s = sin(angle);
    return vec2(
        c * sampleOffset.x - s * sampleOffset.y,
        s * sampleOffset.x + c * sampleOffset.y);
}

float sampleShadowNearest(vec3 shadowCoord, int cascade, float receiverDepth)
{
    float storedDepth = texture(
        uShadowDepthMap,
        vec3(shadowCoord.xy, float(cascade))).r;
    return receiverDepth <= storedDepth ? 1.0 : 0.0;
}

float sampleShadowPoissonPcf(
    vec3 shadowCoord,
    int cascade,
    float receiverDepth,
    float radiusTexels)
{
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0).xy);
    float visible = 0.0;
    for (int index = 0; index < 16; ++index)
    {
        vec2 disk = rotateShadowPoisson(kShadowPoissonDisk[index], cascade);
        visible += texture(
            uShadowMap,
            vec4(
                shadowCoord.xy + disk * texel * radiusTexels,
                float(cascade),
                receiverDepth));
    }
    return visible * (1.0 / 16.0);
}

float sampleShadowPcssPoisson(
    vec3 shadowCoord,
    int cascade,
    float receiverDepth)
{
    ivec2 shadowSize = textureSize(uShadowDepthMap, 0).xy;
    vec2 texel = 1.0 / vec2(shadowSize);
    float resolutionScale = float(shadowSize.x) / 4096.0;

    // PCSS stage 1: search a compact Poisson neighborhood for blockers using
    // raw nearest depth. Directional CSM depth is orthographic, so the mean
    // blocker/receiver depth separation is stable enough to drive penumbra size.
    float blockerDepthSum = 0.0;
    float blockerCount = 0.0;
    float searchRadiusTexels = clamp(3.25 * resolutionScale, 1.25, 5.0);
    for (int index = 0; index < 12; ++index)
    {
        vec2 disk = rotateShadowPoisson(kShadowPoissonDisk[index], cascade);
        float storedDepth = texture(
            uShadowDepthMap,
            vec3(
                shadowCoord.xy + disk * texel * searchRadiusTexels,
                float(cascade))).r;
        if (storedDepth < receiverDepth)
        {
            blockerDepthSum += storedDepth;
            blockerCount += 1.0;
        }
    }

    if (blockerCount < 0.5)
        return 1.0;

    float averageBlockerDepth = blockerDepthSum / blockerCount;
    float depthSeparation = max(receiverDepth - averageBlockerDepth, 0.0);

    // PCSS stage 2: separation widens the Poisson PCF disk. The clamp keeps
    // sunlight penumbrae believable and prevents very distant receivers from
    // turning into a giant blur. Scale with map resolution so softness stays
    // approximately stable when selecting 1024/2048/3072/4096 quality.
    float penumbraAt4096 = clamp(1.25 + depthSeparation * 220.0, 1.25, 12.0);
    float penumbraTexels = max(1.0, penumbraAt4096 * resolutionScale);
    return sampleShadowPoissonPcf(
        shadowCoord,
        cascade,
        receiverDepth,
        penumbraTexels);
}

float sampleSunShadow(vec3 normal, vec3 lightDirection)
{
    if (!uHasShadowMap || vViewDepth <= 0.0 || vViewDepth > uShadowSplits.w)
        return 1.0;

    int cascade = 0;
    if (vViewDepth > uShadowSplits.x) cascade = 1;
    if (vViewDepth > uShadowSplits.y) cascade = 2;
    if (vViewDepth > uShadowSplits.z) cascade = 3;

    vec4 lightClip = uShadowMatrices[cascade] * vec4(vWorldPosition, 1.0);
    if (abs(lightClip.w) <= 0.000001)
        return 1.0;

    vec3 shadowCoord = lightClip.xyz / lightClip.w;
    shadowCoord = shadowCoord * 0.5 + 0.5;
    if (shadowCoord.x <= 0.0 || shadowCoord.x >= 1.0
        || shadowCoord.y <= 0.0 || shadowCoord.y >= 1.0
        || shadowCoord.z <= 0.0 || shadowCoord.z >= 1.0)
    {
        return 1.0;
    }

    float nDotL = max(dot(normal, lightDirection), 0.0);
    float cascadeScale = 1.0 + float(cascade) * 0.55;
    float bias = (0.00016 + (1.0 - nDotL) * 0.00075) * cascadeScale;
    float receiverDepth = shadowCoord.z - bias;

    float filteredVisibility = 1.0;
    if (uShadowFilterMode <= 0)
    {
        filteredVisibility = sampleShadowNearest(
            shadowCoord,
            cascade,
            receiverDepth);
    }
    else if (uShadowFilterMode == 1)
    {
        float resolutionScale = float(textureSize(uShadowMap, 0).x) / 4096.0;
        float radiusTexels = max(1.0, 2.0 * resolutionScale);
        filteredVisibility = sampleShadowPoissonPcf(
            shadowCoord,
            cascade,
            receiverDepth,
            radiusTexels);
    }
    else
    {
        filteredVisibility = sampleShadowPcssPoisson(
            shadowCoord,
            cascade,
            receiverDepth);
    }

    return mix(
        1.0,
        filteredVisibility,
        clamp(uShadowStrength, 0.0, 1.0));
}



struct GpuWaterDecoded
{
    float depthM;
    float dryLine;
    vec2 flow;
};

float decodeGpuWaterDepth(float encoded)
{
    float level = clamp(encoded, 0.0, 1.0) * 15.0;
    if (level <= 1.0) return level * 0.0001;
    if (level <= 2.0) return mix(0.0001, 0.0005, level - 1.0);
    if (level <= 3.0) return mix(0.0005, 0.0010, level - 2.0);
    if (level <= 4.0) return mix(0.0010, 0.0020, level - 3.0);
    if (level <= 5.0) return mix(0.0020, 0.0030, level - 4.0);
    if (level <= 6.0) return mix(0.0030, 0.0040, level - 5.0);
    if (level <= 7.0) return mix(0.0040, 0.0060, level - 6.0);
    if (level <= 8.0) return mix(0.0060, 0.0080, level - 7.0);
    if (level <= 9.0) return mix(0.0080, 0.0100, level - 8.0);
    if (level <= 10.0) return mix(0.0100, 0.0130, level - 9.0);
    if (level <= 11.0) return mix(0.0130, 0.0160, level - 10.0);
    if (level <= 12.0) return mix(0.0160, 0.0200, level - 11.0);
    if (level <= 13.0) return mix(0.0200, 0.0240, level - 12.0);
    if (level <= 14.0) return mix(0.0240, 0.0280, level - 13.0);
    return mix(0.0280, 0.0320, level - 14.0);
}

GpuWaterDecoded decodeGpuWater(vec4 state)
{
    GpuWaterDecoded decoded;
    decoded.depthM = decodeGpuWaterDepth(state.r);
    decoded.dryLine = clamp(state.g, 0.0, 1.0);
    ivec2 flowCode = ivec2(round(clamp(state.ba, 0.0, 1.0) * 15.0));
    // B/A 0/0 is the just-resident initialization sentinel. 0/1 is a legacy
    // invalid code that current Hydro initialization recovers rather than
    // preserving. Neither should look like a full-speed (-1,-1) flow vector.
    decoded.flow = (flowCode.x == 0 && flowCode.y <= 1)
        ? vec2(0.0)
        : clamp(state.ba * 2.0 - 1.0, vec2(-1.0), vec2(1.0));
    return decoded;
}

bool gpuDynamicSurfaceTile(
    vec3 positionRelative,
    out ivec2 tileDelta,
    out vec2 tileUv,
    out uint slot)
{
    vec2 tilePosition = (positionRelative.xz - uGpuDynamicSurfaceCenterOriginRelativeXZ)
        / 10.0;
    tileDelta = ivec2(floor(tilePosition));
    tileUv = fract(tilePosition);
    ivec2 mapCoord = tileDelta + uGpuDynamicSurfaceTileMapCenter;
    ivec2 mapSize = textureSize(uGpuTileIndirection, 0);
    if (any(lessThan(mapCoord, ivec2(0))) || any(greaterThanEqual(mapCoord, mapSize)))
        return false;
    uint encoded = texelFetch(uGpuTileIndirection, mapCoord, 0).r;
    if (encoded == 0u)
        return false;
    slot = encoded - 1u;
    return true;
}

ivec2 gpuDynamicSurfaceAtlasOrigin(uint slot)
{
    return ivec2(
        int(slot % uint(max(uGpuDynamicSurfaceAtlasColumns, 1))) * uGpuDynamicSurfaceTileResolution,
        int(slot / uint(max(uGpuDynamicSurfaceAtlasColumns, 1))) * uGpuDynamicSurfaceTileResolution);
}

vec4 gpuWaterHardwareSample(
    vec3 positionRelative,
    out bool valid,
    out ivec2 tileDelta,
    out vec2 tileUv,
    out uint slot)
{
    valid = false;
    tileDelta = ivec2(0);
    tileUv = vec2(0.0);
    slot = 0u;
    if (!uGpuDynamicSurfaceAuthorityActive
        || !gpuDynamicSurfaceTile(positionRelative, tileDelta, tileUv, slot))
    {
        return vec4(0.0);
    }

    float resolution = float(max(uGpuDynamicSurfaceTileResolution, 1));
    ivec2 origin = gpuDynamicSurfaceAtlasOrigin(slot);
    vec2 atlasSize = vec2(textureSize(uGpuWaterAtlas, 0));
    vec2 halfTexel = vec2(0.5) / atlasSize;
    vec2 minUv = (vec2(origin) + vec2(0.5)) / atlasSize;
    vec2 maxUv = (vec2(origin) + vec2(resolution - 0.5)) / atlasSize;
    vec2 atlasUv = (vec2(origin) + tileUv * resolution) / atlasSize;
    atlasUv = clamp(atlasUv, minUv, maxUv);
    valid = true;
    return texture(uGpuWaterAtlas, atlasUv);
}

bool gpuWaterNearestState(vec3 positionRelative, out vec4 state)
{
    state = vec4(0.0);
    ivec2 tileDelta = ivec2(0);
    vec2 tileUv = vec2(0.0);
    uint slot = 0u;
    if (!gpuDynamicSurfaceTile(positionRelative, tileDelta, tileUv, slot))
        return false;
    int resolution = max(uGpuDynamicSurfaceTileResolution, 1);
    ivec2 localTexel = clamp(
        ivec2(floor(tileUv * float(resolution))),
        ivec2(0), ivec2(resolution - 1));
    state = texelFetch(
        uGpuWaterAtlas,
        gpuDynamicSurfaceAtlasOrigin(slot) + localTexel,
        0);
    return true;
}

vec4 gpuWaterWorldBilinear(vec3 positionRelative, out bool valid)
{
    valid = false;
    ivec2 tileDelta = ivec2(0);
    vec2 tileUv = vec2(0.0);
    uint slot = 0u;
    if (!gpuDynamicSurfaceTile(positionRelative, tileDelta, tileUv, slot))
        return vec4(0.0);

    float resolution = float(max(uGpuDynamicSurfaceTileResolution, 1));
    float cellSizeM = 10.0 / resolution;
    vec2 texelCoordinate = tileUv * resolution - vec2(0.5);
    ivec2 base = ivec2(floor(texelCoordinate));
    vec2 f = fract(texelCoordinate);
    vec2 tileOriginRelative = uGpuDynamicSurfaceCenterOriginRelativeXZ
        + vec2(tileDelta) * 10.0;
    vec2 sampleXZ = tileOriginRelative + (vec2(base) + vec2(0.5)) * cellSizeM;

    vec4 s00 = vec4(0.0), s10 = vec4(0.0), s01 = vec4(0.0), s11 = vec4(0.0);
    vec3 p = positionRelative;
    p.xz = sampleXZ;
    if (!gpuWaterNearestState(p, s00))
        return vec4(0.0);
    p.xz = sampleXZ + vec2(cellSizeM, 0.0);
    if (!gpuWaterNearestState(p, s10)) s10 = s00;
    p.xz = sampleXZ + vec2(0.0, cellSizeM);
    if (!gpuWaterNearestState(p, s01)) s01 = s00;
    p.xz = sampleXZ + vec2(cellSizeM);
    if (!gpuWaterNearestState(p, s11)) s11 = mix(s10, s01, 0.5);

    valid = true;
    return mix(mix(s00, s10, f.x), mix(s01, s11, f.x), f.y);
}

vec4 gpuWaterFilteredSingleSample(vec3 positionRelative, out bool valid)
{
    valid = false;
    if (!uGpuDynamicSurfaceAuthorityActive)
        return vec4(0.0);

    ivec2 tileDelta = ivec2(0);
    vec2 tileUv = vec2(0.0);
    uint slot = 0u;
    bool hardwareValid = false;
    vec4 filtered = gpuWaterHardwareSample(
        positionRelative, hardwareValid, tileDelta, tileUv, slot);
    if (!hardwareValid)
        return vec4(0.0);

    // Fixed-function GL_LINEAR handles the interior. Inside a one-texel seam
    // band, switch to logical-world bilinear sampling so a 10m tile edge never
    // samples the physically adjacent atlas slot.
    float resolution = float(max(uGpuDynamicSurfaceTileResolution, 1));
    vec4 edgeCells = vec4(tileUv.x, 1.0 - tileUv.x, tileUv.y, 1.0 - tileUv.y)
        * resolution;
    float nearestEdgeCells = min(min(edgeCells.x, edgeCells.y), min(edgeCells.z, edgeCells.w));
    if (nearestEdgeCells < 1.25)
    {
        bool logicalValid = false;
        vec4 logical = gpuWaterWorldBilinear(positionRelative, logicalValid);
        if (logicalValid)
            filtered = mix(filtered, logical, 1.0 - smoothstep(0.25, 1.25, nearestEdgeCells));
    }

    valid = true;
    return filtered;
}

vec4 gpuWaterFiltered(vec3 positionRelative, out bool valid, out float lodDetail)
{
    valid = false;
    lodDetail = 0.0;
    if (!uGpuDynamicSurfaceAuthorityActive)
        return vec4(0.0);

    float resolution = float(max(uGpuDynamicSurfaceTileResolution, 1));
    float texelSizeM = 10.0 / resolution;
    vec2 blurHalfOffsetM = vec2(0.5 * texelSizeM);

    bool centerValid = false;
    vec4 center = gpuWaterFilteredSingleSample(positionRelative, centerValid);
    if (!centerValid)
        return vec4(0.0);

    vec3 p00 = positionRelative;
    vec3 p10 = positionRelative;
    vec3 p01 = positionRelative;
    vec3 p11 = positionRelative;
    p00.xz += vec2(-blurHalfOffsetM.x, -blurHalfOffsetM.y);
    p10.xz += vec2( blurHalfOffsetM.x, -blurHalfOffsetM.y);
    p01.xz += vec2(-blurHalfOffsetM.x,  blurHalfOffsetM.y);
    p11.xz += vec2( blurHalfOffsetM.x,  blurHalfOffsetM.y);

    bool s00Valid = false;
    bool s10Valid = false;
    bool s01Valid = false;
    bool s11Valid = false;
    vec4 s00 = gpuWaterFilteredSingleSample(p00, s00Valid);
    vec4 s10 = gpuWaterFilteredSingleSample(p10, s10Valid);
    vec4 s01 = gpuWaterFilteredSingleSample(p01, s01Valid);
    vec4 s11 = gpuWaterFilteredSingleSample(p11, s11Valid);
    if (!s00Valid) s00 = center;
    if (!s10Valid) s10 = center;
    if (!s01Valid) s01 = center;
    if (!s11Valid) s11 = center;

    // Preserve the known-good Blur3x3/FadeFix presentation across the complete
    // 100m authority disk. Four bilinear corner taps approximate a compact
    // Gaussian kernel without the cost of a naive nine-tap fragment blur.
    vec4 filtered = 0.25 * (s00 + s10 + s01 + s11);

    GpuWaterDecoded decoded = decodeGpuWater(filtered);
    float distanceM = length(positionRelative.xz);
    lodDetail = 1.0 - smoothstep(0.0, 100.0, distanceM);
    valid = true;
    return vec4(decoded.depthM, decoded.dryLine, decoded.flow);
}

float gpuSnowDepth(vec3 positionRelative)
{
    if (!uGpuDynamicSurfaceAuthorityActive || !uGpuDynamicSurfaceSnowReady)
        return 0.0;
    ivec2 tileDelta = ivec2(0);
    vec2 tileUv = vec2(0.0);
    uint slot = 0u;
    if (!gpuDynamicSurfaceTile(positionRelative, tileDelta, tileUv, slot))
        return 0.0;
    ivec2 localTexel = clamp(ivec2(floor(tileUv * float(max(uGpuDynamicSurfaceTileResolution, 1)))), ivec2(0), ivec2(max(uGpuDynamicSurfaceTileResolution, 1) - 1));
    uint packedState = texelFetch(uGpuSnowAtlas,
        gpuDynamicSurfaceAtlasOrigin(slot) + localTexel, 0).r;
    float n = float(packedState & 0x0fffu) / 4095.0;
    return n * n;
}

float gpuMudDepth(vec3 positionRelative)
{
    if (!uGpuDynamicSurfaceAuthorityActive || !uGpuDynamicSurfaceMudReady)
        return 0.0;
    ivec2 tileDelta = ivec2(0);
    vec2 tileUv = vec2(0.0);
    uint slot = 0u;
    if (!gpuDynamicSurfaceTile(positionRelative, tileDelta, tileUv, slot))
        return 0.0;
    ivec2 localTexel = clamp(ivec2(floor(tileUv * float(max(uGpuDynamicSurfaceTileResolution, 1)))), ivec2(0), ivec2(max(uGpuDynamicSurfaceTileResolution, 1) - 1));
    uint q = texelFetch(uGpuMudAtlas,
        gpuDynamicSurfaceAtlasOrigin(slot) + localTexel, 0).r;
    return float(q) * 0.001;
}

vec4 dynamicSurfaceSampleHydro(
    vec3 positionRelative,
    out bool valid,
    out float supportGlobalY)
{
    valid = false;
    supportGlobalY = positionRelative.y + uDynamicSurfaceCameraGlobalY;
    if (!uDynamicSurfaceActive || uDynamicSurfacePageWorldSizeM <= 0.001)
        return vec4(0.0);

    vec2 tablePosition =
        (positionRelative.xz - uDynamicSurfaceIndirectionOriginRelativeXZ)
        / uDynamicSurfacePageWorldSizeM;
    ivec2 tableCell = ivec2(floor(tablePosition));
    if (tableCell.x < 0 || tableCell.y < 0
        || tableCell.x >= uDynamicSurfaceIndirectionResolution
        || tableCell.y >= uDynamicSurfaceIndirectionResolution)
    {
        return vec4(0.0);
    }

    // LIVETRACK03: one 100m X/Z tile = one Hydro page, exactly 256x256 RGBA4.
    // There is no vertical Hydro-sheet lookup and no secondary water raster.
    int packedSlot = texelFetch(uDynamicSurfacePageIndirection, tableCell, 0).r;
    if (packedSlot <= 0)
        return vec4(0.0);
    int slot = packedSlot - 1;
    vec2 pageUv = fract(tablePosition);
    vec4 hydro = textureLod(
        uDynamicSurfaceHydroPages,
        vec3(pageUv, float(slot)),
        0.0);
    if (any(isnan(hydro)) || any(isinf(hydro)))
        return vec4(0.0);

    // Decode the filtered 4-bit water code through the same non-linear ladder
    // used by the CPU authority. GL_LINEAR interpolates the normalized code;
    // this piecewise decode turns that back into a continuous physical depth.
    float level = clamp(hydro.r, 0.0, 1.0) * 15.0;
    float depthM = 0.0;
    if (level <= 1.0)
        depthM = level * 0.0001;
    else if (level <= 2.0)
        depthM = mix(0.0001, 0.0005, level - 1.0);
    else if (level <= 3.0)
        depthM = mix(0.0005, 0.0010, level - 2.0);
    else if (level <= 4.0)
        depthM = mix(0.0010, 0.0020, level - 3.0);
    else if (level <= 5.0)
        depthM = mix(0.0020, 0.0030, level - 4.0);
    else if (level <= 6.0)
        depthM = mix(0.0030, 0.0040, level - 5.0);
    else if (level <= 7.0)
        depthM = mix(0.0040, 0.0060, level - 6.0);
    else if (level <= 8.0)
        depthM = mix(0.0060, 0.0080, level - 7.0);
    else if (level <= 9.0)
        depthM = mix(0.0080, 0.0100, level - 8.0);
    else if (level <= 10.0)
        depthM = mix(0.0100, 0.0130, level - 9.0);
    else if (level <= 11.0)
        depthM = mix(0.0130, 0.0160, level - 10.0);
    else if (level <= 12.0)
        depthM = mix(0.0160, 0.0200, level - 11.0);
    else if (level <= 13.0)
        depthM = mix(0.0200, 0.0240, level - 12.0);
    else if (level <= 14.0)
        depthM = mix(0.0240, 0.0280, level - 13.0);
    else
        depthM = mix(0.0280, 0.0320, level - 14.0);

    float flowCodeX = clamp(hydro.b, 0.0, 1.0) * 15.0;
    float flowCodeZ = clamp(hydro.a, 0.0, 1.0) * 15.0;
    float flowX = flowCodeX < 7.0
        ? -(7.0 - flowCodeX) / 7.0
        : (flowCodeX > 8.0 ? (flowCodeX - 8.0) / 7.0 : 0.0);
    float flowZ = flowCodeZ < 7.0
        ? -(7.0 - flowCodeZ) / 7.0
        : (flowCodeZ > 8.0 ? (flowCodeZ - 8.0) / 7.0 : 0.0);

    valid = true;
    return vec4(
        clamp(depthM, 0.0, 0.032),
        clamp(hydro.g, 0.0, 1.0),
        clamp(flowX, -1.0, 1.0),
        clamp(flowZ, -1.0, 1.0));
}

vec4 dynamicSurfaceHeadSample(vec3 positionRelative, out bool valid)
{
    float supportGlobalY = 0.0;
    return dynamicSurfaceSampleHydro(positionRelative, valid, supportGlobalY);
}

vec4 dynamicSurfaceWaterState(vec3 positionRelative)
{
    bool centerValid = false;
    vec4 center = dynamicSurfaceHeadSample(positionRelative, centerValid);
    if (!centerValid)
        return vec4(0.0, 0.0, 0.0, 0.0);

    // GL_LINEAR handles the 256x256 intra-page field. This tiny edge bridge is
    // only for continuity across adjacent 100m array layers.
    vec2 tablePosition =
        (positionRelative.xz - uDynamicSurfaceIndirectionOriginRelativeXZ)
        / uDynamicSurfacePageWorldSizeM;
    vec2 tileUv = fract(tablePosition);
    float stateResolution = max(
        float(textureSize(uDynamicSurfaceHydroPages, 0).x),
        1.0);
    float halfTexelUv = 0.5 / stateResolution;
    float texelWorldM = uDynamicSurfacePageWorldSizeM / stateResolution;

    float blendX = 0.0;
    float directionX = 0.0;
    if (tileUv.x < halfTexelUv)
    {
        blendX = (halfTexelUv - tileUv.x) / (2.0 * halfTexelUv);
        directionX = -1.0;
    }
    else if (tileUv.x > 1.0 - halfTexelUv)
    {
        blendX = (tileUv.x - (1.0 - halfTexelUv)) / (2.0 * halfTexelUv);
        directionX = 1.0;
    }

    float blendZ = 0.0;
    float directionZ = 0.0;
    if (tileUv.y < halfTexelUv)
    {
        blendZ = (halfTexelUv - tileUv.y) / (2.0 * halfTexelUv);
        directionZ = -1.0;
    }
    else if (tileUv.y > 1.0 - halfTexelUv)
    {
        blendZ = (tileUv.y - (1.0 - halfTexelUv)) / (2.0 * halfTexelUv);
        directionZ = 1.0;
    }

    vec4 xNeighbour = center;
    vec4 zNeighbour = center;
    vec4 diagonalNeighbour = center;
    bool neighbourValid = false;
    if (blendX > 0.0)
    {
        vec3 samplePosition = positionRelative;
        samplePosition.x += directionX * (0.5 * texelWorldM);
        xNeighbour = dynamicSurfaceHeadSample(samplePosition, neighbourValid);
        if (!neighbourValid)
            xNeighbour = center;
    }
    if (blendZ > 0.0)
    {
        vec3 samplePosition = positionRelative;
        samplePosition.z += directionZ * (0.5 * texelWorldM);
        zNeighbour = dynamicSurfaceHeadSample(samplePosition, neighbourValid);
        if (!neighbourValid)
            zNeighbour = center;
    }
    if (blendX > 0.0 && blendZ > 0.0)
    {
        vec3 samplePosition = positionRelative;
        samplePosition.x += directionX * (0.5 * texelWorldM);
        samplePosition.z += directionZ * (0.5 * texelWorldM);
        diagonalNeighbour = dynamicSurfaceHeadSample(samplePosition, neighbourValid);
        if (!neighbourValid)
            diagonalNeighbour = mix(xNeighbour, zNeighbour, 0.5);
    }
    else if (blendX > 0.0)
        diagonalNeighbour = xNeighbour;
    else if (blendZ > 0.0)
        diagonalNeighbour = zNeighbour;

    vec4 row0 = mix(center, xNeighbour, blendX);
    vec4 row1 = mix(zNeighbour, diagonalNeighbour, blendX);
    return mix(row0, row1, blendZ);
}

float dynamicSurfacePresentationDepth(vec3 positionRelative, float physicalDepthM)
{
    // LIVETRACK03 deliberately has no second Hydro resolution/shoreline authority.
    // The 256x256 RGBA4 water texture itself owns both simulation and water
    // presentation; ordinary material wetness remains a separate PBR response.
    // GLSL permits unused parameters; do not use C/C++ `(void)x` casts here.
    return max(physicalDepthM, 0.0);
}

vec3 applyDynamicSurfaceWater(
    vec3 litColor,
    vec3 shadingNormal,
    vec3 viewDirection,
    float surfaceRoughness,
    float filmWetness,
    float puddleDepthM,
    float presentationDepthM,
    float freeSurfaceMask,
    float waterSurfaceHeightRelativeY,
    float dynamicSurfaceLodDetail)
{
    if (filmWetness <= 0.0001 && puddleDepthM <= 0.000008)
        return litColor;

    vec2 stableWorldXZ = vWorldPosition.xz + uSurfacePatternCameraModuloXZ;
    // Retained in the interface for future SSR/refraction. Dynamic Surface avoids
    // deriving optical normals from page-cache derivatives.
    float stableHeadReference = waterSurfaceHeightRelativeY * 0.0;
    stableWorldXZ += vec2(stableHeadReference);

    // DSURF04F3: distinguish the continuous rain-wet material film from actual
    // free-surface puddle water. The authoritative GPU depth remains the sole
    // physical water state; this only defines the optical regime. Treating the
    // full millimetric rain layer as a free surface made every receiver look like
    // a single translucent plastic sheet.
    float film = clamp(filmWetness, 0.0, 1.0);
    // presentationDepthM has already had the continuous film baseline and the
    // 1.5mm puddle-excess threshold removed at the authority sample site.
    float freeSurfaceDepthM = max(presentationDepthM, 0.0);
    // DSURF04F5: depth alone must not turn fast runoff into a plastic sheet.
    // The authority sample supplies a mask that combines excess depth and
    // WaterState velocity, so only locally retained/slower water receives the
    // free-surface optical branch.
    float standing = clamp(freeSurfaceMask, 0.0, 1.0);
    float deepPool = smoothstep(0.010, 0.032, freeSurfaceDepthM) * standing;

    // Wet porous material darkens appreciably because liquid fills the air gaps
    // that normally scatter light. This is the ordinary rainy-road response and
    // should read as wet asphalt/soil, not as a clear sheet of plastic.
    float wetDarkening = mix(
        0.88,
        0.68,
        smoothstep(0.20, 0.95, clamp(surfaceRoughness, 0.0, 1.0)));
    vec3 wetSubstrate = litColor * mix(1.0, wetDarkening, film);
    vec3 filmColor = mix(litColor, wetSubstrate, film * 0.88);

    // The common case is rain film with no excess free-surface depth. PBR
    // roughness has already been adjusted before lighting, so stop here rather
    // than manufacturing a second reflective layer over the whole track.
    if (standing <= 0.001)
        return filmColor;

    // DSURF04F7: the centimetre-scale WaterState already owns the spatial
    // shape of the water. Do not stamp a small set of periodic sine waves over
    // every wet receiver: those waves repeated at fixed sub-metre wavelengths
    // and became visible as the same white/silver motif copied across roads and
    // hillsides. Pooled water only relaxes toward gravity-up here. Future rain
    // impacts, if reintroduced, must come from non-periodic event/state data.
    float upward = clamp(shadingNormal.y, 0.0, 1.0);
    float flattening = standing * smoothstep(0.20, 0.85, upward)
        * mix(0.38, 0.86, deepPool);
    vec3 waterNormal = normalize(mix(
        shadingNormal,
        vec3(0.0, 1.0, 0.0),
        flattening));
    float distanceDetail = (1.0 - smoothstep(95.0, 290.0, length(vWorldPosition.xz)))
        * clamp(dynamicSurfaceLodDetail, 0.0, 1.0);

    float ndv = clamp(dot(waterNormal, viewDirection), 0.0, 1.0);
    float fresnelFull = 0.0204 + 0.9796 * pow(1.0 - ndv, 5.0);
    float fresnel = mix(0.0204, fresnelFull, distanceDetail);

    vec3 reflected = vec3(0.20, 0.24, 0.28);
    if (uHasEnvironmentMap)
    {
        vec3 reflectionDirection = reflect(-viewDirection, waterNormal);
        float lod = mix(
            min(2.4, uEnvironmentMaxLod),
            0.16,
            standing);
        reflected = textureLod(
            uEnvironmentMap,
            reflectionDirection,
            lod).rgb;
    }

    // Beer-Lambert transmission uses only the excess free-surface depth. The
    // retained film is already represented by the darkened authored material.
    float opticalPathM = freeSurfaceDepthM / max(ndv, 0.25);
    vec3 absorptionPerM = vec3(0.80, 0.34, 0.16);
    vec3 transmittance = exp(-absorptionPerM * opticalPathM);
    vec3 transmitted = wetSubstrate * transmittance;

    // Only genuinely deep accumulated water receives volumetric tint.
    vec3 deepWaterTint = vec3(0.012, 0.027, 0.031);
    transmitted = mix(transmitted, deepWaterTint, deepPool * 0.22);

    // Physical dielectric Fresnel owns reflection. Keep the environment-only
    // path bounded until SSR/planar local reflections exist, so a grey sky does
    // not turn every puddle into opaque silver plastic.
    float reflectionWeight = clamp(fresnel, 0.0, 0.28);
    vec3 standingColor = mix(transmitted, reflected, reflectionWeight);

    // The free-surface layer grows only from depth above the retained film.
    // Everywhere else remains ordinary wet material.
    return mix(filmColor, standingColor, standing);
}

void main()
{
    if (uTireVisualEnabled
        && (uTireFailureTreadAttachment < 0.90 || uTireFailureStage >= 6))
    {
        float attachmentLoss = clamp(
            1.0 - uTireFailureTreadAttachment, 0.0, 1.0);
        float halfSpan = mix(0.32, 1.22, attachmentLoss);
        float sectorCenter = mod(
            uTireFailureEventSeed * 1.61803398875 + PI,
            2.0 * PI) - PI;
        float delta = abs(mod(
            vTireFailureCoordinates.x - sectorCenter + PI,
            2.0 * PI) - PI);
        bool tornBeltSector = delta <= halfSpan
            && abs(vTireFailureCoordinates.y) <= 0.88
            && vTireFailureCoordinates.z >= 0.56;
        if (uTireFailureRenderPass == 0)
        {
            if (uTireFailureStage >= 6 || tornBeltSector)
                discard;
        }
        else if (!tornBeltSector
            || (uTireFailureStage >= 6
                && uTireFailureEventAgeSeconds >= 2.2))
        {
            discard;
        }
    }

    vec4 baseSample = uHasBaseColorMap
        ? texture(uBaseColorMap, vTexCoord)
        : vec4(1.0);
    vec4 vertexColor = uUseVertexColor ? vColor : vec4(1.0);
    vec3 baseColor =
        uTint * uMaterialBaseColor * baseSample.rgb * vertexColor.rgb;

    float opacity = uMaterialOpacity * baseSample.a * vertexColor.a;
    if (uHasOpacityMap)
        opacity *= sampleChannel(texture(uOpacityMap, vTexCoord), uOpacityChannel);
    if (opacity <= 0.01)
        discard;

    vec3 geometricNormal = normalize(vNormal);
    if (!gl_FrontFacing)
        geometricNormal = -geometricNormal;

    vec3 normal = geometricNormal;
    if (uHasNormalMap)
    {
        vec3 tangent = normalize(vTangent);
        tangent = normalize(
            tangent - geometricNormal * dot(geometricNormal, tangent));
        vec3 bitangent =
            normalize(cross(geometricNormal, tangent)) * vTangentSign;
        vec3 tangentNormal =
            texture(uNormalMap, vTexCoord).xyz * 2.0 - 1.0;
        normal = normalize(
            mat3(tangent, bitangent, geometricNormal) * tangentNormal);
    }

    float dynamicSurfaceDepthM = 0.0;
    float dynamicSurfacePuddleDepthM = 0.0;
    float dynamicSurfaceVisualDepthM = 0.0;
    float dynamicSurfaceWaterSurfaceHeightRelativeY = vWorldPosition.y;
    float dynamicSurfaceFilm = 0.0;
    float dynamicSurfaceStanding = 0.0;
    float dynamicSurfaceLodDetail = 1.0;
    if (uSurfaceWetnessReceiver)
    {
        if (uGpuDynamicSurfaceAuthorityActive)
        {
            bool gpuValid = false;
            float gpuLodDetail = 0.0;
            vec4 gpuState = gpuWaterFiltered(vWorldPosition, gpuValid, gpuLodDetail);
            dynamicSurfaceLodDetail = gpuLodDetail;
            dynamicSurfaceDepthM = gpuValid ? max(gpuState.x, 0.0) : 0.0;
            float gpuDryLineStrength = gpuValid ? clamp(gpuState.y, 0.0, 1.0) : 0.0;
            // DSURF04F4: WaterState is the only water/wetness authority. The
            // legacy global SurfaceWeather film is no longer subtracted from
            // GPU depth and no longer paints every receiver uniformly.
            // A microscopic retained layer reads as wet material; only local
            // excess depth becomes free-surface standing water.
            // A few millimetres of mobile rain film belong to the wet material
            // regime. They are real WaterState mass, but not a settled optical
            // free surface. This is intentionally much larger than the old
            // 0.35mm cutoff: at centimetre resolution runoff must be allowed to
            // exist without coating the entire road in mirror-like water.
            // Retired GPU authority keeps the same puddles-only optical rule
            // for ABI/debug consistency: sub-10mm runoff is wet material, not a
            // translucent sheet. Only accumulated, comparatively still water
            // may become visible puddle optics.
            const float formedPuddleThresholdM = 0.0001;
            dynamicSurfacePuddleDepthM = max(
                dynamicSurfaceDepthM - formedPuddleThresholdM,
                0.0);
            dynamicSurfaceVisualDepthM = dynamicSurfacePresentationDepth(
                vWorldPosition, dynamicSurfacePuddleDepthM);
            dynamicSurfaceWaterSurfaceHeightRelativeY =
                vWorldPosition.y + dynamicSurfaceDepthM;
            // Ordinary weather wetness remains visible on every receiver while
            // the local Hydro field adds spatially varying puddles and dry-line
            // history. This is the known-good LIVETRACK04B presentation and
            // prevents missing or warming tiles from punching dry holes into rain.
            dynamicSurfaceFilm = clamp(uSurfaceWeatherFilmWetness, 0.0, 1.0)
                * (1.0 - 0.62 * gpuDryLineStrength);
            dynamicSurfaceFilm = max(
                dynamicSurfaceFilm,
                smoothstep(0.00002, 0.00050, dynamicSurfaceDepthM)
                    * (1.0 - 0.35 * gpuDryLineStrength));
            float flowStrength = clamp(length(gpuState.zw), 0.0, 1.0);
            float waterPresence = smoothstep(0.00005, 0.00010, dynamicSurfaceDepthM);
            float waterBodyStrength = mix(
                0.08,
                1.0,
                smoothstep(0.00010, 0.0100, dynamicSurfaceDepthM));
            dynamicSurfaceStanding = waterPresence
                * waterBodyStrength
                * dynamicSurfaceLodDetail
                * (1.0 - 0.18 * flowStrength);
        }
        else
        {
            vec4 dynamicSurfaceState = dynamicSurfaceWaterState(vWorldPosition);
            dynamicSurfaceDepthM = max(dynamicSurfaceState.x, 0.0);
            float dryLineStrength = clamp(dynamicSurfaceState.y, 0.0, 1.0);
            vec2 flowDirection = dynamicSurfaceState.zw;
            dynamicSurfaceWaterSurfaceHeightRelativeY =
                vWorldPosition.y + dynamicSurfaceDepthM;

            // Ordinary rain wetness remains a cheap material response. The G
            // channel is persistent tire-clearing/dry-line strength, so a used
            // racing line can stay visibly less wet after rainfall stops.
            float weatherFilm = clamp(uSurfaceWeatherFilmWetness, 0.0, 1.0);
            dynamicSurfaceFilm = weatherFilm * (1.0 - 0.62 * dryLineStrength);
            dynamicSurfaceFilm = max(
                dynamicSurfaceFilm,
                smoothstep(0.00002, 0.00050, dynamicSurfaceDepthM)
                    * (1.0 - 0.35 * dryLineStrength));

            // First visible RGBA4 water state is 0.1mm. Puddle/water optics use
            // the same 256x256 field and fade continuously to ordinary wet
            // material at 100m. There is no additional water mask/page.
            float puddleDistanceFade = 1.0 - smoothstep(
                0.0, 120.0, length(vWorldPosition.xz));
            float waterPresence = smoothstep(
                0.00005, 0.00010, dynamicSurfaceDepthM);
            float waterBodyStrength = mix(
                0.08,
                1.0,
                smoothstep(0.00010, 0.0100, dynamicSurfaceDepthM));
            dynamicSurfacePuddleDepthM = waterPresence > 0.0
                ? dynamicSurfaceDepthM
                : 0.0;
            dynamicSurfaceVisualDepthM = dynamicSurfacePuddleDepthM;
            dynamicSurfaceStanding = waterPresence
                * waterBodyStrength
                * puddleDistanceFade;

            // Keep the packed B/A flow vector relevant to optics without
            // inventing a second simulation: moving water is slightly less
            // mirror-like than retained water.
            dynamicSurfaceStanding *= 1.0 - 0.18 * clamp(length(flowDirection), 0.0, 1.0);
        }
    }

    float roughness = clamp(uMaterialRoughness, 0.04, 1.0);
    if (uHasRoughnessMap)
        roughness = clamp(
            sampleChannel(texture(uRoughnessMap, vTexCoord), uRoughnessChannel),
            0.04,
            1.0);

    // Thin-film roughness is driven by the smooth weather film, not
    // adaptive hydrology depth. Keep the response restrained so wet grass/rough
    // concrete do not become white mirrors. Local puddles may still become
    // substantially smoother once excess standing water is real.
    if (uSurfaceWetnessReceiver && dynamicSurfaceFilm > 0.0001)
    {
        // Thin wet film should darken the authored material without turning it
        // into a broad plastic mirror. Preserve most authored roughness/normal
        // detail; free-surface puddles below own the strong optical smoothing.
        float wetRoughness = max(0.36, roughness * 0.92);
        roughness = mix(
            roughness,
            wetRoughness,
            dynamicSurfaceFilm * 0.35);
    }
    if (dynamicSurfaceStanding > 0.0001)
    {
        roughness = mix(
            roughness,
            0.085,
            dynamicSurfaceStanding * 0.62);
    }

    float metallic = clamp(uMaterialMetallic, 0.0, 1.0);
    if (uHasMetallicMap)
        metallic = clamp(
            sampleChannel(texture(uMetallicMap, vTexCoord), uMetallicChannel),
            0.0,
            1.0);

    vec3 specularTexture = uHasSpecularMap
        ? texture(uSpecularMap, vTexCoord).rgb
        : vec3(1.0);
    float specularFactor = clamp(uMaterialSpecularFactor, 0.0, 1.0);
    if (uHasSpecularFactorMap)
    {
        specularFactor *= sampleChannel(
            texture(uSpecularFactorMap, vTexCoord),
            uSpecularFactorChannel);
    }

    vec3 dielectricF0 = clamp(
        uMaterialSpecularColor * specularTexture * specularFactor,
        vec3(0.0),
        vec3(1.0));
    vec3 f0 = mix(dielectricF0, baseColor, metallic);

    float ao = uHasAmbientOcclusionMap
        ? sampleChannel(texture(uAmbientOcclusionMap, vTexCoord), uAmbientOcclusionChannel)
        : 1.0;
    ao = clamp(ao, 0.0, 1.0);

    vec3 emissive = uMaterialEmissiveColor;
    if (uHasEmissiveMap)
        emissive *= texture(uEmissiveMap, vTexCoord).rgb;

    vec3 viewDirection = normalize(uEye - vWorldPosition);
    vec3 lightDirection = normalize(uSunDirection);
    vec3 halfwayDirection = normalize(viewDirection + lightDirection);
    float nDotV = max(dot(normal, viewDirection), 0.0001);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    float hDotV = max(dot(halfwayDirection, viewDirection), 0.0);

    float d = distributionGGX(normal, halfwayDirection, roughness);
    float g = geometrySmith(normal, viewDirection, lightDirection, roughness);
    vec3 f = fresnelSchlick(hDotV, f0);
    vec3 directSpecular =
        (d * g * f) / max(4.0 * nDotV * max(nDotL, 0.0001), 0.0001);
    vec3 directDiffuseWeight = (vec3(1.0) - f) * (1.0 - metallic);
    vec3 directRadiance = max(uSunRadiance, vec3(0.0));
    float sunVisibility = sampleSunShadow(normal, lightDirection);
    vec3 directLighting =
        (directDiffuseWeight * baseColor / PI + directSpecular)
        * directRadiance * nDotL * sunVisibility;

    vec3 ambientLighting;
    if (uHasEnvironmentMap)
    {
        // GFX6 foundation: the cubemap's generated mip chain provides a
        // deterministic roughness-aware reflection approximation. A future
        // GGX prefilter/BRDF LUT can replace this without changing materials.
        vec3 reflectionDirection = reflect(-viewDirection, normal);
        vec3 diffuseEnvironment = textureLod(
            uEnvironmentMap,
            normal,
            uEnvironmentMaxLod).rgb;
        vec3 specularEnvironment = textureLod(
            uEnvironmentMap,
            reflectionDirection,
            roughness * uEnvironmentMaxLod).rgb;

        vec3 environmentFresnel =
            fresnelSchlickRoughness(nDotV, f0, roughness);
        vec3 environmentDiffuseWeight =
            (vec3(1.0) - environmentFresnel) * (1.0 - metallic);
        vec3 diffuseIbl = diffuseEnvironment * baseColor
            * environmentDiffuseWeight * 0.72;
        vec3 specularIbl = specularEnvironment * environmentFresnel
            * mix(1.0, 0.55, roughness);
        ambientLighting = (diffuseIbl + specularIbl) * ao;
    }
    else
    {
        float upFacing = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
        float hemisphere = mix(0.12, 0.30, upFacing);
        ambientLighting = baseColor * (1.0 - metallic) * hemisphere * ao;
    }

    vec3 color = directLighting + ambientLighting + emissive;

    // The authored receiver is shaded once. Heritage Dynamic Surface is merely
    // surface state sampled per fragment, so shorelines are shader-defined and
    // never inherit simulation-cell or render-mesh topology.
    if (uSurfaceWetnessReceiver)
    {
        color = applyDynamicSurfaceWater(
            color,
            normal,
            viewDirection,
            roughness,
            dynamicSurfaceFilm,
            dynamicSurfacePuddleDepthM,
            dynamicSurfaceVisualDepthM,
            dynamicSurfaceStanding,
            dynamicSurfaceWaterSurfaceHeightRelativeY,
            dynamicSurfaceLodDetail);

        if (uGpuDynamicSurfaceAuthorityActive)
        {
            float surfaceFade = 1.0 - smoothstep(250.0, 300.0, length(vWorldPosition.xz));
            float snowM = gpuSnowDepth(vWorldPosition);
            float snowCover = smoothstep(0.001, 0.025, snowM) * surfaceFade;
            vec3 snowLit = mix(color, vec3(0.78, 0.81, 0.84), 0.82);
            color = mix(color, snowLit, snowCover);
            float mudM = gpuMudDepth(vWorldPosition);
            float mudCover = smoothstep(0.002, 0.06, mudM) * surfaceFade;
            color = mix(color, color * vec3(0.48, 0.38, 0.27), mudCover * 0.72);
        }
    }

    // WEATHER06A far-field precipitation/air-lighting. This is intentionally
    // a cheap distance extinction term; near streaks and the sky rain curtain
    // are separate presentation tiers.
    float weatherFog = 1.0 - exp(
        -max(vViewDepth, 0.0) * max(uWeatherFogDensity, 0.0));
    color = mix(color, uWeatherFogColor, clamp(weatherFog, 0.0, 0.88));

    color = pow(
        clamp(color, 0.0, 1.0),
        vec3(1.0 / max(uGamma, 0.01)));
    color = (color - 0.5) * uContrast + 0.5 + uBrightness;
    float luminance =
        dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, uSaturation);
    // TIRE26A/VIS19: developer probe overlay is intentionally applied after
    // lighting/color grading so contact diagnostics remain unmistakable.
    color = mix(
        color,
        vTireProbeDebugColor,
        clamp(vTireProbeDebugMask, 0.0, 1.0));
    FragColor = vec4(clamp(color, 0.0, 1.0), clamp(opacity, 0.0, 1.0));
}
)glsl";


inline const char* kShadowVertexShader = HERITAGE_MESH_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aPos;
layout(location=4) in vec4 aJoints;
layout(location=5) in vec4 aWeights;

const int HERITAGE_MAX_JOINTS = 128;

uniform mat4 uModel;
uniform bool uUseSkinning;
uniform mat4 uJointMatrices[HERITAGE_MAX_JOINTS];
uniform bool uTireVisualEnabled;
uniform vec3 uTireVisualCenter;
uniform int uTireVisualAxleAxis;
uniform float uTireVisualHalfWidth;
uniform float uTireVisualInnerRadius;
uniform float uTireVisualOuterRadius;
uniform float uTireReferenceRadiusM;
uniform vec3 uTireWheelForwardWorld;
uniform vec3 uTireWheelRightWorld;
uniform vec3 uTireWheelUpWorld;
const int HERITAGE_TIRE_FIELD_STATIONS = 24;
const int HERITAGE_TIRE_FIELD_BANDS = 13;
const int HERITAGE_TIRE_FIELD_COUNT = 312;
uniform bool uTireVisualDeformationFieldValid;
uniform vec3 uTireVisualDisplacementM[HERITAGE_TIRE_FIELD_COUNT];

const float HERITAGE_PI = 3.14159265358979323846;

vec3 tireAxisVector(int axis)
{
    if (axis == 1) return vec3(0.0, 1.0, 0.0);
    if (axis == 2) return vec3(0.0, 0.0, 1.0);
    return vec3(1.0, 0.0, 0.0);
}

vec3 tireRestDown(int axis)
{
    return axis == 1 ? vec3(0.0, 0.0, -1.0) : vec3(0.0, -1.0, 0.0);
}

vec3 rotateAroundAxis(vec3 value, vec3 axis, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return value * c + cross(axis, value) * s
        + axis * dot(axis, value) * (1.0 - c);
}


float tireShadowFieldWidthCoordinate(float widthCoordinate)
{
    const float widths[HERITAGE_TIRE_FIELD_BANDS] = float[](
        -1.00, -0.82, -0.65, -0.49, -0.34, -0.18, 0.00,
         0.18,  0.34,  0.49,  0.65,  0.82, 1.00);
    float w = clamp(widthCoordinate, -1.0, 1.0);
    for (int band = 0; band < HERITAGE_TIRE_FIELD_BANDS - 1; ++band)
    {
        if (w <= widths[band + 1] || band == HERITAGE_TIRE_FIELD_BANDS - 2)
            return float(band) + clamp((w - widths[band])
                / max(widths[band + 1] - widths[band], 1.0e-6), 0.0, 1.0);
    }
    return float(HERITAGE_TIRE_FIELD_BANDS - 1);
}

vec3 tireShadowFieldControl(int station, int band)
{
    station = (station + HERITAGE_TIRE_FIELD_STATIONS)
        % HERITAGE_TIRE_FIELD_STATIONS;
    band = clamp(band, 0, HERITAGE_TIRE_FIELD_BANDS - 1);
    int index = station * HERITAGE_TIRE_FIELD_BANDS + band;
    return uTireVisualDisplacementM[index];
}

vec3 sampleTireShadowField(float theta, float widthCoordinate)
{
    float stationCoordinate = mod(theta + 2.0 * HERITAGE_PI,
        2.0 * HERITAGE_PI) * float(HERITAGE_TIRE_FIELD_STATIONS)
        / (2.0 * HERITAGE_PI);
    int station0 = int(floor(stationCoordinate)) % HERITAGE_TIRE_FIELD_STATIONS;
    int station1 = (station0 + 1) % HERITAGE_TIRE_FIELD_STATIONS;
    float stationT = fract(stationCoordinate);
    stationT = stationT * stationT * (3.0 - 2.0 * stationT);
    float bandCoordinate = tireShadowFieldWidthCoordinate(widthCoordinate);
    int band0 = min(int(floor(bandCoordinate)), HERITAGE_TIRE_FIELD_BANDS - 2);
    int band1 = band0 + 1;
    float bandT = bandCoordinate - float(band0);
    bandT = bandT * bandT * (3.0 - 2.0 * bandT);
    return mix(mix(tireShadowFieldControl(station0, band0),
        tireShadowFieldControl(station0, band1), bandT),
        mix(tireShadowFieldControl(station1, band0),
        tireShadowFieldControl(station1, band1), bandT), stationT);
}

vec3 deformTireShadowPosition(vec3 position)
{
    if (!uTireVisualEnabled || !uTireVisualDeformationFieldValid)
        return position;
    vec3 axle = tireAxisVector(uTireVisualAxleAxis);
    vec3 relative = position - uTireVisualCenter;
    float axial = dot(relative, axle);
    vec3 radial = relative - axle * axial;
    float radius = length(radial);
    float radialSpan = max(uTireVisualOuterRadius
        - uTireVisualInnerRadius, 0.0001);
    if (radius <= 0.000001)
        return position;
    mat3 worldToLocal = inverse(mat3(uModel));
    vec3 forward = worldToLocal * uTireWheelForwardWorld;
    forward -= axle * dot(forward, axle);
    forward = dot(forward, forward) > 0.000001
        ? normalize(forward)
        : normalize(cross(tireRestDown(uTireVisualAxleAxis), axle));
    vec3 lateral = worldToLocal * uTireWheelRightWorld;
    lateral -= forward * dot(lateral, forward);
    lateral = dot(lateral, lateral) > 0.000001 ? normalize(lateral) : axle;
    vec3 up = worldToLocal * uTireWheelUpWorld;
    up -= forward * dot(up, forward);
    up -= lateral * dot(up, lateral);
    vec3 down = dot(up, up) > 0.000001
        ? -normalize(up)
        : normalize(cross(forward, lateral));
    vec3 radialDirection = radial / radius;
    float theta = atan(dot(radialDirection, down),
        dot(radialDirection, forward));
    float widthCoordinate = clamp(dot(relative, lateral)
        / max(uTireVisualHalfWidth, 0.0001), -1.0, 1.0);
    vec3 fieldM = sampleTireShadowField(theta, widthCoordinate);
    float radialFraction = clamp((radius - uTireVisualInnerRadius)
        / radialSpan, 0.0, 1.0);
    float attachment = smoothstep(0.015, 0.62, radialFraction);
    float metersToLocal = uTireVisualOuterRadius
        / max(uTireReferenceRadiusM, 0.02);
    position += metersToLocal * attachment
        * (forward * fieldM.x + down * fieldM.y + lateral * fieldM.z);
    return position;
}

void main()
{
    mat4 skin = mat4(1.0);
    if (uUseSkinning)
    {
        ivec4 joints = ivec4(aJoints + vec4(0.5));
        skin = aWeights.x * uJointMatrices[clamp(joints.x, 0, HERITAGE_MAX_JOINTS - 1)]
             + aWeights.y * uJointMatrices[clamp(joints.y, 0, HERITAGE_MAX_JOINTS - 1)]
             + aWeights.z * uJointMatrices[clamp(joints.z, 0, HERITAGE_MAX_JOINTS - 1)]
             + aWeights.w * uJointMatrices[clamp(joints.w, 0, HERITAGE_MAX_JOINTS - 1)];
    }

    vec4 localPosition = skin * vec4(aPos, 1.0);
    localPosition.xyz = deformTireShadowPosition(localPosition.xyz);

    // SHADOW02: keep the vertex result in camera-relative world space. A
    // layered geometry stage fans the triangle out only to the cascades whose
    // frusta contain this draw range. This removes the old CPU-side four-pass
    // submission loop without changing the shadow-map coordinate system.
    gl_Position = uModel * localPosition;
}
)glsl";

inline const char* kShadowGeometryShader = HERITAGE_MESH_GLSL_VERSION R"glsl(
layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out;

uniform mat4 uLightViewProjection[4];
uniform int uCascadeMask;

void main()
{
    // One source triangle may touch more than one cascade. Fan it out on the
    // GPU into the layered shadow texture instead of asking the CPU/driver to
    // resubmit the same indexed draw once per cascade.
    for (int cascade = 0; cascade < 4; ++cascade)
    {
        if ((uCascadeMask & (1 << cascade)) == 0)
            continue;

        gl_Layer = cascade;
        for (int vertex = 0; vertex < 3; ++vertex)
        {
            gl_Position = uLightViewProjection[cascade] * gl_in[vertex].gl_Position;
            EmitVertex();
        }
        EndPrimitive();
    }
}
)glsl";

inline const char* kShadowFragmentShader = HERITAGE_MESH_GLSL_VERSION R"glsl(
void main()
{
    // Depth-only pass. Alpha-tested foliage support will plug into this pass
    // once glTF alphaMode is represented explicitly by the material system.
}
)glsl";

} // namespace heritage::graphics::entity_mesh_shaders

#undef HERITAGE_MESH_GLSL_VERSION
