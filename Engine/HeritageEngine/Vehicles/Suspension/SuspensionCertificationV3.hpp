#pragma once
#include "SuspensionProductionRuntimeV3.hpp"
#include "SuspensionSerializationV3.hpp"
#include "SuspensionProviderCatalog.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace heritage::vehicles::suspension
{
enum SuspensionCertificationIssueV3 : std::uint32_t
{
    CertNoneV3=0,CertGraphInvalidV3=1u<<0,CertGeometryInvalidV3=1u<<1,CertMissingConstraintFeedbackV3=1u<<2,
    CertDuplicateForceOwnershipV3=1u<<3,CertSerializationV3=1u<<4,CertProviderCoverageV3=1u<<5
};
struct SuspensionCertificationResultV3{std::uint32_t issues=0;bool ok()const{return issues==0;}};

inline SuspensionCertificationResultV3 validateSuspensionGraphRuntimeV3(const SuspensionElementGraphDescriptionV3& d,
    const SuspensionElementGraphStateV3& s,const SuspensionGeometrySampleSetV3& g)
{
    SuspensionCertificationResultV3 r;if(!validateSuspensionElementGraphV3(d))r.issues|=CertGraphInvalidV3;if(!g.converged||!g.allReferencedFramesPresent)r.issues|=CertGeometryInvalidV3;
    std::size_t constraintCount=0;for(std::size_t i=0;i<d.count;++i)if(suspensionElementCarriesConstraintV3(d.elements[i].kind))++constraintCount;
    if(buildConstraintOverridesV3(d,s).count!=constraintCount)r.issues|=CertMissingConstraintFeedbackV3;
    return r;
}

inline constexpr std::array<SuspensionProviderKind,15> suspensionRequiredProviderCatalogV3()
{
    return {SuspensionProviderKind::MacPhersonStrut,SuspensionProviderKind::DoubleWishbone,SuspensionProviderKind::PushrodRockerWishbone,
            SuspensionProviderKind::RigidLiveAxle,SuspensionProviderKind::LeafSpringLiveAxle,SuspensionProviderKind::MotorcycleForkSwingarm,
            SuspensionProviderKind::SemiTrailingArm,SuspensionProviderKind::TwistBeam,SuspensionProviderKind::MultiLink,SuspensionProviderKind::SwingAxle,
            SuspensionProviderKind::SlidingPillar,SuspensionProviderKind::MotorcycleLinkFront,SuspensionProviderKind::ChapmanStrutAlias,
            SuspensionProviderKind::PureTrailingArmAlias,SuspensionProviderKind::DeDionAlias};
}
}
