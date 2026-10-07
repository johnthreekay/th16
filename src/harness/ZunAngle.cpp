// Stand-in callers for ZunAngle's HARNESS_CALLED functions.
#include "../ZunAngle.h"

f32 harness_zun_angle(f32 a, f32 b)
{
    ZunAngle angle(a);
    angle = b;
    angle += a;
    ZunAngle sum = angle + b;
    ZunAngle diff = sum - angle;
    return diff.value;
}

// Like InterpAngle::step (0x464080), which scales and adds angles.
f32 harness_zun_angle_interp(ZunAngle *a, ZunAngle *b, f32 t, f32 u)
{
    ZunAngle x = *a * t;
    ZunAngle y = *b * u;
    ZunAngle sum = x.add(y);
    return sum.value;
}
