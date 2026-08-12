#pragma once

#include "../VehiclePrecision.hpp"

#include <cstddef>
#include <vector>

namespace heritage::vehicles::tires {

// TIRE05/TIRE06 clean-room tandem-cam-inspired road enveloping layer.
// Simcenter's public MF-Swift material describes front/rear rigid elliptical
// cams plus multiple parallel/successive cams for 3D obstacle contact. Heritage
// implements the architecture independently and does not claim numerical
// identity with Siemens' proprietary road filter.
struct TireRoadEnvelopingDescription
{
    bool enabled = false;
    VehicleScalar ellipseShiftScale = 1.0;
    VehicleScalar ellipseLengthM = 0.10;   // semi-major axis
    VehicleScalar ellipseHeightM = 0.05;  // semi-minor axis
    VehicleScalar ellipseOrder = 2.0;
    VehicleScalar maximumRoadStepM = 0.10;

    // Public ELLIPS_NWIDTH / ELLIPS_NLENGTH fidelity hints. TIRE06 uses them
    // to build a bounded 2D sample lattice. The centre ray is already queried
    // by VehicleSystem at 1 kHz; additional footprint rays are slower.
    int widthCamCount = 1;
    int sideCamCount = 1;
    VehicleScalar effectiveHeightAttenuation = 1.0;
    VehicleScalar effectivePlaneAngleAttenuation = 1.0;

    // TIRE06 adaptive 2D footprint controls. Quiet smooth-road operation uses
    // a five-point cross and a slower query cadence; a detected height/surface
    // discontinuity expands to the full requested grid and the faster cadence.
    bool adaptive2D = true;
    VehicleScalar quietQueryIntervalSeconds = 0.008; // 125 Hz
    VehicleScalar queryIntervalSeconds = 0.004;      // 250 Hz when complex
    VehicleScalar refinementHeightThresholdM = 0.004;
    VehicleScalar refinementWetnessThreshold = 0.12;
    VehicleScalar lateralFootprintScale = 0.90;
    int maximumAxisSamples = 5;
};

struct TireRoadEnvelopeOffset
{
    VehicleScalar longitudinalOffsetM = 0.0;
    VehicleScalar lateralOffsetM = 0.0;
};

struct TireRoadEnvelopeSample
{
    bool valid = false;
    VehicleScalar longitudinalOffsetM = 0.0;
    VehicleScalar roadHeightRelativeToCenterM = 0.0;
    // Appended after the historical TIRE05 fields so old 3-value aggregate
    // initializers remain source-compatible and imply the centre row.
    VehicleScalar lateralOffsetM = 0.0;
};

struct TireRoadEnvelopeOutput
{
    bool valid = false;
    VehicleScalar effectiveRoadHeightM = 0.0;
    VehicleScalar effectiveRoadSlopeRadians = 0.0;
    VehicleScalar effectiveCrossSlopeRadians = 0.0;
    VehicleScalar frontCamHeightM = 0.0;
    VehicleScalar rearCamHeightM = 0.0;
    VehicleScalar roughnessHeightRangeM = 0.0;
    VehicleScalar supportedFraction = 0.0;
    std::size_t validSampleCount = 0;
    std::size_t totalSampleCount = 0;
};

bool validTireRoadEnvelopingDescription(
    const TireRoadEnvelopingDescription& description);

VehicleScalar roadEnvelopeCamCenterOffsetM(
    const TireRoadEnvelopingDescription& description,
    VehicleScalar contactPatchLengthM);

VehicleScalar roadEnvelopeLateralHalfSpanM(
    const TireRoadEnvelopingDescription& description,
    VehicleScalar contactPatchWidthM);

// Build either the low-cost five-point cross (refined=false) or the complete
// bounded 2D lattice requested by ELLIPS_NWIDTH / ELLIPS_NLENGTH. The centre
// point is included in the returned pattern so callers can substitute their
// already-known 1 kHz centre ray rather than recasting it.
std::vector<TireRoadEnvelopeOffset> buildTireRoadEnvelopeSamplePattern(
    const TireRoadEnvelopingDescription& description,
    VehicleScalar contactPatchLengthM,
    VehicleScalar contactPatchWidthM,
    bool refined);

bool tireRoadEnvelopeNeedsHeightRefinement(
    const TireRoadEnvelopingDescription& description,
    const std::vector<TireRoadEnvelopeSample>& samples);

// A footprint edge that loses support is itself a reason to refine: the full
// lattice distinguishes a true partial-contact condition from one isolated
// coarse probe miss. The centre sample is authoritative while hitGround=true,
// so a mixture of supported/unsupported samples means partial support.
bool tireRoadEnvelopeHasPartialSupport(
    const std::vector<TireRoadEnvelopeSample>& samples);

// Expected relative support-height change caused purely by a smooth local
// road plane. VehicleSystem subtracts this before feeding samples into the
// obstacle envelope so grade/crossfall does not masquerade as roughness.
VehicleScalar roadEnvelopeLocalPlaneHeightM(
    VehicleScalar longitudinalOffsetM,
    VehicleScalar roadNormalAlongForward,
    VehicleScalar roadNormalAlongSupportRay);

VehicleScalar roadEnvelopeLocalPlaneHeightM(
    VehicleScalar longitudinalOffsetM,
    VehicleScalar lateralOffsetM,
    VehicleScalar roadNormalAlongForward,
    VehicleScalar roadNormalAlongRight,
    VehicleScalar roadNormalAlongSupportRay);

TireRoadEnvelopeOutput evaluateTireRoadEnvelope(
    const TireRoadEnvelopingDescription& description,
    VehicleScalar contactPatchLengthM,
    const std::vector<TireRoadEnvelopeSample>& samples);

} // namespace heritage::vehicles::tires
