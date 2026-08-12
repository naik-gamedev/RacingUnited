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

// TIRE09/VIS01: physics-driven GPU deformation for authored tire nodes.
uniform bool uTireVisualEnabled;
uniform bool uTireVisualGrounded;
uniform vec3 uTireVisualCenter;
uniform int uTireVisualAxleAxis;
uniform float uTireVisualHalfWidth;
uniform float uTireVisualInnerRadius;
uniform float uTireVisualOuterRadius;
uniform float uTireReferenceRadiusM;
uniform float uTireRadialDeflectionM;
uniform float uTireContactPatchLengthM;
uniform float uTireContactPatchWidthM;
uniform float uTireRingRadialOffsetM;
uniform float uTireRingLongitudinalOffsetM;
uniform float uTireRingLateralOffsetM;
uniform float uTireRingYawDegrees;
uniform float uTireRingWindupDegrees;
uniform float uTireFlatSpotDepthM;
uniform float uTireFlatSpotSector;
// TIRE10/VIS02 authoritative native contact plane.
uniform vec3 uTireContactNormalWorld;
// TIRE17C2/VIS04 authoritative physics wheel basis. These directions remove
// mirrored-node ambiguity for longitudinal braking shear and lateral cornering
// carcass deformation.
uniform vec3 uTireWheelForwardWorld;
uniform vec3 uTireWheelRightWorld;
uniform float uTireNormalForceN;
uniform float uTireLongitudinalForceN;
uniform float uTireLateralForceN;
uniform float uTireVisualMotionSpeedMps;
uniform float uTireContactPlaneDistanceM;
// TIRE17C1/VIS03 refined curb/step support residuals.
uniform bool uTireVisualSupportGridValid;
uniform float uTireVisualSupportHalfLengthM;
uniform float uTireVisualSupportHalfWidthM;
uniform float uTireVisualSupportHeightResidualM[9];
// TIRE27/VIS20 authoritative render-facing lower-shell probe lattice.
// 21 non-uniform stations: sparse near the equators, deliberately dense around
// straight-down. 13 width bands resolve centre tread, shoulders and sidewalls.
const int HERITAGE_TIRE_PROBE_STATIONS = 21;
const int HERITAGE_TIRE_PROBE_BANDS = 13;
const int HERITAGE_TIRE_PROBE_COUNT = 273;
uniform bool uTireVisualProbeGridValid;
uniform float uTireVisualProbeCompressionM[HERITAGE_TIRE_PROBE_COUNT];
uniform bool uTireProbeDebugVisible;
// TIRE27: legacy exact-triangle visual uniforms removed; dense probe lattice owns contact deformation.

out vec3 vNormal;
out vec3 vWorldPosition;
out vec2 vTexCoord;
out vec3 vTangent;
out float vTangentSign;
out vec4 vColor;
out float vViewDepth;
out vec3 vTireProbeDebugColor;
out float vTireProbeDebugMask;

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

float tireSupportResidualM(float longitudinalM, float lateralM)
{
    if (!uTireVisualSupportGridValid
        || uTireVisualSupportHalfLengthM <= 0.005
        || uTireVisualSupportHalfWidthM <= 0.005)
        return 0.0;

    float gridX = clamp(
        longitudinalM / uTireVisualSupportHalfLengthM + 1.0, 0.0, 2.0);
    float gridY = clamp(
        lateralM / uTireVisualSupportHalfWidthM + 1.0, 0.0, 2.0);
    int x0 = min(int(floor(gridX)), 1);
    int y0 = min(int(floor(gridY)), 1);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    float tx = gridX - float(x0);
    float ty = gridY - float(y0);
    float h00 = uTireVisualSupportHeightResidualM[y0 * 3 + x0];
    float h10 = uTireVisualSupportHeightResidualM[y0 * 3 + x1];
    float h01 = uTireVisualSupportHeightResidualM[y1 * 3 + x0];
    float h11 = uTireVisualSupportHeightResidualM[y1 * 3 + x1];
    return mix(mix(h00, h10, tx), mix(h01, h11, tx), ty);
}

vec3 tireCurbFaceCompressionLocal(
    float longitudinalM,
    float lateralM,
    float heightAboveCenterPlaneM,
    float radialFraction,
    float roadFacing,
    vec3 forward,
    vec3 lateral,
    float metersToLocal)
{
    if (!uTireVisualSupportGridValid)
        return vec3(0.0);

    float hLatNeg = tireSupportResidualM(
        longitudinalM, -uTireVisualSupportHalfWidthM);
    float hLatMid = tireSupportResidualM(longitudinalM, 0.0);
    float hLatPos = tireSupportResidualM(
        longitudinalM, uTireVisualSupportHalfWidthM);
    float dLatNeg = hLatMid - hLatNeg;
    float dLatPos = hLatPos - hLatMid;
    bool useLatNeg = abs(dLatNeg) >= abs(dLatPos);
    float lateralStepM = useLatNeg ? dLatNeg : dLatPos;
    float lateralEdgeM = (useLatNeg ? -0.5 : 0.5)
        * uTireVisualSupportHalfWidthM;

    float hLongNeg = tireSupportResidualM(
        -uTireVisualSupportHalfLengthM, lateralM);
    float hLongMid = tireSupportResidualM(0.0, lateralM);
    float hLongPos = tireSupportResidualM(
        uTireVisualSupportHalfLengthM, lateralM);
    float dLongNeg = hLongMid - hLongNeg;
    float dLongPos = hLongPos - hLongMid;
    bool useLongNeg = abs(dLongNeg) >= abs(dLongPos);
    float longitudinalStepM = useLongNeg ? dLongNeg : dLongPos;
    float longitudinalEdgeM = (useLongNeg ? -0.5 : 0.5)
        * uTireVisualSupportHalfLengthM;

    bool useLateral = abs(lateralStepM) >= abs(longitudinalStepM);
    float stepSignedM = useLateral ? lateralStepM : longitudinalStepM;
    float stepM = abs(stepSignedM);
    if (stepM <= 0.008)
        return vec3(0.0);

    float highSign = stepSignedM >= 0.0 ? 1.0 : -1.0;
    float coordinateM = useLateral ? lateralM : longitudinalM;
    float edgeM = useLateral ? lateralEdgeM : longitudinalEdgeM;
    vec3 axis = useLateral ? lateral : forward;

    float gridMinM = uTireVisualSupportHeightResidualM[0];
    for (int supportIndex = 1; supportIndex < 9; ++supportIndex)
        gridMinM = min(gridMinM, uTireVisualSupportHeightResidualM[supportIndex]);
    float heightAboveLowSupportM = max(heightAboveCenterPlaneM - gridMinM, 0.0);
    float belowTop = 1.0 - smoothstep(
        max(stepM * 0.78, 0.006),
        max(stepM * 1.05, 0.010),
        heightAboveLowSupportM);

    float penetrationM = highSign * (coordinateM - edgeM);
    float penetrationMask = smoothstep(
        0.0015, max(0.010, stepM * 0.35), penetrationM);
    float structuralMask = smoothstep(0.18, 0.82, radialFraction)
        * smoothstep(-0.18, 0.88, roadFacing);
    float activation = smoothstep(0.010, 0.030, stepM)
        * belowTop * penetrationMask * structuralMask;
    float compressionM = min(
        max(penetrationM, 0.0) * 0.82 + stepM * 0.06,
        0.032);
    return -axis * (highSign * compressionM * metersToLocal * activation);
}

vec3 tireClosestPointOnTriangle(vec3 p, vec3 a, vec3 b, vec3 c)
{
    // Ericson-style finite-triangle closest point. The deformation pass must
    // not treat creator triangles as infinite planes; otherwise a kerb face
    // can distort rubber well beyond its real edge.
    vec3 ab = b - a;
    vec3 ac = c - a;
    vec3 ap = p - a;
    float d1 = dot(ab, ap);
    float d2 = dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0)
        return a;

    vec3 bp = p - b;
    float d3 = dot(ab, bp);
    float d4 = dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3)
        return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
    {
        float v = d1 / max(d1 - d3, 1.0e-12);
        return a + v * ab;
    }

    vec3 cp = p - c;
    float d5 = dot(ab, cp);
    float d6 = dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6)
        return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
    {
        float w = d2 / max(d2 - d6, 1.0e-12);
        return a + w * ac;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
    {
        vec3 bc = c - b;
        float w = (d4 - d3) / max((d4 - d3) + (d5 - d6), 1.0e-12);
        return b + w * bc;
    }

    float denominator = va + vb + vc;
    if (abs(denominator) <= 1.0e-12)
        return a;
    float inverseDenominator = 1.0 / denominator;
    float v = vb * inverseDenominator;
    float w = vc * inverseDenominator;
    return a + ab * v + ac * w;
}


float tireProbeStationPhi(int index)
{
    // Must match EntityRegistry.hpp::TireVisualProbeStationPhiRadians.
    if (index <= 0) return 0.0;
    if (index == 1) return 0.392699082;
    if (index == 2) return 0.698131701;
    if (index == 3) return 0.959931089;
    if (index == 4) return 1.134464014;
    if (index == 5) return 1.265363708;
    if (index == 6) return 1.361356817;
    if (index == 7) return 1.439896633;
    if (index == 8) return 1.500983157;
    if (index == 9) return 1.544616389;
    if (index == 10) return 1.570796327;
    if (index == 11) return 1.596976265;
    if (index == 12) return 1.640609496;
    if (index == 13) return 1.701696021;
    if (index == 14) return 1.780235837;
    if (index == 15) return 1.876228945;
    if (index == 16) return 2.007128640;
    if (index == 17) return 2.181661565;
    if (index == 18) return 2.443460953;
    if (index == 19) return 2.748893572;
    return HERITAGE_PI;
}

