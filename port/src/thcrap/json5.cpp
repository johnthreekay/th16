// JSON5 for jansson. thcrap reads every patch file with a JSON5 parser
// (json5pp: comments, trailing commas, single-quoted strings, unquoted
// keys, hexadecimal and signed numbers) and hands the result to jansson;
// patch files rely on this (th16.v1.00a.js has trailing commas). Here the
// JSON5 text is rewritten into strict JSON, which jansson then parses.
// Written for the port; thcrap's own json5_loadb (thcrap/src/jansson_ex.cpp)
// does the same through json5pp.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "thcrap_internal.h"

namespace thcrap
{
namespace
{

struct Json5Converter
{
    const char *p;
    const char *end;
    std::string out;
    std::string error;

    bool fail(const char *message)
    {
        if (error.empty())
        {
            error = message;
        }
        return false;
    }

    // Skips whitespace and comments.
    bool skip_space()
    {
        while (p < end)
        {
            unsigned char c = *p;
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f')
            {
                p++;
            }
            else if (c == 0xc2 && p + 1 < end && (unsigned char)p[1] == 0xa0)
            {
                p += 2; // U+00A0
            }
            else if (c == 0xef && p + 2 < end && (unsigned char)p[1] == 0xbb && (unsigned char)p[2] == 0xbf)
            {
                p += 3; // a BOM
            }
            else if (c == 0xe2 && p + 2 < end && (unsigned char)p[1] == 0x80 &&
                     ((unsigned char)p[2] == 0xa8 || (unsigned char)p[2] == 0xa9))
            {
                p += 3; // U+2028, U+2029
            }
            else if (c == '/' && p + 1 < end && p[1] == '/')
            {
                while (p < end && *p != '\n')
                {
                    p++;
                }
            }
            else if (c == '/' && p + 1 < end && p[1] == '*')
            {
                const char *close = p + 2;
                while (close + 1 < end && !(close[0] == '*' && close[1] == '/'))
                {
                    close++;
                }
                if (close + 1 >= end)
                {
                    return fail("unterminated comment");
                }
                p = close + 2;
            }
            else
            {
                break;
            }
        }
        return true;
    }

    void put_utf8(uint32_t c)
    {
        if (c < 0x80)
        {
            out += (char)c;
        }
        else if (c < 0x800)
        {
            out += (char)(0xc0 | (c >> 6));
            out += (char)(0x80 | (c & 0x3f));
        }
        else if (c < 0x10000)
        {
            out += (char)(0xe0 | (c >> 12));
            out += (char)(0x80 | ((c >> 6) & 0x3f));
            out += (char)(0x80 | (c & 0x3f));
        }
        else
        {
            out += (char)(0xf0 | (c >> 18));
            out += (char)(0x80 | ((c >> 12) & 0x3f));
            out += (char)(0x80 | ((c >> 6) & 0x3f));
            out += (char)(0x80 | (c & 0x3f));
        }
    }

    // A string in either quote; written out double-quoted.
    bool string()
    {
        char quote = *p++;
        out += '"';
        while (p < end && *p != quote)
        {
            unsigned char c = *p;
            if (c == '\\')
            {
                p++;
                if (p >= end)
                {
                    break;
                }
                char e = *p++;
                switch (e)
                {
                case '\'':
                    out += '\'';
                    break;
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                case '/':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                    out += '\\';
                    out += e;
                    break;
                case 'v':
                    out += "\\u000b";
                    break;
                case '0':
                    out += "\\u0000";
                    break;
                case 'u':
                    out += "\\u";
                    break;
                case 'x':
                {
                    if (end - p < 2)
                    {
                        return fail("bad \\x escape");
                    }
                    char hex[3] = {p[0], p[1], 0};
                    put_utf8((uint32_t)strtoul(hex, NULL, 16));
                    p += 2;
                    break;
                }
                case '\r':
                    if (p < end && *p == '\n')
                    {
                        p++;
                    }
                    break;
                case '\n':
                    break; // line continuation
                default:
                    if ((unsigned char)e == 0xe2 && end - p >= 2 && (unsigned char)p[0] == 0x80 &&
                        ((unsigned char)p[1] == 0xa8 || (unsigned char)p[1] == 0xa9))
                    {
                        p += 2; // continuation over U+2028/U+2029
                        break;
                    }
                    // Any other escaped character stands for itself.
                    if (e == '"' || e == '\\')
                    {
                        out += '\\';
                    }
                    out += e;
                    break;
                }
            }
            else if (c < 0x20)
            {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
                p++;
            }
            else if (c == '"')
            {
                out += "\\\"";
                p++;
            }
            else
            {
                out += (char)c;
                p++;
            }
        }
        if (p >= end)
        {
            return fail("unterminated string");
        }
        p++;
        out += '"';
        return true;
    }

