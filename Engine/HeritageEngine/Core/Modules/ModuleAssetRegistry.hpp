#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace heritage::modules {

struct ModuleAssetRecord
{
    std::string relativePath;
    std::string extension;
    std::uintmax_t sizeBytes = 0;
    std::filesystem::file_time_type writeTime{};
};

// Lightweight development-time asset discovery index for one active module.
//
// This does NOT automatically instantiate arbitrary files into a scene. It
// notices files added/removed/renamed/changed under the module Assets tree and
// exposes a stable, module-relative catalog to higher-level systems. Runtime
// systems decide which discovered assets are meaningful (vehicle, scene,
// texture, etc.) and whether they should be loaded.
//
// AS01A deliberately keeps filesystem discovery failure-isolated from module
// startup. A bad/inaccessible path must never be able to terminate the engine.
class ModuleAssetRegistry
{
public:
    void reset(const std::filesystem::path& assetRoot = {});
    void update(float deltaTime);
    bool forceRefresh();

    std::uint64_t revision() const { return m_revision; }
    const std::filesystem::path& assetRoot() const { return m_assetRoot; }
    const std::string& lastError() const { return m_lastError; }

    std::size_t count(
        const std::string& extension = {},
        const std::string& directoryPrefix = {},
        const std::string& fileNamePrefix = {}) const;

    std::optional<ModuleAssetRecord> recordAt(
        std::size_t oneBasedIndex,
        const std::string& extension = {},
        const std::string& directoryPrefix = {},
        const std::string& fileNamePrefix = {}) const;

    std::optional<ModuleAssetRecord> latest(
        const std::string& extension = {},
        const std::string& directoryPrefix = {},
        const std::string& fileNamePrefix = {}) const;

private:
    static std::string lowercase(std::string text);
    static bool matches(
        const ModuleAssetRecord& record,
        const std::string& normalizedExtension,
        const std::string& normalizedDirectoryPrefix,
        const std::string& normalizedFileNamePrefix);

    bool scan();

    std::filesystem::path m_assetRoot;
    std::vector<ModuleAssetRecord> m_records;
    std::uint64_t m_revision = 0;
    float m_refreshTimer = 0.0f;
    bool m_initialScanComplete = false;
    std::string m_lastError;
};

} // namespace heritage::modules