float tireProbeBandWidth(int index)
{
    // Must match EntityRegistry.hpp::TireVisualProbeWidthCoordinates.
    if (index <= 0) return -1.00;
    if (index == 1) return -0.82;
    if (index == 2) return -0.65;
    if (index == 3) return -0.49;
    if (index == 4) return -0.34;
    if (index == 5) return -0.18;
    if (index == 6) return 0.00;
    if (index == 7) return 0.18;
    if (index == 8) return 0.34;
    if (index == 9) return 0.49;
    if (index == 10) return 0.65;
    if (index == 11) return 0.82;
    return 1.00;
}

float tireProbeStationCoordinateFromPhi(float phi)
{
    float p = clamp(phi, 0.0, HERITAGE_PI);
    for (int i = 0; i < HERITAGE_TIRE_PROBE_STATIONS - 1; ++i)
    {
        float a = tireProbeStationPhi(i);
        float b = tireProbeStationPhi(i + 1);
        if (p <= b || i == HERITAGE_TIRE_PROBE_STATIONS - 2)
            return float(i) + clamp((p - a) / max(b - a, 1.0e-6), 0.0, 1.0);
    }
    return float(HERITAGE_TIRE_PROBE_STATIONS - 1);
}

float tireProbeBandCoordinateFromWidth(float widthCoordinate)
{
    float w = clamp(widthCoordinate, -1.0, 1.0);
    for (int i = 0; i < HERITAGE_TIRE_PROBE_BANDS - 1; ++i)
    {
        float a = tireProbeBandWidth(i);
        float b = tireProbeBandWidth(i + 1);
        if (w <= b || i == HERITAGE_TIRE_PROBE_BANDS - 2)
            return float(i) + clamp((w - a) / max(b - a, 1.0e-6), 0.0, 1.0);
    }
    return float(HERITAGE_TIRE_PROBE_BANDS - 1);
}

float tireProbeGridCompressionM(float phi, float widthCoordinate)
{
    if (!uTireVisualProbeGridValid)
        return 0.0;

    float s = tireProbeStationCoordinateFromPhi(phi);
    float b = tireProbeBandCoordinateFromWidth(widthCoordinate);
    int s0 = min(int(floor(s)), HERITAGE_TIRE_PROBE_STATIONS - 2);
    int b0 = min(int(floor(b)), HERITAGE_TIRE_PROBE_BANDS - 2);
    int s1 = s0 + 1;
    int b1 = b0 + 1;
    float ts = s - float(s0);
    float tb = b - float(b0);
    // TIRE33/VIS26: interpolate the already-relaxed carcass field with a
    // smooth Hermite blend rather than piecewise-linear cell weights. This
    // keeps value continuity AND zeroes the slope at lattice boundaries, so
    // the dense bottom domain reads as one compliant carcass instead of rows
    // of individually grabbed vertices.
    ts = ts * ts * (3.0 - 2.0 * ts);
    tb = tb * tb * (3.0 - 2.0 * tb);
    float c00 = uTireVisualProbeCompressionM[s0 * HERITAGE_TIRE_PROBE_BANDS + b0];
    float c01 = uTireVisualProbeCompressionM[s0 * HERITAGE_TIRE_PROBE_BANDS + b1];
    float c10 = uTireVisualProbeCompressionM[s1 * HERITAGE_TIRE_PROBE_BANDS + b0];
    float c11 = uTireVisualProbeCompressionM[s1 * HERITAGE_TIRE_PROBE_BANDS + b1];
    return mix(mix(c00, c01, tb), mix(c10, c11, tb), ts);
}

float tireEquilibriumCompressionM(float phi, float widthCoordinate)
{
    if (!uTireVisualGrounded || uTireRadialDeflectionM <= 0.0001)
        return 0.0;

    // Must mirror the equilibrium footprint authored by
    // LuaEntitySetMeshNodeTireColliderTrianglesFromWheel. This is the smooth,
    // low-frequency pneumatic load mode; the probe-grid remainder represents
    // only irregular road/kerb detail.
    float referenceRadiusM = max(uTireReferenceRadiusM, 0.05);
    float patchLengthM = uTireContactPatchLengthM > 0.01
        ? uTireContactPatchLengthM : referenceRadiusM * 0.34;
    patchLengthM = clamp(patchLengthM, 0.025, referenceRadiusM * 0.95);
    float patchHalfAngle = clamp(
        0.62 * patchLengthM / referenceRadiusM, 0.075, 0.34);
    float angleFromBottom = abs(phi - 0.5 * HERITAGE_PI);
    if (angleFromBottom >= patchHalfAngle)
        return 0.0;

    float longitudinalT = angleFromBottom / patchHalfAngle;
    float longitudinalWeight = 1.0
        - longitudinalT * longitudinalT * (3.0 - 2.0 * longitudinalT);
    float absWidth = abs(clamp(widthCoordinate, -1.0, 1.0));
    float widthWeight = clamp((1.0 - absWidth) / 0.42, 0.0, 1.0);
    float shoulderWeight = clamp((0.90 - absWidth) / 0.48, 0.0, 1.0);
    float treadWeight = max(widthWeight, 0.42 * shoulderWeight);
    return max(uTireRadialDeflectionM, 0.0)
        * longitudinalWeight * treadWeight;
}

vec4 tireProbeDebugOverlay(vec3 position)
{
    if (!uTireProbeDebugVisible || !uTireVisualEnabled)
        return vec4(0.0);

    vec3 axle = tireAxisVector(uTireVisualAxleAxis);
    vec3 relative = position - uTireVisualCenter;
    float axial = dot(relative, axle);
    vec3 radial = relative - axle * axial;
    float radius = length(radial);
    if (radius <= 0.000001 || uTireVisualOuterRadius <= 0.0001)
        return vec4(0.0);
    vec3 radialDirection = radial / radius;

    mat3 worldToLocal = inverse(mat3(uModel));
    vec3 down = worldToLocal * vec3(0.0, -1.0, 0.0);
    down -= axle * dot(down, axle);
    if (dot(down, down) < 0.000001)
        down = tireRestDown(uTireVisualAxleAxis);
    down = normalize(down);

    vec3 forward = worldToLocal * uTireWheelForwardWorld;
    forward -= axle * dot(forward, axle);
    forward -= down * dot(forward, down);
    if (dot(forward, forward) < 0.000001)
        forward = cross(down, axle);
    forward = normalize(forward);

    vec3 lateral = worldToLocal * uTireWheelRightWorld;
    lateral -= forward * dot(lateral, forward);
    lateral -= down * dot(lateral, down);
    if (dot(lateral, lateral) < 0.000001)
        lateral = axle;
    lateral = normalize(lateral);

    float lower = dot(radialDirection, down);
    if (lower < -0.015)
        return vec4(0.0);

    float alongForward = dot(radialDirection, forward);
    float phi = atan(max(lower, 0.0), alongForward);
    if (phi < 0.0)
        phi += 2.0 * HERITAGE_PI;
    if (phi > HERITAGE_PI + 0.02)
        return vec4(0.0);

    float station = tireProbeStationCoordinateFromPhi(phi);
    float widthCoordinate = clamp(
        dot(relative, lateral) / max(uTireVisualHalfWidth, 0.0001),
        -1.0, 1.0);
    float band = tireProbeBandCoordinateFromWidth(widthCoordinate);

    float nearestStation = floor(station + 0.5);
    float nearestBand = floor(band + 0.5);
    float stationDistance = abs(station - nearestStation);
    float bandDistance = abs(band - nearestBand);
    float markerDistance = max(stationDistance, bandDistance);
    float markerMask = 1.0 - smoothstep(0.12, 0.24, markerDistance);

    float stationLine = 1.0 - smoothstep(0.025, 0.075, stationDistance);
    float bandLine = 1.0 - smoothstep(0.025, 0.075, bandDistance);
    float gridMask = max(stationLine, bandLine) * 0.23;

    int stationIndex = clamp(
        int(nearestStation), 0, HERITAGE_TIRE_PROBE_STATIONS - 1);
    int bandIndex = clamp(
        int(nearestBand), 0, HERITAGE_TIRE_PROBE_BANDS - 1);
    float compressionM = uTireVisualProbeGridValid
        ? uTireVisualProbeCompressionM[
            stationIndex * HERITAGE_TIRE_PROBE_BANDS + bandIndex]
        : 0.0;

    vec3 inactiveColor = vec3(0.38, 0.42, 0.46);
    vec3 shallowColor = vec3(0.05, 1.00, 0.15);
    vec3 mediumColor = vec3(1.00, 0.86, 0.05);
    vec3 deepColor = vec3(1.00, 0.05, 0.02);
    vec3 probeColor = inactiveColor;
    if (compressionM > 0.00010)
    {
        float mediumBlend = smoothstep(0.004, 0.012, compressionM);
        float deepBlend = smoothstep(0.016, 0.030, compressionM);
        probeColor = mix(shallowColor, mediumColor, mediumBlend);
        probeColor = mix(probeColor, deepColor, deepBlend);
    }

    vec3 gridColor = vec3(0.10, 0.75, 1.00);
    vec3 color = mix(gridColor, probeColor, markerMask);
    float mask = max(gridMask, markerMask * 0.95);
    return vec4(color, clamp(mask, 0.0, 1.0));
}

