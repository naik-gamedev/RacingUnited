#pragma once

#include <glad/glad.h>

namespace heritage::graphics {

// Coordinates the fullscreen post-processing passes that are currently used
// by the engine. The anti-aliasing mode selection remains in AntiAliasing;
// this class only owns the OpenGL shaders and fullscreen triangle used to
// execute FXAA and final texture blits.
class PostProcessor
{
public:
    bool initialize();
    void shutdown();

    void applyFxaa(GLuint sceneTexture, int sourceWidth, int sourceHeight,
                   GLuint destinationFramebuffer, int destinationWidth, int destinationHeight) const;

    void blit(GLuint sceneTexture, GLuint destinationFramebuffer,
              int destinationWidth, int destinationHeight, bool nearestNeighbour) const;

private:
    GLuint m_fxaaProgram = 0;
    GLuint m_blitProgram = 0;
    GLuint m_fullscreenVao = 0;
    GLint m_fxaaUniformScene = -1;
    GLint m_fxaaUniformTexelSize = -1;
    GLint m_blitUniformScene = -1;
    GLint m_blitUniformNearestNeighbour = -1;
};

} // namespace heritage::graphics
