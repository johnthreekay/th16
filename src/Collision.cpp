#include <math.h>

#include "Collision.h"
#include "ZunMath.h"

// TODO: same operations, different register and stack slot choices (the
// original keeps 0.5 and the two offsets in registers).
// FUNCTION: TH16 0x403d30
HARNESS_CALLED i32 __stdcall collision_test_circle_rect(f32 rect_x, f32 rect_y, f32 w, f32 h, f32 angle, f32 circle_x,
                                              f32 circle_y, f32 radius)
{
    // Move the circle into the rectangle's frame.
    circle_x -= rect_x;
    circle_y -= rect_y;
    angle = -angle;
    f32 s = zun_sinf(angle);
    f32 c = zun_cosf(angle);
    f32 x = circle_x * c - circle_y * s;
    f32 y = circle_x * s + circle_y * c;
    f32 half_w = w * 0.5f;
    f32 half_h = h * 0.5f;
    f32 abs_x = fabsf(x);
    if (half_w + radius >= abs_x && half_h >= fabsf(y))
    {
        return 1;
    }
    if (half_w >= abs_x && half_h + radius >= fabsf(y))
    {
        return 1;
    }
    // Then the corners.
    f32 radius_sq = radius * radius;
    if (radius_sq > (x - half_w) * (x - half_w) + (y - half_h) * (y - half_h))
    {
        return 1;
    }
    if (radius_sq > (x + half_w) * (x + half_w) + (y - half_h) * (y - half_h))
    {
        return 1;
    }
    if (radius_sq > (x - half_w) * (x - half_w) + (y + half_h) * (y + half_h))
    {
        return 1;
    }
    return radius_sq > (x + half_w) * (x + half_w) + (y + half_h) * (y + half_h);
}
