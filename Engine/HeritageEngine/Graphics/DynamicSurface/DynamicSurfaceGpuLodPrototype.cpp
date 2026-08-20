#include "DynamicSurfaceGpuLodPrototype.hpp"

#include "../ShaderProgram.hpp"
#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <set>
#include <unordered_set>

namespace heritage::graphics::dynamicsurface {
namespace {

constexpr float kWaterDepthMaximumM = 0.032f;
constexpr float kVelocityMaximumMps = 4.0f;
constexpr std::uint32_t kWaterDepthMask = 0x3fffu;
constexpr std::uint32_t kWaterVelocityMask = 0x7fu;

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

constexpr const char* kTileLookupGlsl = R"glsl(
uniform usampler2D uTileMap;
uniform ivec2 uTileMapOrigin;
const float kDsurfAuthorityTileSizeM = 10.0;
const int kDsurfAuthorityResolution = 256;
// Must match DynamicSurfaceGpuLodPrototype::kAtlasColumns.  LIVETRACK07 uses a
// bounded 20x20 atlas; leaving the former 32-column stride here makes every
// neighbour lookup beyond the first atlas row address an unrelated tile.
const int kDsurfAtlasColumns = 20;

bool dsurfAtlasSlotForWorldTile(ivec2 worldTile, out uint slot)
{
    ivec2 mapCoord = worldTile - uTileMapOrigin;
    ivec2 mapSize = textureSize(uTileMap, 0);
    if (any(lessThan(mapCoord, ivec2(0))) || any(greaterThanEqual(mapCoord, mapSize)))
        return false;
    uint encoded = texelFetch(uTileMap, mapCoord, 0).r;
    if (encoded == 0u)
        return false;
    slot = encoded - 1u;
    return true;
}

ivec2 dsurfAtlasOrigin(uint slot)
{
    return ivec2(
        int(slot % uint(kDsurfAtlasColumns)) * kDsurfAuthorityResolution,
        int(slot / uint(kDsurfAtlasColumns)) * kDsurfAuthorityResolution);
}

bool dsurfAtlasTexelAtGlobal(vec2 globalXZ, out ivec2 atlasTexel)
{
    ivec2 worldTile = ivec2(floor(globalXZ / kDsurfAuthorityTileSizeM));
    uint slot = 0u;
    if (!dsurfAtlasSlotForWorldTile(worldTile, slot))
        return false;
    vec2 localXZ = globalXZ - vec2(worldTile) * kDsurfAuthorityTileSizeM;
    localXZ = clamp(localXZ, vec2(0.0), vec2(9.999999));
    ivec2 localTexel = clamp(
        ivec2(floor(localXZ * (float(kDsurfAuthorityResolution) / kDsurfAuthorityTileSizeM))),
        ivec2(0), ivec2(kDsurfAuthorityResolution - 1));
    atlasTexel = dsurfAtlasOrigin(slot) + localTexel;
    return true;
}
)glsl";

// LIVETRACK04 has no secondary water-presentation compute pass. The same
// authoritative GPU Hydro atlas is sampled directly by the material shader.

std::string makeWaterShader()
{
    return std::string(R"glsl(
#version 460 core
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
uniform sampler2D uStateAtlas;
layout(rgba8, binding = 1) writeonly uniform image2DArray uDestinationBatchScratch;
uniform float uCellSizeM;
uniform float uPrecipitationRateMmPerHour;
uniform float uWeatherDrainageRateMmPerHour;
uniform float uEvaporationRateMmPerHour;
uniform float uAmbientTemperatureC;
uniform uint uTickIndex;
uniform uint uBatchTileCount;
struct DsurfHydroBatchTile
{
    ivec2 worldTile;
    ivec2 atlasOrigin;
    uint worldStateIndex;
    float cycleDtSeconds;
    uint reserved0;
    uint reserved1;
};
layout(std430, binding = 25) readonly buffer DsurfHydroBatchTiles
{
    DsurfHydroBatchTile dsurfHydroBatchTiles[];
};
)glsl") + kTileLookupGlsl + kSupportGlsl + R"glsl(

const float kWaterLevelsM[16] = float[](
    0.0,
    0.0001,
    0.0005,
    0.0010,
    0.0020,
    0.0030,
    0.0040,
    0.0060,
    0.0080,
    0.0100,
    0.0130,
    0.0160,
    0.0200,
    0.0240,
    0.0280,
    0.0320);

uint hash32(uint x)
{
    x ^= x >> 16u;
    x *= 0x7feb352du;
    x ^= x >> 15u;
    x *= 0x846ca68bu;
    x ^= x >> 16u;
    return x;
}

float random01(ivec2 worldCell, uint salt)
{
    uint h = hash32(uint(worldCell.x) * 0x9e3779b9u
        ^ uint(worldCell.y) * 0x85ebca6bu ^ salt * 0xc2b2ae35u);
    return float(h & 0x00ffffffu) * (1.0 / 16777216.0);
}

int q4(float n)
{
    return clamp(int(round(clamp(n, 0.0, 1.0) * 15.0)), 0, 15);
}

float n4(int q)
{
    return float(clamp(q, 0, 15)) * (1.0 / 15.0);
}

float decodeWater(float r)
{
    return kWaterLevelsM[q4(r)];
}

int encodeWaterStochastic(float depthM, ivec2 worldCell, uint tick)
{
    depthM = clamp(depthM, 0.0, 0.032);
    if (depthM <= 0.0)
        return 0;
    for (int i = 0; i < 15; ++i)
    {
        float lo = kWaterLevelsM[i];
        float hi = kWaterLevelsM[i + 1];
        if (depthM <= hi)
        {
            if (hi <= lo + 1.0e-12)
                return i;
            float t = clamp((depthM - lo) / (hi - lo), 0.0, 1.0);
            return random01(worldCell, tick + uint(i) * 17u) < t ? i + 1 : i;
        }
    }
    return 15;
}

vec2 decodeFlow(vec2 encoded)
{
    // Flow direction is static downhill guidance, not a high-frequency CFD
    // velocity. B/A remain directly filterable for presentation.  Fifteen
    // quantization intervals have no exact midpoint, so codes 7 and 8 are both
    // the zero dead-band.  Decoding as `encoded * 2 - 1` made a perfectly flat
    // surface acquire a permanent southeast flow, which swept away rain spots
    // and prevented water from settling where the collision mesh is level.
    ivec2 q = ivec2(round(clamp(encoded, 0.0, 1.0) * 15.0));
    vec2 flow = vec2(0.0);
    flow.x = q.x < 7 ? -float(7 - q.x) / 7.0
        : (q.x > 8 ? float(q.x - 8) / 7.0 : 0.0);
    flow.y = q.y < 7 ? -float(7 - q.y) / 7.0
        : (q.y > 8 ? float(q.y - 8) / 7.0 : 0.0);
    return clamp(flow, vec2(-1.0), vec2(1.0));
}

vec2 encodeFlow(vec2 flow)
{
    flow = clamp(flow, vec2(-1.0), vec2(1.0));
    ivec2 q = ivec2(round((flow * 0.5 + 0.5) * 15.0));
    q = clamp(q, ivec2(0), ivec2(15));
    // (0,0) is reserved for uninitialized. The historical (0,1) code is kept
    // readable so an atlas produced earlier in the same process can recover,
    // but new LIVETRACK07 tiles never permanently kill a texel merely because
    // one exact-geometry probe missed.
    if (q.x == 0 && q.y <= 1)
        q.y = 2;
    return vec2(q) * (1.0 / 15.0);
}

bool invalidState(vec4 state)
{
    return q4(state.b) == 0 && q4(state.a) == 1;
}

bool uninitializedState(vec4 state)
{
    return q4(state.b) == 0 && q4(state.a) == 0;
}

bool stateAtWorld(vec2 globalXZ, out vec4 state)
{
    state = vec4(0.0);
    ivec2 atlasTexel = ivec2(0);
    if (!dsurfAtlasTexelAtGlobal(globalXZ, atlasTexel))
        return false;
    state = texelFetch(uStateAtlas, atlasTexel, 0);
    return !invalidState(state) && !uninitializedState(state);
}

vec2 initializeStaticFlow(vec2 globalXZ, out bool valid)
{
    valid = false;
    vec4 hydro = vec4(0.0);
    uint sheet = 0u;
    bool centerValid = false;
    float hc = dsurfSupportAtGlobal(globalXZ, centerValid, hydro, sheet);
    if (!centerValid)
        return vec2(0.0);

    const float probe = max(uCellSizeM * 2.0, 0.08);
    bool ve = false, vw = false, vn = false, vs = false;
    vec4 ignoredHydro = vec4(0.0);
    uint ignoredSheet = 0u;
    float he = dsurfSupportAtGlobal(globalXZ + vec2(probe, 0.0), ve, ignoredHydro, ignoredSheet);
    float hw = dsurfSupportAtGlobal(globalXZ - vec2(probe, 0.0), vw, ignoredHydro, ignoredSheet);
    float hn = dsurfSupportAtGlobal(globalXZ + vec2(0.0, probe), vn, ignoredHydro, ignoredSheet);
    float hs = dsurfSupportAtGlobal(globalXZ - vec2(0.0, probe), vs, ignoredHydro, ignoredSheet);
    if (!ve) he = hc;
    if (!vw) hw = hc;
    if (!vn) hn = hc;
    if (!vs) hs = hc;

    vec2 gradient = vec2(he - hw, hn - hs) / (2.0 * probe);
    vec2 downhill = -gradient;
    float slope = length(downhill);
    valid = true;
    if (slope < 0.00025)
        return vec2(0.0);
    return normalize(downhill) * clamp(slope * 8.0, 0.0, 1.0);
}

float faceTransfer(float leftDepth, float rightDepth, float signedFlow, float dt)
{
    // Preserve the proven LIVETRACK04 transport rule. The stored B/A field is
    // a bounded downhill guide, so transport removes a stable fraction from
    // the donor without trying to reconstruct terrain height from a quantized
    // direction vector.
    float fraction = clamp(abs(signedFlow) * dt * 0.75, 0.0, 0.18);
    return signedFlow >= 0.0
        ? leftDepth * fraction
        : -rightDepth * fraction;
}

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    uint batchIndex = gl_GlobalInvocationID.z;
    if (any(greaterThanEqual(p, ivec2(256))) || batchIndex >= uBatchTileCount)
        return;

    DsurfHydroBatchTile batchTile = dsurfHydroBatchTiles[batchIndex];
    vec2 globalXZ = vec2(batchTile.worldTile) * 10.0
        + (vec2(p) + vec2(0.5)) * uCellSizeM;
    ivec2 worldCell = batchTile.worldTile * 256 + p;
    vec4 state = texelFetch(uStateAtlas, batchTile.atlasOrigin + p, 0);

    // Recover the former permanent-invalid sentinel. Hydro is an X/Z field;
    // collision support provides its static downhill guide, while the material
    // receiver decides where that state is visible. A transient/missing support
    // lookup must therefore not create a dry hole that can never be revisited.
    if (invalidState(state))
        state = vec4(0.0);

    if (uninitializedState(state))
    {
        // A genuinely new tile begins from the accumulated atmospheric seed;
        // retained history tiles never re-enter this initialization path.
        state.g = 0.0;
        bool supportValid = false;
        vec2 flow = initializeStaticFlow(globalXZ, supportValid);
        // A cell without exact authored support remains a valid, level Hydro
        // cell. This keeps rain coverage continuous across the full resident
        // disk and avoids coupling simulation existence to geometry-atlas
        // warming/order. Exact collision support still supplies slope flow at
        // every cell where it is available.
        if (!supportValid)
            flow = vec2(0.0);
        state.b = encodeFlow(flow).x;
        state.a = encodeFlow(flow).y;
    }

    float dt = max(batchTile.cycleDtSeconds, 0.0);
    float depth = decodeWater(state.r);
    float dryLine = n4(q4(state.g));
    vec2 flow = decodeFlow(state.ba);

    // Restore the proven LIVETRACK04 wetting front: active rain immediately
    // establishes the first representable 0.1mm film on exposed support.
    // Deeper accumulation remains stochastically quantized, so no hidden
    // higher-precision water texture is introduced.
    if (uPrecipitationRateMmPerHour > 0.001)
        depth = max(depth, 0.0001);

    float rainM = max(uPrecipitationRateMmPerHour, 0.0)
        * (0.001 / 3600.0) * dt;
    float lossM = (max(uWeatherDrainageRateMmPerHour, 0.0)
        + max(uEvaporationRateMmPerHour, 0.0))
        * (0.001 / 3600.0) * dt;
    depth = clamp(depth + rainM - lossM, 0.0, 0.032);

    vec4 eState = vec4(0.0), wState = vec4(0.0), nState = vec4(0.0), sState = vec4(0.0);
    bool ve = stateAtWorld(globalXZ + vec2(uCellSizeM, 0.0), eState);
    bool vw = stateAtWorld(globalXZ - vec2(uCellSizeM, 0.0), wState);
    bool vn = stateAtWorld(globalXZ + vec2(0.0, uCellSizeM), nState);
    bool vs = stateAtWorld(globalXZ - vec2(0.0, uCellSizeM), sState);

    float eDepth = ve ? decodeWater(eState.r) : depth;
    float wDepth = vw ? decodeWater(wState.r) : depth;
    float nDepth = vn ? decodeWater(nState.r) : depth;
    float sDepth = vs ? decodeWater(sState.r) : depth;
    vec2 eFlow = ve ? decodeFlow(eState.ba) : flow;
    vec2 wFlow = vw ? decodeFlow(wState.ba) : flow;
    vec2 nFlow = vn ? decodeFlow(nState.ba) : flow;
    vec2 sFlow = vs ? decodeFlow(sState.ba) : flow;

    float eastFlux = faceTransfer(depth, eDepth, 0.5 * (flow.x + eFlow.x), dt);
    float westFlux = faceTransfer(wDepth, depth, 0.5 * (wFlow.x + flow.x), dt);
    float northFlux = faceTransfer(depth, nDepth, 0.5 * (flow.y + nFlow.y), dt);
    float southFlux = faceTransfer(sDepth, depth, 0.5 * (sFlow.y + flow.y), dt);
    depth = clamp(depth - eastFlux + westFlux - northFlux + southFlux, 0.0, 0.032);

    // Rain progressively washes a tire-cleared dry line. Natural decay is very
    // slow so a line remains after rain stops.
    float wash = clamp(uPrecipitationRateMmPerHour * dt / 18000.0, 0.0, 0.08);
    float naturalDecay = clamp(dt / 7200.0, 0.0, 0.01);
    dryLine = clamp(dryLine * (1.0 - wash - naturalDecay), 0.0, 1.0);

    int waterQ = encodeWaterStochastic(depth, worldCell, uTickIndex);
    int dryQ = clamp(int(round(dryLine * 15.0)), 0, 15);
    imageStore(uDestinationBatchScratch, ivec3(p, int(batchIndex)),
        vec4(n4(waterQ), n4(dryQ), state.b, state.a));
}
)glsl";
}

