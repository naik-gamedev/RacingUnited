#include "ModuleSaveStore.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace heritage::modules {
namespace {

constexpr const char* kHeader = "HERITAGE_MODULE_SAVE\t1";
constexpr const char* kDefaultFileName = "module_state.hsave";

bool parseInteger(const std::string& text, std::int64_t& value)
{
    if (text.empty())
        return false;

    errno = 0;
    char* end = nullptr;
    const long long parsed = std::strtoll(text.c_str(), &end, 10);
    if (errno != 0 || !end || *end != '\0')
        return false;

    value = static_cast<std::int64_t>(parsed);
    return true;
}

bool parseNumber(const std::string& text, double& value)
{
    if (text.empty())
        return false;

    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (errno != 0 || !end || *end != '\0' || !std::isfinite(parsed))
        return false;

    value = parsed;
    return true;
}

bool replaceFile(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    std::string& errorMessage)
{
#ifdef _WIN32
    if (!MoveFileExW(
        temporary.wstring().c_str(),
        destination.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        errorMessage = "Could not replace module save file. Windows error "
            + std::to_string(GetLastError()) + ".";
        return false;
    }
    return true;
#else
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (!error)
        return true;

    std::filesystem::remove(destination, error);
    error.clear();
    std::filesystem::rename(temporary, destination, error);
    if (error)
    {
        errorMessage = "Could not replace module save file: " + error.message();
        return false;
    }
    return true;
#endif
}

} // namespace

bool ModuleSaveStore::initialize(
    const ModuleContext& context,
    std::string& warningMessage)
{
    reset();

    m_path = context.resolveSavePath(kDefaultFileName);
    if (m_path.empty())
    {
        m_lastError = "Module save path could not be resolved safely.";
        warningMessage = m_lastError;
        return false;
    }

    std::error_code directoryError;
    std::filesystem::create_directories(m_path.parent_path(), directoryError);
    if (directoryError)
    {
        m_lastError = "Could not create module save directory: "
            + m_path.parent_path().string() + " ("
            + directoryError.message() + ")";
        warningMessage = m_lastError;
        return false;
    }

    if (!load())
    {
        // A damaged save must not prevent a module from launching. Preserve the
        // error for Lua/UI diagnostics and continue with an empty in-memory
        // store. A later successful Flush creates a clean file.
        warningMessage = m_lastError;
        m_values.clear();
        m_dirty = false;
        return true;
    }

    warningMessage.clear();
    return true;
}

void ModuleSaveStore::reset()
{
    m_path.clear();
    m_values.clear();
    m_lastError.clear();
    m_dirty = false;
}

bool ModuleSaveStore::has(const std::string& key) const
{
    return isValidKey(key) && m_values.contains(key);
}

bool ModuleSaveStore::remove(const std::string& key)
{
    if (!isValidKey(key))
    {
        m_lastError = "Invalid save key: '" + key + "'.";
        return false;
    }

    const std::size_t erased = m_values.erase(key);
    if (erased > 0)
        m_dirty = true;
    m_lastError.clear();
    return erased > 0;
}

void ModuleSaveStore::clear()
{
    if (!m_values.empty())
    {
        m_values.clear();
        m_dirty = true;
    }
    m_lastError.clear();
}

std::string ModuleSaveStore::getString(
    const std::string& key,
    const std::string& fallback) const
{
    const auto found = m_values.find(key);
    return found != m_values.end() && found->second.type == ValueType::String
        ? found->second.text
        : fallback;
}

std::int64_t ModuleSaveStore::getInteger(
    const std::string& key,
    std::int64_t fallback) const
{
    const auto found = m_values.find(key);
    if (found == m_values.end() || found->second.type != ValueType::Integer)
        return fallback;

    std::int64_t value = fallback;
    return parseInteger(found->second.text, value) ? value : fallback;
}

double ModuleSaveStore::getNumber(
    const std::string& key,
    double fallback) const
{
    const auto found = m_values.find(key);
    if (found == m_values.end())
        return fallback;

    if (found->second.type == ValueType::Integer)
    {
        std::int64_t integer = 0;
        return parseInteger(found->second.text, integer)
            ? static_cast<double>(integer)
            : fallback;
    }

    if (found->second.type != ValueType::Number)
        return fallback;

    double value = fallback;
    return parseNumber(found->second.text, value) ? value : fallback;
}

bool ModuleSaveStore::getBoolean(
    const std::string& key,
    bool fallback) const
{
    const auto found = m_values.find(key);
    if (found == m_values.end() || found->second.type != ValueType::Boolean)
        return fallback;

    if (found->second.text == "1" || found->second.text == "true")
        return true;
    if (found->second.text == "0" || found->second.text == "false")
        return false;
    return fallback;
}

bool ModuleSaveStore::setString(
    const std::string& key,
    const std::string& value)
{
    return setValue(key, ValueType::String, value);
}

bool ModuleSaveStore::setInteger(
    const std::string& key,
    std::int64_t value)
{
    return setValue(key, ValueType::Integer, std::to_string(value));
}

bool ModuleSaveStore::setNumber(
    const std::string& key,
    double value)
{
    if (!std::isfinite(value))
    {
        m_lastError = "Save.SetNumber rejected a non-finite value for key '"
            + key + "'.";
        return false;
    }

    std::ostringstream text;
    text << std::setprecision(17) << value;
    return setValue(key, ValueType::Number, text.str());
}

bool ModuleSaveStore::setBoolean(
    const std::string& key,
    bool value)
{
    return setValue(key, ValueType::Boolean, value ? "1" : "0");
}

bool ModuleSaveStore::flush()
{
    if (m_path.empty())
    {
        m_lastError = "Module save store has not been initialized.";
        return false;
    }

    if (!m_dirty && std::filesystem::is_regular_file(m_path))
    {
        m_lastError.clear();
        return true;
    }

    std::error_code directoryError;
    std::filesystem::create_directories(m_path.parent_path(), directoryError);
    if (directoryError)
    {
        m_lastError = "Could not create module save directory: "
            + directoryError.message();
        return false;
    }

    const std::filesystem::path temporary = m_path.string() + ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        m_lastError = "Could not open temporary module save file: "
            + temporary.string();
        return false;
    }

