// The MSVC CRT functions the game uses that the C library here lacks:
// port_crt.h, process.h and direct.h. These are complete implementations,
// not stubs: they need nothing from the platform.
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

#include "port_stub.h"

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

// The game passes relative paths with either separator; the host wants '/'.
static void to_host_path(char *out, size_t size, const char *path)
{
    size_t i = 0;
    for (; path[i] != '\0' && i + 1 < size; i++)
    {
        out[i] = path[i] == '\\' ? '/' : path[i];
    }
    out[i] = '\0';
}

int _chdir(const char *dirname)
{
    char path[4096];
    to_host_path(path, sizeof(path), dirname);
    return chdir(path);
}

int _mkdir(const char *dirname)
{
    char path[4096];
    to_host_path(path, sizeof(path), dirname);
    return mkdir(path, 0777);
}

char *_getcwd(char *buffer, int maxlen)
{
    return getcwd(buffer, (size_t)maxlen);
}

// Threads: the platform layer will start these with std::thread or SDL.
uintptr_t _beginthread(void(__cdecl *start_address)(void *), unsigned stack_size, void *arglist)
{
    PORT_UNIMPLEMENTED();
    return (uintptr_t)-1;
}

uintptr_t _beginthreadex(void *security, unsigned stack_size, unsigned(__stdcall *start_address)(void *),
                         void *arglist, unsigned initflag, unsigned *thrdaddr)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

void _endthread(void)
{
    PORT_UNIMPLEMENTED();
}

void _endthreadex(unsigned retval)
{
    PORT_UNIMPLEMENTED();
}

} // extern "C"
