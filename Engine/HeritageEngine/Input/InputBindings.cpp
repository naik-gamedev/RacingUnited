#include "InputSystemInternal.hpp"

namespace heritage::input {
using namespace input_internal;

bool InputSystem::registerAction(
    const std::string& actionName,
    const std::string& defaultBinding,
    const std::string& group)
{
    const std::string cleanName = trim(actionName);
    if (cleanName.empty())
    {
        m_lastError = "Input action names cannot be empty.";
        return false;
    }

    std::string cleanGroup = trim(group);
    if (cleanGroup.empty())
        cleanGroup = "Common";
    rememberActionGroup(cleanGroup);

    std::vector<ParsedBinding> parsedDefaults;
    std::vector<std::string> canonicalDefaults;
    std::string parseError;
    // INPUT03: a module may deliberately declare a bindable action without a
    // factory default. This is important for large optional control surfaces
    // such as H-pattern/direct gear selectors: exposing Gear 1..24 must not
    // steal arbitrary keyboard/gamepad buttons merely to make the row exist.
    if (!trim(defaultBinding).empty()
        && !parseBindingList(
            defaultBinding,
            parsedDefaults,
            canonicalDefaults,
            parseError))
    {
        m_lastError = "Action '" + cleanName + "': " + parseError;
        return false;
    }

    if (canonicalDefaults.size() > kMaxBindingsPerAction)
    {
        m_lastError = "Action '" + cleanName
            + "' declares more than "
            + std::to_string(kMaxBindingsPerAction)
            + " default bindings.";
        return false;
    }

    auto iterator = m_actions.find(cleanName);
    if (iterator == m_actions.end())
    {
        ActionRecord record;
        record.group = cleanGroup;
        record.defaultBindings.assign(kMaxBindingsPerAction, {});
        record.bindings.assign(kMaxBindingsPerAction, {});
        record.parsedBindings.assign(kMaxBindingsPerAction, ParsedBinding{});
        record.analogSettings.assign(kMaxBindingsPerAction, InputAnalogSettings{});

        for (std::size_t index = 0; index < canonicalDefaults.size(); ++index)
            record.defaultBindings[index] = canonicalDefaults[index];

        const bool hasLoadedOverride =
            m_loadedBindingOverrides.find(cleanName) != m_loadedBindingOverrides.end();
        record.hasUserBindings = hasLoadedOverride;

        std::vector<std::string> selectedBindings(kMaxBindingsPerAction);
        if (hasLoadedOverride)
        {
            const auto loaded = m_loadedBindings.find(cleanName);
            if (loaded != m_loadedBindings.end())
            {
                for (std::size_t index = 0;
                    index < loaded->second.size() && index < kMaxBindingsPerAction;
                    ++index)
                {
                    selectedBindings[index] = loaded->second[index];
                }
            }
        }
        else
        {
            for (std::size_t index = 0; index < canonicalDefaults.size(); ++index)
                selectedBindings[index] = canonicalDefaults[index];
        }

        // Step 26E wrote one legacy binding per action. Preserve that primary
        // choice while adding newly declared secondary defaults into the next
        // empty slots.
        if (hasLoadedOverride
            && m_loadedLegacyActions.find(cleanName) != m_loadedLegacyActions.end()
            && canonicalDefaults.size() > 1)
        {
            for (std::size_t index = 1; index < canonicalDefaults.size(); ++index)
            {
                if (containsBinding(selectedBindings, canonicalDefaults[index]))
                    continue;
                const std::size_t empty = firstEmptyBindingSlot(selectedBindings);
                if (empty >= kMaxBindingsPerAction)
                    break;
                selectedBindings[empty] = canonicalDefaults[index];
            }
        }

        bool selectedValid = true;
        for (std::size_t index = 0; index < kMaxBindingsPerAction; ++index)
        {
            if (selectedBindings[index].empty())
                continue;

            ParsedBinding parsed;
            if (!parseBinding(selectedBindings[index], parsed, parseError))
            {
                selectedValid = false;
                break;
            }
            if (containsBinding(record.bindings, parsed.canonical))
                continue;

            record.bindings[index] = parsed.canonical;
            record.parsedBindings[index] = std::move(parsed);
        }

        if (!selectedValid)
        {
            record.bindings.assign(kMaxBindingsPerAction, {});
            record.parsedBindings.assign(kMaxBindingsPerAction, ParsedBinding{});
            for (std::size_t index = 0; index < canonicalDefaults.size(); ++index)
            {
                record.bindings[index] = canonicalDefaults[index];
                record.parsedBindings[index] = parsedDefaults[index];
            }
            record.hasUserBindings = false;
        }

        initializeAnalogSettings(cleanName, record);
        m_actions.emplace(cleanName, std::move(record));
    }
    else
    {
        ActionRecord& record = iterator->second;
        if (record.defaultBindings.size() != kMaxBindingsPerAction)
            record.defaultBindings.resize(kMaxBindingsPerAction);
        if (record.bindings.size() != kMaxBindingsPerAction)
            record.bindings.resize(kMaxBindingsPerAction);
        if (record.parsedBindings.size() != kMaxBindingsPerAction)
            record.parsedBindings.resize(kMaxBindingsPerAction);
        if (record.analogSettings.size() != kMaxBindingsPerAction)
            record.analogSettings.resize(kMaxBindingsPerAction);

        // Native module declarations load before Lua. A later Lua call that
        // omits its optional group must not move a Car or Motorcycle action
        // back into Common.
        if ((record.group.empty() || record.group == "Common")
            && cleanGroup != "Common")
        {
            record.group = cleanGroup;
        }

        std::size_t newDefaultCount = 0;
        for (const std::string& canonical : canonicalDefaults)
        {
            if (!containsBinding(record.defaultBindings, canonical))
                ++newDefaultCount;
        }

        if (occupiedBindingCount(record.defaultBindings) + newDefaultCount
            > kMaxBindingsPerAction)
        {
            m_lastError = "Action '" + cleanName
                + "' would exceed the eight-binding limit.";
            return false;
        }

        for (std::size_t index = 0; index < canonicalDefaults.size(); ++index)
        {
            const std::string& canonical = canonicalDefaults[index];
            if (!containsBinding(record.defaultBindings, canonical))
            {
                const std::size_t defaultSlot = firstEmptyBindingSlot(record.defaultBindings);
                if (defaultSlot < kMaxBindingsPerAction)
                    record.defaultBindings[defaultSlot] = canonical;
            }

            if (!record.hasUserBindings
                && !containsBinding(record.bindings, canonical))
            {
                const std::size_t bindingSlot = firstEmptyBindingSlot(record.bindings);
                if (bindingSlot < kMaxBindingsPerAction)
                {
                    record.bindings[bindingSlot] = canonical;
                    record.parsedBindings[bindingSlot] = parsedDefaults[index];
                    record.analogSettings[bindingSlot] =
                        defaultAnalogSettings(parsedDefaults[index]);
                }
            }
        }
    }

    m_lastError.clear();
    return true;
}

bool InputSystem::setBinding(
    const std::string& actionName,
    const std::string& binding)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    return setBinding(actionName, 0, binding);
}

