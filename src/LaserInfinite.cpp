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
            state = LASER_STATE_WARNING;
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
// The offset is a Float3 and y is computed before x: written as separate
// floats in the other order, ours multiplies d.x from memory.
// FUNCTION: TH16 0x436ef0
i32 LaserInfiniteInf::touches_circle(Float3 *pos, f32 radius)
{
    Float3 d = *pos - position;
    f32 a = -angle;
    f32 s = zun_sinf(a);
    f32 c = zun_cosf(a);
    f32 y = d.x * s + d.y * c;
    f32 x = d.x * c - d.y * s;
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
    case LASER_STATE_WARNING:
        if (timer.current >= inner.start_time)
        {
            timer.set_value(0);
            state = LASER_STATE_EXPANDING;
        }
        break;
    case LASER_STATE_EXPANDING:
        if (timer.current < inner.expand_time)
        {
            width = inner.laser_new_arg_4 * timer.current_f / inner.expand_time;
            break;
        }
        timer.set_value(0);
        state = LASER_STATE_ACTIVE;
        width = inner.laser_new_arg_4;
    case LASER_STATE_ACTIVE:
        if (timer.current < inner.duration)
        {
            break;
        }
        timer.set_value(0);
        state = LASER_STATE_SHRINKING;
    case LASER_STATE_SHRINKING:
        if (timer.current >= inner.shrink_time)
        {
            return 1;
        }
        width = inner.laser_new_arg_4 - timer.current_f * inner.laser_new_arg_4 / inner.shrink_time;
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