void applyTireProbeGridConstraint(inout vec3 position)
{
    if (!uTireVisualProbeGridValid)
        return;

    vec3 axle = tireAxisVector(uTireVisualAxleAxis);
    vec3 relative = position - uTireVisualCenter;
    float axial = dot(relative, axle);
    vec3 radial = relative - axle * axial;
    float radius = length(radial);
    float radialSpan = max(
        uTireVisualOuterRadius - uTireVisualInnerRadius, 0.0001);
    if (radius <= 0.000001 || uTireVisualOuterRadius <= 0.0001)
        return;
    vec3 radialDirection = radial / radius;

    // Reconstruct the world-anchored rolling basis in the current spinning
    // tire-node frame. This keeps the bottom contact at the bottom while tire
    // material rotates through it; the dent never spins around with the mesh.
    mat3 worldToLocal = inverse(mat3(uModel));
    vec3 worldDown = vec3(0.0, -1.0, 0.0);
    vec3 down = worldToLocal * worldDown;
    down -= axle * dot(down, axle);
    if (dot(down, down) < 0.000001)
        down = tireRestDown(uTireVisualAxleAxis);
    down = normalize(down);

    vec3 forward = worldToLocal * uTireWheelForwardWorld;
    forward -= axle * dot(forward, axle);
    forward -= down * dot(forward, down);
    if (dot(forward, forward) < 0.000001)
        forward = cross(down, axle);
    forward = normalize(forward);

    vec3 lateral = worldToLocal * uTireWheelRightWorld;
    lateral -= forward * dot(lateral, forward);
    lateral -= down * dot(lateral, down);
    if (dot(lateral, lateral) < 0.000001)
        lateral = axle;
    lateral = normalize(lateral);

    float lower = dot(radialDirection, down);
    if (lower < -0.015)
        return; // no expensive/contact deformation on the upper half

    float alongForward = dot(radialDirection, forward);
    float phi = atan(max(lower, 0.0), alongForward);
    if (phi < 0.0)
        phi += 2.0 * HERITAGE_PI;
    if (phi > HERITAGE_PI + 0.02)
        return;
    float widthCoordinate = clamp(
        dot(relative, lateral) / max(uTireVisualHalfWidth, 0.0001),
        -1.0, 1.0);
    // TIRE35/VIS28: the native radial-deflection/contact-patch state already
    // drives the broad lower-carcass equilibrium in applyTireVisualDeformation.
    // Consume only compression above that equilibrium here, otherwise ordinary
    // vehicle weight is applied twice and pinches a few probe-adjacent vertices.
    float compressionM = max(
        tireProbeGridCompressionM(phi, widthCoordinate)
            - tireEquilibriumCompressionM(phi, widthCoordinate),
        0.0);
    if (compressionM <= 0.00005)
        return;

    float radialFraction = clamp(
        (radius - uTireVisualInnerRadius) / radialSpan, 0.0, 1.0);
    // The bead is attached to the rim; tread/shoulder/sidewall progressively
    // acquire the contact displacement. This is deliberately a deformation
    // field rather than a hard per-vertex projection.
    float beadAnchorMask = smoothstep(0.06, 0.34, radialFraction);
    float absWidth = abs(widthCoordinate);
    float bottomness = clamp(lower, 0.0, 1.0);
    float sideWeight = clamp((absWidth - 0.42) / 0.58, 0.0, 1.0);
    float sideBlend = sideWeight * sideWeight * 0.86;
    vec3 sideDirection = lateral * (widthCoordinate >= 0.0 ? 1.0 : -1.0);

    // TIRE31/VIS24 shape-preserving carcass direction.  TIRE30 fixed the width
    // routing and exposed the remaining geometry problem: bottom sidewall bands
    // were still being pulled laterally toward the wheel centre, which pinched the
    // section into a narrow U and could fold it through itself.  A road/kerb load
    // near straight-down must primarily shorten the tire radially; only contacts
    // approaching the front/rear equator are allowed to become strongly lateral.
    float bottomRadialAuthority = smoothstep(0.22, 0.88, bottomness);
    float lateralContactBlend = sideBlend * (1.0 - bottomRadialAuthority);
    vec3 deformationNormal = normalize(
        radialDirection * (1.0 - lateralContactBlend)
        + sideDirection * lateralContactBlend);

    float metersToLocal = uTireVisualOuterRadius
        / max(uTireReferenceRadiusM, 0.02);
    float compressionLocal = compressionM * metersToLocal * beadAnchorMask;

    // Strong non-inversion guard: preserve a finite rubber section outside the
    // bead/rim core and never allow a lateral contact to cross the tire centre.
    float radialComponent = max(dot(deformationNormal, radialDirection), 0.0);
    float lateralComponent = abs(dot(deformationNormal, lateral));
    float radialCoreRadius = uTireVisualInnerRadius + radialSpan * 0.14;
    float radialRoom = max(radius - radialCoreRadius, 0.0);
    float lateralCoordinate = abs(dot(relative, lateral));
    float lateralProtectedCore = uTireVisualHalfWidth * 0.22;
    float lateralRoom = max(lateralCoordinate - lateralProtectedCore, 0.0) * 0.72;
    float geometricLimit = 1.0e6;
    if (radialComponent > 0.001)
        geometricLimit = min(geometricLimit, radialRoom / radialComponent);
    if (lateralComponent > 0.001)
        geometricLimit = min(geometricLimit, lateralRoom / lateralComponent);
    compressionLocal = min(compressionLocal, max(geometricLimit, 0.0));
    position -= deformationNormal * compressionLocal;

    // Pressure/carcass response for the loaded bottom.  The centre tread load
    // shortens the section and the lower sidewall is allowed to move OUTWARD, not
    // inward.  This is deliberately modest and is suppressed near equator-style
    // side contacts, where direct obstacle compression should remain authoritative.
    float centreSupportM = tireProbeGridCompressionM(phi, 0.0);
    float sidewallShapeMask = smoothstep(0.48, 0.68, absWidth)
        * (1.0 - smoothstep(0.90, 1.01, absWidth));
    float pressureBulgeMask = sidewallShapeMask
        * smoothstep(0.30, 0.92, bottomness)
        * beadAnchorMask
        * (1.0 - lateralContactBlend);
    float bulgeLocal = min(centreSupportM * metersToLocal * 0.18,
        uTireVisualHalfWidth * 0.075) * pressureBulgeMask;
    position += sideDirection * bulgeLocal;
}

void applyTireExactColliderConstraint(inout vec3 position)
{
    // Legacy no-op: TIRE27 dense probe lattice is the sole irregular visual authority.
}