constexpr const char* kWaterBatchScatterShader = R"glsl(
#version 460 core
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(rgba8, binding = 1) readonly uniform image2DArray uBatchScratch;
layout(rgba8, binding = 2) writeonly uniform image2D uDestinationAtlas;
uniform uint uBatchTileCount;
struct DsurfHydroBatchTile
{
    ivec2 worldTile;
    ivec2 atlasOrigin;
    uint worldStateIndex;
    float cycleDtSeconds;
    uint reserved0;
    uint reserved1;
};
layout(std430, binding = 25) readonly buffer DsurfHydroBatchTiles
{
    DsurfHydroBatchTile dsurfHydroBatchTiles[];
};
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    uint batchIndex = gl_GlobalInvocationID.z;
    if (any(greaterThanEqual(p, ivec2(256))) || batchIndex >= uBatchTileCount)
        return;
    ivec2 atlasOrigin = dsurfHydroBatchTiles[batchIndex].atlasOrigin;
    vec4 state = imageLoad(uBatchScratch, ivec3(p, int(batchIndex)));
    imageStore(uDestinationAtlas, atlasOrigin + p, state);
}
)glsl";

constexpr const char* kWorldTileComputeShader = R"glsl(
#version 460 core
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = 24) buffer DsurfWorldTileState
{
    uint states[];
};
layout(std430, binding = 26) readonly buffer DsurfWorldTileCoords
{
    ivec2 worldTiles[];
};
uniform uint uWorldTileCount;
uniform uint uMasterTick;
uniform vec2 uCameraGlobalXZ;
uniform float uPrecipitationRateMmPerHour;
uniform float uWeatherDrainageRateMmPerHour;
uniform float uEvaporationRateMmPerHour;
uniform uint uTickIndex;
const float kWaterLevelsM[16] = float[](
    0.0, 0.0001, 0.0005, 0.0010, 0.0020, 0.0030, 0.0040, 0.0060,
    0.0080, 0.0100, 0.0130, 0.0160, 0.0200, 0.0240, 0.0280, 0.0320);
uint hash32(uint x)
{
    x ^= x >> 16u; x *= 0x7feb352du; x ^= x >> 15u;
    x *= 0x846ca68bu; x ^= x >> 16u; return x;
}
float random01(uint index, uint salt)
{
    return float(hash32(index ^ salt * 0x9e3779b9u) & 0x00ffffffu)
        * (1.0 / 16777216.0);
}
int encodeWaterStochastic(float depthM, uint index, uint tick)
{
    depthM = clamp(depthM, 0.0, 0.032);
    if (depthM <= 0.0) return 0;
    for (int i = 0; i < 15; ++i)
    {
        float lo = kWaterLevelsM[i];
        float hi = kWaterLevelsM[i + 1];
        if (depthM <= hi)
        {
            float f = clamp((depthM - lo) / max(hi - lo, 1.0e-9), 0.0, 1.0);
            return random01(index, tick + uint(i) * 17u) < f ? i + 1 : i;
        }
    }
    return 15;
}
uint cadenceDivisorForDistance(float distanceM)
{
    if (distanceM <= 20.0) return 2u;     // 6Hz from a 12Hz master tick
    if (distanceM <= 40.0) return 3u;     // 4Hz
    if (distanceM <= 60.0) return 6u;     // 2Hz
    if (distanceM <= 80.0) return 12u;    // 1Hz
    if (distanceM <= 100.0) return 24u;   // 0.5Hz
    if (distanceM <= 150.0) return 48u;   // 0.25Hz / 4s
    if (distanceM <= 250.0) return 96u;   // 0.125Hz / 8s
    if (distanceM <= 400.0) return 192u;  // 0.0625Hz / 16s
    if (distanceM <= 600.0) return 384u;  // 0.03125Hz / 32s
    if (distanceM <= 800.0) return 768u;  // 0.015625Hz / 64s
    if (distanceM <= 1000.0) return 1536u;// 0.0078125Hz / 128s
    return 0u;
}

void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= uWorldTileCount) return;

    ivec2 tile = worldTiles[index];
    vec2 tileCenter = (vec2(tile) + vec2(0.5)) * 10.0;
    float distanceM = length(tileCenter - uCameraGlobalXZ);
    uint divisor = cadenceDivisorForDistance(distanceM);
    // Beyond 1000m the allocation seed is the requested single update/state;
    // no recurring work is performed until the player comes within 1000m.
    if (divisor == 0u || (uMasterTick % divisor) != 0u) return;

    uint packed = states[index];
    int waterQ = int((packed & 0xffu) / 17u);
    int dryQ = int(((packed >> 8u) & 0xffu) / 17u);
    float depth = kWaterLevelsM[clamp(waterQ, 0, 15)];
    float dryLine = float(clamp(dryQ, 0, 15)) * (1.0 / 15.0);
    float dt = float(divisor) * (1.0 / 12.0);
    depth = clamp(depth
        + max(uPrecipitationRateMmPerHour, 0.0) * (0.001 / 3600.0) * dt
        - (max(uWeatherDrainageRateMmPerHour, 0.0)
            + max(uEvaporationRateMmPerHour, 0.0)) * (0.001 / 3600.0) * dt,
        0.0, 0.032);
    float wash = clamp(uPrecipitationRateMmPerHour * dt / 18000.0, 0.0, 0.25);
    float naturalDecay = clamp(dt / 7200.0, 0.0, 0.02);
    dryLine = clamp(dryLine * (1.0 - wash - naturalDecay), 0.0, 1.0);
    waterQ = encodeWaterStochastic(depth, index, uTickIndex);
    dryQ = clamp(int(round(dryLine * 15.0)), 0, 15);
    uint flowBytes = packed & 0xffff0000u;
    states[index] = flowBytes | uint(waterQ * 17) | (uint(dryQ * 17) << 8u);
}
)glsl";

constexpr const char* kWorldTireComputeShader = R"glsl(
#version 460 core
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = 24) buffer DsurfWorldTileState
{
    uint states[];
};
struct WorldTireEvent
{
    uint stateIndex;
    float strength;
    uint reserved0;
    uint reserved1;
};
layout(std430, binding = 25) readonly buffer DsurfWorldTireEvents
{
    WorldTireEvent events[];
};
uniform uint uEventCount;
void main()
{
    uint eventIndex = gl_GlobalInvocationID.x;
    if (eventIndex >= uEventCount) return;
    WorldTireEvent e = events[eventIndex];
    uint expected = states[e.stateIndex];
    for (int retry = 0; retry < 16; ++retry)
    {
        int waterQ = int((expected & 0xffu) / 17u);
        int dryQ = int(((expected >> 8u) & 0xffu) / 17u);
        float s = clamp(e.strength, 0.0, 1.0);
        waterQ = max(0, waterQ - int(round(s * 3.0)));
        dryQ = min(15, dryQ + max(1, int(round(s * 4.0))));
        uint desired = (expected & 0xffff0000u)
            | uint(waterQ * 17) | (uint(dryQ * 17) << 8u);
        uint observed = atomicCompSwap(states[e.stateIndex], expected, desired);
        if (observed == expected) break;
        expected = observed;
    }
}
)glsl";

std::string makeSnowShader()
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

std::string makeMudShader()
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
    float depthM = decodeWater(water.r);
    float dryLine = n4(q4(water.g));
    float removal = clamp(strength * profile, 0.0, 0.92);
    depthM *= (1.0 - removal);
    dryLine = clamp(dryLine + profile * strength * 0.28, 0.0, 1.0);
    imageStore(uWater, p,
        vec4(n4(encodeWater(depthM)), n4(int(round(dryLine * 15.0))), water.b, water.a));

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

bool checkNoGlError(std::string& errorMessage, const char* operation)
{
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR)
        return true;
    errorMessage = std::string("OpenGL error during ") + operation
        + ": " + std::to_string(static_cast<unsigned int>(error));
    return false;
}

} // namespace

std::uint64_t DynamicSurfaceGpuLodPrototype::tileKey(std::int32_t x, std::int32_t z)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32u)
        | static_cast<std::uint32_t>(z);
}

bool DynamicSurfaceGpuLodPrototype::worldStateIndexForTile(
    std::int32_t tileX, std::int32_t tileZ, std::uint32_t& index) const
{
    auto floorDiv10 = [](std::int32_t value, std::int32_t& chunk, std::int32_t& local) {
        chunk = value / 10;
        local = value % 10;
        if (local < 0)
        {
            --chunk;
            local += 10;
        }
    };
    std::int32_t chunkX = 0, chunkZ = 0, localX = 0, localZ = 0;
    floorDiv10(tileX, chunkX, localX);
    floorDiv10(tileZ, chunkZ, localZ);
    const auto found = m_worldChunkBaseIndices.find(tileKey(chunkX, chunkZ));
    if (found == m_worldChunkBaseIndices.end())
        return false;
    index = found->second + static_cast<std::uint32_t>(localZ * 10 + localX);
    return index < m_worldTileCount;
}

float DynamicSurfaceGpuLodPrototype::cadenceHzForDistance(float distanceM, bool prewarm)
{
    if (prewarm)
        return 1.0f / 60.0f;
    // The complete visible field advances as one coherent Jacobi generation.
    // Distance/frustum visibility must not make an uphill or farther part of
    // the same wet road develop rain several seconds after the foreground.
    if (distanceM <= kSimulationRadiusM) return 2.0f;
    return 0.0f;
}

std::uint8_t DynamicSurfaceGpuLodPrototype::cadenceBandForDistance(float distanceM, bool prewarm)
{
    if (prewarm)
        return 3u;
    return distanceM <= kSimulationRadiusM ? 0u : 3u;
}

float DynamicSurfaceGpuLodPrototype::cadencePeriodForBand(std::uint8_t band)
{
    switch (band)
    {
    case 0u: return 1.0f / 6.0f;
    case 1u: return 1.0f / 4.0f;
    case 2u: return 1.0f / 2.0f;
    default: return 1.0f;
    }
}

