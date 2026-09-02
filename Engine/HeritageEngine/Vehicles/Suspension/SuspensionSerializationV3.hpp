#pragma once
#include "SuspensionProductionRuntimeV3.hpp"
#include "SuspensionMath.hpp"
#include <bit>
#include <cstdint>
#include <vector>

namespace heritage::vehicles::suspension
{
struct SuspensionStatePacketV3{std::vector<std::uint8_t> bytes;std::uint64_t contentHash=0;};
namespace detailv3
{
inline void putU32(std::vector<std::uint8_t>& b,std::uint32_t v){for(int i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>((v>>(8*i))&255u));}
inline void putU64(std::vector<std::uint8_t>& b,std::uint64_t v){for(int i=0;i<8;++i)b.push_back(static_cast<std::uint8_t>((v>>(8*i))&255u));}
inline void putD(std::vector<std::uint8_t>& b,double v){putU64(b,std::bit_cast<std::uint64_t>(v));}
inline bool getU32(const std::vector<std::uint8_t>& b,std::size_t& p,std::uint32_t& v){if(p+4>b.size())return false;v=0;for(int i=0;i<4;++i)v|=std::uint32_t(b[p++])<<(8*i);return true;}
inline bool getU64(const std::vector<std::uint8_t>& b,std::size_t& p,std::uint64_t& v){if(p+8>b.size())return false;v=0;for(int i=0;i<8;++i)v|=std::uint64_t(b[p++])<<(8*i);return true;}
inline bool getD(const std::vector<std::uint8_t>& b,std::size_t& p,double& v){std::uint64_t u=0;if(!getU64(b,p,u))return false;v=std::bit_cast<double>(u);return std::isfinite(v);}
inline void putVec6(std::vector<std::uint8_t>& b,const SuspVec6& v){for(double x:v)putD(b,x);}
inline bool getVec6(const std::vector<std::uint8_t>& b,std::size_t& p,SuspVec6& v){for(double& x:v)if(!getD(b,p,x))return false;return true;}
inline void putDamage(std::vector<std::uint8_t>& b,const SuspensionDamageStateV2& x){putD(b,x.fatigue);putD(b,x.permanentSet);putD(b,x.leakage);putD(b,x.wear);putD(b,x.overloadSeconds);putD(b,x.brokenSeconds);putU32(b,x.flags);}
inline bool getDamage(const std::vector<std::uint8_t>& b,std::size_t& p,SuspensionDamageStateV2& x){return getD(b,p,x.fatigue)&&getD(b,p,x.permanentSet)&&getD(b,p,x.leakage)&&getD(b,p,x.wear)&&getD(b,p,x.overloadSeconds)&&getD(b,p,x.brokenSeconds)&&getU32(b,p,x.flags);}
inline void putElement(std::vector<std::uint8_t>& b,const SuspensionElementStateV3& e)
{
    putD(b,e.air.chamberMassKg);putD(b,e.air.chamberTemperatureK);putD(b,e.air.reservoirMassKg);putD(b,e.air.reservoirTemperatureK);putD(b,e.air.cumulativeCompressorMassKg);putD(b,e.air.cumulativeLeakMassKg);putU32(b,e.air.initialized?1u:0u);
    putD(b,e.hydro.displacedFluidM3);putD(b,e.hydro.lineGaugePressurePa);putD(b,e.hydro.gasTemperatureK);putD(b,e.hydro.cumulativeLeakM3);
    putD(b,e.damper.compressionPressurePa);putD(b,e.damper.reboundPressurePa);putD(b,e.damper.gasPressurePa);putD(b,e.damper.gasVolumeM3);putD(b,e.damper.compressionShimOpen);putD(b,e.damper.reboundShimOpen);putD(b,e.damper.aeration);putD(b,e.damper.temperatureC);putD(b,e.damper.dissipatedEnergyJ);putD(b,e.damper.leakedOilM3);putU32(b,e.damper.initialized?1u:0u);
    putD(b,e.actuator.forceN);putD(b,e.actuator.electricalEnergyJ);putD(b,e.actuator.regeneratedEnergyJ);
    putVec6(b,e.compliance.deflection);putVec6(b,e.compliance.velocity);putVec6(b,e.compliance.hysteresis);putVec6(b,e.complianceFeedback);putVec6(b,e.permanentSetFeedback);putDamage(b,e.damage);
    putD(b,e.previousLengthM);putD(b,e.previousPathVelocityMps);putD(b,e.accumulatedEnergyJ);putD(b,e.lastForceN);putU32(b,e.initialized?1u:0u);putU32(b,e.constraintEnabled?1u:0u);
}
inline bool getElement(const std::vector<std::uint8_t>& b,std::size_t& p,SuspensionElementStateV3& e)
{
    std::uint32_t f=0;
#define G(x) do{if(!getD(b,p,(x)))return false;}while(false)
    G(e.air.chamberMassKg);G(e.air.chamberTemperatureK);G(e.air.reservoirMassKg);G(e.air.reservoirTemperatureK);G(e.air.cumulativeCompressorMassKg);G(e.air.cumulativeLeakMassKg);if(!getU32(b,p,f))return false;e.air.initialized=f!=0;
    G(e.hydro.displacedFluidM3);G(e.hydro.lineGaugePressurePa);G(e.hydro.gasTemperatureK);G(e.hydro.cumulativeLeakM3);
    G(e.damper.compressionPressurePa);G(e.damper.reboundPressurePa);G(e.damper.gasPressurePa);G(e.damper.gasVolumeM3);G(e.damper.compressionShimOpen);G(e.damper.reboundShimOpen);G(e.damper.aeration);G(e.damper.temperatureC);G(e.damper.dissipatedEnergyJ);G(e.damper.leakedOilM3);if(!getU32(b,p,f))return false;e.damper.initialized=f!=0;
    G(e.actuator.forceN);G(e.actuator.electricalEnergyJ);G(e.actuator.regeneratedEnergyJ);
    if(!getVec6(b,p,e.compliance.deflection)||!getVec6(b,p,e.compliance.velocity)||!getVec6(b,p,e.compliance.hysteresis)||!getVec6(b,p,e.complianceFeedback)||!getVec6(b,p,e.permanentSetFeedback)||!getDamage(b,p,e.damage))return false;
    G(e.previousLengthM);G(e.previousPathVelocityMps);G(e.accumulatedEnergyJ);G(e.lastForceN);if(!getU32(b,p,f))return false;e.initialized=f!=0;if(!getU32(b,p,f))return false;e.constraintEnabled=f!=0;
#undef G
    return true;
}
}
inline SuspensionStatePacketV3 serializeSuspensionRuntimeV3(const SuspensionVehicleGraphDescriptionV3& d,const SuspensionVehicleGraphStateV3& s)
{
    SuspensionStatePacketV3 p;auto& b=p.bytes;detailv3::putU32(b,0x33535553u);detailv3::putU32(b,1u);detailv3::putU32(b,static_cast<std::uint32_t>(d.cornerCount));detailv3::putU32(b,static_cast<std::uint32_t>(d.axleCount));detailv3::putU64(b,s.stepCounter);
    for(std::size_t c=0;c<d.cornerCount;++c){detailv3::putU32(b,static_cast<std::uint32_t>(d.corners[c].graph.count));for(std::size_t i=0;i<d.corners[c].graph.count;++i)detailv3::putElement(b,s.corners[c].graph.elements[i]);}
    for(std::size_t a=0;a<d.axleCount;++a){detailv3::putD(b,s.axles[a].hydraulic.leftGaugePressurePa);detailv3::putD(b,s.axles[a].hydraulic.rightGaugePressurePa);}
    p.contentHash=suspHash64(1469598103934665603ull,b.data(),b.size());return p;
}
inline bool deserializeSuspensionRuntimeV3(const SuspensionVehicleGraphDescriptionV3& d,const SuspensionStatePacketV3& p,SuspensionVehicleGraphStateV3& s)
{
    if(p.contentHash!=suspHash64(1469598103934665603ull,p.bytes.data(),p.bytes.size()))return false;
    std::size_t pos=0;std::uint32_t magic=0,ver=0,cc=0,ac=0;std::uint64_t steps=0;if(!detailv3::getU32(p.bytes,pos,magic)||magic!=0x33535553u||!detailv3::getU32(p.bytes,pos,ver)||ver!=1u||!detailv3::getU32(p.bytes,pos,cc)||!detailv3::getU32(p.bytes,pos,ac)||!detailv3::getU64(p.bytes,pos,steps))return false;
    if(cc!=d.cornerCount||ac!=d.axleCount)return false;
    SuspensionVehicleGraphStateV3 tmp;tmp.stepCounter=steps;
    for(std::size_t c=0;c<d.cornerCount;++c){std::uint32_t ec=0;if(!detailv3::getU32(p.bytes,pos,ec)||ec!=d.corners[c].graph.count)return false;for(std::size_t i=0;i<ec;++i)if(!detailv3::getElement(p.bytes,pos,tmp.corners[c].graph.elements[i]))return false;}
    for(std::size_t a=0;a<d.axleCount;++a)if(!detailv3::getD(p.bytes,pos,tmp.axles[a].hydraulic.leftGaugePressurePa)||!detailv3::getD(p.bytes,pos,tmp.axles[a].hydraulic.rightGaugePressurePa))return false;
    if(pos!=p.bytes.size())return false;
    s=tmp;return true;
}
}
