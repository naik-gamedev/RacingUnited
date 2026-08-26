#pragma once

#include <string>
#include <glad/glad.h>

namespace heritage::graphics::dynamicsurface::detail {


constexpr const char* kSupportGlsl = R"glsl(
struct DsurfSurfaceTriangle
{
    vec4 a;
    vec4 b;
    vec4 c;
    vec4 hydro;
};
layout(std430, binding = 20) readonly buffer DsurfTriangles
{
    DsurfSurfaceTriangle dsurfTriangles[];
};
layout(std430, binding = 21) readonly buffer DsurfBinHeaders
{
    uvec2 dsurfBinHeaders[];
};
layout(std430, binding = 22) readonly buffer DsurfBinIndices
{
    uint dsurfBinIndices[];
};
layout(std430, binding = 23) readonly buffer DsurfChunkMeta
{
    uvec4 dsurfChunkMeta[];
};
uniform ivec2 uGeometryCenterChunk;

const int kDsurfGeometryBinResolution = 64;
const int kDsurfGeometryGridResolution = 5;
const int kDsurfGeometryGridHalfSpan = 2;
const float kDsurfChunkSizeM = 100.0;

bool dsurfTriangleHeightAt(
    DsurfSurfaceTriangle tri,
    vec2 localXZ,
    out float heightY)
{
    vec2 a = tri.a.xz;
    vec2 b = tri.b.xz;
    vec2 c = tri.c.xz;
    vec2 v0 = b - a;
    vec2 v1 = c - a;
    vec2 v2 = localXZ - a;
    float denom = v0.x * v1.y - v0.y * v1.x;
    if (abs(denom) < 1.0e-10)
        return false;
    float invDenom = 1.0 / denom;
    float u = (v2.x * v1.y - v2.y * v1.x) * invDenom;
    float v = (v0.x * v2.y - v0.y * v2.x) * invDenom;
    float w = 1.0 - u - v;
    const float edgeEpsilon = -1.0e-5;
    if (u < edgeEpsilon || v < edgeEpsilon || w < edgeEpsilon)
        return false;
    heightY = tri.a.y * w + tri.b.y * u + tri.c.y * v;
    return true;
}

bool dsurfChunkSlot(ivec2 worldChunk, out uint slot)
{
    ivec2 mapCoord = worldChunk - uGeometryCenterChunk
        + ivec2(kDsurfGeometryGridHalfSpan);
    if (any(lessThan(mapCoord, ivec2(0)))
        || any(greaterThanEqual(mapCoord, ivec2(kDsurfGeometryGridResolution))))
        return false;
    slot = uint(mapCoord.y * kDsurfGeometryGridResolution + mapCoord.x);
    return true;
}

float dsurfSupportAtGlobal(
    vec2 globalXZ,
    out bool valid,
    out vec4 staticHydrology,
    out uint surfaceSheetId)
{
    valid = false;
    staticHydrology = vec4(0.0, 0.0, 0.02, 0.0002);
    surfaceSheetId = 0xffffffffu;

    ivec2 worldChunk = ivec2(floor(globalXZ / kDsurfChunkSizeM));
    uint slot = 0u;
    if (!dsurfChunkSlot(worldChunk, slot))
        return 0.0;
    uvec4 meta = dsurfChunkMeta[slot];
    if (meta.y == 0u)
        return 0.0;

    vec2 localXZ = globalXZ - vec2(worldChunk) * kDsurfChunkSizeM;
    localXZ = clamp(localXZ, vec2(0.0), vec2(99.999999));
    ivec2 bin = clamp(
        ivec2(floor(localXZ * (float(kDsurfGeometryBinResolution) / kDsurfChunkSizeM))),
        ivec2(0),
        ivec2(kDsurfGeometryBinResolution - 1));
    uint headerIndex = meta.x
        + uint(bin.y * kDsurfGeometryBinResolution + bin.x);
    uvec2 header = dsurfBinHeaders[headerIndex];

    // LIVETRACK04 water has no vertical sheet ownership. At a given X/Z the
    // single Hydro field uses the highest authored receiver as its static
    // support reference. This deliberately avoids per-sheet masks/pages at the
    // 10m/256 resolution; stacked surfaces share one X/Z water state.
    float bestHeight = -3.402823466e+38;
    uint bestTriangle = 0xffffffffu;
    for (uint i = 0u; i < header.y; ++i)
    {
        uint triangleIndex = dsurfBinIndices[header.x + i];
        DsurfSurfaceTriangle tri = dsurfTriangles[triangleIndex];
        float heightY = 0.0;
        if (!dsurfTriangleHeightAt(tri, localXZ, heightY))
            continue;
        if (bestTriangle == 0xffffffffu || heightY > bestHeight)
        {
            bestHeight = heightY;
            bestTriangle = triangleIndex;
        }
    }
    if (bestTriangle == 0xffffffffu)
        return 0.0;

    DsurfSurfaceTriangle selected = dsurfTriangles[bestTriangle];
    staticHydrology = selected.hydro;
    surfaceSheetId = 0u;
    valid = true;
    return bestHeight;
}
)glsl";

