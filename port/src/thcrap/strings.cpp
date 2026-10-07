// Hardcoded string translation: stringlocs.js maps addresses of strings in
// the original th16.exe to string ids, stringdefs.js ids to translations.
// thcrap looks strings up by address (strings_lookup on the pointer the
// game passes). The port's strings are its own literals, so the lookup is
// by content: th16_strings.inc (generated from the game sources and the
// original executable, port/tools/thcrap_strings.py) gives the text at each
// original address, and a string the game uses translates when its text
// is the text of a stringlocs address. The game's strings match the
// original's byte for byte (it is a matching decompilation).
//
// Adapted from thcrap (public domain): thcrap/src/strings.cpp
// (stringlocs_reparse, strings_lookup, strings_va_lookup, strings_vsprintf).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <unordered_map>

#include "port_thcrap.h"
#include "thcrap_internal.h"

namespace thcrap
{
namespace
{

struct OriginalString
{
    uint32_t address;
    const char *text;
};

// The string literals of src/ with their addresses in the original
// th16.exe (.rdata).
const OriginalString ORIGINAL_STRINGS[] = {
#ifdef TH16_THCRAP_GENERATED_STRINGS
// Made by the build from the sources and orig/th16.exe.
#include "th16_strings_generated.inc"
#else
#include "th16_strings.inc"
#endif
};

const uint32_t IMAGE_BASE = 0x400000;

// Text of a stringlocs string -> its id.
std::unordered_map<std::string, std::string> g_string_ids;

// thcrap's eval_expr for stringlocs keys: "Rx<hex>" (relative to the
// image base), "0x<hex>" or "<hex>".
bool parse_address(const char *key, uint32_t *address)
{
    char *end;
    if ((key[0] == 'R' || key[0] == 'r') && (key[1] == 'x' || key[1] == 'X'))
    {
        *address = IMAGE_BASE + (uint32_t)strtoul(key + 2, &end, 16);
    }
    else
    {
        *address = (uint32_t)strtoul(key, &end, 16);
    }
    return end != key && *end == '\0';
}

const char *original_string(uint32_t address)
{
    size_t lo = 0, hi = sizeof(ORIGINAL_STRINGS) / sizeof(*ORIGINAL_STRINGS);
    while (lo < hi)
    {
        size_t mid = (lo + hi) / 2;
        if (ORIGINAL_STRINGS[mid].address < address)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    return lo < sizeof(ORIGINAL_STRINGS) / sizeof(*ORIGINAL_STRINGS) && ORIGINAL_STRINGS[lo].address == address
               ? ORIGINAL_STRINGS[lo].text
               : NULL;
}

// One conversion of a printf format, with its argument; %s arguments are
// looked up (strings_va_lookup), %n is dropped.
void format_one(std::string *out, const std::string &spec, char conversion, int length_kind, va_list *args)
{
    char buf[512];
    int n = 0;
    switch (conversion)
    {
    case 'd':
    case 'i':
    case 'u':
    case 'x':
    case 'X':
    case 'o':
        if (length_kind == 2)
        {
            n = snprintf(buf, sizeof(buf), spec.c_str(), va_arg(*args, long long));
        }
        else
        {
            n = snprintf(buf, sizeof(buf), spec.c_str(), va_arg(*args, int));
        }
        break;
    case 'c':
        n = snprintf(buf, sizeof(buf), spec.c_str(), va_arg(*args, int));
        break;
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
        n = snprintf(buf, sizeof(buf), spec.c_str(), va_arg(*args, double));
        break;
    case 'p':
        n = snprintf(buf, sizeof(buf), spec.c_str(), va_arg(*args, void *));
        break;
    case 's':
    {
        const char *s = va_arg(*args, const char *);
        s = s != NULL ? strings_lookup(s) : "(null)";
        int len = snprintf(NULL, 0, spec.c_str(), s);
        if (len > 0)
        {
            std::string piece(len + 1, '\0');
            snprintf(&piece[0], piece.size(), spec.c_str(), s);
            piece.resize(len);
            *out += piece;
        }
        return;
    }
    case 'n':
        (void)va_arg(*args, int *);
        return;
    default:
        *out += spec;
        return;
    }
    if (n > 0)
    {
        out->append(buf, n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1);
    }
}

} // namespace

void strings_init()
{
    json_t *stringlocs = stack_game_json_resolve("stringlocs.js", NULL);
    const char *key;
    json_t *value;
    size_t count = 0;
    json_object_foreach(stringlocs, key, value)
    {
        uint32_t address;
        if (!json_is_string(value) || !parse_address(key, &address))
        {
            continue;
        }
        const char *text = original_string(address);
        if (text == NULL)
        {
            log("stringlocs: no string of the game sources is at %s (%s)", key, json_string_value(value));
            continue;
        }
        g_string_ids[text] = json_string_value(value);
        count++;
    }
    json_decref(stringlocs);
    log("%zu hardcoded strings can be translated", count);
}

const json_t *strings_get(const char *id)
{
    return json_object_get(jsondata_get("stringdefs.js"), id);
}

const char *strings_lookup(const char *in)
{
    if (in == NULL || g_string_ids.empty())
    {
        return in;
    }
    auto found = g_string_ids.find(in);
    if (found == g_string_ids.end())
    {
        return in;
    }
    const char *translation = json_string_value(strings_get(found->second.c_str()));
    return translation != NULL ? translation : in;
}

std::string strings_vsprintf(const char *format, va_list args_in)
{
    format = strings_lookup(format);
    std::string out;
    va_list args;
    va_copy(args, args_in);
    for (const char *p = format; *p != '\0';)
    {
        if (*p != '%')
        {
            const char *next = strchr(p, '%');
            size_t n = next != NULL ? (size_t)(next - p) : strlen(p);
            out.append(p, n);
            p += n;
            continue;
        }
        if (p[1] == '%')
        {
            out += '%';
            p += 2;
            continue;
        }
        // %[flags][width][.precision][length]conversion
        std::string spec = "%";
        const char *q = p + 1;
        while (*q != '\0' && strchr("-+ #0", *q) != NULL)
        {
            spec += *q++;
        }
        if (*q == '*')
        {
            spec += std::to_string(va_arg(args, int));
            q++;
        }
        while (*q >= '0' && *q <= '9')
        {
            spec += *q++;
        }
        if (*q == '.')
        {
            spec += *q++;
            if (*q == '*')
            {
                spec += std::to_string(va_arg(args, int));
                q++;
            }
            while (*q >= '0' && *q <= '9')
            {
                spec += *q++;
            }
        }
        // Length: h, hh, l, ll, L, z, t, j, I32, I64 (MSVC).
        int length_kind = 0;
        if (q[0] == 'I' && q[1] == '6' && q[2] == '4')
        {
            length_kind = 2;
            q += 3;
        }
        else if (q[0] == 'I' && q[1] == '3' && q[2] == '2')
        {
            q += 3;
        }
        else
        {
            while (*q != '\0' && strchr("hlLzjt", *q) != NULL)
            {
                if (*q == 'l')
                {
                    length_kind++;
                }
                else if (*q == 'z' || *q == 't' || *q == 'j')
                {
                    length_kind = sizeof(size_t) == 8 ? 2 : 0;
                }
                q++;
            }
        }
        if (*q == '\0')
        {
            break;
        }
        char conversion = *q++;
        if (length_kind == 2)
        {
            spec += "ll";
        }
        else if (length_kind == 1)
        {
            // MSVC's long is 32 bits: the game passes an i32 for %ld.
            length_kind = 0;
        }
        spec += conversion;
        format_one(&out, spec, conversion, length_kind, &args);
        p = q;
    }
    va_end(args);
    return out;
}

} // namespace thcrap

using namespace thcrap;

int port_thcrap_vsnprintf(char *buf, size_t size, const char *format, va_list args)
{
    if (!port_thcrap_active())
    {
        return vsnprintf(buf, size, format, args);
    }
    std::string text = strings_vsprintf(format, args);
    if (size > 0)
    {
        size_t n = text.size() < size - 1 ? text.size() : size - 1;
        // Cut at a whole UTF-8 character.
        while (n < text.size() && n > 0 && ((uint8_t)text[n] & 0xc0) == 0x80)
        {
            n--;
        }
        memcpy(buf, text.data(), n);
        buf[n] = '\0';
    }
    return (int)text.size();
}

int port_thcrap_snprintf(char *buf, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = port_thcrap_vsnprintf(buf, size, format, args);
    va_end(args);
    return result;
}

const char *port_thcrap_string(const char *text)
{
    return port_thcrap_active() ? strings_lookup(text) : text;
}
