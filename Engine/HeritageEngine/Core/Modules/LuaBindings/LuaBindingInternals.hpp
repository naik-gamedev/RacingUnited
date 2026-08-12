#pragma once

#include <algorithm>

namespace heritage::modules::lua_binding_detail {

inline constexpr int kLuaOk = 0;
inline constexpr int kLuaTypeNil = 0;
inline constexpr int kLuaTypeBoolean = 1;
inline constexpr int kLuaTypeNumber = 3;
inline constexpr int kLuaTypeString = 4;
inline constexpr int kLuaTypeTable = 5;
inline constexpr int kLuaTypeFunction = 6;

inline float clampFloat(float value, float minimum, float maximum)
{
    return (std::max)(minimum, (std::min)(maximum, value));
}

} // namespace heritage::modules::lua_binding_detail
