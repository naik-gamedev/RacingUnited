#include "InputSystemInternal.hpp"

namespace heritage::input {
using namespace input_internal;

bool InputSystem::save()
{
    if (m_settingsPath.empty())
        return true;

    try
    {
        std::filesystem::create_directories(m_settingsPath.parent_path());
        std::ofstream file(m_settingsPath, std::ios::trunc);
        if (!file)
        {
            m_lastError = "Could not write input settings: " + m_settingsPath.string();
            return false;
        }

        file << "# Heritage Engine module input bindings\n";
        file << "# Format 6: positional slots, analogue processing, and named profile state\n";
        file << "profile.last_applied=" << m_lastAppliedProfile << '\n';
        file << "profile.dirty=" << (m_profileDirty ? 1 : 0) << '\n';
        file << std::fixed << std::setprecision(6);

        for (const auto& [name, record] : m_actions)
        {
            // Defaults remain module-owned. Only user binding overrides are stored.
            if (record.hasUserBindings)
            {
                file << "binding_count." << name << '='
                    << kMaxBindingsPerAction << '\n';
                for (std::size_t index = 0;
                    index < kMaxBindingsPerAction;
                    ++index)
                {
                    file << "binding." << name << '.' << index << '='
                        << (index < record.bindings.size()
                            ? record.bindings[index]
                            : std::string{})
                        << '\n';
                }
            }

            for (std::size_t index = 0;
                index < record.parsedBindings.size()
                    && index < record.analogSettings.size();
                ++index)
            {
                const ParsedBinding& binding = record.parsedBindings[index];
                const InputAnalogSettings& settings = record.analogSettings[index];
                if (!parsedBindingSupportsAnalog(binding)
                    || analogSettingsAreDefault(binding, settings))
                {
                    continue;
                }

                file << "analog." << name << '.' << index << '='
                    << binding.canonical << '|'
                    << (settings.invert ? 1 : 0) << ','
                    << settings.innerDeadzone << ','
                    << settings.outerDeadzone << ','
                    << settings.sensitivity << ','
                    << settings.bezierX1 << ','
                    << settings.bezierY1 << ','
                    << settings.bezierX2 << ','
                    << settings.bezierY2 << '\n';
            }
        }

        m_lastError.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        m_lastError = std::string("Could not save input settings: ") + exception.what();
        return false;
    }
}



bool InputSystem::load()
{
    m_loadedBindings.clear();
    m_loadedBindingOverrides.clear();
    m_loadedLegacyActions.clear();
    m_loadedAnalogSettings.clear();
    m_lastAppliedProfile.clear();
    m_profileDirty = false;

    if (m_settingsPath.empty() || !std::filesystem::is_regular_file(m_settingsPath))
        return true;

    std::ifstream file(m_settingsPath);
    if (!file)
    {
        m_lastError = "Could not open input settings: " + m_settingsPath.string();
        return false;
    }

    std::unordered_map<std::string, std::map<std::size_t, std::string>> indexed;
    std::unordered_map<std::string, std::size_t> declaredCounts;
    std::unordered_map<std::string, std::string> legacy;

    std::string line;
    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;

        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        if (key == "profile.last_applied")
        {
            m_lastAppliedProfile = value;
            continue;
        }
        if (key == "profile.dirty")
        {
            m_profileDirty = value == "1" || lower(value) == "true";
            continue;
        }

        constexpr const char* analogPrefix = "analog.";
        if (key.rfind(analogPrefix, 0) == 0)
        {
            const std::string tail = key.substr(
                std::char_traits<char>::length(analogPrefix));
            const std::size_t lastDot = tail.rfind('.');
            if (lastDot == std::string::npos)
                continue;

            const std::string indexText = tail.substr(lastDot + 1);
            if (!isUnsignedInteger(indexText))
                continue;

            const std::string actionName = tail.substr(0, lastDot);
            const std::size_t pipe = value.find('|');
            if (actionName.empty() || pipe == std::string::npos)
                continue;

            LoadedAnalogSettings loaded;
            loaded.binding = trim(value.substr(0, pipe));
            std::stringstream values(value.substr(pipe + 1));
            std::array<std::string, 8> fields{};
            bool complete = true;
            for (std::size_t field = 0; field < fields.size(); ++field)
            {
                if (!std::getline(values, fields[field], ','))
                {
                    complete = false;
                    break;
                }
            }
            if (!complete)
                continue;

            try
            {
                loaded.settings.invert = std::stoi(trim(fields[0])) != 0;
                loaded.settings.innerDeadzone = std::stof(trim(fields[1]));
                loaded.settings.outerDeadzone = std::stof(trim(fields[2]));
                loaded.settings.sensitivity = std::stof(trim(fields[3]));
                loaded.settings.bezierX1 = std::stof(trim(fields[4]));
                loaded.settings.bezierY1 = std::stof(trim(fields[5]));
                loaded.settings.bezierX2 = std::stof(trim(fields[6]));
                loaded.settings.bezierY2 = std::stof(trim(fields[7]));
                loaded.settings = sanitizeAnalogSettings(loaded.settings);
                m_loadedAnalogSettings[actionName][
                    static_cast<std::size_t>(std::stoull(indexText))] = loaded;
            }
            catch (...) {}
            continue;
        }

        constexpr const char* countPrefix = "binding_count.";
        if (key.rfind(countPrefix, 0) == 0)
        {
            const std::string actionName = key.substr(
                std::char_traits<char>::length(countPrefix));
            try
            {
                declaredCounts[actionName] = static_cast<std::size_t>(std::stoull(value));
                m_loadedBindingOverrides.insert(actionName);
            }
            catch (...) {}
            continue;
        }

        constexpr const char* bindingPrefix = "binding.";
        if (key.rfind(bindingPrefix, 0) != 0)
            continue;

        const std::string tail = key.substr(
            std::char_traits<char>::length(bindingPrefix));
        const std::size_t lastDot = tail.rfind('.');

        if (lastDot != std::string::npos)
        {
            const std::string suffix = tail.substr(lastDot + 1);
            if (isUnsignedInteger(suffix))
            {
                const std::string actionName = tail.substr(0, lastDot);
                indexed[actionName][static_cast<std::size_t>(std::stoull(suffix))] = value;
                m_loadedBindingOverrides.insert(actionName);
                continue;
            }
        }

        // Step 26E legacy format: binding.Action Name=Key:W
        legacy[tail] = value;
        m_loadedBindingOverrides.insert(tail);
        m_loadedLegacyActions.insert(tail);
    }

    for (const auto& [actionName, count] : declaredCounts)
    {
        (void)count;
        std::vector<std::string>& bindings = m_loadedBindings[actionName];
        bindings.assign(kMaxBindingsPerAction, {});
        const auto indexedIterator = indexed.find(actionName);
        if (indexedIterator == indexed.end())
            continue;

        for (const auto& [index, value] : indexedIterator->second)
        {
            if (index < kMaxBindingsPerAction)
                bindings[index] = value;
        }
    }

    for (const auto& [actionName, values] : indexed)
    {
        if (declaredCounts.find(actionName) != declaredCounts.end())
            continue;

        std::vector<std::string>& bindings = m_loadedBindings[actionName];
        bindings.assign(kMaxBindingsPerAction, {});
        for (const auto& [index, value] : values)
        {
            if (index < kMaxBindingsPerAction)
                bindings[index] = value;
        }
    }

    for (const auto& [actionName, value] : legacy)
    {
        if (m_loadedBindings.find(actionName) != m_loadedBindings.end())
            continue;
        std::vector<std::string> bindings(kMaxBindingsPerAction);
        bindings[0] = value;
        m_loadedBindings[actionName] = std::move(bindings);
    }

    m_lastError.clear();
    return true;
}


} // namespace heritage::input