bool InputSystem::setBinding(
    const std::string& actionName,
    std::size_t bindingIndex,
    const std::string& binding)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    ActionRecord& record = iterator->second;
    if (bindingIndex >= record.bindings.size())
    {
        m_lastError = "Binding index is outside action '" + actionName + "'.";
        return false;
    }

    ParsedBinding parsed;
    std::string parseError;
    if (!parseBinding(binding, parsed, parseError))
    {
        m_lastError = parseError;
        return false;
    }

    if (containsBinding(record.bindings, parsed.canonical, bindingIndex))
    {
        m_lastError = "That input is already bound to '" + actionName + "'.";
        return false;
    }

    record.bindings[bindingIndex] = parsed.canonical;
    record.parsedBindings[bindingIndex] = std::move(parsed);
    if (bindingIndex >= record.analogSettings.size())
        record.analogSettings.resize(record.parsedBindings.size());
    record.analogSettings[bindingIndex] =
        defaultAnalogSettings(record.parsedBindings[bindingIndex]);
    record.hasUserBindings = true;
    record.value = 0.0f;
    record.down = false;
    record.pressed = false;
    record.released = false;
    markProfileDirty();

    return save();
}

bool InputSystem::addBinding(
    const std::string& actionName,
    const std::string& binding)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    ParsedBinding parsed;
    std::string parseError;
    if (!parseBinding(binding, parsed, parseError))
    {
        m_lastError = parseError;
        return false;
    }

    ActionRecord& record = iterator->second;
    const std::size_t emptySlot = firstEmptyBindingSlot(record.bindings);
    if (emptySlot >= kMaxBindingsPerAction)
    {
        m_lastError = "Action '" + actionName
            + "' already has the maximum of eight bindings.";
        return false;
    }

    if (containsBinding(record.bindings, parsed.canonical))
    {
        m_lastError = "That input is already bound to '" + actionName + "'.";
        return false;
    }

    record.bindings[emptySlot] = parsed.canonical;
    record.parsedBindings[emptySlot] = std::move(parsed);
    record.analogSettings[emptySlot] =
        defaultAnalogSettings(record.parsedBindings[emptySlot]);
    record.hasUserBindings = true;
    markProfileDirty();
    return save();
}

