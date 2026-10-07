#include <math.h>

#include "ZunMath.h"

// FUNCTION: TH16 0x402d30
f32 LTCG_VECTORCALL add_normalize_angle(f32 a, f32 b)
{
    i32 i = 0;
    a += b;
    while (a > ZUN_PI)
    {
        a -= ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    while (a < -ZUN_PI)
    {
        a += ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    return a;
}

// FUNCTION: TH16 0x402d90
HARNESS_CALLED f32 LTCG_VECTORCALL normalize_angle(f32 a)
{
    i32 i = 0;
    while (a > ZUN_PI)
    {
        a -= ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    while (a < -ZUN_PI)
    {
        a += ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    return a;
}

// FUNCTION: TH16 0x4054d0
HARNESS_CALLED void __fastcall from_polar(Float3 *dst, f32 angle, f32 radius)
{
    __asm
    {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp dword ptr [eax]
        fmul radius
        fstp dword ptr [eax + 4]
    }
}

// TODO: the original realigns its frame (and esp, -8) around the call; ours
// only does that for sinf and cosf.
// FUNCTION: TH16 0x43dc90
HARNESS_CALLED f32 zun_tanf(f32 x)
{
    return tanf(x);
}

// FUNCTION: TH16 0x405240
HARNESS_CALLED f32 zun_fabsf(f32 x)
{
    return fabsf(x);
}

// FUNCTION: TH16 0x4052a0
HARNESS_CALLED f32 zun_atan2f(f32 y, f32 x)
{
    // atan2f's body: with enough callers in view LTCG stops inlining the
    // UCRT inline here.
    return (f32)atan2(y, x);
}
