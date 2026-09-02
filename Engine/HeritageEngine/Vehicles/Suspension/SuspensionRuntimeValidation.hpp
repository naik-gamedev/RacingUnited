#pragma once
#include "SuspensionProductionRuntime.hpp"
#include "SuspensionProviderRegistryV2.hpp"
#include <cmath>
#include <cstdint>

namespace heritage::vehicles::suspension
{
enum SuspensionValidationIssueV2 : std::uint32_t
{
    SuspensionValidationNoneV2=0,
    SuspensionValidationNonFiniteV2=1u<<0,
    SuspensionValidationBadMotionRatioV2=1u<<1,
    SuspensionValidationBadPressureV2=1u<<2,
    SuspensionValidationBadTemperatureV2=1u<<3,
    SuspensionValidationConstraintLostV2=1u<<4,
    SuspensionValidationProviderMissingV2=1u<<5,
    SuspensionValidationComplianceNotConsumedV2=1u<<6,
    SuspensionValidationDuplicateAuthorityV2=1u<<7
};

struct SuspensionValidationResultV2
{
    std::uint32_t issues=SuspensionValidationNoneV2;
    bool ok() const{return issues==SuspensionValidationNoneV2;}
};

inline bool validSuspensionMotionRatioV2(double mr)
{
    return std::isfinite(mr) && std::abs(mr)<=20.0;
}

inline SuspensionValidationResultV2 validateSuspensionCornerStepV2(
    const SuspensionKinematicSampleV2& k,const SuspensionCornerResultV2& r)
{
    SuspensionValidationResultV2 v;
    const auto& t=r.telemetry;
    if(!std::isfinite(r.generalizedWheelForceN)||!std::isfinite(r.supportForceN)||
       !std::isfinite(t.springForceN)||!std::isfinite(t.damperForceN)||
       !std::isfinite(t.activeWheelForceN)||!std::isfinite(t.complianceStoredEnergyJ))
        v.issues|=SuspensionValidationNonFiniteV2;
    if(!validSuspensionMotionRatioV2(k.springMotionRatio)||!validSuspensionMotionRatioV2(k.damperMotionRatio)||
       !validSuspensionMotionRatioV2(k.actuatorExtensionMotionRatio))
        v.issues|=SuspensionValidationBadMotionRatioV2;
    if(t.airPressurePa<0.0||t.hydroPressurePa<0.0||t.damperCompressionPressurePa<0.0||t.damperReboundPressurePa<0.0)
        v.issues|=SuspensionValidationBadPressureV2;
    if(!std::isfinite(t.damperTemperatureC)||t.damperTemperatureC < -80.0 || t.damperTemperatureC > 350.0)
        v.issues|=SuspensionValidationBadTemperatureV2;
    if(!t.constraintEnabled && (t.damageFlags&DamageDetachedV2)==0u)
        v.issues|=SuspensionValidationConstraintLostV2;
    return v;
}

inline SuspensionValidationResultV2 validateSuspensionProviderStepV2(
    const SuspensionProviderRegistryV2& registry,SuspensionProviderKind kind,
    const SuspensionKinematicsRequestV2& request,const SuspensionKinematicsPoseV2& pose)
{
    SuspensionValidationResultV2 v;
    if(!registry.resolve(kind))v.issues|=SuspensionValidationProviderMissingV2;
    if(!pose.converged||!std::isfinite(pose.wheelCentre.x)||!std::isfinite(pose.wheelCentre.y)||
       !std::isfinite(pose.wheelCentre.z)||!std::isfinite(pose.maximumConstraintErrorM))
        v.issues|=SuspensionValidationNonFiniteV2;
    double complianceMagnitude=0.0;for(double x:request.mountComplianceOffset)complianceMagnitude+=x*x;
    if(complianceMagnitude>1.0e-20 && !pose.consumedCompliance)
        v.issues|=SuspensionValidationComplianceNotConsumedV2;
    return v;
}

// Runtime adapters set these flags while migrating VehicleSystem. Production mode is valid only
// when the legacy scalar spring/damper path is disabled for every corner owned by SUSP24+.
struct SuspensionAuthorityAuditV2
{
    bool productionCoordinatorCalled=false;
    bool providerSolvedExactlyOnce=false;
    bool mountFeedbackAppliedBeforeKinematics=false;
    bool generalizedForceAppliedExactlyOnce=false;
    bool legacyScalarSpringDamperDisabled=false;
};
inline SuspensionValidationResultV2 validateSuspensionAuthorityV2(const SuspensionAuthorityAuditV2& a)
{
    SuspensionValidationResultV2 r;
    if(!(a.productionCoordinatorCalled&&a.providerSolvedExactlyOnce&&a.mountFeedbackAppliedBeforeKinematics&&
         a.generalizedForceAppliedExactlyOnce&&a.legacyScalarSpringDamperDisabled))
        r.issues|=SuspensionValidationDuplicateAuthorityV2;
    return r;
}
}
