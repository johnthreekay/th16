// The MSVC CRT functions the game uses that the C library here lacks:
// port_crt.h and direct.h (process.h's thread functions are in
// win32_thread.cpp).
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <direct.h>
#include <process.h>

#include <string>

#include <windows.h>

#include "port_stub.h"
#include "port_vfs.h"

extern "C" {

int _stricmp(const char *a, const char *b)
{
    // MSVC compares lowercased bytes in the C locale.
    for (;; a++, b++)
    {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb || ca == 0)
        {
            return ca - cb;
        }
    }
}

int _strnicmp(const char *a, const char *b, size_t count)
{
    for (; count != 0; a++, b++, count--)
    {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb || ca == 0)
        {
            return ca - cb;
        }
    }
    return 0;
}

int _vsnprintf(char *buffer, size_t count, const char *format, va_list args)
{
    // vsnprintf always terminates and returns the full length; MSVC's
    // version returns -1 when the result does not fit, leaving the buffer
    // full and unterminated (unless it fits exactly without the terminator,
    // in which case it returns count and also leaves it unterminated).
    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0)
    {
        return -1;
    }
    if ((size_t)length < count)
    {
        return vsnprintf(buffer, count, format, args);
    }
    if (count != 0)
    {
        char *full = new char[length + 1];
        vsnprintf(full, length + 1, format, args);
        memcpy(buffer, full, count);
        delete[] full;
    }
    return (size_t)length == count ? length : -1;
}

int _vsnprintf_l(char *buffer, size_t count, const char *format, _locale_t locale, va_list args)
{
    return _vsnprintf(buffer, count, format, args);
}

int _vsprintf_l(char *buffer, const char *format, _locale_t locale, va_list args)
{
    return vsprintf(buffer, format, args);
}

int _snprintf(char *buffer, size_t count, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = _vsnprintf(buffer, count, format, args);
    va_end(args);
    return result;
}

__time64_t _time64(__time64_t *t)
{
    __time64_t now = (__time64_t)time(NULL);
    if (t != NULL)
    {
        *t = now;
    }
    return now;
}

struct tm *_localtime64(const __time64_t *t)
{
    // MSVC returns NULL for negative times; a 32-bit time_t cannot hold
    // dates past 2038, which also fail here.
    if (*t < 0 || (__time64_t)(time_t)*t != *t)
    {
        return NULL;
    }
    time_t value = (time_t)*t;
    return localtime(&value);
}

// The current directory and new directories live in the game's file
// namespace (port_vfs.h), like CreateFileA's paths.
int _chdir(const char *dirname)
{
    if (!port_vfs_set_cwd(dirname))
    {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

int _mkdir(const char *dirname)
{
    if (!CreateDirectoryA(dirname, NULL))
    {
        errno = GetLastError() == ERROR_ALREADY_EXISTS ? EEXIST : ENOENT;
        return -1;
    }
    return 0;
}

char *_getcwd(char *buffer, int maxlen)
{
    std::string cwd = port_vfs_get_cwd();
    if (buffer == NULL)
    {
        return strdup(cwd.c_str());
    }
    if ((size_t)maxlen <= cwd.size())
    {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buffer, cwd.c_str(), cwd.size() + 1);
    return buffer;
}

// _beginthread and _beginthreadex are with the other thread functions in
// win32_thread.cpp.

} // extern "C"
