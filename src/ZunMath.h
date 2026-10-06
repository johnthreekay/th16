#pragma once

#include "decomp.h"
#include "types.h"

#define ZUN_PI ((f32)(3.14159265358979323846))
#define ZUN_2PI ((f32)(ZUN_PI * 2.0f))

// Wrap an angle into [-pi, pi], giving up after 32 turns either way.
// TH06 equivalent: utils::AddNormalizeAngle
f32 LTCG_VECTORCALL add_normalize_angle(f32 a, f32 b);
f32 LTCG_VECTORCALL normalize_angle(f32 a);

// The loop of normalize_angle, for the many places that inline it.
inline f32 wrap_angle(f32 a)
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

// Time scale for the object being updated. ECL sets it to slow down boss
// deaths, and the game swaps it per object many times a frame.
extern f32 g_GameSpeed;

struct Float2
{
    f32 x;
    f32 y;
};

struct Float3
{
    f32 x;
    f32 y;
    f32 z;

    // x = cos(angle) * radius, y = sin(angle) * radius; z is untouched.
    // ZUN wrote it with fsincos. The original has a byte-identical copy of
    // it next to the code of most source files (0x406470, 0x406cc0, ...).
    void from_polar(f32 angle, f32 radius);

    Float3 operator+(const Float3 &other) const
    {
        Float3 result;
        result.x = x + other.x;
        result.y = y + other.y;
        result.z = z + other.z;
        return result;
    }

    Float3 &operator+=(const Float3 &other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
};
