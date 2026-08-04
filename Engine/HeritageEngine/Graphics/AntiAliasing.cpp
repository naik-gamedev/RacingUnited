#include "AntiAliasing.hpp"

#include <array>

namespace heritage::graphics
{
namespace
{

constexpr std::array<const char*, 7> kOptionNames = {
    "None",
    "MSAA x2",
    "MSAA x4",
    "MSAA x8",
    "FXAA",
    "FXAA + MSAA x2",
    "FXAA + MSAA x4",
};

} // namespace

const char* const* antiAliasingOptionNames()
{
    return kOptionNames.data();
}

int antiAliasingOptionCount()
{
    return static_cast<int>(kOptionNames.size());
}

AntiAliasingSettings resolveAntiAliasing(int optionIndex)
{
    switch (optionIndex)
    {
    case 1: return { 2, false };
    case 2: return { 4, false };
    case 3: return { 8, false };
    case 4: return { 1, true };
    case 5: return { 2, true };
    case 6: return { 4, true };
    default: return {};
    }
}

} // namespace heritage::graphics
