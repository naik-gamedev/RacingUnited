#pragma once
#include "SuspensionProductionRuntime.hpp"
#include "SuspensionMath.hpp"
#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>

namespace heritage::vehicles::suspension
{
struct SuspensionStatePacketV2
{
    std::vector<std::uint8_t> bytes;
    std::uint64_t contentHash=0;
};
namespace detail
{
inline void putU32(std::vector<std::uint8_t>& b,std::uint32_t v){for(int i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>((v>>(i*8))&0xffu));}
inline void putU64(std::vector<std::uint8_t>& b,std::uint64_t v){for(int i=0;i<8;++i)b.push_back(static_cast<std::uint8_t>((v>>(i*8))&0xffu));}
inline void putD(std::vector<std::uint8_t>& b,double v){putU64(b,std::bit_cast<std::uint64_t>(v));}
inline bool getU32(const std::vector<std::uint8_t>& b,std::size_t& p,std::uint32_t& v){if(p+4>b.size())return false;v=0;for(int i=0;i<4;++i)v|=std::uint32_t(b[p++])<<(i*8);return true;}
inline bool getU64(const std::vector<std::uint8_t>& b,std::size_t& p,std::uint64_t& v){if(p+8>b.size())return false;v=0;for(int i=0;i<8;++i)v|=std::uint64_t(b[p++])<<(i*8);return true;}
inline bool getD(const std::vector<std::uint8_t>& b,std::size_t& p,double& v){std::uint64_t u=0;if(!getU64(b,p,u))return false;v=std::bit_cast<double>(u);return std::isfinite(v);}
inline void putVec6(std::vector<std::uint8_t>& b,const SuspVec6& v){for(double x:v)putD(b,x);}
inline bool getVec6(const std::vector<std::uint8_t>& b,std::size_t& p,SuspVec6& v){for(double& x:v)if(!getD(b,p,x))return false;return true;}
}
inline SuspensionStatePacketV2 serializeSuspensionRuntimeV2(const VehicleSuspensionRuntimeV2& s)
{
    SuspensionStatePacketV2 p;auto& b=p.bytes;
    detail::putU32(b,0x32535553u); // SUS2 little-endian
    detail::putU32(b,3u);
    detail::putU32(b,static_cast<std::uint32_t>(s.cornerCount));detail::putU32(b,static_cast<std::uint32_t>(s.axleCount));
    detail::putU64(b,s.stepCounter);
    for(std::size_t i=0;i<s.cornerCount&&i<VehicleSuspensionRuntimeV2::MaxCorners;++i)
    {
        const auto& c=s.corners[i];
        // air
        detail::putD(b,c.air.chamberMassKg);detail::putD(b,c.air.chamberTemperatureK);detail::putD(b,c.air.reservoirMassKg);detail::putD(b,c.air.reservoirTemperatureK);
        detail::putD(b,c.air.cumulativeCompressorMassKg);detail::putD(b,c.air.cumulativeLeakMassKg);detail::putU32(b,c.air.initialized?1u:0u);
        // hydro
        detail::putD(b,c.hydro.displacedFluidM3);detail::putD(b,c.hydro.lineGaugePressurePa);detail::putD(b,c.hydro.gasTemperatureK);detail::putD(b,c.hydro.cumulativeLeakM3);
        // damper
        detail::putD(b,c.damper.compressionPressurePa);detail::putD(b,c.damper.reboundPressurePa);detail::putD(b,c.damper.gasPressurePa);detail::putD(b,c.damper.gasVolumeM3);
        detail::putD(b,c.damper.compressionShimOpen);detail::putD(b,c.damper.reboundShimOpen);detail::putD(b,c.damper.aeration);detail::putD(b,c.damper.temperatureC);
        detail::putD(b,c.damper.dissipatedEnergyJ);detail::putD(b,c.damper.leakedOilM3);detail::putU32(b,c.damper.initialized?1u:0u);
        // compliance
        detail::putVec6(b,c.compliance.deflection);detail::putVec6(b,c.compliance.velocity);detail::putVec6(b,c.compliance.hysteresis);detail::putVec6(b,c.complianceOffsetsForNextKinematics);detail::putVec6(b,c.damageOffsetsForNextKinematics);
        // damage
        detail::putD(b,c.damage.fatigue);detail::putD(b,c.damage.permanentSet);detail::putD(b,c.damage.leakage);detail::putD(b,c.damage.wear);detail::putD(b,c.damage.overloadSeconds);detail::putD(b,c.damage.brokenSeconds);detail::putU32(b,c.damage.flags);
        // actuator and scalar history
        detail::putD(b,c.actuator.forceN);detail::putD(b,c.actuator.electricalEnergyJ);detail::putD(b,c.actuator.regeneratedEnergyJ);
        detail::putD(b,c.previousWheelCompressionM);detail::putD(b,c.accumulatedSpringEnergyJ);
    }
    for(std::size_t i=0;i<s.axleCount&&i<VehicleSuspensionRuntimeV2::MaxAxles;++i)
    {detail::putD(b,s.axles[i].hydraulic.leftGaugePressurePa);detail::putD(b,s.axles[i].hydraulic.rightGaugePressurePa);}
    p.contentHash=suspHash64(1469598103934665603ull,b.data(),b.size());
    return p;
}
inline bool deserializeSuspensionRuntimeV2(const SuspensionStatePacketV2& packet,VehicleSuspensionRuntimeV2& s)
{
    const auto& b=packet.bytes;
    if(packet.contentHash!=suspHash64(1469598103934665603ull,b.data(),b.size()))return false;
    std::size_t p=0;std::uint32_t magic=0,version=0,cc=0,ac=0;std::uint64_t steps=0;
    if(!detail::getU32(b,p,magic)||magic!=0x32535553u||!detail::getU32(b,p,version)||version!=3u)return false;
    if(!detail::getU32(b,p,cc)||!detail::getU32(b,p,ac)||!detail::getU64(b,p,steps))return false;
    if(cc>VehicleSuspensionRuntimeV2::MaxCorners||ac>VehicleSuspensionRuntimeV2::MaxAxles)return false;
    VehicleSuspensionRuntimeV2 tmp;tmp.cornerCount=cc;tmp.axleCount=ac;tmp.stepCounter=steps;
    for(std::size_t i=0;i<cc;++i)
    {
        auto& c=tmp.corners[i];std::uint32_t flag=0;
#define GD(x) do{if(!detail::getD(b,p,(x)))return false;}while(false)
        GD(c.air.chamberMassKg);GD(c.air.chamberTemperatureK);GD(c.air.reservoirMassKg);GD(c.air.reservoirTemperatureK);GD(c.air.cumulativeCompressorMassKg);GD(c.air.cumulativeLeakMassKg);if(!detail::getU32(b,p,flag))return false;c.air.initialized=flag!=0;
        GD(c.hydro.displacedFluidM3);GD(c.hydro.lineGaugePressurePa);GD(c.hydro.gasTemperatureK);GD(c.hydro.cumulativeLeakM3);
        GD(c.damper.compressionPressurePa);GD(c.damper.reboundPressurePa);GD(c.damper.gasPressurePa);GD(c.damper.gasVolumeM3);GD(c.damper.compressionShimOpen);GD(c.damper.reboundShimOpen);GD(c.damper.aeration);GD(c.damper.temperatureC);GD(c.damper.dissipatedEnergyJ);GD(c.damper.leakedOilM3);if(!detail::getU32(b,p,flag))return false;c.damper.initialized=flag!=0;
        if(!detail::getVec6(b,p,c.compliance.deflection)||!detail::getVec6(b,p,c.compliance.velocity)||!detail::getVec6(b,p,c.compliance.hysteresis)||!detail::getVec6(b,p,c.complianceOffsetsForNextKinematics)||!detail::getVec6(b,p,c.damageOffsetsForNextKinematics))return false;
        GD(c.damage.fatigue);GD(c.damage.permanentSet);GD(c.damage.leakage);GD(c.damage.wear);GD(c.damage.overloadSeconds);GD(c.damage.brokenSeconds);if(!detail::getU32(b,p,c.damage.flags))return false;
        GD(c.actuator.forceN);GD(c.actuator.electricalEnergyJ);GD(c.actuator.regeneratedEnergyJ);GD(c.previousWheelCompressionM);GD(c.accumulatedSpringEnergyJ);
#undef GD
    }
    for(std::size_t i=0;i<ac;++i){if(!detail::getD(b,p,tmp.axles[i].hydraulic.leftGaugePressurePa)||!detail::getD(b,p,tmp.axles[i].hydraulic.rightGaugePressurePa))return false;}
    if (p != b.size()) return false;
    s = tmp;
    return true;
}
}
