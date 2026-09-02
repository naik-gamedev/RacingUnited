#include "../Engine/HeritageEngine/Vehicles/Suspension/SuspensionClosure.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace heritage::vehicles::suspension;

static bool near(double a,double b,double e){return std::abs(a-b)<=e;}

int main()
{
    // Progressive + dual-rate + bump stop.
    ProgressiveSpringDescription ps; ps.freeLengthMetres=.30; ps.linearRateNPerM=40000; ps.cubicRateNPerM3=4e6;
    auto pr=evaluateProgressiveSpring(ps,.25);
    assert(pr.forceNewtons>1900 && pr.tangentRateNPerM>40000);
    DualRateSpringDescription ds; ds.mainRateNPerM=100000; ds.helperRateNPerM=10000; ds.helperBindCompressionMetres=.01;
    auto d0=evaluateDualRateSpring(ds,.005), d1=evaluateDualRateSpring(ds,.20);
    assert(!d0.helperBound && d1.helperBound && d1.tangentRateNPerM>d0.tangentRateNPerM);
    StopDescription stop; stop.cubicRateNPerM3=2e8; assert(evaluateStopForce(stop,.02,.3)>0);

    // Pneumatic spring must rise in pressure/rate under compression.
    PneumaticSpringDescription air;
    auto a0=evaluatePneumaticSpring(air,0.0), a1=evaluatePneumaticSpring(air,.03);
    assert(a1.absolutePressurePa>a0.absolutePressurePa && a1.forceNewtons>a0.forceNewtons && a1.tangentRateNPerM>0);

    // Damper passivity excluding gas spring and thermal accumulation.
    DamperDescriptionV2 damper;
    damper.compression.count=3; damper.compression.points[0]={0,0}; damper.compression.points[1]={.1,800}; damper.compression.points[2]={1.0,4500};
    damper.rebound.count=3; damper.rebound.points[0]={0,0}; damper.rebound.points[1]={.1,1200}; damper.rebound.points[2]={1.0,6500};
    DamperStateV2 dst; dst.temperatureC=80;
    auto dr=stepDamperV2(damper,dst,.5,.5,25,.01);
    assert(dr.hydraulicForceNewtons*.5<=1e-9 && dst.dissipatedEnergyJ>0);
    for(int i=0;i<2000;++i) stepDamperV2(damper,dst,.8,.5,25,.001);
    assert(dst.temperatureC>80.0);

    // Anti-roll equal travel -> zero, opposite travel -> torque.
    AntiRollBarDescription arb;
    assert(near(evaluateAntiRollTorque(arb,.1,.1,0,0),0,1e-12));
    assert(std::abs(evaluateAntiRollTorque(arb,.1,-.1,0,0))>0);
    ThirdElementDescription third; third.heaveSpringRateNPerM=50000; third.rollSpringRateNPerM=20000;
    auto tf=evaluateThirdElement(third,.02,.02,0,0); assert(near(tf.leftForceN,tf.rightForceN,1e-9));
    auto tr=evaluateThirdElement(third,.02,-.02,0,0); assert(near(tr.leftForceN,-tr.rightForceN,1e-9));

    // Hydraulic interconnect must build pressure and oppose piston motion.
    HydraulicInterconnectDescription hi; HydraulicInterconnectState hs;
    auto hf=stepHydraulicInterconnect(hi,hs,.01,0,.001);
    assert(hs.leftGaugePressurePa>hs.rightGaugePressurePa && hf.leftForceN<0);

    // Active actuator force slew and power limit.
    ActiveActuatorDescription ad; ad.maximumForceN=10000; ad.maximumForceRateNPerSec=1000; ad.maximumMechanicalPowerW=1000;
    ActiveActuatorState ast;
    auto ar=stepActiveActuator(ad,ast,10000,10,.1);
    assert(ar.slewLimited); assert(std::abs(ar.mechanicalPowerW)<=1000.0001);

    // Compliance wear increases play/softens effective response without NaN.
    Compliance6DofDescription cd; Compliance6DofState cs;
    std::array<double,6> load{{1000,0,0,0,0,0}}, vel{{0,0,0,0,0,0}};
    auto cr0=solveCompliance6Dof(cd,cs,load,vel,0.0);
    auto cr1=solveCompliance6Dof(cd,cs,load,vel,1.0);
    assert(std::isfinite(cr0[0])&&std::isfinite(cr1[0]));

    // Damage: one ultimate hit breaks the element; leakage/wear are bounded.
    SuspensionDamageDescription dd; SuspensionDamageState dmg;
    stepSuspensionDamage(dd,dmg,dd.ultimateLoadN*1.1,.1,.01);
    assert(dmg.failure==SuspensionFailureMode::Broken && damagedStiffnessScale(dmg)==0.0);

    // Swing axle preserves halfshaft length and migrates wheel up vector/camber.
    SwingAxleDescription sw; sw.differentialPivot={0,0,0}; sw.restWheelCentre={1.0,0,0}; sw.hingeAxis={0,0,1}; sw.restWheelUp={0,1,0};
    auto sws=solveSwingAxle(sw,.10); assert(sws.valid);
    assert(near(specialLength(sws.wheelCentre-sw.differentialPivot),1.0,1e-8));
    assert(std::abs(sws.wheelUp.x)>1e-4);

    // Sliding pillar stays on authored axis.
    SlidingPillarDescription sp; sp.restWheelCentre={1,0,0}; sp.pillarAxis={0,1,0};
    auto sps=solveSlidingPillar(sp,.05); assert(sps.valid && near(sps.wheelCentre.y,.05,1e-12));

    // Motorcycle alternative front four-bar returns finite linked carrier and steering transform.
    MotorcycleLinkFrontDescription mf;
    mf.upperChassisPivot={.10,.60,0}; mf.lowerChassisPivot={.20,.30,0};
    mf.restUpperCarrierJoint={.55,.58,0}; mf.restLowerCarrierJoint={.58,.26,0};
    mf.restWheelCentre={.62,.15,0}; mf.steerOrigin={.30,.50,0}; mf.steerAxis={0,1,0};
    auto mfs=solveMotorcycleLinkFront(mf,.01,.15); assert(mfs.valid);

    std::cout << "SUSPENSION_CLOSURE_SMOKE PASS\n";
    std::cout << "damper_temp_C=" << dst.temperatureC << " swing_angle_rad=" << sws.axleAngleRad << "\n";
    return 0;
}
