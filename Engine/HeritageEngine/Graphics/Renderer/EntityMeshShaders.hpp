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

vec3 tireFlexibleRingDisplacementWorld(vec3 localPosition, vec3 worldPosition)
{
    if (!uTireVisualDeformationFieldValid)
        return vec3(0.0);

    vec3 axleLocal = tireAxisVector(uTireVisualAxleAxis);
    vec3 relativeLocal = localPosition - uTireVisualCenter;
    float axialLocal = dot(relativeLocal, axleLocal);
    vec3 radialLocal = relativeLocal - axleLocal * axialLocal;
    float radiusLocal = length(radialLocal);
    float radialSpanLocal = max(
        uTireVisualOuterRadius - uTireVisualInnerRadius, 0.0001);
    if (radiusLocal <= 0.000001 || uTireVisualOuterRadius <= 0.0001)
        return vec3(0.0);

    // TIRE45C: sample and apply the physics field in WORLD space.  The old
    // presentation reconstructed a wheel basis through inverse(mat3(uModel))
    // and then converted metre displacements back to authored local units.
    // That makes correctness depend on the exact GLB mirror/scale/pivot chain.
    // The physical field already lives in the non-spinning wheel world basis,
    // so use that basis directly and let the already-spun vertex determine the
    // spatial station.  Mirrored/non-uniform authoring transforms can no longer
    // turn radial/longitudinal deformation into lateral tire lean.
    vec3 forwardWorld = uTireWheelForwardWorld;
    forwardWorld = dot(forwardWorld, forwardWorld) > 0.000001
        ? normalize(forwardWorld) : vec3(0.0, 0.0, 1.0);

    vec3 rightWorld = uTireWheelRightWorld;
    rightWorld -= forwardWorld * dot(rightWorld, forwardWorld);
    rightWorld = dot(rightWorld, rightWorld) > 0.000001
        ? normalize(rightWorld) : vec3(1.0, 0.0, 0.0);

    vec3 upWorld = uTireWheelUpWorld;
    upWorld -= forwardWorld * dot(upWorld, forwardWorld);
    upWorld -= rightWorld * dot(upWorld, rightWorld);
    if (dot(upWorld, upWorld) > 0.000001)
        upWorld = normalize(upWorld);
    else
        upWorld = normalize(cross(forwardWorld, rightWorld));
    // Preserve the physical handedness even if a fallback path was needed.
    if (dot(cross(forwardWorld, rightWorld), upWorld) < 0.0)
        upWorld = -upWorld;
    vec3 downWorld = -upWorld;

    vec3 centerWorld = (uModel * vec4(uTireVisualCenter, 1.0)).xyz;
    vec3 relativeWorld = worldPosition - centerWorld;
    vec3 radialWorld = relativeWorld
        - rightWorld * dot(relativeWorld, rightWorld);
    float radialWorldLength = length(radialWorld);
    if (radialWorldLength <= 0.000001)
        return vec3(0.0);
    vec3 radialDirectionWorld = radialWorld / radialWorldLength;
    float theta = atan(
        dot(radialDirectionWorld, downWorld),
        dot(radialDirectionWorld, forwardWorld));

    // Width is most reliable in the authored node's own coordinates because
    // the importer measured half-width there.  Only the sign is matched to the
    // physical right direction; spin cannot affect an axle coordinate.
    vec3 authoredAxleWorld = mat3(uModel) * axleLocal;
    float axleSign = dot(authoredAxleWorld, rightWorld) < 0.0 ? -1.0 : 1.0;
    float widthCoordinate = clamp(
        axleSign * axialLocal / max(uTireVisualHalfWidth, 0.0001),
        -1.0, 1.0);
    vec3 fieldM = sampleTireFlexibleRingField(theta, widthCoordinate);

    // The bead remains fixed to the rim. The attachment is evaluated in the
    // same local geometry space in which the importer measured the tire radii.
    float radialFraction = clamp(
        (radiusLocal - uTireVisualInnerRadius) / radialSpanLocal, 0.0, 1.0);
    float carcassAttachment = smoothstep(0.015, 0.62, radialFraction);

    // fieldM is already metres. uModel maps the authored mesh to world metres,
    // therefore no metres-to-local conversion belongs in this path.
    return carcassAttachment
        * (forwardWorld * fieldM.x
            + downWorld * fieldM.y
            + rightWorld * fieldM.z);
}

