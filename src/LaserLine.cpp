#include <math.h>

#include "AnmManager.h"
#include "Laser.h"
#include "Supervisor.h"

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
    if (lo.x > hit_length || lo.y > width / 2 || hi.x < 0.0f || hi.y < -width / 2)
    {
        return 0;
    }
    return 2;
}

// The same et_ex step as LaserCurveInf::method_3c.
// FUNCTION: TH16 0x432dc0
i32 LaserLineInf::method_3c()
{
    BulletExState *st = &ex_state[1];
    if (st->timer.current >= st->ints[0])
    {
        ex_flags &= ~BULLET_EX_ACCEL;
        return 1;
    }
    length += st->floats[0] * g_game_speed;
    Float3 v = *(Float3 *)&st->floats[5] * g_game_speed;
    D3DXVec3Add(&tip_offset, &tip_offset, &v);
    if (fabsf(tip_offset.x) > 0.0001f || fabsf(tip_offset.y) > 0.0001f)
    {
        angle = atan2(tip_offset.y, tip_offset.x);
    }
    st->timer.tick();
    return 0;
}
