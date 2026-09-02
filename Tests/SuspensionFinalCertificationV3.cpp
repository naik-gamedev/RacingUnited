#include "Engine/HeritageEngine/Vehicles/Suspension/SuspensionProductionV3.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace heritage::vehicles::suspension;

struct TestProviderDescription{double rockerGain=2.0;};
struct TestProviderState{std::uint64_t commits=0;};

static SuspMat3V3 rotZ(double a)
{
    SuspMat3V3 r;const double c=std::cos(a),s=std::sin(a);
    r.m={{{{c,-s,0}},{{s,c,0}},{{0,0,1}}}};return r;
}

static bool testProvider(const void* desc,void* rawState,const SuspensionGeometrySolveRequestV3& req,SuspensionFrameSetV3& out)
{
    const auto& d=*static_cast<const TestProviderDescription*>(desc);auto* state=static_cast<TestProviderState*>(rawState);
    if(state&&!req.probeOnly)++state->commits;
    out.count=3;
    out.frames[0].id=1;out.frames[0].position={0,0,0};out.frames[0].hasWheelDerivatives=true;out.frames[0].valid=true; // chassis
    out.frames[1].id=2;out.frames[1].position={0,req.wheelCompressionM,0};out.frames[1].linearVelocity={0,req.wheelCompressionVelocityMps,0};out.frames[1].linearAcceleration={0,req.wheelCompressionAccelerationMps2,0};out.frames[1].dPositionDWheel={0,1,0};out.frames[1].hasWheelDerivatives=true;out.frames[1].valid=true; // upright
    out.frames[2].id=3;out.frames[2].position={0.35,0.20,0};out.frames[2].orientation=rotZ(d.rockerGain*req.wheelCompressionM);
    out.frames[2].angularVelocity={0,0,d.rockerGain*req.wheelCompressionVelocityMps};out.frames[2].angularAcceleration={0,0,d.rockerGain*req.wheelCompressionAccelerationMps2};out.frames[2].dAngularDWheel={0,0,d.rockerGain};out.frames[2].hasWheelDerivatives=true;out.frames[2].valid=true; // rocker/bellcrank frame
    out.constraintOverridesConsumed=true;
    // A real provider consumes individual override IDs before closing constraints. This fixture verifies plumbing.
    if(req.constraintOverrides)
        for(std::size_t i=0;i<req.constraintOverrides->count;++i)
            if(req.constraintOverrides->entries[i].elementId==999u && !req.constraintOverrides->entries[i].enabled)return false;
    return true;
}

static SuspensionElementDescriptionV3 makeSpring(std::uint32_t id,std::uint32_t movingFrame,double x,double y)
{
    SuspensionElementDescriptionV3 e;e.id=id;e.kind=SuspensionElementKindV3::CoilSpring;e.a.frameId=1;e.a.localPoint={x,y,0};e.b.frameId=movingFrame;e.b.localPoint={0,0,0};
    e.referenceLengthM=std::sqrt(x*x+y*y);e.progressive.freeLengthMetres=e.referenceLengthM;e.progressive.linearRateNPerM=40000;e.damageEnabled=false;return e;
}

static void testGeometryAndMultipleElements()
{
    SuspensionCornerGraphDescriptionV3 d;d.graph.count=3;
    d.graph.elements[0]=makeSpring(10,2,0,1.0); // exact MR 1 with vertical upright path
    d.graph.elements[1]=makeSpring(11,2,0,1.0);d.graph.elements[1].progressive.linearRateNPerM=20000; // second physical spring, same corner
    auto damper=makeSpring(12,2,0,1.0);damper.kind=SuspensionElementKindV3::Damper;damper.damageEnabled=false;d.graph.elements[2]=damper;
    SuspensionCornerGraphStateV3 s;SuspensionCornerGraphInputV3 in;SuspensionCornerGraphControlV3 control;TestProviderDescription pd;TestProviderState ps;
    in.provider={&pd,&ps,&testProvider};in.geometryRequest.wheelCompressionM=0.02;in.geometryRequest.wheelCompressionVelocityMps=0.15;in.geometryRequest.wheelCompressionAccelerationMps2=0.3;
    const auto r=stepSuspensionCornerGraphV3(d,s,in,control,0.001);assert(r.valid);assert(ps.commits==1); // +/- probes must not mutate warm state
    assert(std::abs(r.geometry.elements[0].dCompressionDWheel-1.0)<1e-7);assert(std::abs(r.geometry.elements[1].dCompressionDWheel-1.0)<1e-7);
    assert(r.elements[0].localForceN>700.0&&r.elements[1].localForceN>350.0);
    assert(r.elements[0].generalizedWheelForceN<0.0&&r.elements[1].generalizedWheelForceN<0.0);assert(std::isfinite(r.generalizedWheelForceN));
}