vec4 tireProbeDebugOverlay(vec3 displacementWorldM)
{
    if (!uTireProbeDebugVisible || !uTireVisualEnabled)
        return vec4(0.0);
    float magnitudeM = length(displacementWorldM);
    vec3 color = mix(vec3(0.05, 0.75, 1.0), vec3(1.0, 0.05, 0.02),
        smoothstep(0.004, 0.030, magnitudeM));
    return vec4(color, smoothstep(0.0001, 0.0010, magnitudeM) * 0.92);
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
    vTireFailureCoordinates = tireFailureCoordinates(localPosition.xyz);
    vec3 deformedPosition = localPosition.xyz;
    applyTireFailureStrip(deformedPosition, vTireFailureCoordinates);
    localPosition = vec4(deformedPosition, 1.0);

    vec4 world = uModel * localPosition;
    vec3 tireWorldDisplacement = tireFlexibleRingDisplacementWorld(
        localPosition.xyz, world.xyz);
    world.xyz += tireWorldDisplacement;
    vec4 tireProbeDebug = tireProbeDebugOverlay(tireWorldDisplacement);
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
uniform samplerCube uEnvironmentMapPrevious;
uniform sampler2DArrayShadow uShadowMap;
uniform sampler2DArray uShadowDepthMap;

// LIVETRACK15 production water uses a near 10m/256x256 RGBA8 topology atlas
// plus a rolling 10m/32x32 RGB8 far topology cache through 500m. Both carry immutable
// mesh-prebaked runoff / 4-bit standing-depth ceiling / flow; live fill is reconstructed on-GPU.
uniform sampler2D uGpuWaterAtlas;
uniform sampler2D uGpuFarWaterAtlas;
uniform isampler2D uGpuFarTileTags;
uniform usampler2D uGpuSnowAtlas;
uniform usampler2D uGpuMudAtlas;
// LIVETRACK22: close tire marks are a surface-material R8 state in the same
// 10m/256x256 tile slots/indirection as the other Dynamic Surface channels.
uniform sampler2D uGpuTireMarkAtlas;
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
uniform bool uGpuDynamicSurfaceAuthorityActive;
uniform bool uGpuDynamicSurfaceSnowReady;
uniform bool uGpuDynamicSurfaceMudReady;
uniform bool uGpuDynamicSurfaceTireMarksReady;
uniform vec2 uGpuDynamicSurfaceCenterOriginRelativeXZ;
uniform ivec2 uGpuDynamicSurfaceCenterWorldTile;
uniform ivec2 uGpuDynamicSurfaceTileMapCenter;
uniform int uGpuDynamicSurfaceTileResolution;
uniform int uGpuDynamicSurfaceAtlasColumns;
uniform int uGpuFarTileResolution;
uniform int uGpuFarAtlasTilesPerAxis;
uniform vec2 uSurfacePatternCameraModuloXZ;
uniform float uSurfacePresentationTime;
// Ordinary scene wetness remains the cheap fallback outside detailed topology.
uniform float uSurfaceWeatherFilmWetness;
// LIVETRACK15: one scene rainfall accumulator drives every resident prebaked
// puddle tile. Static standing-depth ceiling/flow live in the near/far atlases; the
// fill head is derived every draw inside the same 0..0.70mm domain, so rain never
// requires a periodic full-field compute pulse.
uniform float uPrebakedWaterExposureM;
uniform float uRainWettingExposureM;
uniform float uRainRateMmPerHour;

uniform int uRoughnessChannel;
uniform int uMetallicChannel;
uniform int uAmbientOcclusionChannel;
uniform int uOpacityChannel;
uniform int uSpecularFactorChannel;
uniform bool uUseVertexColor;
uniform float uEnvironmentMaxLod;
uniform float uEnvironmentBlend;
uniform mat4 uShadowMatrices[4];
uniform vec4 uShadowSplits;
uniform float uShadowStrength;
uniform int uShadowFilterMode;

uniform vec3 uEye;
uniform vec3 uSunDirection;
uniform vec3 uSunRadiance;
uniform float uDayNightCycle;
uniform float uGamma;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uWeatherFogDensity;
uniform vec3 uWeatherFogColor;
uniform sampler2D uRegionalWeatherMap;
uniform bool uRegionalWeatherMapValid;
uniform vec2 uRegionalWeatherCameraOffsetXZ;
uniform vec2 uRegionalWeatherAdvectionXZ;
uniform float uRegionalWeatherHalfRangeM;
uniform float uWeatherCloudBaseM;

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


vec4 sampleRegionalWeather(vec2 cameraRelativeXZ)
{
    if (!uRegionalWeatherMapValid || uRegionalWeatherHalfRangeM <= 1.0)
        return vec4(0.0);
    vec2 relativeToFieldCenter = cameraRelativeXZ
        + uRegionalWeatherCameraOffsetXZ
        + uRegionalWeatherAdvectionXZ;
    vec2 uv = vec2(0.5) + relativeToFieldCenter
        / (2.0 * uRegionalWeatherHalfRangeM);
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
        return vec4(0.0);
    return texture(uRegionalWeatherMap, uv);
}

vec3 regionalCloudSunTransmission(vec3 surfacePosition, vec3 lightDirection)
{
    if (!uRegionalWeatherMapValid || lightDirection.y <= 0.02)
        return vec3(1.0);

    // Project the surface point toward the sun into the representative cloud
    // layer. The same regional weather texture also drives visible volumetric
    // cloud density and radar precipitation, so moving cloud cells darken the
    // corresponding world region instead of using an unrelated shadow decal.
    float heightToCloudM = max(uWeatherCloudBaseM - surfacePosition.y, 0.0);
    vec2 projectedXZ = surfacePosition.xz
        + lightDirection.xz * (heightToCloudM / max(lightDirection.y, 0.06));
    vec4 weather = sampleRegionalWeather(projectedXZ);
    float cloud = weather.r;
    float rain = weather.g;
    float storm = weather.a;
    float optical = smoothstep(0.18, 0.88, cloud)
        * mix(0.55, 1.0, storm);
    // CELESTIAL08: preserve the exact regional-cloud response shape, but blend
    // only ten percent of CELESTIAL07's illumination loss/tint into the receiver.
    // Volumetric cloud density/opacity itself is unchanged.
    float legacyTransmission = mix(1.0, 0.22, optical);
    legacyTransmission *= mix(1.0, 0.78, rain * storm);
    float transmission = mix(1.0, legacyTransmission, 0.10);
    vec3 legacyTint = mix(vec3(1.0), vec3(0.80, 0.86, 0.94), optical * 0.34);
    vec3 tint = mix(vec3(1.0), legacyTint, 0.10);
    return tint * transmission;
}

vec3 volumetricCloudSunTransmission(vec3 surfacePosition, vec3 lightDirection)
{
    // CELESTIAL07: the detailed 256x256 optical-depth cookie has exactly one
    // receiver authority: the dedicated post-opaque ground-shadow pass. The
    // old material path sampled the same cookie again, so a validity/source
    // change could darken direct light + ambient here and then multiply the
    // finished receiver a second time. Keep only the continuous regional cloud
    // transmission in materials; detailed moving shadow structure is applied
    // once after opaque rendering.
    return regionalCloudSunTransmission(surfacePosition, lightDirection);
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
    float radiusTexels,
    int sampleCount)
{
    // CLOUDURP15BK: hardware linear shadow comparison already gives a useful
    // 2x2 footprint per fetch. Eight Poisson taps are ample near the camera;
    // far cascades use four. This replaces the old unconditional 16 taps.
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0).xy);
    float visible = 0.0;
    for (int index = 0; index < 8; ++index)
    {
        if(index>=sampleCount) break;
        vec2 disk = rotateShadowPoisson(kShadowPoissonDisk[index], cascade);
        visible += texture(
            uShadowMap,
            vec4(
                shadowCoord.xy + disk * texel * radiusTexels,
                float(cascade),
                receiverDepth));
    }
    return visible / max(float(sampleCount),1.0);
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
    for (int index = 0; index < 4; ++index)
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
        penumbraTexels,
        8);
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
            radiusTexels,
            cascade >= 2 ? 4 : 8);
    }
    else
    {
        // CLOUDURP15BK: PCSS is only worth its blocker search in the two near
        // cascades. Far cascades use four hardware-linear PCF taps instead of
        // paying the old 12-blocker + 16-filter fetch budget per fragment.
        if(cascade>=2)
        {
            float resolutionScale = float(textureSize(uShadowMap, 0).x) / 4096.0;
            filteredVisibility = sampleShadowPoissonPcf(
                shadowCoord,
                cascade,
                receiverDepth,
                max(1.0,2.5*resolutionScale),
                4);
        }
        else
        {
            filteredVisibility = sampleShadowPcssPoisson(
                shadowCoord,
                cascade,
                receiverDepth);
        }
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
    float basinStrength;
    float runoffPotential;
    float runoffAreaM2;
    float flowStrength;
    vec2 flowDirection;
};

float decodeGpuRunoffAreaM2(float encoded)
{
    // Matches the .hhyd v9 4-bit logarithmic catchment ladder. Keeping the
    // physical contributing area lets the runtime estimate discharge from
    // actual rainfall intensity instead of treating runoff as a cosmetic mask.
    float level = clamp(encoded, 0.0, 1.0) * 15.0;
    if (level <= 0.0) return 0.0;
    if (level <= 1.0) return mix(0.0, 0.25, level);
    if (level <= 2.0) return mix(0.25, 0.50, level - 1.0);
    if (level <= 3.0) return mix(0.50, 1.0, level - 2.0);
    if (level <= 4.0) return mix(1.0, 2.0, level - 3.0);
    if (level <= 5.0) return mix(2.0, 4.0, level - 4.0);
    if (level <= 6.0) return mix(4.0, 8.0, level - 5.0);
    if (level <= 7.0) return mix(8.0, 16.0, level - 6.0);
    if (level <= 8.0) return mix(16.0, 32.0, level - 7.0);
    if (level <= 9.0) return mix(32.0, 64.0, level - 8.0);
    if (level <= 10.0) return mix(64.0, 128.0, level - 9.0);
    if (level <= 11.0) return mix(128.0, 256.0, level - 10.0);
    if (level <= 12.0) return mix(256.0, 512.0, level - 11.0);
    if (level <= 13.0) return mix(512.0, 1024.0, level - 12.0);
    if (level <= 14.0) return mix(1024.0, 2048.0, level - 13.0);
    return mix(2048.0, 4096.0, level - 14.0);
}

float waterDepthFromLadderCode(int code)
{
    // LIVETRACK21I authoritative 4-bit ultra-shallow ladder, metres:
    // 0.00, 0.01, 0.05, 0.10, 0.15 ... 0.70 mm. Codes 2..15 are a
    // regular 0.05-mm sequence, so decoding is arithmetic instead of a
    // 16-entry lookup.
    code = clamp(code, 0, 15);
    if (code == 0) return 0.0;
    if (code == 1) return 0.00001;
    return float(code - 1) * 0.00005;
}

const float kStandingWaterMaxDepthM = 0.00070;

float quantizeStandingWaterDepth(float depthM)
{
    // LIVETRACK21I: the exact 16-value ladder is the standing-water authority,
    // not merely a capacity hint. Every final puddle depth is snapped back to
    // the same codebook that the prebake writes.
    depthM = clamp(depthM, 0.0, kStandingWaterMaxDepthM);
    if (depthM < 0.000005) return 0.0;
    if (depthM < 0.000030) return 0.00001;
    int code = clamp(int(floor(depthM / 0.00005 + 0.5)) + 1, 2, 15);
    return float(code - 1) * 0.00005;
}

float decodeGpuWaterDepth(float encoded)
{
    // The RGBA8 atlas stores the 4-bit code as exact multiples of 17/255.
    // Hardware filtering remains enabled, but capacity is rounded back to a
    // legal code before its physical meaning is used. The decoded capacity
    // therefore always comes from the exact LIVETRACK21I ladder.
    int code = int(round(clamp(encoded, 0.0, 1.0) * 15.0));
    return waterDepthFromLadderCode(code);
}

