#include "Texture2D.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

#ifndef GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT 0x8E8F
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

namespace heritage::graphics {
namespace {

using Microsoft::WRL::ComPtr;

class ComApartment
{
public:
    ComApartment()
        : m_result(CoInitializeEx(nullptr, COINIT_MULTITHREADED)),
          m_mustUninitialize(SUCCEEDED(m_result))
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

bool supportsAnisotropy()
{
    static int cached = -1;
    if (cached >= 0)
        return cached != 0;

    cached = 0;
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint index = 0; index < count; ++index)
    {
        const char* extension = reinterpret_cast<const char*>(
            glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(index)));
        if (extension
            && std::string(extension) == "GL_EXT_texture_filter_anisotropic")
        {
            cached = 1;
            break;
        }
    }
    return cached != 0;
}

float requestedAnisotropy(int textureFilterIndex)
{
    switch (textureFilterIndex)
    {
    case 3: return 2.0f;
    case 4: return 4.0f;
    case 5: return 8.0f;
    case 6: return 16.0f;
    default: return 1.0f;
    }
}

std::string lowercasePath(const std::filesystem::path& path)
{
    // EntityMeshRenderer already canonicalizes and validates external material
    // paths at the module boundary. Re-running weakly_canonical here turned a
    // cache-key lookup into another filesystem operation for every material.
    std::string key = path.lexically_normal().generic_string();
    std::transform(
        key.begin(), key.end(), key.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return key;
}


std::size_t hashBytes(const std::vector<std::uint8_t>& bytes)
{
    std::size_t hash = 1469598103934665603ull;
    for (std::uint8_t value : bytes)
    {
        hash ^= static_cast<std::size_t>(value);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::uint32_t readU32Le(const std::uint8_t* data)
{
    return static_cast<std::uint32_t>(data[0])
        | (static_cast<std::uint32_t>(data[1]) << 8u)
        | (static_cast<std::uint32_t>(data[2]) << 16u)
        | (static_cast<std::uint32_t>(data[3]) << 24u);
}

std::uint64_t readU64Le(const std::uint8_t* data)
{
    return static_cast<std::uint64_t>(readU32Le(data))
        | (static_cast<std::uint64_t>(readU32Le(data + 4)) << 32u);
}

bool uploadKtx2Bc6h(
    const std::filesystem::path& path,
    Texture2D& texture,
    std::string& errorMessage)
{
    constexpr std::uint8_t kIdentifier[12] = {
        0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32,
        0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
    };
    constexpr std::uint32_t kVkFormatBc6hUfloatBlock = 143u;
    constexpr std::size_t kHeaderBytes = 80u;
    constexpr std::size_t kLevelIndexBytes = 24u;

    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
    {
        errorMessage = "Could not open KTX2 texture '" + path.string() + "'.";
        return false;
    }
    const std::streamoff length = input.tellg();
    if (length < static_cast<std::streamoff>(kHeaderBytes + kLevelIndexBytes))
    {
        errorMessage = "KTX2 texture is truncated: " + path.string();
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!input)
    {
        errorMessage = "Could not read KTX2 texture '" + path.string() + "'.";
        return false;
    }
    if (std::memcmp(bytes.data(), kIdentifier, sizeof(kIdentifier)) != 0)
    {
        errorMessage = "KTX2 identifier is invalid: " + path.string();
        return false;
    }

    const std::uint32_t vkFormat = readU32Le(bytes.data() + 12u);
    const std::uint32_t width = readU32Le(bytes.data() + 20u);
    const std::uint32_t height = readU32Le(bytes.data() + 24u);
    const std::uint32_t depth = readU32Le(bytes.data() + 28u);
    const std::uint32_t layerCount = readU32Le(bytes.data() + 32u);
    const std::uint32_t faceCount = readU32Le(bytes.data() + 36u);
    const std::uint32_t levelCount = readU32Le(bytes.data() + 40u);
    const std::uint32_t supercompression = readU32Le(bytes.data() + 44u);

    if (vkFormat != kVkFormatBc6hUfloatBlock)
    {
        errorMessage = "Heritage KTX2 loader currently expects BC6H_UFLOAT_BLOCK (VK format 143): "
            + path.string();
        return false;
    }
    if (width == 0u || height == 0u || depth != 0u
        || layerCount > 1u || faceCount != 1u || levelCount != 1u
        || supercompression != 0u)
    {
        errorMessage = "Heritage KTX2 sky path expects one uncompressed-container 2D BC6H level with no array/cubemap/mips: "
            + path.string();
        return false;
    }

    const std::uint64_t levelOffset = readU64Le(bytes.data() + kHeaderBytes);
    const std::uint64_t levelLength = readU64Le(bytes.data() + kHeaderBytes + 8u);
    const std::uint64_t expectedBlocksX = (static_cast<std::uint64_t>(width) + 3ull) / 4ull;
    const std::uint64_t expectedBlocksY = (static_cast<std::uint64_t>(height) + 3ull) / 4ull;
    const std::uint64_t expectedLength = expectedBlocksX * expectedBlocksY * 16ull;
    if (levelLength != expectedLength
        || levelOffset > bytes.size()
        || levelLength > static_cast<std::uint64_t>(bytes.size()) - levelOffset)
    {
        errorMessage = "KTX2 BC6H level layout is invalid: " + path.string();
        return false;
    }
    if (width > static_cast<std::uint32_t>((std::numeric_limits<GLsizei>::max)())
        || height > static_cast<std::uint32_t>((std::numeric_limits<GLsizei>::max)())
        || levelLength > static_cast<std::uint64_t>((std::numeric_limits<GLsizei>::max)()))
    {
        errorMessage = "KTX2 texture is too large for OpenGL upload: " + path.string();
        return false;
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    if (id == 0)
    {
        errorMessage = "OpenGL could not allocate KTX2 texture '" + path.string() + "'.";
        return false;
    }

    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    while (glGetError() != GL_NO_ERROR) {}
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glCompressedTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height),
        0,
        static_cast<GLsizei>(levelLength),
        bytes.data() + static_cast<std::size_t>(levelOffset));
    const GLenum uploadError = glGetError();
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    if (uploadError != GL_NO_ERROR)
    {
        glDeleteTextures(1, &id);
        errorMessage = "OpenGL rejected BC6H KTX2 texture '" + path.string()
            + "' (error " + std::to_string(uploadError) + ").";
        return false;
    }

    texture.id = id;
    texture.width = static_cast<int>(width);
    texture.height = static_cast<int>(height);
    texture.colorSpace = TextureColorSpace::Linear;
    texture.hasMipmaps = false;
    return true;
}

bool decodeFrameAndUpload(
    IWICBitmapFrameDecode* frame,
    const std::string& diagnosticName,
    TextureColorSpace colorSpace,
    bool flipVerticalOnDecode,
    Texture2D& texture,
    std::string& errorMessage)
{
    UINT width = 0;
    UINT height = 0;
    HRESULT result = frame->GetSize(&width, &height);
    if (FAILED(result) || width == 0 || height == 0)
    {
        errorMessage = "Material texture has invalid dimensions: " + diagnosticName;
        return false;
    }

    if (width > static_cast<UINT>((std::numeric_limits<int>::max)())
        || height > static_cast<UINT>((std::numeric_limits<int>::max)()))
    {
        errorMessage = "Material texture is too large to load: " + diagnosticName;
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    result = CoCreateInstance(
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

    ComPtr<IWICFormatConverter> converter;
    result = factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(result))
    {
        errorMessage = "Could not create an RGBA converter for '"
            + diagnosticName + "'.";
        return false;
    }

    result = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(result))
    {
        errorMessage = "Could not convert material texture to RGBA: "
            + diagnosticName;
        return false;
    }

    const std::uint64_t stride64 = static_cast<std::uint64_t>(width) * 4ull;
    const std::uint64_t byteCount64 =
        stride64 * static_cast<std::uint64_t>(height);
    if (stride64 > (std::numeric_limits<UINT>::max)()
        || byteCount64 > (std::numeric_limits<UINT>::max)())
    {
        errorMessage = "Material texture pixel buffer is too large: "
            + diagnosticName;
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
        errorMessage = "Could not copy decoded material pixels for '"
            + diagnosticName + "'.";
        return false;
    }

    if (flipVerticalOnDecode)
    {
        std::vector<std::uint8_t> flipped(byteCount);
        for (UINT y = 0; y < height; ++y)
        {
            const std::size_t source =
                static_cast<std::size_t>(height - 1u - y) * stride;
            const std::size_t destination =
                static_cast<std::size_t>(y) * stride;
            std::copy_n(
                pixels.data() + source,
                static_cast<std::size_t>(stride),
                flipped.data() + destination);
        }
        pixels.swap(flipped);
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    if (id == 0)
    {
        errorMessage = "OpenGL could not allocate a material texture for '"
            + diagnosticName + "'.";
        return false;
    }

    GLint previousTexture = 0;
    GLint previousUnpackAlignment = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);

    while (glGetError() != GL_NO_ERROR)
    {
        // Discard unrelated errors so upload validation is local.
    }

    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    const GLint internalFormat =
        colorSpace == TextureColorSpace::SRgb
        ? GL_SRGB8_ALPHA8
        : GL_RGBA8;

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    const GLenum uploadError = glGetError();
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));

    if (uploadError != GL_NO_ERROR)
    {
        glDeleteTextures(1, &id);
        errorMessage = "OpenGL rejected material texture '"
            + diagnosticName + "' (error "
            + std::to_string(uploadError) + ").";
        return false;
    }

    texture.id = id;
    texture.width = static_cast<int>(width);
    texture.height = static_cast<int>(height);
    texture.colorSpace = colorSpace;
    texture.hasMipmaps = true;
    return true;
}

bool decodeDecoderAndUpload(
    IWICBitmapDecoder* decoder,
    const std::string& diagnosticName,
    TextureColorSpace colorSpace,
    bool flipVerticalOnDecode,
    Texture2D& texture,
    std::string& errorMessage)
{
    ComPtr<IWICBitmapFrameDecode> frame;
    HRESULT result = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(result))
    {
        errorMessage = "Could not read the first image frame from '"
            + diagnosticName + "'.";
        return false;
    }

