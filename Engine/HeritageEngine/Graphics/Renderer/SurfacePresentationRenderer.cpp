#include "SurfacePresentationRenderer.hpp"

#include "../ShaderProgram.hpp"
#include "../LodTransitionPolicy.hpp"
#include "../PresentationPrecision.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace heritage::graphics {
namespace {

#ifdef _WIN32
#define HERITAGE_SURFACE_GLSL_VERSION "#version 460 core\n"
#else
#define HERITAGE_SURFACE_GLSL_VERSION "#version 330 core\n"
#endif

const char* kTrackVertexShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
uniform mat4 uView;
uniform mat4 uProjection;
out vec4 vColor;
void main()
{
    vColor = aColor;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
)glsl";

const char* kTrackFragmentShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
in vec4 vColor;
uniform float uGamma;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
out vec4 FragColor;
void main()
{
    vec3 color = pow(clamp(vColor.rgb, 0.0, 1.0), vec3(1.0 / max(uGamma, 0.01)));
    color = (color - 0.5) * uContrast + 0.5 + uBrightness;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, uSaturation);
    FragColor = vec4(clamp(color, 0.0, 1.0), vColor.a);
}
)glsl";

// WATER15: settled water is rendered by EntityMeshRenderer's depth-reconstructed
// Dynamic Track surface-state pass. SurfacePresentationRenderer now owns only
// tire marks, rubber/marbles, particles and engineering debug overlays.
const char* kTireMarkVertexShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aStartLocal;
layout(location=1) in vec3 aEndLocal;
layout(location=2) in vec3 aStartNormal;
layout(location=3) in vec3 aEndNormal;
layout(location=4) in vec3 aStartRight;
layout(location=5) in vec3 aEndRight;
layout(location=6) in vec4 aStartData; // width, intensity, inside, centre
layout(location=7) in vec4 aEndData;   // width, intensity, inside, centre
layout(location=8) in vec4 aMisc;      // startOutside, endOutside, birthTime, flags

out TireMarkRecord
{
    vec3 startLocal;
    vec3 endLocal;
    vec3 startNormal;
    vec3 endNormal;
    vec3 startRight;
    vec3 endRight;
    vec4 startData;
    vec4 endData;
    vec4 misc;
} vRecord;

void main()
{
    vRecord.startLocal = aStartLocal;
    vRecord.endLocal = aEndLocal;
    vRecord.startNormal = aStartNormal;
    vRecord.endNormal = aEndNormal;
    vRecord.startRight = aStartRight;
    vRecord.endRight = aEndRight;
    vRecord.startData = aStartData;
    vRecord.endData = aEndData;
    vRecord.misc = aMisc;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
)glsl";

const char* kTireMarkGeometryShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
layout(points) in;
layout(triangle_strip, max_vertices=36) out;

in TireMarkRecord
{
    vec3 startLocal;
    vec3 endLocal;
    vec3 startNormal;
    vec3 endNormal;
    vec3 startRight;
    vec3 endRight;
    vec4 startData;
    vec4 endData;
    vec4 misc;
} gRecord[];

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uChunkOriginRelative;
uniform float uPresentationTime;
uniform float uHistoryFloorBirthTime;
uniform float uRetirementSeconds;
uniform float uDetailedDistance;
uniform float uLodBlendWidth;
uniform float uDrawDistance;
uniform float uVisibilityFadeWidth;
uniform float uCapDistance;

out float gLateral;
out float gEdgeFeather;
out float gAlpha;

float smooth01(float value)
{
    float t = clamp(value, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

vec3 normalizeSafe(vec3 value, vec3 fallbackValue)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 1.0e-10
        ? value * inversesqrt(lengthSquared)
        : fallbackValue;
}

float pressureMultiplier(vec3 loads, float lateral)
{
    float u = clamp(lateral, -1.0, 1.0);
    float load = loads.y;
    if (u < 0.0)
    {
        float t = clamp(-u * 1.5, 0.0, 1.0);
        load = mix(loads.y, loads.x, t);
    }
    else
    {
        float t = clamp(u * 1.5, 0.0, 1.0);
        load = mix(loads.y, loads.z, t);
    }
    float relativeToUniform = clamp(load * 3.0 - 1.0, -0.75, 0.75);
    return 1.0 + relativeToUniform * 0.38;
}

float edgeEnvelope(float lateral)
{
    float u = clamp(abs(lateral), 0.0, 1.0);
    return sqrt(max(1.0 - u * u, 0.0));
}

float visibilityWeight(float distanceMeters)
{
    if (distanceMeters >= uDrawDistance)
        return 0.0;
    float fadeStart = max(0.0, uDrawDistance - uVisibilityFadeWidth);
    if (distanceMeters <= fadeStart)
        return 1.0;
    return 1.0 - smooth01(
        (distanceMeters - fadeStart) / max(uDrawDistance - fadeStart, 0.0001));
}

void emitMarkVertex(vec3 position, float lateral, float edgeFeatherMix, float alpha)
{
    gLateral = lateral;
    gEdgeFeather = edgeFeatherMix;
    gAlpha = alpha;
    gl_Position = uProjection * uView * vec4(position, 1.0);
    EmitVertex();
}

void main()
{
    float birthTime = gRecord[0].misc.z;
    if (birthTime + 0.0001 < uHistoryFloorBirthTime)
        return;

    float ageSeconds = max(uPresentationTime - birthTime, 0.0);
    if (ageSeconds >= uRetirementSeconds)
        return;

    vec3 startCenter = uChunkOriginRelative + gRecord[0].startLocal;
    vec3 endCenter = uChunkOriginRelative + gRecord[0].endLocal;
    vec3 midpoint = (startCenter + endCenter) * 0.5;
    float distanceMeters = length(midpoint.xz);
    if (distanceMeters >= uDrawDistance)
        return;

    float ageT = clamp(ageSeconds / max(uRetirementSeconds, 0.0001), 0.0, 1.0);
    float ageOpacity = 1.0 - smooth01(ageT);
    float rangeVisibility = visibilityWeight(distanceMeters);
    if (ageOpacity * rangeVisibility <= 0.0001)
        return;

    float nearWeight = 1.0 - smooth01(
        (distanceMeters - uDetailedDistance) / max(uLodBlendWidth, 0.0001));
    float farDistanceT = clamp(
        (distanceMeters - uDetailedDistance)
            / max(uDrawDistance - uDetailedDistance, 0.0001),
        0.0, 1.0);
    float farOpacityScale = 0.60 - 0.12 * farDistanceT;
    float opacityScale = mix(farOpacityScale, 1.0, nearWeight)
        * rangeVisibility * ageOpacity;
    float detailMix = nearWeight;
    float edgeFeatherMix = clamp(1.0 - nearWeight, 0.0, 1.0);

    vec3 startNormal = normalizeSafe(gRecord[0].startNormal, vec3(0.0, 1.0, 0.0));
    vec3 endNormal = normalizeSafe(gRecord[0].endNormal, startNormal);
    vec3 startRight = normalizeSafe(gRecord[0].startRight, vec3(1.0, 0.0, 0.0));
    vec3 endRight = normalizeSafe(gRecord[0].endRight, startRight);
    startRight = normalizeSafe(startRight - startNormal * dot(startRight, startNormal), startRight);
    endRight = normalizeSafe(endRight - endNormal * dot(endRight, endNormal), endRight);
    vec3 tangentFallback = normalizeSafe(cross(startRight, startNormal), vec3(0.0, 0.0, 1.0));
    vec3 tangent = normalizeSafe(endCenter - startCenter, tangentFallback);

    float startHalfWidth = max(gRecord[0].startData.x * 0.5, 0.035);
    float endHalfWidth = max(gRecord[0].endData.x * 0.5, 0.035);
    vec3 startLoads = vec3(
        gRecord[0].startData.z, gRecord[0].startData.w, gRecord[0].misc.x);
    vec3 endLoads = vec3(
        gRecord[0].endData.z, gRecord[0].endData.w, gRecord[0].misc.y);
    int flags = int(floor(gRecord[0].misc.w + 0.5));

    bool detailed = distanceMeters < uDetailedDistance + uLodBlendWidth;
    int sampleCount = detailed ? 6 : 2;
    for (int lateralIndex = 0; lateralIndex < 6; ++lateralIndex)
    {
        if (lateralIndex >= sampleCount)
            break;
        float u = sampleCount == 2
            ? (-1.0 + 2.0 * float(lateralIndex))
            : (-1.0 + 2.0 * float(lateralIndex) / 5.0);
        float startEnvelope = mix(1.0, edgeEnvelope(u), detailMix);
        float endEnvelope = startEnvelope;
        float startPressure = mix(1.0, pressureMultiplier(startLoads, u), detailMix);
        float endPressure = mix(1.0, pressureMultiplier(endLoads, u), detailMix);
        float startAlpha = clamp(
            gRecord[0].startData.y * startEnvelope * startPressure
                * 0.78 * opacityScale,
            0.0, 0.82);
        float endAlpha = clamp(
            gRecord[0].endData.y * endEnvelope * endPressure
                * 0.78 * opacityScale,
            0.0, 0.82);
        vec3 startPosition = startCenter + startRight * (u * startHalfWidth);
        vec3 endPosition = endCenter + endRight * (u * endHalfWidth);
        emitMarkVertex(startPosition, u, edgeFeatherMix, startAlpha);
        emitMarkVertex(endPosition, u, edgeFeatherMix, endAlpha);
    }
    EndPrimitive();

    // Longitudinal 5.5 cm feather caps are only meaningful inside 100 m. They
    // are generated by the GPU from the same logical record and cost the CPU
    // nothing after the original segment upload.
    if (detailed && distanceMeters <= uCapDistance && (flags & 1) != 0)
    {
        for (int lateralIndex = 0; lateralIndex < 6; ++lateralIndex)
        {
            float u = -1.0 + 2.0 * float(lateralIndex) / 5.0;
            float envelope = mix(1.0, edgeEnvelope(u), detailMix);
            float pressure = mix(1.0, pressureMultiplier(startLoads, u), detailMix);
            float alpha = clamp(
                gRecord[0].startData.y * envelope * pressure
                    * 0.78 * opacityScale,
                0.0, 0.82);
            vec3 startPosition = startCenter + startRight * (u * startHalfWidth);
            vec3 capPosition = startPosition - tangent * 0.055;
            emitMarkVertex(capPosition, u, edgeFeatherMix, 0.0);
            emitMarkVertex(startPosition, u, edgeFeatherMix, alpha);
        }
        EndPrimitive();
    }

    if (detailed && distanceMeters <= uCapDistance && (flags & 2) != 0)
    {
        for (int lateralIndex = 0; lateralIndex < 6; ++lateralIndex)
        {
            float u = -1.0 + 2.0 * float(lateralIndex) / 5.0;
            float envelope = mix(1.0, edgeEnvelope(u), detailMix);
            float pressure = mix(1.0, pressureMultiplier(endLoads, u), detailMix);
            float alpha = clamp(
                gRecord[0].endData.y * envelope * pressure
                    * 0.78 * opacityScale,
                0.0, 0.82);
            vec3 endPosition = endCenter + endRight * (u * endHalfWidth);
            vec3 capPosition = endPosition + tangent * 0.055;
            emitMarkVertex(endPosition, u, edgeFeatherMix, alpha);
            emitMarkVertex(capPosition, u, edgeFeatherMix, 0.0);
        }
        EndPrimitive();
    }
}
)glsl";

const char* kTireMarkFragmentShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
in float gLateral;
in float gEdgeFeather;
in float gAlpha;
out vec4 FragColor;
void main()
{
    float featherStart = mix(1.0, 0.82, clamp(gEdgeFeather, 0.0, 1.0));
    float edgeMask = 1.0 - smoothstep(featherStart, 1.0, abs(gLateral));
    float alpha = clamp(gAlpha, 0.0, 1.0)
        * mix(1.0, edgeMask, clamp(gEdgeFeather, 0.0, 1.0));
    if (alpha <= 0.0001)
        discard;
    FragColor = vec4(0.0, 0.0, 0.0, alpha);
}
)glsl";


