#pragma once
#include "SuspensionProviderCatalog.hpp"
#include "SuspensionProductionRuntime.hpp"
#include "MultiLinkKinematics.hpp"
#include "LegacyAndSpecialKinematics.hpp"
#include "MotorcycleFront3D.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace heritage::vehicles::suspension
{

inline double suspProviderNorm6(const SuspVec6& v)
{
    double sum=0.0; for(double x:v) sum+=x*x; return std::sqrt(sum);
}

struct SuspensionKinematicsRequestV2
{
    double requestedTravelM=0.0;
    double steeringAngleRad=0.0;
    double deltaTimeSeconds=0.0;
    // Generalized compliance of the mount/subframe from the previous force step.
    // A provider adapter that owns compliant hardpoints must consume this before
    // solving its geometry. It is deliberately input-only to avoid a second owner.
    SuspVec6 mountComplianceOffset{};
};

struct SuspensionKinematicsPoseV2
{
    MultiLinkVec3 wheelCentre{};
    MultiLinkVec3 wheelForward{0.0,0.0,1.0};
    MultiLinkVec3 wheelUp{0.0,1.0,0.0};
    double maximumConstraintErrorM=0.0;
    bool converged=false;
    bool consumedCompliance=false;
    SuspensionKinematicSampleV2 forceSample{};
};

using SuspensionProviderSolveFnV2 = bool (*)(
    const void* description,
    void* mutableState,
    const SuspensionKinematicsRequestV2& request,
    SuspensionKinematicsPoseV2& output);

struct SuspensionProviderEntryV2
{
    SuspensionProviderKind kind=SuspensionProviderKind::MacPhersonStrut;
    SuspensionProviderKind canonical=SuspensionProviderKind::MacPhersonStrut;
    std::string_view key{};
    SuspensionProviderSolveFnV2 solve=nullptr;
    bool alias=false;
};

class SuspensionProviderRegistryV2
{
public:
    static constexpr std::size_t Capacity=32;

    bool registerProvider(SuspensionProviderKind kind,SuspensionProviderSolveFnV2 solve)
    {
        if(!solve || findExact(kind)!=nullptr || m_count>=Capacity) return false;
        auto& e=m_entries[m_count++];
        e.kind=kind;e.canonical=kind;e.key=suspensionProviderKey(kind);e.solve=solve;e.alias=false;
        return true;
    }

    bool registerAlias(SuspensionProviderKind aliasKind,SuspensionProviderKind canonicalKind)
    {
        if(findExact(aliasKind)!=nullptr || m_count>=Capacity) return false;
        const SuspensionProviderEntryV2* target=resolve(canonicalKind);
        if(!target || !target->solve) return false;
        auto& e=m_entries[m_count++];
        e.kind=aliasKind;e.canonical=target->canonical;e.key=suspensionProviderKey(aliasKind);
        e.solve=target->solve;e.alias=true;
        return true;
    }

    const SuspensionProviderEntryV2* findExact(SuspensionProviderKind kind) const
    {
        for(std::size_t i=0;i<m_count;++i) if(m_entries[i].kind==kind) return &m_entries[i];
        return nullptr;
    }

    const SuspensionProviderEntryV2* findKey(std::string_view key) const
    {
        for(std::size_t i=0;i<m_count;++i) if(m_entries[i].key==key) return &m_entries[i];
        return nullptr;
    }

    const SuspensionProviderEntryV2* resolve(SuspensionProviderKind kind) const
    {
        const auto* exact=findExact(kind);
        if(!exact) return nullptr;
        if(!exact->alias) return exact;
        for(std::size_t i=0;i<m_count;++i)
            if(!m_entries[i].alias && m_entries[i].kind==exact->canonical) return &m_entries[i];
        return nullptr;
    }

    bool solve(SuspensionProviderKind kind,const void* description,void* state,
               const SuspensionKinematicsRequestV2& request,SuspensionKinematicsPoseV2& output) const
    {
        const auto* e=resolve(kind);
        if(!e || !e->solve || !description) return false;
        output={};
        return e->solve(description,state,request,output) && output.converged;
    }

    std::size_t count() const{return m_count;}

    bool valid() const
    {
        for(std::size_t i=0;i<m_count;++i)
        {
            if(m_entries[i].key.empty() || !m_entries[i].solve) return false;
            for(std::size_t j=i+1;j<m_count;++j)
                if(m_entries[i].kind==m_entries[j].kind || m_entries[i].key==m_entries[j].key) return false;
            if(m_entries[i].alias && resolve(m_entries[i].kind)==nullptr) return false;
        }
        return true;
    }

private:
    std::array<SuspensionProviderEntryV2,Capacity> m_entries{};
    std::size_t m_count=0;
};

inline bool solveMultiLinkProviderV2(const void* description,void* state,
                                     const SuspensionKinematicsRequestV2& request,
                                     SuspensionKinematicsPoseV2& output)
{
    const auto& d=*static_cast<const MultiLinkDescription*>(description);
    auto* warm=static_cast<MultiLinkState*>(state);
    const MultiLinkState result=MultiLinkKinematics::solve(d,request.requestedTravelM,
                                                            warm&&warm->converged?warm:nullptr);
    if(warm) *warm=result;
    output.wheelCentre=result.wheelCentre;
    output.wheelForward=normalized(rotate(result.uprightOrientation,{0.0,0.0,1.0}));
    output.wheelUp=normalized(rotate(result.uprightOrientation,{0.0,1.0,0.0}));
    output.maximumConstraintErrorM=result.maximumLinkErrorMetres;
    output.converged=result.converged;
    // The generic provider does not know which physical bush each 6-DOF offset belongs to.
    // Vehicle-specific adapters must map compliance to authored hardpoints before calling it.
    output.consumedCompliance=suspProviderNorm6(request.mountComplianceOffset)<=1.0e-15;
    output.forceSample.wheelCompressionM=request.requestedTravelM;
    return output.converged;
}

inline bool solveSwingAxleProviderV2(const void* description,void*,
                                     const SuspensionKinematicsRequestV2& request,
                                     SuspensionKinematicsPoseV2& output)
{
    const auto r=solveSwingAxle(*static_cast<const SwingAxleDescription*>(description),request.requestedTravelM);
    output.wheelCentre={r.wheelCentre.x,r.wheelCentre.y,r.wheelCentre.z};
    output.wheelUp={r.wheelUp.x,r.wheelUp.y,r.wheelUp.z};
    output.wheelForward={0.0,0.0,1.0};
    output.converged=r.valid;
    output.consumedCompliance=suspProviderNorm6(request.mountComplianceOffset)<=1.0e-15;
    output.forceSample.wheelCompressionM=request.requestedTravelM;
    return r.valid;
}

inline bool solveSlidingPillarProviderV2(const void* description,void*,
                                         const SuspensionKinematicsRequestV2& request,
                                         SuspensionKinematicsPoseV2& output)
{
    const auto r=solveSlidingPillar(*static_cast<const SlidingPillarDescription*>(description),request.requestedTravelM);
    output.wheelCentre={r.wheelCentre.x,r.wheelCentre.y,r.wheelCentre.z};
    output.wheelUp={r.wheelUp.x,r.wheelUp.y,r.wheelUp.z};
    output.wheelForward={0.0,0.0,1.0};
    output.converged=r.valid;
    output.consumedCompliance=suspProviderNorm6(request.mountComplianceOffset)<=1.0e-15;
    output.forceSample.wheelCompressionM=request.requestedTravelM;
    return r.valid;
}

inline bool solveMotorcycleAArmProviderV2(const void* description,void* state,
                                          const SuspensionKinematicsRequestV2& request,
                                          SuspensionKinematicsPoseV2& output)
{
    const auto& d=*static_cast<const MotorcycleAArm3DDescription*>(description);
    auto* warm=static_cast<MotorcycleAArm3DState*>(state);
    const auto r=solveMotorcycleAArm3D(d,request.requestedTravelM,request.steeringAngleRad,
                                       warm&&warm->converged?warm:nullptr);
    if(warm) *warm=r;
    output.wheelCentre=r.wheelCentre;output.wheelForward=r.forward;output.wheelUp=r.steeringAxis;
    output.maximumConstraintErrorM=r.maxConstraintErrorM;output.converged=r.converged;
    output.consumedCompliance=suspProviderNorm6(request.mountComplianceOffset)<=1.0e-15;
    output.forceSample.wheelCompressionM=request.requestedTravelM;
    return r.converged;
}

inline SuspensionProviderRegistryV2 makeSuspensionProviderRegistryV2()
{
    SuspensionProviderRegistryV2 r;
    r.registerProvider(SuspensionProviderKind::MultiLink,&solveMultiLinkProviderV2);
    r.registerProvider(SuspensionProviderKind::SwingAxle,&solveSwingAxleProviderV2);
    r.registerProvider(SuspensionProviderKind::SlidingPillar,&solveSlidingPillarProviderV2);
    r.registerProvider(SuspensionProviderKind::MotorcycleLinkFront,&solveMotorcycleAArmProviderV2);
    // SUSP01-SUSP13 production providers are registered by VehicleSystem adapters because
    // their exact description/state types live in the current repository, not this header.
    return r;
}

} // namespace heritage::vehicles::suspension
