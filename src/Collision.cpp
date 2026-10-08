#include <math.h>

#include "Collision.h"
#include "ZunMath.h"

// Whether a circle touches a w x h rectangle centered on (rect_x, rect_y)
// and rotated by angle: the edges first, then the corners.
// TODO: the rotation multiplies into the sine and cosine registers with the circle offsets from memory (the original loads the offsets), and the stack slots differ.
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
    f32 abs_x = fabsf(x);
    if (half_w + radius >= abs_x && fabsf(y) <= h * 0.5f)
    {
        return 1;
    }
    if (half_w >= abs_x && fabsf(y) <= h * 0.5f + radius)
    {
        return 1;
    }
    // Then the corners.
    f32 half_h = h * 0.5f;
    f32 radius_sq = radius * radius;
    Float3 d;
    d.x = x - half_w;
    d.y = y - half_h;
    if (radius_sq > offset_length_sq(&d))
    {
        return 1;
    }
    d.x = x + half_w;
    d.y = y - half_h;
    if (radius_sq > offset_length_sq(&d))
    {
        return 1;
    }
    d.x = x - half_w;
    d.y = y + half_h;
    if (radius_sq > offset_length_sq(&d))
    {
        return 1;
    }
    d.x = x + half_w;
    d.y = y + half_h;
    return radius_sq > offset_length_sq(&d);
}

// The corners each edge of a rectangle joins.
// GLOBAL: TH16 0x490e90
const i32 g_rect_edges[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};

// Rotates four points about the origin. sinf and cosf are written inside
// the loop: MSVC still hoists them (the original calls them once before the
// loop) but keeps the loop rolled, with the array in memory and its /GS
// cookie; with the calls written before the loop it unrolls and scalarizes
// it.
static __forceinline void rotate_points(Float2 *points, f32 angle)
{
#pragma loop(no_vector)
    for (i32 i = 0; i < 4; i++, points++)
    {
        f32 s = sinf(angle);
        f32 c = cosf(angle);
        f32 x = points->x;
        f32 y = points->y;
        points->x = x * c - y * s;
        points->y = y * c + x * s;
    }
}

// The corners of a w x h rectangle centered on the origin.
static __forceinline void rect_corners(Float2 *corners, f32 w, f32 h)
{
    corners[0].x = w * -0.5f;
    corners[0].y = h * -0.5f;
    corners[1].x = w * -0.5f;
    corners[1].y = h * 0.5f;
    corners[2].x = w * 0.5f;
    corners[2].y = h * 0.5f;
    corners[3].x = w * 0.5f;
    corners[3].y = h * -0.5f;
}

// The difference of two angles, wrapped into [-pi, pi].
static __forceinline f32 angle_diff(f32 a, f32 b)
{
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
    return wrap_angle(d);
}

// Whether segments (x1, y1)-(x2, y2) and (x3, y3)-(x4, y4) touch.
static __forceinline i32 segments_cross(f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, f32 x4, f32 y4)
{
    f32 c1 = (y3 - y1) * (x1 - x2) + (x1 - x3) * (y1 - y2);
    f32 c2 = (y4 - y1) * (x1 - x2) + (x1 - x4) * (y1 - y2);
    if (c2 * c1 > 0.0f)
    {
        return 0;
    }
    if (c1 == 0.0f && c2 == 0.0f)
    {
        // On one line: compare the extents.
        f32 t;
        if (x1 > x2)
        {
            t = x1;
            x1 = x2;
            x2 = t;
            t = y1;
            y1 = y2;
            y2 = t;
        }
        if (x3 > x4)
        {
            t = x3;
            x3 = x4;
            x4 = t;
            t = y3;
            y3 = y4;
            y4 = t;
        }
        return x4 >= x1 && y4 >= y1 && x2 >= x3 && y2 >= y3;
    }
    f32 c3 = (x3 - x1) * (y3 - y4) + (y1 - y3) * (x3 - x4);
    f32 c4 = (x3 - x2) * (y3 - y4) + (y2 - y3) * (x3 - x4);
    return !(c3 * c4 > 0.0f);
}

// TODO: the rotation loop computes and stores the new x before the new y; the original computes y first and stores it first.
// FUNCTION: TH16 0x403a90
HARNESS_CALLED i32 __stdcall collision_test_points_in_rect(f32 x, f32 y, f32 w, f32 h, f32 angle, Float2 *points)
{
    Float2 p[4];
    for (i32 i = 0; i < 4; i++)
    {
        p[i].x = points[i].x - x;
        p[i].y = points[i].y - y;
    }
    if (angle != 0.0f)
    {
        angle = -angle;
        rotate_points(p, angle);
    }
    f32 half_w = w * 0.5f;
    if (half_w >= fabsf(p[0].x) && fabsf(p[0].y) <= h * 0.5f)
    {
        return 1;
    }
    if (half_w >= fabsf(p[1].x) && fabsf(p[1].y) <= h * 0.5f)
    {
        return 1;
    }
    if (half_w >= fabsf(p[2].x) && fabsf(p[2].y) <= h * 0.5f)
    {
        return 1;
    }
    if (half_w >= fabsf(p[3].x) && fabsf(p[3].y) <= h * 0.5f)
    {
        return 1;
    }
    return 0;
}