    static bool ident_start(unsigned char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$' || c >= 0x80;
    }
    static bool ident_char(unsigned char c)
    {
        return ident_start(c) || (c >= '0' && c <= '9');
    }

    bool number()
    {
        std::string text;
        bool negative = false;
        if (*p == '+' || *p == '-')
        {
            negative = *p == '-';
            p++;
        }
        if (end - p >= 8 && strncmp(p, "Infinity", 8) == 0)
        {
            // JSON has no infinity; the largest double stands in.
            p += 8;
            out += negative ? "-1.7976931348623157e308" : "1.7976931348623157e308";
            return true;
        }
        if (end - p >= 3 && strncmp(p, "NaN", 3) == 0)
        {
            p += 3;
            out += "0";
            return true;
        }
        if (end - p >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        {
            p += 2;
            const char *start = p;
            while (p < end && isxdigit((unsigned char)*p))
            {
                p++;
            }
            if (p == start)
            {
                return fail("bad hexadecimal number");
            }
            unsigned long long value = strtoull(std::string(start, p).c_str(), NULL, 16);
            char buf[32];
            snprintf(buf, sizeof(buf), "%s%llu", negative ? "-" : "", value);
            out += buf;
            return true;
        }
        const char *start = p;
        while (p < end && (isdigit((unsigned char)*p) || *p == '.' || *p == 'e' || *p == 'E' ||
                           ((*p == '+' || *p == '-') && (p[-1] == 'e' || p[-1] == 'E'))))
        {
            p++;
        }
        text.assign(start, p);
        if (text.empty())
        {
            return fail("bad number");
        }
        // JSON wants digits on both sides of the point and no leading zeros.
        if (text[0] == '.')
        {
            text = "0" + text;
        }
        size_t dot = text.find('.');
        if (dot != std::string::npos && (dot + 1 == text.size() || !isdigit((unsigned char)text[dot + 1])))
        {
            text.insert(dot + 1, "0");
        }
        size_t digits = 0;
        while (digits + 1 < text.size() && text[digits] == '0' && isdigit((unsigned char)text[digits + 1]))
        {
            digits++;
        }
        text.erase(0, digits);
        if (negative)
        {
            out += '-';
        }
        out += text;
        return true;
    }

    bool value(int depth)
    {
        if (depth > 512)
        {
            return fail("nesting too deep");
        }
        if (!skip_space())
        {
            return false;
        }
        if (p >= end)
        {
            return fail("unexpected end");
        }
        char c = *p;
        if (c == '{')
        {
            p++;
            out += '{';
            bool first = true;
            for (;;)
            {
                if (!skip_space())
                {
                    return false;
                }
                if (p < end && *p == '}')
                {
                    p++;
                    break;
                }
                if (!first)
                {
                    out += ',';
                }
                first = false;
                if (p >= end)
                {
                    return fail("unterminated object");
                }
                if (*p == '"' || *p == '\'')
                {
                    if (!string())
                    {
                        return false;
                    }
                }
                else if (ident_start((unsigned char)*p) || isdigit((unsigned char)*p))
                {
                    const char *start = p;
                    while (p < end && ident_char((unsigned char)*p))
                    {
                        p++;
                    }
                    out += '"';
                    out.append(start, p);
                    out += '"';
                }
                else
                {
                    return fail("bad object key");
                }
                if (!skip_space())
                {
                    return false;
                }
                if (p >= end || *p != ':')
                {
                    return fail("missing ':'");
                }
                p++;
                out += ':';
                if (!value(depth + 1))
                {
                    return false;
                }
                if (!skip_space())
                {
                    return false;
                }
                if (p < end && *p == ',')
                {
                    p++;
                }
                else if (p < end && *p == '}')
                {
                    p++;
                    break;
                }
                else
                {
                    return fail("missing ',' or '}'");
                }
            }
            out += '}';
            return true;
        }
        if (c == '[')
        {
            p++;
            out += '[';
            bool first = true;
            for (;;)
            {
                if (!skip_space())
                {
                    return false;
                }
                if (p < end && *p == ']')
                {
                    p++;
                    break;
                }
                if (!first)
                {
                    out += ',';
                }
                first = false;
                if (!value(depth + 1))
                {
                    return false;
                }
                if (!skip_space())
                {
                    return false;
                }
                if (p < end && *p == ',')
                {
                    p++;
                }
                else if (p < end && *p == ']')
                {
                    p++;
                    break;
                }
                else
                {
                    return fail("missing ',' or ']'");
                }
            }
            out += ']';
            return true;
        }
        if (c == '"' || c == '\'')
        {
            return string();
        }
        if (c == '+' || c == '-' || c == '.' || isdigit((unsigned char)c) || (c == 'I' || c == 'N'))
        {
            if ((c == 'I' && (end - p < 8 || strncmp(p, "Infinity", 8) != 0)) ||
                (c == 'N' && (end - p < 3 || strncmp(p, "NaN", 3) != 0)))
            {
                return fail("bad value");
            }
            return number();
        }
        static const char *const words[] = {"true", "false", "null"};
        for (const char *word : words)
        {
            size_t n = strlen(word);
            if ((size_t)(end - p) >= n && strncmp(p, word, n) == 0 && (p + n == end || !ident_char(p[n])))
            {
                p += n;
                out += word;
                return true;
            }
        }
        return fail("bad value");
    }
};

} // namespace

json_t *json5_loadb(const char *text, size_t length, std::string *error)
{
    Json5Converter conv;
    conv.p = text;
    conv.end = text + length;
    if (!conv.value(0) || !conv.skip_space() || conv.p != conv.end)
    {
        if (error != NULL)
        {
            char where[64];
            snprintf(where, sizeof(where), " (at byte %zu)", (size_t)(conv.p - text));
            *error = (conv.error.empty() ? std::string("garbage after the value") : conv.error) + where;
        }
        return NULL;
    }
    json_error_t json_error;
    json_t *result = json_loadb(conv.out.data(), conv.out.size(), JSON_DECODE_ANY | JSON_ALLOW_NUL, &json_error);
    if (result == NULL && error != NULL)
    {
        *error = json_error.text;
    }
    return result;
}

json_t *json5_load_file(const std::string &path, size_t *size)
{
    if (size != NULL)
    {
        *size = 0;
    }
    std::string data;
    if (!read_host_file(path, &data))
    {
        return NULL;
    }
    // UTF-16LE (with a BOM, or ASCII-looking with zero high bytes).
    if (data.size() >= 2 && (((uint8_t)data[0] == 0xff && (uint8_t)data[1] == 0xfe) || data[1] == '\0'))
    {
        size_t start = (uint8_t)data[0] == 0xff ? 2 : 0;
        std::string utf8;
        for (size_t i = start; i + 1 < data.size(); i += 2)
        {
            uint32_t c = (uint8_t)data[i] | ((uint8_t)data[i + 1] << 8);
            if (c >= 0xd800 && c < 0xdc00 && i + 3 < data.size())
            {
                uint32_t low = (uint8_t)data[i + 2] | ((uint8_t)data[i + 3] << 8);
                c = 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
                i += 2;
            }
            Json5Converter conv;
            conv.put_utf8(c);
            utf8 += conv.out;
        }
        data = utf8;
    }
    std::string error;
    json_t *json = json5_loadb(data.data(), data.size(), &error);
    if (json == NULL)
    {
        log("JSON error in %s: %s", path.c_str(), error.c_str());
        return NULL;
    }
    if (size != NULL)
    {
        *size = data.size();
    }
    return json;
}

} // namespace thcrap
