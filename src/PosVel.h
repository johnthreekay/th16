#pragma once

#include "ZunAngle.h"
#include "ZunMath.h"
#include "types.h"

enum PosVelMode
{
    // pos += velocity.
    POSVEL_MODE_VELOCITY = 0,
    POSVEL_MODE_NONE = 1,
    // Circle of radius radial_dist around velocity (used as the center).
    POSVEL_MODE_CIRCLE = 2,
    // Ellipse: the circle squashed along x by ellipse_ratio, then rotated
    // by ellipse_angle.
    POSVEL_MODE_ELLIPSE = 3,
    // Moves along center, which itself moves by velocity, swinging from
    // side to side as wave_angle advances.
    POSVEL_MODE_WAVE = 4,
};

// Position and motion of an enemy or bullet-like object. Layout from
// ExpHP's th-re-data (zPosVel).
struct PosVel
{
    Float3 pos;
    Float3 center;
    f32 speed;
    ZunAngle angle;
    f32 radial_dist;
    f32 radial_speed;
    ZunAngle ellipse_angle;
    f32 ellipse_ratio;
    ZunAngle wave_angle;
    Float3 velocity;
    // Low 4 bits: PosVelMode.
    u32 flags;

    void update_secondary_fields();
    void step();
    void step_from_center();
    // 0x4260d0. LTCG passes the angle in xmm1.
    HARNESS_CALLED void set_ellipse_angle(f32 angle);
    // 0x411410. LTCG passes the angle in xmm1.
    HARNESS_CALLED void set_angle(f32 angle);
};
