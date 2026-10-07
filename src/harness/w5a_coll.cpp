// Stand-in callers for the collision helpers of wave 5 agent A, whose real
// callers (the lasers, the player's damage sources) are not decompiled yet.
#include "../Collision.h"

// LaserLine (0x43272e and three more) and the player (0x446a9d) find where
// a line crosses segments and rectangles.
i32 harness_collision_segments(f32 *out, f32 *a, f32 *b, Float3 *pos, f32 angle)
{
    i32 n = collision_segment_intersection(&out[0], &out[1], a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
    n += collision_segment_intersection(&out[2], &out[3], b[4], b[5], a[4], a[5], b[6], b[7], a[6], a[7]);
    n += collision_line_rect((Float2 *)&out[8], (Float2 *)&out[10], pos, angle, a[13], b[13], a[14], b[14], a[15]);
    n += collision_line_rect((Float2 *)&out[12], (Float2 *)&out[14], pos + 1, b[15], a[16], b[16], angle, a[17],
                             b[17]);
    return n;
}

// The player's damage sources test rectangles against enemies (0x445b37).
i32 harness_collision_rect_rect(f32 *a, f32 *b, f32 angle)
{
    return collision_test_rect_rect(a[0], a[1], a[2], a[3], angle, b[0], b[1], b[2], b[3], b[4]) +
           collision_test_rect_rect(b[5], b[6], a[4], a[5], b[7], angle, a[6], b[8], a[7], b[9]);
}