// TIRE16L resting marbles: one persistent GPU point record represents one
// authoritative TrackRubber cell. The geometry shader reconstructs the same
// irregular two-triangle flakes that used to be CPU tessellated every frame.
// The 100 m chunk is storage/batching only; random placement is derived from
// the authoritative cell centre and never from chunk edges.
const char* kMarbleVertexShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aCenterLocal;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aForward;
layout(location=3) in vec4 aState; // loose, maturity, severity, piecePopulation
layout(location=4) in vec4 aMisc;  // stable seed, cellSize, passCount, active

out MarbleRecord
{
    vec3 centerLocal;
    vec3 normal;
    vec3 forward;
    vec4 state;
    vec4 misc;
} vRecord;

void main()
{
    vRecord.centerLocal = aCenterLocal;
    vRecord.normal = aNormal;
    vRecord.forward = aForward;
    vRecord.state = aState;
    vRecord.misc = aMisc;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
)glsl";

const char* kMarbleGeometryShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
layout(points) in;
layout(triangle_strip, max_vertices=156) out;

in MarbleRecord
{
    vec3 centerLocal;
    vec3 normal;
    vec3 forward;
    vec4 state;
    vec4 misc;
} gRecord[];

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uChunkOriginRelative;
uniform float uDetailedDistance;
uniform float uLodBlendWidth;
uniform float uDrawDistance;
uniform float uVisibilityFadeWidth;

out float gShade;
out float gAlpha;

float smooth01(float value)
{
    float t = clamp(value, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

vec3 normalizeSafe(vec3 value, vec3 fallbackValue)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 1.0e-10
        ? value * inversesqrt(lengthSquared)
        : fallbackValue;
}

float visibilityWeight(float distanceMeters)
{
    if (distanceMeters >= uDrawDistance)
        return 0.0;
    float fadeStart = max(0.0, uDrawDistance - uVisibilityFadeWidth);
    if (distanceMeters <= fadeStart)
        return 1.0;
    return 1.0 - smooth01(
        (distanceMeters - fadeStart) / max(uDrawDistance - fadeStart, 0.0001));
}

uint nextRandom(inout uint state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

float random01(inout uint state)
{
    return float((nextRandom(state) >> 8u) & 0x00ffffffu) / 16777216.0;
}

void emitRubberVertex(vec3 position, float shade, float alpha)
{
    gShade = shade;
    gAlpha = alpha;
    gl_Position = uProjection * uView * vec4(position, 1.0);
    EmitVertex();
}

void emitFlake(
    vec3 center,
    vec3 normal,
    vec3 baseRight,
    vec3 baseForward,
    float angleRadians,
    float lengthM,
    float widthM,
    float curl,
    float liftM,
    float shade,
    float alpha)
{
    float c = cos(angleRadians);
    float q = sin(angleRadians);
    vec3 direction = normalizeSafe(baseForward * c + baseRight * q, baseForward);
    vec3 side = normalizeSafe(cross(normal, direction), baseRight);
    vec3 raised = center + normal * liftM;
    float halfLength = lengthM * 0.5;
    float halfWidth = widthM * 0.5;
    float bendAmplitude = curl * min(lengthM * 0.16, widthM * 0.75);

    vec3 p0 = raised + direction * -halfLength + side * -halfWidth;
    vec3 p1 = raised + direction * halfLength + side * -halfWidth
        + normal * bendAmplitude;
    vec3 p2 = raised + direction * halfLength + side * halfWidth;
    vec3 p3 = raised + direction * -halfLength + side * halfWidth
        - normal * bendAmplitude * 0.72;

    emitRubberVertex(p0, shade, alpha);
    emitRubberVertex(p1, shade, alpha);
    emitRubberVertex(p2, shade, alpha);
    EndPrimitive();
    emitRubberVertex(p0, shade, alpha);
    emitRubberVertex(p2, shade, alpha);
    emitRubberVertex(p3, shade, alpha);
    EndPrimitive();
}

void main()
{
    float looseRubber = max(gRecord[0].state.x, 0.0);
    float maturity = clamp(gRecord[0].state.y, 0.0, 1.0);
    float severity = clamp(gRecord[0].state.z, 0.0, 1.0);
    float piecePopulation = max(gRecord[0].state.w, 0.0);
    if (gRecord[0].misc.w < 0.5
        || (looseRubber <= 0.0012 && piecePopulation < 0.45))
    {
        return;
    }

    vec3 center = uChunkOriginRelative + gRecord[0].centerLocal;
    float distanceMeters = length(center.xz);
    float rangeVisibility = visibilityWeight(distanceMeters);
    if (rangeVisibility <= 0.0001)
        return;

    float nearWeight = 1.0 - smooth01(
        (distanceMeters - uDetailedDistance) / max(uLodBlendWidth, 0.0001));
    float farWeight = 1.0 - nearWeight;
    float cellSize = max(gRecord[0].misc.y, 0.05);
    vec3 normal = normalizeSafe(gRecord[0].normal, vec3(0.0, 1.0, 0.0));
    vec3 forward = normalizeSafe(gRecord[0].forward, vec3(0.0, 0.0, 1.0));
    forward = normalizeSafe(forward - normal * dot(forward, normal), vec3(0.0, 0.0, 1.0));
    vec3 right = normalizeSafe(cross(normal, forward), vec3(1.0, 0.0, 0.0));
    forward = normalizeSafe(cross(right, normal), forward);
    uint baseSeed = max(uint(gRecord[0].misc.x + 0.5), 1u);

    if (farWeight > 0.0001 && (looseRubber > 0.0035 || piecePopulation >= 1.0))
    {
        float populationSignal = clamp(piecePopulation / 18.0, 0.0, 1.0);
        float alpha = clamp(
            max(looseRubber * (0.12 + 0.07 * maturity), populationSignal * 0.10),
            0.0, 0.14) * farWeight * rangeVisibility;
        int farCount = piecePopulation >= 7.0 ? 2 : 1;
        for (int farIndex = 0; farIndex < 2; ++farIndex)
        {
            if (farIndex >= farCount)
                break;
            uint state = baseSeed ^ uint(0x85ebca6bu + farIndex * 0x27d4eb2du);
            float longitudinal = (random01(state) - 0.5) * cellSize * 1.30;
            float lateral = (random01(state) - 0.5) * cellSize * 1.30;
            float angle = random01(state) * 6.28318530718;
            float lengthM = cellSize * (0.48 + 0.62 * random01(state));
            float widthM = 0.006 + 0.012 * random01(state);
            float curl = (random01(state) * 2.0 - 1.0) * 0.35;
            vec3 farCenter = center + forward * longitudinal + right * lateral;
            emitFlake(
                farCenter, normal, right, forward, angle, lengthM, widthM, curl,
                0.0022, 0.0005, alpha / float(farCount));
        }
    }

    if (nearWeight <= 0.0001)
        return;

    uint countState = baseSeed ^ 0x7f4a7c15u;
    float expectedMarbles = min(piecePopulation, 24.0);
    int marbleCount = int(floor(expectedMarbles));
    if (random01(countState) < expectedMarbles - float(marbleCount))
        ++marbleCount;
    marbleCount = clamp(marbleCount, 0, 24);
    if (marbleCount == 0)
        return;

    float densityScale = clamp(looseRubber * 9.0, 0.0, 1.0);
    float pileFactor = clamp(piecePopulation / 42.0, 0.0, 1.0);
    float scatter = cellSize * (0.42 - 0.24 * pileFactor);

    float anchorLongitudinal[3];
    float anchorLateral[3];
    uint anchorState = baseSeed ^ 0x6du;
    for (int anchor = 0; anchor < 3; ++anchor)
    {
        anchorLongitudinal[anchor] = (random01(anchorState) - 0.5) * cellSize * 1.34;
        anchorLateral[anchor] = (random01(anchorState) - 0.5) * cellSize * 1.34;
    }

    for (int marble = 0; marble < 24; ++marble)
    {
        if (marble >= marbleCount)
            break;
        uint state = baseSeed ^ uint(0x9e3779b9u * uint(marble + 1));
        int anchor = int(state % 3u);
        float longitudinal = anchorLongitudinal[anchor]
            + (random01(state) + random01(state) - 1.0) * scatter;
        float lateral = anchorLateral[anchor]
            + (random01(state) + random01(state) - 1.0) * scatter;
        float angle = random01(state) * 6.28318530718;
        float severityLengthScale = 0.82 + 0.88 * severity;
        float severityWidthScale = 0.88 + 0.62 * severity;
        float freshLengthM = (0.016 + random01(state)
            * (0.028 + densityScale * 0.015)) * severityLengthScale;
        float matureLengthM = (0.010 + random01(state)
            * (0.015 + densityScale * 0.010)) * (0.92 + 0.42 * severity);
        float lengthM = mix(freshLengthM, matureLengthM, maturity);
        float freshWidthM = (0.0042 + random01(state) * 0.0055) * severityWidthScale;
        float matureWidthM = (0.0070 + random01(state)
            * (0.0075 + densityScale * 0.0045)) * (0.95 + 0.40 * severity);
        float widthM = mix(freshWidthM, matureWidthM, maturity);
        float curl = (random01(state) * 2.0 - 1.0)
            * (0.25 + 0.55 * maturity + densityScale * 0.40 + 0.18 * severity);
        int maximumStackLayer = 1 + int(floor(4.0 * pileFactor));
        int stackLayer = maximumStackLayer > 1
            ? int(random01(state) * float(maximumStackLayer))
            : 0;
        float stackLiftM = float(stackLayer)
            * (0.0014 + 0.42 * widthM) * pileFactor;
        float liftM = 0.0016 + random01(state)
            * (0.0022 + maturity * 0.0045) + stackLiftM;
        float shade = 0.00012 + random01(state) * 0.00095;
        vec3 marbleCenter = center + forward * longitudinal + right * lateral;
        emitFlake(
            marbleCenter, normal, right, forward, angle, lengthM, widthM, curl,
            liftM, shade, 0.96 * nearWeight * rangeVisibility);
    }
}
)glsl";

const char* kRubberFragmentShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
in float gShade;
in float gAlpha;
out vec4 FragColor;
void main()
{
    float alpha = clamp(gAlpha, 0.0, 1.0);
    if (alpha <= 0.0001)
        discard;
    float shade = clamp(gShade, 0.0, 1.0);
    FragColor = vec4(shade, shade, shade, alpha);
}
)glsl";

// TIRE16L moving rubber remains authoritative CPU simulation, but its visual
// representatives are expanded on the GPU from one compact packet record. The
// CPU uploads at most the bounded packet pool, never 12 reconstructed flakes
// per packet.
const char* kMovingRubberVertexShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aCenterRelative;
layout(location=1) in vec3 aAxisRight;
layout(location=2) in vec3 aAxisForward;
layout(location=3) in vec3 aAxisNormal;
layout(location=4) in vec4 aShape; // length, width, bend1, bend3
layout(location=5) in vec4 aState; // age, opacity, piecePopulation, severity
layout(location=6) in vec4 aMisc;  // seed, phase, maturity, quantity

out MovingRubberRecord
{
    vec3 centerRelative;
    vec3 axisRight;
    vec3 axisForward;
    vec3 axisNormal;
    vec4 shape;
    vec4 state;
    vec4 misc;
} vRecord;

void main()
{
    vRecord.centerRelative = aCenterRelative;
    vRecord.axisRight = aAxisRight;
    vRecord.axisForward = aAxisForward;
    vRecord.axisNormal = aAxisNormal;
    vRecord.shape = aShape;
    vRecord.state = aState;
    vRecord.misc = aMisc;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
)glsl";

const char* kMovingRubberGeometryShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
layout(points) in;
layout(triangle_strip, max_vertices=72) out;

in MovingRubberRecord
{
    vec3 centerRelative;
    vec3 axisRight;
    vec3 axisForward;
    vec3 axisNormal;
    vec4 shape;
    vec4 state;
    vec4 misc;
} gRecord[];

uniform mat4 uView;
uniform mat4 uProjection;
uniform float uDrawDistance;
uniform float uVisibilityFadeWidth;

out float gShade;
out float gAlpha;