    file << kHeader << '\n';
    for (const auto& [key, value] : m_values)
    {
        file << typeCode(value.type) << '\t'
             << escape(key) << '\t'
             << escape(value.text) << '\n';
    }

    file.flush();
    if (!file)
    {
        m_lastError = "Writing module save file failed: " + temporary.string();
        file.close();
        std::error_code removeError;
        std::filesystem::remove(temporary, removeError);
        return false;
    }
    file.close();

    std::string replaceError;
    if (!replaceFile(temporary, m_path, replaceError))
    {
        m_lastError = replaceError;
        std::error_code removeError;
        std::filesystem::remove(temporary, removeError);
        return false;
    }

    m_dirty = false;
    m_lastError.clear();
    return true;
}

bool ModuleSaveStore::load()
{
    m_values.clear();
    m_dirty = false;

    if (!std::filesystem::exists(m_path))
    {
        m_lastError.clear();
        return true;
    }

    std::ifstream file(m_path, std::ios::binary);
    if (!file)
    {
        m_lastError = "Could not open module save file: " + m_path.string();
        return false;
    }

    std::string line;
    if (!std::getline(file, line) || line != kHeader)
    {
        m_lastError = "Module save file has an invalid or unsupported header: "
            + m_path.string();
        return false;
    }

    std::size_t lineNumber = 1;
    while (std::getline(file, line))
    {
        ++lineNumber;
        if (line.empty())
            continue;

        const std::size_t firstTab = line.find('\t');
        const std::size_t secondTab = firstTab == std::string::npos
            ? std::string::npos
            : line.find('\t', firstTab + 1);
        if (firstTab != 1 || secondTab == std::string::npos)
        {
            m_lastError = "Malformed module save record at line "
                + std::to_string(lineNumber) + ".";
            return false;
        }

        ValueType type;
        if (!typeFromCode(line.front(), type))
        {
            m_lastError = "Unknown module save value type at line "
                + std::to_string(lineNumber) + ".";
            return false;
        }

        std::string key;
        std::string text;
        if (!unescape(line.substr(firstTab + 1, secondTab - firstTab - 1), key)
            || !unescape(line.substr(secondTab + 1), text)
            || !isValidKey(key))
        {
            m_lastError = "Invalid module save key or escape sequence at line "
                + std::to_string(lineNumber) + ".";
            return false;
        }

        if (type == ValueType::Integer)
        {
            std::int64_t ignored = 0;
            if (!parseInteger(text, ignored))
            {
                m_lastError = "Invalid integer in module save at line "
                    + std::to_string(lineNumber) + ".";
                return false;
            }
        }
        else if (type == ValueType::Number)
        {
            double ignored = 0.0;
            if (!parseNumber(text, ignored))
            {
                m_lastError = "Invalid number in module save at line "
                    + std::to_string(lineNumber) + ".";
                return false;
            }
        }
        else if (type == ValueType::Boolean
            && text != "0" && text != "1"
            && text != "false" && text != "true")
        {
            m_lastError = "Invalid boolean in module save at line "
                + std::to_string(lineNumber) + ".";
            return false;
        }

        m_values[key] = Value{ type, text };
    }

    m_lastError.clear();
    return true;
}

