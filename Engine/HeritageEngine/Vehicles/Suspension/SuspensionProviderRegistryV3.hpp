#pragma once
#include "SuspensionGeometryJacobianV3.hpp"
#include "SuspensionProviderCatalog.hpp"
#include <array>
#include <cstddef>
#include <string_view>

namespace heritage::vehicles::suspension
{

struct SuspensionProviderEntryV3
{
    SuspensionProviderKind kind=SuspensionProviderKind::MacPhersonStrut;
    SuspensionProviderKind canonical=SuspensionProviderKind::MacPhersonStrut;
    SuspensionFrameSolveFnV3 solve=nullptr;
    bool alias=false;
};

class SuspensionProviderRegistryV3
{
public:
    static constexpr std::size_t Capacity=24;
    bool registerProvider(SuspensionProviderKind kind,SuspensionFrameSolveFnV3 solve)
    {
        if(!solve||find(kind)||m_count>=Capacity||canonicalProvider(kind)!=kind)return false;
        m_entries[m_count++]={kind,kind,solve,false};return true;
    }
    bool registerAlias(SuspensionProviderKind aliasKind,SuspensionProviderKind target)
    {
        if(find(aliasKind)||m_count>=Capacity||canonicalProvider(aliasKind)==aliasKind)return false;
        const auto* t=resolve(target);if(!t)return false;m_entries[m_count++]={aliasKind,t->canonical,t->solve,true};return true;
    }
    const SuspensionProviderEntryV3* find(SuspensionProviderKind k)const
    {
        for(std::size_t i=0;i<m_count;++i)if(m_entries[i].kind==k)return &m_entries[i];
        return nullptr;
    }
    const SuspensionProviderEntryV3* resolve(SuspensionProviderKind k)const
    {
        const auto* e=find(k);if(!e)return nullptr;if(!e->alias)return e;
        for(std::size_t i=0;i<m_count;++i)if(!m_entries[i].alias&&m_entries[i].kind==e->canonical)return &m_entries[i];
        return nullptr;
    }
    bool solve(SuspensionProviderKind k,const void* description,void* state,const SuspensionGeometrySolveRequestV3& request,SuspensionFrameSetV3& frames)const
    {
        const auto* e=resolve(k);return e&&e->solve&&description&&e->solve(description,state,request,frames);
    }
    bool completeForProduction()const
    {
        constexpr std::array<SuspensionProviderKind,12> canonical{{
            SuspensionProviderKind::MacPhersonStrut,SuspensionProviderKind::DoubleWishbone,SuspensionProviderKind::PushrodRockerWishbone,
            SuspensionProviderKind::RigidLiveAxle,SuspensionProviderKind::LeafSpringLiveAxle,SuspensionProviderKind::MotorcycleForkSwingarm,
            SuspensionProviderKind::SemiTrailingArm,SuspensionProviderKind::TwistBeam,SuspensionProviderKind::MultiLink,SuspensionProviderKind::SwingAxle,
            SuspensionProviderKind::SlidingPillar,SuspensionProviderKind::MotorcycleLinkFront}};
        for(auto k:canonical)if(!resolve(k))return false;
        constexpr std::array<SuspensionProviderKind,3> aliases{{SuspensionProviderKind::ChapmanStrutAlias,SuspensionProviderKind::PureTrailingArmAlias,SuspensionProviderKind::DeDionAlias}};
        for(auto k:aliases)if(!resolve(k))return false;
        return true;
    }
    std::size_t count()const{return m_count;}
private:
    std::array<SuspensionProviderEntryV3,Capacity> m_entries{};std::size_t m_count=0;
};

inline bool registerStandardSuspensionAliasesV3(SuspensionProviderRegistryV3& r)
{
    return r.registerAlias(SuspensionProviderKind::ChapmanStrutAlias,SuspensionProviderKind::MacPhersonStrut)
        && r.registerAlias(SuspensionProviderKind::PureTrailingArmAlias,SuspensionProviderKind::SemiTrailingArm)
        && r.registerAlias(SuspensionProviderKind::DeDionAlias,SuspensionProviderKind::RigidLiveAxle);
}

} // namespace heritage::vehicles::suspension