float smooth01(float value)
{
    float t = clamp(value, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

vec3 normalizeSafe(vec3 value, vec3 fallbackValue)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 1.0e-10
        ? value * inversesqrt(lengthSquared)
        : fallbackValue;
}

float visibilityWeight(float distanceMeters)
{
    if (distanceMeters >= uDrawDistance)
        return 0.0;
    float fadeStart = max(0.0, uDrawDistance - uVisibilityFadeWidth);
    if (distanceMeters <= fadeStart)
        return 1.0;
    return 1.0 - smooth01(
        (distanceMeters - fadeStart) / max(uDrawDistance - fadeStart, 0.0001));
}

uint nextRandom(inout uint state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

float random01(inout uint state)
{
    return float((nextRandom(state) >> 8u) & 0x00ffffffu) / 16777216.0;
}

void emitRubberVertex(vec3 position, float shade, float alpha)
{
    gShade = shade;
    gAlpha = alpha;
    gl_Position = uProjection * uView * vec4(position, 1.0);
    EmitVertex();
}

void main()
{
    vec3 centerBase = gRecord[0].centerRelative;
    float distanceMeters = length(centerBase);
    float rangeVisibility = visibilityWeight(distanceMeters);
    if (rangeVisibility <= 0.0001)
        return;

    float age = max(gRecord[0].state.x, 0.0);
    float opacity = clamp(gRecord[0].state.y, 0.0, 1.0) * rangeVisibility;
    float piecePopulation = clamp(gRecord[0].state.z, 0.0, 12.0);
    float severity = clamp(gRecord[0].state.w, 0.0, 1.0);
    bool airborne = gRecord[0].misc.y < 0.5;
    uint countState = max(uint(gRecord[0].misc.x + 0.5), 1u) ^ 0x7f4a7c15u;
    int representativeCount = int(floor(piecePopulation));
    if (random01(countState) < piecePopulation - float(representativeCount))
        ++representativeCount;
    representativeCount = clamp(representativeCount, 0, 12);
    if (representativeCount == 0)
        return;

    vec3 packetForward = normalizeSafe(gRecord[0].axisForward, vec3(0.0, 0.0, 1.0));
    vec3 packetRight = normalizeSafe(gRecord[0].axisRight, vec3(1.0, 0.0, 0.0));
    vec3 packetNormal = normalizeSafe(gRecord[0].axisNormal, cross(packetForward, packetRight));
    packetRight = normalizeSafe(cross(packetNormal, packetForward), packetRight);
    packetNormal = normalizeSafe(cross(packetForward, packetRight), packetNormal);

    for (int representative = 0; representative < 12; ++representative)
    {
        if (representative >= representativeCount)
            break;
        uint state = max(uint(gRecord[0].misc.x + 0.5), 1u)
            ^ (0x9e3779b9u * uint(representative + 1));
        float randomA = random01(state);
        float randomB = random01(state);
        float randomC = random01(state);
        float randomD = random01(state);
        float randomE = random01(state);
        float spreadM = airborne
            ? 0.010 + 0.028 * severity
            : 0.006 + 0.014 * severity;
        float forwardOffset = (randomA + randomB - 1.0) * spreadM;
        float rightOffset = (randomC + randomD - 1.0) * spreadM;
        float normalOffset = airborne
            ? (randomE - 0.5) * spreadM * 0.65
            : float(representative / 4) * 0.0012;
        vec3 center = centerBase
            + packetForward * forwardOffset
            + packetRight * rightOffset
            + packetNormal * normalOffset;

        float spinRate = 7.0 + random01(state) * 15.0;
        float phase = age * spinRate + random01(state) * 6.28318530718;
        vec3 normal = packetNormal;
        if (airborne)
        {
            float tiltA = sin(phase * 0.83) * (0.18 + random01(state) * 0.42);
            float tiltB = cos(phase * 1.17) * (0.14 + random01(state) * 0.34);
            normal = normalizeSafe(
                packetNormal + packetRight * tiltA + packetForward * tiltB,
                packetNormal);
        }
        float spin = phase + (randomA - 0.5) * 1.8;
        vec3 forward = normalizeSafe(
            packetForward * cos(spin) + packetRight * sin(spin), packetForward);
        forward = normalizeSafe(forward - normal * dot(forward, normal), packetForward);
        vec3 right = normalizeSafe(cross(normal, forward), packetRight);
        normal = normalizeSafe(cross(forward, right), normal);

        float sizeJitter = 0.72 + random01(state) * 0.56;
        float halfLength = max(gRecord[0].shape.x * 0.5 * sizeJitter, 0.003);
        float halfWidth = max(gRecord[0].shape.y * 0.5
            * (0.78 + random01(state) * 0.44), 0.0015);
        float bendJitter = 0.70 + random01(state) * 0.60;
        float shade = 0.00018 + 0.00025 * severity;

        vec3 p0 = center + forward * -halfLength + right * -halfWidth;
        vec3 p1 = center + forward * halfLength + right * -halfWidth
            + normal * (gRecord[0].shape.z * bendJitter);
        vec3 p2 = center + forward * halfLength + right * halfWidth;
        vec3 p3 = center + forward * -halfLength + right * halfWidth
            + normal * (gRecord[0].shape.w * (1.25 - 0.25 * bendJitter));

        emitRubberVertex(p0, shade, opacity);
        emitRubberVertex(p1, shade, opacity);
        emitRubberVertex(p2, shade, opacity);
        EndPrimitive();
        emitRubberVertex(p0, shade, opacity);
        emitRubberVertex(p2, shade, opacity);
        emitRubberVertex(p3, shade, opacity);
        EndPrimitive();
    }
}
)glsl";

const char* kParticleVertexShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
layout(location=2) in float aPointSize;
uniform mat4 uView;
uniform mat4 uProjection;
out vec4 vColor;
void main()
{
    vColor = aColor;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
    gl_PointSize = aPointSize;
}
)glsl";

const char* kParticleFragmentShader = HERITAGE_SURFACE_GLSL_VERSION R"glsl(
in vec4 vColor;
out vec4 FragColor;
void main()
{
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    float radiusSquared = dot(centered, centered);
    if (radiusSquared > 1.0)
        discard;
    float edge = smoothstep(1.0, 0.45, radiusSquared);
    FragColor = vec4(vColor.rgb, vColor.a * edge);
}
)glsl";

using TrackVertex = SurfaceTrackVertex;
using ParticleVertex = SurfaceParticleVertex;

// Compact TIRE16K logical record uploaded once to a persistent GPU page.
// Positions are FP32 relative to a 100 m FP64 chunk origin; the authoritative
// SurfacePresentation history remains FP64. The geometry shader expands this
// record into the six-control near ribbon or the uniform far strip on demand.
struct TireMarkGpuRecord
{
    float startLocal[3]{};
    float endLocal[3]{};
    float startNormal[3]{};
    float endNormal[3]{};
    float startRight[3]{};
    float endRight[3]{};
    float startData[4]{};
    float endData[4]{};
    float misc[4]{};
};

static_assert(sizeof(TireMarkGpuRecord) == sizeof(float) * 30);

struct MarbleCellGpuRecord
{
    float centerLocal[3]{};
    float normal[3]{};
    float forward[3]{};
    float state[4]{};
    float misc[4]{};
};

static_assert(sizeof(MarbleCellGpuRecord) == sizeof(float) * 17);

struct MovingRubberGpuRecord
{
    float centerRelative[3]{};
    float axisRight[3]{};
    float axisForward[3]{};
    float axisNormal[3]{};
    float shape[4]{};
    float state[4]{};
    float misc[4]{};
};

static_assert(sizeof(MovingRubberGpuRecord) == sizeof(float) * 24);

inline constexpr std::uint32_t kTireMarkGpuMaximumSegmentsPerPage = 8192;
inline constexpr std::uint32_t kMarbleGpuMaximumCellsPerPage = 8192;

std::uint32_t tireMarkGpuPageCapacity(std::size_t pageIndex)
{
    // Sparse 100 m chunks should not reserve a full megabyte just because one
    // tire touched them once. Pages grow geometrically; dense skid pads still
    // converge on 8192-record pages and keep draw-call count bounded.
    if (pageIndex == 0)
        return 256;
    if (pageIndex == 1)
        return 1024;
    if (pageIndex == 2)
        return 4096;
    return kTireMarkGpuMaximumSegmentsPerPage;
}
std::uint32_t marbleGpuPageCapacity(std::size_t pageIndex)
{
    if (pageIndex == 0)
        return 256;
    if (pageIndex == 1)
        return 1024;
    if (pageIndex == 2)
        return 4096;
    return kMarbleGpuMaximumCellsPerPage;
}

inline constexpr double kMarbleGpuDrawDistanceM = 220.0;
inline constexpr double kMarbleGpuDetailedDistanceM = 55.0;
inline constexpr double kMovingRubberGpuDrawDistanceM = 120.0;

inline constexpr double kTireMarkGpuDrawDistanceM = 500.0;
inline constexpr double kTireMarkGpuDetailedDistanceM = 200.0;
inline constexpr double kTireMarkGpuCapDistanceM = 100.0;

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

