#include "DynamicSurfaceGpuShaders.hpp"
#include "DynamicSurfaceGpuRuntime.hpp"

#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace heritage::graphics::dynamicsurface {

bool DynamicSurfaceGpuRuntime::allocateState(
    StateRuntime& state,
    GLenum internalFormat,
    GLenum clearFormat,
    GLenum clearType,
    std::size_t bytesPerTexel,
    bool allocateScratch,
    std::string& errorMessage)
{
    destroyState(state);
    state.internalFormat = internalFormat;
    state.clearFormat = clearFormat;
    state.clearType = clearType;
    state.bytesPerTexel = bytesPerTexel;

    glGenTextures(1, &state.atlas);
    glBindTexture(GL_TEXTURE_2D, state.atlas);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat,
        static_cast<GLsizei>(kAtlasWidth), static_cast<GLsizei>(kAtlasHeight));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (internalFormat == GL_RGBA8)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if (allocateScratch)
    {
        glGenTextures(1, &state.scratch);
        glBindTexture(GL_TEXTURE_2D, state.scratch);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat,
            static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    if (!state.atlas || (allocateScratch && !state.scratch)
        || !detail::checkNoGlError(errorMessage, "state atlas allocation"))
    {
        destroyState(state);
        return false;
    }
    state.allocated = true;
    double bytes = static_cast<double>(kAtlasWidth)
        * static_cast<double>(kAtlasHeight) * static_cast<double>(bytesPerTexel);
    if (allocateScratch)
        bytes += static_cast<double>(kTileResolution) * static_cast<double>(kTileResolution)
            * static_cast<double>(bytesPerTexel);
    m_stats.committedMiB += bytes / (1024.0 * 1024.0);
    return true;
}
bool DynamicSurfaceGpuRuntime::ensureSnowState(std::string& errorMessage)
{
    if (m_snow.allocated)
        return true;
    if (!allocateState(m_snow, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT,
            sizeof(std::uint16_t), true, errorMessage))
        return false;
    for (const auto& [key, tile] : m_tiles)
    {
        (void)key;
        clearStateSlot(m_snow, tile.slot, 0u);
    }
    m_stats.snowReady = true;
    return true;
}
bool DynamicSurfaceGpuRuntime::ensureMudState(std::string& errorMessage)
{
    if (m_mud.allocated)
        return true;
    if (!allocateState(m_mud, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
            sizeof(std::uint8_t), true, errorMessage))
        return false;
    for (const auto& [key, tile] : m_tiles)
    {
        (void)key;
        clearStateSlot(m_mud, tile.slot, 0u);
    }
    m_stats.mudReady = true;
    return true;
}
void DynamicSurfaceGpuRuntime::destroyState(StateRuntime& state)
{
    if (state.scratch)
        glDeleteTextures(1, &state.scratch);
    if (state.atlas)
        glDeleteTextures(1, &state.atlas);
    state = {};
}
void DynamicSurfaceGpuRuntime::clearStateSlot(
    StateRuntime& state,
    std::uint16_t slot,
    std::uint32_t clearValue)
{
    if (!state.allocated)
        return;
    const auto origin = atlasSlotOrigin(slot);
    if (state.internalFormat == GL_RGBA8)
    {
        const std::array<std::uint8_t, 4> rgba{{
            static_cast<std::uint8_t>(clearValue & 0xffu),
            static_cast<std::uint8_t>((clearValue >> 8u) & 0xffu),
            static_cast<std::uint8_t>((clearValue >> 16u) & 0xffu),
            static_cast<std::uint8_t>((clearValue >> 24u) & 0xffu) }};
        glClearTexSubImage(state.atlas, 0,
            static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]), 0,
            static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution), 1,
            GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    }
    else
    {
        glClearTexSubImage(state.atlas, 0,
            static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]), 0,
            static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution), 1,
            state.clearFormat, state.clearType, &clearValue);
    }
}

} // namespace heritage::graphics::dynamicsurface