static void testNonlinearRockerJacobian()
{
    SuspensionElementGraphDescriptionV3 g;g.count=1;auto& e=g.elements[0];e.id=20;e.kind=SuspensionElementKindV3::CoilSpring;e.a.frameId=1;e.a.localPoint={0.1,0.55,0};e.b.frameId=3;e.b.localPoint={0.15,0,0};e.damageEnabled=false;
    TestProviderDescription pd;TestProviderState ps;SuspensionGeometryProviderV3 p{&pd,&ps,&testProvider};SuspensionGeometrySolveRequestV3 req;req.wheelCompressionM=0.0;
    SuspensionFrameSetV3 base;testProvider(&pd,&ps,req,base);double l=0;suspensionElementLengthV3(base,e,l);e.referenceLengthM=l;
    auto a=evaluateSuspensionGeometryV3(g,p,req);req.wheelCompressionM=0.04;auto b=evaluateSuspensionGeometryV3(g,p,req);assert(a.converged&&b.converged);assert(std::abs(a.elements[0].dCompressionDWheel-b.elements[0].dCompressionDWheel)>1e-3);
}

static void testComponentDamageAndCompliance()
{
    SuspensionElementGraphDescriptionV3 g;g.count=2;
    auto& link=g.elements[0];link.id=100;link.kind=SuspensionElementKindV3::StructuralLink;link.a.frameId=1;link.b.frameId=2;link.referenceLengthM=1;link.damage.ultimateLoadN=1000;link.damage.detachLoadN=5000;link.damage.yieldLoadN=800;link.damageEnabled=true;
    auto& bush=g.elements[1];bush.id=101;bush.kind=SuspensionElementKindV3::Bushing6Dof;bush.a.frameId=1;bush.b.frameId=2;bush.referenceLengthM=1;bush.damageEnabled=false;bush.complianceEnabled=true;
    SuspensionElementGraphStateV3 s;SuspensionElementGeometryV3 geo;geo.valid=true;geo.lengthM=1;geo.dCompressionDWheel=1;SuspensionElementControlV3 c;SuspVec6 high{{2500,0,0,0,0,0}},low{{100,0,0,0,0,0}};
    stepSuspensionElementV3(link,s.elements[0],geo,c,high,0.001);stepSuspensionElementV3(bush,s.elements[1],geo,c,low,0.01);
    const auto o=buildConstraintOverridesV3(g,s);assert(o.count==2);bool linkDisabled=false,bushMoved=false;for(std::size_t i=0;i<o.count;++i){if(o.entries[i].elementId==100)linkDisabled=!o.entries[i].enabled;if(o.entries[i].elementId==101)bushMoved=std::abs(o.entries[i].endpointOffset[0])>0;}
    assert(linkDisabled);assert(bushMoved);assert(s.elements[1].constraintEnabled); // one broken link does not magically destroy every component on corner
}

static void testSerialization()
{
    SuspensionVehicleGraphDescriptionV3 d;d.cornerCount=1;d.corners[0].graph.count=2;d.corners[0].graph.elements[0].id=1;d.corners[0].graph.elements[1].id=2;
    SuspensionVehicleGraphStateV3 s;s.stepCounter=42;s.corners[0].graph.elements[0].damage.wear=0.37;s.corners[0].graph.elements[1].damper.temperatureC=91.25;s.corners[0].graph.elements[1].lastForceN=-1234.5;
    const auto p=serializeSuspensionRuntimeV3(d,s);SuspensionVehicleGraphStateV3 restored;assert(deserializeSuspensionRuntimeV3(d,p,restored));const auto p2=serializeSuspensionRuntimeV3(d,restored);assert(p.bytes==p2.bytes&&p.contentHash==p2.contentHash);
}

static void testFullCatalog()
{
    constexpr auto all=suspensionRequiredProviderCatalogV3();assert(all.size()==15);for(auto k:all)assert(suspensionProviderKey(k)!="unknown");
}


