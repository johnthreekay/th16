#include "Laser.h"

// FUNCTION: TH16 0x431fa0
i32 __fastcall LaserLineInf::on_sprite_set(AnmVm *vm, i32 sprite)
{
    LaserLineInf *laser = (LaserLineInf *)vm->associated_game_entity;
    if (g_bullet_types[laser->bullet_type].sprites[0][0] >= 0)
    {
        return g_bullet_types[laser->bullet_type].sprites[laser->bullet_color][sprite];
    }
    return sprite;
}

// FUNCTION: TH16 0x433850
i32 LaserLineInf::on_destroy()
{
    return 0;
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x431b30
i32 LaserLineInf::initialize(void *params)
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x431fe0
void LaserLineInf::run_ex()
{
    unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x432f40
i32 LaserLineInf::on_tick()
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x433720
i32 LaserLineInf::on_draw()
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x434010
i32 LaserLineInf::method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x433860
i32 LaserLineInf::cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e)
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x434730
i32 LaserLineInf::cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 c, i32 d)
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x434cd0
i32 LaserLineInf::cancel(i32 mode, i32 b)
{
    return unit5_placeholder(this);
}

// 2 if a circle at pos touches the laser's rectangle, else 0.
// TODO: the original loads dx, dy and the sine into registers and multiplies by the cosine in xmm0; ours multiplies from memory.
// FUNCTION: TH16 0x434f70
i32 LaserLineInf::method_30(Float3 *pos, f32 radius)
{
    f32 dx = pos->x - position.x;
    f32 dy = pos->y - position.y;
    f32 a = -angle;
    f32 s = zun_sinf(a);
    f32 c = zun_cosf(a);
    f32 x = dx * c - dy * s;
    f32 y = dx * s + dy * c;
    D3DXVECTOR2 lo(x - radius, y - radius);
    D3DXVECTOR2 hi(x + radius, y + radius);
    if (lo.x > unk_70 || lo.y > width / 2 || hi.x < 0.0f || hi.y < -width / 2)
    {
        return 0;
    }
    return 2;
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x433510
i32 LaserLineInf::check_graze_or_kill(i32 a)
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x432dc0
i32 LaserLineInf::method_3c()
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x432c20
i32 LaserLineInf::method_44()
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x432620
i32 LaserLineInf::method_50()
{
    return unit5_placeholder(this);
}
