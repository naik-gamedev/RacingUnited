#include "GltfInternal.hpp"

namespace heritage::graphics::gltf_internal {

class JsonParser
{
public:
    explicit JsonParser(std::string_view text)
        : m_text(text)
    {
    }

    bool parse(JsonValue& value, std::string& error)
    {
        skipWhitespace();
        if (!parseValue(value, error))
            return false;
        skipWhitespace();
        if (m_position != m_text.size())
        {
            error = "Unexpected trailing JSON data.";
            return false;
        }
        return true;
    }

private:
    bool parseValue(JsonValue& value, std::string& error)
    {
        skipWhitespace();
        if (m_position >= m_text.size())
        {
            error = "Unexpected end of JSON input.";
            return false;
        }

        const char c = m_text[m_position];
        if (c == '{')
            return parseObject(value, error);
        if (c == '[')
            return parseArray(value, error);
        if (c == '"')
        {
            value.type = JsonValue::Type::String;
            return parseString(value.stringValue, error);
        }
        if (c == 't' || c == 'f')
            return parseBool(value, error);
        if (c == 'n')
            return parseNull(value, error);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0)
            return parseNumber(value, error);

        error = "Unexpected JSON token.";
        return false;
    }

    bool parseObject(JsonValue& value, std::string& error)
    {
        value = {};
        value.type = JsonValue::Type::Object;
        ++m_position; // {
        skipWhitespace();
        if (consume('}'))
            return true;

        while (m_position < m_text.size())
        {
            std::string key;
            if (!parseString(key, error))
                return false;
            skipWhitespace();
            if (!consume(':'))
            {
                error = "Expected ':' in JSON object.";
                return false;
            }

            JsonValue child;
            if (!parseValue(child, error))
                return false;
            value.objectValue.emplace(std::move(key), std::move(child));

            skipWhitespace();
            if (consume('}'))
                return true;
            if (!consume(','))
            {
                error = "Expected ',' or '}' in JSON object.";
                return false;
            }
            skipWhitespace();
        }

        error = "Unterminated JSON object.";
        return false;
    }

    bool parseArray(JsonValue& value, std::string& error)
    {
        value = {};
        value.type = JsonValue::Type::Array;
        ++m_position; // [
        skipWhitespace();
        if (consume(']'))
            return true;

        while (m_position < m_text.size())
        {
            JsonValue child;
            if (!parseValue(child, error))
                return false;
            value.arrayValue.push_back(std::move(child));

            skipWhitespace();
            if (consume(']'))
                return true;
            if (!consume(','))
            {
                error = "Expected ',' or ']' in JSON array.";
                return false;
            }
            skipWhitespace();
        }

        error = "Unterminated JSON array.";
        return false;
    }

    bool parseString(std::string& value, std::string& error)
    {
        value.clear();
        if (!consume('"'))
        {
            error = "Expected JSON string.";
            return false;
        }

        while (m_position < m_text.size())
        {
            const char c = m_text[m_position++];
            if (c == '"')
                return true;
            if (c == '\\')
            {
                if (m_position >= m_text.size())
                {
                    error = "Invalid JSON string escape.";
                    return false;
                }
                const char escaped = m_text[m_position++];
                switch (escaped)
                {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case 'u':
                    if (m_position + 4 > m_text.size())
                    {
                        error = "Invalid JSON unicode escape.";
                        return false;
                    }
                    else
                    {
                        unsigned int codepoint = 0;
                        for (int i = 0; i < 4; ++i)
                        {
                            const char hex = m_text[m_position++];
                            codepoint <<= 4;
                            if (hex >= '0' && hex <= '9')
                                codepoint |= static_cast<unsigned int>(hex - '0');
                            else if (hex >= 'a' && hex <= 'f')
                                codepoint |= static_cast<unsigned int>(hex - 'a' + 10);
                            else if (hex >= 'A' && hex <= 'F')
                                codepoint |= static_cast<unsigned int>(hex - 'A' + 10);
                            else
                            {
                                error = "Invalid JSON unicode escape.";
                                return false;
                            }
                        }

                        if (codepoint <= 0x7Fu)
                        {
                            value.push_back(static_cast<char>(codepoint));
                        }
                        else if (codepoint <= 0x7FFu)
                        {
                            value.push_back(static_cast<char>(0xC0u | ((codepoint >> 6) & 0x1Fu)));
                            value.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
                        }
                        else
                        {
                            value.push_back(static_cast<char>(0xE0u | ((codepoint >> 12) & 0x0Fu)));
                            value.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
                            value.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
                        }
                    }
                    break;
                default:
                    error = "Unsupported JSON string escape.";
                    return false;
                }
                continue;
            }
            value.push_back(c);
        }

        error = "Unterminated JSON string.";
        return false;
    }

