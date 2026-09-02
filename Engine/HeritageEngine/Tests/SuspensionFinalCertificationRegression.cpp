#include "PhysicsRegressionCommon.hpp"

#include "../Vehicles/Suspension/SuspensionProductionV3.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace heritage::tests {
namespace {

using namespace heritage::vehicles::suspension;

struct CertificationProviderDescription
{
    double rockerGain = 2.0;
};

struct CertificationProviderState
{
    std::uint64_t committedSolves = 0;
};

SuspMat3V3 rotationAroundZ(double radians)
{
    SuspMat3V3 result;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    result.m = {{{{cosine, -sine, 0.0}},
                 {{sine, cosine, 0.0}},
                 {{0.0, 0.0, 1.0}}}};
    return result;
}

bool certificationProviderSolve(
    const void* rawDescription,
    void* rawState,
    const SuspensionGeometrySolveRequestV3& request,
    SuspensionFrameSetV3& output)
{
    if (!rawDescription)
        return false;

    const auto& description =
        *static_cast<const CertificationProviderDescription*>(rawDescription);
    auto* state = static_cast<CertificationProviderState*>(rawState);
    if (state && !request.probeOnly)
        ++state->committedSolves;

    output.count = 3;
    output.frames[0].id = 1;
    output.frames[0].hasWheelDerivatives = true;
    output.frames[0].valid = true;

    output.frames[1].id = 2;
    output.frames[1].position = { 0.0, request.wheelCompressionM, 0.0 };
    output.frames[1].linearVelocity = {
        0.0, request.wheelCompressionVelocityMps, 0.0 };
    output.frames[1].linearAcceleration = {
        0.0, request.wheelCompressionAccelerationMps2, 0.0 };
    output.frames[1].dPositionDWheel = { 0.0, 1.0, 0.0 };
    output.frames[1].hasWheelDerivatives = true;
    output.frames[1].valid = true;

    output.frames[2].id = 3;
    output.frames[2].position = { 0.35, 0.20, 0.0 };
    output.frames[2].orientation = rotationAroundZ(
        description.rockerGain * request.wheelCompressionM);
    output.frames[2].angularVelocity = {
        0.0, 0.0,
        description.rockerGain * request.wheelCompressionVelocityMps };
    output.frames[2].angularAcceleration = {
        0.0, 0.0,
        description.rockerGain * request.wheelCompressionAccelerationMps2 };
    output.frames[2].dAngularDWheel = {
        0.0, 0.0, description.rockerGain };
    output.frames[2].hasWheelDerivatives = true;
    output.frames[2].valid = true;
    output.constraintOverridesConsumed = true;
    return true;
}

SuspensionElementDescriptionV3 makeCertificationElement(
    std::uint32_t id,
    SuspensionElementKindV3 kind,
    double rateNPerM)
{
    SuspensionElementDescriptionV3 element;
    element.id = id;
    element.kind = kind;
    element.a.frameId = 1;
    element.a.localPoint = { 0.0, 1.0, 0.0 };
    element.b.frameId = 2;
    element.referenceLengthM = 1.0;
    element.progressive.freeLengthMetres = 1.0;
    element.progressive.linearRateNPerM = rateNPerM;
    element.damageEnabled = false;
    return element;
}

bool providerRegistryIsComplete()
{
    SuspensionProviderRegistryV3 registry;
    constexpr std::array<SuspensionProviderKind, 12> canonical{{
        SuspensionProviderKind::MacPhersonStrut,
        SuspensionProviderKind::DoubleWishbone,
        SuspensionProviderKind::PushrodRockerWishbone,
        SuspensionProviderKind::RigidLiveAxle,
        SuspensionProviderKind::LeafSpringLiveAxle,
        SuspensionProviderKind::MotorcycleForkSwingarm,
        SuspensionProviderKind::SemiTrailingArm,
        SuspensionProviderKind::TwistBeam,
        SuspensionProviderKind::MultiLink,
        SuspensionProviderKind::SwingAxle,
        SuspensionProviderKind::SlidingPillar,
        SuspensionProviderKind::MotorcycleLinkFront
    }};
    for (const SuspensionProviderKind kind : canonical)
    {
        if (!registry.registerProvider(kind, &certificationProviderSolve))
            return false;
    }
    if (!registerStandardSuspensionAliasesV3(registry)
        || !registry.completeForProduction()
        || registry.count() != suspensionRequiredProviderCatalogV3().size())
    {
        return false;
    }
    for (const SuspensionProviderKind kind : suspensionRequiredProviderCatalogV3())
    {
        if (!registry.resolve(kind))
            return false;
    }
    return true;
}

bool graphDamageAndSerializationAreDeterministic()
{
    // The production V3 packets deliberately contain bounded fixed-capacity
    // arrays.  Keep the certification copies off the small default Windows
    // thread stack, just as the owning vehicle/runtime storage does.
    auto description = std::make_unique<SuspensionVehicleGraphDescriptionV3>();
    description->cornerCount = 2;
    description->axleCount = 1;
    description->axleCorners[0] = { 0, 1 };
    for (std::size_t corner = 0; corner < description->cornerCount; ++corner)
    {
        auto& graph = description->corners[corner].graph;
        graph.count = 3;
        graph.elements[0] = makeCertificationElement(
            static_cast<std::uint32_t>(100 + corner * 10),
            SuspensionElementKindV3::CoilSpring,
            38000.0);
        graph.elements[1] = makeCertificationElement(
            static_cast<std::uint32_t>(101 + corner * 10),
            SuspensionElementKindV3::Damper,
            0.0);
        graph.elements[2] = makeCertificationElement(
            static_cast<std::uint32_t>(102 + corner * 10),
            SuspensionElementKindV3::Bushing6Dof,
            0.0);
        graph.elements[2].complianceEnabled = true;
    }

    CertificationProviderDescription providerDescription;
    std::array<CertificationProviderState, 2> providerStates{};
    auto state = std::make_unique<SuspensionVehicleGraphStateV3>();
    double checksum = 0.0;
    for (std::size_t step = 0; step < 250; ++step)
    {
        SuspensionVehicleGraphInputV3 input;
        const double phase = static_cast<double>(step) * 0.01;
        for (std::size_t corner = 0; corner < description->cornerCount; ++corner)
        {
            auto& cornerInput = input.corners[corner];
            cornerInput.provider = {
                &providerDescription,
                &providerStates[corner],
                &certificationProviderSolve };
            cornerInput.geometryRequest.wheelCompressionM =
                0.025 * std::sin(phase + static_cast<double>(corner));
            cornerInput.geometryRequest.wheelCompressionVelocityMps =
                0.25 * std::cos(phase + static_cast<double>(corner));
            cornerInput.geometryRequest.wheelCompressionAccelerationMps2 =
                -0.0025 * std::sin(phase + static_cast<double>(corner));
        }
        const SuspensionVehicleGraphResultV3 result =
            stepSuspensionVehicleGraphV3(*description, *state, input, 0.001);
        if (!result.valid
            || !std::isfinite(result.corners[0].generalizedWheelForceN)
            || !std::isfinite(result.corners[1].generalizedWheelForceN))
        {
            return false;
        }
        checksum += result.corners[0].generalizedWheelForceN
            + result.corners[1].generalizedWheelForceN;
    }
    if (!std::isfinite(checksum)
        || state->stepCounter != 250
        || providerStates[0].committedSolves != 250
        || providerStates[1].committedSolves != 250)
    {
        return false;
    }

    auto packet = std::make_unique<SuspensionStatePacketV3>(
        serializeSuspensionRuntimeV3(*description, *state));
    auto restored = std::make_unique<SuspensionVehicleGraphStateV3>();
    if (!deserializeSuspensionRuntimeV3(*description, *packet, *restored))
        return false;
    auto roundTrip = std::make_unique<SuspensionStatePacketV3>(
        serializeSuspensionRuntimeV3(*description, *restored));
    return packet->contentHash == roundTrip->contentHash
        && packet->bytes == roundTrip->bytes
        && restored->stepCounter == state->stepCounter;
}

bool degradedTopologySeparatesAfterBreakage()
{
    auto description =
        std::make_unique<SuspensionDegradedDynamicsDescriptionV3>();
    description->bodyCount = 2;
    description->gravityMps2 = { 0.0, 0.0, 0.0 };
    description->bodies[0].frameId = 1;
    description->bodies[0].fixed = true;
    description->bodies[1].frameId = 2;
    description->bodies[1].massKg = 10.0;
    description->graph.count = 1;
    auto& link = description->graph.elements[0];
    link.id = 700;
    link.kind = SuspensionElementKindV3::StructuralLink;
    link.a.frameId = 1;
    link.b.frameId = 2;
    link.referenceLengthM = 1.0;
    link.damageEnabled = true;

    SuspensionDegradedDynamicsInputV3 input;
    input.externalLoadCount = 1;
    input.externalLoads[0].frameId = 2;
    input.externalLoads[0].forceN = { 10000.0, 0.0, 0.0 };

    auto intact = std::make_unique<SuspensionDegradedDynamicsStateV3>();
    intact->bodies[1].position = { 0.0, -1.0, 0.0 };
    for (int step = 0; step < 20; ++step)
    {
        if (!stepSuspensionDegradedDynamicsV3(
                *description, *intact, input, 0.001).valid)
        {
            return false;
        }
    }

    auto broken = std::make_unique<SuspensionDegradedDynamicsStateV3>();
    broken->bodies[1].position = { 0.0, -1.0, 0.0 };
    broken->elements.elements[0].constraintEnabled = false;
    broken->elements.elements[0].damage.flags = DamageBrokenV2;
    if (!suspensionRequiresDegradedDynamicsV3(
            description->graph, broken->elements))
    {
        return false;
    }
    for (int step = 0; step < 20; ++step)
    {
        if (!stepSuspensionDegradedDynamicsV3(
                *description, *broken, input, 0.001).valid)
        {
            return false;
        }
    }
    const double intactDistance = suspNormV3(
        intact->bodies[1].position - intact->bodies[0].position);
    const double brokenDistance = suspNormV3(
        broken->bodies[1].position - broken->bodies[0].position);
    return std::abs(intactDistance - 1.0) < 0.02
        && brokenDistance > intactDistance + 0.01;
}

bool mixed150VehicleSuspensionWorkloadIsFinite()
{
    constexpr std::size_t vehicleCount = 150;
    constexpr std::size_t steps = 100;
    CertificationProviderDescription providerDescription;
    std::vector<CertificationProviderState> providers(vehicleCount);
    std::vector<SuspensionCornerGraphStateV3> states(vehicleCount);
    auto description = std::make_unique<SuspensionCornerGraphDescriptionV3>();
    description->graph.count = 4;
    for (std::size_t element = 0; element < description->graph.count; ++element)
    {
        description->graph.elements[element] = makeCertificationElement(
            static_cast<std::uint32_t>(800 + element),
            element == 3 ? SuspensionElementKindV3::BumpStop
                         : SuspensionElementKindV3::CoilSpring,
            12000.0 + static_cast<double>(element) * 4000.0);
    }

    SuspensionCornerGraphControlV3 control;
    double checksum = 0.0;
    for (std::size_t step = 0; step < steps; ++step)
    {
        for (std::size_t vehicle = 0; vehicle < vehicleCount; ++vehicle)
        {
            SuspensionCornerGraphInputV3 input;
            input.provider = {
                &providerDescription,
                &providers[vehicle],
                &certificationProviderSolve };
            const double phase = 0.01 * static_cast<double>(step)
                + 0.03 * static_cast<double>(vehicle);
            input.geometryRequest.wheelCompressionM = 0.025 * std::sin(phase);
            input.geometryRequest.wheelCompressionVelocityMps =
                0.25 * std::cos(phase);
            input.geometryRequest.wheelCompressionAccelerationMps2 =
                -0.0025 * std::sin(phase);
            const auto result = stepSuspensionCornerGraphV3(
                *description, states[vehicle], input, control, 0.001);
            if (!result.valid || !std::isfinite(result.generalizedWheelForceN))
                return false;
            checksum += result.generalizedWheelForceN * 1.0e-12;
        }
    }
    if (!std::isfinite(checksum))
        return false;
    for (const CertificationProviderState& provider : providers)
    {
        if (provider.committedSolves != steps)
            return false;
    }
    return true;
}

} // namespace

bool suspensionFinalCertificationV3Passes()
{
    return providerRegistryIsComplete()
        && graphDamageAndSerializationAreDeterministic()
        && degradedTopologySeparatesAfterBreakage()
        && mixed150VehicleSuspensionWorkloadIsFinite();
}

} // namespace heritage::tests
