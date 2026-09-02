#pragma once
#include "SuspensionProductionRuntimeV3.hpp"
#include "SuspensionProviderRegistryV3.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace heritage::vehicles::suspension
{
inline constexpr std::string_view suspensionElementKindKeyV3(SuspensionElementKindV3 k)
{
    switch(k)
    {
    case SuspensionElementKindV3::StructuralLink:return "structural_link";
    case SuspensionElementKindV3::BallJoint:return "ball_joint";
    case SuspensionElementKindV3::Bushing6Dof:return "bushing_6dof";
    case SuspensionElementKindV3::CoilSpring:return "coil_spring";
    case SuspensionElementKindV3::DualRateSpring:return "dual_rate_spring";
    case SuspensionElementKindV3::LeafSpring:return "leaf_spring";
    case SuspensionElementKindV3::AirSpring:return "air_spring";
    case SuspensionElementKindV3::HydroPneumaticSpring:return "hydropneumatic_spring";
    case SuspensionElementKindV3::TorsionEquivalent:return "torsion_element";
    case SuspensionElementKindV3::Damper:return "damper";
    case SuspensionElementKindV3::BumpStop:return "bump_stop";
    case SuspensionElementKindV3::ReboundStop:return "rebound_stop";
    case SuspensionElementKindV3::ActiveActuator:return "active_actuator";
    case SuspensionElementKindV3::AntiRollDropLink:return "antiroll_drop_link";
    case SuspensionElementKindV3::ThirdElementLink:return "third_element_path";
    case SuspensionElementKindV3::InerterLink:return "inerter";
    case SuspensionElementKindV3::HydraulicPistonLink:return "hydraulic_piston_path";
    case SuspensionElementKindV3::Mount:return "mount";
    }
    return "unknown";
}

struct SuspensionAuthoringIssueV3
{
    std::uint32_t corner=0,elementId=0;
    std::string_view message{};
};
struct SuspensionAuthoringValidationV3
{
    static constexpr std::size_t Capacity=128;
    std::array<SuspensionAuthoringIssueV3,Capacity> issues{};std::size_t count=0;
    bool ok()const{return count==0;}
    void add(std::uint32_t c,std::uint32_t id,std::string_view m){if(count<Capacity)issues[count++]={c,id,m};}
};

inline bool suspensionGraphHasElementIdV3(const SuspensionCornerGraphDescriptionV3& c,std::uint32_t id)
{
    if(id==0)return false;
    for(std::size_t i=0;i<c.graph.count;++i)if(c.graph.elements[i].id==id)return true;
    return false;
}

inline SuspensionAuthoringValidationV3 validateSuspensionAuthoringV3(const SuspensionVehicleGraphDescriptionV3& d,const SuspensionProviderRegistryV3& registry)
{
    SuspensionAuthoringValidationV3 v;
    if(d.cornerCount==0||d.cornerCount>d.MaxCorners)v.add(0,0,"corner_count_out_of_range");
    if(d.axleCount>d.MaxAxles)v.add(0,0,"axle_count_out_of_range");
    if(!registry.completeForProduction())v.add(0,0,"provider_registry_incomplete");
    for(std::size_t c=0;c<d.cornerCount;++c)
    {
        if(!validateSuspensionElementGraphV3(d.corners[c].graph))v.add(static_cast<std::uint32_t>(c),0,"element_graph_invalid");
        for(std::size_t i=0;i<d.corners[c].graph.count;++i)
        {
            const auto& e=d.corners[c].graph.elements[i];
            if(e.a.frameId==0||e.b.frameId==0)v.add(static_cast<std::uint32_t>(c),e.id,"attachment_frame_missing");
            if(suspensionElementKindKeyV3(e.kind)=="unknown")v.add(static_cast<std::uint32_t>(c),e.id,"element_kind_unknown");
            if((suspensionElementCarriesForceV3(e.kind)||suspensionElementCarriesConstraintV3(e.kind))&&e.referenceLengthM<0)v.add(static_cast<std::uint32_t>(c),e.id,"negative_reference_length");
        }
    }
    for(std::size_t a=0;a<d.axleCount;++a)
    {
        const auto l=d.axleCorners[a][0],r=d.axleCorners[a][1];if(l>=d.cornerCount||r>=d.cornerCount){v.add(0,0,"axle_corner_map_invalid");continue;}
        const auto& x=d.axles[a];
        if((x.antiRollEnabled||x.activeAntiRollEnabled)&&(!suspensionGraphHasElementIdV3(d.corners[l],x.leftDropLinkElementId)||!suspensionGraphHasElementIdV3(d.corners[r],x.rightDropLinkElementId)))v.add(0,0,"antiroll_drop_link_path_missing");
        if(x.thirdEnabled&&(!suspensionGraphHasElementIdV3(d.corners[l],x.leftThirdPathElementId)||!suspensionGraphHasElementIdV3(d.corners[r],x.rightThirdPathElementId)))v.add(0,0,"third_element_path_missing");
        if(x.hydraulicEnabled&&(!suspensionGraphHasElementIdV3(d.corners[l],x.leftHydraulicPathElementId)||!suspensionGraphHasElementIdV3(d.corners[r],x.rightHydraulicPathElementId)))v.add(0,0,"hydraulic_path_missing");
        if(x.inerterEnabled&&(!suspensionGraphHasElementIdV3(d.corners[l],x.leftInerterPathElementId)||!suspensionGraphHasElementIdV3(d.corners[r],x.rightInerterPathElementId)))v.add(0,0,"inerter_path_missing");
    }
    return v;
}

// Stable names consumed by Lua manifests, Heritage Studio property panels, telemetry and replay tooling.
inline constexpr std::uint32_t SuspensionAuthoringSchemaVersionV3=4u;
inline constexpr std::string_view SuspensionElementIdFieldV3="element_id";
inline constexpr std::string_view SuspensionElementKindFieldV3="element_kind";
inline constexpr std::string_view SuspensionFrameAFieldV3="frame_a";
inline constexpr std::string_view SuspensionFrameBFieldV3="frame_b";
inline constexpr std::string_view SuspensionLocalPointAFieldV3="local_point_a";
inline constexpr std::string_view SuspensionLocalPointBFieldV3="local_point_b";
inline constexpr std::string_view SuspensionReferenceLengthFieldV3="reference_length_m";
}
