#include "BackbufferClipboard.hpp"

#ifdef _WIN32

#include <glad/glad.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <cstring>
#include <vector>

namespace heritage::platform::windows {

bool copyBackbufferToClipboard(GLFWwindow* window, int width, int height)
{
    if (!window || width <= 0 || height <= 0)
        return false;

    // Read the final composited default backbuffer after scene + ImGui rendering.
    // OpenGL and a positive-height Windows DIB are both bottom-up, so no row flip
    // is required. This deliberately bypasses DWM/desktop capture, which can lag
    // behind an independently-presented OpenGL window on some Windows systems.
    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);

    GLint previousReadBuffer = GL_BACK;
    GLint previousPackAlignment = 4;
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());

    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));

    BITMAPINFOHEADER header{};
    header.biSize = sizeof(BITMAPINFOHEADER);
    header.biWidth = width;
    header.biHeight = height;
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = static_cast<DWORD>(pixels.size());

    const SIZE_T allocationSize = sizeof(BITMAPINFOHEADER) + pixels.size();
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, allocationSize);
    if (!memory)
        return false;

    void* destination = GlobalLock(memory);
    if (!destination)
    {
        GlobalFree(memory);
        return false;
    }

    std::memcpy(destination, &header, sizeof(header));
    std::memcpy(
        static_cast<unsigned char*>(destination) + sizeof(header),
        pixels.data(),
        pixels.size());
    GlobalUnlock(memory);

    HWND hwnd = glfwGetWin32Window(window);
    if (!OpenClipboard(hwnd))
    {
        GlobalFree(memory);
        return false;
    }

    EmptyClipboard();
    const HANDLE clipboardResult = SetClipboardData(CF_DIB, memory);
    CloseClipboard();

    if (!clipboardResult)
    {
        GlobalFree(memory);
        return false;
    }

    return true;
}

} // namespace heritage::platform::windows

#else

namespace heritage::platform::windows {

bool copyBackbufferToClipboard(GLFWwindow*, int, int)
{
    return false;
}

} // namespace heritage::platform::windows

#endif
