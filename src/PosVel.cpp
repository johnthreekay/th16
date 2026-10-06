#include <math.h>

#include "PosVel.h"

// LTCG inlines these CRT header functions at some call sites and not at
// others; in this file the original calls them.
DECOMP_NOINLINE float __CRTDECL floorf(float);
DECOMP_NOINLINE float __CRTDECL sinf(float);
DECOMP_NOINLINE float __CRTDECL cosf(float);
DECOMP_NOINLINE float __CRTDECL atan2f(float, float);

// FUNCTION: TH16 0x402ff0
void PosVel::update_secondary_fields()
{
    switch (flags & 0xf)
    {
    case POSVEL_MODE_VELOCITY:
        velocity.from_polar(angle.value, speed * g_GameSpeed);
        break;
    case POSVEL_MODE_CIRCLE:
    case POSVEL_MODE_ELLIPSE:
        radial_dist += radial_speed * g_GameSpeed;
        angle.value = wrap_angle(angle.value + speed * g_GameSpeed);
        break;
    case POSVEL_MODE_WAVE:
        wave_angle += radial_speed * g_GameSpeed;
        velocity.from_polar(ellipse_angle.value, speed * g_GameSpeed);
        velocity.z = 0.0f;
        break;
    }
}

// FUNCTION: TH16 0x403110
// TODO: ours aligns the frame for sinf/cosf, adds pos+velocity the other way round, misses a tail merge.
void PosVel::step()
{
    switch (flags & 0xf)
    {
    case POSVEL_MODE_VELOCITY:
        pos += velocity;
        break;
    case POSVEL_MODE_CIRCLE:
    {
        Float3 offset;
        offset.from_polar(angle.value, radial_dist);
        offset.z = 0.0f;
        pos = velocity + offset;
        break;
    }
    case POSVEL_MODE_ELLIPSE:
    {
        Float3 offset;
        ZunAngle a = angle - ellipse_angle;
        offset.from_polar(normalize_angle(a.value), radial_dist);
        f32 x = ellipse_ratio * offset.x;
        f32 s = sinf(ellipse_angle.value);
        f32 c = cosf(ellipse_angle.value);
        Float3 rotated;
        rotated.x = c * x - offset.y * s;
        rotated.y = offset.y * c + s * x;
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
        offset.from_polar(normalize_angle(side.value), sinf(wave_angle.value) * radial_dist * g_GameSpeed);
        offset.z = 0.0f;
        pos = center + offset;
        angle = atan2f(pos.y - prev.y, pos.x - prev.x);
        break;
    }
    }
    // Snap to hundredths of a pixel.
    pos.x = floorf(pos.x * 100.0f) / 100.0f;
    pos.y = floorf(pos.y * 100.0f) / 100.0f;
}

// FUNCTION: TH16 0x4033d0
void PosVel::step_from_center()
{
    if ((flags & 0xf) == POSVEL_MODE_WAVE)
    {
        center = pos;
    }
    step();
}
