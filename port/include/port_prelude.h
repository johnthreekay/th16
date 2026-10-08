// Force-included (-include port_prelude.h) into every translation unit of the
// portable build, before the first line of the file. It stands in for what
// MSVC provides without any header: its calling convention keywords,
// __forceinline, __int64, __assume and the 32-bit layout checks.
//
// The matching MSVC build never sees this file.
#pragma once

#ifndef TH16_PORT
#error "port_prelude.h is only for the portable build (TH16_PORT)"
#endif

// The C headers the game includes, pulled in before the keyword macros
// below so that none of them leak into the standard library.
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__LP64__) || defined(_WIN64) || UINTPTR_MAX > 0xffffffffu
#define TH16_PORT_64BIT 1
#else
#define TH16_PORT_64BIT 0
#endif

// Calling conventions. LTCG picked the original's conventions; the port
// compiles every caller and callee itself, so the platform default is right
// everywhere (including function pointer tables, which are only filled and
// called by our own code).
#define __cdecl
#define __stdcall
#define __fastcall
#define __thiscall
#define __vectorcall
#define WINAPI
#define CALLBACK
#define APIENTRY

// Only an inlining hint here: always_inline turns a failed inline into an
// error, and nothing in the port depends on inlining.
#define __forceinline inline

// __declspec(safebuffers) turns off MSVC's /GS stack cookies for a function;
// the matching build puts it on a few functions. The port has no /GS, so it
// expands to nothing. Any other __declspec stops the build here (the paste
// names no macro) rather than being dropped silently.
#define __declspec(spec) TH16_PORT_DECLSPEC_##spec
#define TH16_PORT_DECLSPEC_safebuffers

// (MSVC aligns it to 8 inside structs and i386 GCC to 4; the game's two
// __int64 fields sit at offsets where that makes no difference, see
// CMakeLists.txt.)
#define __int64 long long

// An optimizer hint in MSVC. The port drops it rather than turning it into
// __builtin_unreachable: behaviour is the same whenever the assumption
// holds, and nothing undefined is added when it does not.
#define __assume(x) ((void)0)

#ifndef _alloca
#define _alloca(size) __builtin_alloca(size)
#endif

// A compile-time check that stays on in the 64-bit build, for layout facts
// that hold at every pointer size (port/src/layout_checks.cpp).
#define TH16_PORT_CHECK_CAT2(a, b) a##b
#define TH16_PORT_CHECK_CAT(a, b) TH16_PORT_CHECK_CAT2(a, b)
#define TH16_PORT_CHECK(cond) \
    typedef char TH16_PORT_CHECK_CAT(th16_port_check_, __LINE__)[(cond) ? 1 : -1] __attribute__((unused))

// The game's static_assert checks describe the 32-bit MSVC layout (offsets
// of fields next to pointers, sizes of structs holding pointers). They stay
// in force for the -m32 build, which keeps that layout; the 64-bit build
// skips them. The standard headers above are already included, so this only
// reaches the game's own asserts (and port/ headers included after it).
#if TH16_PORT_64BIT
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define static_assert(...) static_assert(true, "32-bit layout check, skipped on 64-bit")
#endif

// The MSVC CRT extensions (_stricmp, _vsnprintf_l, _time64, ...).
#include "port_crt.h"

// x87 helpers for the game's inline assembly (fsincos, frndint).
#include "port_fpu.h"
