#include "InputSystemInternal.hpp"

namespace heritage::input {
using namespace input_internal;

std::filesystem::path InputSystem::profilesDirectory() const
{
    if (m_settingsPath.empty())
        return {};
    return m_settingsPath.parent_path() / "InputProfiles";
}

bool InputSystem::validateProfileName(
    const std::string& profileName,
    std::string& cleanName,
    std::string& errorMessage) const
{
    cleanName = trim(profileName);
    if (cleanName.empty())
    {
        errorMessage = "Profile names cannot be empty.";
        return false;
    }
    if (cleanName.size() > 48)
    {
        errorMessage = "Profile names may contain at most 48 characters.";
        return false;
    }
    if (cleanName == "." || cleanName == "..")
    {
        errorMessage = "That profile name is reserved.";
        return false;
    }
    if (cleanName.back() == ' ' || cleanName.back() == '.')
    {
        errorMessage = "Profile names cannot end with a space or period.";
        return false;
    }

    constexpr const char* invalid = "<>:\"/\\|?*=";
    for (unsigned char character : cleanName)
    {
        if (character < 32 || std::strchr(invalid, character) != nullptr)
        {
            errorMessage =
                "Profile names cannot contain control characters or <>:\"/\\|?*=.";
            return false;
        }
    }

    if (isReservedWindowsProfileName(cleanName))
    {
        errorMessage = "That profile name is reserved by Windows.";
        return false;
    }

    errorMessage.clear();
    return true;
}

std::filesystem::path InputSystem::profilePathForName(
    const std::string& cleanName) const
{
    const std::filesystem::path directory = profilesDirectory();
    if (directory.empty())
        return {};
    return directory / (cleanName + ".heinputprofile");
}

std::vector<InputProfileInfo> InputSystem::profiles() const
{
    std::vector<InputProfileInfo> result;
    const std::filesystem::path directory = profilesDirectory();
    if (directory.empty() || !std::filesystem::is_directory(directory))
        return result;

    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error))
    {
        if (error)
            break;
        if (!entry.is_regular_file(error) || error)
            continue;
        if (lower(entry.path().extension().string()) != ".heinputprofile")
            continue;

        ProfileSnapshot snapshot;
        std::string readError;
        if (!readProfileSnapshot(entry.path(), snapshot, readError))
            continue;

        InputProfileInfo info;
        info.name = snapshot.name;
        info.path = entry.path();
        result.push_back(std::move(info));
    }

    std::sort(result.begin(), result.end(),
        [](const InputProfileInfo& left, const InputProfileInfo& right) {
            std::string leftName = left.name;
            std::string rightName = right.name;
            std::transform(leftName.begin(), leftName.end(), leftName.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
            std::transform(rightName.begin(), rightName.end(), rightName.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
            return leftName < rightName;
        });
    return result;
}

std::filesystem::path InputSystem::findProfilePath(
    const std::string& profileName) const
{
    const std::string clean = trim(profileName);
    for (const InputProfileInfo& profile : profiles())
    {
        if (profileNamesEqual(profile.name, clean))
            return profile.path;
    }
    return {};
}

bool InputSystem::profileExists(const std::string& profileName) const
{
    return !findProfilePath(profileName).empty();
}

InputSystem::ProfileSnapshot InputSystem::captureProfileSnapshot(
    const std::string& profileName) const
{
    ProfileSnapshot snapshot;
    snapshot.name = profileName;

    for (const auto& [actionName, record] : m_actions)
    {
        ProfileActionSnapshot action;
        action.bindings.assign(kMaxBindingsPerAction, {});

        for (std::size_t index = 0;
            index < record.bindings.size() && index < kMaxBindingsPerAction;
            ++index)
        {
            action.bindings[index] = record.bindings[index];

            if (index >= record.parsedBindings.size()
                || index >= record.analogSettings.size()
                || !parsedBindingSupportsAnalog(record.parsedBindings[index]))
            {
                continue;
            }

            LoadedAnalogSettings loaded;
            loaded.binding = record.parsedBindings[index].canonical;
            loaded.settings = sanitizeAnalogSettings(record.analogSettings[index]);
            action.analogSettings[index] = loaded;
        }

        snapshot.actions[actionName] = std::move(action);
    }

    return snapshot;
}

bool InputSystem::writeProfileSnapshot(
    const std::filesystem::path& path,
    const ProfileSnapshot& snapshot)
{
    try
    {
        std::filesystem::create_directories(path.parent_path());
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream file(temporary, std::ios::trunc);
        if (!file)
        {
            m_lastError = "Could not write input profile: " + path.string();
            return false;
        }

        file << "# Heritage Engine named input profile\n";
        file << "# Complete snapshot: eight binding slots and analogue settings\n";
        file << "profile_format=1\n";
        file << "profile_name=" << snapshot.name << '\n';
        file << std::fixed << std::setprecision(6);

        for (const auto& [actionName, action] : snapshot.actions)
        {
            file << "binding_count." << actionName << '='
                << kMaxBindingsPerAction << '\n';

            for (std::size_t index = 0; index < kMaxBindingsPerAction; ++index)
            {
                file << "binding." << actionName << '.' << index << '=';
                if (index < action.bindings.size())
                    file << action.bindings[index];
                file << '\n';
            }

            for (const auto& [index, loaded] : action.analogSettings)
            {
                if (index >= kMaxBindingsPerAction || loaded.binding.empty())
                    continue;

                const InputAnalogSettings settings =
                    sanitizeAnalogSettings(loaded.settings);
                file << "analog." << actionName << '.' << index << '='
                    << loaded.binding << '|'
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

        file.close();
        if (!file)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            m_lastError = "Could not finish writing input profile: " + path.string();
            return false;
        }

        std::error_code error;
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            m_lastError = "Could not install input profile: " + path.string();
            return false;
        }

        m_lastError.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        m_lastError =
            std::string("Could not save input profile: ") + exception.what();
        return false;
    }
}

bool InputSystem::readProfileSnapshot(
    const std::filesystem::path& path,
    ProfileSnapshot& snapshot,
    std::string& errorMessage) const
{
    snapshot = {};

    std::ifstream file(path);
    if (!file)
    {
        errorMessage = "Could not open input profile: " + path.string();
        return false;
    }

    std::unordered_map<std::string, std::map<std::size_t, std::string>> indexed;
    std::unordered_map<std::string, std::size_t> declaredCounts;
    std::unordered_map<std::string, std::map<std::size_t, LoadedAnalogSettings>>
        analog;

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

        if (key == "profile_name")
        {
            snapshot.name = value;
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
                analog[actionName][
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
                declaredCounts[actionName] =
                    static_cast<std::size_t>(std::stoull(value));
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
        if (lastDot == std::string::npos)
            continue;

        const std::string indexText = tail.substr(lastDot + 1);
        if (!isUnsignedInteger(indexText))
            continue;

        const std::string actionName = tail.substr(0, lastDot);
        indexed[actionName][
            static_cast<std::size_t>(std::stoull(indexText))] = value;
    }

    if (snapshot.name.empty())
        snapshot.name = path.stem().string();

    for (const auto& [actionName, count] : declaredCounts)
    {
        (void)count;
        snapshot.actions[actionName].bindings.assign(
            kMaxBindingsPerAction,
            {});
    }
    for (const auto& [actionName, values] : indexed)
    {
        ProfileActionSnapshot& action = snapshot.actions[actionName];
        if (action.bindings.empty())
            action.bindings.assign(kMaxBindingsPerAction, {});

        for (const auto& [index, value] : values)
        {
            if (index < kMaxBindingsPerAction)
                action.bindings[index] = value;
        }
    }
    for (const auto& [actionName, values] : analog)
    {
        ProfileActionSnapshot& action = snapshot.actions[actionName];
        if (action.bindings.empty())
            action.bindings.assign(kMaxBindingsPerAction, {});
        action.analogSettings = values;
    }

    errorMessage.clear();
    return true;
}

bool InputSystem::applyProfileSnapshot(const ProfileSnapshot& snapshot)
{
    struct PreparedAction
    {
        std::vector<std::string> bindings;
        std::vector<ParsedBinding> parsedBindings;
        std::vector<InputAnalogSettings> analogSettings;
    };

    std::map<std::string, PreparedAction> prepared;

    for (const auto& [actionName, record] : m_actions)
    {
        const auto profileAction = snapshot.actions.find(actionName);
        if (profileAction == snapshot.actions.end())
            continue;

        PreparedAction action;
        action.bindings.assign(kMaxBindingsPerAction, {});
        action.parsedBindings.assign(kMaxBindingsPerAction, ParsedBinding{});
        action.analogSettings.assign(kMaxBindingsPerAction, InputAnalogSettings{});

        for (std::size_t index = 0; index < kMaxBindingsPerAction; ++index)
        {
            if (index >= profileAction->second.bindings.size()
                || profileAction->second.bindings[index].empty())
            {
                continue;
            }

            ParsedBinding parsed;
            std::string parseError;
            if (!parseBinding(
                profileAction->second.bindings[index],
                parsed,
                parseError))
            {
                m_lastError = "Profile '" + snapshot.name
                    + "', action '" + actionName
                    + "', Binding " + std::to_string(index + 1)
                    + ": " + parseError;
                return false;
            }

            if (containsBinding(action.bindings, parsed.canonical))
            {
                m_lastError = "Profile '" + snapshot.name
                    + "' binds the same input more than once to action '"
                    + actionName + "'.";
                return false;
            }

            action.bindings[index] = parsed.canonical;
            action.parsedBindings[index] = parsed;
            action.analogSettings[index] = defaultAnalogSettings(parsed);

            const auto analogSetting =
                profileAction->second.analogSettings.find(index);
            if (analogSetting != profileAction->second.analogSettings.end()
                && analogSetting->second.binding == parsed.canonical
                && parsedBindingSupportsAnalog(parsed))
            {
                action.analogSettings[index] =
                    sanitizeAnalogSettings(analogSetting->second.settings);
            }
        }

        prepared[actionName] = std::move(action);
    }

    for (auto& [actionName, action] : prepared)
    {
        ActionRecord& record = m_actions[actionName];
        record.bindings = std::move(action.bindings);
        record.parsedBindings = std::move(action.parsedBindings);
        record.analogSettings = std::move(action.analogSettings);
        record.hasUserBindings = true;
        record.value = 0.0f;
        record.down = false;
        record.pressed = false;
        record.released = false;
    }

    cancelBindingCapture();
    m_lastAppliedProfile = snapshot.name;
    m_profileDirty = false;
    return save();
}

bool InputSystem::createProfile(const std::string& profileName)
{
    std::string cleanName;
    std::string errorMessage;
    if (!validateProfileName(profileName, cleanName, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }
    if (profileExists(cleanName))
    {
        m_lastError = "An input profile named '" + cleanName + "' already exists.";
        return false;
    }

    const ProfileSnapshot snapshot = captureProfileSnapshot(cleanName);
    if (!writeProfileSnapshot(profilePathForName(cleanName), snapshot))
        return false;

    m_lastAppliedProfile = cleanName;
    m_profileDirty = false;
    return save();
}

bool InputSystem::updateProfile(const std::string& profileName)
{
    const std::filesystem::path path = findProfilePath(profileName);
    if (path.empty())
    {
        m_lastError = "Input profile not found: " + trim(profileName);
        return false;
    }

    ProfileSnapshot existing;
    std::string errorMessage;
    if (!readProfileSnapshot(path, existing, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }

    const ProfileSnapshot snapshot = captureProfileSnapshot(existing.name);
    if (!writeProfileSnapshot(path, snapshot))
        return false;

    m_lastAppliedProfile = existing.name;
    m_profileDirty = false;
    return save();
}

bool InputSystem::applyProfile(const std::string& profileName)
{
    const std::filesystem::path path = findProfilePath(profileName);
    if (path.empty())
    {
        m_lastError = "Input profile not found: " + trim(profileName);
        return false;
    }

    ProfileSnapshot snapshot;
    std::string errorMessage;
    if (!readProfileSnapshot(path, snapshot, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }

    return applyProfileSnapshot(snapshot);
}

bool InputSystem::duplicateProfile(
    const std::string& sourceProfileName,
    const std::string& newProfileName)
{
    const std::filesystem::path sourcePath =
        findProfilePath(sourceProfileName);
    if (sourcePath.empty())
    {
        m_lastError = "Input profile not found: " + trim(sourceProfileName);
        return false;
    }

    std::string cleanName;
    std::string errorMessage;
    if (!validateProfileName(newProfileName, cleanName, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }
    if (profileExists(cleanName))
    {
        m_lastError = "An input profile named '" + cleanName + "' already exists.";
        return false;
    }

    ProfileSnapshot snapshot;
    if (!readProfileSnapshot(sourcePath, snapshot, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }

    snapshot.name = cleanName;
    return writeProfileSnapshot(profilePathForName(cleanName), snapshot);
}

bool InputSystem::renameProfile(
    const std::string& oldProfileName,
    const std::string& newProfileName)
{
    const std::filesystem::path oldPath = findProfilePath(oldProfileName);
    if (oldPath.empty())
    {
        m_lastError = "Input profile not found: " + trim(oldProfileName);
        return false;
    }

    std::string cleanName;
    std::string errorMessage;
    if (!validateProfileName(newProfileName, cleanName, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }
    if (!profileNamesEqual(oldProfileName, cleanName)
        && profileExists(cleanName))
    {
        m_lastError = "An input profile named '" + cleanName + "' already exists.";
        return false;
    }

    ProfileSnapshot snapshot;
    if (!readProfileSnapshot(oldPath, snapshot, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }

    snapshot.name = cleanName;
    const bool sameLogicalName =
        profileNamesEqual(oldProfileName, cleanName);
    const std::filesystem::path newPath = sameLogicalName
        ? oldPath
        : profilePathForName(cleanName);
    if (!writeProfileSnapshot(newPath, snapshot))
        return false;

    if (!sameLogicalName && oldPath != newPath)
    {
        std::error_code error;
        std::filesystem::remove(oldPath, error);
        if (error)
        {
            std::error_code ignored;
            std::filesystem::remove(newPath, ignored);
            m_lastError = "Could not remove the old input profile file.";
            return false;
        }
    }

    if (profileNamesEqual(m_lastAppliedProfile, oldProfileName))
    {
        m_lastAppliedProfile = cleanName;
        return save();
    }

    m_lastError.clear();
    return true;
}

bool InputSystem::deleteProfile(const std::string& profileName)
{
    const std::filesystem::path path = findProfilePath(profileName);
    if (path.empty())
    {
        m_lastError = "Input profile not found: " + trim(profileName);
        return false;
    }

    std::error_code error;
    const bool removed = std::filesystem::remove(path, error);
    if (error || !removed)
    {
        m_lastError = "Could not delete input profile: " + path.string();
        return false;
    }

    if (profileNamesEqual(m_lastAppliedProfile, profileName))
    {
        m_lastAppliedProfile.clear();
        m_profileDirty = false;
        return save();
    }

    m_lastError.clear();
    return true;
}

void InputSystem::markProfileDirty()
{
    if (!m_lastAppliedProfile.empty())
        m_profileDirty = true;
}


} // namespace heritage::input
