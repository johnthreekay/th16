// The MSVC CRT extensions the game calls without including anything beyond
// the standard C headers (MSVC declares them in string.h, stdio.h and
// time.h). Included by port_prelude.h; implemented in
// port/src/crt_compat.cpp.
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define __CRTDECL

typedef struct th16_crt_locale *_locale_t;

// 64-bit time as stored in replays and the score file. (32-bit glibc has the
// same typedef, which C++ allows to repeat.)
typedef long long __time64_t;

extern "C" {
int _stricmp(const char *a, const char *b);
int _strnicmp(const char *a, const char *b, size_t count);

// MSVC semantics: a result that does not fit returns -1 and is not
// terminated when it fills the buffer exactly.
int _vsnprintf_l(char *buffer, size_t count, const char *format, _locale_t locale, va_list args);
int _vsprintf_l(char *buffer, const char *format, _locale_t locale, va_list args);
int _vsnprintf(char *buffer, size_t count, const char *format, va_list args);
int _snprintf(char *buffer, size_t count, const char *format, ...);

__time64_t _time64(__time64_t *t);
struct tm *_localtime64(const __time64_t *t);
}

// MSVC's time_t is 64 bits and its localtime is _localtime64; the game calls
// it with __time64_t fields of replays and the score file. Where time_t is
// narrower or a different type, this overload takes those.
inline struct tm *localtime(const __time64_t *t)
{
    return _localtime64(t);
}
