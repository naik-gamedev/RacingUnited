#include "RenderScaler.hpp"
#include <algorithm>

namespace heritage::graphics {

RenderSize RenderScaler::calculateRenderSize(int framebufferWidth,
                                             int framebufferHeight,
                                             int scaleModeIndex)
{
    RenderSize result{ framebufferWidth, framebufferHeight };

    switch (scaleModeIndex)
    {
    case 0: // Native
    case 1: // Integer x1
        break;
    case 2: // Integer x2
        result.width = framebufferWidth / 2;
        result.height = framebufferHeight / 2;
        break;
    case 3: // Integer x3
        result.width = framebufferWidth / 3;
        result.height = framebufferHeight / 3;
        break;
    case 4: // Half (50%)
        result.width = framebufferWidth / 2;
        result.height = framebufferHeight / 2;
        break;
    case 5: // Quarter (25%)
        result.width = framebufferWidth / 4;
        result.height = framebufferHeight / 4;
        break;
    default:
        break;
    }

    result.width = std::max(result.width, 1);
    result.height = std::max(result.height, 1);
    return result;
}

bool RenderScaler::requiresScaling(int framebufferWidth,
                                   int framebufferHeight,
                                   const RenderSize& renderSize)
{
    return renderSize.width != framebufferWidth ||
           renderSize.height != framebufferHeight;
}

bool RenderScaler::usesNearestNeighbour(int scaleModeIndex)
{
    return scaleModeIndex >= 1 && scaleModeIndex <= 3;
}

} // namespace heritage::graphics