std::array<std::uint32_t, 2> DynamicSurfaceGpuLodPrototype::atlasSlotOrigin(std::uint16_t slot)
{
    return {
        (static_cast<std::uint32_t>(slot) % kAtlasColumns) * kTileResolution,
        (static_cast<std::uint32_t>(slot) / kAtlasColumns) * kTileResolution };
}

std::uint32_t DynamicSurfaceGpuLodPrototype::packWaterSeed(float depthM)
{
    static constexpr std::array<float, 16> kLevels{{
        0.0f, 0.0001f, 0.0005f, 0.0010f, 0.0020f, 0.0030f, 0.0040f, 0.0060f,
        0.0080f, 0.0100f, 0.0130f, 0.0160f, 0.0200f, 0.0240f, 0.0280f, 0.0320f }};
    std::uint8_t best = 0u;
    float bestError = std::abs(depthM - kLevels[0]);
    for (std::uint8_t i = 1u; i < kLevels.size(); ++i)
    {
        const float error = std::abs(depthM - kLevels[i]);
        if (error < bestError)
        {
            best = i;
            bestError = error;
        }
    }
    // RGBA8 byte layout: R water, G dry-line, B/A = 0/0 initialization sentinel.
    return static_cast<std::uint32_t>(best * 17u);
}

bool DynamicSurfaceGpuLodPrototype::initialize(std::string& errorMessage)
{
    shutdown();
    errorMessage.clear();

    GLint maximumTextureSize = 0;
    GLint maximumArrayLayers = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maximumArrayLayers);
    if (maximumTextureSize < static_cast<GLint>(kAtlasWidth)
        || maximumTextureSize < static_cast<GLint>(kAtlasHeight))
    {
        errorMessage = "GPU cannot satisfy LIVETRACK07 5120x5120 10m-tile Hydro atlas.";
        return false;
    }
    if (maximumArrayLayers < static_cast<GLint>(kMaximumBatchTiles))
    {
        errorMessage = "GPU cannot satisfy LIVETRACK07 384-layer batched Hydro scratch array.";
        return false;
    }

    while (glGetError() != GL_NO_ERROR) {}

    m_waterProgram = heritage::graphics::buildComputeShaderProgram(makeWaterShader().c_str());
    m_waterBatchScatterProgram = heritage::graphics::buildComputeShaderProgram(kWaterBatchScatterShader);
    // The former all-scene world shaders were never dispatched by LIVETRACK06.
    // Do not compile or allocate their duplicate state in the live path.
    m_worldTileProgram = 0;
    m_worldTireProgram = 0;
    m_waterPresentationProgram = 0;
    m_snowProgram = heritage::graphics::buildComputeShaderProgram(makeSnowShader().c_str());
    m_mudProgram = heritage::graphics::buildComputeShaderProgram(makeMudShader().c_str());
    m_tireEventProgram = heritage::graphics::buildComputeShaderProgram(kTireEventComputeShader);
    if (!m_waterProgram || !m_waterBatchScatterProgram
        || !m_snowProgram || !m_mudProgram || !m_tireEventProgram)
    {
        errorMessage = "LIVETRACK04C GPU Hydro compute shader compilation/link failed.";
        shutdown();
        return false;
    }

    const auto cacheUniforms = [](GLuint program, ProgramUniforms& u) {
        u.worldTile = glGetUniformLocation(program, "uWorldTile");
        u.atlasOrigin = glGetUniformLocation(program, "uAtlasOrigin");
        u.tileMapOrigin = glGetUniformLocation(program, "uTileMapOrigin");
        u.geometryCenterChunk = glGetUniformLocation(program, "uGeometryCenterChunk");
        u.cellSizeM = glGetUniformLocation(program, "uCellSizeM");
        u.cycleDtSeconds = glGetUniformLocation(program, "uCycleDtSeconds");
        u.precipitationRateMmPerHour = glGetUniformLocation(program, "uPrecipitationRateMmPerHour");
        u.weatherDrainageRateMmPerHour = glGetUniformLocation(program, "uWeatherDrainageRateMmPerHour");
        u.evaporationRateMmPerHour = glGetUniformLocation(program, "uEvaporationRateMmPerHour");
        u.ambientTemperatureC = glGetUniformLocation(program, "uAmbientTemperatureC");
        u.tickIndex = glGetUniformLocation(program, "uTickIndex");
        u.worldStateIndex = glGetUniformLocation(program, "uWorldStateIndex");
        u.stateAtlas = glGetUniformLocation(program, "uStateAtlas");
        u.tileMap = glGetUniformLocation(program, "uTileMap");
        u.batchTileCount = glGetUniformLocation(program, "uBatchTileCount");
    };
    cacheUniforms(m_waterProgram, m_waterUniforms);
    cacheUniforms(m_snowProgram, m_snowUniforms);
    cacheUniforms(m_mudProgram, m_mudUniforms);
    m_waterBatchScatterCountLocation = glGetUniformLocation(
        m_waterBatchScatterProgram, "uBatchTileCount");

    m_waterPresentationUniforms = {};

    m_tireEventUniforms.atlasOrigin = glGetUniformLocation(m_tireEventProgram, "uAtlasOrigin");
    m_tireEventUniforms.minTexel = glGetUniformLocation(m_tireEventProgram, "uMinTexel");
    m_tireEventUniforms.extentTexels = glGetUniformLocation(m_tireEventProgram, "uExtentTexels");
    m_tireEventUniforms.cellSizeM = glGetUniformLocation(m_tireEventProgram, "uCellSizeM");
    m_tireEventUniforms.eventLocalXZ = glGetUniformLocation(m_tireEventProgram, "uEventLocalXZ");
    m_tireEventUniforms.forwardXZ = glGetUniformLocation(m_tireEventProgram, "uForwardXZ");
    m_tireEventUniforms.rightXZ = glGetUniformLocation(m_tireEventProgram, "uRightXZ");
    m_tireEventUniforms.patchHalfLengthM = glGetUniformLocation(m_tireEventProgram, "uPatchHalfLengthM");
    m_tireEventUniforms.patchHalfWidthM = glGetUniformLocation(m_tireEventProgram, "uPatchHalfWidthM");
    m_tireEventUniforms.normalLoadN = glGetUniformLocation(m_tireEventProgram, "uNormalLoadN");
    m_tireEventUniforms.speedMps = glGetUniformLocation(m_tireEventProgram, "uSpeedMps");
    m_tireEventUniforms.accumulatedDtSeconds = glGetUniformLocation(m_tireEventProgram, "uAccumulatedDtSeconds");
    m_tireEventUniforms.snowReady = glGetUniformLocation(m_tireEventProgram, "uSnowReady");
    m_tireEventUniforms.mudReady = glGetUniformLocation(m_tireEventProgram, "uMudReady");
    m_tireEventUniforms.mudDeformable = glGetUniformLocation(m_tireEventProgram, "uMudDeformable");

    if (!allocateState(m_water, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,
            4u, errorMessage))
    {
        shutdown();
        return false;
    }

    // LIVETRACK04D: one layered scratch target for the entire <=100m cohort.
    // The first dispatch writes every due tile into one layer; a second single
    // dispatch scatters all layers back into their atlas slots. There are no
    // per-tile compute dispatches, copies, or memory barriers in the water path.
    glGenTextures(1, &m_waterBatchScratch);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_waterBatchScratch);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8,
        static_cast<GLsizei>(kTileResolution),
        static_cast<GLsizei>(kTileResolution),
        static_cast<GLsizei>(kMaximumBatchTiles));
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenBuffers(1, &m_waterBatchTileBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_waterBatchTileBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(sizeof(GpuHydroBatchTile) * kMaximumBatchTiles),
        nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    if (!m_waterBatchScratch || !m_waterBatchTileBuffer
        || !checkNoGlError(errorMessage, "LIVETRACK04D batched Hydro scratch allocation"))
    {
        shutdown();
        return false;
    }
    m_stats.committedMiB += static_cast<double>(kTileResolution)
        * static_cast<double>(kTileResolution)
        * static_cast<double>(kMaximumBatchTiles) * 4.0 / (1024.0 * 1024.0);
    m_waterBatchTileScratch.clear();
    m_waterBatchTileScratch.reserve(kMaximumBatchTiles);

    // LIVETRACK04 renders the authoritative filterable water atlas directly.
    m_waterPresentationAtlas = 0;
    m_stats.waterPresentationMiB = 0.0;

    glGenTextures(1, &m_tileIndirectionTexture);
    glBindTexture(GL_TEXTURE_2D, m_tileIndirectionTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI,
        static_cast<GLsizei>(kTileMapResolution), static_cast<GLsizei>(kTileMapResolution));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_tileIndirectionScratch.assign(
        static_cast<std::size_t>(kTileMapResolution) * kTileMapResolution, 0u);

    m_freeSlots.reserve(kMaximumTileSlots);
    for (std::uint32_t slot = 0; slot < kMaximumTileSlots; ++slot)
        m_freeSlots.push_back(static_cast<std::uint16_t>(kMaximumTileSlots - 1u - slot));

    glGenQueries(static_cast<GLsizei>(m_gpuTimerStartQueries.size()),
        m_gpuTimerStartQueries.data());
    glGenQueries(static_cast<GLsizei>(m_gpuTimerEndQueries.size()),
        m_gpuTimerEndQueries.data());

    if (!m_tileIndirectionTexture
        || !checkNoGlError(errorMessage, "LIVETRACK04 GPU Hydro allocation"))
    {
        shutdown();
        return false;
    }

    m_stats.ready = true;
    m_stats.waterReady = true;
    m_stats.waterPresentationReady = true;
    m_stats.authoritative = true;
    m_lastElapsedSeconds = -1.0;
    return true;
}

bool DynamicSurfaceGpuLodPrototype::allocateState(
    StateRuntime& state,
    GLenum internalFormat,
    GLenum clearFormat,
    GLenum clearType,
    std::size_t bytesPerTexel,
    std::string& errorMessage)
{
    destroyState(state);
    state.internalFormat = internalFormat;
    state.clearFormat = clearFormat;
    state.clearType = clearType;
    state.bytesPerTexel = bytesPerTexel;

    glGenTextures(1, &state.atlas);
    glBindTexture(GL_TEXTURE_2D, state.atlas);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat,
        static_cast<GLsizei>(kAtlasWidth), static_cast<GLsizei>(kAtlasHeight));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (internalFormat == GL_RGBA8)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glGenTextures(1, &state.scratch);
    glBindTexture(GL_TEXTURE_2D, state.scratch);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat,
        static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (!state.atlas || !state.scratch || !checkNoGlError(errorMessage, "state atlas allocation"))
    {
        destroyState(state);
        return false;
    }
    state.allocated = true;
    const double bytes = static_cast<double>(kAtlasWidth)
        * static_cast<double>(kAtlasHeight) * static_cast<double>(bytesPerTexel)
        + static_cast<double>(kTileResolution) * static_cast<double>(kTileResolution)
            * static_cast<double>(bytesPerTexel);
    m_stats.committedMiB += bytes / (1024.0 * 1024.0);
    return true;
}

bool DynamicSurfaceGpuLodPrototype::ensureSnowState(std::string& errorMessage)
{
    if (m_snow.allocated)
        return true;
    if (!allocateState(m_snow, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT,
            sizeof(std::uint16_t), errorMessage))
        return false;
    for (const auto& [key, tile] : m_tiles)
    {
        (void)key;
        clearStateSlot(m_snow, tile.slot, 0u);
    }
    m_stats.snowReady = true;
    return true;
}

bool DynamicSurfaceGpuLodPrototype::ensureMudState(std::string& errorMessage)
{
    if (m_mud.allocated)
        return true;
    if (!allocateState(m_mud, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
            sizeof(std::uint8_t), errorMessage))
        return false;
    for (const auto& [key, tile] : m_tiles)
    {
        (void)key;
        clearStateSlot(m_mud, tile.slot, 0u);
    }
    m_stats.mudReady = true;
    return true;
}

void DynamicSurfaceGpuLodPrototype::destroyState(StateRuntime& state)
{
    if (state.scratch)
        glDeleteTextures(1, &state.scratch);
    if (state.atlas)
        glDeleteTextures(1, &state.atlas);
    state = {};
}

