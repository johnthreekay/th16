#pragma once

#include "decomp.h"
#include "types.h"

// Shape intersection tests shared by bullets, lasers and the player. The
// first four float arguments arrive in xmm0-xmm3 (LTCG), the rest on the
// stack, which the callee pops.

// 0x403d30. Whether a circle touches a rectangle of size w x h centered on
// (rect_x, rect_y) and rotated by angle.
HARNESS_CALLED i32 __stdcall collision_test_circle_rect(f32 rect_x, f32 rect_y, f32 w, f32 h, f32 angle, f32 circle_x,
                                              f32 circle_y, f32 radius);
