#pragma once

// Heritage Engine transform-math ownership scaffold.
//
// Quaternion algebra now lives in Quaternion.hpp.  CLEAN04B and later cleanup
// steps may graduate genuinely shared point/pose/world-local helpers here only
// when their coordinate-space contracts are identical across subsystems.
// Keeping this file deliberate and small avoids creating a new generic-math
// dumping ground while making the intended architecture visible early.

#include "Math.hpp"
#include "Quaternion.hpp"

namespace heritage::math
{

// Intentionally no policy-bearing helpers yet.  Physics bodies, entity
// hierarchy and vehicle topology still own different transform semantics.

} // namespace heritage::math
