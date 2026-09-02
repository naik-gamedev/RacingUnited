#pragma once

#include <glad/glad.h>

namespace heritage::graphics {

// Framebuffer used by the current post-processing pipeline.
// Color is always texture-backed. Multisampled scenes can therefore be read
// directly by post effects instead of first resolving into a throwaway copy.
class PostFramebuffer
{
public:
    GLuint fbo = 0;
    GLuint tex = 0;
    GLuint colorRbo = 0;
    GLuint rbo = 0;
    GLuint depthStencilTex = 0;
    int w = 0;
    int h = 0;

    void init(int width, int height, int samples = 1);
    void destroy();
};

} // namespace heritage::graphics