void applyTireVisualDeformation(inout vec3 position, inout vec3 normal)
{
    if (!uTireVisualEnabled)
        return;

    vec3 axle = tireAxisVector(uTireVisualAxleAxis);
    vec3 relative = position - uTireVisualCenter;
    float axial = dot(relative, axle);
    vec3 radial = relative - axle * axial;
    float radius = length(radial);
    float radialSpan = max(
        uTireVisualOuterRadius - uTireVisualInnerRadius, 0.0001);
    if (radius <= 0.000001 || uTireVisualOuterRadius <= 0.0001)
        return;

    float metersToLocal = uTireVisualOuterRadius
        / max(uTireReferenceRadiusM, 0.02);
    float radialFraction = clamp(
        (radius - uTireVisualInnerRadius) / radialSpan, 0.0, 1.0);
    float carcassMask = smoothstep(0.04, 0.78, radialFraction);
    float treadRadialMask = smoothstep(0.72, 0.96, radialFraction);
    float axialFraction = abs(axial) / max(uTireVisualHalfWidth, 0.0001);
    float treadWidthMask = 1.0 - smoothstep(0.72, 1.02, axialFraction);

    vec3 worldDown = uTireVisualGrounded
        ? -uTireContactNormalWorld
        : vec3(0.0, -1.0, 0.0);
    vec3 down = inverse(mat3(uModel)) * worldDown;
    down -= axle * dot(down, axle);
    if (dot(down, down) < 0.000001)
        down = tireRestDown(uTireVisualAxleAxis);
    down = normalize(down);

    // Use VehicleSystem's actual wheel basis rather than inferring direction
    // from a possibly mirrored GLB node. Fall back to the geometric basis only
    // for legacy callers that do not yet supply the vectors.
    mat3 worldToLocal = inverse(mat3(uModel));
    vec3 forward = worldToLocal * uTireWheelForwardWorld;
    forward -= down * dot(forward, down);
    if (dot(forward, forward) < 0.000001)
        forward = cross(down, axle);
    forward = normalize(forward);
    vec3 lateral = worldToLocal * uTireWheelRightWorld;
    lateral -= down * dot(lateral, down);
    lateral -= forward * dot(lateral, forward);
    if (dot(lateral, lateral) < 0.000001)
        lateral = axle;
    lateral = normalize(lateral);

    // TIRE24/VIS16: TIRE23 proved this exact shader draws the player's
    // visible tire.  The unconditional proof squash is now removed; all
    // irregular deformation below is driven only by real collider geometry.

    // TIRE32/VIS25: use the actual structural ring displacement solved by
    // physics instead of estimating carcass shear from Fy/Fz * radial deflection.
    // The old shortcut made a normal-pressure road tire look dramatically too
    // soft. The rigid-ring state obeys identified longitudinal/lateral stiffness,
    // pressure/temperature scaling and second-order damping.
    float normalForLegacyShearN = max(uTireNormalForceN, 800.0);
    float legacyLongRatio = clamp(
        uTireLongitudinalForceN / normalForLegacyShearN, -1.20, 1.20);
    float legacyLatRatio = clamp(
        uTireLateralForceN / normalForLegacyShearN, -1.20, 1.20);
    float legacyDeflectionM = max(uTireRadialDeflectionM, 0.0020);
    float legacyMotion = smoothstep(
        0.15, 0.60, max(uTireVisualMotionSpeedMps, 0.0));

    float physicalLongM = uTireRingLongitudinalOffsetM;
    float physicalLatM = uTireRingLateralOffsetM;
    bool physicalLongAvailable = abs(physicalLongM) > 0.00002;
    bool physicalLatAvailable = abs(physicalLatM) > 0.00002;

    float visualRingLongM = physicalLongAvailable
        ? clamp(physicalLongM, -0.014, 0.014)
        : clamp(legacyLongRatio * legacyDeflectionM * 0.18 * legacyMotion,
            -0.006, 0.006);
    float visualRingLatM = physicalLatAvailable
        ? clamp(physicalLatM, -0.014, 0.014)
        : clamp(legacyLatRatio * legacyDeflectionM * 0.20 * legacyMotion,
            -0.006, 0.006);
    float visualRingRadM = clamp(
        uTireRingRadialOffsetM * 0.55, -0.018, 0.018);

    // TIRE34/VIS27 whole-bottom carcass shear. TIRE33's intent was correct,
    // but live testing showed that the radial bead fade left too much of the
    // lower sidewall visually anchored, so the physical rigid-ring displacement
    // still read as a handful of pulled vertices at the contact patch. Define
    // the shear domain from world gravity (the same frame as the 21x13 lower
    // probe lattice), then let essentially the complete lower carcass participate.
    // Only the immediate bead/rim attachment remains strongly constrained.
    vec3 initialRadialDirection = radial / max(radius, 0.000001);
    vec3 carcassDown = worldToLocal * vec3(0.0, -1.0, 0.0);
    carcassDown -= axle * dot(carcassDown, axle);
    if (dot(carcassDown, carcassDown) < 0.000001)
        carcassDown = tireRestDown(uTireVisualAxleAxis);
    carcassDown = normalize(carcassDown);

    float lowerHemisphere = dot(initialRadialDirection, carcassDown);
    // Start coupling slightly above the geometric equator and reach full
    // participation well before the dense-bottom rows. This creates one smooth
    // elastic lower-half mode rather than a narrow bottom patch.
    float lowerCarcassRegion = smoothstep(-0.08, 0.34, lowerHemisphere);
    // The bead itself must stay on the rim, but the sidewall should bend with
    // the displaced belt. Reach near-full participation much earlier through
    // the rubber depth than TIRE33's 0.70 radial threshold.
    float beadToCarcass = smoothstep(0.015, 0.42, radialFraction);
    float carcassShearMask = lowerCarcassRegion * beadToCarcass;

    // Add a broad shape-preserving floor through the lower sidewall. It keeps
    // the bead anchored (radialFraction == 0) while preventing mid-sidewall
    // vertices from lagging far behind the tread and producing a visible pinch.
    float lowerSidewallFlex = lowerCarcassRegion
        * smoothstep(0.04, 0.30, radialFraction)
        * (1.0 - smoothstep(0.86, 1.0, radialFraction));
    carcassShearMask = max(carcassShearMask, lowerSidewallFlex * 0.78);

    vec3 ringTranslation = metersToLocal * (
        forward * visualRingLongM + lateral * visualRingLatM);
    position += ringTranslation * carcassShearMask;
    // Radial ring displacement follows the same gravity-defined lower carcass
    // envelope so it cannot create a separate narrow bottom hinge.
    position += carcassDown * (
        metersToLocal * visualRingRadM * beadToCarcass * lowerCarcassRegion);

    // Circumferential wind-up is visually represented as belt/tread twist
    // relative to the bead. Yaw is deliberately much weaker here: the real
    // structural state remains in physics and presentation must not cartoon it.
    relative = position - uTireVisualCenter;
    float windup = radians(uTireRingWindupDegrees) * carcassMask;
    relative = rotateAroundAxis(relative, axle, windup);
    float yaw = radians(uTireRingYawDegrees) * carcassMask * 0.35;
    relative = rotateAroundAxis(relative, down, yaw);
    position = uTireVisualCenter + relative;

    // TIRE08 wear is material-fixed. Sector zero is the authored rest-pose
    // bottom; the GLB wheel-node spin rotates this dent around with the tire.
    relative = position - uTireVisualCenter;
    axial = dot(relative, axle);
    radial = relative - axle * axial;
    radius = max(length(radial), 0.000001);
    vec3 radialDirection = radial / radius;
    vec3 restDown = tireRestDown(uTireVisualAxleAxis);
    vec3 restForward = normalize(cross(restDown, axle));
    float sectorAngle = 2.0 * HERITAGE_PI
        * (uTireFlatSpotSector / 16.0);
    vec3 flatDirection = normalize(
        restDown * cos(sectorAngle) + restForward * sin(sectorAngle));
    float flatAngularMask = smoothstep(
        cos(HERITAGE_PI / 8.0), 1.0,
        dot(radialDirection, flatDirection));
    float flatDepth = max(uTireFlatSpotDepthM, 0.0) * metersToLocal;
    float flatMask = flatAngularMask * treadRadialMask * treadWidthMask;
    position -= radialDirection * flatDepth * flatMask;

    // Loaded footprint: clamp only the road-facing outer tread to the loaded
    // plane. This makes a real flat contact patch instead of vertically
    // squashing the complete wheel/rim assembly.
    relative = position - uTireVisualCenter;
    axial = dot(relative, axle);
    radial = relative - axle * axial;
    radius = max(length(radial), 0.000001);
    radialDirection = radial / radius;
    float radialFractionAfter = clamp(
        (radius - uTireVisualInnerRadius) / radialSpan, 0.0, 1.0);
    treadRadialMask = smoothstep(0.72, 0.96, radialFractionAfter);
    float contactHalfWidth = min(
        max(uTireContactPatchWidthM * metersToLocal * 0.5, 0.0),
        uTireVisualHalfWidth);
    float contactWidthMask = contactHalfWidth > 0.0001
        ? 1.0 - smoothstep(
            contactHalfWidth * 0.92,
            min(uTireVisualHalfWidth, contactHalfWidth * 1.12 + 0.0001),
            abs(axial))
        : treadWidthMask;
    // The broad pneumatic equilibrium must remain active when the detailed
    // probe lattice is valid. The old !uTireVisualProbeGridValid branch disabled
    // this entire shape on the live path and left only local vertex dents.
    float deflection = uTireVisualGrounded
        ? max(uTireRadialDeflectionM, 0.0) * metersToLocal
        : 0.0;
    float contactLengthLocal =
        max(uTireContactPatchLengthM, 0.0) * metersToLocal;
    float contactHalfLength = contactLengthLocal * 0.5;
    float longitudinalSigned = dot(radial, forward);
    float longitudinalCoordinate = abs(longitudinalSigned);
    float contactLengthMask = contactHalfLength > 0.0001
        ? 1.0 - smoothstep(
            contactHalfLength * 0.92,
            contactHalfLength * 1.12 + 0.0001,
            longitudinalCoordinate)
        : 1.0;
    float loadedPlane = uTireVisualOuterRadius - deflection;
    if (uTireVisualGrounded && !uTireVisualProbeGridValid
        && uTireContactPlaneDistanceM > 0.02)
    {
        // A measured centre-to-plane value may demand extra compression, but
        // must never erase the radial deflection already solved by physics.
        float measuredPlane = clamp(
            uTireContactPlaneDistanceM * metersToLocal,
            uTireVisualInnerRadius + 0.0001,
            uTireVisualOuterRadius);
        loadedPlane = min(loadedPlane, measuredPlane);
    }
    float centerLoadedPlane = loadedPlane;
    if (uTireVisualGrounded && !uTireVisualProbeGridValid
        && uTireVisualSupportGridValid)
    {
        float longitudinalM = longitudinalSigned / max(metersToLocal, 0.0001);
        // Support lateral coordinates are defined by VehicleSystem wheel-right,
        // not by the authored GLB axle sign. This matters on mirrored wheels.
        float lateralM = dot(relative, lateral) / max(metersToLocal, 0.0001);
        float residualM = tireSupportResidualM(longitudinalM, lateralM);
        float localConformLimitM = min(
            0.045, max(0.015, uTireRadialDeflectionM * 1.75 + 0.008));
        residualM = clamp(residualM, -localConformLimitM, localConformLimitM);
        loadedPlane = clamp(
            loadedPlane - residualM * metersToLocal,
            uTireVisualInnerRadius + 0.0001,
            uTireVisualOuterRadius);

        float heightAboveCenterPlaneM = max(
            (centerLoadedPlane - dot(radial, down))
                / max(metersToLocal, 0.0001),
            0.0);
        if (!uTireVisualProbeGridValid)
        {
            position += tireCurbFaceCompressionLocal(
                longitudinalM, lateralM, heightAboveCenterPlaneM,
                radialFractionAfter, dot(radialDirection, down),
                forward, lateral, metersToLocal);
        }
        relative = position - uTireVisualCenter;
        axial = dot(relative, axle);
        radial = relative - axle * axial;
        radius = max(length(radial), 0.000001);
        radialDirection = radial / radius;
    }
    float groundCoordinate = dot(radial, down);
    float flattenAmount = max(groundCoordinate - loadedPlane, 0.0)
        * treadRadialMask * contactWidthMask * contactLengthMask;
    position -= down * flattenAmount;

    // Sidewall bulge follows actual tire deflection, peaks between bead and
    // shoulder, and remains almost zero at the bead/rim interface. The same
    // authoritative finite contact length bounds both tread flattening and
    // the surrounding sidewall influence.
    float halfPatchAngle = asin(clamp(
        contactLengthLocal / max(2.0 * uTireVisualOuterRadius, 0.0001),
        0.0, 0.95));
    float influenceAngle = max(halfPatchAngle * 2.8, 0.45);
    float localFootprintEnvelope = smoothstep(
        cos(influenceAngle), 1.0, dot(radialDirection, down));
    // A pressurised carcass transmits the contact load through the complete
    // lower sidewall/belt structure. This gravity-anchored, low-order mode begins
    // just above the equator and rises smoothly toward the footprint, independent
    // of visual mesh tessellation. It is deliberately broad, not a soft-body mesh.
    float loadedLowerHemisphere = dot(radialDirection, down);
    float wholeLowerCarcassEnvelope = smoothstep(
        -0.10, 0.92, loadedLowerHemisphere);
    float bottomMask = max(
        localFootprintEnvelope, wholeLowerCarcassEnvelope * 0.78);
    float sidewallRadialMask = smoothstep(0.08, 0.28, radialFractionAfter)
        * (1.0 - smoothstep(0.76, 0.96, radialFractionAfter));
    float sidewallAxialMask = smoothstep(0.48, 0.90,
        abs(axial) / max(uTireVisualHalfWidth, 0.0001));
    // Radial carcass compression affects more than the exact tread vertices.
    // The lower sidewall rises slightly as the footprint forms while its sides
    // bulge outward. This makes normal road-tire deflection readable in motion
    // without moving the bead/rim interface.
    float lowerCarcassMask = bottomMask
        * smoothstep(0.035, 0.52, radialFractionAfter)
        * (1.0 - treadRadialMask * 0.35);
    position -= down * (deflection * 0.48 * lowerCarcassMask);

    float bulge = deflection * 0.46
        * bottomMask * sidewallRadialMask * sidewallAxialMask;
    if (abs(axial) > 0.000001)
        position += axle * sign(axial) * bulge;

    // TIRE17C6R/VIS09 final flat-road non-penetration safety net. The 3x3
    // support field and curb-face approximation remain responsible for local
    // shape, but road-facing rubber is never allowed below the authoritative
    // centre contact plane. This is deliberately presentation-only.
    if (uTireVisualGrounded && !uTireVisualProbeGridValid
        && uTireContactPlaneDistanceM > 0.02)
    {
        relative = position - uTireVisualCenter;
        axial = dot(relative, axle);
        radial = relative - axle * axial;
        radius = max(length(radial), 0.000001);
        radialDirection = radial / radius;
        float radialFractionFinal = clamp(
            (radius - uTireVisualInnerRadius) / radialSpan, 0.0, 1.0);
        float roadFacingFinal = dot(radialDirection, down);
        float rubberPlaneMask = smoothstep(0.08, 0.34, radialFractionFinal)
            * smoothstep(0.02, 0.40, roadFacingFinal);
        float hardPlaneDistanceLocal = uTireContactPlaneDistanceM * metersToLocal;
        float hardPlanePenetration = max(
            dot(radial, down) - hardPlaneDistanceLocal, 0.0);
        position -= down * hardPlanePenetration * rubberPlaneMask;
    }

    // TIRE27/VIS20: the bottom-biased 21x13 CollisionSystem probe lattice is the sole
    // irregular-contact visual authority. TIRE23 proved this is the live draw
    // path; TIRE24/25 proved absolute triangle bridges were not trustworthy.
    applyTireProbeGridConstraint(position);

    // Keep lighting consistent with both the flattened tread and the bending
    // sidewall. The tangential tilt is deliberately subtle; position remains
    // the authoritative visual signal.
    float normalFlatten = clamp(
        flattenAmount / max(deflection, 0.0001), 0.0, 1.0);
    normal = normalize(mix(normal, -down, normalFlatten * 0.85));
    vec3 tangentialShearLocal = metersToLocal * (
        forward * visualRingLongM + lateral * visualRingLatM);
    float shearSlope = clamp(
        length(tangentialShearLocal) / max(radialSpan, 0.0001), 0.0, 0.35);
    vec3 shearDirection = length(tangentialShearLocal) > 0.000001
        ? normalize(tangentialShearLocal) : vec3(0.0);
    normal = normalize(normal - shearDirection
        * shearSlope * sidewallRadialMask * 0.45);
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
    vec3 deformedPosition = localPosition.xyz;
    applyTireVisualDeformation(deformedPosition, localNormal);
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

void main()
{
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

    float roughness = clamp(uMaterialRoughness, 0.04, 1.0);
    if (uHasRoughnessMap)
        roughness = clamp(
            sampleChannel(texture(uRoughnessMap, vTexCoord), uRoughnessChannel),
            0.04,
            1.0);

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
uniform bool uTireVisualGrounded;
uniform vec3 uTireVisualCenter;
uniform int uTireVisualAxleAxis;
uniform float uTireVisualHalfWidth;
uniform float uTireVisualInnerRadius;
uniform float uTireVisualOuterRadius;
uniform float uTireReferenceRadiusM;
uniform float uTireRadialDeflectionM;
uniform float uTireContactPatchLengthM;
uniform float uTireContactPatchWidthM;
uniform float uTireRingRadialOffsetM;
uniform float uTireRingLongitudinalOffsetM;
uniform float uTireRingLateralOffsetM;
uniform float uTireRingYawDegrees;
uniform float uTireRingWindupDegrees;
uniform float uTireFlatSpotDepthM;
uniform float uTireFlatSpotSector;
// TIRE10/VIS02 authoritative native contact plane.
uniform vec3 uTireContactNormalWorld;
// TIRE17C2/VIS04 authoritative physics wheel basis. These directions remove
// mirrored-node ambiguity for longitudinal braking shear and lateral cornering
// carcass deformation.
uniform vec3 uTireWheelForwardWorld;
uniform vec3 uTireWheelRightWorld;
uniform float uTireNormalForceN;
uniform float uTireLongitudinalForceN;
uniform float uTireLateralForceN;
uniform float uTireVisualMotionSpeedMps;
uniform float uTireContactPlaneDistanceM;
// TIRE17C1/VIS03 refined curb/step support residuals.
uniform bool uTireVisualSupportGridValid;
uniform float uTireVisualSupportHalfLengthM;
uniform float uTireVisualSupportHalfWidthM;
uniform float uTireVisualSupportHeightResidualM[9];
// TIRE27/VIS20 authoritative render-facing lower-shell probe lattice.
// 21 non-uniform stations: sparse near the equators, deliberately dense around
// straight-down. 13 width bands resolve centre tread, shoulders and sidewalls.
const int HERITAGE_TIRE_PROBE_STATIONS = 21;
const int HERITAGE_TIRE_PROBE_BANDS = 13;
const int HERITAGE_TIRE_PROBE_COUNT = 273;
uniform bool uTireVisualProbeGridValid;
uniform float uTireVisualProbeCompressionM[HERITAGE_TIRE_PROBE_COUNT];
// TIRE27: legacy exact-triangle visual uniforms removed; dense probe lattice owns contact deformation.

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

float tireSupportResidualM(float longitudinalM, float lateralM)
{
    if (!uTireVisualSupportGridValid
        || uTireVisualSupportHalfLengthM <= 0.005
        || uTireVisualSupportHalfWidthM <= 0.005)
        return 0.0;

    float gridX = clamp(
        longitudinalM / uTireVisualSupportHalfLengthM + 1.0, 0.0, 2.0);
    float gridY = clamp(
        lateralM / uTireVisualSupportHalfWidthM + 1.0, 0.0, 2.0);
    int x0 = min(int(floor(gridX)), 1);
    int y0 = min(int(floor(gridY)), 1);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    float tx = gridX - float(x0);
    float ty = gridY - float(y0);
    float h00 = uTireVisualSupportHeightResidualM[y0 * 3 + x0];
    float h10 = uTireVisualSupportHeightResidualM[y0 * 3 + x1];
    float h01 = uTireVisualSupportHeightResidualM[y1 * 3 + x0];
    float h11 = uTireVisualSupportHeightResidualM[y1 * 3 + x1];
    return mix(mix(h00, h10, tx), mix(h01, h11, tx), ty);
}

vec3 tireCurbFaceCompressionLocal(
    float longitudinalM,
    float lateralM,
    float heightAboveCenterPlaneM,
    float radialFraction,
    float roadFacing,
    vec3 forward,
    vec3 lateral,
    float metersToLocal)
{
    if (!uTireVisualSupportGridValid)
        return vec3(0.0);

    float hLatNeg = tireSupportResidualM(
        longitudinalM, -uTireVisualSupportHalfWidthM);
    float hLatMid = tireSupportResidualM(longitudinalM, 0.0);
    float hLatPos = tireSupportResidualM(
        longitudinalM, uTireVisualSupportHalfWidthM);
    float dLatNeg = hLatMid - hLatNeg;
    float dLatPos = hLatPos - hLatMid;
    bool useLatNeg = abs(dLatNeg) >= abs(dLatPos);
    float lateralStepM = useLatNeg ? dLatNeg : dLatPos;
    float lateralEdgeM = (useLatNeg ? -0.5 : 0.5)
        * uTireVisualSupportHalfWidthM;

    float hLongNeg = tireSupportResidualM(
        -uTireVisualSupportHalfLengthM, lateralM);
    float hLongMid = tireSupportResidualM(0.0, lateralM);
    float hLongPos = tireSupportResidualM(
        uTireVisualSupportHalfLengthM, lateralM);
    float dLongNeg = hLongMid - hLongNeg;
    float dLongPos = hLongPos - hLongMid;
    bool useLongNeg = abs(dLongNeg) >= abs(dLongPos);
    float longitudinalStepM = useLongNeg ? dLongNeg : dLongPos;
    float longitudinalEdgeM = (useLongNeg ? -0.5 : 0.5)
        * uTireVisualSupportHalfLengthM;

    bool useLateral = abs(lateralStepM) >= abs(longitudinalStepM);
    float stepSignedM = useLateral ? lateralStepM : longitudinalStepM;
    float stepM = abs(stepSignedM);
    if (stepM <= 0.008)
        return vec3(0.0);

    float highSign = stepSignedM >= 0.0 ? 1.0 : -1.0;
    float coordinateM = useLateral ? lateralM : longitudinalM;
    float edgeM = useLateral ? lateralEdgeM : longitudinalEdgeM;
    vec3 axis = useLateral ? lateral : forward;

    float gridMinM = uTireVisualSupportHeightResidualM[0];
    for (int supportIndex = 1; supportIndex < 9; ++supportIndex)
        gridMinM = min(gridMinM, uTireVisualSupportHeightResidualM[supportIndex]);
    float heightAboveLowSupportM = max(heightAboveCenterPlaneM - gridMinM, 0.0);
    float belowTop = 1.0 - smoothstep(
        max(stepM * 0.78, 0.006),
        max(stepM * 1.05, 0.010),
        heightAboveLowSupportM);

    float penetrationM = highSign * (coordinateM - edgeM);
    float penetrationMask = smoothstep(
        0.0015, max(0.010, stepM * 0.35), penetrationM);
    float structuralMask = smoothstep(0.18, 0.82, radialFraction)
        * smoothstep(-0.18, 0.88, roadFacing);
    float activation = smoothstep(0.010, 0.030, stepM)
        * belowTop * penetrationMask * structuralMask;
    float compressionM = min(
        max(penetrationM, 0.0) * 0.82 + stepM * 0.06,
        0.032);
    return -axis * (highSign * compressionM * metersToLocal * activation);
}

vec3 tireClosestPointOnTriangle(vec3 p, vec3 a, vec3 b, vec3 c)
{
    // Ericson-style finite-triangle closest point. The deformation pass must
    // not treat creator triangles as infinite planes; otherwise a kerb face
    // can distort rubber well beyond its real edge.
    vec3 ab = b - a;
    vec3 ac = c - a;
    vec3 ap = p - a;
    float d1 = dot(ab, ap);
    float d2 = dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0)
        return a;

    vec3 bp = p - b;
    float d3 = dot(ab, bp);
    float d4 = dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3)
        return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
    {
        float v = d1 / max(d1 - d3, 1.0e-12);
        return a + v * ab;
    }

    vec3 cp = p - c;
    float d5 = dot(ab, cp);
    float d6 = dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6)
        return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
    {
        float w = d2 / max(d2 - d6, 1.0e-12);
        return a + w * ac;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
    {
        vec3 bc = c - b;
        float w = (d4 - d3) / max((d4 - d3) + (d5 - d6), 1.0e-12);
        return b + w * bc;
    }

    float denominator = va + vb + vc;
    if (abs(denominator) <= 1.0e-12)
        return a;
    float inverseDenominator = 1.0 / denominator;
    float v = vb * inverseDenominator;
    float w = vc * inverseDenominator;
    return a + ab * v + ac * w;
}