static void testConstraintLoadAndOverrideContract()
{
    struct LoadDesc{} desc;struct LoadState{int commits=0;} state;
    auto solver=[](const void*,void* raw,const SuspensionGeometrySolveRequestV3& req,SuspensionFrameSetV3& out)->bool
    {
        auto* st=static_cast<LoadState*>(raw);if(!req.probeOnly)++st->commits;out.count=2;
        out.frames[0].id=1;out.frames[0].hasWheelDerivatives=true;out.frames[0].valid=true;
        out.frames[1].id=2;out.frames[1].position={0,req.wheelCompressionM-1.0,0};out.frames[1].dPositionDWheel={0,1,0};out.frames[1].linearVelocity={0,req.wheelCompressionVelocityMps,0};out.frames[1].linearAcceleration={0,req.wheelCompressionAccelerationMps2,0};out.frames[1].hasWheelDerivatives=true;out.frames[1].valid=true;
        out.constraintLoadCount=1;out.constraintLoads[0].elementId=500;out.constraintLoads[0].generalizedLoad[0]=1200;out.constraintLoads[0].valid=true;out.constraintOverridesConsumed=true;return true;
    };
    SuspensionCornerGraphDescriptionV3 d;d.graph.count=1;auto& e=d.graph.elements[0];e.id=500;e.kind=SuspensionElementKindV3::StructuralLink;e.a.frameId=1;e.b.frameId=2;e.referenceLengthM=1;e.damage.ultimateLoadN=10000;e.damage.yieldLoadN=9000;
    SuspensionCornerGraphStateV3 st;SuspensionCornerGraphInputV3 in;in.provider={&desc,&state,solver};SuspensionCornerGraphControlV3 ctl;auto r=stepSuspensionCornerGraphV3(d,st,in,ctl,0.001);assert(r.valid);assert(r.geometry.allDamageConstraintLoadsPresent&&r.geometry.constraintOverridesConsumed);assert(state.commits==1);
}

static SuspensionElementDescriptionV3 makeGeometryPath(std::uint32_t id)
{
    auto e=makeSpring(id,2,0,1.0);e.kind=SuspensionElementKindV3::ThirdElementLink;e.damageEnabled=false;return e;
}

static void testAxleGeometryOwnership()
{
    SuspensionVehicleGraphDescriptionV3 d;d.cornerCount=2;d.axleCount=1;d.axleCorners[0]={0,1};
    for(int c=0;c<2;++c)
    {
        d.corners[c].graph.count=4;d.corners[c].graph.elements[0]=makeGeometryPath(600+c*10);d.corners[c].graph.elements[1]=makeGeometryPath(601+c*10);d.corners[c].graph.elements[2]=makeGeometryPath(602+c*10);
        auto drop=makeGeometryPath(603+c*10);drop.kind=SuspensionElementKindV3::AntiRollDropLink;drop.damageEnabled=false;d.corners[c].graph.elements[3]=drop;
    }
    auto& ax=d.axles[0];ax.thirdEnabled=true;ax.third.heaveSpringRateNPerM=50000;ax.third.rollSpringRateNPerM=20000;ax.leftThirdPathElementId=600;ax.rightThirdPathElementId=610;
    ax.hydraulicEnabled=true;ax.hydraulic.accumulatorPressurePa=0;ax.leftHydraulicPathElementId=601;ax.rightHydraulicPathElementId=611;
    ax.inerterEnabled=true;ax.inerterKg=120;ax.leftInerterPathElementId=602;ax.rightInerterPathElementId=612;
    ax.antiRollEnabled=true;ax.antiRoll.rateNmPerRad=5000;ax.leftDropLinkElementId=603;ax.rightDropLinkElementId=613;ax.leftBarPath.frameId=3;ax.rightBarPath.frameId=3;
    SuspensionVehicleGraphStateV3 state;SuspensionVehicleGraphInputV3 in;TestProviderDescription pd;TestProviderState ps0,ps1;in.corners[0].provider={&pd,&ps0,&testProvider};in.corners[1].provider={&pd,&ps1,&testProvider};
    in.corners[0].geometryRequest.wheelCompressionM=0.03;in.corners[1].geometryRequest.wheelCompressionM=-0.01;in.corners[0].geometryRequest.wheelCompressionVelocityMps=0.2;in.corners[1].geometryRequest.wheelCompressionVelocityMps=-0.1;in.corners[0].geometryRequest.wheelCompressionAccelerationMps2=1.0;in.corners[1].geometryRequest.wheelCompressionAccelerationMps2=-0.5;
    const auto r=stepSuspensionVehicleGraphV3(d,state,in,0.001);assert(r.valid);assert(std::abs(r.axles[0].leftGeneralizedForceN)>1.0);assert(std::abs(r.axles[0].rightGeneralizedForceN)>1.0);
}