// TODO: same tests and formulas; register allocation and stack slots differ (the original spills x4 - x3 into the x4 argument slot).
// FUNCTION: TH16 0x403ec0
HARNESS_CALLED i32 __stdcall collision_segment_intersection(f32 *out_x, f32 *out_y, f32 x1, f32 y1, f32 x2, f32 y2,
                                                            f32 x3, f32 y3, f32 x4, f32 y4)
{
    if (!segments_cross(x1, y1, x2, y2, x3, y3, x4, y4))
    {
        return 0;
    }
    f32 slope1;
    f32 intercept1;
    i32 vertical1;
    f32 dx1 = x2 - x1;
    if (fabsf(dx1) < 0.01f)
    {
        slope1 = 0.0f;
        intercept1 = x1;
        vertical1 = 1;
    }
    else
    {
        vertical1 = 0;
        slope1 = (y2 - y1) / dx1;
        intercept1 = y1 - (y2 - y1) * x1 / dx1;
    }
    f32 slope2;
    f32 intercept2;
    i32 vertical2;
    f32 dx2 = x4 - x3;
    if (fabsf(dx2) < 0.01f)
    {
        slope2 = 0.0f;
        intercept2 = x3;
        vertical2 = 1;
    }
    else
    {
        vertical2 = 0;
        slope2 = (y4 - y3) / dx2;
        intercept2 = y3 - (y4 - y3) * x3 / dx2;
    }
    if (!vertical1)
    {
        if (!vertical2)
        {
            *out_x = (intercept2 - intercept1) / (slope1 - slope2);
            *out_y = (intercept2 - intercept1) * slope1 / (slope1 - slope2) + intercept1;
            return 1;
        }
        *out_x = x3;
        *out_y = slope1 * x3 + intercept1;
        return 1;
    }
    if (vertical2)
    {
        if (fabsf(x1 - x3) < 0.001f)
        {
            *out_x = x1;
            *out_y = y1;
            return 1;
        }
        return 0;
    }
    *out_x = x1;
    *out_y = slope2 * x1 + intercept2;
    return 1;
}

// FUNCTION: TH16 0x404220
HARNESS_CALLED i32 __stdcall collision_line_intersection(f32 *out_x, f32 *out_y, f32 x1, f32 y1, f32 angle1, f32 x2,
                                                         f32 y2, f32 angle2)
{
    f32 a1 = wrap_angle(angle1);
    f32 a2 = wrap_angle(angle2);
    if (fabsf(angle_diff(a1, a2)) < 0.001f || fabsf(angle_diff(angle_diff(a1, a2), ZUN_PI)) < 0.001f)
    {
        *out_y = y1;
        *out_x = x1;
        return 0;
    }
    Float3 dir;
    f32 slope1;
    f32 intercept1;
    i32 vertical1;
    from_polar(&dir, angle1, 10.0f);
    if (fabsf(dir.x) < 0.01f)
    {
        intercept1 = x1;
        slope1 = 0.0f;
        vertical1 = 1;
    }
    else
    {
        vertical1 = 0;
        slope1 = dir.y / dir.x;
        intercept1 = y1 - slope1 * x1;
    }
    f32 slope2;
    f32 intercept2;
    i32 vertical2;
    from_polar(&dir, angle2, 10.0f);
    if (fabsf(dir.x) < 0.01f)
    {
        slope2 = 0.0f;
        vertical2 = 1;
        intercept2 = x2;
    }
    else
    {
        vertical2 = 0;
        slope2 = dir.y / dir.x;
        intercept2 = y2 - slope2 * x2;
    }
    if (!vertical1)
    {
        if (!vertical2)
        {
            *out_x = (intercept2 - intercept1) / (slope1 - slope2);
            *out_y = (intercept2 - intercept1) * slope1 / (slope1 - slope2) + intercept1;
            return 1;
        }
        *out_x = x2;
        *out_y = slope1 * x2 + intercept1;
        return 1;
    }
    if (vertical2)
    {
        if (fabsf(x1 - x2) < 0.001f)
        {
            *out_y = y1;
            *out_x = x1;
            return 1;
        }
        return 0;
    }
    *out_x = x1;
    *out_y = slope2 * x1 + intercept2;
    return 1;
}

