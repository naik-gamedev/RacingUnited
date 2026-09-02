#include "../Engine/HeritageEngine/Vehicles/Suspension/SuspensionProduction.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace heritage::vehicles::suspension;
static void require(bool ok,const char* m){if(!ok){std::cerr<<"FAIL "<<m<<"\n";std::exit(2);}std::cout<<"PASS "<<m<<"\n";}
static bool near(double a,double b,double e){return std::abs(a-b)<=e;}

static SuspensionCornerDescriptionV2 makeCorner()
{
    SuspensionCornerDescriptionV2 d;
    d.springKind=SpringHardwareKindV2::ProgressiveCoil;
    d.progressive.freeLengthMetres=0.30;d.progressive.preloadNewtons=800.0;d.progressive.linearRateNPerM=35000.0;d.progressive.quadraticRateNPerM2=120000.0;
    d.bumpStop.linearRateNPerM=120000.0;d.bumpStop.cubicRateNPerM3=4e8;d.reboundStop.linearRateNPerM=80000.0;
    d.damper.pistonAreaM2=0.0012;d.damper.rodAreaM2=0.00016;d.damper.compressionValve.bleedConductanceM3PerSecPerPa=2e-12;d.damper.reboundValve.bleedConductanceM3PerSecPerPa=1e-12;
    d.damper.compressionValve.orificeCoefficientM3PerSecPerSqrtPa=2.5e-8;d.damper.reboundValve.orificeCoefficientM3PerSecPerSqrtPa=1.8e-8;
    d.complianceEnabled=true;d.compliance.coulombFriction={20,20,20,1,1,1};d.compliance.hysteresisStrength={50,50,50,2,2,2};
    d.activeActuatorEnabled=true;d.actuator.maximumForceN=8000;d.actuator.stallForceN=9000;d.actuator.noLoadSpeedMps=1.5;d.actuator.maximumExtensionSpeedMps=1.5;d.actuator.maximumMechanicalPowerW=5000;
    d.damage.ultimateLoadN=90000;d.damage.detachLoadN=110000;
    return d;
}
static SuspensionKinematicSampleV2 sampleAt(double t)
{
    SuspensionKinematicSampleV2 k;
    const double w=2.0*3.14159265358979323846*2.0;
    k.wheelCompressionM=0.035*std::sin(w*t);k.wheelCompressionVelocityMps=0.035*w*std::cos(w*t);k.wheelCompressionAccelerationMps2=-0.035*w*w*std::sin(w*t);
    k.springCompressionM=0.08+k.wheelCompressionM*0.72;k.springCompressionVelocityMps=k.wheelCompressionVelocityMps*0.72;k.springMotionRatio=0.72;
    k.damperCompressionM=0.06+k.wheelCompressionM*0.65;k.damperCompressionVelocityMps=k.wheelCompressionVelocityMps*0.65;k.damperMotionRatio=0.65;
    k.actuatorExtensionVelocityMps=-k.wheelCompressionVelocityMps*0.8;k.actuatorExtensionMotionRatio=-0.8;
    k.bumpStopPenetrationM=std::max(0.0,k.wheelCompressionM-0.025);k.bumpStopVelocityMps=k.wheelCompressionVelocityMps;
    k.reboundStopPenetrationM=std::max(0.0,-k.wheelCompressionM-0.030);k.reboundStopVelocityMps=-k.wheelCompressionVelocityMps;
    k.hardpointGeneralizedLoad={5000.0*std::sin(w*t),1000,500,50,20,10};
    return k;
}
int main()
{
    // Dynamic compliance has bounded finite state and coupled off-axis response.
    Compliance6DofDescriptionV2 cd;cd.stiffness[1][0]=2.0e5;cd.stiffness[0][1]=2.0e5;Compliance6DofStateV2 cs;
    bool complianceFinite=true;
    for(int i=0;i<2000;++i){SuspVec6 l{1000,0,0,0,0,0};auto r=stepCompliance6DofV2(cd,cs,l,0.2,0.0005);complianceFinite=complianceFinite&&suspFinite6(r.deflection);}
    require(complianceFinite,"dynamic compliance finite");
    require(std::abs(cs.deflection[1])>1e-8,"coupled compliance cross-axis deflection");

    // Active actuator cannot claim force above force-speed or power envelope using clamped fake velocity.
    ActiveActuatorDescriptionV2 ad;ad.maximumForceN=12000;ad.stallForceN=12000;ad.noLoadSpeedMps=2;ad.maximumExtensionSpeedMps=2;ad.maximumMechanicalPowerW=3000;ad.maximumForceRateNPerSec=1e9;
    ActiveActuatorStateV2 as;auto ar=stepActiveActuatorV2(ad,as,12000,1.5,0.01);
    require(std::abs(ar.mechanicalPowerW)<=3000.0001,"active actuator real-velocity power limit");
    auto ar2=stepActiveActuatorV2(ad,as,12000,2.5,0.01);require(std::abs(ar2.forceN)<1e-9&&ar2.speedLimited,"active actuator no force beyond speed envelope");

    // Damper V3 uses pressure chambers/valves and remains dissipative rather than injecting hydraulic energy.
    DamperDescriptionV3 dam;dam.pistonAreaM2=0.0012;dam.rodAreaM2=0.00016;
    dam.compressionValve.bleedConductanceM3PerSecPerPa=2e-12;dam.reboundValve.bleedConductanceM3PerSecPerPa=1e-12;
    dam.compressionValve.orificeCoefficientM3PerSecPerSqrtPa=2.5e-8;dam.reboundValve.orificeCoefficientM3PerSecPerSqrtPa=1.8e-8;
    DamperStateV3 damS;double damDiss=0.0;bool damFinite=true;
    for(int i=0;i<2000;++i){double t=i*0.0005;double x=0.02*std::sin(t*20.0);double v=0.4*std::cos(t*20.0);auto rr=stepDamperV3(dam,damS,x,v,25.0,0.0005);damDiss+=rr.dissipatedPowerW*0.0005;damFinite=damFinite&&std::isfinite(rr.forceN)&&std::isfinite(rr.compressionPressurePa)&&rr.dissipatedPowerW>=0.0;}
    require(damFinite&&damDiss>0.0,"damper V3 finite dissipative chamber solve");

    // Air spring evolves thermal/mass state and loses pressure with an induced leak.
    AirSpringDescriptionV2 air;air.leakConductanceKgPerSecPerPa=2e-11;AirSpringStateV2 airS;
    auto a0=stepAirSpringV2(air,airS,0.03,0,101325,293.15,0,0.001,0);double p0=a0.chamberPressurePa;
    for(int i=0;i<3000;++i)stepAirSpringV2(air,airS,0.03,0,101325,293.15,0,0.001,1.0);
    auto a1=stepAirSpringV2(air,airS,0.03,0,101325,293.15,0,0,1.0);require(a1.chamberPressurePa<p0,"air spring leakage changes physical pressure");

    // Hydropneumatic sphere has line pressure/flow state and physical leakage.
    HydroPneumaticDescriptionV2 hd;hd.leakM3PerSecPerPa=1e-13;HydroPneumaticStateV2 hs;
    bool hydroFinite=true;for(int i=0;i<1500;++i){auto hr=stepHydroPneumaticV2(hd,hs,0.03,101325,293.15,0.0005,1.0);hydroFinite=hydroFinite&&std::isfinite(hr.forceN)&&std::isfinite(hr.linePressurePa);}
    require(hydroFinite&&hs.cumulativeLeakM3>0.0,"hydropneumatic thermal hydraulic leak state");

    // Damage progresses through leak/bend/break/detach rather than a cosmetic health scalar.
    SuspensionDamageDescriptionV2 dd;dd.yieldLoadN=1000;dd.ultimateLoadN=2000;dd.detachLoadN=3000;dd.brokenDetachDelayS=0.002;SuspensionDamageStateV2 ds;
    stepSuspensionDamageV2(dd,ds,1500,0.02,180,0.01);require((ds.flags&DamageBentV2)!=0u,"damage bent state");
    stepSuspensionDamageV2(dd,ds,2500,0.02,180,0.01);require((ds.flags&DamageBrokenV2)!=0u,"damage broken state");
    auto dres=stepSuspensionDamageV2(dd,ds,3500,0.02,230,0.01);require(dres.detached&&!dres.constraintEnabled,"damage detached removes constraint");

    // The provider catalog is now a real callback dispatcher, not only names/enums.
    auto registry=makeSuspensionProviderRegistryV2();require(registry.valid()&&registry.count()>=4,"production suspension provider registry valid");
    SwingAxleDescription sad;sad.differentialPivot={0.0,0.4,0.0};sad.restWheelCentre={0.75,0.3,0.0};sad.hingeAxis={1.0,0.0,0.0};
    SuspensionKinematicsRequestV2 skr;skr.requestedTravelM=0.01;SuspensionKinematicsPoseV2 skp;
    require(registry.solve(SuspensionProviderKind::SwingAxle,&sad,nullptr,skr,skp)&&skp.converged,"provider registry dispatches physical kinematics");

    // Motorcycle alternative front is now a full 3D double-A-arm carrier solve, then steering about the solved carrier axis.
    MotorcycleAArm3DDescription md;
    md.upperChassisFront={0.20,0.60,0.15}; md.upperChassisRear={0.20,0.60,-0.15};
    md.lowerChassisFront={0.05,0.25,0.18}; md.lowerChassisRear={0.05,0.25,-0.18};
    md.restUpperCarrierJoint={0.55,0.52,0.0}; md.restLowerCarrierJoint={0.52,0.20,0.0}; md.restWheelCentre={0.62,0.30,0.0};
    md.restForward={1.0,0.0,0.0}; md.travelAxis={0.0,1.0,0.0}; md.minimumTravelM=-0.05; md.maximumTravelM=0.05;
    auto m0=solveMotorcycleAArm3D(md,0.0,0.0,nullptr);
    auto m1=solveMotorcycleAArm3D(md,0.015,0.25,m0.converged?&m0:nullptr);
    require(m0.converged&&m1.converged&&m1.maxConstraintErrorM<1e-5,"3D motorcycle carrier hardpoint closure");
    require(std::abs(m1.wheelCentre.z)>1e-5,"3D motorcycle steering rotates wheel carrier");

    // Full corner runtime determinism and bounded telemetry over a 1 kHz excitation.
    auto desc=makeCorner();VehicleSuspensionRuntimeV2 va,vb;va.cornerCount=vb.cornerCount=4;
    double hashAccumA=0,hashAccumB=0;
    for(int step=0;step<5000;++step)
    {
        const double t=step*0.001;auto k=sampleAt(t);SuspensionCornerControlV2 ctl;ctl.activeForceCommandN=1500*std::sin(t*5.0);ctl.semiActiveValveScale=0.8+0.4*(0.5+0.5*std::sin(t));
        for(int c=0;c<4;++c)
        {
            auto ra=stepSuspensionCornerV2(desc,va.corners[c],k,ctl,101325,25,0.001);
            auto rb=stepSuspensionCornerV2(desc,vb.corners[c],k,ctl,101325,25,0.001);
            if(!std::isfinite(ra.supportForceN)||!std::isfinite(ra.telemetry.damperTemperatureC)){std::cerr<<"FAIL production corner finite\n";return 2;}
            hashAccumA+=ra.supportForceN*(c+1);hashAccumB+=rb.supportForceN*(c+1);
        }
        ++va.stepCounter;++vb.stepCounter;
    }
    require(true,"production corner finite");
    require(near(hashAccumA,hashAccumB,1e-9),"1kHz deterministic corner pipeline");

    // Leaf packs are part of the production corner dispatch, including interleaf friction.
    auto leafDesc=makeCorner();leafDesc.springKind=SpringHardwareKindV2::LeafPack;leafDesc.leafFriction.coulombForceNewtons=450.0;SuspensionCornerStateV2 leafState;auto leafK=sampleAt(0.0);leafK.springCompressionVelocityMps=0.2;
    auto leafR=stepSuspensionCornerV2(leafDesc,leafState,leafK,{},101325,25,0.001);require(leafR.telemetry.springFrictionForceN<0.0,"leaf interleaf friction in production force path");

    // Bent structure is fed forward as a real next-step kinematic hardpoint offset.
    auto bentDesc=makeCorner();bentDesc.damage.yieldLoadN=100.0;bentDesc.damage.ultimateLoadN=1e9;bentDesc.damage.detachLoadN=1e10;SuspensionCornerStateV2 bentState;auto bentK=sampleAt(0.1);bentK.hardpointGeneralizedLoad={5000,1000,500,50,20,10};
    stepSuspensionCornerV2(bentDesc,bentState,bentK,{},101325,25,0.02);auto bentOffset=suspensionMountOffsetForNextKinematicsV2(bentState);double bentNorm=0;for(double v:bentOffset)bentNorm+=v*v;require(bentNorm>0.0,"bent damage feeds next kinematic solve");

    // Coupled axle systems act equal/opposite in pure roll where appropriate.
    SuspensionAxleCouplingDescriptionV2 axd;axd.antiRollEnabled=true;axd.antiRoll.rateNmPerRad=10000;SuspensionAxleCouplingStateV2 axs;SuspensionAxleCouplingInputV2 axi;axi.leftBarAngleRad=0.05;axi.rightBarAngleRad=-0.05;
    auto axr=stepSuspensionAxleCouplingV2(axd,axs,axi,0.001);require(axr.leftGeneralizedForceN*axr.rightGeneralizedForceN<0.0,"anti-roll opposite wheel forces");
    SuspensionAxleCouplingDescriptionV2 ix;ix.inerterEnabled=true;ix.inerter.inertanceKg=120.0;ix.activeAntiRollEnabled=true;ix.activeAntiRoll.proportionalNmPerRad=5000.0;SuspensionAxleCouplingStateV2 ixs;SuspensionAxleCouplingInputV2 ixi;ixi.leftAccelerationMps2=4.0;ixi.rightAccelerationMps2=-2.0;ixi.bodyRollErrorRad=0.05;
    auto ixr=stepSuspensionAxleCouplingV2(ix,ixs,ixi,0.001);require(ixr.leftInerterForceN*ixr.rightInerterForceN<0.0,"inerter applied equal/opposite in production axle path");require(std::abs(ixr.activeAntiRollTorqueNm)>0.0,"active anti-roll applied in production axle path");

    // Whole-vehicle coordinator owns corner forces and axle coupling in one deterministic high-rate call.
    SuspensionVehicleDescriptionV2 vd;vd.cornerCount=4;vd.axleCount=2;for(int c=0;c<4;++c)vd.corners[c]=makeCorner();vd.axleMap[0]={0,1};vd.axleMap[1]={2,3};vd.axles[0].antiRollEnabled=true;vd.axles[1].thirdEnabled=true;vd.axles[1].third.heaveSpringRateNPerM=15000.0;
    VehicleSuspensionRuntimeV2 vs;SuspensionVehicleStepInputV2 vi;for(int c=0;c<4;++c)vi.kinematics[c]=sampleAt(0.25+c*0.01);auto vr=stepVehicleSuspensionV2(vd,vs,vi,0.001);require(vr.valid&&vs.stepCounter==1,"whole-vehicle production suspension coordinator");

    // Versioned state snapshot is bit-stable and round-trips every runtime owner.
    va.axleCount=1;va.axles[0]=axs;auto packet=serializeSuspensionRuntimeV2(va);VehicleSuspensionRuntimeV2 restored;
    require(deserializeSuspensionRuntimeV2(packet,restored),"suspension state deserialize");auto packet2=serializeSuspensionRuntimeV2(restored);
    require(packet.contentHash==packet2.contentHash&&packet.bytes==packet2.bytes,"replay/save/network snapshot bit roundtrip");

    // 150-vehicle mixed-fleet stepping smoke at 1kHz equivalent workload.
    std::vector<VehicleSuspensionRuntimeV2> fleet(150);for(auto& v:fleet)v.cornerCount=4;
    std::array<SuspensionCornerDescriptionV2,5> fleetTypes{makeCorner(),makeCorner(),makeCorner(),makeCorner(),makeCorner()};
    fleetTypes[1].springKind=SpringHardwareKindV2::DualRateCoil;fleetTypes[2].springKind=SpringHardwareKindV2::LeafPack;fleetTypes[2].leafFriction.coulombForceNewtons=300.0;fleetTypes[3].springKind=SpringHardwareKindV2::Air;fleetTypes[4].springKind=SpringHardwareKindV2::HydroPneumatic;
    double fleetChecksum=0.0;
    for(int step=0;step<250;++step){auto k=sampleAt(step*0.001);SuspensionCornerControlV2 ctl;for(std::size_t n=0;n<fleet.size();++n){auto& v=fleet[n];const auto& fd=fleetTypes[n%fleetTypes.size()];for(int c=0;c<4;++c)fleetChecksum+=stepSuspensionCornerV2(fd,v.corners[c],k,ctl,101325,25,0.001).supportForceN;}}
    require(std::isfinite(fleetChecksum),"150 vehicle mixed suspension fleet finite");

    std::cout<<"SUSP24_SUSP26_CERTIFICATION PASS checksum="<<fleetChecksum<<" hash="<<packet.contentHash<<"\n";
    return 0;
}
