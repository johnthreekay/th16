// Shift-JIS (code page 932) <-> UTF-8 with iconv, MultiByteToWideChar and
// WideCharToMultiByte, and the port's log. The game keeps every string as
// Shift-JIS bytes; these convert where text leaves the game (fonts, file
// names on the host, message boxes, the log).
#include <errno.h>
#include <iconv.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <mutex>
#include <string>

#include <windows.h>

#include "port_platform.h"

namespace
{

// One converter per direction, shared under a lock (iconv_t is not
// thread-safe, and the conversions are short).
struct Converter
{
    std::mutex lock;
    iconv_t cd = (iconv_t)-1;
    bool tried = false;

    iconv_t get(const char *to, const char *from)
    {
        if (!tried)
        {
            tried = true;
            // glibc calls it CP932; libiconv (macOS) knows both CP932 and
            // SHIFT_JIS, and plain Shift-JIS lacks the NEC/IBM extensions.
            cd = iconv_open(to, from);
            if (cd == (iconv_t)-1 && strcmp(from, "CP932") == 0)
            {
                cd = iconv_open(to, "SHIFT_JIS");
            }
            if (cd == (iconv_t)-1 && strcmp(to, "CP932") == 0)
            {
                cd = iconv_open("SHIFT_JIS", from);
            }
            if (cd == (iconv_t)-1)
            {
                fprintf(stderr, "[th16-port] iconv cannot convert %s to %s\n", from, to);
            }
        }
        return cd;
    }
};

Converter g_sjis_to_utf8;
Converter g_utf8_to_sjis;

// Converts with iconv, replacing what does not convert by `replacement`
// (one input byte at a time). Returns false if anything was replaced.
bool convert(Converter &converter, const char *to, const char *from, const char *text, size_t length,
             std::string *out, const char *replacement)
{
    std::lock_guard<std::mutex> guard(converter.lock);
    iconv_t cd = converter.get(to, from);
    out->clear();
    if (cd == (iconv_t)-1)
    {
        // No converter: pass ASCII through.
        bool ok = true;
        for (size_t i = 0; i < length; i++)
        {
            if ((unsigned char)text[i] < 0x80)
            {
                out->push_back(text[i]);
            }
            else
            {
                out->append(replacement);
                ok = false;
            }
        }
        return ok;
    }
    iconv(cd, NULL, NULL, NULL, NULL);
    bool ok = true;
    char *in = (char *)text;
    size_t in_left = length;
    char buffer[256];
    while (in_left > 0)
    {
        char *dst = buffer;
        size_t dst_left = sizeof(buffer);
        size_t result = iconv(cd, &in, &in_left, &dst, &dst_left);
        out->append(buffer, dst - buffer);
        if (result == (size_t)-1)
        {
            if (errno == E2BIG)
            {
                continue;
            }
            // EILSEQ or EINVAL (an incomplete sequence at the end): skip a
            // byte.
            out->append(replacement);
            ok = false;
            in++;
            in_left--;
            iconv(cd, NULL, NULL, NULL, NULL);
        }
    }
    char *dst = buffer;
    size_t dst_left = sizeof(buffer);
    iconv(cd, NULL, NULL, &dst, &dst_left);
    out->append(buffer, dst - buffer);
    return ok;
}

// UTF-8 decoding for the wide-character functions.
size_t utf8_decode(const unsigned char *s, size_t length, uint32_t *code_point)
{
    if (s[0] < 0x80)
    {
        *code_point = s[0];
        return 1;
    }
    int extra = s[0] >= 0xf0 ? 3 : s[0] >= 0xe0 ? 2 : s[0] >= 0xc0 ? 1 : -1;
    if (extra < 0 || (size_t)extra >= length)
    {
        *code_point = 0xfffd;
        return 1;
    }
    uint32_t c = s[0] & (0x3f >> extra);
    for (int i = 1; i <= extra; i++)
    {
        if ((s[i] & 0xc0) != 0x80)
        {
            *code_point = 0xfffd;
            return 1;
        }
        c = (c << 6) | (s[i] & 0x3f);
    }
    *code_point = c;
    return extra + 1;
}

void utf8_encode(uint32_t c, std::string *out)
{
    if (c < 0x80)
    {
        out->push_back((char)c);
    }
    else if (c < 0x800)
    {
        out->push_back((char)(0xc0 | (c >> 6)));
        out->push_back((char)(0x80 | (c & 0x3f)));
    }
    else if (c < 0x10000)
    {
        out->push_back((char)(0xe0 | (c >> 12)));
        out->push_back((char)(0x80 | ((c >> 6) & 0x3f)));
        out->push_back((char)(0x80 | (c & 0x3f)));
    }
    else
    {
        out->push_back((char)(0xf0 | (c >> 18)));
        out->push_back((char)(0x80 | ((c >> 12) & 0x3f)));
        out->push_back((char)(0x80 | ((c >> 6) & 0x3f)));
        out->push_back((char)(0x80 | (c & 0x3f)));
    }
}

} // namespace

