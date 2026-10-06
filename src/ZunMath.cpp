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
f32 LTCG_VECTORCALL normalize_angle(f32 a)
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