    return decodeFrameAndUpload(
        frame.Get(),
        diagnosticName,
        colorSpace,
        flipVerticalOnDecode,
        texture,
        errorMessage);
}

} // namespace

const Texture2D* Texture2DCache::acquire(
    const std::filesystem::path& absolutePath,
    TextureColorSpace colorSpace,
    int textureFilterIndex,
    bool flipVerticalOnDecode,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (absolutePath.empty())
        return nullptr;

    const std::string key = cacheKey(absolutePath, colorSpace, flipVerticalOnDecode);
    CachedTexture& cached = m_textures[key];

    // File timestamps are deliberately sampled once per renderer hot-reload
    // epoch, not once per material draw. The old path performed multiple
    // filesystem calls for every texture reference in every GLB primitive and
    // could spend tens of milliseconds per frame in render submission alone.
    bool changed = !cached.attempted || cached.isEmbedded;
    bool exists = cached.sourceExists;
    std::filesystem::file_time_type writeTime = cached.lastWriteTime;
    const bool pollSource = !cached.attempted
        || cached.lastHotReloadEpoch != m_hotReloadEpoch;
    if (pollSource)
    {
        cached.lastHotReloadEpoch = m_hotReloadEpoch;
        std::error_code fileError;
        exists = std::filesystem::is_regular_file(absolutePath, fileError);
        writeTime = {};
        if (exists)
        {
            std::error_code timeError;
            writeTime = std::filesystem::last_write_time(absolutePath, timeError);
            if (timeError)
                writeTime = {};
        }
        changed = changed
            || cached.sourceExists != exists
            || (exists && cached.lastWriteTime != writeTime);
    }

    if (changed)
    {
        if (cached.texture.id != 0)
            glDeleteTextures(1, &cached.texture.id);

        cached.texture = {};
        cached.attempted = true;
        cached.sourceExists = exists;
        cached.lastWriteTime = writeTime;
        cached.appliedFilterIndex = -1;
        cached.isEmbedded = false;
        cached.lastHotReloadEpoch = m_hotReloadEpoch;
        cached.embeddedByteCount = 0;
        cached.embeddedFingerprint = 0;
        cached.error.clear();

        if (!exists)
        {
            cached.error = "Material texture was not found: "
                + absolutePath.string();
        }
        else if (!decodeAndUpload(
                     absolutePath,
                     colorSpace,
                     flipVerticalOnDecode,
                     cached.texture,
                     cached.error))
        {
            // decodeAndUpload supplies the diagnostic.
        }

        if (!cached.error.empty())
            errorMessage = cached.error;
    }

    if (cached.texture.id == 0)
        return nullptr;

    if (cached.appliedFilterIndex != textureFilterIndex)
    {
        applySampling(cached.texture, textureFilterIndex);
        cached.appliedFilterIndex = textureFilterIndex;
    }

    return &cached.texture;
}

