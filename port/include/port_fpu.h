// Portable stand-ins for the x87 inline assembly in the game: the TH16_PORT
// branches of the src/ZunAsm.h macros call these.
//
// The original evaluates these on the x87 stack: the angle is loaded from a
// float, fsincos produces 64-bit-mantissa results, and the products are
// rounded to float once, when stored. The game runs `finit` at startup
// (Rng.cpp, GameThread.cpp, ...), which sets the x87 precision control to
// 64 bits, so extended precision is the state these blocks ran in.
//
// On x86 and x86-64 the helpers use the same instructions (GNU inline asm,
// long double is the x87 80-bit type), which gives bit-identical results.
// Elsewhere they fall back to sinl/cosl, which can differ in the last bit of
// the float result; replays that depend on it would desync there.
#pragma once

#include <math.h>

#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
#define TH16_PORT_X87 1
#else
#define TH16_PORT_X87 0
#endif

typedef long double port_x87_t;

// fld angle; fsincos. Leaves cos in st0 and sin in st1.
static inline void port_fsincos(float angle, port_x87_t *sine, port_x87_t *cosine)
{
#if TH16_PORT_X87
    port_x87_t c;
    port_x87_t s;
    __asm__("fsincos" : "=t"(c), "=u"(s) : "0"((port_x87_t)angle));
    *sine = s;
    *cosine = c;
#else
    *sine = sinl((port_x87_t)angle);
    *cosine = cosl((port_x87_t)angle);
#endif
}

// ZUN_ASM_SINCOSMUL_XY, ZUN_ASM_SINCOSMUL and ZUN_ASM_SINCOSMUL_PTRS:
// fld angle; fsincos; fmul rx; fstp [x]; fmul ry; fstp [y].
static inline void port_sincosmul2(float *x, float *y, float angle, float rx, float ry)
{
    port_x87_t s;
    port_x87_t c;
    port_fsincos(angle, &s, &c);
    *x = (float)(c * (port_x87_t)rx);
    *y = (float)(s * (port_x87_t)ry);
}

// ZUN_ASM_SINCOS: fld angle; fsincos; fstp cosine; fstp sine.
static inline void port_sincos(float angle, float *sine, float *cosine)
{
    port_x87_t s;
    port_x87_t c;
    port_fsincos(angle, &s, &c);
    *cosine = (float)c;
    *sine = (float)s;
}

// ZUN_ASM_SNAP_QUAD_TO_PIXEL_CENTERS, per coordinate:
// fld x; frndint; fsub half: round to nearest (even) in the default
// rounding mode, then subtract. One rounding to float either way, so float
// arithmetic gives the same result as the x87 sequence.
static inline float port_frndint_sub(float x, float sub)
{
    return (float)(nearbyintf(x) - sub);
}

// ZUN_ASM_FINIT (__asm finit). The x87 unit's default state (round to nearest, 64-bit
// precision) is what Linux and macOS start threads with, and the port's
// float math does not go through the x87 stack, so there is nothing to
// reset.
static inline void port_finit(void)
{
}
