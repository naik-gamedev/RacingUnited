#pragma once

#include <string>

namespace heritage::modules {

enum class ModuleRuntimeActionType
{
    None,
    OpenEngineSettings,
    ExitApplication
};

struct ModuleRuntimeAction
{
    ModuleRuntimeActionType type = ModuleRuntimeActionType::None;
    std::string payload;
};

} // namespace heritage::modules
