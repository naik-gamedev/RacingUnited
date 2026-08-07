#pragma once

namespace heritage::graphics {

struct RenderSize
{
    int width = 1;
    int height = 1;
};

class RenderScaler
{
public:
    // Uses the existing Video menu scale-mode indices without changing behavior:
    // 0 Native, 1 Integer x1, 2 Integer x2, 3 Integer x3,
    // 4 Half (50%), 5 Quarter (25%).
    static RenderSize calculateRenderSize(int framebufferWidth,
                                          int framebufferHeight,
                                          int scaleModeIndex);

    static bool requiresScaling(int framebufferWidth,
                                int framebufferHeight,
                                const RenderSize& renderSize);

    static bool usesNearestNeighbour(int scaleModeIndex);
};

} // namespace heritage::graphics