    bool parseBool(JsonValue& value, std::string& error)
    {
        value = {};
        value.type = JsonValue::Type::Bool;
        if (matchLiteral("true"))
        {
            value.boolValue = true;
            return true;
        }
        if (matchLiteral("false"))
        {
            value.boolValue = false;
            return true;
        }
        error = "Invalid JSON boolean literal.";
        return false;
    }

    bool parseNull(JsonValue& value, std::string& error)
    {
        if (!matchLiteral("null"))
        {
            error = "Invalid JSON null literal.";
            return false;
        }
        value = {};
        value.type = JsonValue::Type::Null;
        return true;
    }

    bool parseNumber(JsonValue& value, std::string& error)
    {
        const std::size_t start = m_position;
        if (m_text[m_position] == '-')
            ++m_position;

        if (m_position >= m_text.size())
        {
            error = "Invalid JSON number.";
            return false;
        }

        if (m_text[m_position] == '0')
        {
            ++m_position;
        }
        else if (std::isdigit(static_cast<unsigned char>(m_text[m_position])) != 0)
        {
            while (m_position < m_text.size()
                && std::isdigit(static_cast<unsigned char>(m_text[m_position])) != 0)
            {
                ++m_position;
            }
        }
        else
        {
            error = "Invalid JSON number.";
            return false;
        }

        if (m_position < m_text.size() && m_text[m_position] == '.')
        {
            ++m_position;
            if (m_position >= m_text.size()
                || std::isdigit(static_cast<unsigned char>(m_text[m_position])) == 0)
            {
                error = "Invalid JSON fractional number.";
                return false;
            }
            while (m_position < m_text.size()
                && std::isdigit(static_cast<unsigned char>(m_text[m_position])) != 0)
            {
                ++m_position;
            }
        }

        if (m_position < m_text.size()
            && (m_text[m_position] == 'e' || m_text[m_position] == 'E'))
        {
            ++m_position;
            if (m_position < m_text.size()
                && (m_text[m_position] == '+' || m_text[m_position] == '-'))
            {
                ++m_position;
            }
            if (m_position >= m_text.size()
                || std::isdigit(static_cast<unsigned char>(m_text[m_position])) == 0)
            {
                error = "Invalid JSON exponent.";
                return false;
            }
            while (m_position < m_text.size()
                && std::isdigit(static_cast<unsigned char>(m_text[m_position])) != 0)
            {
                ++m_position;
            }
        }

        value = {};
        value.type = JsonValue::Type::Number;
        try
        {
            value.numberValue = std::stod(std::string(m_text.substr(start, m_position - start)));
        }
        catch (...)
        {
            error = "Could not parse JSON number.";
            return false;
        }
        return true;
    }

    void skipWhitespace()
    {
        while (m_position < m_text.size()
            && std::isspace(static_cast<unsigned char>(m_text[m_position])) != 0)
        {
            ++m_position;
        }
    }

    bool consume(char expected)
    {
        if (m_position < m_text.size() && m_text[m_position] == expected)
        {
            ++m_position;
            return true;
        }
        return false;
    }

    bool matchLiteral(const char* literal)
    {
        const std::size_t length = std::char_traits<char>::length(literal);
        if (m_position + length > m_text.size())
            return false;
        if (m_text.substr(m_position, length) == literal)
        {
            m_position += length;
            return true;
        }
        return false;
    }

    std::string_view m_text;
    std::size_t m_position = 0;
};


bool parseJsonDocument(std::string_view text, JsonValue& value, std::string& error)
{
    JsonParser parser(text);
    return parser.parse(value, error);
}

} // namespace heritage::graphics::gltf_internal