void DynamicSurfaceGpuLodPrototype::clearStateSlot(
    StateRuntime& state,
    std::uint16_t slot,
    std::uint32_t clearValue)
{
    if (!state.allocated)
        return;
    const auto origin = atlasSlotOrigin(slot);
    if (state.internalFormat == GL_RGBA8)
    {
        const std::array<std::uint8_t, 4> rgba{{
            static_cast<std::uint8_t>(clearValue & 0xffu),
            static_cast<std::uint8_t>((clearValue >> 8u) & 0xffu),
            static_cast<std::uint8_t>((clearValue >> 16u) & 0xffu),
            static_cast<std::uint8_t>((clearValue >> 24u) & 0xffu) }};
        glClearTexSubImage(state.atlas, 0,
            static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]), 0,
            static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution), 1,
            GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    }
    else
    {
        glClearTexSubImage(state.atlas, 0,
            static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]), 0,
            static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution), 1,
            state.clearFormat, state.clearType, &clearValue);
    }
}

void DynamicSurfaceGpuLodPrototype::clearWaterPresentationSlot(std::uint16_t slot)
{
    (void)slot;
}

void DynamicSurfaceGpuLodPrototype::shutdown()
{
    destroyWorldTileSimulation();
    destroyExactGeometryAtlas();
    if (m_waterPresentationAtlas)
        glDeleteTextures(1, &m_waterPresentationAtlas);
    m_waterPresentationAtlas = 0;
    if (m_tileIndirectionTexture)
        glDeleteTextures(1, &m_tileIndirectionTexture);
    m_tileIndirectionTexture = 0;
    if (m_waterBatchTileBuffer)
        glDeleteBuffers(1, &m_waterBatchTileBuffer);
    m_waterBatchTileBuffer = 0;
    if (m_waterBatchScratch)
        glDeleteTextures(1, &m_waterBatchScratch);
    m_waterBatchScratch = 0;
    destroyState(m_mud);
    destroyState(m_snow);
    destroyState(m_water);

    if (m_gpuTimerEndQueries[0])
        glDeleteQueries(static_cast<GLsizei>(m_gpuTimerEndQueries.size()), m_gpuTimerEndQueries.data());
    if (m_gpuTimerStartQueries[0])
        glDeleteQueries(static_cast<GLsizei>(m_gpuTimerStartQueries.size()), m_gpuTimerStartQueries.data());
    m_gpuTimerStartQueries.fill(0u);
    m_gpuTimerEndQueries.fill(0u);
    m_gpuTimerPending.fill(false);

    if (m_tireEventProgram) glDeleteProgram(m_tireEventProgram);
    if (m_worldTireProgram) glDeleteProgram(m_worldTireProgram);
    if (m_worldTileProgram) glDeleteProgram(m_worldTileProgram);
    if (m_waterPresentationProgram) glDeleteProgram(m_waterPresentationProgram);
    if (m_mudProgram) glDeleteProgram(m_mudProgram);
    if (m_snowProgram) glDeleteProgram(m_snowProgram);
    if (m_waterBatchScatterProgram) glDeleteProgram(m_waterBatchScatterProgram);
    if (m_waterProgram) glDeleteProgram(m_waterProgram);
    m_waterProgram = m_worldTileProgram = m_worldTireProgram = 0;
    m_waterBatchScatterProgram = 0;
    m_waterPresentationProgram = m_snowProgram = m_mudProgram = m_tireEventProgram = 0;
    m_waterBatchScatterCountLocation = -1;
    m_waterUniforms = {};
    m_snowUniforms = {};
    m_mudUniforms = {};
    m_tireEventUniforms = {};
    m_waterBatchTileScratch.clear();

    m_tiles.clear();
    m_freeSlots.clear();
    m_tileIndirectionScratch.clear();
    m_centerTileValid = false;
    m_geometryAtlasReady = false;
    m_lastElapsedSeconds = -1.0;
    m_nextHighResolutionBatchDueSeconds = 0.0;
    m_lastCameraSampleSeconds = -1.0;
    m_nextWaterDiagnosticsSeconds = 0.0;
    m_stats = {};
}

std::uint16_t DynamicSurfaceGpuLodPrototype::allocateTileSlot()
{
    if (m_freeSlots.empty())
        return std::numeric_limits<std::uint16_t>::max();
    const std::uint16_t slot = m_freeSlots.back();
    m_freeSlots.pop_back();
    return slot;
}

void DynamicSurfaceGpuLodPrototype::releaseTileSlot(std::uint16_t slot)
{
    m_freeSlots.push_back(slot);
}

void DynamicSurfaceGpuLodPrototype::destroyWorldTileSimulation()
{
    if (m_stats.worldTileStateMiB > 0.0)
        m_stats.committedMiB = std::max(0.0, m_stats.committedMiB - m_stats.worldTileStateMiB);
    if (m_worldTireEventBuffer)
        glDeleteBuffers(1, &m_worldTireEventBuffer);
    if (m_worldTileCoordBuffer)
        glDeleteBuffers(1, &m_worldTileCoordBuffer);
    if (m_worldTileStateBuffer)
        glDeleteBuffers(1, &m_worldTileStateBuffer);
    m_worldTireEventBuffer = 0;
    m_worldTileCoordBuffer = 0;
    m_worldTileStateBuffer = 0;
    m_worldChunkBaseIndices.clear();
    m_worldLayoutFingerprint = 0u;
    m_worldTileCount = 0u;
    m_worldCatalogReady = false;
    m_worldCohortCursor = 0u;
    m_nextWorldTileDueSeconds = 0.0;
    m_stats.worldTiles = 0u;
    m_stats.worldTileStateMiB = 0.0;
}

bool DynamicSurfaceGpuLodPrototype::ensureWorldTileSimulation(
    const heritage::physics::dynamicsurface::DynamicSurfaceSystem* dynamicSurface,
    float backgroundSeedDepthM,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (!dynamicSurface)
        return true;
    // LIVETRACK07: this is a CPU-only authored-coverage catalog. It intentionally
    // owns no duplicate water state or GPU buffers.
    if (m_worldCatalogReady)
        return true;

    const auto chunks = dynamicSurface->chunkAddresses();
    std::uint64_t fingerprint = 1469598103934665603ull;
    for (const auto& address : chunks)
    {
        fingerprint ^= static_cast<std::uint64_t>(address.x);
        fingerprint *= 1099511628211ull;
        fingerprint ^= static_cast<std::uint64_t>(address.z);
        fingerprint *= 1099511628211ull;
    }
    fingerprint ^= static_cast<std::uint64_t>(chunks.size());
    fingerprint *= 1099511628211ull;

    destroyWorldTileSimulation();
    m_worldCatalogReady = true;
    m_worldLayoutFingerprint = fingerprint;
    if (chunks.empty())
        return true;

    m_worldChunkBaseIndices.reserve(chunks.size());
    (void)backgroundSeedDepthM;

    for (const auto& chunk : chunks)
    {
        const std::int64_t baseTileX64 = chunk.x * 10ll;
        const std::int64_t baseTileZ64 = chunk.z * 10ll;
        if (baseTileX64 < std::numeric_limits<std::int32_t>::min()
            || baseTileX64 + 9ll > std::numeric_limits<std::int32_t>::max()
            || baseTileZ64 < std::numeric_limits<std::int32_t>::min()
            || baseTileZ64 + 9ll > std::numeric_limits<std::int32_t>::max())
        {
            continue;
        }
        const std::uint32_t baseIndex = m_worldTileCount;
        m_worldChunkBaseIndices.emplace(
            tileKey(static_cast<std::int32_t>(chunk.x), static_cast<std::int32_t>(chunk.z)),
            baseIndex);
        m_worldTileCount += 100u;
    }

    m_worldCohortCursor = 0u;
    m_nextWorldTileDueSeconds = 0.0;
    m_stats.worldTiles = m_worldTileCount;
    m_stats.worldTileStateMiB = 0.0;
    return true;
}

void DynamicSurfaceGpuLodPrototype::dispatchWorldTileSimulation(
    double elapsedSeconds,
    float precipitationRateMmPerHour,
    float weatherDrainageRateMmPerHour,
    float evaporationRateMmPerHour)
{
    if (!m_worldTileProgram || !m_worldTileStateBuffer || !m_worldTileCoordBuffer
        || m_worldTileCount == 0u)
    {
        return;
    }

    const double masterPeriod = 1.0 / static_cast<double>(kWorldDispatchHz);
    if (m_nextWorldTileDueSeconds <= 0.0)
    {
        m_nextWorldTileDueSeconds = elapsedSeconds;
    }
    if (elapsedSeconds + 1.0e-9 < m_nextWorldTileDueSeconds)
        return;

    // One tiny all-world dispatch at a 12Hz master rate. Every tile computes
    // its distance band on the GPU and returns immediately unless its own
    // cadence is due. Camera/frustum never determines whether world state
    // exists; it only changes how often that state advances.
    const std::uint32_t masterTick = static_cast<std::uint32_t>(
        std::floor(std::max(elapsedSeconds, 0.0) * kWorldDispatchHz));
    glUseProgram(m_worldTileProgram);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 24, m_worldTileStateBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 26, m_worldTileCoordBuffer);
    const GLint countLoc = glGetUniformLocation(m_worldTileProgram, "uWorldTileCount");
    const GLint tickLoc = glGetUniformLocation(m_worldTileProgram, "uMasterTick");
    const GLint cameraLoc = glGetUniformLocation(m_worldTileProgram, "uCameraGlobalXZ");
    const GLint rainLoc = glGetUniformLocation(m_worldTileProgram, "uPrecipitationRateMmPerHour");
    const GLint drainLoc = glGetUniformLocation(m_worldTileProgram, "uWeatherDrainageRateMmPerHour");
    const GLint evapLoc = glGetUniformLocation(m_worldTileProgram, "uEvaporationRateMmPerHour");
    const GLint tickIndexLoc = glGetUniformLocation(m_worldTileProgram, "uTickIndex");
    if (countLoc >= 0) glUniform1ui(countLoc, m_worldTileCount);
    if (tickLoc >= 0) glUniform1ui(tickLoc, masterTick);
    if (cameraLoc >= 0) glUniform2f(cameraLoc,
        static_cast<float>(m_lastCameraGlobalX), static_cast<float>(m_lastCameraGlobalZ));
    if (rainLoc >= 0) glUniform1f(rainLoc, precipitationRateMmPerHour);
    if (drainLoc >= 0) glUniform1f(drainLoc, weatherDrainageRateMmPerHour);
    if (evapLoc >= 0) glUniform1f(evapLoc, evaporationRateMmPerHour);
    if (tickIndexLoc >= 0) glUniform1ui(tickIndexLoc, masterTick);
    glDispatchCompute((m_worldTileCount + 255u) / 256u, 1u, 1u);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    ++m_stats.worldTileDispatches;
    ++m_stats.dispatchesThisFrame;
    m_stats.cellsThisFrame += m_worldTileCount;

    m_nextWorldTileDueSeconds = elapsedSeconds + masterPeriod;
}