// TODO: same logic; the rotation loop stores x before y and the register allocation after the rotation differs.
// FUNCTION: TH16 0x404600
HARNESS_CALLED i32 __stdcall collision_line_rect(Float2 *near_point, Float2 *far_point, Float3 *pos, f32 line_angle,
                                                 f32 rect_x, f32 rect_y, f32 w, f32 h, f32 rect_angle)
{
    Float2 corners[4];
    Float3 hits[2];
    rect_corners(corners, w, h);
    // Tests the line's angle, but turns by the rectangle's.
    if (line_angle != 0.0f)
    {
        rotate_points(corners, rect_angle);
    }
    for (i32 i = 0; i < 4; i++)
    {
        corners[i].x += rect_x;
        corners[i].y += rect_y;
    }
    f32 s = sinf(line_angle);
    f32 c = cosf(line_angle);
    f32 dx = 1000.0f * c - 0.0f * s;
    f32 dy = 0.0f * c + 1000.0f * s;
    f32 end_x = pos->x + dx;
    f32 end_y = pos->y + dy;
    f32 start_x = pos->x - dx;
    f32 start_y = pos->y - dy;
    i32 n = 0;
    for (const i32(*edge)[2] = g_rect_edges; edge < g_rect_edges + 4; edge++)
    {
        if (collision_segment_intersection(&hits[n].x, &hits[n].y, start_x, start_y, end_x, end_y,
                                           corners[(*edge)[0]].x, corners[(*edge)[0]].y, corners[(*edge)[1]].x,
                                           corners[(*edge)[1]].y))
        {
            n++;
            if (n >= 2)
            {
                break;
            }
        }
    }
    if (n == 0)
    {
        return 0;
    }
    if (n == 2)
    {
        if ((start_x - hits[1].x) * (start_x - hits[1].x) + (start_y - hits[1].y) * (start_y - hits[1].y) >
            (start_x - hits[0].x) * (start_x - hits[0].x) + (start_y - hits[0].y) * (start_y - hits[0].y))
        {
            near_point->x = hits[0].x;
            near_point->y = hits[0].y;
            far_point->x = hits[1].x;
            far_point->y = hits[1].y;
        }
        else
        {
            near_point->x = hits[1].x;
            near_point->y = hits[1].y;
            far_point->x = hits[0].x;
            far_point->y = hits[0].y;
        }
    }
    else
    {
        near_point->x = hits[0].x;
        near_point->y = hits[0].y;
        far_point->x = hits[0].x;
        far_point->y = hits[0].y;
    }
    return 1;
}

// TODO: same logic; the corner arrays and registers are allocated differently and the rotation loops store x before y.
// FUNCTION: TH16 0x4049c0
HARNESS_CALLED i32 __stdcall collision_test_rect_rect(f32 x1, f32 y1, f32 w1, f32 h1, f32 angle1, f32 x2, f32 y2,
                                                      f32 w2, f32 h2, f32 angle2)
{
    Float2 corners2[4];
    Float2 corners1[4];
    Float2 rel[4];
    f32 half_w1 = w1 * 0.5f;
    f32 half_h1 = h1 * 0.5f;
    f32 half_w2 = w2 * 0.5f;
    f32 half_h2 = h2 * 0.5f;
    // Bounding circles first.
    f32 distance = sqrtf((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
    if (distance >= sqrtf(half_h1 * half_h1 + half_w1 * half_w1) + sqrtf(half_w2 * half_w2 + half_h2 * half_h2))
    {
        return 0;
    }
    rect_corners(corners1, w1, h1);
    if (angle1 != 0.0f)
    {
        rotate_points(corners1, angle1);
    }
    rect_corners(corners2, w2, h2);
    if (angle2 != 0.0f)
    {
        rotate_points(corners2, angle2);
    }
    for (i32 i = 0; i < 4; i++)
    {
        corners2[i].x += x2;
        corners2[i].y += y2;
        corners1[i].x += x1;
        corners1[i].y += y1;
    }
    // A corner of the second inside the first?
    for (i32 i = 0; i < 4; i++)
    {
        rel[i].x = corners2[i].x - x1;
        rel[i].y = corners2[i].y - y1;
    }
    if (angle1 != 0.0f)
    {
        angle1 = -angle1;
        rotate_points(rel, angle1);
    }
    for (i32 i = 0; i < 4; i++)
    {
        if (half_w1 >= fabsf(rel[i].x) && half_h1 >= fabsf(rel[i].y))
        {
            return 1;
        }
    }
    // Of the first inside the second?
    if (collision_test_points_in_rect(x2, y2, w2, h2, angle2, corners1))
    {
        return 1;
    }
    // Crossing edges.
    for (const i32(*e1)[2] = g_rect_edges; e1 < g_rect_edges + 4; e1++)
    {
        for (const i32(*e2)[2] = g_rect_edges; e2 < g_rect_edges + 4; e2++)
        {
            if (segments_cross(corners1[(*e1)[0]].x, corners1[(*e1)[0]].y, corners1[(*e1)[1]].x,
                               corners1[(*e1)[1]].y, corners2[(*e2)[0]].x, corners2[(*e2)[0]].y,
                               corners2[(*e2)[1]].x, corners2[(*e2)[1]].y))
            {
                return 1;
            }
        }
    }
    return 0;
}
