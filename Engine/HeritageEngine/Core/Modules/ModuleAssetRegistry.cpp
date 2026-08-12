#include "ModuleAssetRegistry.hpp"
#include "../Paths/Utf8Path.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <system_error>

namespace heritage::modules {
namespace {

constexpr float kInitialScanDelaySeconds = 0.10f;
constexpr const char* kAssetRegistryMarker = "HERITAGE_AS01_MODULE_ASSET_DISCOVERY HERITAGE_AS01A_SAFE_LAZY_DISCOVERY HERITAGE_AS01B_UTF8_PATHS";


std::string filenameFromGenericPath(const std::string& path)
{
    const std::size_t separator = path.find_last_of('/');
    return separator == std::string::npos
        ? path
        : path.substr(separator + 1);
}

std::string normalizedPrefix(std::string text)
{
    std::replace(text.begin(), text.end(), '\\', '/');
    while (!text.empty() && text.front() == '/')
        text.erase(text.begin());
    while (!text.empty() && text.back() == '/')
        text.pop_back();
    std::transform(
        text.begin(), text.end(), text.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return text;
}

bool recordsEqual(
    const std::vector<ModuleAssetRecord>& left,
    const std::vector<ModuleAssetRecord>& right)
{
    if (left.size() != right.size())
        return false;

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (left[index].relativePath != right[index].relativePath
            || left[index].extension != right[index].extension
            || left[index].sizeBytes != right[index].sizeBytes
            || left[index].writeTime != right[index].writeTime)
        {
            return false;
        }
    }
    return true;
}

} // namespace

void ModuleAssetRegistry::reset(const std::filesystem::path& assetRoot)
{
    m_assetRoot = assetRoot.lexically_normal();
    m_records.clear();
    m_revision = 0;
    m_refreshTimer = 0.0f;
    m_initialScanComplete = false;
    m_lastError.clear();
    if (!m_assetRoot.empty())
    {
        std::cout << "Module asset registry: " << kAssetRegistryMarker
            << " root=" << heritage::paths::toUtf8(m_assetRoot)
            << " (lazy first scan)\n";
    }
}

void ModuleAssetRegistry::update(float deltaTime)
{
    if (m_assetRoot.empty() || m_initialScanComplete)
        return;

    // Discover the active module once after startup, then stay completely quiet
    // during gameplay. Recursive filesystem scans on the render/game thread can
    // produce rhythmic frametime spikes on Windows (NTFS/Defender/cache misses).
    // F5 and Module.RefreshAssetIndex() perform explicit refreshes when authoring.
    m_refreshTimer += (std::max)(0.0f, deltaTime);
    if (m_refreshTimer < kInitialScanDelaySeconds)
        return;

    m_refreshTimer = 0.0f;
    m_initialScanComplete = true;
    scan();
}

bool ModuleAssetRegistry::forceRefresh()
{
    m_refreshTimer = 0.0f;
    m_initialScanComplete = true;
    return scan();
}

std::size_t ModuleAssetRegistry::count(
    const std::string& extension,
    const std::string& directoryPrefix,
    const std::string& fileNamePrefix) const
{
    const std::string normalizedExtension = lowercase(extension);
    const std::string normalizedDirectory = normalizedPrefix(directoryPrefix);
    const std::string normalizedFileName = lowercase(fileNamePrefix);

    std::size_t result = 0;
    for (const ModuleAssetRecord& record : m_records)
    {
        if (matches(record, normalizedExtension, normalizedDirectory, normalizedFileName))
            ++result;
    }
    return result;
}

std::optional<ModuleAssetRecord> ModuleAssetRegistry::recordAt(
    std::size_t oneBasedIndex,
    const std::string& extension,
    const std::string& directoryPrefix,
    const std::string& fileNamePrefix) const
{
    if (oneBasedIndex == 0)
        return std::nullopt;

    const std::string normalizedExtension = lowercase(extension);
    const std::string normalizedDirectory = normalizedPrefix(directoryPrefix);
    const std::string normalizedFileName = lowercase(fileNamePrefix);

    std::size_t matchedIndex = 0;
    for (const ModuleAssetRecord& record : m_records)
    {
        if (!matches(record, normalizedExtension, normalizedDirectory, normalizedFileName))
            continue;
        ++matchedIndex;
        if (matchedIndex == oneBasedIndex)
            return record;
    }
    return std::nullopt;
}

std::optional<ModuleAssetRecord> ModuleAssetRegistry::latest(
    const std::string& extension,
    const std::string& directoryPrefix,
    const std::string& fileNamePrefix) const
{
    const std::string normalizedExtension = lowercase(extension);
    const std::string normalizedDirectory = normalizedPrefix(directoryPrefix);
    const std::string normalizedFileName = lowercase(fileNamePrefix);

    const ModuleAssetRecord* result = nullptr;
    for (const ModuleAssetRecord& record : m_records)
    {
        if (!matches(record, normalizedExtension, normalizedDirectory, normalizedFileName))
            continue;
        if (!result
            || record.writeTime > result->writeTime
            || (record.writeTime == result->writeTime
                && record.relativePath < result->relativePath))
        {
            result = &record;
        }
    }

    return result ? std::optional<ModuleAssetRecord>(*result) : std::nullopt;
}

std::string ModuleAssetRegistry::lowercase(std::string text)
{
    std::transform(
        text.begin(), text.end(), text.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return text;
}

bool ModuleAssetRegistry::matches(
    const ModuleAssetRecord& record,
    const std::string& normalizedExtension,
    const std::string& normalizedDirectoryPrefix,
    const std::string& normalizedFileNamePrefix)
{
    if (!normalizedExtension.empty())
    {
        std::string extension = normalizedExtension;
        if (extension.front() != '.')
            extension.insert(extension.begin(), '.');
        if (record.extension != extension)
            return false;
    }

    const std::string path = lowercase(record.relativePath);
    if (!normalizedDirectoryPrefix.empty())
    {
        const std::string directory = normalizedDirectoryPrefix + "/";
        if (path.rfind(directory, 0) != 0
            && path != normalizedDirectoryPrefix)
        {
            return false;
        }
    }

    if (!normalizedFileNamePrefix.empty())
    {
        const std::string filename = lowercase(
            filenameFromGenericPath(record.relativePath));
        if (filename.rfind(normalizedFileNamePrefix, 0) != 0)
            return false;
    }

    return true;
}

bool ModuleAssetRegistry::scan()
{
    if (m_assetRoot.empty())
        return true;

    try
    {
        std::vector<ModuleAssetRecord> discovered;
        std::error_code error;
        if (!std::filesystem::is_directory(m_assetRoot, error) || error)
        {
            m_lastError = error
                ? "Could not inspect Assets directory: " + error.message()
                : "Assets directory does not exist: " + heritage::paths::toUtf8(m_assetRoot);
            if (!m_records.empty())
            {
                m_records.clear();
                ++m_revision;
            }
            return false;
        }

        std::filesystem::recursive_directory_iterator iterator(
            m_assetRoot,
            std::filesystem::directory_options::skip_permission_denied,
            error);
        const std::filesystem::recursive_directory_iterator end;
        while (!error && iterator != end)
        {
            std::error_code entryError;
            if (iterator->is_regular_file(entryError) && !entryError)
            {
                const std::filesystem::path absolute = iterator->path();
                const std::filesystem::path relative = absolute.lexically_relative(m_assetRoot);
                if (!relative.empty() && !relative.is_absolute())
                {
                    ModuleAssetRecord record;
                    record.relativePath = heritage::paths::toUtf8(relative.lexically_normal());
                    record.extension = lowercase(heritage::paths::toUtf8(absolute.extension()));
                    record.sizeBytes = iterator->file_size(entryError);
                    if (entryError)
                        record.sizeBytes = 0;
                    entryError.clear();
                    record.writeTime = iterator->last_write_time(entryError);
                    if (entryError)
                        record.writeTime = {};
                    discovered.push_back(std::move(record));
                }
            }

            iterator.increment(error);
            if (error)
            {
                // A single transient/inaccessible entry is not fatal to the engine.
                m_lastError = "Asset scan stopped early: " + error.message();
                error.clear();
                break;
            }
        }

        std::sort(
            discovered.begin(), discovered.end(),
            [](const ModuleAssetRecord& left, const ModuleAssetRecord& right) {
                return left.relativePath < right.relativePath;
            });

        if (!recordsEqual(m_records, discovered))
        {
            m_records = std::move(discovered);
            ++m_revision;
        }

        if (m_lastError.rfind("Asset scan stopped early:", 0) != 0)
            m_lastError.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        m_lastError = std::string("Asset discovery exception: ") + exception.what();
        std::cerr << "[AssetRegistry] " << m_lastError << '\n';
        return false;
    }
    catch (...)
    {
        m_lastError = "Asset discovery exception: unknown failure";
        std::cerr << "[AssetRegistry] " << m_lastError << '\n';
        return false;
    }
}

} // namespace heritage::modules