bool ModuleSaveStore::setValue(
    const std::string& key,
    ValueType type,
    const std::string& text)
{
    if (!isValidKey(key))
    {
        m_lastError = "Invalid save key: '" + key
            + "'. Use 1-256 letters, numbers, '.', '_', '-', ':' or '/'.";
        return false;
    }

    constexpr std::size_t kMaximumValueBytes = 1024 * 1024;
    if (text.size() > kMaximumValueBytes)
    {
        m_lastError = "Save value for key '" + key
            + "' exceeds the 1 MiB primitive-value limit.";
        return false;
    }

    const auto found = m_values.find(key);
    if (found != m_values.end()
        && found->second.type == type
        && found->second.text == text)
    {
        m_lastError.clear();
        return true;
    }

    m_values[key] = Value{ type, text };
    m_dirty = true;
    m_lastError.clear();
    return true;
}

bool ModuleSaveStore::isValidKey(const std::string& key)
{
    if (key.empty() || key.size() > 256)
        return false;

    for (const unsigned char character : key)
    {
        const bool alphaNumeric =
            (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9');
        const bool punctuation = character == '.' || character == '_'
            || character == '-' || character == ':' || character == '/';
        if (!alphaNumeric && !punctuation)
            return false;
    }
    return true;
}

char ModuleSaveStore::typeCode(ValueType type)
{
    switch (type)
    {
    case ValueType::String:  return 'S';
    case ValueType::Integer: return 'I';
    case ValueType::Number:  return 'N';
    case ValueType::Boolean: return 'B';
    }
    return 'S';
}

bool ModuleSaveStore::typeFromCode(char code, ValueType& type)
{
    switch (code)
    {
    case 'S': type = ValueType::String; return true;
    case 'I': type = ValueType::Integer; return true;
    case 'N': type = ValueType::Number; return true;
    case 'B': type = ValueType::Boolean; return true;
    default: return false;
    }
}

std::string ModuleSaveStore::escape(const std::string& value)
{
    std::string output;
    output.reserve(value.size());
    for (const char character : value)
    {
        switch (character)
        {
        case '\\': output += "\\\\"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default: output.push_back(character); break;
        }
    }
    return output;
}

bool ModuleSaveStore::unescape(
    const std::string& value,
    std::string& output)
{
    output.clear();
    output.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const char character = value[index];
        if (character != '\\')
        {
            output.push_back(character);
            continue;
        }

        if (++index >= value.size())
            return false;

        switch (value[index])
        {
        case '\\': output.push_back('\\'); break;
        case 'n': output.push_back('\n'); break;
        case 'r': output.push_back('\r'); break;
        case 't': output.push_back('\t'); break;
        default: return false;
        }
    }
    return true;
}

} // namespace heritage::modules
