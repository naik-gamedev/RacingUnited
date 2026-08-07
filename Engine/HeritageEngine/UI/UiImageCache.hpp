#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace heritage::ui {

struct UiImage
{
    std::uint32_t textureId = 0;
    int width = 0;
    int height = 0;
};

// Module UI image cache.
//
// This is intentionally separate from future material textures. UI images use
// clamp-to-edge sampling, no mipmaps and ordinary RGBA colour data. Terrain,
// vehicle and PBR textures will need different wrapping, colour-space and
// filtering rules later.
class UiImageCache
{
public:
    UiImageCache() = default;
    UiImageCache(const UiImageCache&) = delete;
    UiImageCache& operator=(const UiImageCache&) = delete;

    const UiImage* load(
        const std::filesystem::path& absolutePath,
        std::string& errorMessage);

    bool unload(const std::filesystem::path& absolutePath);
    void clear();

private:
    static std::wstring cacheKey(const std::filesystem::path& path);
    static bool decodeAndUpload(
        const std::filesystem::path& path,
        UiImage& image,
        std::string& errorMessage);

    std::unordered_map<std::wstring, UiImage> m_images;
};

} // namespace heritage::ui