void DynamicSurfaceGpuLodPrototype::applyWorldTireEvents(
    const std::vector<DynamicSurfaceGpuTireContactEvent>& events)
{
    if (!m_worldTireProgram || !m_worldTileStateBuffer || events.empty())
        return;

    struct alignas(16) GpuWorldTireEvent
    {
        std::uint32_t stateIndex = 0u;
        float strength = 0.0f;
        std::uint32_t reserved0 = 0u;
        std::uint32_t reserved1 = 0u;
    };

    std::unordered_map<std::uint32_t, float> strengths;
    strengths.reserve(events.size());
    for (const auto& event : events)
    {
        const std::int32_t tileX = static_cast<std::int32_t>(std::floor(event.globalX / kTileWorldSizeM));
        const std::int32_t tileZ = static_cast<std::int32_t>(std::floor(event.globalZ / kTileWorldSizeM));
        std::uint32_t worldStateIndex = 0u;
        if (!worldStateIndexForTile(tileX, tileZ, worldStateIndex))
            continue;
        const float timeStrength = std::clamp(event.accumulatedDtSeconds * 60.0f, 0.05f, 1.0f);
        const float loadStrength = std::clamp(event.normalLoadN / 5000.0f, 0.15f, 1.5f);
        const float speedStrength = std::clamp(0.35f + event.speedMps * 0.035f, 0.35f, 1.4f);
        const float strength = std::clamp(timeStrength * loadStrength * speedStrength, 0.0f, 1.0f);
        auto& slot = strengths[worldStateIndex];
        slot = std::max(slot, strength);
    }
    if (strengths.empty())
        return;

    std::vector<GpuWorldTireEvent> gpuEvents;
    gpuEvents.reserve(strengths.size());
    for (const auto& [index, strength] : strengths)
        gpuEvents.push_back({ index, strength, 0u, 0u });

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_worldTireEventBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(gpuEvents.size() * sizeof(GpuWorldTireEvent)),
        gpuEvents.data(), GL_STREAM_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 24, m_worldTileStateBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 25, m_worldTireEventBuffer);
    glUseProgram(m_worldTireProgram);
    const GLint countLoc = glGetUniformLocation(m_worldTireProgram, "uEventCount");
    if (countLoc >= 0) glUniform1ui(countLoc, static_cast<GLuint>(gpuEvents.size()));
    glDispatchCompute(static_cast<GLuint>((gpuEvents.size() + 63u) / 64u), 1u, 1u);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void DynamicSurfaceGpuLodPrototype::refreshResidency(
    double elapsedSeconds,
    double cameraGlobalX,
    double cameraGlobalZ,
    float backgroundSeedDepthM)
{
    const std::int32_t newCenterX = static_cast<std::int32_t>(
        std::floor(cameraGlobalX / kTileWorldSizeM));
    const std::int32_t newCenterZ = static_cast<std::int32_t>(
        std::floor(cameraGlobalZ / kTileWorldSizeM));
    if (!m_centerTileValid || newCenterX != m_centerTileX || newCenterZ != m_centerTileZ)
    {
        if (m_centerTileValid)
            ++m_stats.cameraTileRebases;
        m_centerTileX = newCenterX;
        m_centerTileZ = newCenterZ;
        m_centerTileValid = true;
    }

    struct Desired
    {
        std::int32_t x = 0;
        std::int32_t z = 0;
        float distanceM = 0.0f;
    };
    std::unordered_map<std::uint64_t, Desired> nearTiles;
    nearTiles.reserve(512u);
    const int tileRadius = static_cast<int>(std::ceil(kSimulationRadiusM / kTileWorldSizeM));
    for (int dz = -tileRadius; dz <= tileRadius; ++dz)
    {
        for (int dx = -tileRadius; dx <= tileRadius; ++dx)
        {
            // Distance to the nearest point of the 10m tile, not merely its
            // center, so the complete 100m disk is resident without a square
            // corner tax.
            const float centerDx = float(dx) * kTileWorldSizeM;
            const float centerDz = float(dz) * kTileWorldSizeM;
            const float nearestDx = std::max(std::abs(centerDx) - kTileWorldSizeM * 0.5f, 0.0f);
            const float nearestDz = std::max(std::abs(centerDz) - kTileWorldSizeM * 0.5f, 0.0f);
            const float distanceM = std::hypot(nearestDx, nearestDz);
            if (distanceM > kSimulationRadiusM)
                continue;
            Desired item;
            item.x = m_centerTileX + dx;
            item.z = m_centerTileZ + dz;
            item.distanceM = distanceM;
            nearTiles.emplace(tileKey(item.x, item.z), item);
        }
    }

    // Keep exact recently visited water state when a tile leaves the 100m
    // presentation disk. The bounded 400-slot pool retains up to 43 history
    // tiles in addition to the worst-case 357-tile near disk, evicting only
    // the oldest distant history when a new near tile needs its slot.
    bool residencyChanged = false;
    for (auto& [key, tile] : m_tiles)
    {
        if (nearTiles.find(key) != nearTiles.end())
        {
            if (tile.prewarm)
                tile.nextDueSeconds = elapsedSeconds;
            tile.prewarm = false;
        }
        else
        {
            tile.cadenceBand = 3u;
            tile.prewarm = true;
        }
    }

    for (const auto& [key, item] : nearTiles)
    {
        auto found = m_tiles.find(key);
        if (found != m_tiles.end())
            continue;

        if (m_freeSlots.empty())
        {
            auto victim = m_tiles.end();
            for (auto it = m_tiles.begin(); it != m_tiles.end(); ++it)
            {
                if (!it->second.prewarm)
                    continue;
                if (victim == m_tiles.end()
                    || it->second.lastUpdateSeconds < victim->second.lastUpdateSeconds)
                {
                    victim = it;
                }
            }
            if (victim != m_tiles.end())
            {
                releaseTileSlot(victim->second.slot);
                m_tiles.erase(victim);
                residencyChanged = true;
            }
        }

        const std::uint16_t slot = allocateTileSlot();
        if (slot == std::numeric_limits<std::uint16_t>::max())
            continue;

        TileRuntime tile;
        tile.x = item.x;
        tile.z = item.z;
        tile.slot = slot;
        tile.cadenceBand = 0u;
        tile.prewarm = false;
        // New near tiles initialize immediately. The per-texel static downhill
        // direction is generated once by the first GPU update and then stored
        // in B/A; later 6Hz updates no longer walk collision triangles.
        tile.nextDueSeconds = elapsedSeconds;
        tile.lastUpdateSeconds = elapsedSeconds;
        m_tiles.emplace(key, tile);

        clearStateSlot(m_water, slot, packWaterSeed(backgroundSeedDepthM));
        if (m_snow.allocated) clearStateSlot(m_snow, slot, 0u);
        if (m_mud.allocated) clearStateSlot(m_mud, slot, 0u);
        residencyChanged = true;
    }

    m_stats.activeTiles.fill(0u);
    m_stats.prewarmTiles = 0u;
    for (auto& [key, tile] : m_tiles)
    {
        (void)key;
        if (tile.prewarm)
        {
            ++m_stats.prewarmTiles;
            continue;
        }
        const float centerDx = static_cast<float>(tile.x - m_centerTileX) * kTileWorldSizeM;
        const float centerDz = static_cast<float>(tile.z - m_centerTileZ) * kTileWorldSizeM;
        const float nearestDx = std::max(std::abs(centerDx) - kTileWorldSizeM * 0.5f, 0.0f);
        const float nearestDz = std::max(std::abs(centerDz) - kTileWorldSizeM * 0.5f, 0.0f);
        tile.cadenceBand = cadenceBandForDistance(
            std::hypot(nearestDx, nearestDz), false);
        ++m_stats.activeTiles[std::min<std::size_t>(tile.cadenceBand, 3u)];
    }
    m_stats.residentTiles = static_cast<std::uint32_t>(m_tiles.size());
    m_stats.cameraSpeedMps = std::hypot(m_cameraVelocityX, m_cameraVelocityZ);
    m_stats.predictivePrewarmM = 0.0;

    if (residencyChanged || m_tileMapOriginX != m_centerTileX - kTileMapHalfSpan
        || m_tileMapOriginZ != m_centerTileZ - kTileMapHalfSpan)
    {
        rebuildTileIndirection();
    }
}

void DynamicSurfaceGpuLodPrototype::rebuildTileIndirection()
{
    m_tileMapOriginX = m_centerTileX - kTileMapHalfSpan;
    m_tileMapOriginZ = m_centerTileZ - kTileMapHalfSpan;
    std::fill(m_tileIndirectionScratch.begin(), m_tileIndirectionScratch.end(), 0u);
    for (const auto& [key, tile] : m_tiles)
    {
        (void)key;
        const int mx = tile.x - m_tileMapOriginX;
        const int mz = tile.z - m_tileMapOriginZ;
        if (mx < 0 || mz < 0 || mx >= static_cast<int>(kTileMapResolution)
            || mz >= static_cast<int>(kTileMapResolution))
            continue;
        m_tileIndirectionScratch[static_cast<std::size_t>(mz) * kTileMapResolution + mx]
            = static_cast<std::uint16_t>(tile.slot + 1u);
    }
    glBindTexture(GL_TEXTURE_2D, m_tileIndirectionTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
        static_cast<GLsizei>(kTileMapResolution), static_cast<GLsizei>(kTileMapResolution),
        GL_RED_INTEGER, GL_UNSIGNED_SHORT, m_tileIndirectionScratch.data());
}

void DynamicSurfaceGpuLodPrototype::destroyExactGeometryAtlas()
{
    if (m_geometryTileMetaBuffer) glDeleteBuffers(1, &m_geometryTileMetaBuffer);
    if (m_geometryBinIndexBuffer) glDeleteBuffers(1, &m_geometryBinIndexBuffer);
    if (m_geometryBinHeaderBuffer) glDeleteBuffers(1, &m_geometryBinHeaderBuffer);
    if (m_geometryTriangleBuffer) glDeleteBuffers(1, &m_geometryTriangleBuffer);
    m_geometryTriangleBuffer = 0;
    m_geometryBinHeaderBuffer = 0;
    m_geometryBinIndexBuffer = 0;
    m_geometryTileMetaBuffer = 0;
    m_geometryAtlasReady = false;
    m_stats.exactGeometrySupportReady = false;
    m_stats.geometryTriangles = 0;
    m_stats.geometryBinReferences = 0;
    m_stats.geometryUploadMiB = 0.0;
}

