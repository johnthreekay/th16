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
