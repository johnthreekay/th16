#include "ZunMath.h"

// GLOBAL: TH16 0x4a5788
f32 g_GameSpeed = 1.0f;

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
DECOMP_NOINLINE f32 LTCG_VECTORCALL normalize_angle(f32 a)
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
void Float3::from_polar(f32 angle, f32 radius)
{
    __asm
    {
        mov eax, this
        fld angle
        fsincos
        fmul radius
        fstp dword ptr [eax]
        fmul radius
        fstp dword ptr [eax + 4]
    }
}