float dot(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

float length(const heritage::math::Vec3& value)
{
    return std::sqrt(dot(value, value));
}

heritage::math::Vec3 normalize(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitude = length(value);
    if (magnitude <= 1.0e-6f)
        return fallback;
    return { value.x / magnitude, value.y / magnitude, value.z / magnitude };
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

void stableSurfaceBasis(
    const heritage::math::Vec3& normal,
    heritage::math::Vec3& forward,
    heritage::math::Vec3& right)
{
    // TIRE15C3: procedural track debris must be world-anchored. Using the
    // latest tire contact direction as the cell basis made every existing
    // shred rotate when the driver steered through that cell. Project a fixed
    // world axis onto the support plane instead; the deterministic per-piece
    // random angle then remains stable for the lifetime of the track state.
    const heritage::math::Vec3 worldForward{ 0.0f, 0.0f, 1.0f };
    const heritage::math::Vec3 worldRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 tangent = subtract(
        worldForward, scale(normal, dot(worldForward, normal)));
    if (length(tangent) <= 0.10f)
    {
        tangent = subtract(
            worldRight, scale(normal, dot(worldRight, normal)));
    }
    forward = normalize(tangent, { 0.0f, 0.0f, 1.0f });
    right = normalize(cross(normal, forward), { 1.0f, 0.0f, 0.0f });
    forward = normalize(cross(right, normal), forward);
}

heritage::math::Mat4 lookAt(
    const heritage::math::Vec3& eye,
    const heritage::math::Vec3& target,
    const heritage::math::Vec3& up)
{
    const heritage::math::Vec3 forward = normalize(subtract(target, eye), { 0.0f, 0.0f, -1.0f });
    const heritage::math::Vec3 side = normalize(cross(forward, up), { 1.0f, 0.0f, 0.0f });
    const heritage::math::Vec3 correctedUp = cross(side, forward);

    heritage::math::Mat4 result = heritage::math::identity();
    result.m[0] = side.x;
    result.m[1] = correctedUp.x;
    result.m[2] = -forward.x;
    result.m[4] = side.y;
    result.m[5] = correctedUp.y;
    result.m[6] = -forward.y;
    result.m[8] = side.z;
    result.m[9] = correctedUp.z;
    result.m[10] = -forward.z;
    result.m[12] = -dot(side, eye);
    result.m[13] = -dot(correctedUp, eye);
    result.m[14] = dot(forward, eye);
    return result;
}

heritage::math::Vec3 trackCenterColor(heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    switch (material)
    {
    case SurfaceMaterial::Mud: return { 0.075f, 0.045f, 0.025f };
    case SurfaceMaterial::Sand: return { 0.36f, 0.27f, 0.14f };
    case SurfaceMaterial::DeepSnow: return { 0.58f, 0.66f, 0.72f };
    case SurfaceMaterial::SoftSoil: return { 0.12f, 0.075f, 0.035f };
    default: return { 0.09f, 0.08f, 0.07f };
    }
}

heritage::math::Vec3 trackShoulderColor(heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    switch (material)
    {
    case SurfaceMaterial::Mud: return { 0.16f, 0.09f, 0.045f };
    case SurfaceMaterial::Sand: return { 0.62f, 0.48f, 0.25f };
    case SurfaceMaterial::DeepSnow: return { 0.88f, 0.92f, 0.96f };
    case SurfaceMaterial::SoftSoil: return { 0.24f, 0.14f, 0.06f };
    default: return { 0.18f, 0.15f, 0.10f };
    }
}

heritage::math::Vec3 particleColor(
    heritage::physics::SurfaceParticleKind kind,
    heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    using heritage::physics::SurfaceParticleKind;
    switch (kind)
    {
    case SurfaceParticleKind::WaterSpray: return { 0.67f, 0.75f, 0.80f };
    case SurfaceParticleKind::Dust:
        return material == SurfaceMaterial::Sand
            ? heritage::math::Vec3{ 0.72f, 0.60f, 0.38f }
            : heritage::math::Vec3{ 0.52f, 0.43f, 0.30f };
    case SurfaceParticleKind::Mud: return { 0.13f, 0.075f, 0.035f };
    case SurfaceParticleKind::Snow: return { 0.90f, 0.94f, 0.98f };
    case SurfaceParticleKind::RubberShred: return { 0.002f, 0.002f, 0.002f };
    case SurfaceParticleKind::TireFailureSmoke: return { 0.48f, 0.49f, 0.50f };
    case SurfaceParticleKind::TireFailureDebris: return { 0.018f, 0.020f, 0.022f };
    case SurfaceParticleKind::LooseDebris:
    default:
        return material == SurfaceMaterial::Gravel
            ? heritage::math::Vec3{ 0.36f, 0.34f, 0.31f }
            : heritage::math::Vec3{ 0.29f, 0.21f, 0.13f };
    }
}

void appendTriangle(
    std::vector<TrackVertex>& vertices,
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b,
    const heritage::math::Vec3& c,
    const heritage::math::Vec3& color,
    float alpha)
{
    vertices.push_back({ a.x, a.y, a.z, color.x, color.y, color.z, alpha });
    vertices.push_back({ b.x, b.y, b.z, color.x, color.y, color.z, alpha });
    vertices.push_back({ c.x, c.y, c.z, color.x, color.y, color.z, alpha });
}

void appendQuad(
    std::vector<TrackVertex>& vertices,
    const heritage::math::Vec3& p0,
    const heritage::math::Vec3& p1,
    const heritage::math::Vec3& p2,
    const heritage::math::Vec3& p3,
    const heritage::math::Vec3& color,
    float alpha)
{
    appendTriangle(vertices, p0, p1, p2, color, alpha);
    appendTriangle(vertices, p0, p2, p3, color, alpha);
}

std::uint32_t rubberSeed(
    const heritage::physics::rubber::TrackRubberVisualCell& cell,
    std::uint32_t sequence)
{
    const std::int64_t x = static_cast<std::int64_t>(std::floor(cell.globalPosition.x * 2.0));
    const std::int64_t z = static_cast<std::int64_t>(std::floor(cell.globalPosition.z * 2.0));
    std::uint64_t value = static_cast<std::uint64_t>(x) * 0x9e3779b97f4a7c15ULL;
    value ^= static_cast<std::uint64_t>(z) + 0x85ebca6bULL + (value << 6u) + (value >> 2u);
    // Keep visual slots stable as rubber state updates. Do not mix the update
    // serial into this seed or existing resting flakes would jump every touch.
    value ^= static_cast<std::uint64_t>(sequence) * 0xc2b2ae3d27d4eb4fULL;
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31u;
    return static_cast<std::uint32_t>(value ^ (value >> 32u));
}


} // namespace

void SurfacePresentationRenderer::clearTireMarkGpuCache() const
{
    for (auto& [address, chunk] : m_tireMarkGpuChunks)
    {
        (void)address;
        for (TireMarkGpuPage& page : chunk.pages)
        {
            if (page.vbo != 0)
                glDeleteBuffers(1, &page.vbo);
            if (page.vao != 0)
                glDeleteVertexArrays(1, &page.vao);
        }
    }
    m_tireMarkGpuChunks.clear();
    m_tireMarkGpuTailLocations.clear();
    m_lastCachedTireMarkSerial = 0;
    m_lastTireMarkCachePresentationTime = -1.0;
}

void SurfacePresentationRenderer::clearTireMarkGpuEndFeather(
    std::uint64_t serial) const
{
    const auto tailIt = m_tireMarkGpuTailLocations.find(serial);
    if (tailIt == m_tireMarkGpuTailLocations.end())
        return;

    const TireMarkGpuTailLocation location = tailIt->second;
    const std::uint32_t updatedFlags = location.flags & ~2u;
    const float updatedFlagsFloat = static_cast<float>(updatedFlags);
    constexpr std::size_t kFlagByteOffset =
        offsetof(TireMarkGpuRecord, misc) + sizeof(float) * 3;

    const auto chunkIt = m_tireMarkGpuChunks.find(location.address);
    if (chunkIt != m_tireMarkGpuChunks.end())
    {
        for (TireMarkGpuPage& page : chunkIt->second.pages)
        {
            if (page.vbo != location.vbo)
                continue;

            const std::size_t recordByteOffset =
                static_cast<std::size_t>(location.recordIndex) * sizeof(TireMarkGpuRecord);
            const std::size_t fieldByteOffset = recordByteOffset + kFlagByteOffset;
            const std::size_t pendingBegin = page.pendingByteOffset;
            const std::size_t pendingEnd = pendingBegin + page.pendingUpload.size();

            if (!page.pendingUpload.empty()
                && fieldByteOffset >= pendingBegin
                && fieldByteOffset + sizeof(float) <= pendingEnd)
            {
                const std::size_t localOffset = fieldByteOffset - pendingBegin;
                std::memcpy(
                    page.pendingUpload.data() + localOffset,
                    &updatedFlagsFloat,
                    sizeof(updatedFlagsFloat));
            }
            else
            {
                glBindBuffer(GL_ARRAY_BUFFER, page.vbo);
                glBufferSubData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLintptr>(fieldByteOffset),
                    static_cast<GLsizeiptr>(sizeof(updatedFlagsFloat)),
                    &updatedFlagsFloat);
            }
            break;
        }
    }

    // Whether or not the page survived a cache/history transition, this serial
    // is no longer a trail tail once a successor exists.
    m_tireMarkGpuTailLocations.erase(tailIt);
}

