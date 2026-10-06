#pragma once

#include <d3dx9math.h>

#include "BulletManager.h"
#include "types.h"

// Parameters of an infinite laser, filled in by ECL before the laser is
// created. Layout from ExpHP (zLaserInfiniteInner); his field names say
// which BulletManager shooter field each one comes from.
struct LaserInfiniteInner
{
    D3DXVECTOR3 start_pos;
    u8 unk_c[0x18 - 0xc];
    f32 ang_aim;
    f32 laser_st_rotation;
    f32 laser_new_arg_2;
    f32 laser_new_arg_1;
    f32 laser_new_arg_4;
    f32 spd1;
    i32 unk_30;
    i32 unk_34;
    i32 unk_38;
    i32 unk_3c;
    i32 shot_sfx;
    i32 shot_transform_sfx;
    i32 laser_st_on_arg_1;
    f32 distance;
    u8 unk_50[4];
    i32 type;
    i32 color;
    u32 flags;
    BulletEx ex[0x12];

    LaserInfiniteInner();
};
