#include "PostFramebuffer.hpp"

#include <iostream>

namespace heritage::graphics {

void PostFramebuffer::init(int width, int height, int samples)
{
    destroy();
    w = width;
    h = height;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    if (samples > 1)
    {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);
        glTexImage2DMultisample(
            GL_TEXTURE_2D_MULTISAMPLE,
            samples,
            GL_RGB8,
            w,
            h,
            GL_TRUE);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D_MULTISAMPLE,
            tex,
            0);
    }
    else
    {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGB,
            w,
            h,
            0,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            tex,
            0);
    }

    if (samples > 1)
    {
        glGenTextures(1, &depthStencilTex);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, depthStencilTex);
        glTexImage2DMultisample(
            GL_TEXTURE_2D_MULTISAMPLE,
            samples,
            GL_DEPTH32F_STENCIL8,
            w,
            h,
            GL_TRUE);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D_MULTISAMPLE,
            depthStencilTex,
            0);
    }
    else
    {
        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, w, h);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER,
            rbo);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Post-processing framebuffer is incomplete.\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostFramebuffer::destroy()
{
    if (fbo)
    {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }

    if (tex)
    {
        glDeleteTextures(1, &tex);
        tex = 0;
    }

    if (colorRbo)
    {
        glDeleteRenderbuffers(1, &colorRbo);
        colorRbo = 0;
    }

    if (rbo)
    {
        glDeleteRenderbuffers(1, &rbo);
        rbo = 0;
    }

    if (depthStencilTex)
    {
        glDeleteTextures(1, &depthStencilTex);
        depthStencilTex = 0;
    }
}

} // namespace heritage::graphics