void SurfacePresentationRenderer::syncTireMarkGpuCache(
    const heritage::physics::SurfacePresentation& presentation) const
{
    const double presentationTime = presentation.elapsedSeconds();
    const std::uint64_t firstSerial = presentation.firstTireMarkSerial();
    const std::uint64_t lastSerial = presentation.lastTireMarkSerial();

    // Reset/scene reload: SurfacePresentation restarts both elapsed time and
    // serials. Drop GPU pages rather than accidentally associating old buffers
    // with a new world's marks.
    if (presentationTime + 1.0e-6 < m_lastTireMarkCachePresentationTime
        || (m_lastCachedTireMarkSerial != 0
            && lastSerial != 0
            && lastSerial < m_lastCachedTireMarkSerial))
    {
        clearTireMarkGpuCache();
    }

    if (lastSerial == 0)
    {
        if (!m_tireMarkGpuChunks.empty())
            clearTireMarkGpuCache();
        m_lastTireMarkCachePresentationTime = presentationTime;
        return;
    }

    // Retire whole GPU pages once every record on that page is guaranteed to
    // be gone from the authoritative one-million / twenty-minute history. A
    // partially old page remains resident and its stale records are discarded
    // in the geometry shader, avoiding CPU-side compaction/rebuild work.
    double historyFloorBirthTime = -std::numeric_limits<double>::infinity();
    if (!presentation.tireMarkSegments().empty())
        historyFloorBirthTime = presentation.tireMarkSegments().front().birthTimeSeconds;
    const double expiryBirthTime = std::max(
        presentationTime - heritage::physics::SurfacePresentation::kTireMarkRetirementSeconds,
        historyFloorBirthTime - 1.0e-6);

    for (auto tailIt = m_tireMarkGpuTailLocations.begin();
         tailIt != m_tireMarkGpuTailLocations.end();)
    {
        if (tailIt->second.birthTimeSeconds < expiryBirthTime)
            tailIt = m_tireMarkGpuTailLocations.erase(tailIt);
        else
            ++tailIt;
    }

    for (auto chunkIt = m_tireMarkGpuChunks.begin();
         chunkIt != m_tireMarkGpuChunks.end();)
    {
        auto& pages = chunkIt->second.pages;
        for (auto pageIt = pages.begin(); pageIt != pages.end();)
        {
            if (pageIt->segmentCount > 0
                && pageIt->maximumBirthTimeSeconds < expiryBirthTime)
            {
                if (pageIt->vbo != 0)
                    glDeleteBuffers(1, &pageIt->vbo);
                if (pageIt->vao != 0)
                    glDeleteVertexArrays(1, &pageIt->vao);
                pageIt = pages.erase(pageIt);
            }
            else
            {
                ++pageIt;
            }
        }
        if (pages.empty())
            chunkIt = m_tireMarkGpuChunks.erase(chunkIt);
        else
            ++chunkIt;
    }

    std::uint64_t nextSerial = m_lastCachedTireMarkSerial == 0
        ? firstSerial
        : m_lastCachedTireMarkSerial + 1;
    if (nextSerial < firstSerial)
        nextSerial = firstSerial;

    for (std::uint64_t serial = nextSerial; serial <= lastSerial; ++serial)
    {
        const heritage::physics::SurfaceTireMarkSegment* mark =
            presentation.tireMarkSegmentBySerial(serial);
        if (mark == nullptr || mark->serial == 0)
            continue;

        // The CPU presentation initially marks a freshly emitted segment as a
        // possible trail tail. If a successor arrives, clear only that prior
        // segment's provisional end-feather flag. This is a four-byte GPU patch,
        // not a geometry rebuild/re-upload, and preserves continuous longitudinal
        // darkness without the 11.5 cm "barcode" overlap from TIRE16K.
        if (mark->previousSegmentSerial != 0)
            clearTireMarkGpuEndFeather(mark->previousSegmentSerial);

        const heritage::math::DVec3 midpoint{
            (mark->startGlobalPosition.x + mark->endGlobalPosition.x) * 0.5,
            (mark->startGlobalPosition.y + mark->endGlobalPosition.y) * 0.5,
            (mark->startGlobalPosition.z + mark->endGlobalPosition.z) * 0.5
        };
        const heritage::graphics::tiremarks::ChunkAddress address =
            heritage::graphics::tiremarks::chunkAddress(midpoint);
        auto [chunkIt, inserted] = m_tireMarkGpuChunks.try_emplace(address);
        TireMarkGpuChunk& chunk = chunkIt->second;
        if (inserted)
            chunk.globalOrigin = heritage::graphics::tiremarks::chunkOrigin(address);

        if (chunk.pages.empty()
            || chunk.pages.back().segmentCount >= chunk.pages.back().capacitySegments)
        {
            TireMarkGpuPage page;
            page.capacitySegments = tireMarkGpuPageCapacity(chunk.pages.size());
            glGenVertexArrays(1, &page.vao);
            glGenBuffers(1, &page.vbo);
            glBindVertexArray(page.vao);
            glBindBuffer(GL_ARRAY_BUFFER, page.vbo);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    sizeof(TireMarkGpuRecord) * page.capacitySegments),
                nullptr,
                GL_DYNAMIC_DRAW);

            const GLsizei stride = static_cast<GLsizei>(sizeof(TireMarkGpuRecord));
            const auto attribute = [stride](GLuint location, GLint count, std::size_t offset) {
                glEnableVertexAttribArray(location);
                glVertexAttribPointer(
                    location, count, GL_FLOAT, GL_FALSE, stride,
                    reinterpret_cast<const void*>(offset));
            };
            attribute(0, 3, offsetof(TireMarkGpuRecord, startLocal));
            attribute(1, 3, offsetof(TireMarkGpuRecord, endLocal));
            attribute(2, 3, offsetof(TireMarkGpuRecord, startNormal));
            attribute(3, 3, offsetof(TireMarkGpuRecord, endNormal));
            attribute(4, 3, offsetof(TireMarkGpuRecord, startRight));
            attribute(5, 3, offsetof(TireMarkGpuRecord, endRight));
            attribute(6, 4, offsetof(TireMarkGpuRecord, startData));
            attribute(7, 4, offsetof(TireMarkGpuRecord, endData));
            attribute(8, 4, offsetof(TireMarkGpuRecord, misc));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            chunk.pages.push_back(std::move(page));
        }

        TireMarkGpuPage& page = chunk.pages.back();
        TireMarkGpuRecord record{};
        const heritage::math::Vec3 startLocal =
            heritage::graphics::tiremarks::localFp32(
                mark->startGlobalPosition, chunk.globalOrigin);
        const heritage::math::Vec3 endLocal =
            heritage::graphics::tiremarks::localFp32(
                mark->endGlobalPosition, chunk.globalOrigin);
        const auto storeVec3 = [](float (&destination)[3], const heritage::math::Vec3& value) {
            destination[0] = value.x;
            destination[1] = value.y;
            destination[2] = value.z;
        };
        storeVec3(record.startLocal, startLocal);
        storeVec3(record.endLocal, endLocal);
        storeVec3(record.startNormal, mark->startNormal);
        storeVec3(record.endNormal, mark->endNormal);
        storeVec3(record.startRight, mark->startRight);
        storeVec3(record.endRight, mark->endRight);
        record.startData[0] = mark->startWidthM;
        record.startData[1] = mark->startIntensity;
        record.startData[2] = mark->startLoadFractions[0];
        record.startData[3] = mark->startLoadFractions[1];
        record.endData[0] = mark->endWidthM;
        record.endData[1] = mark->endIntensity;
        record.endData[2] = mark->endLoadFractions[0];
        record.endData[3] = mark->endLoadFractions[1];
        record.misc[0] = mark->startLoadFractions[2];
        record.misc[1] = mark->endLoadFractions[2];
        record.misc[2] = static_cast<float>(mark->birthTimeSeconds);
        record.misc[3] = static_cast<float>(
            (mark->startFeather ? 1u : 0u) | (mark->endFeather ? 2u : 0u));

        if (page.pendingUpload.empty())
        {
            page.pendingByteOffset = static_cast<std::size_t>(page.segmentCount)
                * sizeof(TireMarkGpuRecord);
        }
        const std::size_t oldBytes = page.pendingUpload.size();
        page.pendingUpload.resize(oldBytes + sizeof(TireMarkGpuRecord));
        std::memcpy(page.pendingUpload.data() + oldBytes, &record, sizeof(record));

        const std::uint32_t recordIndex = page.segmentCount;
        const std::uint32_t flags =
            (mark->startFeather ? 1u : 0u) | (mark->endFeather ? 2u : 0u);
        if (mark->endFeather)
        {
            m_tireMarkGpuTailLocations[mark->serial] = TireMarkGpuTailLocation{
                address, page.vbo, recordIndex, flags, mark->birthTimeSeconds
            };
        }

        if (page.segmentCount == 0)
        {
            page.minimumBirthTimeSeconds = mark->birthTimeSeconds;
            page.maximumBirthTimeSeconds = mark->birthTimeSeconds;
        }
        else
        {
            page.minimumBirthTimeSeconds = std::min(
                page.minimumBirthTimeSeconds, mark->birthTimeSeconds);
            page.maximumBirthTimeSeconds = std::max(
                page.maximumBirthTimeSeconds, mark->birthTimeSeconds);
        }
        ++page.segmentCount;
    }

    // Batch all records appended to the same page this frame into one upload.
    // A frozen page is never touched again; old history therefore consumes no
    // recurring CPU tessellation, sorting, memcpy or glBufferData work.
    for (auto& [address, chunk] : m_tireMarkGpuChunks)
    {
        (void)address;
        for (TireMarkGpuPage& page : chunk.pages)
        {
            if (page.pendingUpload.empty())
                continue;
            glBindBuffer(GL_ARRAY_BUFFER, page.vbo);
            glBufferSubData(
                GL_ARRAY_BUFFER,
                static_cast<GLintptr>(page.pendingByteOffset),
                static_cast<GLsizeiptr>(page.pendingUpload.size()),
                page.pendingUpload.data());
            page.pendingUpload.clear();
            page.pendingByteOffset = 0;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    m_lastCachedTireMarkSerial = lastSerial;
    m_lastTireMarkCachePresentationTime = presentationTime;
}

void SurfacePresentationRenderer::drawTireMarkGpuCache(
    const heritage::physics::SurfacePresentation& presentation,
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const heritage::math::DVec3& cameraGlobal) const
{
    if (m_tireMarkGpuChunks.empty())
        return;

    const float detailedDistance = static_cast<float>(kTireMarkGpuDetailedDistanceM);
    const float drawDistance = static_cast<float>(kTireMarkGpuDrawDistanceM);
    const float lodBlendWidth = heritage::graphics::lod::lodBlendWidthMeters(
        detailedDistance);
    const float visibilityFadeWidth = heritage::graphics::lod::visibilityFadeWidthMeters(
        drawDistance);
    const float presentationTime = static_cast<float>(presentation.elapsedSeconds());
    const float historyFloorBirthTime = presentation.tireMarkSegments().empty()
        ? -1.0e20f
        : static_cast<float>(presentation.tireMarkSegments().front().birthTimeSeconds);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 4.0f);
    glUseProgram(m_tireMarkProgram);
    glUniformMatrix4fv(
        m_tireMarkUniformView,
        1, GL_FALSE, view.m);
    glUniformMatrix4fv(
        m_tireMarkUniformProjection,
        1, GL_FALSE, projection.m);
    glUniform1f(
        m_tireMarkUniformPresentationTime,
        presentationTime);
    glUniform1f(
        m_tireMarkUniformHistoryFloorBirthTime,
        historyFloorBirthTime);
    glUniform1f(
        m_tireMarkUniformRetirementSeconds,
        static_cast<float>(heritage::physics::SurfacePresentation::kTireMarkRetirementSeconds));
    glUniform1f(
        m_tireMarkUniformDetailedDistance,
        detailedDistance);
    glUniform1f(
        m_tireMarkUniformLodBlendWidth,
        lodBlendWidth);
    glUniform1f(
        m_tireMarkUniformDrawDistance,
        drawDistance);
    glUniform1f(
        m_tireMarkUniformVisibilityFadeWidth,
        visibilityFadeWidth);
    glUniform1f(
        m_tireMarkUniformCapDistance,
        static_cast<float>(kTireMarkGpuCapDistanceM));
    const GLint chunkOriginLocation = m_tireMarkUniformChunkOriginRelative;

    const double conservativeChunkRange = kTireMarkGpuDrawDistanceM
        + heritage::graphics::tiremarks::kChunkHorizontalHalfDiagonalM;
    const double conservativeChunkRangeSquared =
        conservativeChunkRange * conservativeChunkRange;

    for (const auto& [address, chunk] : m_tireMarkGpuChunks)
    {
        (void)address;
        const double dx = chunk.globalOrigin.x - cameraGlobal.x;
        const double dz = chunk.globalOrigin.z - cameraGlobal.z;
        const double chunkDistanceSquared = dx * dx + dz * dz;
        if (chunkDistanceSquared > conservativeChunkRangeSquared)
            continue;

        const heritage::math::Vec3 relativeOrigin =
            heritage::graphics::presentation::cameraRelativeFp32(
                chunk.globalOrigin, cameraGlobal);
        glUniform3f(
            chunkOriginLocation,
            relativeOrigin.x, relativeOrigin.y, relativeOrigin.z);

        const double chunkDistance = std::sqrt(chunkDistanceSquared);
        const std::uint64_t approximateTrianglesPerSegment =
            chunkDistance <= kTireMarkGpuDetailedDistanceM
                ? 10u
                : 2u;
        for (const TireMarkGpuPage& page : chunk.pages)
        {
            if (page.segmentCount == 0)
                continue;
            glBindVertexArray(page.vao);
            glDrawArrays(
                GL_POINTS, 0, static_cast<GLsizei>(page.segmentCount));
            ++m_frameStats.drawCalls;
            m_frameStats.visibleTireMarkSegments += page.segmentCount;
            m_frameStats.trackTriangles +=
                static_cast<std::uint64_t>(page.segmentCount)
                * approximateTrianglesPerSegment;
        }
    }

    glBindVertexArray(0);
    glDisable(GL_POLYGON_OFFSET_FILL);
}


void SurfacePresentationRenderer::clearMarbleGpuCache() const
{
    for (auto& [address, chunk] : m_marbleGpuChunks)
    {
        (void)address;
        for (MarbleGpuPage& page : chunk.pages)
        {
            if (page.vbo != 0)
                glDeleteBuffers(1, &page.vbo);
            if (page.vao != 0)
                glDeleteVertexArrays(1, &page.vao);
        }
    }
    m_marbleGpuChunks.clear();
    m_marbleGpuLocations.clear();
    m_lastMarbleCellCount = 0;
    m_lastMarbleResidentChunkCount = 0;
}

