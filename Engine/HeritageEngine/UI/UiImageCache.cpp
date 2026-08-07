#include "UiImageCache.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <glad/glad.h>

#include <algorithm>
#include <cwctype>
#include <limits>
#include <sstream>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace heritage::ui {
namespace {

using Microsoft::WRL::ComPtr;

class ComApartment
{
public:
    ComApartment()
        : m_result(CoInitializeEx(nullptr, COINIT_MULTITHREADED)),
          m_mustUninitialize(m_result == S_OK)
    {
    }

    ~ComApartment()
    {
        if (m_mustUninitialize)
            CoUninitialize();
    }

    bool available() const
    {
        return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE;
    }

    HRESULT result() const { return m_result; }

private:
    HRESULT m_result = E_FAIL;
    bool m_mustUninitialize = false;
};

std::string hresultText(HRESULT value)
{
    std::ostringstream output;
    output << "HRESULT 0x" << std::hex << std::uppercase
        << static_cast<unsigned long>(value);
    return output.str();
}

} // namespace

const UiImage* UiImageCache::load(
    const std::filesystem::path& absolutePath,
    std::string& errorMessage)
{
    errorMessage.clear();

    if (absolutePath.empty())
    {
        errorMessage = "UI image path is empty or unsafe.";
        return nullptr;
    }

    std::error_code fileError;
    if (!std::filesystem::is_regular_file(absolutePath, fileError))
    {
        errorMessage = "UI image was not found: " + absolutePath.string();
        return nullptr;
    }

    const std::wstring key = cacheKey(absolutePath);
    const auto found = m_images.find(key);
    if (found != m_images.end())
        return &found->second;

    UiImage image;
    if (!decodeAndUpload(absolutePath, image, errorMessage))
        return nullptr;

    auto [iterator, inserted] = m_images.emplace(key, image);
    if (!inserted)
    {
        if (image.textureId != 0)
        {
            const GLuint texture = static_cast<GLuint>(image.textureId);
            glDeleteTextures(1, &texture);
        }
        return &iterator->second;
    }

    return &iterator->second;
}

bool UiImageCache::unload(const std::filesystem::path& absolutePath)
{
    if (absolutePath.empty())
        return false;

    const std::wstring key = cacheKey(absolutePath);
    const auto found = m_images.find(key);
    if (found == m_images.end())
        return false;

    if (found->second.textureId != 0)
    {
        const GLuint texture = static_cast<GLuint>(found->second.textureId);
        glDeleteTextures(1, &texture);
    }

    m_images.erase(found);
    return true;
}

void UiImageCache::clear()
{
    for (const auto& [key, image] : m_images)
    {
        (void)key;
        if (image.textureId == 0)
            continue;

        const GLuint texture = static_cast<GLuint>(image.textureId);
        glDeleteTextures(1, &texture);
    }
    m_images.clear();
}

std::wstring UiImageCache::cacheKey(const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(path, error);
    const std::filesystem::path normalized = error
        ? path.lexically_normal()
        : canonical.lexically_normal();

    std::wstring key = normalized.native();
    std::transform(
        key.begin(),
        key.end(),
        key.begin(),
        [](wchar_t value) {
            return static_cast<wchar_t>(std::towlower(value));
        });
    return key;
}

bool UiImageCache::decodeAndUpload(
    const std::filesystem::path& path,
    UiImage& image,
    std::string& errorMessage)
{
    image = {};
    errorMessage.clear();

    ComApartment apartment;
    if (!apartment.available())
    {
        errorMessage = "Could not initialize COM for UI image loading ("
            + hresultText(apartment.result()) + ").";
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(result))
    {
        errorMessage = "Could not create the Windows image decoder ("
            + hresultText(result) + ").";
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    result = factory->CreateDecoderFromFilename(
        path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        decoder.GetAddressOf());
    if (FAILED(result))
    {
        errorMessage = "Could not decode UI image '" + path.string()
            + "' (" + hresultText(result) + ").";
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(result))
    {
        errorMessage = "Could not read the first image frame from '"
            + path.string() + "'.";
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    result = frame->GetSize(&width, &height);
    if (FAILED(result) || width == 0 || height == 0)
    {
        errorMessage = "UI image has invalid dimensions: " + path.string();
        return false;
    }

    if (width > static_cast<UINT>((std::numeric_limits<int>::max)())
        || height > static_cast<UINT>((std::numeric_limits<int>::max)()))
    {
        errorMessage = "UI image is too large to load: " + path.string();
        return false;
    }

    ComPtr<IWICFormatConverter> converter;
    result = factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(result))
    {
        errorMessage = "Could not create an RGBA converter for '"
            + path.string() + "'.";
        return false;
    }

    result = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(result))
    {
        errorMessage = "Could not convert UI image to RGBA: "
            + path.string();
        return false;
    }

    const std::uint64_t stride64 = static_cast<std::uint64_t>(width) * 4ull;
    const std::uint64_t byteCount64 = stride64 * static_cast<std::uint64_t>(height);
    if (stride64 > (std::numeric_limits<UINT>::max)()
        || byteCount64 > (std::numeric_limits<UINT>::max)())
    {
        errorMessage = "UI image pixel buffer is too large: " + path.string();
        return false;
    }

    const UINT stride = static_cast<UINT>(stride64);
    const UINT byteCount = static_cast<UINT>(byteCount64);
    std::vector<std::uint8_t> pixels(byteCount);
    result = converter->CopyPixels(
        nullptr,
        stride,
        byteCount,
        pixels.data());

    if (FAILED(result))
    {
        errorMessage = "Could not copy decoded pixels for '"
            + path.string() + "'.";
        return false;
    }

    // Windows Imaging Component returns rows from top to bottom, while the
    // OpenGL texture coordinates used by ImGui treat v = 0 as the bottom row.
    // Flip the decoded image once during upload so every module UI image is
    // displayed upright without requiring scripts to reverse their UVs.
    std::vector<std::uint8_t> flippedPixels(byteCount);
    for (UINT y = 0; y < height; ++y)
    {
        const std::size_t sourceOffset =
            static_cast<std::size_t>(height - 1u - y) * stride;
        const std::size_t destinationOffset =
            static_cast<std::size_t>(y) * stride;

        std::copy_n(
            pixels.data() + sourceOffset,
            static_cast<std::size_t>(stride),
            flippedPixels.data() + destinationOffset);
    }
    pixels.swap(flippedPixels);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0)
    {
        errorMessage = "OpenGL could not allocate a texture for '"
            + path.string() + "'.";
        return false;
    }

    GLint previousTexture = 0;
    GLint previousUnpackAlignment = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);

    while (glGetError() != GL_NO_ERROR)
    {
        // Clear unrelated previous OpenGL errors so upload validation below
        // reports only this texture operation.
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data());

    const GLenum uploadError = glGetError();
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));

    if (uploadError != GL_NO_ERROR)
    {
        glDeleteTextures(1, &texture);
        errorMessage = "OpenGL rejected UI image '" + path.string()
            + "' (error " + std::to_string(uploadError) + ").";
        return false;
    }

    image.textureId = static_cast<std::uint32_t>(texture);
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    return true;
}

} // namespace heritage::ui
