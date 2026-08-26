#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>

namespace heritage::graphics {

enum class TextureColorSpace
{
    Linear,
    SRgb
};

struct Texture2D
{
    GLuint id = 0;
    int width = 0;
    int height = 0;
    TextureColorSpace colorSpace = TextureColorSpace::Linear;
    bool hasMipmaps = true;
};

class Texture2DCache
{
public:
    Texture2DCache() = default;
    Texture2DCache(const Texture2DCache&) = delete;
    Texture2DCache& operator=(const Texture2DCache&) = delete;

    const Texture2D* acquire(
        const std::filesystem::path& absolutePath,
        TextureColorSpace colorSpace,
        int textureFilterIndex,
        bool flipVerticalOnDecode,
        std::string& errorMessage);

    const Texture2D* acquireEmbedded(
        const std::string& key,
        const std::vector<std::uint8_t>& encodedBytes,
        TextureColorSpace colorSpace,
        int textureFilterIndex,
        bool flipVerticalOnDecode,
        std::string& errorMessage);

    // Renderer hot-reload checks are intentionally amortized. A texture that is
    // referenced by dozens of GLB primitives must not hit the filesystem (or
    // re-hash an embedded image) dozens of times every frame.
    void setHotReloadEpoch(std::uint64_t epoch) { m_hotReloadEpoch = epoch; }

    void clear();

private:
    struct CachedTexture
    {
        Texture2D texture;
        std::filesystem::file_time_type lastWriteTime{};
        bool sourceExists = false;
        bool attempted = false;
        bool isEmbedded = false;
        std::size_t embeddedByteCount = 0;
        std::size_t embeddedFingerprint = 0;
        std::uint64_t lastHotReloadEpoch = 0;
        int appliedFilterIndex = -1;
        std::string error;
    };

    static std::string cacheKey(
        const std::filesystem::path& path,
        TextureColorSpace colorSpace,
        bool flipVerticalOnDecode);
    static std::string embeddedCacheKey(
        const std::string& key,
        TextureColorSpace colorSpace,
        bool flipVerticalOnDecode);
    static bool decodeAndUpload(
        const std::filesystem::path& path,
        TextureColorSpace colorSpace,
        bool flipVerticalOnDecode,
        Texture2D& texture,
        std::string& errorMessage);
    static bool decodeAndUploadEmbedded(
        const std::string& diagnosticName,
        const std::vector<std::uint8_t>& encodedBytes,
        TextureColorSpace colorSpace,
        bool flipVerticalOnDecode,
        Texture2D& texture,
        std::string& errorMessage);
    static void applySampling(Texture2D& texture, int textureFilterIndex);

    std::unordered_map<std::string, CachedTexture> m_textures;
    std::uint64_t m_hotReloadEpoch = 1;
};

} // namespace heritage::graphics