bool DynamicSurfaceGpuLodPrototype::rebuildExactGeometryAtlas(
    const heritage::physics::dynamicsurface::DynamicSurfaceSystem* dynamicSurface,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (!dynamicSurface || !m_centerTileValid)
    {
        errorMessage = "Dynamic Surface static triangle bake is unavailable.";
        return false;
    }

    using heritage::physics::dynamicsurface::ChunkAddress;
    using heritage::physics::dynamicsurface::StaticSurfacePatchTriangle;
    constexpr std::uint32_t binsPerChunk = kGeometryBinResolution * kGeometryBinResolution;
    constexpr std::uint32_t chunkSlots = kGeometryGridResolution * kGeometryGridResolution;

    m_geometryCenterChunkX = static_cast<std::int32_t>(std::floor(
        (static_cast<double>(m_centerTileX) * kTileWorldSizeM) / 100.0));
    m_geometryCenterChunkZ = static_cast<std::int32_t>(std::floor(
        (static_cast<double>(m_centerTileZ) * kTileWorldSizeM) / 100.0));

    std::vector<GpuSurfaceTriangle> gpuTriangles;
    std::vector<GpuBinHeader> gpuHeaders(
        static_cast<std::size_t>(chunkSlots) * binsPerChunk);
    std::vector<std::uint32_t> gpuIndices;
    std::vector<GpuTileGeometryMeta> meta(chunkSlots);

    const auto binCoordinate = [](double value) -> std::uint32_t {
        const double scaled = value * (static_cast<double>(kGeometryBinResolution) / 100.0);
        return static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(std::floor(scaled)), 0,
            static_cast<int>(kGeometryBinResolution) - 1));
    };

    std::uint32_t validChunks = 0u;
    for (int gz = 0; gz < static_cast<int>(kGeometryGridResolution); ++gz)
    {
        for (int gx = 0; gx < static_cast<int>(kGeometryGridResolution); ++gx)
        {
            const std::uint32_t slot = static_cast<std::uint32_t>(
                gz * static_cast<int>(kGeometryGridResolution) + gx);
            meta[slot].binHeaderBase = slot * binsPerChunk;
            const std::int32_t chunkX = m_geometryCenterChunkX + gx - kGeometryGridHalfSpan;
            const std::int32_t chunkZ = m_geometryCenterChunkZ + gz - kGeometryGridHalfSpan;
            const auto* chunk = dynamicSurface->findChunk(ChunkAddress{
                static_cast<std::int64_t>(chunkX), static_cast<std::int64_t>(chunkZ) });
            if (!chunk || chunk->staticTriangles().empty())
                continue;

            const double originX = static_cast<double>(chunkX) * 100.0;
            const double originZ = static_cast<double>(chunkZ) * 100.0;
            const auto& sourceTriangles = chunk->staticTriangles();
            const std::uint32_t triangleBase = static_cast<std::uint32_t>(gpuTriangles.size());
            std::vector<std::uint32_t> counts(binsPerChunk, 0u);
            struct Bounds { std::uint32_t minX, maxX, minZ, maxZ; };
            std::vector<Bounds> boundsList;
            boundsList.reserve(sourceTriangles.size());

            for (const StaticSurfacePatchTriangle& triangle : sourceTriangles)
            {
                GpuSurfaceTriangle gpu;
                gpu.a = { static_cast<float>(triangle.a.x - originX),
                    static_cast<float>(triangle.a.y),
                    static_cast<float>(triangle.a.z - originZ),
                    static_cast<float>(triangle.surfaceSheetId) };
                gpu.b = { static_cast<float>(triangle.b.x - originX),
                    static_cast<float>(triangle.b.y),
                    static_cast<float>(triangle.b.z - originZ), 0.0f };
                gpu.c = { static_cast<float>(triangle.c.x - originX),
                    static_cast<float>(triangle.c.y),
                    static_cast<float>(triangle.c.z - originZ), 0.0f };
                gpu.hydro = { std::max(triangle.infiltrationCapacityMmPerHour, 0.0f),
                    std::max(triangle.drainageCapacityMmPerHour, 0.0f),
                    std::max(std::abs(triangle.flowRoughness), 0.005f),
                    std::max(triangle.depressionStorageM, 0.0f) };
                gpuTriangles.push_back(gpu);

                const double minX = std::clamp(std::min({ triangle.a.x, triangle.b.x, triangle.c.x }) - originX, 0.0, 99.999999);
                const double maxX = std::clamp(std::max({ triangle.a.x, triangle.b.x, triangle.c.x }) - originX, 0.0, 99.999999);
                const double minZ = std::clamp(std::min({ triangle.a.z, triangle.b.z, triangle.c.z }) - originZ, 0.0, 99.999999);
                const double maxZ = std::clamp(std::max({ triangle.a.z, triangle.b.z, triangle.c.z }) - originZ, 0.0, 99.999999);
                Bounds bounds{ binCoordinate(minX), binCoordinate(maxX),
                    binCoordinate(minZ), binCoordinate(maxZ) };
                boundsList.push_back(bounds);
                for (std::uint32_t z = bounds.minZ; z <= bounds.maxZ; ++z)
                    for (std::uint32_t x = bounds.minX; x <= bounds.maxX; ++x)
                        ++counts[static_cast<std::size_t>(z) * kGeometryBinResolution + x];
            }

            std::uint32_t running = static_cast<std::uint32_t>(gpuIndices.size());
            std::vector<std::uint32_t> cursors(binsPerChunk, 0u);
            for (std::uint32_t bin = 0; bin < binsPerChunk; ++bin)
            {
                GpuBinHeader& header = gpuHeaders[
                    static_cast<std::size_t>(meta[slot].binHeaderBase) + bin];
                header.offset = running;
                header.count = counts[bin];
                cursors[bin] = running;
                running += counts[bin];
            }
            gpuIndices.resize(running);
            for (std::uint32_t localTriangle = 0;
                 localTriangle < static_cast<std::uint32_t>(boundsList.size());
                 ++localTriangle)
            {
                const Bounds& bounds = boundsList[localTriangle];
                for (std::uint32_t z = bounds.minZ; z <= bounds.maxZ; ++z)
                {
                    for (std::uint32_t x = bounds.minX; x <= bounds.maxX; ++x)
                    {
                        const std::uint32_t bin = z * kGeometryBinResolution + x;
                        gpuIndices[cursors[bin]++] = triangleBase + localTriangle;
                    }
                }
            }
            meta[slot].valid = 1u;
            ++validChunks;
        }
    }

    if (gpuTriangles.empty()) gpuTriangles.push_back({});
    if (gpuIndices.empty()) gpuIndices.push_back(0u);

    destroyExactGeometryAtlas();
    glGenBuffers(1, &m_geometryTriangleBuffer);
    glGenBuffers(1, &m_geometryBinHeaderBuffer);
    glGenBuffers(1, &m_geometryBinIndexBuffer);
    glGenBuffers(1, &m_geometryTileMetaBuffer);
    if (!m_geometryTriangleBuffer || !m_geometryBinHeaderBuffer
        || !m_geometryBinIndexBuffer || !m_geometryTileMetaBuffer)
    {
        errorMessage = "Failed to allocate DSURF04G exact-geometry SSBOs.";
        destroyExactGeometryAtlas();
        return false;
    }
    const auto upload = [](GLuint buffer, const void* data, std::size_t bytes) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
    };
    upload(m_geometryTriangleBuffer, gpuTriangles.data(), gpuTriangles.size() * sizeof(GpuSurfaceTriangle));
    upload(m_geometryBinHeaderBuffer, gpuHeaders.data(), gpuHeaders.size() * sizeof(GpuBinHeader));
    upload(m_geometryBinIndexBuffer, gpuIndices.data(), gpuIndices.size() * sizeof(std::uint32_t));
    upload(m_geometryTileMetaBuffer, meta.data(), meta.size() * sizeof(GpuTileGeometryMeta));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    if (!checkNoGlError(errorMessage, "DSURF04G exact-geometry atlas upload"))
    {
        destroyExactGeometryAtlas();
        return false;
    }

    m_geometryAtlasReady = true;
    m_stats.exactGeometrySupportReady = true;
    m_stats.geometryTriangles = gpuTriangles.size();
    m_stats.geometryBinReferences = gpuIndices.size();
    const std::size_t bytes = gpuTriangles.size() * sizeof(GpuSurfaceTriangle)
        + gpuHeaders.size() * sizeof(GpuBinHeader)
        + gpuIndices.size() * sizeof(std::uint32_t)
        + meta.size() * sizeof(GpuTileGeometryMeta);
    m_stats.geometryUploadMiB = static_cast<double>(bytes) / (1024.0 * 1024.0);
    (void)validChunks;
    return true;
}

void DynamicSurfaceGpuLodPrototype::refreshWaterPresentationTile(
    const TileRuntime& tile)
{
    (void)tile;
}

void DynamicSurfaceGpuLodPrototype::refreshWaterDiagnostics(
    double elapsedSeconds,
    double cameraGlobalX,
    double cameraGlobalZ)
{
    if (elapsedSeconds + 1.0e-9 < m_nextWaterDiagnosticsSeconds)
        return;
    m_nextWaterDiagnosticsSeconds = elapsedSeconds + 1.0;

    m_stats.waterProbeValid = false;
    m_stats.waterProbeWetTexels = 0u;
    m_stats.waterProbeTexels = 0u;
    m_stats.waterProbeMeanDepthM = 0.0f;
    m_stats.waterProbeMaximumDepthM = 0.0f;
    if (!m_water.allocated || !glad_glGetTextureSubImage)
        return;

    const std::int32_t tileX = static_cast<std::int32_t>(
        std::floor(cameraGlobalX / kTileWorldSizeM));
    const std::int32_t tileZ = static_cast<std::int32_t>(
        std::floor(cameraGlobalZ / kTileWorldSizeM));
    const auto found = m_tiles.find(tileKey(tileX, tileZ));
    if (found == m_tiles.end())
        return;

    const auto origin = atlasSlotOrigin(found->second.slot);
    const double localX = cameraGlobalX - static_cast<double>(tileX) * kTileWorldSizeM;
    const double localZ = cameraGlobalZ - static_cast<double>(tileZ) * kTileWorldSizeM;
    const int cameraTexelX = std::clamp(static_cast<int>(std::floor(
        localX * static_cast<double>(kTileResolution) / kTileWorldSizeM)),
        0, static_cast<int>(kTileResolution) - 1);
    const int cameraTexelZ = std::clamp(static_cast<int>(std::floor(
        localZ * static_cast<double>(kTileResolution) / kTileWorldSizeM)),
        0, static_cast<int>(kTileResolution) - 1);
    const int probeX = std::clamp(
        cameraTexelX - static_cast<int>(kWaterProbeResolution / 2u),
        0, static_cast<int>(kTileResolution - kWaterProbeResolution));
    const int probeZ = std::clamp(
        cameraTexelZ - static_cast<int>(kWaterProbeResolution / 2u),
        0, static_cast<int>(kTileResolution - kWaterProbeResolution));

    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT
        | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glGetTextureSubImage(
        m_water.atlas,
        0,
        static_cast<GLint>(origin[0]) + probeX,
        static_cast<GLint>(origin[1]) + probeZ,
        0,
        static_cast<GLsizei>(kWaterProbeResolution),
        static_cast<GLsizei>(kWaterProbeResolution),
        1,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        static_cast<GLsizei>(m_waterProbeScratch.size()),
        m_waterProbeScratch.data());

    static constexpr std::array<float, 16> kWaterLevelsM{{
        0.0f, 0.0001f, 0.0005f, 0.0010f, 0.0020f, 0.0030f, 0.0040f, 0.0060f,
        0.0080f, 0.0100f, 0.0130f, 0.0160f, 0.0200f, 0.0240f, 0.0280f, 0.0320f }};
    float accumulatedDepthM = 0.0f;
    for (std::size_t i = 0; i < m_waterProbeScratch.size(); i += 4u)
    {
        const std::uint32_t q = std::min<std::uint32_t>(
            (static_cast<std::uint32_t>(m_waterProbeScratch[i]) + 8u) / 17u,
            15u);
        const float depthM = kWaterLevelsM[q];
        accumulatedDepthM += depthM;
        m_stats.waterProbeMaximumDepthM = std::max(
            m_stats.waterProbeMaximumDepthM, depthM);
        if (q > 0u)
            ++m_stats.waterProbeWetTexels;
    }
    m_stats.waterProbeTexels = kWaterProbeResolution * kWaterProbeResolution;
    m_stats.waterProbeMeanDepthM = accumulatedDepthM
        / static_cast<float>(m_stats.waterProbeTexels);
    m_stats.waterProbeValid = true;
}

void DynamicSurfaceGpuLodPrototype::dispatchWaterBatch(
    const std::vector<TileRuntime*>& tiles,
    double elapsedSeconds,
    float precipitationRateMmPerHour,
    float weatherDrainageRateMmPerHour,
    float evaporationRateMmPerHour,
    float ambientTemperatureC)
{
    if (tiles.empty() || !m_water.allocated || !m_waterProgram
        || !m_waterBatchScatterProgram || !m_waterBatchScratch
        || !m_waterBatchTileBuffer)
    {
        return;
    }

    const std::size_t tileCount = std::min<std::size_t>(tiles.size(), kMaximumBatchTiles);
    m_waterBatchTileScratch.clear();
    for (std::size_t i = 0; i < tileCount; ++i)
    {
        const TileRuntime& tile = *tiles[i];
        const auto origin = atlasSlotOrigin(tile.slot);
        GpuHydroBatchTile batch{};
        batch.worldTile = { tile.x, tile.z };
        batch.atlasOrigin = { static_cast<std::int32_t>(origin[0]),
            static_cast<std::int32_t>(origin[1]) };
        batch.worldStateIndex = tile.worldStateIndex;
        batch.cycleDtSeconds = static_cast<float>(std::clamp(
            elapsedSeconds - tile.lastUpdateSeconds, 0.0, 60.0));
        m_waterBatchTileScratch.push_back(batch);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_waterBatchTileBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
        static_cast<GLsizeiptr>(m_waterBatchTileScratch.size() * sizeof(GpuHydroBatchTile)),
        m_waterBatchTileScratch.data());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 25, m_waterBatchTileBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, m_geometryTriangleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 21, m_geometryBinHeaderBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 22, m_geometryBinIndexBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 23, m_geometryTileMetaBuffer);
    glUseProgram(m_waterProgram);
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, m_water.atlas);
    glActiveTexture(GL_TEXTURE0 + 6);
    glBindTexture(GL_TEXTURE_2D, m_tileIndirectionTexture);
    glBindImageTexture(1, m_waterBatchScratch, 0, GL_TRUE, 0,
        GL_WRITE_ONLY, GL_RGBA8);

    if (m_waterUniforms.stateAtlas >= 0) glUniform1i(m_waterUniforms.stateAtlas, 0);
    if (m_waterUniforms.tileMap >= 0) glUniform1i(m_waterUniforms.tileMap, 6);
    if (m_waterUniforms.batchTileCount >= 0)
        glUniform1ui(m_waterUniforms.batchTileCount, static_cast<GLuint>(tileCount));
    if (m_waterUniforms.tileMapOrigin >= 0) glUniform2i(m_waterUniforms.tileMapOrigin,
        m_tileMapOriginX, m_tileMapOriginZ);
    if (m_waterUniforms.geometryCenterChunk >= 0) glUniform2i(m_waterUniforms.geometryCenterChunk,
        m_geometryCenterChunkX, m_geometryCenterChunkZ);
    if (m_waterUniforms.cellSizeM >= 0) glUniform1f(m_waterUniforms.cellSizeM, kCellSizeM);
    if (m_waterUniforms.precipitationRateMmPerHour >= 0)
        glUniform1f(m_waterUniforms.precipitationRateMmPerHour, precipitationRateMmPerHour);
    if (m_waterUniforms.weatherDrainageRateMmPerHour >= 0)
        glUniform1f(m_waterUniforms.weatherDrainageRateMmPerHour, weatherDrainageRateMmPerHour);
    if (m_waterUniforms.evaporationRateMmPerHour >= 0)
        glUniform1f(m_waterUniforms.evaporationRateMmPerHour, evaporationRateMmPerHour);
    if (m_waterUniforms.ambientTemperatureC >= 0)
        glUniform1f(m_waterUniforms.ambientTemperatureC, ambientTemperatureC);
    if (m_waterUniforms.tickIndex >= 0)
        glUniform1ui(m_waterUniforms.tickIndex, static_cast<GLuint>(
            std::max(0.0, std::floor(m_lastElapsedSeconds * 12.0))));

    // One 3D compute dispatch represents the whole near-field cohort. Z selects
    // the 10m tile; X/Y cover all 256x256 texels of that tile. Hardware schedules
    // the workgroups across SMs/waves, but Heritage submits it as one GPU job.
    glDispatchCompute(kTileResolution / 16u, kTileResolution / 16u,
        static_cast<GLuint>(tileCount));
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    // One scatter dispatch publishes every completed layer into its atlas slot.
    // This replaces hundreds of glCopyImageSubData calls and per-tile barriers.
    glUseProgram(m_waterBatchScatterProgram);
    glBindImageTexture(1, m_waterBatchScratch, 0, GL_TRUE, 0,
        GL_READ_ONLY, GL_RGBA8);
    glBindImageTexture(2, m_water.atlas, 0, GL_FALSE, 0,
        GL_WRITE_ONLY, GL_RGBA8);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 25, m_waterBatchTileBuffer);
    if (m_waterBatchScatterCountLocation >= 0)
        glUniform1ui(m_waterBatchScatterCountLocation, static_cast<GLuint>(tileCount));
    glDispatchCompute(kTileResolution / 16u, kTileResolution / 16u,
        static_cast<GLuint>(tileCount));
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
        | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    m_stats.dispatchesThisFrame += 2u;
    m_stats.cellsThisFrame += static_cast<std::uint64_t>(tileCount)
        * static_cast<std::uint64_t>(kTileResolution) * kTileResolution;
}