std::string port_sjis_to_utf8(const char *text, int length)
{
    std::string out;
    if (text == NULL)
    {
        return out;
    }
    size_t n = length < 0 ? strlen(text) : (size_t)length;
    bool ascii = true;
    for (size_t i = 0; i < n; i++)
    {
        if ((unsigned char)text[i] >= 0x80)
        {
            ascii = false;
            break;
        }
    }
    if (ascii)
    {
        return std::string(text, n);
    }
    convert(g_sjis_to_utf8, "UTF-8", "CP932", text, n, &out, "\xef\xbf\xbd");
    return out;
}

bool port_utf8_to_sjis(const char *text, std::string *out)
{
    return convert(g_utf8_to_sjis, "CP932", "UTF-8", text, strlen(text), out, "?");
}

void port_log(const char *format, ...)
{
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    // Seconds since the first message.
    static const auto start = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    fprintf(stderr, "[th16-port %7.3f] %s\n", seconds, buffer);
}

extern "C" {

// The game converts one Shift-JIS path for the shell (resolve_shortcut);
// CP_ACP is Shift-JIS here, as on a Japanese Windows. WCHAR is wchar_t,
// 32 bits on Linux and macOS, so the result is UTF-32 there.
int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr,
                        int cchWideChar)
{
    size_t length = cbMultiByte < 0 ? strlen(lpMultiByteStr) + 1 : (size_t)cbMultiByte;
    std::string utf8 = CodePage == CP_UTF8 ? std::string(lpMultiByteStr, length)
                                           : port_sjis_to_utf8(lpMultiByteStr, (int)length);
    int count = 0;
    const unsigned char *s = (const unsigned char *)utf8.data();
    size_t left = utf8.size();
    while (left > 0)
    {
        uint32_t c;
        size_t used = utf8_decode(s, left, &c);
        s += used;
        left -= used;
        if (sizeof(WCHAR) == 2 && c >= 0x10000)
        {
            if (cchWideChar != 0)
            {
                if (count + 2 > cchWideChar)
                {
                    SetLastError(ERROR_INSUFFICIENT_BUFFER);
                    return 0;
                }
                c -= 0x10000;
                lpWideCharStr[count] = (WCHAR)(0xd800 + (c >> 10));
                lpWideCharStr[count + 1] = (WCHAR)(0xdc00 + (c & 0x3ff));
            }
            count += 2;
            continue;
        }
        if (cchWideChar != 0)
        {
            if (count + 1 > cchWideChar)
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return 0;
            }
            lpWideCharStr[count] = (WCHAR)c;
        }
        count++;
    }
    return count;
}

int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr,
                        int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
{
    size_t length = 0;
    if (cchWideChar < 0)
    {
        while (lpWideCharStr[length] != 0)
        {
            length++;
        }
        length++;
    }
    else
    {
        length = (size_t)cchWideChar;
    }
    std::string utf8;
    for (size_t i = 0; i < length; i++)
    {
        uint32_t c = (uint32_t)lpWideCharStr[i];
        if (sizeof(WCHAR) == 2 && c >= 0xd800 && c < 0xdc00 && i + 1 < length)
        {
            c = 0x10000 + ((c - 0xd800) << 10) + ((uint32_t)lpWideCharStr[i + 1] - 0xdc00);
            i++;
        }
        utf8_encode(c, &utf8);
    }
    std::string out;
    bool ok = true;
    if (CodePage == CP_UTF8)
    {
        out = utf8;
    }
    else
    {
        // Converted piecewise so that the terminator survives.
        size_t start = 0;
        while (start <= utf8.size())
        {
            size_t end = utf8.find('\0', start);
            if (end == std::string::npos)
            {
                end = utf8.size();
            }
            std::string piece;
            ok &= convert(g_utf8_to_sjis, "CP932", "UTF-8", utf8.data() + start, end - start, &piece,
                          lpDefaultChar != NULL ? lpDefaultChar : "?");
            out += piece;
            if (end < utf8.size())
            {
                out.push_back('\0');
            }
            start = end + 1;
        }
    }
    if (lpUsedDefaultChar != NULL)
    {
        *lpUsedDefaultChar = !ok;
    }
    if (cbMultiByte == 0)
    {
        return (int)out.size();
    }
    if (out.size() > (size_t)cbMultiByte)
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }
    memcpy(lpMultiByteStr, out.data(), out.size());
    return (int)out.size();
}

} // extern "C"