inline std::string makeSnowShader()
{
    return std::string(R"glsl(
#version 460 core
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(binding = 0) uniform usampler2D uStateAtlas;
layout(r16ui, binding = 1) uniform writeonly uimage2D uDestinationScratch;
uniform ivec2 uWorldTile;
uniform ivec2 uAtlasOrigin;
uniform float uCellSizeM;
uniform float uCycleDtSeconds;
uniform float uPrecipitationRateMmPerHour;
uniform float uAmbientTemperatureC;
)glsl") + kSupportGlsl + R"glsl(
const float kSnowMaximumM = 1.0;
float decodeSnow(uint q)
{
    float n = float(q & 0x0fffu) / 4095.0;
    return n * n * kSnowMaximumM;
}
uint encodeSnow(float depthM)
{
    return uint(round(sqrt(clamp(depthM / kSnowMaximumM, 0.0, 1.0)) * 4095.0));
}
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(p, ivec2(256))))
        return;
    vec2 globalXZ = vec2(uWorldTile) * 10.0 + (vec2(p) + vec2(0.5)) * uCellSizeM;
    bool supportValid = false;
    vec4 hydro = vec4(0.0);
    uint sheetId = 0u;
    dsurfSupportAtGlobal(globalXZ, supportValid, hydro, sheetId);
    if (!supportValid)
    {
        imageStore(uDestinationScratch, p, uvec4(0u));
        return;
    }
    uint packedState = texelFetch(uStateAtlas, uAtlasOrigin + p, 0).r;
    float depthM = decodeSnow(packedState);
    uint compaction = (packedState >> 12u) & 0x0fu;
    float dt = max(uCycleDtSeconds, 0.0);
    if (uAmbientTemperatureC <= 0.5 && uPrecipitationRateMmPerHour > 0.0)
        depthM += uPrecipitationRateMmPerHour * (0.001 / 3600.0) * dt;
    else if (uAmbientTemperatureC > 0.5 && depthM > 0.0)
        depthM = max(depthM - (uAmbientTemperatureC - 0.5) * 0.0000025 * dt, 0.0);
    imageStore(uDestinationScratch, p,
        uvec4(encodeSnow(depthM) | (compaction << 12u), 0u, 0u, 0u));
}
)glsl";
}

inline std::string makeMudShader()
{
    return std::string(R"glsl(
#version 460 core
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(binding = 0) uniform usampler2D uStateAtlas;
layout(r8ui, binding = 1) uniform writeonly uimage2D uDestinationScratch;
uniform ivec2 uWorldTile;
uniform ivec2 uAtlasOrigin;
uniform float uCellSizeM;
)glsl") + kSupportGlsl + R"glsl(
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(p, ivec2(256))))
        return;
    vec2 globalXZ = vec2(uWorldTile) * 10.0 + (vec2(p) + vec2(0.5)) * uCellSizeM;
    bool supportValid = false;
    vec4 hydro = vec4(0.0);
    uint sheetId = 0u;
    dsurfSupportAtGlobal(globalXZ, supportValid, hydro, sheetId);
    if (!supportValid)
    {
        imageStore(uDestinationScratch, p, uvec4(0u));
        return;
    }
    imageStore(uDestinationScratch, p,
        uvec4(texelFetch(uStateAtlas, uAtlasOrigin + p, 0).r));
}
)glsl";
}

constexpr const char* kTireEventComputeShader = R"glsl(
#version 460 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba8, binding = 0) uniform image2D uWater;
layout(r16ui, binding = 1) uniform uimage2D uSnow;
layout(r8ui, binding = 2) uniform uimage2D uMud;
uniform ivec2 uAtlasOrigin;
uniform ivec2 uMinTexel;
uniform ivec2 uExtentTexels;
uniform float uCellSizeM;
uniform vec2 uEventLocalXZ;
uniform vec2 uForwardXZ;
uniform vec2 uRightXZ;
uniform float uPatchHalfLengthM;
uniform float uPatchHalfWidthM;
uniform float uNormalLoadN;
uniform float uSpeedMps;
uniform float uAccumulatedDtSeconds;
uniform bool uSnowReady;
uniform bool uMudReady;
uniform bool uMudDeformable;
const float kWaterLevelsM[16] = float[](
    0.0, 0.0001, 0.0005, 0.0010, 0.0020, 0.0030, 0.0040, 0.0060,
    0.0080, 0.0100, 0.0130, 0.0160, 0.0200, 0.0240, 0.0280, 0.0320);
