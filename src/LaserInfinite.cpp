#include "AnmManager.h"
#include "Globals.h"
#include "Supervisor.h"
#include "Enemy.h"
#include "EnemyManager.h"
#include "Laser.h"

// FUNCTION: TH16 0x435870
i32 LaserInfiniteInf::on_destroy()
{
    return 0;
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
        case BULLET_EX_INVULN:
            ex_invuln_remaining_frames = ex->a;
            break;
        case BULLET_EX_DELETE:
            state = 3;
            break;
        case BULLET_EX_BLEND:
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
    if (lo.x > hit_length || lo.y > width / 2 || hi.x < 0.0f || hi.y < -width / 2)
    {
        return 0;
    }
    return 2;
}


// One frame: et_ex, growth and rotation, following the boss (flag 1),
// moving, then the warning (3), grow (4), full (2) and fade (5) states.
// Nonzero once the laser is done.
// TODO: the original realigns the frame and saves esi/edi, and its find_enemy_by_id loop keeps a NULL in ecx.
// FUNCTION: TH16 0x4352f0
i32 LaserInfiniteInf::on_tick()
{
    run_ex();
    if (ex_flags != 0)
    {
        if ((i32)ex_flags < 0)
        {
            if (ex_state[5].timer.current <= 0)
            {
                ex_flags ^= BULLET_EX_WAIT;
            }
            else
            {
                ex_state[5].timer--;
            }
        }
        if (ex_invuln_remaining_frames != 0)
        {
            ex_invuln_remaining_frames--;
        }
    }
    if (hit_length < inner.laser_new_arg_2)
    {
        hit_length = length * g_game_speed + hit_length;
        if (hit_length > inner.laser_new_arg_2)
        {
            hit_length = inner.laser_new_arg_2;
        }
    }
    i32 i = 0;
    f32 a = inner.laser_st_rotation * g_game_speed + angle;
    while (a > ZUN_PI)
    {
        a -= ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    while (a < -ZUN_PI)
    {
        a += ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    angle = a;
    if (inner.flags & 1)
    {
        EnemyManager *mgr = g_EnemyManager;
        if (mgr->find_enemy_by_id(mgr->inner.boss_ids[0]) != NULL)
        {
            position = g_EnemyManager->get_boss(0)->enemy.final_pos.pos;
        }
    }
    position.x = inner.velocity.x * g_game_speed + position.x;
    position.y = position.y + inner.velocity.y * g_game_speed;
    position.z = position.z + inner.velocity.z * g_game_speed;
    switch (state)
    {
    case 3:
        if (timer.current >= inner.unk_30)
        {
            timer.set_value(0);
            state = 4;
        }
        break;
    case 4:
        if (timer.current < inner.unk_34)
        {
            width = inner.laser_new_arg_4 * timer.current_f / inner.unk_34;
            break;
        }
        timer.set_value(0);
        state = 2;
        width = inner.laser_new_arg_4;
    case 2:
        if (timer.current < inner.unk_38)
        {
            break;
        }
        timer.set_value(0);
        state = 5;
    case 5:
        if (timer.current >= inner.unk_3c)
        {
            return 1;
        }
        width = inner.laser_new_arg_4 - timer.current_f * inner.laser_new_arg_4 / inner.unk_3c;
        break;
    }
    check_graze_or_kill(0);
    AnmVm *vm = &vm_950;
    vm->flags_lo |= ANM_VM_SCALE_CHANGED;
    vm->scale.x = width / g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].sprite_width;
    vm->flags_lo |= ANM_VM_SCALE_CHANGED;
    vm->scale.y = hit_length / g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].sprite_height;
    vm->run();
    if (unk_7c == 0.0f)
    {
        vm_f4c.run();
    }
    return 0;
}