float tireProbeStationPhi(int index)
{
    // Must match EntityRegistry.hpp::TireVisualProbeStationPhiRadians.
    if (index <= 0) return 0.0;
    if (index == 1) return 0.392699082;
    if (index == 2) return 0.698131701;
    if (index == 3) return 0.959931089;
    if (index == 4) return 1.134464014;
    if (index == 5) return 1.265363708;
    if (index == 6) return 1.361356817;
    if (index == 7) return 1.439896633;
    if (index == 8) return 1.500983157;
    if (index == 9) return 1.544616389;
    if (index == 10) return 1.570796327;
    if (index == 11) return 1.596976265;
    if (index == 12) return 1.640609496;
    if (index == 13) return 1.701696021;
    if (index == 14) return 1.780235837;
    if (index == 15) return 1.876228945;
    if (index == 16) return 2.007128640;
    if (index == 17) return 2.181661565;
    if (index == 18) return 2.443460953;
    if (index == 19) return 2.748893572;
    return HERITAGE_PI;
}

float tireProbeBandWidth(int index)
{
    // Must match EntityRegistry.hpp::TireVisualProbeWidthCoordinates.
    if (index <= 0) return -1.00;
    if (index == 1) return -0.82;
    if (index == 2) return -0.65;
    if (index == 3) return -0.49;
    if (index == 4) return -0.34;
    if (index == 5) return -0.18;
    if (index == 6) return 0.00;
    if (index == 7) return 0.18;
    if (index == 8) return 0.34;
    if (index == 9) return 0.49;
    if (index == 10) return 0.65;
    if (index == 11) return 0.82;
    return 1.00;
}