const Texture2D* Texture2DCache::acquireEmbedded(
    const std::string& key,
    const std::vector<std::uint8_t>& encodedBytes,
    TextureColorSpace colorSpace,
    int textureFilterIndex,
    bool flipVerticalOnDecode,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (key.empty() || encodedBytes.empty())
        return nullptr;

    CachedTexture& cached = m_textures[embeddedCacheKey(key, colorSpace, flipVerticalOnDecode)];

    // Embedded GLB images are immutable for the lifetime of a loaded Mesh.
    // Re-hashing the complete PNG/JPEG byte vector for every material draw was
    // catastrophically expensive when one texture was shared by many
    // primitives. EntityMeshRenderer clears this cache whenever a source mesh
    // hot-reloads, so a fingerprint is required only for the first upload.
    const bool changed = !cached.attempted || !cached.isEmbedded;
    std::size_t fingerprint = cached.embeddedFingerprint;
    if (changed)
        fingerprint = hashBytes(encodedBytes);

    if (changed)
    {
        if (cached.texture.id != 0)
            glDeleteTextures(1, &cached.texture.id);

        cached.texture = {};
        cached.attempted = true;
        cached.sourceExists = true;
        cached.lastWriteTime = {};
        cached.appliedFilterIndex = -1;
        cached.isEmbedded = true;
        cached.lastHotReloadEpoch = m_hotReloadEpoch;
        cached.embeddedByteCount = encodedBytes.size();
        cached.embeddedFingerprint = fingerprint;
        cached.error.clear();

        if (!decodeAndUploadEmbedded(
                key,
                encodedBytes,
                colorSpace,
                flipVerticalOnDecode,
                cached.texture,
                cached.error))
        {
            // decodeAndUploadEmbedded supplies the diagnostic.
        }

        if (!cached.error.empty())
            errorMessage = cached.error;
    }

    if (cached.texture.id == 0)
        return nullptr;

    if (cached.appliedFilterIndex != textureFilterIndex)
    {
        applySampling(cached.texture, textureFilterIndex);
        cached.appliedFilterIndex = textureFilterIndex;
    }

    return &cached.texture;
}

