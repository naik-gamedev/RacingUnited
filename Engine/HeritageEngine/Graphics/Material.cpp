#include "Material.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <sstream>

namespace heritage::graphics {
namespace {

std::string trim(std::string value)
{
    const auto first = std::find_if_not(
        value.begin(), value.end(),
        [](unsigned char c) { return std::isspace(c) != 0; });
    if (first == value.end())
        return {};

    const auto last = std::find_if_not(
        value.rbegin(), value.rend(),
        [](unsigned char c) { return std::isspace(c) != 0; }).base();
    return std::string(first, last);
}

std::string lowercase(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float roughnessFromShininess(float shininess)
{
    // Common Blinn-Phong -> GGX-style perceptual roughness approximation.
    // MTL allows Ns in [0,1000]. Keeping a small floor avoids singular
    // highlights while preserving mirror-like authored values.
    const float safe = std::max(0.0f, shininess);
    return std::clamp(std::sqrt(2.0f / (safe + 2.0f)), 0.04f, 1.0f);
}

std::vector<std::string> tokenizeMapArguments(const std::string& text)
{
    std::vector<std::string> tokens;
    std::string current;
    bool quoted = false;
    char quote = '\0';

    for (char c : text)
    {
        if (quoted)
        {
            if (c == quote)
            {
                quoted = false;
            }
            else
            {
                current.push_back(c);
            }
            continue;
        }

        if (c == '"' || c == '\'')
        {
            quoted = true;
            quote = c;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(c)) != 0)
        {
            if (!current.empty())
            {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(c);
    }

    if (!current.empty())
        tokens.push_back(current);
    return tokens;
}

int mapOptionArgumentCount(const std::string& option)
{
    const std::string lowered = lowercase(option);
    if (lowered == "-mm")
        return 2;
    if (lowered == "-o" || lowered == "-s" || lowered == "-t")
        return 3;
    if (lowered == "-blendu" || lowered == "-blendv"
        || lowered == "-cc" || lowered == "-clamp"
        || lowered == "-texres" || lowered == "-bm"
        || lowered == "-imfchan" || lowered == "-type")
    {
        return 1;
    }
    return 0;
}

bool looksLikeWindowsAbsolutePath(const std::string& raw)
{
    return raw.size() >= 3
        && std::isalpha(static_cast<unsigned char>(raw[0])) != 0
        && raw[1] == ':'
        && (raw[2] == '/' || raw[2] == '\\');
}

std::filesystem::path extractMapPath(
    const std::string& argumentText,
    const std::filesystem::path& materialDirectory)
{
    const std::vector<std::string> tokens = tokenizeMapArguments(argumentText);
    if (tokens.empty())
        return {};

    std::size_t index = 0;
    while (index < tokens.size() && !tokens[index].empty() && tokens[index][0] == '-')
    {
        const int argumentCount = mapOptionArgumentCount(tokens[index]);
        ++index;
        for (int argument = 0; argument < argumentCount && index < tokens.size(); ++argument)
            ++index;
    }

    if (index >= tokens.size())
        return {};

    std::string raw = tokens[index++];
    while (index < tokens.size())
    {
        raw += " ";
        raw += tokens[index++];
    }

    std::filesystem::path requested(raw);
    if (requested.empty())
        return {};

    if (requested.is_absolute()
        || requested.has_root_name()
        || looksLikeWindowsAbsolutePath(raw))
    {
        // Blender and other DCCs often export workstation-specific absolute
        // paths. Prefer a portable copy beside the MTL when it exists.
        std::string portableRaw = raw;
        std::replace(
            portableRaw.begin(), portableRaw.end(), '\\', '/');
        const std::filesystem::path localCandidate =
            materialDirectory
            / std::filesystem::path(portableRaw).filename();
        std::error_code localError;
        if (std::filesystem::is_regular_file(localCandidate, localError))
            return localCandidate.lexically_normal();

        // Preserve the original path for diagnostics. The renderer applies the
        // module Assets sandbox and will refuse to load it from outside.
        return requested.lexically_normal();
    }

    return (materialDirectory / requested).lexically_normal();
}

bool filenameSuggests(
    const std::filesystem::path& path,
    const char* word)
{
    const std::string file = lowercase(path.filename().string());
    return file.find(word) != std::string::npos;
}

} // namespace

MaterialLibraryLoadResult loadMaterialLibrary(
    const std::filesystem::path& materialLibraryPath)
{
    MaterialLibraryLoadResult result;

    std::ifstream file(materialLibraryPath);
    if (!file.is_open())
    {
        result.warnings.push_back(
            "Could not open MTL: " + materialLibraryPath.string());
        return result;
    }

    const std::filesystem::path materialDirectory =
        materialLibraryPath.parent_path();

    MaterialDefinition* current = nullptr;
    bool explicitRoughness = false;

    std::string line;
    while (std::getline(file, line))
    {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos)
            line.erase(comment);

        line = trim(std::move(line));
        if (line.empty())
            continue;

        std::istringstream stream(line);
        std::string token;
        stream >> token;
        const std::string lowered = lowercase(token);

        if (lowered == "newmtl")
        {
            std::string name;
            std::getline(stream, name);
            name = trim(std::move(name));
            if (name.empty())
            {
                current = nullptr;
                result.warnings.push_back(
                    "MTL contains an unnamed material: "
                    + materialLibraryPath.string());
                continue;
            }

            MaterialDefinition definition;
            definition.name = name;
            definition.sourceDirectory = materialDirectory;
            auto [iterator, inserted] =
                result.materials.insert_or_assign(name, std::move(definition));
            current = &iterator->second;
            explicitRoughness = false;
            continue;
        }

        if (!current)
            continue;

        if (lowered == "kd")
        {
            stream >> current->baseColor.x
                   >> current->baseColor.y
                   >> current->baseColor.z;
            current->baseColor.x = clamp01(current->baseColor.x);
            current->baseColor.y = clamp01(current->baseColor.y);
            current->baseColor.z = clamp01(current->baseColor.z);
        }
        else if (lowered == "ks")
        {
            stream >> current->specularColor.x
                   >> current->specularColor.y
                   >> current->specularColor.z;
            current->specularColor.x = clamp01(current->specularColor.x);
            current->specularColor.y = clamp01(current->specularColor.y);
            current->specularColor.z = clamp01(current->specularColor.z);
        }
        else if (lowered == "ke")
        {
            stream >> current->emissiveColor.x
                   >> current->emissiveColor.y
                   >> current->emissiveColor.z;
            current->emissiveColor.x = std::max(0.0f, current->emissiveColor.x);
            current->emissiveColor.y = std::max(0.0f, current->emissiveColor.y);
            current->emissiveColor.z = std::max(0.0f, current->emissiveColor.z);
        }
        else if (lowered == "ns")
        {
            stream >> current->shininess;
            current->shininess = std::max(0.0f, current->shininess);
            if (!explicitRoughness)
                current->roughness = roughnessFromShininess(current->shininess);
        }
        else if (lowered == "pr")
        {
            stream >> current->roughness;
            current->roughness = clamp01(current->roughness);
            explicitRoughness = true;
        }
        else if (lowered == "pm")
        {
            stream >> current->metallic;
            current->metallic = clamp01(current->metallic);
        }
        else if (lowered == "d")
        {
            stream >> current->opacity;
            current->opacity = clamp01(current->opacity);
        }
        else if (lowered == "tr")
        {
            float transparency = 0.0f;
            stream >> transparency;
            current->opacity = 1.0f - clamp01(transparency);
        }
        else if (lowered.rfind("map_", 0) == 0
            || lowered == "bump"
            || lowered == "norm"
            || lowered == "normal"
            || lowered == "map_normal"
            || lowered == "map_roughness"
            || lowered == "map_metallic"
            || lowered == "map_ao")
        {
            std::string arguments;
            std::getline(stream, arguments);
            const std::filesystem::path mapPath =
                extractMapPath(arguments, materialDirectory);
            if (mapPath.empty())
                continue;

            if (lowered == "map_kd")
            {
                current->baseColorMap.filePath = mapPath;
            }
            else if (lowered == "map_ks")
            {
                current->specularMap.filePath = mapPath;
            }
            else if (lowered == "map_ke")
            {
                current->emissiveMap.filePath = mapPath;
            }
            else if (lowered == "map_pr" || lowered == "map_roughness")
            {
                current->roughnessMap.filePath = mapPath;
            }
            else if (lowered == "map_pm" || lowered == "map_metallic")
            {
                current->metallicMap.filePath = mapPath;
            }
            else if (lowered == "map_ka" || lowered == "map_ao")
            {
                current->ambientOcclusionMap.filePath = mapPath;
            }
            else if (lowered == "norm"
                || lowered == "normal"
                || lowered == "map_normal")
            {
                current->normalMap.filePath = mapPath;
            }
            else if (lowered == "map_bump" || lowered == "bump")
            {
                // Height/bump maps require a different conversion path. Treat
                // exporter-provided *_Normal textures as tangent-space normals;
                // otherwise leave them unused rather than shading incorrectly.
                if (filenameSuggests(mapPath, "normal"))
                    current->normalMap.filePath = mapPath;
                else
                    result.warnings.push_back(
                        "Ignoring unsupported height/bump map for material '"
                        + current->name + "': " + mapPath.string());
            }
            else if (lowered == "map_d")
            {
                // Standard MTL uses map_d for opacity. Some Blender-era asset
                // pipelines incorrectly wrote an Albedo/Diffuse texture here;
                // recognize that obvious filename case without globally
                // redefining map_d semantics.
                if (filenameSuggests(mapPath, "albedo")
                    || filenameSuggests(mapPath, "diffuse")
                    || filenameSuggests(mapPath, "basecolor")
                    || filenameSuggests(mapPath, "base_color"))
                {
                    if (current->baseColorMap.empty())
                        current->baseColorMap.filePath = mapPath;
                }
                else
                {
                    current->opacityMap.filePath = mapPath;
                }
            }
            else if (lowered == "map_ns")
            {
                // A few exporters place a true roughness texture in map_Ns.
                // Only accept the unmistakable naming case; ordinary shininess
                // maps are not silently inverted.
                if (filenameSuggests(mapPath, "roughness")
                    || filenameSuggests(mapPath, "rough"))
                {
                    current->roughnessMap.filePath = mapPath;
                }
            }
        }
    }

    return result;
}

} // namespace heritage::graphics
