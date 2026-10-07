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

// 0x4049c0. Whether two rotated rectangles (center, size, angle) overlap.
HARNESS_CALLED i32 __stdcall collision_test_rect_rect(f32 x1, f32 y1, f32 w1, f32 h1, f32 angle1, f32 x2, f32 y2,
                                                      f32 w2, f32 h2, f32 angle2);

struct D3DXVECTOR3;
// 0x404600. Where a ray from start along angle enters a rectangle of size
// w x h centered on (x, y) and rotated by rect_angle (and where it leaves);
// 0 if it misses.
HARNESS_CALLED i32 __fastcall collision_ray_rect(D3DXVECTOR3 *enter, D3DXVECTOR3 *exit, D3DXVECTOR3 *start, f32 angle,
                                                 f32 x, f32 y, f32 w, f32 h, f32 rect_angle);