void SurfacePresentationRenderer::syncMarbleGpuCache(
    const heritage::physics::SurfaceWorld& surfaces,
    const heritage::math::DVec3& cameraGlobal) const
{
    const auto& rubber = surfaces.trackRubber();
    const std::size_t currentCellCount = rubber.cellCount();
    const std::size_t currentResidentChunks = rubber.residentChunkCount();

    if (currentCellCount == 0)
    {
        if (!m_marbleGpuChunks.empty())
            clearMarbleGpuCache();
        return;
    }

    // Explicit reset and common eviction cases are safest as a cache rebuild.
    // This is presentation-only and rare; normal contact/wake updates patch one
    // compact cell record in-place instead.
    if ((m_lastMarbleCellCount != 0 && currentCellCount < m_lastMarbleCellCount)
        || (m_lastMarbleResidentChunkCount != 0
            && currentResidentChunks < m_lastMarbleResidentChunkCount))
    {
        clearMarbleGpuCache();
    }

    auto& cells = m_marbleCellScratch;
    cells.clear();
    rubber.collectPresentationCellsUnsorted(
        cameraGlobal, kMarbleGpuDrawDistanceM + 8.0, cells, true);
    const float cellSize = rubber.description().cellSizeM;
    const double verticalLayerSize = std::max(
        static_cast<double>(rubber.description().verticalLayerSizeM), 0.01);

    const auto floatBits = [](float value) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    };
    const auto signatureFor = [&](const auto& cell) {
        std::uint64_t value = cell.updateSerial * 0x9e3779b97f4a7c15ULL;
        const auto mix = [&](std::uint32_t bits) {
            value ^= static_cast<std::uint64_t>(bits)
                + 0x9e3779b97f4a7c15ULL + (value << 6u) + (value >> 2u);
        };
        mix(floatBits(cell.looseRubber));
        mix(floatBits(cell.marbleMaturity));
        mix(floatBits(cell.fragmentSeverity));
        mix(floatBits(cell.persistentPiecePopulation));
        mix(floatBits(cell.normal.x));
        mix(floatBits(cell.normal.y));
        mix(floatBits(cell.normal.z));
        return value;
    };

    const auto storeVec3 = [](float (&destination)[3], const heritage::math::Vec3& value) {
        destination[0] = value.x;
        destination[1] = value.y;
        destination[2] = value.z;
    };

    for (const auto& cell : cells)
    {
        const bool active = cell.looseRubber > 0.0012f
            || cell.persistentPiecePopulation >= 0.45f;
        MarbleGpuCellKey key;
        key.x = static_cast<std::int64_t>(std::floor(
            cell.globalPosition.x / static_cast<double>(cellSize)));
        key.y = static_cast<std::int64_t>(std::floor(
            (cell.globalPosition.y + verticalLayerSize * 0.5) / verticalLayerSize));
        key.z = static_cast<std::int64_t>(std::floor(
            cell.globalPosition.z / static_cast<double>(cellSize)));
        key.material = static_cast<std::uint8_t>(cell.material);

        const std::uint64_t visualSignature = signatureFor(cell);
        auto locationIt = m_marbleGpuLocations.find(key);
        if (locationIt != m_marbleGpuLocations.end()
            && locationIt->second.updateSerial == cell.updateSerial
            && locationIt->second.visualSignature == visualSignature)
        {
            continue;
        }
        if (locationIt == m_marbleGpuLocations.end() && !active)
            continue;

        MarbleCellGpuRecord record{};
        heritage::math::Vec3 normal = normalize(cell.normal, { 0.0f, 1.0f, 0.0f });
        heritage::math::Vec3 forward{};
        heritage::math::Vec3 right{};
        stableSurfaceBasis(normal, forward, right);
        (void)right;
        storeVec3(record.normal, normal);
        storeVec3(record.forward, forward);
        record.state[0] = cell.looseRubber;
        record.state[1] = cell.marbleMaturity;
        record.state[2] = cell.fragmentSeverity;
        record.state[3] = cell.persistentPiecePopulation;
        record.misc[0] = static_cast<float>(rubberSeed(cell, 0u) & 0x00ffffffu);
        if (record.misc[0] < 1.0f)
            record.misc[0] = 1.0f;
        record.misc[1] = cellSize;
        record.misc[2] = static_cast<float>(cell.passCount);
        record.misc[3] = active ? 1.0f : 0.0f;

        MarbleGpuLocation location;
        MarbleGpuPage* pagePointer = nullptr;
        if (locationIt == m_marbleGpuLocations.end())
        {
            const auto address = heritage::graphics::tiremarks::chunkAddress(
                cell.globalPosition);
            auto [chunkIt, inserted] = m_marbleGpuChunks.try_emplace(address);
            MarbleGpuChunk& chunk = chunkIt->second;
            if (inserted)
                chunk.globalOrigin = heritage::graphics::tiremarks::chunkOrigin(address);

            if (chunk.pages.empty()
                || chunk.pages.back().cellCount >= chunk.pages.back().capacityCells)
            {
                MarbleGpuPage page;
                page.capacityCells = marbleGpuPageCapacity(chunk.pages.size());
                page.cpuMirror.resize(
                    static_cast<std::size_t>(page.capacityCells)
                    * sizeof(MarbleCellGpuRecord));
                glGenVertexArrays(1, &page.vao);
                glGenBuffers(1, &page.vbo);
                glBindVertexArray(page.vao);
                glBindBuffer(GL_ARRAY_BUFFER, page.vbo);
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(page.cpuMirror.size()),
                    nullptr,
                    GL_DYNAMIC_DRAW);

                const GLsizei stride = static_cast<GLsizei>(sizeof(MarbleCellGpuRecord));
                const auto attribute = [stride](GLuint index, GLint count, std::size_t offset) {
                    glEnableVertexAttribArray(index);
                    glVertexAttribPointer(
                        index, count, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<const void*>(offset));
                };
                attribute(0, 3, offsetof(MarbleCellGpuRecord, centerLocal));
                attribute(1, 3, offsetof(MarbleCellGpuRecord, normal));
                attribute(2, 3, offsetof(MarbleCellGpuRecord, forward));
                attribute(3, 4, offsetof(MarbleCellGpuRecord, state));
                attribute(4, 4, offsetof(MarbleCellGpuRecord, misc));
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
                chunk.pages.push_back(std::move(page));
            }

            MarbleGpuPage& page = chunk.pages.back();
            location.address = address;
            location.pageIndex = chunk.pages.size() - 1u;
            location.recordIndex = page.cellCount++;
            location.updateSerial = cell.updateSerial;
            location.visualSignature = visualSignature;
            locationIt = m_marbleGpuLocations.emplace(key, location).first;
            pagePointer = &page;
        }
        else
        {
            location = locationIt->second;
            auto chunkIt = m_marbleGpuChunks.find(location.address);
            if (chunkIt == m_marbleGpuChunks.end()
                || location.pageIndex >= chunkIt->second.pages.size())
            {
                // Defensive cache self-heal. A reset/eviction should normally
                // have cleared this location together with its GPU pages.
                m_marbleGpuLocations.erase(locationIt);
                continue;
            }
            pagePointer = &chunkIt->second.pages[location.pageIndex];
            locationIt->second.updateSerial = cell.updateSerial;
            locationIt->second.visualSignature = visualSignature;
        }

        auto chunkIt = m_marbleGpuChunks.find(locationIt->second.address);
        if (chunkIt == m_marbleGpuChunks.end() || pagePointer == nullptr)
            continue;
        const heritage::math::Vec3 local = heritage::graphics::tiremarks::localFp32(
            cell.globalPosition, chunkIt->second.globalOrigin);
        storeVec3(record.centerLocal, local);

        MarbleGpuPage& page = *pagePointer;
        const std::size_t byteOffset = static_cast<std::size_t>(
            locationIt->second.recordIndex) * sizeof(MarbleCellGpuRecord);
        std::memcpy(page.cpuMirror.data() + byteOffset, &record, sizeof(record));
        page.dirtyBeginByte = std::min(page.dirtyBeginByte, byteOffset);
        page.dirtyEndByte = std::max(
            page.dirtyEndByte, byteOffset + sizeof(MarbleCellGpuRecord));
    }

    // One contiguous upload per dirty page, regardless of how many cells were
    // touched by tires/wakes this frame. Frozen pages incur no recurring upload.
    for (auto& [address, chunk] : m_marbleGpuChunks)
    {
        (void)address;
        for (MarbleGpuPage& page : chunk.pages)
        {
            if (page.dirtyBeginByte == static_cast<std::size_t>(-1)
                || page.dirtyEndByte <= page.dirtyBeginByte)
            {
                continue;
            }
            glBindBuffer(GL_ARRAY_BUFFER, page.vbo);
            glBufferSubData(
                GL_ARRAY_BUFFER,
                static_cast<GLintptr>(page.dirtyBeginByte),
                static_cast<GLsizeiptr>(page.dirtyEndByte - page.dirtyBeginByte),
                page.cpuMirror.data() + page.dirtyBeginByte);
            page.dirtyBeginByte = static_cast<std::size_t>(-1);
            page.dirtyEndByte = 0;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    m_lastMarbleCellCount = currentCellCount;
    m_lastMarbleResidentChunkCount = currentResidentChunks;
}

void SurfacePresentationRenderer::drawMarbleGpuCache(
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const heritage::math::DVec3& cameraGlobal) const
{
    if (m_marbleGpuChunks.empty())
        return;

    const float detailedDistance = static_cast<float>(kMarbleGpuDetailedDistanceM);
    const float drawDistance = static_cast<float>(kMarbleGpuDrawDistanceM);
    const float lodBlendWidth = heritage::graphics::lod::lodBlendWidthMeters(
        detailedDistance);
    const float visibilityFadeWidth = heritage::graphics::lod::visibilityFadeWidthMeters(
        drawDistance);

    glUseProgram(m_marbleProgram);
    glUniformMatrix4fv(
        m_marbleUniformView,
        1, GL_FALSE, view.m);
    glUniformMatrix4fv(
        m_marbleUniformProjection,
        1, GL_FALSE, projection.m);
    glUniform1f(
        m_marbleUniformDetailedDistance, detailedDistance);
    glUniform1f(
        m_marbleUniformLodBlendWidth, lodBlendWidth);
    glUniform1f(
        m_marbleUniformDrawDistance, drawDistance);
    glUniform1f(
        m_marbleUniformVisibilityFadeWidth,
        visibilityFadeWidth);
    const GLint chunkOriginLocation = m_marbleUniformChunkOriginRelative;

    const double conservativeRange = kMarbleGpuDrawDistanceM
        + heritage::graphics::tiremarks::kChunkHorizontalHalfDiagonalM;
    const double conservativeRangeSquared = conservativeRange * conservativeRange;
    for (const auto& [address, chunk] : m_marbleGpuChunks)
    {
        (void)address;
        const double dx = chunk.globalOrigin.x - cameraGlobal.x;
        const double dz = chunk.globalOrigin.z - cameraGlobal.z;
        if (dx * dx + dz * dz > conservativeRangeSquared)
            continue;

        const heritage::math::Vec3 relativeOrigin =
            heritage::graphics::presentation::cameraRelativeFp32(
                chunk.globalOrigin, cameraGlobal);
        glUniform3f(
            chunkOriginLocation,
            relativeOrigin.x, relativeOrigin.y, relativeOrigin.z);

        const double chunkDistance = std::sqrt(dx * dx + dz * dz);
        const std::uint64_t approximateTrianglesPerCell =
            chunkDistance <= kMarbleGpuDetailedDistanceM ? 48u : 4u;
        for (const MarbleGpuPage& page : chunk.pages)
        {
            if (page.cellCount == 0)
                continue;
            glBindVertexArray(page.vao);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(page.cellCount));
            ++m_frameStats.drawCalls;
            m_frameStats.visibleRubberCells += page.cellCount;
            m_frameStats.trackTriangles +=
                static_cast<std::uint64_t>(page.cellCount)
                * approximateTrianglesPerCell;
        }
    }
    glBindVertexArray(0);
}

void SurfacePresentationRenderer::drawMovingRubberGpu(
    const heritage::physics::SurfaceWorld& surfaces,
    const heritage::math::Mat4& view,
    const heritage::math::Mat4& projection,
    const heritage::math::DVec3& cameraGlobal) const
{
    auto& packets = m_movingRubberPacketScratch;
    packets.clear();
    surfaces.trackRubber().collectTransientPresentationUnsorted(
        cameraGlobal, kMovingRubberGpuDrawDistanceM, packets);
    if (packets.empty())
        return;

    std::vector<MovingRubberGpuRecord> records;
    records.reserve(packets.size());
    std::uint64_t approximateRepresentatives = 0;
    const auto storeVec3 = [](float (&destination)[3], const heritage::math::Vec3& value) {
        destination[0] = value.x;
        destination[1] = value.y;
        destination[2] = value.z;
    };
    for (const auto& packet : packets)
    {
        MovingRubberGpuRecord record{};
        const heritage::math::Vec3 relative =
            heritage::graphics::presentation::cameraRelativeFp32(
                packet.globalPosition, cameraGlobal);
        storeVec3(record.centerRelative, relative);
        storeVec3(record.axisRight, packet.axisRight);
        storeVec3(record.axisForward, packet.axisForward);
        storeVec3(record.axisNormal, packet.axisNormal);
        record.shape[0] = packet.lengthM;
        record.shape[1] = packet.widthM;
        record.shape[2] = packet.bendVertex1M;
        record.shape[3] = packet.bendVertex3M;
        record.state[0] = packet.ageSeconds;
        record.state[1] = packet.opacity;
        record.state[2] = packet.piecePopulation;
        record.state[3] = packet.fragmentSeverity;
        record.misc[0] = static_cast<float>(packet.seed & 0x00ffffffu);
        if (record.misc[0] < 1.0f)
            record.misc[0] = 1.0f;
        record.misc[1] = packet.phase
            == heritage::physics::rubber::TrackRubberTransientPhase::Airborne
            ? 0.0f : 1.0f;
        record.misc[2] = packet.marbleMaturity;
        record.misc[3] = packet.quantity;
        records.push_back(record);
        approximateRepresentatives += static_cast<std::uint64_t>(std::clamp(
            static_cast<int>(std::ceil(packet.piecePopulation)), 0, 12));
    }

    const std::uint32_t required = static_cast<std::uint32_t>(records.size());
    if (required > m_movingRubberCapacity)
    {
        std::uint32_t capacity = std::max<std::uint32_t>(256u, m_movingRubberCapacity);
        while (capacity < required)
            capacity *= 2u;
        m_movingRubberCapacity = capacity;
        glBindBuffer(GL_ARRAY_BUFFER, m_movingRubberVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                static_cast<std::size_t>(capacity) * sizeof(MovingRubberGpuRecord)),
            nullptr,
            GL_DYNAMIC_DRAW);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_movingRubberVbo);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizeiptr>(records.size() * sizeof(MovingRubberGpuRecord)),
        records.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const float drawDistance = static_cast<float>(kMovingRubberGpuDrawDistanceM);
    glUseProgram(m_movingRubberProgram);
    glUniformMatrix4fv(
        m_movingRubberUniformView,
        1, GL_FALSE, view.m);
    glUniformMatrix4fv(
        m_movingRubberUniformProjection,
        1, GL_FALSE, projection.m);
    glUniform1f(
        m_movingRubberUniformDrawDistance, drawDistance);
    glUniform1f(
        m_movingRubberUniformVisibilityFadeWidth,
        heritage::graphics::lod::visibilityFadeWidthMeters(drawDistance));
    glBindVertexArray(m_movingRubberVao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(records.size()));
    glBindVertexArray(0);
    ++m_frameStats.drawCalls;
    m_frameStats.visibleMovingRubber += approximateRepresentatives;
    m_frameStats.trackTriangles += approximateRepresentatives * 2u;
}

