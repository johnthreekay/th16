#pragma once

#include <d3dx9math.h>

#include "decomp.h"
#include "types.h"

// Small math helpers shared by the whole game: angles, ZUN's vector types
// and wrappers around the CRT's float functions.

#define ZUN_PI ((f32)(3.14159265358979323846))
#define ZUN_2PI ((f32)(ZUN_PI * 2.0f))

// a + b wrapped into [-pi, pi], giving up after 32 turns either way.
// TH06 equivalent: utils::AddNormalizeAngle
f32 LTCG_VECTORCALL add_normalize_angle(f32 a, f32 b);
// a wrapped into [-pi, pi] the same way.
f32 LTCG_VECTORCALL normalize_angle(f32 a);

// ZUN's vectors are D3DX's: get_point's adds only match with D3DXVECTOR3's
// own operator+=.
typedef D3DXVECTOR2 Float2;
typedef D3DXVECTOR3 Float3;

// Integer vector (ExpHP: zInt3), used for interpolated colors. Copied as
// a whole; indexing keeps the array syntax InterpInt3's fields had before.
struct Int3
{
    i32 x;
    i32 y;
    i32 z;

    Int3()
    {
    }
    Int3(i32 x, i32 y, i32 z) : x(x), y(y), z(z)
    {
    }
    Int3 operator+(const Int3 &o) const
    {
        return Int3(x + o.x, y + o.y, z + o.z);
    }
    Int3 operator-(const Int3 &o) const
    {
        return Int3(x - o.x, y - o.y, z - o.z);
    }
    Int3 operator*(f32 f) const
    {
        return Int3((i32)(x * f), (i32)(y * f), (i32)(z * f));
    }
    Int3 &operator+=(const Int3 &o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    i32 &operator[](i32 i)
    {
        return (&x)[i];
    }
};

// dst->x = radius * cosf(angle); dst->y = radius * sinf(angle) (ZUN's
// sincosmul, see ZunAsm.h). TH16 keeps a copy of it in each object file
// that uses it; this is the one at 0x4054d0, which PosVel and the collision
// code call. The other copies are statics in their own files
// (bullet_sincosmul, laser_sincosmul, ...).
void __fastcall from_polar(Float3 *dst, f32 angle, f32 radius);

// Small inline helpers around the UCRT's inline sinf, cosf and floorf. The
// functions at 0x405510, 0x4054f0 and 0x405260 are not ZUN's: they are
// LTCG's out-of-line copies of the UCRT inlines (_sinf, _cosf, _floorf, see
// CrtInline.cpp). In LTCG's call graph each helper is a node of its own,
// which the double stack alignment pass reaches, so LTCG keeps the UCRT
// body out of line there; the helper itself is then inlined into every
// caller, which is left calling the copy, as in the original. A large
// function that calls sinf directly inlines it and realigns its frame
// instead (docs/findings.md).
inline f32 zun_sinf(f32 x)
{
    return sinf(x);
}
inline f32 zun_cosf(f32 x)
{
    return cosf(x);
}
inline f32 zun_floorf(f32 x)
{
    return floorf(x);
}
// fabsf (0x405240), atan2f (0x4052a0) and tanf (0x43dc90) stay out-of-line
// wrappers of their own, called from all over the game (ECL, the HUD, the
// camera setup).
HARNESS_CALLED f32 zun_fabsf(f32 x);
HARNESS_CALLED f32 zun_atan2f(f32 y, f32 x);
HARNESS_CALLED f32 zun_tanf(f32 x);

// normalize_angle's loop, for the many places that inline it (ZunAngle,
// PosVel, the collision code). The original inlines it everywhere; ours
// would call normalize_angle from PosVel::step.
__forceinline f32 wrap_angle(f32 a)
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
