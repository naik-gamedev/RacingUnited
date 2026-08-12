#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace heritage::paths {

// Module-facing paths are UTF-8. On Windows std::filesystem::path's narrow
// constructor follows the active code page, so it must not be used for names
// such as Ivarčko, Škofja, Brno, etc.
inline std::filesystem::path fromUtf8(std::string_view text)
{
    const std::u8string utf8(
        reinterpret_cast<const char8_t*>(text.data()),
        text.size());
    return std::filesystem::path(utf8);
}

inline std::string toUtf8(const std::filesystem::path& path)
{
    const std::u8string text = path.generic_u8string();
    return std::string(
        reinterpret_cast<const char*>(text.data()),
        text.size());
}

} // namespace heritage::paths
