#pragma once

#include "../Core/Math/Math.hpp"

namespace heritage::graphics::presentation {

// Presentation-only precision policy for bounded visual effects.
// Persistent/authoritative state may stay FP64, but GPU-facing coordinates are
// rebuilt every frame relative to the FP64 camera origin and then cast once to
// FP32. This keeps tire marks, marbles and particles millimetre-stable within
// their visual ranges without storing/rebasing persistent state in low precision.
inline heritage::math::Vec3 cameraRelativeFp32(
    const heritage::math::DVec3& global,
    const heritage::math::DVec3& cameraGlobal)
{
    return {
        static_cast<float>(global.x - cameraGlobal.x),
        static_cast<float>(global.y - cameraGlobal.y),
        static_cast<float>(global.z - cameraGlobal.z)
    };
}

} // namespace heritage::graphics::presentation