bool InputSystem::removeBinding(
    const std::string& actionName,
    std::size_t bindingIndex)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    ActionRecord& record = iterator->second;
    if (bindingIndex >= kMaxBindingsPerAction
        || bindingIndex >= record.bindings.size())
    {
        m_lastError = "Binding index is outside action '" + actionName + "'.";
        return false;
    }
    if (record.bindings[bindingIndex].empty())
    {
        m_lastError.clear();
        return true;
    }

    // Binding slots are positional. Removing Binding 1 must never pull
    // Binding 2 into its place; the selected cell simply becomes empty.
    record.bindings[bindingIndex].clear();
    record.parsedBindings[bindingIndex] = ParsedBinding{};
    record.analogSettings[bindingIndex] = InputAnalogSettings{};
    record.hasUserBindings = true;
    record.value = 0.0f;
    record.down = false;
    record.pressed = false;
    record.released = false;
    markProfileDirty();
    return save();
}

bool InputSystem::resetBindings(const std::string& actionName)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    ActionRecord& record = iterator->second;
    record.bindings.assign(kMaxBindingsPerAction, {});
    record.parsedBindings.assign(kMaxBindingsPerAction, ParsedBinding{});
    record.analogSettings.assign(kMaxBindingsPerAction, InputAnalogSettings{});

    for (std::size_t index = 0;
        index < record.defaultBindings.size() && index < kMaxBindingsPerAction;
        ++index)
    {
        if (record.defaultBindings[index].empty())
            continue;

        ParsedBinding parsed;
        std::string error;
        if (!parseBinding(record.defaultBindings[index], parsed, error))
        {
            m_lastError = error;
            return false;
        }
        record.bindings[index] = parsed.canonical;
        record.parsedBindings[index] = std::move(parsed);
        record.analogSettings[index] =
            defaultAnalogSettings(record.parsedBindings[index]);
    }

    record.hasUserBindings = false;
    record.value = 0.0f;
    record.down = false;
    record.pressed = false;
    record.released = false;
    markProfileDirty();
    return save();
}

bool InputSystem::actionDown(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end() && iterator->second.down;
}

bool InputSystem::actionPressed(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end() && iterator->second.pressed;
}

bool InputSystem::actionReleased(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end() && iterator->second.released;
}

float InputSystem::actionValue(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end() ? iterator->second.value : 0.0f;
}

std::string InputSystem::actionBinding(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
        return {};

    std::ostringstream summary;
    bool first = true;
    for (const std::string& binding : iterator->second.bindings)
    {
        if (binding.empty())
            continue;
        if (!first)
            summary << " | ";
        summary << binding;
        first = false;
    }
    return summary.str();
}

std::string InputSystem::actionBinding(
    const std::string& actionName,
    std::size_t bindingIndex) const
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end()
        || bindingIndex >= iterator->second.bindings.size())
    {
        return {};
    }
    return iterator->second.bindings[bindingIndex];
}

std::size_t InputSystem::actionBindingCount(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end()
        ? occupiedBindingSpan(iterator->second.bindings)
        : 0;
}

std::vector<std::string> InputSystem::actionBindings(
    const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end()
        ? iterator->second.bindings
        : std::vector<std::string>{};
}


} // namespace heritage::input
