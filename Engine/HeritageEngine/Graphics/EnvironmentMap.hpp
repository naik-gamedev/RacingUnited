#pragma once

#include <glad/glad.h>

#include <chrono>
#include <cstdint>
#include <string>

#include "EnvironmentSystem.hpp"

namespace heritage::graphics {

// Renderer-owned cubemap used by materials for diffuse environment lighting
// and roughness-aware specular reflections.
//
// PERF09 keeps two small procedural cubemaps. The active one is sampled by the
// renderer while the staging one is refreshed one face per frame, then swapped
// atomically after all six faces and mip levels are complete. This avoids a
// single ~6-8 ms CPU/driver pulse when the day/night IBL updates.
class EnvironmentMap
{
public:
    bool initializeProcedural(const EnvironmentLighting& lighting);
    bool updateProcedural(const EnvironmentLighting& lighting, bool force = false);
    void shutdown();

    GLuint textureId() const { return m_textureId; }
    float maximumLod() const { return m_maximumLod; }
    bool valid() const { return m_textureId != 0; }
    std::uint64_t generationSerial() const { return m_generationSerial; }
    bool refreshInProgress() const { return m_refreshInProgress; }
    int refreshFaceIndex() const { return m_refreshInProgress ? m_nextRefreshFace : -1; }
    const std::string& lastError() const { return m_lastError; }

private:
    bool allocateCube(GLuint& textureId);
    bool uploadProcedural(const EnvironmentLighting& lighting);
    bool uploadProceduralFace(
        GLuint textureId,
        int face,
        const EnvironmentLighting& lighting);
    bool finishStagedRefresh();

    GLuint m_textureId = 0;
    GLuint m_stagingTextureId = 0;
    float m_maximumLod = 0.0f;
    float m_lastGeneratedTimeHours = -1000.0f;
    std::chrono::steady_clock::time_point m_lastUploadWallTime{};
    std::chrono::steady_clock::time_point m_lastFaceStepWallTime{};
    bool m_hasUploadWallTime = false;
    bool m_hasFaceStepWallTime = false;
    bool m_refreshInProgress = false;
    int m_nextRefreshFace = 0;
    EnvironmentLighting m_pendingLighting{};
    std::uint64_t m_generationSerial = 0;
    std::string m_lastError;
};

} // namespace heritage::graphics
