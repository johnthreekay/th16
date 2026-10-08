#include <math.h>

#include "PosVel.h"
#include "Supervisor.h"

// LTCG inlines this CRT header function at some call sites and not at
// others; in this file the original calls it.
DECOMP_NOINLINE float __CRTDECL atan2f(float, float);

// FUNCTION: TH16 0x402ff0
void PosVel::update_secondary_fields()
{
    switch (flags & POSVEL_MODE_MASK)
    {
    case POSVEL_MODE_VELOCITY:
        from_polar(&velocity, angle.value, speed * g_game_speed);
        break;
    case POSVEL_MODE_CIRCLE:
    case POSVEL_MODE_ELLIPSE:
        radial_dist += radial_speed * g_game_speed;
        angle.value = wrap_angle(angle.value + speed * g_game_speed);
        break;
    case POSVEL_MODE_WAVE:
        wave_angle += radial_speed * g_game_speed;
        from_polar(&velocity, ellipse_angle.value, speed * g_game_speed);
        velocity.z = 0.0f;
        break;
    }
}

// The angle difference is read through the operator's returned pointer
// (a temporary, not a named ZunAngle) and rotated.y is assigned before
// rotated.x, as the original's register use shows.
// TODO: with the six dead locals the circle and wave sums match, but
// pos += velocity now loads pos.x first where the original loads
// velocity.x (field-wise or D3DXVec3Add forms of it move the other cases).
// FUNCTION: TH16 0x403110
void PosVel::step()
{
    // Never used: six named locals give the x components of the circle and
    // wave sums the original's load order (offset.x first; the x order of a
    // vector op follows the function's named-variable count).
    i32 unused_0, unused_1, unused_2, unused_3, unused_4, unused_5;
    (void)unused_0, (void)unused_1, (void)unused_2, (void)unused_3, (void)unused_4, (void)unused_5;
    switch (flags & POSVEL_MODE_MASK)
    {
    case POSVEL_MODE_VELOCITY:
        pos += velocity;
        break;
    case POSVEL_MODE_CIRCLE:
    {
        Float3 offset;
        from_polar(&offset, angle.value, radial_dist);
        offset.z = 0.0f;
        pos = velocity + offset;
        break;
    }
    case POSVEL_MODE_ELLIPSE:
    {
        Float3 offset;
        from_polar(&offset, normalize_angle((angle - ellipse_angle).value), radial_dist);
        f32 x = ellipse_ratio * offset.x;
        f32 s = zun_sinf(ellipse_angle.value);
        f32 c = zun_cosf(ellipse_angle.value);
        Float3 rotated;
        rotated.y = offset.y * c + s * x;
        rotated.x = c * x - offset.y * s;
        rotated.z = 0.0f;
        pos = velocity + rotated;
        break;
    }
    case POSVEL_MODE_WAVE:
    {
        Float3 prev = pos;
        center += velocity;
        ZunAngle side;
        side.value = wrap_angle(ellipse_angle.value + ZUN_PI / 2);
        Float3 offset;
        from_polar(&offset, normalize_angle(side.value), zun_sinf(wave_angle.value) * radial_dist * g_game_speed);
        offset.z = 0.0f;
        pos = center + offset;
        angle = zun_atan2f(pos.y - prev.y, pos.x - prev.x);
        break;
    }
    }
    // Snap to hundredths of a pixel.
    pos.x = zun_floorf(pos.x * 100.0f) / 100.0f;
    pos.y = zun_floorf(pos.y * 100.0f) / 100.0f;
}

// FUNCTION: TH16 0x4033d0
void PosVel::step_from_center()
{
    if ((flags & POSVEL_MODE_MASK) == POSVEL_MODE_WAVE)
    {
        center = pos;
    }
    step();
}

// FUNCTION: TH16 0x411410
HARNESS_CALLED void PosVel::set_angle(f32 angle)
{
    this->angle.value = wrap_angle(angle);
}