bool SurfacePresentationRenderer::initialize()
{
    shutdown();

    m_trackProgram = buildShaderProgram(kTrackVertexShader, kTrackFragmentShader);
    m_tireMarkProgram = buildShaderProgram(
        kTireMarkVertexShader, kTireMarkGeometryShader, kTireMarkFragmentShader);
    m_marbleProgram = buildShaderProgram(
        kMarbleVertexShader, kMarbleGeometryShader, kRubberFragmentShader);
    m_movingRubberProgram = buildShaderProgram(
        kMovingRubberVertexShader, kMovingRubberGeometryShader, kRubberFragmentShader);
    m_particleProgram = buildShaderProgram(kParticleVertexShader, kParticleFragmentShader);
    if (!m_trackProgram || !m_tireMarkProgram || !m_marbleProgram
        || !m_movingRubberProgram || !m_particleProgram)
    {
        shutdown();
        return false;
    }

    // PERF10: cache every presentation-program uniform location once. This
    // removes dozens of driver string lookups from each rendered view.
    m_trackUniformView = glGetUniformLocation(m_trackProgram, "uView");
    m_trackUniformProjection = glGetUniformLocation(m_trackProgram, "uProjection");
    m_trackUniformGamma = glGetUniformLocation(m_trackProgram, "uGamma");
    m_trackUniformBrightness = glGetUniformLocation(m_trackProgram, "uBrightness");
    m_trackUniformContrast = glGetUniformLocation(m_trackProgram, "uContrast");
    m_trackUniformSaturation = glGetUniformLocation(m_trackProgram, "uSaturation");

    m_tireMarkUniformView = glGetUniformLocation(m_tireMarkProgram, "uView");
    m_tireMarkUniformProjection = glGetUniformLocation(m_tireMarkProgram, "uProjection");
    m_tireMarkUniformPresentationTime = glGetUniformLocation(
        m_tireMarkProgram, "uPresentationTime");
    m_tireMarkUniformHistoryFloorBirthTime = glGetUniformLocation(
        m_tireMarkProgram, "uHistoryFloorBirthTime");
    m_tireMarkUniformRetirementSeconds = glGetUniformLocation(
        m_tireMarkProgram, "uRetirementSeconds");
    m_tireMarkUniformDetailedDistance = glGetUniformLocation(
        m_tireMarkProgram, "uDetailedDistance");
    m_tireMarkUniformLodBlendWidth = glGetUniformLocation(
        m_tireMarkProgram, "uLodBlendWidth");
    m_tireMarkUniformDrawDistance = glGetUniformLocation(
        m_tireMarkProgram, "uDrawDistance");
    m_tireMarkUniformVisibilityFadeWidth = glGetUniformLocation(
        m_tireMarkProgram, "uVisibilityFadeWidth");
    m_tireMarkUniformCapDistance = glGetUniformLocation(
        m_tireMarkProgram, "uCapDistance");
    m_tireMarkUniformChunkOriginRelative = glGetUniformLocation(
        m_tireMarkProgram, "uChunkOriginRelative");

    m_marbleUniformView = glGetUniformLocation(m_marbleProgram, "uView");
    m_marbleUniformProjection = glGetUniformLocation(m_marbleProgram, "uProjection");
    m_marbleUniformDetailedDistance = glGetUniformLocation(
        m_marbleProgram, "uDetailedDistance");
    m_marbleUniformLodBlendWidth = glGetUniformLocation(
        m_marbleProgram, "uLodBlendWidth");
    m_marbleUniformDrawDistance = glGetUniformLocation(
        m_marbleProgram, "uDrawDistance");
    m_marbleUniformVisibilityFadeWidth = glGetUniformLocation(
        m_marbleProgram, "uVisibilityFadeWidth");
    m_marbleUniformChunkOriginRelative = glGetUniformLocation(
        m_marbleProgram, "uChunkOriginRelative");

    m_movingRubberUniformView = glGetUniformLocation(
        m_movingRubberProgram, "uView");
    m_movingRubberUniformProjection = glGetUniformLocation(
        m_movingRubberProgram, "uProjection");
    m_movingRubberUniformDrawDistance = glGetUniformLocation(
        m_movingRubberProgram, "uDrawDistance");
    m_movingRubberUniformVisibilityFadeWidth = glGetUniformLocation(
        m_movingRubberProgram, "uVisibilityFadeWidth");

    m_particleUniformView = glGetUniformLocation(m_particleProgram, "uView");
    m_particleUniformProjection = glGetUniformLocation(
        m_particleProgram, "uProjection");

    // PERF10: allocate transient staging capacity once, not once per frame.
    // 240k * 28-byte TrackVertex was a ~6.7 MB (~6.4 MiB) reserve/free cycle per view.
    m_trackVertexScratch.clear();
    m_trackVertexScratch.reserve(240000);
    m_particleVertexScratch.clear();
    m_particleVertexScratch.reserve(1024);
    m_marbleCellScratch.clear();
    m_movingRubberPacketScratch.clear();

    glGenVertexArrays(1, &m_trackVao);
    glGenBuffers(1, &m_trackVbo);
    glBindVertexArray(m_trackVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_trackVbo);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrackVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, sizeof(TrackVertex),
        reinterpret_cast<const void*>(sizeof(float) * 3));

    glGenVertexArrays(1, &m_particleVao);
    glGenBuffers(1, &m_particleVbo);
    glBindVertexArray(m_particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
        reinterpret_cast<const void*>(sizeof(float) * 3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
        reinterpret_cast<const void*>(sizeof(float) * 7));

    glGenVertexArrays(1, &m_movingRubberVao);
    glGenBuffers(1, &m_movingRubberVbo);
    glBindVertexArray(m_movingRubberVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_movingRubberVbo);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
    {
        const GLsizei stride = static_cast<GLsizei>(sizeof(MovingRubberGpuRecord));
        const auto attribute = [stride](GLuint index, GLint count, std::size_t offset) {
            glEnableVertexAttribArray(index);
            glVertexAttribPointer(
                index, count, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const void*>(offset));
        };
        attribute(0, 3, offsetof(MovingRubberGpuRecord, centerRelative));
        attribute(1, 3, offsetof(MovingRubberGpuRecord, axisRight));
        attribute(2, 3, offsetof(MovingRubberGpuRecord, axisForward));
        attribute(3, 3, offsetof(MovingRubberGpuRecord, axisNormal));
        attribute(4, 4, offsetof(MovingRubberGpuRecord, shape));
        attribute(5, 4, offsetof(MovingRubberGpuRecord, state));
        attribute(6, 4, offsetof(MovingRubberGpuRecord, misc));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    // WATER12: settled-water parcel rendering is retired from the active
    // presentation path, so do not allocate its 65k-particle SSBOs/FBOs at
    // startup. The class stays in-tree for future detached spray/splash work.
    return true;
}

void SurfacePresentationRenderer::shutdown()
{
    clearTireMarkGpuCache();
    clearMarbleGpuCache();
    if (m_trackVbo)
        glDeleteBuffers(1, &m_trackVbo);
    if (m_trackVao)
        glDeleteVertexArrays(1, &m_trackVao);
    if (m_particleVbo)
        glDeleteBuffers(1, &m_particleVbo);
    if (m_particleVao)
        glDeleteVertexArrays(1, &m_particleVao);
    if (m_movingRubberVbo)
        glDeleteBuffers(1, &m_movingRubberVbo);
    if (m_movingRubberVao)
        glDeleteVertexArrays(1, &m_movingRubberVao);
    if (m_trackProgram)
        glDeleteProgram(m_trackProgram);
    if (m_tireMarkProgram)
        glDeleteProgram(m_tireMarkProgram);
    if (m_marbleProgram)
        glDeleteProgram(m_marbleProgram);
    if (m_movingRubberProgram)
        glDeleteProgram(m_movingRubberProgram);
    if (m_particleProgram)
        glDeleteProgram(m_particleProgram);
    m_trackVbo = 0;
    m_trackVao = 0;
    std::vector<SurfaceTrackVertex>().swap(m_trackVertexScratch);
    std::vector<SurfaceParticleVertex>().swap(m_particleVertexScratch);
    std::vector<heritage::physics::rubber::TrackRubberVisualCell>().swap(
        m_marbleCellScratch);
    std::vector<heritage::physics::rubber::TrackRubberTransientVisual>().swap(
        m_movingRubberPacketScratch);
    m_particleVbo = 0;
    m_particleVao = 0;
    m_movingRubberVbo = 0;
    m_movingRubberVao = 0;
    m_movingRubberCapacity = 0;
    m_trackProgram = 0;
    m_trackUniformView = -1;
    m_trackUniformProjection = -1;
    m_trackUniformGamma = -1;
    m_trackUniformBrightness = -1;
    m_trackUniformContrast = -1;
    m_trackUniformSaturation = -1;
    m_tireMarkUniformView = -1;
    m_tireMarkUniformProjection = -1;
    m_tireMarkUniformPresentationTime = -1;
    m_tireMarkUniformHistoryFloorBirthTime = -1;
    m_tireMarkUniformRetirementSeconds = -1;
    m_tireMarkUniformDetailedDistance = -1;
    m_tireMarkUniformLodBlendWidth = -1;
    m_tireMarkUniformDrawDistance = -1;
    m_tireMarkUniformVisibilityFadeWidth = -1;
    m_tireMarkUniformCapDistance = -1;
    m_tireMarkUniformChunkOriginRelative = -1;
    m_marbleUniformView = -1;
    m_marbleUniformProjection = -1;
    m_marbleUniformDetailedDistance = -1;
    m_marbleUniformLodBlendWidth = -1;
    m_marbleUniformDrawDistance = -1;
    m_marbleUniformVisibilityFadeWidth = -1;
    m_marbleUniformChunkOriginRelative = -1;
    m_movingRubberUniformView = -1;
    m_movingRubberUniformProjection = -1;
    m_movingRubberUniformDrawDistance = -1;
    m_movingRubberUniformVisibilityFadeWidth = -1;
    m_particleUniformView = -1;
    m_particleUniformProjection = -1;
    m_tireMarkProgram = 0;
    m_marbleProgram = 0;
    m_movingRubberProgram = 0;
    m_particleProgram = 0;
    m_frameStats = {};
}

void SurfacePresentationRenderer::draw(
    const heritage::physics::SurfaceWorld& surfaces,
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings,
    const heritage::camera::CameraFrame& cameraFrame,
    const EnvironmentMap& environmentMap) const
{
    // WATER15 moved settled-water environment reflection to EntityMeshRenderer.
    // Keep the shared draw signature stable for other presentation callers.
    (void)environmentMap;
    if (!m_trackProgram || !m_tireMarkProgram || !m_marbleProgram
        || !m_movingRubberProgram || !m_particleProgram)
        return;

    const heritage::math::Vec3 eyeLocal = cameraFrame.valid
        ? cameraFrame.eyeLocal
        : heritage::math::Vec3{ 0.0f, 3.4f, 8.5f };
    const heritage::math::Vec3 targetLocal = cameraFrame.valid
        ? cameraFrame.targetLocal
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const heritage::math::Vec3 cameraUp = cameraFrame.valid
        ? cameraFrame.up
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const heritage::math::Vec3 cameraRelativeTarget = subtract(targetLocal, eyeLocal);
    const heritage::math::Mat4 view = lookAt(
        { 0.0f, 0.0f, 0.0f }, cameraRelativeTarget, cameraUp);
    const heritage::math::DVec3 cameraGlobal = surfaces.localToGlobal(eyeLocal);

    auto& trackVertices = m_trackVertexScratch;
    trackVertices.clear();
    constexpr double kTrackDrawDistanceM = 85.0;
    constexpr double kTrackDrawDistanceSquared =
        kTrackDrawDistanceM * kTrackDrawDistanceM;

    // TIRE16K: old tire-mark history is no longer scanned, distance-sorted,
    // tessellated and copied into one giant dynamic VBO every frame. Only new
    // serials are appended once to 100 m persistent GPU-cache pages. The GPU
    // performs per-segment age/range rejection and expands the six-control
    // pressure ribbon from the compact logical record.
    syncTireMarkGpuCache(surfaces.presentation());

    for (const heritage::physics::SurfaceTrackMark& mark
        : surfaces.presentation().trackMarks())
    {
        if (mark.updateSerial == 0 || mark.intensity <= 0.001f)
            continue;
        const double dx = mark.globalPosition.x - cameraGlobal.x;
        const double dy = mark.globalPosition.y - cameraGlobal.y;
        const double dz = mark.globalPosition.z - cameraGlobal.z;
        const double distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared > kTrackDrawDistanceSquared)
            continue;
        const float rangeVisibility = heritage::graphics::lod::visibilityWeight(
            static_cast<float>(std::sqrt(distanceSquared)),
            static_cast<float>(kTrackDrawDistanceM));
        if (rangeVisibility <= 0.0001f)
            continue;

        heritage::math::Vec3 normal = normalize(
            mark.normal, { 0.0f, 1.0f, 0.0f });
        heritage::math::Vec3 forward = normalize(
            mark.forward, { 0.0f, 0.0f, 1.0f });
        heritage::math::Vec3 right = normalize(
            cross(normal, forward), { 1.0f, 0.0f, 0.0f });
        forward = normalize(cross(right, normal), forward);

        const float rutVisual = std::clamp(mark.rutDepthM, 0.0f, 0.16f);
        const float surfaceLift = 0.007f + rutVisual * 0.04f;
        const float shoulderLift = surfaceLift + std::min(rutVisual * 0.35f, 0.028f);
        heritage::math::Vec3 center = heritage::graphics::presentation::cameraRelativeFp32(mark.globalPosition, cameraGlobal);
        center = add(center, scale(normal, surfaceLift));

        const float halfLength = std::max(mark.lengthM * 0.5f, 0.10f);
        const float halfWidth = std::max(mark.widthM * 0.5f, 0.04f);
        const float centerHalfWidth = halfWidth * 0.68f;
        const float alpha = (0.20f + 0.43f * std::clamp(mark.intensity, 0.0f, 1.0f))
            * rangeVisibility;

        const auto point = [&](float longitudinal, float lateral, float lift) {
            return add(
                center,
                add(
                    scale(forward, longitudinal),
                    add(scale(right, lateral), scale(normal, lift))));
        };

        const heritage::math::Vec3 centerColor = trackCenterColor(mark.material);
        const heritage::math::Vec3 shoulderColor = trackShoulderColor(mark.material);

        appendQuad(
            trackVertices,
            point(-halfLength, -centerHalfWidth, 0.0f),
            point(halfLength, -centerHalfWidth, 0.0f),
            point(halfLength, centerHalfWidth, 0.0f),
            point(-halfLength, centerHalfWidth, 0.0f),
            centerColor,
            alpha);
        appendQuad(
            trackVertices,
            point(-halfLength, -halfWidth, shoulderLift),
            point(halfLength, -halfWidth, shoulderLift),
            point(halfLength, -centerHalfWidth, 0.0f),
            point(-halfLength, -centerHalfWidth, 0.0f),
            shoulderColor,
            alpha * 0.75f);
        appendQuad(
            trackVertices,
            point(-halfLength, centerHalfWidth, 0.0f),
            point(halfLength, centerHalfWidth, 0.0f),
            point(halfLength, halfWidth, shoulderLift),
            point(-halfLength, halfWidth, shoulderLift),
            shoulderColor,
            alpha * 0.75f);
        ++m_frameStats.visibleTrackMarks;
    }

    // DSURF03B: water/moisture/flow diagnostics now come from Heritage
    // Dynamic Surface authority. The old adaptive SurfaceHydrology solver is
    // no longer advanced by SurfaceWorld and cannot own tire/rendered water.
    const auto& hydrologyStats = surfaces.dynamicSurface().hydroStats();
    constexpr std::size_t kCellsPerAuthorityPage =
        heritage::physics::dynamicsurface::kHydroAuthorityResolution
        * heritage::physics::dynamicsurface::kHydroAuthorityResolution;
    m_frameStats.waterHydrologyStepMs = hydrologyStats.lastStepMilliseconds;
    m_frameStats.waterHydrologyHz = hydrologyStats.activePages > 0u
        ? heritage::physics::dynamicsurface::UpdateCadence::hydroNearHz : 0.0;
    m_frameStats.waterWetCells = hydrologyStats.wetTexels;
    m_frameStats.waterTotalCells = hydrologyStats.validTexels;
    m_frameStats.waterSupportCells = hydrologyStats.validTexels;
    m_frameStats.waterSimulationMinimumCellM =
        heritage::physics::dynamicsurface::kHydroAuthorityTexelPitchM;
    m_frameStats.waterSimulationMaximumCellM =
        heritage::physics::dynamicsurface::kHydroAuthorityTexelPitchM;
    m_frameStats.waterPresentationBasins = 0u;
    m_frameStats.waterActivePresentationBasins = 0u;
    m_frameStats.waterInterestSources =
        surfaces.dynamicSurface().interestSources().size();
    m_frameStats.waterCadence30Cells =
        hydrologyStats.cadence30HzPages * kCellsPerAuthorityPage;
    m_frameStats.waterCadence20Cells =
        hydrologyStats.cadence20HzPages * kCellsPerAuthorityPage;
    m_frameStats.waterCadence6Cells =
        hydrologyStats.cadence6HzPages * kCellsPerAuthorityPage;
    m_frameStats.waterCadence2Cells =
        hydrologyStats.cadence2HzPages * kCellsPerAuthorityPage;
    m_frameStats.waterCadenceBackgroundCells =
        hydrologyStats.cadenceDistantPages * kCellsPerAuthorityPage;
    m_frameStats.waterScheduledCells =
        hydrologyStats.scheduledPagesThisAdvance * kCellsPerAuthorityPage;
    m_frameStats.waterMaximumFlowSpeedMps = hydrologyStats.maximumFlowSpeedMps;

    // DSURF04: Track.R is now the persistent, sheet-aware road-temperature
    // authority. SurfaceWeather's scalar road temperature is only an
    // environmental compatibility/reference value after this cutover.
    const auto& thermalStats = surfaces.dynamicSurface().thermalStats();
    m_frameStats.surfaceThermalStepMs = thermalStats.lastStepMilliseconds;
    m_frameStats.surfaceThermalCells = thermalStats.validTexels;
    m_frameStats.surfaceTemperatureMinimumC = thermalStats.minimumTemperatureC;
    m_frameStats.surfaceTemperatureAverageC = thermalStats.averageTemperatureC;
    m_frameStats.surfaceTemperatureMaximumC = thermalStats.maximumTemperatureC;
    m_frameStats.surfaceThermalTireContacts = thermalStats.tireContactCount;

    // TIRE16L: resting marbles are synchronized as compact persistent GPU
    // cell records. Moving packets remain authoritative CPU simulation but
    // are uploaded one-record-per-packet and expanded into flakes on the GPU.
    // Neither path contributes per-flake CPU triangles to trackVertices.
    syncMarbleGpuCache(surfaces, cameraGlobal);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    // WATER15: no second settled-water geometry pass exists here. Wetness,
    // drying lines, puddles, pooling, reflections and ripples are composited
    // over the original scene surface by EntityMeshRenderer. This permanently
    // removes the WATER08-WATER14 water-ring mesh from depth ownership.

    // TIRE16K persistent GPU tire-mark pages. No per-frame history vector,
    // sorting, ribbon tessellation or giant dynamic-VBO upload remains here.
    drawTireMarkGpuCache(
        surfaces.presentation(), view, projection, cameraGlobal);
    drawMarbleGpuCache(view, projection, cameraGlobal);
    drawMovingRubberGpu(surfaces, view, projection, cameraGlobal);

    if (!trackVertices.empty())
    {
        glUseProgram(m_trackProgram);
        glUniformMatrix4fv(
            m_trackUniformView,
            1, GL_FALSE, view.m);
        glUniformMatrix4fv(
            m_trackUniformProjection,
            1, GL_FALSE, projection.m);
        glUniform1f(m_trackUniformGamma, videoSettings.gamma);
        glUniform1f(m_trackUniformBrightness, videoSettings.brightness);
        glUniform1f(m_trackUniformContrast, videoSettings.contrast);
        glUniform1f(m_trackUniformSaturation, videoSettings.saturation);
        glBindVertexArray(m_trackVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_trackVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(trackVertices.size() * sizeof(TrackVertex)),
            trackVertices.data(),
            GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(trackVertices.size()));
        ++m_frameStats.drawCalls;
        m_frameStats.trackTriangles += trackVertices.size() / 3;
    }

    auto& particleVertices = m_particleVertexScratch;
    particleVertices.clear();
    constexpr double kParticleDrawDistanceM = 110.0;
    constexpr double kParticleDrawDistanceSquared =
        kParticleDrawDistanceM * kParticleDrawDistanceM;
    for (const heritage::physics::SurfacePresentationParticle& particle
        : surfaces.presentation().particles())
    {
        if (particle.kind == heritage::physics::SurfaceParticleKind::RubberShred)
            continue;
        if (particle.ageSeconds >= particle.lifetimeSeconds)
            continue;
        const double dx = particle.globalPosition.x - cameraGlobal.x;
        const double dy = particle.globalPosition.y - cameraGlobal.y;
        const double dz = particle.globalPosition.z - cameraGlobal.z;
        const double distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared > kParticleDrawDistanceSquared)
            continue;
        const float distance = static_cast<float>(std::sqrt(distanceSquared));
        const float rangeVisibility = heritage::graphics::lod::visibilityWeight(
            distance, static_cast<float>(kParticleDrawDistanceM));
        if (rangeVisibility <= 0.0001f)
            continue;

        const float life = particle.lifetimeSeconds > 0.0001f
            ? std::clamp(
                1.0f - particle.ageSeconds / particle.lifetimeSeconds,
                0.0f, 1.0f)
            : 0.0f;
        const heritage::math::Vec3 relative = heritage::graphics::presentation::cameraRelativeFp32(
            particle.globalPosition, cameraGlobal);
        const heritage::math::Vec3 color = particleColor(
            particle.kind, particle.material);
        const bool failureSmoke = particle.kind
            == heritage::physics::SurfaceParticleKind::TireFailureSmoke;
        const bool failureDebris = particle.kind
            == heritage::physics::SurfaceParticleKind::TireFailureDebris;
        const float pointSize = std::clamp(
            particle.sizeM * 600.0f / std::max(distance, 1.0f),
            2.0f,
            particle.kind == heritage::physics::SurfaceParticleKind::Dust
                ? 22.0f
                : (failureSmoke ? 42.0f : (failureDebris ? 30.0f : 15.0f)));
        particleVertices.push_back({
            relative.x, relative.y, relative.z,
            color.x, color.y, color.z,
            particle.opacity * life * rangeVisibility,
            pointSize
        });
        ++m_frameStats.visibleParticles;
    }

    if (!particleVertices.empty())
    {
        glUseProgram(m_particleProgram);
        glUniformMatrix4fv(
            m_particleUniformView,
            1, GL_FALSE, view.m);
        glUniformMatrix4fv(
            m_particleUniformProjection,
            1, GL_FALSE, projection.m);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glBindVertexArray(m_particleVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(particleVertices.size() * sizeof(ParticleVertex)),
            particleVertices.data(),
            GL_DYNAMIC_DRAW);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(particleVertices.size()));
        glDisable(GL_PROGRAM_POINT_SIZE);
        ++m_frameStats.drawCalls;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

} // namespace heritage::graphics
