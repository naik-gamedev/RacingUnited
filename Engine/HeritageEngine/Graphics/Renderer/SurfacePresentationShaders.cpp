#include "SurfacePresentationShaders.hpp"

namespace heritage::graphics::surface_presentation_shaders {

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

} // namespace heritage::graphics::surface_presentation_shaders