void Texture2DCache::clear()
{
    for (auto& [key, cached] : m_textures)
    {
        (void)key;
        if (cached.texture.id != 0)
            glDeleteTextures(1, &cached.texture.id);
    }
    m_textures.clear();
}

std::string Texture2DCache::cacheKey(
    const std::filesystem::path& path,
    TextureColorSpace colorSpace,
    bool flipVerticalOnDecode)
{
    return lowercasePath(path)
        + (colorSpace == TextureColorSpace::SRgb ? "|srgb" : "|linear")
        + (flipVerticalOnDecode ? "|flipv" : "|rawrows");
}

std::string Texture2DCache::embeddedCacheKey(
    const std::string& key,
    TextureColorSpace colorSpace,
    bool flipVerticalOnDecode)
{
    std::string result = key;
    std::transform(
        result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    result += (colorSpace == TextureColorSpace::SRgb ? "|srgb" : "|linear");
    result += (flipVerticalOnDecode ? "|flipv" : "|rawrows");
    return result;
}

bool Texture2DCache::decodeAndUpload(
    const std::filesystem::path& path,
    TextureColorSpace colorSpace,
    bool flipVerticalOnDecode,
    Texture2D& texture,
    std::string& errorMessage)
{
    texture = {};
    errorMessage.clear();

    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension == ".ktx2")
    {
        if (flipVerticalOnDecode)
        {
            errorMessage = "KTX2 runtime textures cannot be row-flipped during GPU upload: " + path.string();
            return false;
        }
        return uploadKtx2Bc6h(path, texture, errorMessage);
    }

    ComApartment apartment;
    if (!apartment.available())
    {
        errorMessage = "Could not initialize COM for material texture loading ("
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
        errorMessage = "Could not decode material texture '"
            + path.string() + "' (" + hresultText(result) + ").";
        return false;
    }

    return decodeDecoderAndUpload(
        decoder.Get(),
        path.string(),
        colorSpace,
        flipVerticalOnDecode,
        texture,
        errorMessage);
}

