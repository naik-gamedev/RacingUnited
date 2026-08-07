#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

#include "ModuleContext.hpp"

namespace heritage::modules {

// Persistent primitive-value storage owned by one module.
//
// The store is intentionally small and deterministic. It is not a general
// database and it does not expose arbitrary file access to Lua. All values are
// written beneath UserData/Modules/<ModuleID>/Saves/ through ModuleContext.
class ModuleSaveStore
{
public:
    bool initialize(
        const ModuleContext& context,
        std::string& warningMessage);

    void reset();

    bool has(const std::string& key) const;
    bool remove(const std::string& key);
    void clear();

    std::string getString(
        const std::string& key,
        const std::string& fallback) const;
    std::int64_t getInteger(
        const std::string& key,
        std::int64_t fallback) const;
    double getNumber(
        const std::string& key,
        double fallback) const;
    bool getBoolean(
        const std::string& key,
        bool fallback) const;

    bool setString(const std::string& key, const std::string& value);
    bool setInteger(const std::string& key, std::int64_t value);
    bool setNumber(const std::string& key, double value);
    bool setBoolean(const std::string& key, bool value);

    bool flush();

    bool isDirty() const { return m_dirty; }
    const std::filesystem::path& path() const { return m_path; }
    const std::string& lastError() const { return m_lastError; }

private:
    enum class ValueType
    {
        String,
        Integer,
        Number,
        Boolean
    };

    struct Value
    {
        ValueType type = ValueType::String;
        std::string text;
    };

    bool load();
    bool setValue(
        const std::string& key,
        ValueType type,
        const std::string& text);

    static bool isValidKey(const std::string& key);
    static char typeCode(ValueType type);
    static bool typeFromCode(char code, ValueType& type);
    static std::string escape(const std::string& value);
    static bool unescape(
        const std::string& value,
        std::string& output);

    std::filesystem::path m_path;
    std::map<std::string, Value> m_values;
    std::string m_lastError;
    bool m_dirty = false;
};

} // namespace heritage::modules