vec2 decodeGpuFlowDirection(float encoded)
{
    // Constant lookup avoids doing sin/cos for every sample in the 3x3 water
    // filter. Codes 1 and 15 intentionally meet at the -X wrap direction.
    const vec2 directions[16] = vec2[16](
        vec2(0.0, 0.0),
        vec2(-1.00000000, 0.00000000),
        vec2(-0.90096887, -0.43388374),
        vec2(-0.62348980, -0.78183148),
        vec2(-0.22252093, -0.97492791),
        vec2( 0.22252093, -0.97492791),
        vec2( 0.62348980, -0.78183148),
        vec2( 0.90096887, -0.43388374),
        vec2( 1.00000000,  0.00000000),
        vec2( 0.90096887,  0.43388374),
        vec2( 0.62348980,  0.78183148),
        vec2( 0.22252093,  0.97492791),
        vec2(-0.22252093,  0.97492791),
        vec2(-0.62348980,  0.78183148),
        vec2(-0.90096887,  0.43388374),
        vec2(-1.00000000,  0.00000000));
    int angleCode = int(round(clamp(encoded, 0.0, 1.0) * 15.0));
    return directions[clamp(angleCode, 0, 15)];
}

GpuWaterDecoded decodeGpuWater(vec4 state)
{
    GpuWaterDecoded decoded;
    decoded.dryLine = clamp(state.g, 0.0, 1.0);
    decoded.runoffPotential = clamp(state.r, 0.0, 1.0);
    decoded.runoffAreaM2 = decodeGpuRunoffAreaM2(state.r);

    // B is the immutable mesh-prebaked STANDING-WATER DEPTH CEILING. The bake
    // writes only the user's 16 legal depths. Runtime filling is deliberately
    // solved in the SAME 0..0.70 mm domain; no hidden 28/32 mm legacy range is
    // allowed to reinterpret the field.
    float catchmentFill = mix(0.72, 1.22,
        smoothstep(0.10, 0.78, decoded.runoffPotential));
    float retainedHeadDriverM = clamp(
        uPrebakedWaterExposureM * catchmentFill, 0.0, 0.0040);
    float capacityM = decodeGpuWaterDepth(state.b);
    // Encoding already rejects sub-0.005 mm mesh noise into code zero. Any
    // non-zero baked code is therefore a genuine standing-water candidate.
    decoded.basinStrength = step(0.000005, capacityM);

    // Priority-flood capacity = spill elevation - local terrain. A common
    // below-spill hydraulic head preserves a horizontal free surface. The head
    // deficit now spans exactly the ladder's 0.70 mm physical range, so changing
    // the ladder necessarily changes the simulated and rendered puddle depths.
    float fillProgress = smoothstep(0.000050, 0.00350, retainedHeadDriverM);
    float headDeficitM = kStandingWaterMaxDepthM
        * (1.0 - pow(fillProgress, 0.72));
    float equilibriumDepthM = max(capacityM - headDeficitM, 0.0);
    decoded.depthM = min(equilibriumDepthM, capacityM)
        * (1.0 - 0.72 * decoded.dryLine);

    decoded.flowDirection = decodeGpuFlowDirection(state.a);
    decoded.flowStrength = length(decoded.flowDirection) > 0.5 ? 1.0 : 0.0;
    return decoded;
}