float tireProbeStationCoordinateFromPhi(float phi)
{
    float p = clamp(phi, 0.0, HERITAGE_PI);
    for (int i = 0; i < HERITAGE_TIRE_PROBE_STATIONS - 1; ++i)
    {
        float a = tireProbeStationPhi(i);
        float b = tireProbeStationPhi(i + 1);
        if (p <= b || i == HERITAGE_TIRE_PROBE_STATIONS - 2)
            return float(i) + clamp((p - a) / max(b - a, 1.0e-6), 0.0, 1.0);
    }
    return float(HERITAGE_TIRE_PROBE_STATIONS - 1);
}

float tireProbeBandCoordinateFromWidth(float widthCoordinate)
{
    float w = clamp(widthCoordinate, -1.0, 1.0);
    for (int i = 0; i < HERITAGE_TIRE_PROBE_BANDS - 1; ++i)
    {
        float a = tireProbeBandWidth(i);
        float b = tireProbeBandWidth(i + 1);
        if (w <= b || i == HERITAGE_TIRE_PROBE_BANDS - 2)
            return float(i) + clamp((w - a) / max(b - a, 1.0e-6), 0.0, 1.0);
    }
    return float(HERITAGE_TIRE_PROBE_BANDS - 1);
}

float tireProbeGridCompressionM(float phi, float widthCoordinate)
{
    if (!uTireVisualProbeGridValid)
        return 0.0;

    float s = tireProbeStationCoordinateFromPhi(phi);
    float b = tireProbeBandCoordinateFromWidth(widthCoordinate);
    int s0 = min(int(floor(s)), HERITAGE_TIRE_PROBE_STATIONS - 2);
    int b0 = min(int(floor(b)), HERITAGE_TIRE_PROBE_BANDS - 2);
    int s1 = s0 + 1;
    int b1 = b0 + 1;
    float ts = s - float(s0);
    float tb = b - float(b0);
    // TIRE33/VIS26: interpolate the already-relaxed carcass field with a
    // smooth Hermite blend rather than piecewise-linear cell weights. This
    // keeps value continuity AND zeroes the slope at lattice boundaries, so
    // the dense bottom domain reads as one compliant carcass instead of rows
    // of individually grabbed vertices.
    ts = ts * ts * (3.0 - 2.0 * ts);
    tb = tb * tb * (3.0 - 2.0 * tb);
    float c00 = uTireVisualProbeCompressionM[s0 * HERITAGE_TIRE_PROBE_BANDS + b0];
    float c01 = uTireVisualProbeCompressionM[s0 * HERITAGE_TIRE_PROBE_BANDS + b1];
    float c10 = uTireVisualProbeCompressionM[s1 * HERITAGE_TIRE_PROBE_BANDS + b0];
    float c11 = uTireVisualProbeCompressionM[s1 * HERITAGE_TIRE_PROBE_BANDS + b1];
    return mix(mix(c00, c01, tb), mix(c10, c11, tb), ts);
}

float tireEquilibriumCompressionM(float phi, float widthCoordinate)
{
    if (!uTireVisualGrounded || uTireRadialDeflectionM <= 0.0001)
        return 0.0;

    float referenceRadiusM = max(uTireReferenceRadiusM, 0.05);
    float patchLengthM = uTireContactPatchLengthM > 0.01
        ? uTireContactPatchLengthM : referenceRadiusM * 0.34;
    patchLengthM = clamp(patchLengthM, 0.025, referenceRadiusM * 0.95);
    float patchHalfAngle = clamp(
        0.62 * patchLengthM / referenceRadiusM, 0.075, 0.34);
    float angleFromBottom = abs(phi - 0.5 * HERITAGE_PI);
    if (angleFromBottom >= patchHalfAngle)
        return 0.0;

    float longitudinalT = angleFromBottom / patchHalfAngle;
    float longitudinalWeight = 1.0
        - longitudinalT * longitudinalT * (3.0 - 2.0 * longitudinalT);
    float absWidth = abs(clamp(widthCoordinate, -1.0, 1.0));
    float widthWeight = clamp((1.0 - absWidth) / 0.42, 0.0, 1.0);
    float shoulderWeight = clamp((0.90 - absWidth) / 0.48, 0.0, 1.0);
    float treadWeight = max(widthWeight, 0.42 * shoulderWeight);
    return max(uTireRadialDeflectionM, 0.0)
        * longitudinalWeight * treadWeight;
}