static void testDegradedMultibodyFallback()
{
    SuspensionDegradedDynamicsDescriptionV3 d;d.bodyCount=2;d.gravityMps2={0,0,0};d.bodies[0].frameId=1;d.bodies[0].fixed=true;d.bodies[1].frameId=2;d.bodies[1].massKg=10;d.graph.count=1;auto& link=d.graph.elements[0];link.id=700;link.kind=SuspensionElementKindV3::StructuralLink;link.a.frameId=1;link.b.frameId=2;link.referenceLengthM=1;link.damageEnabled=true;link.damage.ultimateLoadN=1e9;link.damage.detachLoadN=1e10;
    SuspensionDegradedDynamicsStateV3 intact;intact.bodies[0].position={0,0,0};intact.bodies[1].position={0,-1,0};SuspensionDegradedDynamicsInputV3 input;input.externalLoadCount=1;input.externalLoads[0].frameId=2;input.externalLoads[0].forceN={10000,0,0};
    for(int i=0;i<20;++i){auto r=stepSuspensionDegradedDynamicsV3(d,intact,input,0.001);assert(r.valid);}const double intactDist=suspNormV3(intact.bodies[1].position-intact.bodies[0].position);assert(std::abs(intactDist-1.0)<0.02);
    SuspensionDegradedDynamicsStateV3 broken;broken.bodies[0].position={0,0,0};broken.bodies[1].position={0,-1,0};broken.elements.elements[0].constraintEnabled=false;broken.elements.elements[0].damage.flags=DamageBrokenV2;assert(suspensionRequiresDegradedDynamicsV3(d.graph,broken.elements));
    for(int i=0;i<20;++i){auto r=stepSuspensionDegradedDynamicsV3(d,broken,input,0.001);assert(r.valid);}const double brokenDist=suspNormV3(broken.bodies[1].position-broken.bodies[0].position);assert(brokenDist>intactDist+0.01);
}


static void testProviderRegistryV3()
{
    SuspensionProviderRegistryV3 r;
    constexpr std::array<SuspensionProviderKind,12> canonical{{SuspensionProviderKind::MacPhersonStrut,SuspensionProviderKind::DoubleWishbone,SuspensionProviderKind::PushrodRockerWishbone,SuspensionProviderKind::RigidLiveAxle,SuspensionProviderKind::LeafSpringLiveAxle,SuspensionProviderKind::MotorcycleForkSwingarm,SuspensionProviderKind::SemiTrailingArm,SuspensionProviderKind::TwistBeam,SuspensionProviderKind::MultiLink,SuspensionProviderKind::SwingAxle,SuspensionProviderKind::SlidingPillar,SuspensionProviderKind::MotorcycleLinkFront}};
    for(auto k:canonical)assert(r.registerProvider(k,&testProvider));
    assert(registerStandardSuspensionAliasesV3(r));assert(r.completeForProduction());assert(r.count()==15);
}

static void testMixed150VehicleWorkload()
{
    TestProviderDescription pd;std::vector<TestProviderState> providers(150);std::vector<SuspensionCornerGraphStateV3> states(150);
    SuspensionCornerGraphDescriptionV3 d;d.graph.count=6;
    for(std::size_t i=0;i<6;++i){d.graph.elements[i]=makeSpring(static_cast<std::uint32_t>(200+i),2,0,1.0);d.graph.elements[i].progressive.linearRateNPerM=10000.0+5000.0*i;if(i==4)d.graph.elements[i].kind=SuspensionElementKindV3::DualRateSpring;if(i==5)d.graph.elements[i].kind=SuspensionElementKindV3::BumpStop;d.graph.elements[i].damageEnabled=false;}
    SuspensionCornerGraphControlV3 control;double checksum=0;
    for(int step=0;step<1000;++step)
    {
        for(std::size_t v=0;v<150;++v)
        {
            SuspensionCornerGraphInputV3 in;in.provider={&pd,&providers[v],&testProvider};in.geometryRequest.wheelCompressionM=0.025*std::sin(0.01*step+0.03*v);in.geometryRequest.wheelCompressionVelocityMps=0.25*std::cos(0.01*step+0.03*v);in.geometryRequest.wheelCompressionAccelerationMps2=-0.0025*std::sin(0.01*step+0.03*v);
            const auto r=stepSuspensionCornerGraphV3(d,states[v],in,control,0.001);assert(r.valid&&std::isfinite(r.generalizedWheelForceN));checksum+=r.generalizedWheelForceN*1e-12;
        }
    }
    assert(std::isfinite(checksum));for(const auto& p:providers)assert(p.commits==1000);
}

int main()
{
    testGeometryAndMultipleElements();
    testNonlinearRockerJacobian();
    testComponentDamageAndCompliance();
    testSerialization();
    testFullCatalog();
    testConstraintLoadAndOverrideContract();
    testAxleGeometryOwnership();
    testDegradedMultibodyFallback();
    testProviderRegistryV3();
    testMixed150VehicleWorkload();
    std::cout<<"SUSP27_SUSP30_FINAL_CERTIFICATION PASS\n";
    return 0;
}