int q4(float n) { return clamp(int(round(clamp(n, 0.0, 1.0) * 15.0)), 0, 15); }
float n4(int q) { return float(clamp(q, 0, 15)) * (1.0 / 15.0); }
float decodeWater(float r) { return kWaterLevelsM[q4(r)]; }
int encodeWater(float d)
{
    d = clamp(d, 0.0, 0.032);
    int best = 0;
    float bestError = abs(d - kWaterLevelsM[0]);
    for (int i = 1; i < 16; ++i)
    {
        float e = abs(d - kWaterLevelsM[i]);
        if (e < bestError) { best = i; bestError = e; }
    }
    return best;
}
float decodeSnow(uint state)
{
    float n = float(state & 0x0fffu) / 4095.0;
    return n * n;
}
uint encodeSnow(float depthM)
{
    return uint(round(sqrt(clamp(depthM, 0.0, 1.0)) * 4095.0));
}
void main()
{
    ivec2 local = ivec2(gl_GlobalInvocationID.xy);
    if (local.x >= uExtentTexels.x || local.y >= uExtentTexels.y)
        return;
    ivec2 texel = uMinTexel + local;
    if (any(lessThan(texel, ivec2(0))) || any(greaterThanEqual(texel, ivec2(256))))
        return;
    vec2 cellCenterM = (vec2(texel) + vec2(0.5)) * uCellSizeM;
    vec2 deltaM = cellCenterM - uEventLocalXZ;
    vec2 forward = normalize(length(uForwardXZ) > 1e-5 ? uForwardXZ : vec2(0.0, 1.0));
    vec2 right = normalize(length(uRightXZ) > 1e-5 ? uRightXZ : vec2(1.0, 0.0));
    float longitudinal = dot(deltaM, forward) / max(uPatchHalfLengthM, uCellSizeM);
    float lateral = dot(deltaM, right) / max(uPatchHalfWidthM, uCellSizeM);
    float radius2 = longitudinal * longitudinal + lateral * lateral;
    if (radius2 > 1.0)
        return;
    float profile = 1.0 - smoothstep(0.28, 1.0, sqrt(max(radius2, 0.0)));
    float timeStrength = clamp(uAccumulatedDtSeconds * 60.0, 0.05, 1.0);
    float loadStrength = clamp(uNormalLoadN / 5000.0, 0.15, 1.5);
    float speedStrength = clamp(0.35 + uSpeedMps * 0.035, 0.35, 1.4);
    float strength = clamp(timeStrength * loadStrength * speedStrength, 0.0, 1.0);
    ivec2 p = uAtlasOrigin + texel;

    vec4 water = imageLoad(uWater, p);
    float dryLine = n4(q4(water.g));
    dryLine = clamp(dryLine + profile * strength * 0.28, 0.0, 1.0);
    // LIVETRACK18 water depth is not stored/mutated here. R/B/A are immutable
    // prebaked runoff accumulation/capacity/flow. Tires only modify local dry-line.
    imageStore(uWater, p,
        vec4(water.r, n4(int(round(dryLine * 15.0))), water.b, water.a));

    if (uSnowReady)
    {
        uint snow = imageLoad(uSnow, p).r;
        float snowDepth = decodeSnow(snow);
        uint compaction = (snow >> 12u) & 0x0fu;
        float depression = clamp(profile * strength, 0.0, 0.85);
        snowDepth *= (1.0 - depression * 0.38);
        compaction = min(15u, compaction + uint(round(profile * strength * 5.0)));
        imageStore(uSnow, p,
            uvec4(encodeSnow(snowDepth) | (compaction << 12u), 0u, 0u, 0u));
    }
    if (uMudReady && uMudDeformable)
    {
        uint mud = imageLoad(uMud, p).r;
        uint added = uint(round(clamp(profile * strength, 0.0, 1.0) * 14.0));
        imageStore(uMud, p, uvec4(min(255u, mud + added), 0u, 0u, 0u));
    }
}
)glsl";

