#pragma once

#include <type_traits>

namespace heritage::vehicles {

// Heritage vehicle simulation precision policy.
//
// High-rate vehicle state and force-model arithmetic use FP64 on the CPU.
// Render-facing geometry remains FP32 and large-world/global transforms will
// migrate separately so rendering can stay camera-relative and GPU-friendly.
using VehicleScalar = double;

static_assert(std::is_same_v<VehicleScalar, double>);
static_assert(sizeof(VehicleScalar) == 8);

} // namespace heritage::vehicles