void applyTireProbeGridConstraint(inout vec3 position)
{
    if (!uTireVisualProbeGridValid)
        return;

    vec3 axle = tireAxisVector(uTireVisualAxleAxis);
    vec3 relative = position - uTireVisualCenter;
    float axial = dot(relative, axle);
    vec3 radial = relative - axle * axial;
    float radius = length(radial);
    float radialSpan = max(
        uTireVisualOuterRadius - uTireVisualInnerRadius, 0.0001);
    if (radius <= 0.000001 || uTireVisualOuterRadius <= 0.0001)
        return;
    vec3 radialDirection = radial / radius;

    // Reconstruct the world-anchored rolling basis in the current spinning
    // tire-node frame. This keeps the bottom contact at the bottom while tire
    // material rotates through it; the dent never spins around with the mesh.
    mat3 worldToLocal = inverse(mat3(uModel));
    vec3 worldDown = vec3(0.0, -1.0, 0.0);
    vec3 down = worldToLocal * worldDown;
    down -= axle * dot(down, axle);
    if (dot(down, down) < 0.000001)
        down = tireRestDown(uTireVisualAxleAxis);
    down = normalize(down);

    vec3 forward = worldToLocal * uTireWheelForwardWorld;
    forward -= axle * dot(forward, axle);
    forward -= down * dot(forward, down);
    if (dot(forward, forward) < 0.000001)
        forward = cross(down, axle);
    forward = normalize(forward);

    vec3 lateral = worldToLocal * uTireWheelRightWorld;
    lateral -= forward * dot(lateral, forward);
    lateral -= down * dot(lateral, down);
    if (dot(lateral, lateral) < 0.000001)
        lateral = axle;
    lateral = normalize(lateral);

    float lower = dot(radialDirection, down);
    if (lower < -0.015)
        return; // no expensive/contact deformation on the upper half

    float alongForward = dot(radialDirection, forward);
    float phi = atan(max(lower, 0.0), alongForward);
    if (phi < 0.0)
        phi += 2.0 * HERITAGE_PI;
    if (phi > HERITAGE_PI + 0.02)
        return;
    float widthCoordinate = clamp(
        dot(relative, lateral) / max(uTireVisualHalfWidth, 0.0001),
        -1.0, 1.0);
    float compressionM = max(
        tireProbeGridCompressionM(phi, widthCoordinate)
            - tireEquilibriumCompressionM(phi, widthCoordinate),
        0.0);
    if (compressionM <= 0.00005)
        return;

    float radialFraction = clamp(
        (radius - uTireVisualInnerRadius) / radialSpan, 0.0, 1.0);
    // The bead is attached to the rim; tread/shoulder/sidewall progressively
    // acquire the contact displacement. This is deliberately a deformation
    // field rather than a hard per-vertex projection.
    float beadAnchorMask = smoothstep(0.06, 0.34, radialFraction);
    float absWidth = abs(widthCoordinate);
    float bottomness = clamp(lower, 0.0, 1.0);
    float sideWeight = clamp((absWidth - 0.42) / 0.58, 0.0, 1.0);
    float sideBlend = sideWeight * sideWeight * 0.86;
    vec3 sideDirection = lateral * (widthCoordinate >= 0.0 ? 1.0 : -1.0);

    // TIRE31/VIS24 shape-preserving carcass direction.  TIRE30 fixed the width
    // routing and exposed the remaining geometry problem: bottom sidewall bands
    // were still being pulled laterally toward the wheel centre, which pinched the
    // section into a narrow U and could fold it through itself.  A road/kerb load
    // near straight-down must primarily shorten the tire radially; only contacts
    // approaching the front/rear equator are allowed to become strongly lateral.
    float bottomRadialAuthority = smoothstep(0.22, 0.88, bottomness);
    float lateralContactBlend = sideBlend * (1.0 - bottomRadialAuthority);
    vec3 deformationNormal = normalize(
        radialDirection * (1.0 - lateralContactBlend)
        + sideDirection * lateralContactBlend);

    float metersToLocal = uTireVisualOuterRadius
        / max(uTireReferenceRadiusM, 0.02);
    float compressionLocal = compressionM * metersToLocal * beadAnchorMask;

    // Strong non-inversion guard: preserve a finite rubber section outside the
    // bead/rim core and never allow a lateral contact to cross the tire centre.
    float radialComponent = max(dot(deformationNormal, radialDirection), 0.0);
    float lateralComponent = abs(dot(deformationNormal, lateral));
    float radialCoreRadius = uTireVisualInnerRadius + radialSpan * 0.14;
    float radialRoom = max(radius - radialCoreRadius, 0.0);
    float lateralCoordinate = abs(dot(relative, lateral));
    float lateralProtectedCore = uTireVisualHalfWidth * 0.22;
    float lateralRoom = max(lateralCoordinate - lateralProtectedCore, 0.0) * 0.72;
    float geometricLimit = 1.0e6;
    if (radialComponent > 0.001)
        geometricLimit = min(geometricLimit, radialRoom / radialComponent);
    if (lateralComponent > 0.001)
        geometricLimit = min(geometricLimit, lateralRoom / lateralComponent);
    compressionLocal = min(compressionLocal, max(geometricLimit, 0.0));
    position -= deformationNormal * compressionLocal;

    // Pressure/carcass response for the loaded bottom.  The centre tread load
    // shortens the section and the lower sidewall is allowed to move OUTWARD, not
    // inward.  This is deliberately modest and is suppressed near equator-style
    // side contacts, where direct obstacle compression should remain authoritative.
    float centreSupportM = tireProbeGridCompressionM(phi, 0.0);
    float sidewallShapeMask = smoothstep(0.48, 0.68, absWidth)
        * (1.0 - smoothstep(0.90, 1.01, absWidth));
    float pressureBulgeMask = sidewallShapeMask
        * smoothstep(0.30, 0.92, bottomness)
        * beadAnchorMask
        * (1.0 - lateralContactBlend);
    float bulgeLocal = min(centreSupportM * metersToLocal * 0.18,
        uTireVisualHalfWidth * 0.075) * pressureBulgeMask;
    position += sideDirection * bulgeLocal;
}

void applyTireExactColliderConstraint(inout vec3 position)
{
    // Legacy no-op: TIRE27 dense probe lattice is the sole irregular visual authority.
}




