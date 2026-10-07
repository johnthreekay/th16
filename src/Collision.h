#pragma once

#include "ZunMath.h"
#include "decomp.h"
#include "types.h"

// Shape intersection tests shared by bullets, lasers and the player. The
// first four float arguments arrive in xmm0-xmm3 (LTCG), the rest on the
// stack, which the callee pops.

// 0x403d30. Whether a circle touches a rectangle of size w x h centered on
// (rect_x, rect_y) and rotated by angle.
HARNESS_CALLED i32 __stdcall collision_test_circle_rect(f32 rect_x, f32 rect_y, f32 w, f32 h, f32 angle, f32 circle_x,
                                              f32 circle_y, f32 radius);

// 0x403a90. Whether any of four points lies in a rectangle of size w x h
// centered on (x, y) and rotated by angle. The points arrive in ecx.
HARNESS_CALLED i32 __stdcall collision_test_points_in_rect(f32 x, f32 y, f32 w, f32 h, f32 angle, Float2 *points);

// 0x403ec0. Where segments (x1, y1)-(x2, y2) and (x3, y3)-(x4, y4) cross;
// 0 if they do not. The result pointers arrive in ecx and edx.
HARNESS_CALLED i32 __stdcall collision_segment_intersection(f32 *out_x, f32 *out_y, f32 x1, f32 y1, f32 x2, f32 y2,
                                                            f32 x3, f32 y3, f32 x4, f32 y4);

// 0x404220. Where the lines through (x1, y1) at angle1 and (x2, y2) at
// angle2 cross; 0 (and the first point) if they are parallel.
HARNESS_CALLED i32 __stdcall collision_line_intersection(f32 *out_x, f32 *out_y, f32 x1, f32 y1, f32 angle1, f32 x2,
                                                         f32 y2, f32 angle2);

// 0x404600. Where a line through pos at line_angle (as a 2000 pixel
// segment) enters and leaves a rotated rectangle: near is the crossing
// closer to the start of the segment. 0 if it misses.
HARNESS_CALLED i32 __stdcall collision_line_rect(Float2 *near_point, Float2 *far_point, Float3 *pos, f32 line_angle,
                                                 f32 rect_x, f32 rect_y, f32 w, f32 h, f32 rect_angle);

// 0x4049c0. Whether two rotated rectangles overlap.
HARNESS_CALLED i32 __stdcall collision_test_rect_rect(f32 x1, f32 y1, f32 w1, f32 h1, f32 angle1, f32 x2, f32 y2,
                                                      f32 w2, f32 h2, f32 angle2);
