#include "ZunAngle.h"
#include "ZunMath.h"

// FUNCTION: TH16 0x4052e0
HARNESS_CALLED ZunAngle ZunAngle::operator+(f32 delta) const
{
    ZunAngle result;
    result.value = wrap_angle(value + delta);
    return result;
}

// FUNCTION: TH16 0x405340
HARNESS_CALLED ZunAngle::ZunAngle(f32 value)
{
    this->value = wrap_angle(value);
}

// The shortest signed difference: a - b, taken the other way round the
// circle when that is shorter.
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
    result.value = wrap_angle(d);
    return result;
}

// FUNCTION: TH16 0x405420
HARNESS_CALLED ZunAngle &ZunAngle::operator+=(f32 delta)
{
    value = wrap_angle(value + delta);
    return *this;
}

// FUNCTION: TH16 0x405480
HARNESS_CALLED ZunAngle &ZunAngle::operator=(f32 value)
{
    this->value = wrap_angle(value);
    return *this;
}

// The dead double makes LTCG's double stack alignment pass count this as
// wanting an 8-aligned stack, so InterpAngle::step, its only caller, pads its
// frame for it like the original (see TitleInf::set_substate).
// FUNCTION: TH16 0x4475f0
HARNESS_CALLED ZunAngle ZunAngle::operator*(f32 factor) const
{
    double unused = factor;
    (void)unused;
    ZunAngle result;
    result.value = wrap_angle(value * factor);
    return result;
}

// FUNCTION: TH16 0x447650
HARNESS_CALLED ZunAngle ZunAngle::operator+(const ZunAngle &other) const
{
    ZunAngle result;
    result.value = wrap_angle(value + other.value);
    return result;
}
