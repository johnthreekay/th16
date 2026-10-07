// Stand-ins compiled with /GL for functions that wave 5 range C
// (0x440000-0x44c000) calls but other ranges have not decompiled, where
// LTCG has to see a body (register arguments). Each forwards to an opaque
// stub.
#include "../Collision.h"
#include "../ZunMath.h"

int w5c_sink(void *object, int value);
int w5c_sink_f(float a, float b);

// STUB: TH16 0x4049c0
HARNESS_CALLED i32 __stdcall collision_test_rect_rect(f32 x1, f32 y1, f32 w1, f32 h1, f32 angle1, f32 x2, f32 y2,
                                                      f32 w2, f32 h2, f32 angle2)
{
    return w5c_sink_f(x1 * y1 + w1 * h1 + angle1, x2 * y2 + w2 * h2 + angle2);
}

// STUB: TH16 0x404600
HARNESS_CALLED i32 __fastcall collision_ray_rect(D3DXVECTOR3 *enter, D3DXVECTOR3 *exit, D3DXVECTOR3 *start, f32 angle,
                                                 f32 x, f32 y, f32 w, f32 h, f32 rect_angle)
{
    enter->x = start->x + x * angle;
    exit->y = start->y + y * w;
    return w5c_sink_f(h, rect_angle);
}