vec3 deformTireShadowPosition(vec3 position)
{
    if (!uTireVisualEnabled)
        return position;

    vec3 axle = tireAxisVector(uTireVisualAxleAxis);
    vec3 relative = position - uTireVisualCenter;
    float axial = dot(relative, axle);
    vec3 radial = relative - axle * axial;
    float radius = length(radial);
    float radialSpan = max(
        uTireVisualOuterRadius - uTireVisualInnerRadius, 0.0001);
    if (radius <= 0.000001 || uTireVisualOuterRadius <= 0.0001)
        return position;

    float metersToLocal = uTireVisualOuterRadius
        / max(uTireReferenceRadiusM, 0.02);
    float radialFraction = clamp(
        (radius - uTireVisualInnerRadius) / radialSpan, 0.0, 1.0);
    float carcassMask = smoothstep(0.04, 0.78, radialFraction);
    float treadRadialMask = smoothstep(0.72, 0.96, radialFraction);
    float axialFraction = abs(axial) / max(uTireVisualHalfWidth, 0.0001);
    float treadWidthMask = 1.0 - smoothstep(0.72, 1.02, axialFraction);

    vec3 worldDown = uTireVisualGrounded
        ? -uTireContactNormalWorld
        : vec3(0.0, -1.0, 0.0);
    vec3 down = inverse(mat3(uModel)) * worldDown;
    down -= axle * dot(down, axle);
    if (dot(down, down) < 0.000001)
        down = tireRestDown(uTireVisualAxleAxis);
    down = normalize(down);

    // Use VehicleSystem's actual wheel basis rather than inferring direction
    // from a possibly mirrored GLB node. Fall back to the geometric basis only
    // for legacy callers that do not yet supply the vectors.
    mat3 worldToLocal = inverse(mat3(uModel));
    vec3 forward = worldToLocal * uTireWheelForwardWorld;
    forward -= down * dot(forward, down);
    if (dot(forward, forward) < 0.000001)
        forward = cross(down, axle);
    forward = normalize(forward);
    vec3 lateral = worldToLocal * uTireWheelRightWorld;
    lateral -= down * dot(lateral, down);
    lateral -= forward * dot(lateral, forward);
    if (dot(lateral, lateral) < 0.000001)
        lateral = axle;
    lateral = normalize(lateral);

    // TIRE24/VIS16 shadow path uses the same real contact-kernel constraint;
    // the temporary unconditional TIRE23 proof deformation is removed.

    // TIRE32/VIS25 shadow path mirrors the main physical carcass shear.
    float normalForLegacyShearN = max(uTireNormalForceN, 800.0);
    float legacyLongRatio = clamp(
        uTireLongitudinalForceN / normalForLegacyShearN, -1.20, 1.20);
    float legacyLatRatio = clamp(
        uTireLateralForceN / normalForLegacyShearN, -1.20, 1.20);
    float legacyDeflectionM = max(uTireRadialDeflectionM, 0.0020);
    float legacyMotion = smoothstep(
        0.15, 0.60, max(uTireVisualMotionSpeedMps, 0.0));

    float physicalLongM = uTireRingLongitudinalOffsetM;
    float physicalLatM = uTireRingLateralOffsetM;
    bool physicalLongAvailable = abs(physicalLongM) > 0.00002;
    bool physicalLatAvailable = abs(physicalLatM) > 0.00002;
    float visualRingLongM = physicalLongAvailable
        ? clamp(physicalLongM, -0.014, 0.014)
        : clamp(legacyLongRatio * legacyDeflectionM * 0.18 * legacyMotion,
            -0.006, 0.006);
    float visualRingLatM = physicalLatAvailable
        ? clamp(physicalLatM, -0.014, 0.014)
        : clamp(legacyLatRatio * legacyDeflectionM * 0.20 * legacyMotion,
            -0.006, 0.006);
    float visualRingRadM = clamp(
        uTireRingRadialOffsetM * 0.55, -0.018, 0.018);

    // TIRE34/VIS27 shadow path mirrors the main whole-bottom carcass mode.
    vec3 initialRadialDirection = radial / max(radius, 0.000001);
    vec3 carcassDown = worldToLocal * vec3(0.0, -1.0, 0.0);
    carcassDown -= axle * dot(carcassDown, axle);
    if (dot(carcassDown, carcassDown) < 0.000001)
        carcassDown = tireRestDown(uTireVisualAxleAxis);
    carcassDown = normalize(carcassDown);
    float lowerHemisphere = dot(initialRadialDirection, carcassDown);
    float lowerCarcassRegion = smoothstep(-0.08, 0.34, lowerHemisphere);
    float beadToCarcass = smoothstep(0.015, 0.42, radialFraction);
    float carcassShearMask = lowerCarcassRegion * beadToCarcass;
    float lowerSidewallFlex = lowerCarcassRegion
        * smoothstep(0.04, 0.30, radialFraction)
        * (1.0 - smoothstep(0.86, 1.0, radialFraction));
    carcassShearMask = max(carcassShearMask, lowerSidewallFlex * 0.78);
    position += metersToLocal * (
        forward * visualRingLongM + lateral * visualRingLatM)
        * carcassShearMask;
    position += carcassDown * (
        metersToLocal * visualRingRadM * beadToCarcass * lowerCarcassRegion);

    relative = position - uTireVisualCenter;
    relative = rotateAroundAxis(
        relative, axle, radians(uTireRingWindupDegrees) * carcassMask);
    relative = rotateAroundAxis(
        relative, down, radians(uTireRingYawDegrees) * carcassMask * 0.35);
    position = uTireVisualCenter + relative;

    relative = position - uTireVisualCenter;
    axial = dot(relative, axle);
    radial = relative - axle * axial;
    radius = max(length(radial), 0.000001);
    vec3 radialDirection = radial / radius;
    vec3 restDown = tireRestDown(uTireVisualAxleAxis);
    vec3 restForward = normalize(cross(restDown, axle));
    float sectorAngle = 2.0 * HERITAGE_PI
        * (uTireFlatSpotSector / 16.0);
    vec3 flatDirection = normalize(
        restDown * cos(sectorAngle) + restForward * sin(sectorAngle));
    float flatAngularMask = smoothstep(
        cos(HERITAGE_PI / 8.0), 1.0,
        dot(radialDirection, flatDirection));
    position -= radialDirection
        * (max(uTireFlatSpotDepthM, 0.0) * metersToLocal)
        * flatAngularMask * treadRadialMask * treadWidthMask;

    relative = position - uTireVisualCenter;
    axial = dot(relative, axle);
    radial = relative - axle * axial;
    radius = max(length(radial), 0.000001);
    radialDirection = radial / radius;
    float radialFractionAfter = clamp(
        (radius - uTireVisualInnerRadius) / radialSpan, 0.0, 1.0);
    treadRadialMask = smoothstep(0.72, 0.96, radialFractionAfter);
    float contactHalfWidth = min(
        max(uTireContactPatchWidthM * metersToLocal * 0.5, 0.0),
        uTireVisualHalfWidth);
    float contactWidthMask = contactHalfWidth > 0.0001
        ? 1.0 - smoothstep(
            contactHalfWidth * 0.92,
            min(uTireVisualHalfWidth, contactHalfWidth * 1.12 + 0.0001),
            abs(axial))
        : treadWidthMask;
    float deflection = uTireVisualGrounded
        ? max(uTireRadialDeflectionM, 0.0) * metersToLocal
        : 0.0;
    float contactLengthLocal =
        max(uTireContactPatchLengthM, 0.0) * metersToLocal;
    float contactHalfLength = contactLengthLocal * 0.5;
    float longitudinalSigned = dot(radial, forward);
    float longitudinalCoordinate = abs(longitudinalSigned);
    float contactLengthMask = contactHalfLength > 0.0001
        ? 1.0 - smoothstep(
            contactHalfLength * 0.92,
            contactHalfLength * 1.12 + 0.0001,
            longitudinalCoordinate)
        : 1.0;
    float loadedPlane = uTireVisualOuterRadius - deflection;
    if (uTireVisualGrounded && !uTireVisualProbeGridValid
        && uTireContactPlaneDistanceM > 0.02)
    {
        // A measured centre-to-plane value may demand extra compression, but
        // must never erase the radial deflection already solved by physics.
        float measuredPlane = clamp(
            uTireContactPlaneDistanceM * metersToLocal,
            uTireVisualInnerRadius + 0.0001,
            uTireVisualOuterRadius);
        loadedPlane = min(loadedPlane, measuredPlane);
    }
    float centerLoadedPlane = loadedPlane;
    if (uTireVisualGrounded && !uTireVisualProbeGridValid
        && uTireVisualSupportGridValid)
    {
        float longitudinalM = longitudinalSigned / max(metersToLocal, 0.0001);
        float lateralM = dot(relative, lateral) / max(metersToLocal, 0.0001);
        float residualM = tireSupportResidualM(longitudinalM, lateralM);
        float localConformLimitM = min(
            0.045, max(0.015, uTireRadialDeflectionM * 1.75 + 0.008));
        residualM = clamp(residualM, -localConformLimitM, localConformLimitM);
        loadedPlane = clamp(
            loadedPlane - residualM * metersToLocal,
            uTireVisualInnerRadius + 0.0001,
            uTireVisualOuterRadius);

        float heightAboveCenterPlaneM = max(
            (centerLoadedPlane - dot(radial, down))
                / max(metersToLocal, 0.0001),
            0.0);
        if (!uTireVisualProbeGridValid)
        {
            position += tireCurbFaceCompressionLocal(
                longitudinalM, lateralM, heightAboveCenterPlaneM,
                radialFractionAfter, dot(radialDirection, down),
                forward, lateral, metersToLocal);
        }
        relative = position - uTireVisualCenter;
        axial = dot(relative, axle);
        radial = relative - axle * axial;
        radius = max(length(radial), 0.000001);
        radialDirection = radial / radius;
    }
    float flattenAmount = max(dot(radial, down) - loadedPlane, 0.0)
        * treadRadialMask * contactWidthMask * contactLengthMask;
    position -= down * flattenAmount;
    float halfPatchAngle = asin(clamp(
        contactLengthLocal / max(2.0 * uTireVisualOuterRadius, 0.0001),
        0.0, 0.95));
    float influenceAngle = max(halfPatchAngle * 2.8, 0.45);
    float localFootprintEnvelope = smoothstep(
        cos(influenceAngle), 1.0, dot(radialDirection, down));
    float loadedLowerHemisphere = dot(radialDirection, down);
    float wholeLowerCarcassEnvelope = smoothstep(
        -0.10, 0.92, loadedLowerHemisphere);
    float bottomMask = max(
        localFootprintEnvelope, wholeLowerCarcassEnvelope * 0.78);
    float sidewallRadialMask = smoothstep(0.08, 0.28, radialFractionAfter)
        * (1.0 - smoothstep(0.76, 0.96, radialFractionAfter));
    float sidewallAxialMask = smoothstep(0.48, 0.90,
        abs(axial) / max(uTireVisualHalfWidth, 0.0001));
    float lowerCarcassMask = bottomMask
        * smoothstep(0.035, 0.52, radialFractionAfter)
        * (1.0 - treadRadialMask * 0.35);
    position -= down * (deflection * 0.48 * lowerCarcassMask);
    float bulge = deflection * 0.46
        * bottomMask * sidewallRadialMask * sidewallAxialMask;
    if (abs(axial) > 0.000001)
        position += axle * sign(axial) * bulge;

    if (uTireVisualGrounded && !uTireVisualProbeGridValid
        && uTireContactPlaneDistanceM > 0.02)
    {
        relative = position - uTireVisualCenter;
        axial = dot(relative, axle);
        radial = relative - axle * axial;
        radius = max(length(radial), 0.000001);
        radialDirection = radial / radius;
        float radialFractionFinal = clamp(
            (radius - uTireVisualInnerRadius) / radialSpan, 0.0, 1.0);
        float roadFacingFinal = dot(radialDirection, down);
        float rubberPlaneMask = smoothstep(0.08, 0.34, radialFractionFinal)
            * smoothstep(0.02, 0.40, roadFacingFinal);
        float hardPlaneDistanceLocal = uTireContactPlaneDistanceM * metersToLocal;
        float hardPlanePenetration = max(
            dot(radial, down) - hardPlaneDistanceLocal, 0.0);
        position -= down * hardPlanePenetration * rubberPlaneMask;
    }
    // TIRE17C7/VIS10 shadow geometry uses the same exact collider constraint.
    applyTireProbeGridConstraint(position);
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