constexpr const char* kTireWaterSampleComputeShader = R"glsl(
#version 460 core
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer TireWaterSampleInput
{
    vec4 uSamplePositions[]; // tile delta XY + tile UV ZW
};
layout(std430, binding = 1) writeonly buffer TireWaterSampleOutput
{
    vec4 uSampleResults[]; // depth M, dry line, valid, reserved
};
uniform sampler2D uWaterAtlas;
uniform usampler2D uTileIndirection;
uniform int uTileResolution;
uniform int uAtlasColumns;
uniform int uTileMapCenter;
uniform float uPrebakedWaterExposureM;
uniform float uRainWettingExposureM;
uniform float uRunoffDriverMmPerHour;
uniform int uSampleCount;

struct WaterDecoded
{
    float depthM;
    float dryLine;
    float basinStrength;
    float runoffPotential;
    float runoffAreaM2;
    float flowStrength;
    vec2 flowDirection;
};

float waterDepthFromLadderCode(int code)
{
    code = clamp(code, 0, 15);
    if (code == 0) return 0.0;
    if (code == 1) return 0.00001;
    return float(code - 1) * 0.00005;
}
float quantizeStandingWaterDepth(float depthM)
{
    depthM = clamp(depthM, 0.0, 0.00070);
    if (depthM < 0.000005) return 0.0;
    if (depthM < 0.000030) return 0.00001;
    int code = clamp(int(floor(depthM / 0.00005 + 0.5)) + 1, 2, 15);
    return float(code - 1) * 0.00005;
}
float decodeRunoffAreaM2(float encoded)
{
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
vec2 decodeFlowDirection(float encoded)
{
    const vec2 directions[16] = vec2[16](
        vec2(0.0, 0.0), vec2(-1.00000000, 0.00000000),
        vec2(-0.90096887, -0.43388374), vec2(-0.62348980, -0.78183148),
        vec2(-0.22252093, -0.97492791), vec2(0.22252093, -0.97492791),
        vec2(0.62348980, -0.78183148), vec2(0.90096887, -0.43388374),
        vec2(1.00000000, 0.00000000), vec2(0.90096887, 0.43388374),
        vec2(0.62348980, 0.78183148), vec2(0.22252093, 0.97492791),
        vec2(-0.22252093, 0.97492791), vec2(-0.62348980, 0.78183148),
        vec2(-0.90096887, 0.43388374), vec2(-1.00000000, 0.00000000));
    return directions[clamp(int(round(clamp(encoded, 0.0, 1.0) * 15.0)), 0, 15)];
}
WaterDecoded decodeWater(vec4 state)
{
    WaterDecoded d;
    d.dryLine = clamp(state.g, 0.0, 1.0);
    d.runoffPotential = clamp(state.r, 0.0, 1.0);
    d.runoffAreaM2 = decodeRunoffAreaM2(state.r);
    int capacityCode = int(round(clamp(state.b, 0.0, 1.0) * 15.0));
    float capacityM = waterDepthFromLadderCode(capacityCode);
    d.basinStrength = step(0.000005, capacityM);
    float catchmentFill = mix(0.72, 1.22, smoothstep(0.10, 0.78, d.runoffPotential));
    float retainedHeadDriverM = clamp(uPrebakedWaterExposureM * catchmentFill, 0.0, 0.0040);
    float fillProgress = smoothstep(0.000050, 0.00350, retainedHeadDriverM);
    float headDeficitM = 0.00070 * (1.0 - pow(fillProgress, 0.72));
    d.depthM = min(max(capacityM - headDeficitM, 0.0), capacityM)
        * (1.0 - 0.72 * d.dryLine);
    d.flowDirection = decodeFlowDirection(state.a);
    d.flowStrength = length(d.flowDirection) > 0.5 ? 1.0 : 0.0;
    return d;
}
bool waterStateAt(vec2 tilePosition, out vec4 state)
{
    ivec2 tileDelta = ivec2(floor(tilePosition));
    vec2 tileUv = fract(tilePosition);
    ivec2 mapCoord = tileDelta + ivec2(uTileMapCenter);
    ivec2 mapSize = textureSize(uTileIndirection, 0);
    if (any(lessThan(mapCoord, ivec2(0))) || any(greaterThanEqual(mapCoord, mapSize)))
        return false;
    uint encoded = texelFetch(uTileIndirection, mapCoord, 0).r;
    if (encoded == 0u)
        return false;
    uint slot = encoded - 1u;
    int resolution = max(uTileResolution, 1);
    ivec2 origin = ivec2(
        int(slot % uint(max(uAtlasColumns, 1))) * resolution,
        int(slot / uint(max(uAtlasColumns, 1))) * resolution);
    vec2 atlasSize = vec2(textureSize(uWaterAtlas, 0));
    vec2 minUv = (vec2(origin) + vec2(0.5)) / atlasSize;
    vec2 maxUv = (vec2(origin) + vec2(float(resolution) - 0.5)) / atlasSize;
    vec2 atlasUv = (vec2(origin) + tileUv * float(resolution)) / atlasSize;
    state = texture(uWaterAtlas, clamp(atlasUv, minUv, maxUv));
    return true;
}
bool filteredWater(vec4 inputPosition, out WaterDecoded d)
{
    vec2 centerPosition = inputPosition.xy + inputPosition.zw;
    vec4 centerState = vec4(0.0);
    if (!waterStateAt(centerPosition, centerState))
        return false;
    WaterDecoded center = decodeWater(centerState);
    float texelTile = 1.0 / float(max(uTileResolution, 1));
    const vec2 offsets[4] = vec2[4](vec2(-1.0,0.0), vec2(1.0,0.0), vec2(0.0,-1.0), vec2(0.0,1.0));
    float depthSum = center.depthM * 4.0;
    float drySum = center.dryLine * 4.0;
    float basinSum = center.basinStrength * 4.0;
    float runoffSum = center.runoffPotential * 4.0;
    float runoffAreaSum = center.runoffAreaM2 * 4.0;
    vec2 flowSum = center.flowDirection * 4.0;
    float weightSum = 4.0;
    for (int i = 0; i < 4; ++i)
    {
        vec4 state = centerState;
        WaterDecoded sampleValue = center;
        if (waterStateAt(centerPosition + offsets[i] * texelTile, state))
            sampleValue = decodeWater(state);
        depthSum += sampleValue.depthM * 2.0;
        drySum += sampleValue.dryLine * 2.0;
        basinSum += sampleValue.basinStrength * 2.0;
        runoffSum += sampleValue.runoffPotential * 2.0;
        runoffAreaSum += sampleValue.runoffAreaM2 * 2.0;
        flowSum += sampleValue.flowDirection * 2.0;
        weightSum += 2.0;
    }
    float inv = 1.0 / weightSum;
    d.depthM = quantizeStandingWaterDepth(depthSum * inv);
    d.dryLine = drySum * inv;
    d.basinStrength = basinSum * inv;
    d.runoffPotential = runoffSum * inv;
    d.runoffAreaM2 = runoffAreaSum * inv;
    float coherence = length(flowSum) * inv;
    d.flowStrength = clamp(coherence, 0.0, 1.0);
    d.flowDirection = coherence > 1.0e-4 ? normalize(flowSum) : vec2(0.0);
    return true;
}
float runoffDepthM(WaterDecoded d)
{
    if (d.runoffAreaM2 <= 0.25 || d.flowStrength <= 0.035 || uRunoffDriverMmPerHour <= 0.01)
        return 0.0;
    float rainfallMps = max(uRunoffDriverMmPerHour, 0.0) * (0.001 / 3600.0);
    float dischargeM3ps = rainfallMps * max(d.runoffAreaM2, 0.0);
    float effectiveWidthM = clamp(0.28 + 0.080 * sqrt(max(d.runoffAreaM2, 0.0)), 0.28, 2.40);
    float unitDischargeM2ps = dischargeM3ps / effectiveWidthM;
    float slope = mix(0.006, 0.024, clamp(d.flowStrength, 0.0, 1.0));
    float depthM = pow(max(unitDischargeM2ps * 0.014 / sqrt(max(slope, 0.0005)), 0.0), 0.60);
    float connected = smoothstep(0.000010, 0.000180, uRainWettingExposureM);
    depthM *= connected * 0.65 * (1.0 - 0.64 * d.dryLine);
    return clamp(depthM, 0.0, 0.0030);
}
void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= uint(max(uSampleCount, 0)))
        return;
    WaterDecoded d;
    if (!filteredWater(uSamplePositions[index], d))
    {
        uSampleResults[index] = vec4(0.0);
        return;
    }
    float depthM = max(max(d.depthM, 0.0), runoffDepthM(d));
    uSampleResults[index] = vec4(depthM, clamp(d.dryLine, 0.0, 1.0), 1.0, 0.0);
}
)glsl";

inline bool checkNoGlError(std::string& errorMessage, const char* operation)
{
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR)
        return true;
    errorMessage = std::string("OpenGL error during ") + operation
        + ": " + std::to_string(static_cast<unsigned int>(error));
    return false;
}


} // namespace heritage::graphics::dynamicsurface::detail
