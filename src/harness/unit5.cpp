// Stand-in callers for unit 5's HARNESS_CALLED functions, shaped like the
// original call sites (ECL instructions around 0x4216c8-0x4222b5, bombs
// around 0x40eb98 and the player around 0x442669).
#include "../Laser.h"

i32 harness_laser_cancel(Float3 *a, Float3 *b, f32 angle, f32 radius, i32 mode)
{
    i32 n = g_LaserManager->cancel_in_rectangle(a, b, angle, 5, 1);
    n += g_LaserManager->cancel_in_radius(a, radius, 0, 1);
    n += g_LaserManager->cancel_in_radius(a, radius, 1, 1);
    n += g_LaserManager->cancel_in_radius(b, radius, 0, 0);
    g_LaserManager->clear_all(1, 0);
    g_LaserManager->clear_all(mode, 0);
    return n;
}