bool Texture2DCache::decodeAndUploadEmbedded(
    const std::string& diagnosticName,
    const std::vector<std::uint8_t>& encodedBytes,
    TextureColorSpace colorSpace,
    bool flipVerticalOnDecode,
    Texture2D& texture,
    std::string& errorMessage)
{
    texture = {};
    errorMessage.clear();

    ComApartment apartment;
    if (!apartment.available())
    {
        errorMessage = "Could not initialize COM for embedded texture loading ("
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

    HGLOBAL memoryHandle = GlobalAlloc(GMEM_MOVEABLE, encodedBytes.size());
    if (!memoryHandle)
    {
        errorMessage = "Could not allocate memory for embedded texture '"
            + diagnosticName + "'.";
        return false;
    }

    void* memory = GlobalLock(memoryHandle);
    if (!memory)
    {
        GlobalFree(memoryHandle);
        errorMessage = "Could not lock memory for embedded texture '"
            + diagnosticName + "'.";
        return false;
    }
    std::memcpy(memory, encodedBytes.data(), encodedBytes.size());
    GlobalUnlock(memoryHandle);

    ComPtr<IStream> stream;
    result = CreateStreamOnHGlobal(memoryHandle, TRUE, stream.GetAddressOf());
    if (FAILED(result))
    {
        GlobalFree(memoryHandle);
        errorMessage = "Could not create a stream for embedded texture '"
            + diagnosticName + "' (" + hresultText(result) + ").";
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    result = factory->CreateDecoderFromStream(
        stream.Get(),
        nullptr,
        WICDecodeMetadataCacheOnLoad,
        decoder.GetAddressOf());
    if (FAILED(result))
    {
        errorMessage = "Could not decode embedded texture '"
            + diagnosticName + "' (" + hresultText(result) + ").";
        return false;
    }

    return decodeDecoderAndUpload(
        decoder.Get(),
        diagnosticName,
        colorSpace,
        flipVerticalOnDecode,
        texture,
        errorMessage);
}

void Texture2DCache::applySampling(
    Texture2D& texture,
    int textureFilterIndex)
{
    if (texture.id == 0)
        return;

    const int filter = std::clamp(textureFilterIndex, 0, 6);
    GLint minFilter = texture.hasMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    GLint magFilter = GL_LINEAR;

    if (filter == 0)
    {
        minFilter = GL_NEAREST;
        magFilter = GL_NEAREST;
    }
    else if (filter == 1 && texture.hasMipmaps)
    {
        minFilter = GL_LINEAR_MIPMAP_NEAREST;
        magFilter = GL_LINEAR;
    }

    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

    if (supportsAnisotropy())
    {
        GLfloat maximum = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximum);
        const float requested = requestedAnisotropy(filter);
        glTexParameterf(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAX_ANISOTROPY_EXT,
            std::clamp(requested, 1.0f, maximum));
    }

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
}

} // namespace heritage::graphics
