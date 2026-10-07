#include "ZunAngle.h"
#include "ZunMath.h"

// Inlined everywhere in the original; same loop as normalize_angle.
static inline f32 wrap(f32 a)
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

// FUNCTION: TH16 0x4052e0
HARNESS_CALLED ZunAngle ZunAngle::operator+(f32 delta) const
{
    ZunAngle result;
    result.value = wrap(value + delta);
    return result;
}

// FUNCTION: TH16 0x405340
HARNESS_CALLED ZunAngle::ZunAngle(f32 value)
{
    this->value = wrap(value);
}

// FUNCTION: TH16 0x405390
HARNESS_CALLED ZunAngle ZunAngle::operator-(const ZunAngle &other) const
{
    f32 a = value;
    f32 b = other.value;
    f32 d;
    if (a - b > ZUN_PI)
    {
        d = a - (b + ZUN_2PI);
    }
    else if (b - a > ZUN_PI)
    {
        d = a - (b - ZUN_2PI);
    }
    else
    {
        d = a - b;
    }
    ZunAngle result;
    result.value = wrap(d);
    return result;
}

// FUNCTION: TH16 0x405420
HARNESS_CALLED ZunAngle &ZunAngle::operator+=(f32 delta)
{
    value = wrap(value + delta);
    return *this;
}

// FUNCTION: TH16 0x405480
HARNESS_CALLED ZunAngle &ZunAngle::operator=(f32 value)
{
    this->value = wrap(value);
    return *this;
}

// FUNCTION: TH16 0x4475f0
HARNESS_CALLED ZunAngle ZunAngle::operator*(f32 factor) const
{
    ZunAngle result;
    result.value = wrap(value * factor);
    return result;
}

// FUNCTION: TH16 0x447650
HARNESS_CALLED ZunAngle ZunAngle::operator+(const ZunAngle &other) const
{
    ZunAngle result;
    result.value = wrap(value + other.value);
    return result;
}
