#include "Laser.h"

// FUNCTION: TH16 0x435870
i32 LaserInfiniteInf::on_destroy()
{
    return 0;
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x435050
i32 LaserInfiniteInf::initialize(void *params)
{
    return unit5_placeholder(this);
}

// Runs the laser's pending et_ex transforms.
// TODO: in the blend mode case the original increments ex_index in memory (inc, reload) instead of from the loaded index.
// FUNCTION: TH16 0x436fd0
void LaserInfiniteInf::run_ex()
{
    while (ex_index < 0x12)
    {
        BulletEx *ex = &inner.ex[ex_index];
        if (ex->type == 0)
        {
            return;
        }
        if (ex->slot == 0 && ex_flags != 0)
        {
            return;
        }
        switch (ex->type)
        {
        case 0x80:
            countdown_5c8 = ex->a;
            break;
        case 0x400:
            state = 3;
            break;
        case 0x100000:
            if (ex->a != 0)
            {
                vm_950.flags_lo = vm_950.flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
            }
            else
            {
                vm_950.flags_lo &= ~ANM_VM_BLEND_MODE_MASK;
            }
            ex_index++;
            continue;
        }
        ex_index++;
    }
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x4352f0
i32 LaserInfiniteInf::on_tick()
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x4357a0
i32 LaserInfiniteInf::on_draw()
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x436010
i32 LaserInfiniteInf::method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x435880
i32 LaserInfiniteInf::cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e)
{
    return unit5_placeholder(this);
}

// 2 if a circle at pos touches the laser's rectangle, else 0.
// TODO: the original loads dx, dy and the sine into registers and multiplies by the cosine in xmm0; ours multiplies from memory.
// FUNCTION: TH16 0x436ef0
i32 LaserInfiniteInf::method_30(Float3 *pos, f32 radius)
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
// STUB: TH16 0x435610
i32 LaserInfiniteInf::check_graze_or_kill(i32 a)
{
    return unit5_placeholder(this);
}
