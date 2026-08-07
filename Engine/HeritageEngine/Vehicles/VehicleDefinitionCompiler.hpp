#pragma once

#include "VehicleDefinition.hpp"

namespace heritage::vehicles {

// Resolves authored string references into stable indices and determines which
// runtime provider can execute the resulting component graph. Classification
// metadata is deliberately ignored when selecting a provider.
class VehicleDefinitionCompiler
{
public:
    static VehicleDefinitionCompileResult compile(
        const VehicleDefinitionV2Source& source);
};

} // namespace heritage::vehicles