void DynamicSurfaceGpuLodPrototype::dispatchTileState(
    StateKind kind,
    StateRuntime& state,
    GLuint program,
    const ProgramUniforms& uniforms,
    const TileRuntime& tile,
    float cycleDtSeconds,
    float precipitationRateMmPerHour,
    float weatherDrainageRateMmPerHour,
    float evaporationRateMmPerHour,
    float ambientTemperatureC)
{
    if (!state.allocated || !program)
        return;
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, state.atlas);
    glActiveTexture(GL_TEXTURE0 + 6);
    glBindTexture(GL_TEXTURE_2D, m_tileIndirectionTexture);
    glBindImageTexture(1, state.scratch, 0, GL_FALSE, 0, GL_WRITE_ONLY, state.internalFormat);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, m_geometryTriangleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 21, m_geometryBinHeaderBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 22, m_geometryBinIndexBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 23, m_geometryTileMetaBuffer);
    const auto origin = atlasSlotOrigin(tile.slot);
    if (uniforms.worldTile >= 0) glUniform2i(uniforms.worldTile, tile.x, tile.z);
    if (uniforms.atlasOrigin >= 0) glUniform2i(uniforms.atlasOrigin,
        static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]));
    if (uniforms.tileMapOrigin >= 0) glUniform2i(uniforms.tileMapOrigin,
        m_tileMapOriginX, m_tileMapOriginZ);
    if (uniforms.geometryCenterChunk >= 0) glUniform2i(uniforms.geometryCenterChunk,
        m_geometryCenterChunkX, m_geometryCenterChunkZ);
    if (uniforms.cellSizeM >= 0) glUniform1f(uniforms.cellSizeM, kCellSizeM);
    if (uniforms.cycleDtSeconds >= 0) glUniform1f(uniforms.cycleDtSeconds, cycleDtSeconds);
    if (uniforms.precipitationRateMmPerHour >= 0)
        glUniform1f(uniforms.precipitationRateMmPerHour, precipitationRateMmPerHour);
    if (uniforms.weatherDrainageRateMmPerHour >= 0)
        glUniform1f(uniforms.weatherDrainageRateMmPerHour, weatherDrainageRateMmPerHour);
    if (uniforms.evaporationRateMmPerHour >= 0)
        glUniform1f(uniforms.evaporationRateMmPerHour, evaporationRateMmPerHour);
    if (uniforms.ambientTemperatureC >= 0)
        glUniform1f(uniforms.ambientTemperatureC, ambientTemperatureC);
    if (uniforms.tickIndex >= 0)
        glUniform1ui(uniforms.tickIndex, static_cast<GLuint>(
            std::max(0.0, std::floor(m_lastElapsedSeconds * 12.0))));
    if (uniforms.worldStateIndex >= 0)
        glUniform1ui(uniforms.worldStateIndex, tile.worldStateIndex);

    if (kind == StateKind::Water)
    {
        if (uniforms.stateAtlas >= 0) glUniform1i(uniforms.stateAtlas, 0);
        if (uniforms.tileMap >= 0) glUniform1i(uniforms.tileMap, 6);
    }
    else
    {
        if (uniforms.stateAtlas >= 0) glUniform1i(uniforms.stateAtlas, 0);
    }

    glDispatchCompute(kTileResolution / 16u, kTileResolution / 16u, 1u);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    glCopyImageSubData(state.scratch, GL_TEXTURE_2D, 0, 0, 0, 0,
        state.atlas, GL_TEXTURE_2D, 0,
        static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]), 0,
        static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution), 1);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    if (kind == StateKind::Water)
        refreshWaterPresentationTile(tile);

    ++m_stats.dispatchesThisFrame;
    m_stats.cellsThisFrame += static_cast<std::uint64_t>(kTileResolution) * kTileResolution;
}

void DynamicSurfaceGpuLodPrototype::dispatchDueTiles(
    double elapsedSeconds,
    float precipitationRateMmPerHour,
    float weatherDrainageRateMmPerHour,
    float evaporationRateMmPerHour,
    float ambientTemperatureC)
{
    if (m_tiles.empty())
    {
        m_stats.dueTiles = 0u;
        m_stats.dispatchBacklogTiles = 0u;
        return;
    }

    // Collect due work first, then admit one bounded near-field cohort. Batched
    // submission removes hundreds of driver calls and the complete <=100m disk
    // fits in the preallocated scratch. Simulation residency is camera-distance
    // based only; mesh/frustum visibility remains a presentation concern.
    std::vector<TileRuntime*> due;
    due.reserve(m_tiles.size());
    for (auto& [key, tile] : m_tiles)
    {
        (void)key;
        const float centerDx = (static_cast<float>(tile.x - m_centerTileX)) * kTileWorldSizeM;
        const float centerDz = (static_cast<float>(tile.z - m_centerTileZ)) * kTileWorldSizeM;
        const float nearestDx = std::max(std::abs(centerDx) - kTileWorldSizeM * 0.5f, 0.0f);
        const float nearestDz = std::max(std::abs(centerDz) - kTileWorldSizeM * 0.5f, 0.0f);
        const float distanceM = std::hypot(nearestDx, nearestDz);
        const float hz = cadenceHzForDistance(distanceM, tile.prewarm);
        if (hz <= 0.0f)
            continue;

        if (!tile.initialized || elapsedSeconds + 1.0e-9 >= tile.nextDueSeconds)
        {
            tile.cadenceBand = cadenceBandForDistance(distanceM, tile.prewarm);
            due.push_back(&tile);
        }
    }

    std::sort(due.begin(), due.end(), [](const TileRuntime* a, const TileRuntime* b) {
        // Every due near tile must fit before retained-history work. Otherwise
        // a once-per-minute history cohort could temporarily crowd newly entered
        // camera-radius tiles out of the supposedly complete 100m field.
        if (a->prewarm != b->prewarm) return a->prewarm < b->prewarm;
        if (a->initialized != b->initialized) return a->initialized < b->initialized;
        if (a->nextDueSeconds != b->nextDueSeconds) return a->nextDueSeconds < b->nextDueSeconds;
        if (a->cadenceBand != b->cadenceBand) return a->cadenceBand < b->cadenceBand;
        if (a->z != b->z) return a->z < b->z;
        return a->x < b->x;
    });

    m_stats.dueTiles = static_cast<std::uint32_t>(due.size());
    std::vector<TileRuntime*> cohort;
    cohort.reserve(std::min<std::size_t>(due.size(), kMaximumTileUpdatesPerFrame));
    std::size_t newTileCount = 0u;
    for (TileRuntime* tile : due)
    {
        if (cohort.size() >= kMaximumTileUpdatesPerFrame)
            break;
        if (!tile->initialized)
        {
            if (newTileCount >= kMaximumNewTileInitializationsPerFrame)
                continue;
            ++newTileCount;
        }
        cohort.push_back(tile);
    }
    m_stats.dispatchBacklogTiles = due.size() > cohort.size()
        ? static_cast<std::uint32_t>(due.size() - cohort.size()) : 0u;
    if (cohort.empty())
        return;

    dispatchWaterBatch(cohort, elapsedSeconds, precipitationRateMmPerHour,
        weatherDrainageRateMmPerHour, evaporationRateMmPerHour,
        ambientTemperatureC);

    // Snow/mud remain lazy optional states; water is the continuously active
    // batched path. Keep their legacy update local until promoted.
    if (m_snow.allocated || m_mud.allocated)
    {
        for (TileRuntime* tile : cohort)
        {
            const float hz = cadenceHzForDistance(
                std::hypot(float(tile->x - m_centerTileX) * kTileWorldSizeM,
                    float(tile->z - m_centerTileZ) * kTileWorldSizeM), false);
            const float dt = hz > 0.0f ? 1.0f / hz : 2.0f;
            if (m_snow.allocated)
                dispatchTileState(StateKind::Snow, m_snow, m_snowProgram, m_snowUniforms,
                    *tile, dt, precipitationRateMmPerHour, weatherDrainageRateMmPerHour,
                    evaporationRateMmPerHour, ambientTemperatureC);
            if (m_mud.allocated)
                dispatchTileState(StateKind::Mud, m_mud, m_mudProgram, m_mudUniforms,
                    *tile, dt, precipitationRateMmPerHour, weatherDrainageRateMmPerHour,
                    evaporationRateMmPerHour, ambientTemperatureC);
        }
    }

    for (TileRuntime* tile : cohort)
    {
        tile->initialized = true;
        tile->lastUpdateSeconds = elapsedSeconds;
        const float centerDx = static_cast<float>(tile->x - m_centerTileX) * kTileWorldSizeM;
        const float centerDz = static_cast<float>(tile->z - m_centerTileZ) * kTileWorldSizeM;
        const float nearestDx = std::max(std::abs(centerDx) - kTileWorldSizeM * 0.5f, 0.0f);
        const float nearestDz = std::max(std::abs(centerDz) - kTileWorldSizeM * 0.5f, 0.0f);
        const float hz = cadenceHzForDistance(
            std::hypot(nearestDx, nearestDz), tile->prewarm);
        tile->nextDueSeconds = elapsedSeconds
            + (hz > 0.0f ? 1.0 / static_cast<double>(hz) : 60.0);
        const std::size_t statBand = std::min<std::size_t>(tile->cadenceBand, 3u);
        ++m_stats.waterLodDispatches[statBand];
        ++m_stats.waterLodPublishedCycles[statBand];
        ++m_stats.waterPublishedCycles;
    }
}