GpuWaterDecoded weightedGpuWaterDecoded(
    GpuWaterDecoded a, float wa,
    GpuWaterDecoded b, float wb,
    GpuWaterDecoded c, float wc,
    GpuWaterDecoded d, float wd)
{
    float total = max(wa + wb + wc + wd, 1.0e-6);
    GpuWaterDecoded outValue;
    outValue.depthM = (a.depthM * wa + b.depthM * wb
        + c.depthM * wc + d.depthM * wd) / total;
    outValue.dryLine = (a.dryLine * wa + b.dryLine * wb
        + c.dryLine * wc + d.dryLine * wd) / total;
    outValue.basinStrength = (a.basinStrength * wa + b.basinStrength * wb
        + c.basinStrength * wc + d.basinStrength * wd) / total;
    outValue.runoffPotential = (a.runoffPotential * wa + b.runoffPotential * wb
        + c.runoffPotential * wc + d.runoffPotential * wd) / total;
    outValue.runoffAreaM2 = (a.runoffAreaM2 * wa + b.runoffAreaM2 * wb
        + c.runoffAreaM2 * wc + d.runoffAreaM2 * wd) / total;
    vec2 flow = a.flowDirection * wa + b.flowDirection * wb
        + c.flowDirection * wc + d.flowDirection * wd;
    float coherence = length(flow) / total;
    outValue.flowStrength = clamp(coherence, 0.0, 1.0);
    outValue.flowDirection = coherence > 1.0e-4 ? normalize(flow) : vec2(0.0);
    return outValue;
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

// LIVETRACK18B GL_LINEAR experiment: use the existing near RGBA8 atlas and
// its hardware linear sampler directly. No extra texture, channel or decoded
// presentation buffer is introduced. The packed capacity/flow channels are
// intentionally allowed to interpolate for this visual experiment.
bool gpuWaterNearestState(vec3 positionRelative, out vec4 state)
{
    state = vec4(0.0);
    ivec2 tileDelta = ivec2(0);
    vec2 tileUv = vec2(0.0);
    uint slot = 0u;
    if (!gpuDynamicSurfaceTile(positionRelative, tileDelta, tileUv, slot))
        return false;

    float resolution = float(max(uGpuDynamicSurfaceTileResolution, 1));
    ivec2 origin = gpuDynamicSurfaceAtlasOrigin(slot);
    vec2 atlasSize = vec2(textureSize(uGpuWaterAtlas, 0));
    vec2 minUv = (vec2(origin) + vec2(0.5)) / atlasSize;
    vec2 maxUv = (vec2(origin) + vec2(resolution - 0.5)) / atlasSize;
    vec2 atlasUv = (vec2(origin) + tileUv * resolution) / atlasSize;
    atlasUv = clamp(atlasUv, minUv, maxUv);
    state = texture(uGpuWaterAtlas, atlasUv);
    return true;
}

bool gpuNearWaterDecoded(vec3 positionRelative, out GpuWaterDecoded decoded)
{
    vec4 centerState = vec4(0.0);
    if (!gpuWaterNearestState(positionRelative, centerState))
        return false;
    GpuWaterDecoded center = decodeGpuWater(centerState);

    // LIVETRACK21I performance: retain hardware GL_LINEAR filtering but reduce
    // the old 3x3/9-state decode to a five-tap cross kernel. The old kernel made
    // nine tile-indirection lookups plus nine atlas samples per near fragment.
    // Center + N/S/E/W keeps the important low-frequency smoothing at nearly
    // half that topology-sampling cost.
    float resolution = float(max(uGpuDynamicSurfaceTileResolution, 1));
    float texelSizeM = 10.0 / resolution;
    float depthSum = center.depthM * 4.0;
    float drySum = center.dryLine * 4.0;
    float basinSum = center.basinStrength * 4.0;
    float runoffSum = center.runoffPotential * 4.0;
    float runoffAreaSum = center.runoffAreaM2 * 4.0;
    vec2 flowSum = center.flowDirection * 4.0;
    float weightSum = 4.0;
    const vec2 offsets[4] = vec2[4](
        vec2(-1.0, 0.0), vec2(1.0, 0.0),
        vec2(0.0, -1.0), vec2(0.0, 1.0));
    for (int i = 0; i < 4; ++i)
    {
        vec3 p = positionRelative;
        p.xz += offsets[i] * texelSizeM;
        vec4 sampleState = centerState;
        GpuWaterDecoded sampleValue = center;
        if (gpuWaterNearestState(p, sampleState))
            sampleValue = decodeGpuWater(sampleState);
        const float weight = 2.0;
        depthSum += sampleValue.depthM * weight;
        drySum += sampleValue.dryLine * weight;
        basinSum += sampleValue.basinStrength * weight;
        runoffSum += sampleValue.runoffPotential * weight;
        runoffAreaSum += sampleValue.runoffAreaM2 * weight;
        flowSum += sampleValue.flowDirection * weight;
        weightSum += weight;
    }
    float inverseWeight = 1.0 / weightSum;
    decoded.depthM = depthSum * inverseWeight;
    decoded.dryLine = drySum * inverseWeight;
    decoded.basinStrength = basinSum * inverseWeight;
    decoded.runoffPotential = runoffSum * inverseWeight;
    decoded.runoffAreaM2 = runoffAreaSum * inverseWeight;
    float flowCoherence = length(flowSum) * inverseWeight;
    decoded.flowStrength = clamp(flowCoherence, 0.0, 1.0);
    decoded.flowDirection = flowCoherence > 1.0e-4
        ? normalize(flowSum) : vec2(0.0);
    return true;
}

int positiveModulo(int value, int divisor)
{
    int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

bool gpuFarWaterNearestState(vec3 positionRelative, out vec4 state)
{
    state = vec4(0.0);
    if (!uGpuDynamicSurfaceAuthorityActive)
        return false;

    vec2 tilePosition = (positionRelative.xz - uGpuDynamicSurfaceCenterOriginRelativeXZ)
        / 10.0;
    ivec2 tileDelta = ivec2(floor(tilePosition));
    vec2 tileUv = fract(tilePosition);
    ivec2 worldTile = uGpuDynamicSurfaceCenterWorldTile + tileDelta;
    int axis = max(uGpuFarAtlasTilesPerAxis, 1);
    ivec2 slotCoord = ivec2(
        positiveModulo(worldTile.x, axis),
        positiveModulo(worldTile.y, axis));
    ivec2 tag = texelFetch(uGpuFarTileTags, slotCoord, 0).rg;
    if (any(notEqual(tag, worldTile)))
        return false;

    int resolution = max(uGpuFarTileResolution, 1);
    ivec2 localTexel = clamp(
        ivec2(floor(tileUv * float(resolution))),
        ivec2(0), ivec2(resolution - 1));
    vec3 rcf = texelFetch(
        uGpuFarWaterAtlas,
        slotCoord * resolution + localTexel,
        0).rgb;
    // Far RGB mirrors near logical channels without dry-line:
    // R=runoff accumulation, G=capacity, B=flow angle.
    state = vec4(rcf.r, 0.0, rcf.g, rcf.b);
    return true;
}

bool gpuFarWaterDecoded(vec3 positionRelative, out GpuWaterDecoded decoded)
{
    if (!uGpuDynamicSurfaceAuthorityActive)
        return false;

    vec2 tilePosition = (positionRelative.xz - uGpuDynamicSurfaceCenterOriginRelativeXZ)
        / 10.0;
    ivec2 tileDelta = ivec2(floor(tilePosition));
    vec2 tileUv = fract(tilePosition);
    float resolution = float(max(uGpuFarTileResolution, 1));
    float texelSizeM = 10.0 / resolution;
    vec2 texelCoordinate = tileUv * resolution - vec2(0.5);
    ivec2 base = ivec2(floor(texelCoordinate));
    vec2 f = fract(texelCoordinate);
    vec2 tileOriginRelative = uGpuDynamicSurfaceCenterOriginRelativeXZ
        + vec2(tileDelta) * 10.0;
    vec2 sampleXZ = tileOriginRelative + (vec2(base) + vec2(0.5)) * texelSizeM;

    vec4 s00 = vec4(0.0), s10 = vec4(0.0), s01 = vec4(0.0), s11 = vec4(0.0);
    vec3 p = positionRelative;
    p.xz = sampleXZ;
    if (!gpuFarWaterNearestState(p, s00))
        return false;
    p.xz = sampleXZ + vec2(texelSizeM, 0.0);
    if (!gpuFarWaterNearestState(p, s10)) s10 = s00;
    p.xz = sampleXZ + vec2(0.0, texelSizeM);
    if (!gpuFarWaterNearestState(p, s01)) s01 = s00;
    p.xz = sampleXZ + vec2(texelSizeM);
    if (!gpuFarWaterNearestState(p, s11)) s11 = s10;

    // Decoded bilinear filtering: interpolate physical water quantities and
    // circular flow vectors, never packed nibble/angle codes.
    GpuWaterDecoded d00 = decodeGpuWater(s00);
    GpuWaterDecoded d10 = decodeGpuWater(s10);
    GpuWaterDecoded d01 = decodeGpuWater(s01);
    GpuWaterDecoded d11 = decodeGpuWater(s11);
    decoded = weightedGpuWaterDecoded(
        d00, (1.0 - f.x) * (1.0 - f.y),
        d10, f.x * (1.0 - f.y),
        d01, (1.0 - f.x) * f.y,
        d11, f.x * f.y);
    decoded.dryLine = 0.0;
    return true;
}

bool gpuWaterFiltered(
    vec3 positionRelative,
    out GpuWaterDecoded combined,
    out float lodDetail)
{
    lodDetail = 0.0;
    if (!uGpuDynamicSurfaceAuthorityActive)
        return false;

    float distanceM = length(positionRelative.xz);
    if (distanceM > 500.0)
        return false;

    GpuWaterDecoded nearDecoded;
    GpuWaterDecoded farDecoded;
    bool nearValid = false;
    bool farValid = false;

    // LIVETRACK21I performance: do not evaluate both topology LODs for every
    // fragment. The two fields are needed simultaneously only in the 85..100m
    // transition ring. This removes the far four-tap path from almost every
    // near fragment and removes the expensive near 3x3 path from every far
    // fragment. Missing-cache fallbacks still preserve continuity.
    if (distanceM < 85.0)
    {
        nearValid = gpuNearWaterDecoded(positionRelative, nearDecoded);
        if (!nearValid)
            farValid = gpuFarWaterDecoded(positionRelative, farDecoded);
    }
    else if (distanceM > 100.0)
    {
        farValid = gpuFarWaterDecoded(positionRelative, farDecoded);
        if (!farValid && distanceM <= 105.0)
            nearValid = gpuNearWaterDecoded(positionRelative, nearDecoded);
    }
    else
    {
        nearValid = gpuNearWaterDecoded(positionRelative, nearDecoded);
        farValid = gpuFarWaterDecoded(positionRelative, farDecoded);
    }

    if (!nearValid && !farValid)
        return false;

    combined = nearValid ? nearDecoded : farDecoded;
    if (nearValid && farValid)
    {
        float nearWeight = 1.0 - smoothstep(85.0, 100.0, distanceM);
        float farWeight = 1.0 - nearWeight;
        combined.depthM = farDecoded.depthM * farWeight
            + nearDecoded.depthM * nearWeight;
        combined.dryLine = farDecoded.dryLine * farWeight
            + nearDecoded.dryLine * nearWeight;
        combined.basinStrength = farDecoded.basinStrength * farWeight
            + nearDecoded.basinStrength * nearWeight;
        combined.runoffPotential = farDecoded.runoffPotential * farWeight
            + nearDecoded.runoffPotential * nearWeight;
        combined.runoffAreaM2 = farDecoded.runoffAreaM2 * farWeight
            + nearDecoded.runoffAreaM2 * nearWeight;
        vec2 flow = farDecoded.flowDirection * farWeight
            + nearDecoded.flowDirection * nearWeight;
        float coherence = length(flow);
        combined.flowStrength = clamp(coherence, 0.0, 1.0);
        combined.flowDirection = coherence > 1.0e-4
            ? normalize(flow) : vec2(0.0);
    }

    // Exactly one standing-depth quantisation per fragment. The bake and every
    // final rendered standing-water result therefore share the same 16 values,
    // without the three redundant branch-heavy snaps used by LIVETRACK21G.
    combined.depthM = quantizeStandingWaterDepth(combined.depthM);
    lodDetail = 1.0 - smoothstep(450.0, 500.0, distanceM);
    return true;
}

float kinematicRunoffDepthM(GpuWaterDecoded state)
{
    // LIVETRACK21 moving-water solve. The MFD bake supplies contributing area
    // and downhill direction. Runtime rain intensity converts that catchment to
    // discharge, then a compact Manning-style kinematic-wave relation estimates
    // the flowing sheet/channel depth. No circular rain splats and no 20-million
    // cell CFD pass are required.
    if (state.runoffAreaM2 <= 0.25 || state.flowStrength <= 0.035)
        return 0.0;

    // uRainRateMmPerHour is the live runoff driver: it attacks immediately with
    // rainfall and is allowed to decay for tens of seconds after the shower by
    // the CPU-side scalar state. It is deliberately independent of the much
    // longer-lived material wetness film.
    float effectiveRainRateMmPerHour = max(uRainRateMmPerHour, 0.0);
    if (effectiveRainRateMmPerHour <= 0.01)
        return 0.0;

    float rainfallMps = effectiveRainRateMmPerHour * (0.001 / 3600.0);
    float dischargeM3ps = rainfallMps * max(state.runoffAreaM2, 0.0);
    // Broad catchments spread into wider films/gutters instead of becoming an
    // infinitely deep one-texel river.
    float effectiveWidthM = clamp(
        0.28 + 0.080 * sqrt(max(state.runoffAreaM2, 0.0)), 0.28, 2.40);
    float unitDischargeM2ps = dischargeM3ps / effectiveWidthM;
    const float manningN = 0.014;
    // The bake currently stores direction, not slope magnitude. Use a stable
    // road/runoff slope surrogate; coherent flow gets the faster end of it.
    float slope = mix(0.006, 0.024, clamp(state.flowStrength, 0.0, 1.0));
    float depthM = pow(max(
        unitDischargeM2ps * manningN / sqrt(max(slope, 0.0005)), 0.0), 0.60);
    // LIVETRACK21K: runoff should establish soon after a coherent wet film
    // exists instead of waiting for most of a millimetre of accumulated exposure.
    // This changes onset timing only; the Manning discharge/depth ceiling is unchanged.
    float connected = smoothstep(0.000010, 0.000180, uRainWettingExposureM);
    depthM *= connected * 0.65 * (1.0 - 0.64 * state.dryLine);
    return clamp(depthM, 0.0, 0.0030);
}


float gpuTireMarkStrength(vec3 positionRelative)
{
    if (!uGpuDynamicSurfaceAuthorityActive || !uGpuDynamicSurfaceTireMarksReady)
        return 0.0;
    ivec2 tileDelta = ivec2(0);
    vec2 tileUv = vec2(0.0);
    uint slot = 0u;
    if (!gpuDynamicSurfaceTile(positionRelative, tileDelta, tileUv, slot))
        return 0.0;

    float resolution = float(max(uGpuDynamicSurfaceTileResolution, 1));
    ivec2 origin = gpuDynamicSurfaceAtlasOrigin(slot);
    vec2 atlasSize = vec2(textureSize(uGpuTireMarkAtlas, 0));
    vec2 minUv = (vec2(origin) + vec2(0.5)) / atlasSize;
    vec2 maxUv = (vec2(origin) + vec2(resolution - 0.5)) / atlasSize;
    vec2 atlasUv = (vec2(origin) + tileUv * resolution) / atlasSize;
    atlasUv = clamp(atlasUv, minUv, maxUv);
    return clamp(texture(uGpuTireMarkAtlas, atlasUv).r, 0.0, 1.0);
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



float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 hash22(vec2 p)
{
    float n = hash12(p);
    float m = hash12(p + vec2(19.19, 73.73));
    return vec2(n, m);
}

vec3 rainImpactRippleNormal(
    vec2 stableWorldXZ,
    vec2 flowDir,
    float rainIntensity,
    float standingDepthM,
    float runoffDepthM,
    float standing,
    float runoff)
{
    // Procedural rain ripples are presentation-only and are intentionally
    // concentrated in actual free water. Mere wet film receives little or no
    // circular ripple response; deeper standing water gets the strongest rings.
    float puddleGate = smoothstep(0.00001, 0.00008, standingDepthM) * standing;
    float deepPuddle = smoothstep(0.00010, 0.00035, standingDepthM) * standing;
    float runoffGate = smoothstep(0.00005, 0.00030, runoffDepthM) * runoff * 0.10;
    float rippleMask = clamp(puddleGate + runoffGate, 0.0, 1.0)
        * smoothstep(0.02, 0.25, rainIntensity);
    if (rippleMask <= 0.0001)
        return vec3(0.0, 1.0, 0.0);

    // Around 30 cm cells gives a dense but not overwhelming rainfall pattern.
    const float rippleCellPerM = 3.2;
    vec2 p = stableWorldXZ * rippleCellPerM;
    ivec2 baseCell = ivec2(floor(p));
    vec2 grad = vec2(0.0);
    float activity = 0.0;

    for (int oy = 0; oy <= 1; ++oy)
    {
        for (int ox = 0; ox <= 1; ++ox)
        {
            ivec2 cell = baseCell + ivec2(ox, oy);
            vec2 cellf = vec2(cell);
            vec2 jitter = hash22(cellf);
            vec2 center = cellf + jitter;
            vec2 toPoint = p - center;
            float dist = length(toPoint);
            if (dist > 1.25)
                continue;

            float seed = hash12(cellf + vec2(7.17, 3.41));
            float life = fract(uSurfacePresentationTime
                * mix(0.70, 1.30, seed)
                * mix(0.85, 1.35, rainIntensity)
                + seed);
            float radius = mix(0.05, 0.95, life);
            float width = mix(0.070, 0.030, life);
            float ring = max(1.0 - abs(dist - radius) / max(width, 1.0e-4), 0.0);
            ring *= (1.0 - life);
            if (ring <= 0.0 || dist <= 1.0e-4)
                continue;

            // Advect a small part of the ring response along the flow so puddle
            // edges/readout remain circular while moving water feels slightly
            // stretched/broken by the current.
            vec2 dir = toPoint / dist;
            vec2 advectedDir = normalize(mix(dir, flowDir, runoff * 0.18));
            float slopeSign = clamp((radius - dist) / max(width, 1.0e-4), -1.0, 1.0);
            float amplitude = mix(0.070, 0.125, deepPuddle) * rippleMask;
            grad += advectedDir * (slopeSign * ring * amplitude);
            activity += ring;
        }
    }

    if (activity <= 1.0e-5)
        return vec3(0.0, 1.0, 0.0);

    return normalize(vec3(-grad.x, 1.0, -grad.y));
}
float dynamicSurfacePresentationDepth(vec3 positionRelative, float physicalDepthM)
{
    // LIVETRACK15 keeps one physical depth interpretation across both topology LODs.
    // The near/far prebaked atlases only change spatial resolution; ordinary
    // material wetness remains a separate PBR response.
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
    float runoffStrength,
    vec2 flowDirection,
    float waterSurfaceHeightRelativeY,
    float dynamicSurfaceLodDetail)
{
    if (filmWetness <= 0.0001
        && puddleDepthM <= 0.000008
        && presentationDepthM <= 0.000008)
        return litColor;

    vec2 stableWorldXZ = vWorldPosition.xz + uSurfacePatternCameraModuloXZ;
    // Retained in the interface for future SSR/refraction. Dynamic Surface avoids
    // deriving optical normals from page-cache derivatives.
    float stableHeadReference = waterSurfaceHeightRelativeY * 0.0;
    stableWorldXZ += vec2(stableHeadReference);

    // Thin weather film and free-surface puddle optics are intentionally
    // separate: the former darkens/smooths material, the latter gets water
    // reflection/refraction behavior only where prebaked basin depth exists.
    float film = clamp(filmWetness, 0.0, 1.0);
    // LIVETRACK21I keeps the two water regimes separate. The baked ladder owns
    // standing depth; the independent kinematic solver owns runoff depth. The
    // old max(standing,runoff) optical depth hid ladder changes whenever runoff
    // happened to be deeper.
    float freeSurfaceDepthM = max(puddleDepthM, 0.0);
    float runoffDepthM = max(presentationDepthM, 0.0);
    // Downhill-flow direction mildly reduces the retained-water optical mask,
    // keeping sloped runoff less mirror-like than a static basin center.
    float standing = clamp(freeSurfaceMask, 0.0, 1.0);
    float runoff = clamp(runoffStrength, 0.0, 1.0);
    vec2 flowDir = length(flowDirection) > 0.0
        ? normalize(flowDirection)
        : vec2(0.0, 1.0);
    float deepPool = smoothstep(0.00030, kStandingWaterMaxDepthM,
        freeSurfaceDepthM) * standing;

    // Wet porous material darkens appreciably because liquid fills the air gaps
    // that normally scatter light. This is the ordinary rainy-road response and
    // should read as wet asphalt/soil, not as a clear sheet of plastic.
    float wetDarkening = mix(
        0.88,
        0.68,
        smoothstep(0.20, 0.95, clamp(surfaceRoughness, 0.0, 1.0)));
    vec3 wetSubstrate = litColor * mix(1.0, wetDarkening, film);
    vec3 filmColor = mix(litColor, wetSubstrate, film * 0.88);

    // Microscopic rain film is allowed to become optically visible from 0.001mm,
    // while full free-surface behavior is still weighted by resolved basin strength.
    // This keeps the early wetting phase readable without inventing a horizontal
    // puddle plane on every ordinary sloped receiver.
    // Even before a resolved puddle exists, a connected rain film should be
    // able to show a very thin flowing sheen on roads and a tiny splash/ripple
    // response to the falling rain.
    // LIVETRACK18: runoff is intentionally patchy rather than a uniform glass
    // sheet. Stretch the world-locked breakup field along the prebaked flow
    // direction so low wet lanes read as irregular runoff ribbons/gutters.
    vec2 crossFlow = vec2(-flowDir.y, flowDir.x);
    float alongFlowM = dot(stableWorldXZ, flowDir);
    float acrossFlowM = dot(stableWorldXZ, crossFlow);
    float runoffPatch = 1.0;
    if (runoff > 0.001)
    {
        // One breakup fetch is sufficient here; the former two-sample blend was
        // paid by nearly every rainy-road fragment before the early-out.
        float patchA = texture(
            uSurfaceWetnessBreakupMask,
            vec2(alongFlowM * 0.016, acrossFlowM * 0.082)).r;
        runoffPatch = mix(0.18, 1.0, smoothstep(0.28, 0.76, patchA));
    }
    float runoffDepthMask = runoff
        * smoothstep(0.000005, 0.00060, runoffDepthM);
    float runoffSheet = runoff * max(film, 0.12) * runoffPatch
        * (1.0 - standing) * smoothstep(0.25, 0.90, film);
    // Kinematic runoff is now a real water-depth regime, not a faint cosmetic
    // sheen. Keep it less mirror-like than ponded water, but strong enough for
    // gutter streams and broad downhill sheets to be unmistakable.
    float opticalWaterMask = max(
        standing,
        max(runoffSheet * 0.18, runoffDepthMask * 0.34));
    if (opticalWaterMask <= 0.001)
        return filmColor;

    float upward = clamp(shadingNormal.y, 0.0, 1.0);
    // A retained puddle flattens toward a horizontal free surface. Moving runoff
    // remains attached to the road slope and receives only a tiny smoothing term.
    float flattening = standing * smoothstep(0.15, 0.88, upward)
        * mix(0.26, 0.90, deepPool)
        + runoffDepthMask * 0.07 * upward;
    vec3 waterNormal = normalize(mix(
        shadingNormal,
        vec3(0.0, 1.0, 0.0),
        flattening));

    // LIVETRACK21J: keep the subtle directional flow sheen, but add actual
    // rain-impact ripple rings mainly in real standing water. This is a
    // presentation-only effect; it does not alter the hydrology solver.
    // Begin optical impact ripples shortly after rain creates a coherent film;
    // standing-depth gating below still prevents circles on merely damp asphalt.
    float rippleRain = smoothstep(0.000005, 0.000250, uRainWettingExposureM);
    float breakupScale = mix(0.035, 0.075, 0.5 + 0.5 * runoff);
    vec2 breakupUv = stableWorldXZ * breakupScale
        - flowDir * (0.021 * uSurfacePresentationTime);
    const vec2 rippleStep = vec2(0.0105, 0.0);
    float a0 = texture(uSurfaceWetnessBreakupMask, breakupUv).r;
    float ax = texture(uSurfaceWetnessBreakupMask, breakupUv + rippleStep.xy).r;
    float az = texture(uSurfaceWetnessBreakupMask, breakupUv + rippleStep.yx).r;
    vec2 rippleGrad = vec2(ax - a0, az - a0);
    vec2 flowRipple = flowDir * dot(rippleGrad, flowDir) * (0.26 + 0.18 * runoff);
    vec3 flowRippleNormal = normalize(vec3(
        -(rippleGrad.x * 1.30 + flowRipple.x),
        1.0,
        -(rippleGrad.y * 1.30 + flowRipple.y)));
    float flowRippleMask = rippleRain * (0.030 + 0.055 * runoff + 0.015 * standing);
    waterNormal = normalize(mix(waterNormal, flowRippleNormal, flowRippleMask));

    vec3 impactRippleNormal = rainImpactRippleNormal(
        stableWorldXZ,
        flowDir,
        rippleRain,
        freeSurfaceDepthM,
        runoffDepthM,
        standing,
        runoff);
    float puddleRippleMask = rippleRain * clamp(
        smoothstep(0.00001, 0.00008, freeSurfaceDepthM) * (0.30 + 0.70 * standing)
        + smoothstep(0.00005, 0.00030, runoffDepthM) * runoff * 0.06,
        0.0,
        1.0);
    waterNormal = normalize(mix(waterNormal, impactRippleNormal, puddleRippleMask));
    float distanceDetail = (1.0 - smoothstep(180.0, 500.0, length(vWorldPosition.xz)))
        * clamp(dynamicSurfaceLodDetail, 0.0, 1.0);

    float ndv = clamp(dot(waterNormal, viewDirection), 0.0, 1.0);
    float deepNight = 1.0 - smoothstep(0.020, 0.280, uDayNightCycle);
    float fresnelFull = 0.0204 + 0.9796 * pow(1.0 - ndv, 5.0);
    float fresnel = mix(0.0204, fresnelFull, distanceDetail);

    vec3 reflected = vec3(0.20, 0.24, 0.28);
    if (uHasEnvironmentMap)
    {
        vec3 reflectionDirection = reflect(-viewDirection, waterNormal);
        float lod = mix(min(2.2, uEnvironmentMaxLod), 0.03, opticalWaterMask);
        reflected = textureLod(uEnvironmentMap, reflectionDirection, lod).rgb;
        // Slight cool-water bias without painting the environment blue.
        reflected *= mix(vec3(1.0), vec3(0.965, 0.995, 1.060), opticalWaterMask * 0.32);
        reflected *= mix(1.0, 0.12, deepNight);
    }
    else
    {
        float skyWeight = clamp(waterNormal.y * 0.5 + 0.5, 0.0, 1.0);
        reflected = mix(vec3(0.008, 0.010, 0.014),
            vec3(0.06, 0.08, 0.11), skyWeight);
        reflected *= mix(1.0, 0.18, deepNight);
    }

    // Beer-Lambert transmission uses only the excess free-surface depth. The
    // retained film is already represented by the darkened authored material.
    float opticalPathM = freeSurfaceDepthM / max(ndv, 0.25);
    vec3 absorptionPerM = vec3(0.80, 0.34, 0.16);
    vec3 transmittance = exp(-absorptionPerM * opticalPathM);
    vec3 transmitted = wetSubstrate * transmittance;

    // A restrained blue/cyan bias helps standing water separate from merely
    // dark wet asphalt while preserving the authored material underneath.
    transmitted *= mix(vec3(1.0), vec3(0.952, 0.982, 1.060), opticalWaterMask * 0.32);
    vec3 deepWaterTint = vec3(0.020, 0.048, 0.070);
    transmitted = mix(transmitted, deepWaterTint, deepPool * 0.24);

    // Physical dielectric Fresnel owns the inexpensive puddle reflection. At a
    // near-normal view the roughly two-percent water F0 leaves the asphalt
    // visible through the complete supported 0..32mm depth range. At grazing
    // angles the environment becomes dominant, as real shallow water does. The
    // small sub-millimetre depth response prevents a newly wet road from turning
    // into a mirror before a coherent free surface has formed.
    float opticalSurfaceFormation = mix(
        0.48,
        1.0,
        smoothstep(0.000010, kStandingWaterMaxDepthM, freeSurfaceDepthM));
    float reflectionWeight = min(
        fresnel * opticalSurfaceFormation * mix(1.0, 1.34, opticalWaterMask), 0.95);
    reflectionWeight *= mix(1.0, 0.22, deepNight);
    vec3 standingColor = mix(transmitted, reflected, reflectionWeight);

    // The free-surface layer grows only from depth above the retained film.
    // Everywhere else remains ordinary wet material.
    return mix(filmColor, standingColor, opticalWaterMask);
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
    float dynamicSurfaceRunoff = 0.0;
    vec2 dynamicSurfaceFlowDirection = vec2(0.0, 1.0);
    float dynamicSurfaceLodDetail = 1.0;
    if (uSurfaceWetnessReceiver)
    {
        if (uGpuDynamicSurfaceAuthorityActive)
        {
            float gpuLodDetail = 0.0;
            GpuWaterDecoded gpuState;
            bool gpuValid = gpuWaterFiltered(vWorldPosition, gpuState, gpuLodDetail);
            dynamicSurfaceLodDetail = gpuLodDetail;
            float standingDepthM = gpuValid ? max(gpuState.depthM, 0.0) : 0.0;
            float runningDepthM = gpuValid ? kinematicRunoffDepthM(gpuState) : 0.0;
            dynamicSurfaceDepthM = max(standingDepthM, runningDepthM);
            float gpuDryLineStrength = gpuValid ? clamp(gpuState.dryLine, 0.0, 1.0) : 0.0;
            float basinStrength = gpuValid ? clamp(gpuState.basinStrength, 0.0, 1.0) : 0.0;
            float runoffPotential = gpuValid ? clamp(gpuState.runoffPotential, 0.0, 1.0) : 0.0;
            float flowStrength = gpuValid ? clamp(gpuState.flowStrength, 0.0, 1.0) : 0.0;
            dynamicSurfaceFlowDirection = gpuValid && flowStrength > 0.001
                ? gpuState.flowDirection : vec2(0.0, 1.0);

            // LIVETRACK21C: 0.01 mm presentation onset. This is above the microscopic
            // wet-film regime but low enough that shallow real road puddles no longer
            // disappear completely behind the conservative storage model.
            const float visibleWaterOnsetM = 0.000010;
            dynamicSurfacePuddleDepthM = standingDepthM;
            // The second optical depth argument is runoff only. Standing water
            // is passed separately as dynamicSurfacePuddleDepthM so the baked
            // 4-bit ladder cannot be masked by a deeper continuous runoff film.
            dynamicSurfaceVisualDepthM = dynamicSurfacePresentationDepth(
                vWorldPosition, runningDepthM);
            dynamicSurfaceWaterSurfaceHeightRelativeY =
                vWorldPosition.y + standingDepthM;

            // LIVETRACK21 removes the procedural rain-impact circles completely.
            // Rain now creates one continuous microscopic material film. Visible
            // free water comes only from the two hydrology regimes above:
            // priority-flood standing storage or MFD kinematic runoff.
            float weatherWetness = clamp(uSurfaceWeatherFilmWetness, 0.0, 1.0);
            float wettingExposureM = clamp(uRainWettingExposureM, 0.0, 0.004);
            float exposureWetness = smoothstep(0.000010, 0.00120, wettingExposureM);
            float filmAuthority = max(weatherWetness, exposureWetness * 0.94);
            dynamicSurfaceFilm = filmAuthority
                * (1.0 - 0.62 * gpuDryLineStrength);
            dynamicSurfaceFilm = max(
                dynamicSurfaceFilm,
                smoothstep(0.000001, 0.00050, dynamicSurfaceDepthM)
                    * (1.0 - 0.35 * gpuDryLineStrength));
            float runningPresence = smoothstep(0.000005, 0.00035, runningDepthM);
            float routeStrength = mix(0.50, 0.94, flowStrength)
                * smoothstep(0.04, 0.50, runoffPotential);
            dynamicSurfaceRunoff = runningPresence * routeStrength
                * dynamicSurfaceLodDetail;

            float waterPresence = smoothstep(
                visibleWaterOnsetM * 0.75, visibleWaterOnsetM * 2.0,
                standingDepthM);
            float waterBodyStrength = mix(
                0.12,
                1.0,
                smoothstep(visibleWaterOnsetM, kStandingWaterMaxDepthM,
                    standingDepthM));
            // Only actual retained basin depth is standing water now. Sloped
            // runoff has its own optical response and never masquerades as a
            // horizontal puddle merely because the road is wet.
            float basinWater = waterPresence * basinStrength * waterBodyStrength;
            dynamicSurfaceStanding = basinWater
                * dynamicSurfaceLodDetail
                * (1.0 - 0.10 * flowStrength);
        }
        else
        {
            // No second/legacy Hydro renderer exists. If the prebaked GPU
            // topology is unavailable, the scene still looks rain-wet but no
            // detailed standing water is invented. Once .hhyd is ready the
            // same fixed GPU path above becomes authoritative.
            dynamicSurfaceFilm = clamp(uSurfaceWeatherFilmWetness, 0.0, 1.0);
            dynamicSurfaceDepthM = 0.0;
            dynamicSurfacePuddleDepthM = 0.0;
            dynamicSurfaceVisualDepthM = 0.0;
            dynamicSurfaceStanding = 0.0;
            dynamicSurfaceRunoff = 0.0;
        }
    }

    // LIVETRACK22: close skid marks are part of the road material itself, so
    // they cannot z-fight against coplanar road triangles. Their contrast is
    // continuously reduced by the SAME live wet-film/standing-water state; a
    // rain-soaked road no longer carries dry-looking black ribbons. Distance
    // visibility is a slow 0..500m master fade, matching the far-vector LOD.
    if (uSurfaceWetnessReceiver && uGpuDynamicSurfaceAuthorityActive
        && uGpuDynamicSurfaceTireMarksReady)
    {
        float tireMark = gpuTireMarkStrength(vWorldPosition);
        float markDistanceM = length(vWorldPosition.xz);
        float markRangeVisibility = 1.0 - smoothstep(0.0, 500.0, markDistanceM);
        // Complement the far-vector 85..110m handoff rather than double-darkening
        // the road while both representations are available.
        float nearTileWeight = 1.0 - smoothstep(85.0, 110.0, markDistanceM);
        float wetAuthority = clamp(
            max(dynamicSurfaceFilm, dynamicSurfaceStanding * 1.15), 0.0, 1.0);
        float wetVisibility = mix(
            1.0, 0.14, smoothstep(0.05, 0.88, wetAuthority));
        wetVisibility *= 1.0 - 0.48 * dynamicSurfaceStanding;
        float markVisual = clamp(
            tireMark * markRangeVisibility * nearTileWeight * wetVisibility, 0.0, 1.0);
        baseColor *= mix(vec3(1.0), vec3(0.42), markVisual);
    }

    float roughness = clamp(uMaterialRoughness, 0.04, 1.0);
    if (uHasRoughnessMap)
        roughness = clamp(
            sampleChannel(texture(uRoughnessMap, vTexCoord), uRoughnessChannel),
            0.04,
            1.0);

    // Thin-film roughness is driven by the smooth weather film, not
    // adaptive hydrology depth. This is also the deliberately cheap far-field
    // fallback outside the 500m prebaked presentation range: rain gradually darkens and
    // smooths authored receivers, so the environment and sun produce wet-road
    // highlights without pretending that distant puddles were simulated.
    if (uSurfaceWetnessReceiver && dynamicSurfaceFilm > 0.0001)
    {
        // Preserve authored normal detail and a material-dependent roughness
        // floor. A wet rough surface becomes glossy, but remains much rougher
        // than the locally simulated free-water surface below.
        float wetRoughness = max(0.20, roughness * 0.62);
        float filmOpticalResponse = smoothstep(
            0.02,
            0.95,
            dynamicSurfaceFilm);
        roughness = mix(
            roughness,
            wetRoughness,
            filmOpticalResponse * 0.72);
    }
    if (dynamicSurfaceStanding > 0.0001)
    {
        roughness = mix(
            roughness,
            0.045,
            dynamicSurfaceStanding * 0.94);
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
    float nDotV = max(dot(normal, viewDirection), 0.0001);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    vec3 directLighting=vec3(0.0);
    vec3 directRadiance=max(uSunRadiance,vec3(0.0));
    float sunPower=max(directRadiance.r,max(directRadiance.g,directRadiance.b));
    // CELESTIAL07: material lighting keeps only broad regional cloud
    // transmission. The detailed moving cookie is applied once later by the
    // dedicated post-opaque receiver, avoiding the former double attenuation.
    vec3 celestialCloudTransmission=sunPower>0.00001
        ?volumetricCloudSunTransmission(vWorldPosition,lightDirection):vec3(1.0);
    float celestialCloudVisibility=clamp(
        dot(celestialCloudTransmission,vec3(0.2126,0.7152,0.0722)),0.0,1.0);
    // CLOUDURP15BK: do not evaluate GGX, cloud-shadow lookup or cascaded shadow
    // filtering for back-facing fragments (or when the astronomical Sun is off).
    // Previously those costs were paid even though nDotL multiplied the result by 0.
    if(nDotL>0.0001 && sunPower>0.00001)
    {
        vec3 halfwayDirection=normalize(viewDirection+lightDirection);
        float hDotV=max(dot(halfwayDirection,viewDirection),0.0);
        float d=distributionGGX(normal,halfwayDirection,roughness);
        float g=geometrySmith(normal,viewDirection,lightDirection,roughness);
        vec3 f=fresnelSchlick(hDotV,f0);
        vec3 directSpecular=(d*g*f)/max(4.0*nDotV*nDotL,0.0001);
        vec3 directDiffuseWeight=(vec3(1.0)-f)*(1.0-metallic);
        directRadiance*=celestialCloudTransmission;
        float filteredSunPower=max(directRadiance.r,max(directRadiance.g,directRadiance.b));
        float sunVisibility=filteredSunPower>0.00001?sampleSunShadow(normal,lightDirection):1.0;
        directLighting=(directDiffuseWeight*baseColor/PI+directSpecular)
            *directRadiance*nDotL*sunVisibility;
    }

    vec3 ambientLighting;
    if (uHasEnvironmentMap)
    {
        // GFX6 foundation: the cubemap's generated mip chain provides a
        // deterministic roughness-aware reflection approximation. A future
        // GGX prefilter/BRDF LUT can replace this without changing materials.
        vec3 reflectionDirection = reflect(-viewDirection, normal);
        vec3 diffuseEnvironment = mix(
            textureLod(uEnvironmentMapPrevious, normal, uEnvironmentMaxLod).rgb,
            textureLod(uEnvironmentMap, normal, uEnvironmentMaxLod).rgb,
            uEnvironmentBlend);
        vec3 specularEnvironment = mix(
            textureLod(uEnvironmentMapPrevious, reflectionDirection, roughness * uEnvironmentMaxLod).rgb,
            textureLod(uEnvironmentMap, reflectionDirection, roughness * uEnvironmentMaxLod).rgb,
            uEnvironmentBlend);

        vec3 environmentFresnel =
            fresnelSchlickRoughness(nDotV, f0, roughness);
        vec3 environmentDiffuseWeight =
            (vec3(1.0) - environmentFresnel) * (1.0 - metallic);
        vec3 diffuseIbl = diffuseEnvironment * baseColor
            * environmentDiffuseWeight * 0.72;
        vec3 specularIbl = specularEnvironment * environmentFresnel
            * mix(1.0, 0.55, roughness);
        ambientLighting = (diffuseIbl + specularIbl) * ao;
        float ambientDeepNight = 1.0 - smoothstep(0.020, 0.280, uDayNightCycle);
        ambientLighting *= mix(1.0, 0.12, ambientDeepNight);
    }
    else
    {
        float upFacing = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
        float hemisphere = mix(0.12, 0.30, upFacing);
        ambientLighting = baseColor * (1.0 - metallic) * hemisphere * ao;
        float ambientDeepNight = 1.0 - smoothstep(0.020, 0.280, uDayNightCycle);
        ambientLighting *= mix(1.0, 0.12, ambientDeepNight);
    }

    // Broad regional cloud cover blocks only part of the hemispherical sky.
    // Modulate diffuse/IBL modestly here; detailed moving cloud-shadow structure
    // is deliberately not sampled in this material path anymore.
    float cloudAmbientVisibility=mix(0.62,1.0,pow(celestialCloudVisibility,0.70));
    vec3 cloudAmbientTint=mix(vec3(0.84,0.90,1.0),vec3(1.0),celestialCloudVisibility);
    ambientLighting*=cloudAmbientVisibility*cloudAmbientTint;

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
            dynamicSurfaceRunoff,
            dynamicSurfaceFlowDirection,
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

vec3 deformTireShadowWorldPosition(vec3 localPosition, vec3 worldPosition)
{
    if (!uTireVisualEnabled || !uTireVisualDeformationFieldValid)
        return worldPosition;

    vec3 axleLocal = tireAxisVector(uTireVisualAxleAxis);
    vec3 relativeLocal = localPosition - uTireVisualCenter;
    float axialLocal = dot(relativeLocal, axleLocal);
    vec3 radialLocal = relativeLocal - axleLocal * axialLocal;
    float radiusLocal = length(radialLocal);
    float radialSpanLocal = max(uTireVisualOuterRadius
        - uTireVisualInnerRadius, 0.0001);
    if (radiusLocal <= 0.000001)
        return worldPosition;

    vec3 forwardWorld = uTireWheelForwardWorld;
    forwardWorld = dot(forwardWorld, forwardWorld) > 0.000001
        ? normalize(forwardWorld) : vec3(0.0, 0.0, 1.0);
    vec3 rightWorld = uTireWheelRightWorld;
    rightWorld -= forwardWorld * dot(rightWorld, forwardWorld);
    rightWorld = dot(rightWorld, rightWorld) > 0.000001
        ? normalize(rightWorld) : vec3(1.0, 0.0, 0.0);
    vec3 upWorld = uTireWheelUpWorld;
    upWorld -= forwardWorld * dot(upWorld, forwardWorld);
    upWorld -= rightWorld * dot(upWorld, rightWorld);
    upWorld = dot(upWorld, upWorld) > 0.000001
        ? normalize(upWorld) : normalize(cross(forwardWorld, rightWorld));
    if (dot(cross(forwardWorld, rightWorld), upWorld) < 0.0)
        upWorld = -upWorld;
    vec3 downWorld = -upWorld;

    vec3 centerWorld = (uModel * vec4(uTireVisualCenter, 1.0)).xyz;
    vec3 relativeWorld = worldPosition - centerWorld;
    vec3 radialWorld = relativeWorld
        - rightWorld * dot(relativeWorld, rightWorld);
    float radialWorldLength = length(radialWorld);
    if (radialWorldLength <= 0.000001)
        return worldPosition;
    vec3 radialDirectionWorld = radialWorld / radialWorldLength;
    float theta = atan(dot(radialDirectionWorld, downWorld),
        dot(radialDirectionWorld, forwardWorld));

    vec3 authoredAxleWorld = mat3(uModel) * axleLocal;
    float axleSign = dot(authoredAxleWorld, rightWorld) < 0.0 ? -1.0 : 1.0;
    float widthCoordinate = clamp(axleSign * axialLocal
        / max(uTireVisualHalfWidth, 0.0001), -1.0, 1.0);
    vec3 fieldM = sampleTireShadowField(theta, widthCoordinate);
    float radialFraction = clamp((radiusLocal - uTireVisualInnerRadius)
        / radialSpanLocal, 0.0, 1.0);
    float attachment = smoothstep(0.015, 0.62, radialFraction);
    worldPosition += attachment
        * (forwardWorld * fieldM.x
            + downWorld * fieldM.y
            + rightWorld * fieldM.z);
    return worldPosition;
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
    vec4 worldPosition = uModel * localPosition;
    worldPosition.xyz = deformTireShadowWorldPosition(
        localPosition.xyz, worldPosition.xyz);

    // SHADOW02: keep the vertex result in camera-relative world space. A
    // layered geometry stage fans the triangle out only to the cascades whose
    // frusta contain this draw range. This removes the old CPU-side four-pass
    // submission loop without changing the shadow-map coordinate system.
    gl_Position = worldPosition;
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
