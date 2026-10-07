#pragma once

// ZUN's inline x87 assembly.
//
// TH16 has a handful of short __asm blocks: the fsincos helpers (a sine and
// cosine in one instruction, scaled by a radius), a pixel-snapping block in
// the sprite renderer and `finit` at the start of a few functions. MSVC
// copies inline asm into the output as written, so these stay asm; the
// macros below give each block a name and say in C terms what it computes.
//
// Each macro expands to one braced __asm block, so it is used as a
// statement (`ZUN_ASM_FINIT();`). The operands must be plain variable
// names (locals, parameters or globals): MSVC's inline assembler reads
// variables by name and cannot evaluate C expressions. Inside a macro every
// instruction needs its own __asm prefix, and `;` would start an asm comment
// that swallows the rest of the line, so there is none.
//
// The ZUN_ASM_SINCOSMUL* blocks are the bodies of small __fastcall functions
// that TH16 keeps once per object file (0x4054d0, 0x417510, 0x430df0, ...).
// Each source file defines its own static copy so that every copy can be
// annotated with its address; they must stay per file. ZUN_ASM_SINCOS and
// ZUN_ASM_FINIT sit inline in the functions that use them. Direct3D 9
// switches the x87 FPU to 24-bit precision when it creates a device (TH16
// does not ask it to preserve the FPU state), which matters for these
// x87 blocks; most of the game's float code uses SSE.
//
// The portable build (port/, TH16_PORT) has no MSVC inline assembler: there
// each macro calls the port/include/port_fpu.h helper that computes the
// same thing (on x86 and x86-64 with the same x87 instructions, through
// GNU inline asm). Those expand to ordinary calls, so the operands may be
// any expressions there.

#ifdef TH16_PORT
#include "port_fpu.h"
#endif

// Resets the x87 FPU: control word 0x37f (64-bit precision, round to
// nearest even, all exceptions masked), empty register stack, cleared
// status. Has no effect on C variables; it undoes Direct3D's 24-bit
// precision setting for the x87 code that follows.
#ifdef TH16_PORT
#define ZUN_ASM_FINIT() port_finit()
#else
#define ZUN_ASM_FINIT() __asm { __asm finit }
#endif

// cosine = cosf(angle); sine = sinf(angle);
// (all three f32 variables; fsincos leaves cos on top of the x87 stack and
// sin below it)
#ifdef TH16_PORT
#define ZUN_ASM_SINCOS(angle, sine, cosine) port_sincos(angle, &(sine), &(cosine))
#else
#define ZUN_ASM_SINCOS(angle, sine, cosine) \
    __asm { \
        __asm fld angle \
        __asm fsincos \
        __asm fstp cosine \
        __asm fstp sine \
    }
#endif

// dst->x = radius_x * cosf(angle); dst->y = radius_y * sinf(angle);
// (dst a Float3 * variable, the others f32 variables; dst->z is left alone)
#ifdef TH16_PORT
#define ZUN_ASM_SINCOSMUL_XY(dst, angle, radius_x, radius_y) \
    port_sincosmul2(&(dst)->x, &(dst)->y, angle, radius_x, radius_y)
#else
#define ZUN_ASM_SINCOSMUL_XY(dst, angle, radius_x, radius_y) \
    __asm { \
        __asm mov eax, dst \
        __asm fld angle \
        __asm fsincos \
        __asm fmul radius_x \
        __asm fstp dword ptr [eax] \
        __asm fmul radius_y \
        __asm fstp dword ptr [eax+4] \
    }
#endif

// dst->x = radius * cosf(angle); dst->y = radius * sinf(angle);
// The point at distance radius from the origin in direction angle. TH06
// calls this helper sincosmul.
#define ZUN_ASM_SINCOSMUL(dst, angle, radius) ZUN_ASM_SINCOSMUL_XY(dst, angle, radius, radius)

// *x = radius * cosf(angle); *y = radius * sinf(angle);
// The same with the two results going through separate f32 * variables.
#ifdef TH16_PORT
#define ZUN_ASM_SINCOSMUL_PTRS(x, y, angle, radius) port_sincosmul2(x, y, angle, radius, radius)
#else
#define ZUN_ASM_SINCOSMUL_PTRS(x, y, angle, radius) \
    __asm { \
        __asm mov eax, x \
        __asm fld angle \
        __asm fsincos \
        __asm fmul radius \
        __asm fstp dword ptr [eax] \
        __asm fmul radius \
        __asm mov eax, y \
        __asm fstp dword ptr [eax] \
    }
#endif

// Snaps an axis-aligned quad to pixel centers. verts is a global array of
// four vertices with a Float3 `pos` (corners 0 top left, 1 top right, 2
// bottom left, 3 bottom right) and half an f32 variable holding 0.5f:
//   verts[0].pos.x = verts[2].pos.x = rintf(verts[0].pos.x) - half;
//   verts[1].pos.x = verts[3].pos.x = rintf(verts[1].pos.x) - half;
//   verts[0].pos.y = verts[1].pos.y = rintf(verts[0].pos.y) - half;
//   verts[2].pos.y = verts[3].pos.y = rintf(verts[2].pos.y) - half;
// frndint rounds in the FPU's rounding mode, to nearest even by default
// (like rintf). Inline asm indexes arrays in bytes, hence `n * TYPE verts`
// for element n. (The port computes all four values before storing any,
// as the asm does, since the stores overwrite corners it reads.)
#ifdef TH16_PORT
#define ZUN_ASM_SNAP_QUAD_TO_PIXEL_CENTERS(verts, half) \
    do \
    { \
        f32 zun_snap_x0 = port_frndint_sub((verts)[0].pos.x, half); \
        f32 zun_snap_x1 = port_frndint_sub((verts)[1].pos.x, half); \
        f32 zun_snap_y0 = port_frndint_sub((verts)[0].pos.y, half); \
        f32 zun_snap_y2 = port_frndint_sub((verts)[2].pos.y, half); \
        (verts)[2].pos.y = (verts)[3].pos.y = zun_snap_y2; \
        (verts)[0].pos.y = (verts)[1].pos.y = zun_snap_y0; \
        (verts)[1].pos.x = (verts)[3].pos.x = zun_snap_x1; \
        (verts)[0].pos.x = (verts)[2].pos.x = zun_snap_x0; \
    } while (0)
#else
#define ZUN_ASM_SNAP_QUAD_TO_PIXEL_CENTERS(verts, half) \
    __asm { \
        __asm fld verts[0 * TYPE verts].pos.x \
        __asm frndint \
        __asm fsub half \
        __asm fld verts[1 * TYPE verts].pos.x \
        __asm frndint \
        __asm fsub half \
        __asm fld verts[0 * TYPE verts].pos.y \
        __asm frndint \
        __asm fsub half \
        __asm fld verts[2 * TYPE verts].pos.y \
        __asm frndint \
        __asm fsub half \
        __asm fst verts[2 * TYPE verts].pos.y \
        __asm fstp verts[3 * TYPE verts].pos.y \
        __asm fst verts[0 * TYPE verts].pos.y \
        __asm fstp verts[1 * TYPE verts].pos.y \
        __asm fst verts[1 * TYPE verts].pos.x \
        __asm fstp verts[3 * TYPE verts].pos.x \
        __asm fst verts[0 * TYPE verts].pos.x \
        __asm fstp verts[2 * TYPE verts].pos.x \
    }
#endif