void DynamicSurfaceGpuLodPrototype::update(
    double elapsedSeconds,
    double cameraGlobalX,
    double cameraGlobalY,
    double cameraGlobalZ,
    float precipitationRateMmPerHour,
    float weatherDrainageRateMmPerHour,
    float evaporationRateMmPerHour,
    float ambientTemperatureC,
    const heritage::physics::dynamicsurface::DynamicSurfaceSystem* dynamicSurface,
    const std::vector<DynamicSurfaceGpuTireContactEvent>& tireEvents)
{
    (void)cameraGlobalY;
    if (!m_stats.ready)
        return;

    updateGpuTimerResult();
    const auto cpuStarted = std::chrono::steady_clock::now();
    const double deltaSeconds = m_lastElapsedSeconds >= 0.0
        ? std::clamp(elapsedSeconds - m_lastElapsedSeconds, 0.0, 0.25) : 0.0;
    m_lastElapsedSeconds = elapsedSeconds;

    m_stats.dispatchesThisFrame = 0u;
    m_stats.waterPresentationDispatchesThisFrame = 0u;
    m_stats.cellsThisFrame = 0u;
    m_stats.tireEventDispatches = 0u;
    m_stats.tireEventCells = 0u;
    m_stats.waterLodDispatches.fill(0u);
    m_stats.geometryValidTiles.fill(0u);
    m_stats.precipitationRateMmPerHour = std::max(precipitationRateMmPerHour, 0.0f);
    m_stats.drainageRateMmPerHour = std::max(weatherDrainageRateMmPerHour, 0.0f);
    m_stats.evaporationRateMmPerHour = std::max(evaporationRateMmPerHour, 0.0f);

    if (m_lastCameraSampleSeconds >= 0.0 && elapsedSeconds > m_lastCameraSampleSeconds)
    {
        const double dt = std::clamp(elapsedSeconds - m_lastCameraSampleSeconds, 1.0e-4, 0.25);
        const double instantVx = (cameraGlobalX - m_lastCameraGlobalX) / dt;
        const double instantVz = (cameraGlobalZ - m_lastCameraGlobalZ) / dt;
        const double alpha = 1.0 - std::exp(-dt * 6.0);
        m_cameraVelocityX += (instantVx - m_cameraVelocityX) * alpha;
        m_cameraVelocityZ += (instantVz - m_cameraVelocityZ) * alpha;
    }
    m_lastCameraGlobalX = cameraGlobalX;
    m_lastCameraGlobalZ = cameraGlobalZ;
    m_lastCameraSampleSeconds = elapsedSeconds;

    if (deltaSeconds > 0.0)
    {
        const float rainM = std::max(precipitationRateMmPerHour, 0.0f)
            * (0.001f / 3600.0f) * static_cast<float>(deltaSeconds);
        const float lossM = (std::max(weatherDrainageRateMmPerHour, 0.0f)
            + std::max(evaporationRateMmPerHour, 0.0f))
            * (0.001f / 3600.0f) * static_cast<float>(deltaSeconds);
        m_backgroundSeedDepthM = std::clamp(m_backgroundSeedDepthM + rainM - lossM,
            0.0f, 0.032f);
    }
    m_stats.backgroundSeedDepthM = m_backgroundSeedDepthM;

    std::string worldStateError;
    if (!ensureWorldTileSimulation(dynamicSurface, m_backgroundSeedDepthM, worldStateError))
    {
        m_stats.authoritative = false;
        return;
    }

    if (!m_centerTileValid || elapsedSeconds >= m_nextResidencyRefreshSeconds
        || static_cast<std::int32_t>(std::floor(cameraGlobalX / kTileWorldSizeM)) != m_centerTileX
        || static_cast<std::int32_t>(std::floor(cameraGlobalZ / kTileWorldSizeM)) != m_centerTileZ)
    {
        refreshResidency(elapsedSeconds, cameraGlobalX, cameraGlobalZ, m_backgroundSeedDepthM);
        m_nextResidencyRefreshSeconds = elapsedSeconds + 0.10;
    }

    const std::int32_t centerChunkX = static_cast<std::int32_t>(std::floor(cameraGlobalX / 100.0));
    const std::int32_t centerChunkZ = static_cast<std::int32_t>(std::floor(cameraGlobalZ / 100.0));
    if (!m_geometryAtlasReady || centerChunkX != m_geometryCenterChunkX
        || centerChunkZ != m_geometryCenterChunkZ)
    {
        std::string geometryError;
        if (!rebuildExactGeometryAtlas(dynamicSurface, geometryError))
        {
            m_stats.authoritative = false;
            return;
        }
    }

    if (ambientTemperatureC <= 1.5f && precipitationRateMmPerHour > 0.0f
        && !m_snow.allocated)
    {
        std::string error;
        ensureSnowState(error);
    }
    bool mudRequested = false;
    for (const auto& event : tireEvents)
        mudRequested = mudRequested || event.mudDeformable;
    if (mudRequested && !m_mud.allocated)
    {
        std::string error;
        ensureMudState(error);
    }

    m_stats.snowReady = m_snow.allocated;
    m_stats.mudReady = m_mud.allocated;
    m_stats.authoritative = m_geometryAtlasReady && m_water.allocated;
    m_stats.waterReady = m_water.allocated;
    m_stats.waterPresentationReady = m_water.allocated;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, m_geometryTriangleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 21, m_geometryBinHeaderBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 22, m_geometryBinIndexBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 23, m_geometryTileMetaBuffer);

    const bool timerBegan = beginGpuTimer();
    // The detailed <=100m cohort uses distance-adaptive cadence. Spare atlas
    // slots retain a small recently visited history set at one update/minute;
    // there is still no duplicate all-scene per-texel water simulation.
    dispatchDueTiles(elapsedSeconds, precipitationRateMmPerHour,
        weatherDrainageRateMmPerHour, evaporationRateMmPerHour, ambientTemperatureC);
    applyTireContactEvents(tireEvents);
    endGpuTimer(timerBegan);
    // A tiny 16x16 readback once per second makes the actual authoritative
    // atlas observable in F8.  It is intentionally outside the GPU timer and
    // replaces guesswork about whether rain reached the compute field.
    refreshWaterDiagnostics(elapsedSeconds, cameraGlobalX, cameraGlobalZ);

    m_stats.centerTileX = m_centerTileX;
    m_stats.centerTileZ = m_centerTileZ;
    const auto cpuEnded = std::chrono::steady_clock::now();
    m_stats.cpuDispatchMs = std::chrono::duration<double, std::milli>(
        cpuEnded - cpuStarted).count();
}

void DynamicSurfaceGpuLodPrototype::applyTireContactEvents(
    const std::vector<DynamicSurfaceGpuTireContactEvent>& events)
{
    if (!m_tireEventProgram || !m_water.allocated || events.empty())
        return;
    glUseProgram(m_tireEventProgram);
    glBindImageTexture(0, m_water.atlas, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    if (m_snow.allocated)
        glBindImageTexture(1, m_snow.atlas, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R16UI);
    if (m_mud.allocated)
        glBindImageTexture(2, m_mud.atlas, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R8UI);
    std::unordered_set<std::uint64_t> presentationDirtyTiles;
    const std::size_t eventCount = std::min<std::size_t>(
        events.size(), kMaximumTireContactEventsPerFrame);
    for (std::size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex)
    {
        const auto& event = events[eventIndex];
        const std::int32_t tileX = static_cast<std::int32_t>(std::floor(event.globalX / kTileWorldSizeM));
        const std::int32_t tileZ = static_cast<std::int32_t>(std::floor(event.globalZ / kTileWorldSizeM));
        const auto found = m_tiles.find(tileKey(tileX, tileZ));
        if (found == m_tiles.end())
            continue;
        const TileRuntime& tile = found->second;
        const auto origin = atlasSlotOrigin(tile.slot);
        const double tileOriginX = static_cast<double>(tileX) * kTileWorldSizeM;
        const double tileOriginZ = static_cast<double>(tileZ) * kTileWorldSizeM;
        const float localX = static_cast<float>(event.globalX - tileOriginX);
        const float localZ = static_cast<float>(event.globalZ - tileOriginZ);
        const float halfLength = std::max(event.patchLengthM * 0.5f, kCellSizeM);
        const float halfWidth = std::max(event.patchWidthM * 0.5f, kCellSizeM);
        const float radiusM = std::max(halfLength, halfWidth) + kCellSizeM * 2.0f;
        const int minX = static_cast<int>(std::floor((localX - radiusM) / kCellSizeM));
        const int minZ = static_cast<int>(std::floor((localZ - radiusM) / kCellSizeM));
        const int maxX = static_cast<int>(std::ceil((localX + radiusM) / kCellSizeM));
        const int maxZ = static_cast<int>(std::ceil((localZ + radiusM) / kCellSizeM));
        const int extentX = std::max(maxX - minX + 1, 1);
        const int extentZ = std::max(maxZ - minZ + 1, 1);

        if (m_tireEventUniforms.atlasOrigin >= 0) glUniform2i(m_tireEventUniforms.atlasOrigin,
            static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]));
        if (m_tireEventUniforms.minTexel >= 0) glUniform2i(m_tireEventUniforms.minTexel, minX, minZ);
        if (m_tireEventUniforms.extentTexels >= 0) glUniform2i(m_tireEventUniforms.extentTexels, extentX, extentZ);
        if (m_tireEventUniforms.cellSizeM >= 0) glUniform1f(m_tireEventUniforms.cellSizeM, kCellSizeM);
        if (m_tireEventUniforms.eventLocalXZ >= 0) glUniform2f(m_tireEventUniforms.eventLocalXZ, localX, localZ);
        if (m_tireEventUniforms.forwardXZ >= 0) glUniform2f(m_tireEventUniforms.forwardXZ, event.forwardX, event.forwardZ);
        if (m_tireEventUniforms.rightXZ >= 0) glUniform2f(m_tireEventUniforms.rightXZ, event.rightX, event.rightZ);
        if (m_tireEventUniforms.patchHalfLengthM >= 0) glUniform1f(m_tireEventUniforms.patchHalfLengthM, halfLength);
        if (m_tireEventUniforms.patchHalfWidthM >= 0) glUniform1f(m_tireEventUniforms.patchHalfWidthM, halfWidth);
        if (m_tireEventUniforms.normalLoadN >= 0) glUniform1f(m_tireEventUniforms.normalLoadN, event.normalLoadN);
        if (m_tireEventUniforms.speedMps >= 0) glUniform1f(m_tireEventUniforms.speedMps, event.speedMps);
        if (m_tireEventUniforms.accumulatedDtSeconds >= 0) glUniform1f(m_tireEventUniforms.accumulatedDtSeconds, event.accumulatedDtSeconds);
        if (m_tireEventUniforms.snowReady >= 0) glUniform1i(m_tireEventUniforms.snowReady, m_snow.allocated ? 1 : 0);
        if (m_tireEventUniforms.mudReady >= 0) glUniform1i(m_tireEventUniforms.mudReady, m_mud.allocated ? 1 : 0);
        if (m_tireEventUniforms.mudDeformable >= 0) glUniform1i(m_tireEventUniforms.mudDeformable, event.mudDeformable ? 1 : 0);

        glDispatchCompute(static_cast<GLuint>((extentX + 7) / 8),
            static_cast<GLuint>((extentZ + 7) / 8), 1u);
        ++m_stats.tireEventDispatches;
        m_stats.tireEventCells += static_cast<std::uint64_t>(extentX)
            * static_cast<std::uint64_t>(extentZ);
        presentationDirtyTiles.insert(tileKey(tileX, tileZ));
    }
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    for (const std::uint64_t key : presentationDirtyTiles)
    {
        const auto found = m_tiles.find(key);
        if (found != m_tiles.end())
            refreshWaterPresentationTile(found->second);
    }
}

void DynamicSurfaceGpuLodPrototype::updateGpuTimerResult()
{
    for (std::size_t i = 0; i < m_gpuTimerPending.size(); ++i)
    {
        if (!m_gpuTimerPending[i])
            continue;
        GLint available = GL_FALSE;
        glGetQueryObjectiv(m_gpuTimerEndQueries[i], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available != GL_TRUE)
            continue;
        GLuint64 start = 0;
        GLuint64 end = 0;
        glGetQueryObjectui64v(m_gpuTimerStartQueries[i], GL_QUERY_RESULT, &start);
        glGetQueryObjectui64v(m_gpuTimerEndQueries[i], GL_QUERY_RESULT, &end);
        if (end >= start)
            m_stats.gpuComputeMs = static_cast<double>(end - start) / 1000000.0;
        m_gpuTimerPending[i] = false;
    }
}

bool DynamicSurfaceGpuLodPrototype::beginGpuTimer()
{
    const std::size_t index = m_gpuTimerWriteIndex;
    if (m_gpuTimerPending[index])
        return false;
    glQueryCounter(m_gpuTimerStartQueries[index], GL_TIMESTAMP);
    return true;
}

void DynamicSurfaceGpuLodPrototype::endGpuTimer(bool began)
{
    if (!began)
        return;
    const std::size_t index = m_gpuTimerWriteIndex;
    glQueryCounter(m_gpuTimerEndQueries[index], GL_TIMESTAMP);
    m_gpuTimerPending[index] = true;
    m_gpuTimerWriteIndex = (m_gpuTimerWriteIndex + 1u) % m_gpuTimerPending.size();
}

} // namespace heritage::graphics::dynamicsurface
