#pragma once

#include <d3dx9math.h>

#include "decomp.h"
#include "types.h"

#define ZUN_PI ((f32)(3.14159265358979323846))
#define ZUN_2PI ((f32)(ZUN_PI * 2.0f))

// Wrap an angle into [-pi, pi], giving up after 32 turns either way.
// TH06 equivalent: utils::AddNormalizeAngle
f32 LTCG_VECTORCALL add_normalize_angle(f32 a, f32 b);
f32 LTCG_VECTORCALL normalize_angle(f32 a);

// ZUN's vectors are D3DX's: get_point's adds only match with D3DXVECTOR3's
// own operator+=.
typedef D3DXVECTOR2 Float2;
typedef D3DXVECTOR3 Float3;

// out->x, out->y = radius * (cos angle, sin angle). TH06 equivalent:
// sincosmul. TH16 keeps a separate out-of-line copy in many object files.
static void __fastcall sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}
