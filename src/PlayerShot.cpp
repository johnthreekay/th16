// Shot type callbacks: the functions a .sht file's shooters name by index
// (g_sht_*_funcs), called with the bullet in ecx.
#include <math.h>

#include <dsound.h>

#include "Player.h"

#include "EffectManager.h"
#include "Enemy.h"
#include "EnemyManager.h"
#include "Collision.h"
#include "Globals.h"
#include "Gui.h"
#include "Rng.h"
#include "SoundManager.h"

i32 __fastcall sht_on_init_445ed0(PlayerBullet *bullet);
i32 __fastcall sht_on_init_446200(PlayerBullet *bullet);
i32 __fastcall sht_on_init_4470e0(PlayerBullet *bullet);
i32 __fastcall sht_on_init_447450(PlayerBullet *bullet);
i32 __fastcall sht_on_init_4474d0(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_445ee0(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_446260(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_446e00(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_4470f0(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_447480(PlayerBullet *bullet);
i32 __fastcall sht_on_hit_4460c0(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall sht_on_hit_446870(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall sht_on_hit_446e20(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall sht_on_hit_446f80(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall sht_on_hit_447270(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall sht_on_hit_447320(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall damage_source_on_hit_445d40(PlayerDamageSource *source, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall damage_source_on_hit_4474a0(PlayerDamageSource *source, i32 unk, i32 enemy, f32 x, f32 y);

// GLOBAL: TH16 0x4919c0
ShtBulletFunc const g_sht_on_init_funcs[7] = {
    NULL,
    sht_on_init_445ed0,
    sht_on_init_446200,
    sht_on_init_4470e0,
    sht_on_init_447450,
    sht_on_init_4474d0,
    NULL,
};

// GLOBAL: TH16 0x4919a0
ShtBulletFunc const g_sht_on_tick_funcs[8] = {
    NULL,
    sht_on_tick_445ee0,
    sht_on_tick_446260,
    sht_on_tick_446e00,
    sht_on_tick_4470f0,
    sht_on_tick_447480,
    NULL,
    NULL,
};

// GLOBAL: TH16 0x491980
ShtHitFunc const g_sht_on_hit_funcs[8] = {
    NULL,
    sht_on_hit_4460c0,
    sht_on_hit_446870,
    sht_on_hit_446e20,
    sht_on_hit_446f80,
    sht_on_hit_447270,
    sht_on_hit_447320,
    NULL,
};

// Nothing uses the third callback; its table has only the empty entry.
// GLOBAL: TH16 0x4a6f04
ShtBulletFunc g_sht_func_3_table[1];

// GLOBAL: TH16 0x4919dc
DamageSourceHitFunc const g_damage_source_hit_funcs[4] = {
    NULL,
    damage_source_on_hit_445d40,
    damage_source_on_hit_4474a0,
    NULL,
};

// Homing: turns toward the nearest enemy, slowing down while the turn is
// sharp and speeding up once it is on course.
// FUNCTION: TH16 0x445ee0
i32 __fastcall sht_on_tick_445ee0(PlayerBullet *bullet)
{
    if (bullet->state == 2)
    {
        return 0;
    }
    EnemyRef *target = (EnemyRef *)&bullet->unk_90;
    if (g_EnemyManager == NULL)
    {
        target->id = 0;
    }
    else if (target->id == 0)
    {
        Float3 pos = bullet->pos.pos;
        *target = g_EnemyManager->find_closest(&pos, 256.0f);
    }
    if (bullet->unk_90 != 0)
    {
        if (!g_EnemyManager->is_enemy_alive(bullet->unk_90))
        {
            bullet->unk_90 = 0;
        }
        else
        {
            EnemyInf *enemy = target->get();
            if (!(enemy->enemy.flags_low & 0xc000021))
            {
                f32 angle = atan2f(enemy->enemy.final_pos.pos.y - bullet->pos.pos.y,
                                   enemy->enemy.final_pos.pos.x - bullet->pos.pos.x);
                f32 current = bullet->pos.angle.value;
                f32 diff;
                if (angle - current > ZUN_PI)
                {
                    diff = angle - (current + ZUN_2PI);
                }
                else if (current - angle > ZUN_PI)
                {
                    diff = angle - (current - ZUN_2PI);
                }
                else
                {
                    diff = angle - current;
                }
                f32 speed = bullet->pos.speed;
                if (bullet->timer_c.current < 60)
                {
                    f32 turn = (f32)fabs(diff);
                    if (turn >= ZUN_PI / 4)
                    {
                        f32 slower = speed - 0.2f;
                        speed = 4.0f > slower ? 4.0f : slower;
                    }
                    else if (turn < ZUN_PI / 12)
                    {
                        f32 faster = speed + 0.2f;
                        speed = 16.0f < faster ? 16.0f : faster;
                    }
                    bullet->pos.set_angle((bullet->pos.angle + diff * 0.08f).value);
                    bullet->pos.speed = speed;
                }
                else
                {
                    bullet->pos.speed = speed + 0.2f;
                }
                return 0;
            }
        }
    }
    f32 faster = bullet->pos.speed + 0.1f;
    bullet->pos.speed = 16.0f < faster ? 16.0f : faster;
    return 0;
}

// FUNCTION: TH16 0x445ed0
i32 __fastcall sht_on_init_445ed0(PlayerBullet *bullet)
{
    bullet->unk_90 = 0;
    return 0;
}

// FUNCTION: TH16 0x446200
i32 __fastcall sht_on_init_446200(PlayerBullet *bullet)
{
    Player *player = g_Player;
    bullet->unk_a0 = 0;
    g_SoundManager.play_sound_at_position(0x14, player->inner.pos.x);
    // The original reuses g_Player across the sound call, which LTCG knows
    // leaves it alone; with the sound code still a placeholder, spell that out.
    PlayerDamageSource *source =
        bullet->damage_source_index == 0 ? NULL : &player->inner.damage_sources[bullet->damage_source_index - 1];
    source->unk_80 = 1;
    source->unk_14 = 0.0f;
    bullet->flags &= ~1;
    return 0;
}

// FUNCTION: TH16 0x446e00
i32 __fastcall sht_on_tick_446e00(PlayerBullet *bullet)
{
    if (bullet->state == 1)
    {
        bullet->pos.speed += 1.0f;
    }
    return 0;
}

// FUNCTION: TH16 0x4470e0
i32 __fastcall sht_on_init_4470e0(PlayerBullet *bullet)
{
    bullet->flags &= ~0x3c;
    bullet->unk_90 = 0;
    return 0;
}

// Waits for an enemy in the same row, then stops and flies at it
// sideways.
// TODO: ours gets a /GS cookie for pos and merges the flags_low & 1 test into the 0xc000021 one.
// FUNCTION: TH16 0x4470f0
i32 __fastcall sht_on_tick_4470f0(PlayerBullet *bullet)
{
    if (bullet->state == 2)
    {
        return 0;
    }
    if (!(bullet->flags & 0x3c))
    {
        EnemyManager *mgr = g_EnemyManager;
        if (mgr == NULL)
        {
            bullet->unk_90 = 0;
        }
        else if (bullet->unk_90 == 0)
        {
            mgr->unk_15c = mgr->active_enemy_list_head;
            EnemyInf *enemy = mgr->unk_15c->entry;
            Float3 pos = bullet->pos.pos;
            while (enemy != NULL)
            {
                if (!(enemy->enemy.flags_low & 1) && !(enemy->enemy.flags_low & 0xc000021) &&
                    pos.y >= enemy->enemy.final_pos.pos.y - 16.0f && enemy->enemy.final_pos.pos.y + 16.0f >= pos.y &&
                    (enemy->enemy.final_pos.pos.x - 16.0f >= pos.x || pos.x >= enemy->enemy.final_pos.pos.x + 16.0f))
                {
                    bullet->flags = (bullet->flags & ~0x38) | 4;
                    AnmManager::interrupt_tree(bullet->anm_id, 2);
                    bullet->timer_20.set_value(0);
                    bullet->pos.speed = 0.0f;
                    bullet->target_pos = enemy->enemy.final_pos.pos;
                    break;
                }
                mgr->unk_15c = mgr->unk_15c->next;
                enemy = mgr->unk_15c != NULL ? mgr->unk_15c->entry : NULL;
            }
        }
    }
    if ((bullet->flags & 0x3c) == 4)
    {
        if (bullet->timer_20.current == 4)
        {
            bullet->pos.set_angle(bullet->pos.pos.x > bullet->target_pos.x ? -ZUN_PI : 0.0f);
            bullet->pos.speed = 14.0f;
            bullet->flags = (bullet->flags & ~0x34) | 8;
        }
        bullet->timer_20++;
    }
    return 0;
}

// FUNCTION: TH16 0x447450
i32 __fastcall sht_on_init_447450(PlayerBullet *bullet)
{
    bullet->damage_source()->unk_90 = 2;
    return 0;
}

// FUNCTION: TH16 0x447480
i32 __fastcall sht_on_tick_447480(PlayerBullet *bullet)
{
    if (bullet->state == 1)
    {
        bullet->pos.speed += 0.5f;
    }
    return 0;
}

// TODO: register allocation: the original loads the player into ecx and
// the scaled index into edx, reading the old value straight into eax.
// FUNCTION: TH16 0x4474a0
i32 __fastcall damage_source_on_hit_4474a0(PlayerDamageSource *source, i32 unk, i32 enemy, f32 x, f32 y)
{
    i32 index = source->bullet_index;
    Player *player = g_Player;
    i32 was_hit = player->inner.bullets[index].unk_9c;
    player->inner.bullets[index].unk_9c = 1;
    return was_hit;
}

// FUNCTION: TH16 0x4474d0
i32 __fastcall sht_on_init_4474d0(PlayerBullet *bullet)
{
    bullet->pos.angle.value = wrap_angle(bullet->pos.angle.value +
                                         g_replay_safe_rng.randf_neg_1_to_1() * (ZUN_PI / 180.0f) * 15.0f);
    return 0;
}

// TODO: the original computes the shooter twice from scratch (keeping ref in
// ebx); ours shares the common parts and spills them.
// FUNCTION: TH16 0x445d40
i32 __fastcall damage_source_on_hit_445d40(PlayerDamageSource *source, i32 unk, i32 enemy, f32 x, f32 y)
{
    Player *player = g_Player;
    i32 ref = player->inner.bullets[source->bullet_index].shooter_ref;
    if (player->get_shooter(ref)->func_on_hit != NULL)
    {
        return player->get_shooter(ref)->func_on_hit(&player->inner.bullets[source->bullet_index], unk, enemy, x,
                                                      y);
    }
    return player->inner.bullets[source->bullet_index].hit();
}

// FUNCTION: TH16 0x445e20
i32 PlayerBullet::hit()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    pos.pos.z = 0.1f;
    vm->interrupt(1);
    pos.speed *= 0.125f;
    state = 2;
    vm->entity_pos = pos.pos;
    damage_source()->flags &= ~1;
    i32 result = unk_9c;
    damage_source_index = 0;
    return result;
}

// TODO: the original pushes interrupt_tree's 1 between the stores to the new
// damage source; ours pushes it first.
// FUNCTION: TH16 0x446e20
i32 __fastcall sht_on_hit_446e20(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y)
{
    i32 damage = g_Player->get_shooter(bullet->shooter_ref)->damage;
    PlayerDamageSource *source =
        g_Player->get_damage_source(g_Player->create_damage_source(&bullet->pos.pos, 24.0f, 1.0f, 0x14, damage));
    source->unk_80 = 4;
    source->pos.speed = 0.3f;
    source->pos.angle.value = -ZUN_PI / 2;
    AnmManager::interrupt_tree(bullet->anm_id, 1);
    bullet->state = 2;
    source = bullet->damage_source();
    source->flags &= ~1;
    bullet->damage_source_index = 0;
    bullet->pos.speed = 0.3f;
    source->pos = bullet->pos;
    g_SoundManager.play_sound_at_position(0x41, bullet->pos.pos.x);
    return bullet->unk_9c;
}

// A tinted effect pointing back the way the bullet came, give or take 20
// degrees.
// FUNCTION: TH16 0x4460c0
i32 __fastcall sht_on_hit_4460c0(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y)
{
    f32 angle = wrap_angle(bullet->pos.angle.value + g_replay_unsafe_rng.randf_neg_1_to_1() * 0.34906584f);
    angle = wrap_angle(angle + ZUN_PI);
    AnmId id = g_EffectManager->effect_anm->create_vm(0x98, &bullet->pos.pos, angle, -1, 0);
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    vm->color_1.r = (g_replay_unsafe_rng.rand_u32() & 0x7f) + 0x7f;
    vm->color_1.g = (g_replay_unsafe_rng.rand_u32() & 0x3f) + 0x40;
    vm->color_1.b = (g_replay_unsafe_rng.rand_u32() & 0x3f) + 0x40;
    vm->color_1.a = (g_replay_unsafe_rng.rand_u32() & 0x3f) + 0x60;
    return bullet->hit();
}

// Turns the effect up to 20 degrees either way.
// FUNCTION: TH16 0x447270
i32 __fastcall sht_on_hit_447270(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y)
{
    f32 angle = wrap_angle(bullet->pos.angle.value + g_replay_unsafe_rng.randf_neg_1_to_1() * 0.34906584f);
    g_EffectManager->effect_anm->create_vm(0x98, &bullet->pos.pos, angle, -1, 0);
    return bullet->hit();
}

// TODO: register allocation: the original keeps the player in edi and the
// bullet in esi throughout (with an unused stack slot); ours reloads the
// player for create_damage_source.
// FUNCTION: TH16 0x446f80
i32 __fastcall sht_on_hit_446f80(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y)
{
    Player *player = g_Player;
    i32 damage = player->get_shooter(bullet->shooter_ref)->damage;
    i32 index = player->create_damage_source(&bullet->pos.pos, 24.0f, 2.0f, 0x14, damage);
    PlayerDamageSource *source = index != 0 ? &player->inner.damage_sources[index - 1] : NULL;
    source->unk_80 = 4;
    AnmManager::interrupt_tree(bullet->anm_id, 1);
    bullet->state = 2;
    bullet->pos.speed = 2.0f;
    source->pos = bullet->pos;
    bullet->damage_source()->flags &= ~1;
    bullet->damage_source_index = 0;
    g_SoundManager.play_sound_at_position(0x41, bullet->pos.pos.x);
    return bullet->unk_9c;
}

// TODO: the original aligns its frame to 8 bytes (and esp, -8) and
// addresses its locals through esp.
// FUNCTION: TH16 0x447320
i32 __fastcall sht_on_hit_447320(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y)
{
    f32 angle = wrap_angle(bullet->pos.angle.value + g_replay_unsafe_rng.randf_neg_1_to_1() * 0.34906584f);
    AnmId id = g_EffectManager->effect_anm->create_vm(0x98, &bullet->pos.pos, angle, -1, 0);
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    D3DXVECTOR2 initial(1.0f, 1.0f);
    D3DXVECTOR2 goal(3.0f, 3.0f);
    vm->set_scale_interp(0x14, 0, &initial, &goal);
    vm->color_1.r = (g_replay_unsafe_rng.rand_u32() & 0x7f) + 0x7f;
    vm->color_1.g = (g_replay_unsafe_rng.rand_u32() & 0x7f) + 0x40;
    vm->color_1.b = (g_replay_unsafe_rng.rand_u32() & 0x3f) + 0x40;
    return bullet->hit();
}

// This file's copy of ZunMath.h's sincosmul.
// FUNCTION: TH16 0x4476b0
static void __fastcall player_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}

// ZunAngle's subtraction (shortest signed difference) as the laser code
// inlines it, without the final wrap.
static __forceinline f32 angle_sub_unwrapped(f32 a, f32 b)
{
    if (a - b > ZUN_PI)
    {
        return a - (b + ZUN_2PI);
    }
    else if (b - a > ZUN_PI)
    {
        return a - (b - ZUN_2PI);
    }
    return a - b;
}

// ZunAngle's operators as the laser code inlines them.
static __forceinline ZunAngle angle_sub(const ZunAngle &a, const ZunAngle &b)
{
    ZunAngle result;
    result.value = wrap_angle(angle_sub_unwrapped(a.value, b.value));
    return result;
}

static __forceinline ZunAngle angle_mul(const ZunAngle &a, f32 factor)
{
    ZunAngle result;
    result.value = wrap_angle(a.value * factor);
    return result;
}

static __forceinline ZunAngle angle_add(const ZunAngle &a, const ZunAngle &b)
{
    ZunAngle result;
    result.value = wrap_angle(a.value + b.value);
    return result;
}

// The position of the option a shooter names (option number minus one).
static __forceinline Int2 *option_pos(Player *player, i32 index)
{
    return index >= 100 ? &player->inner.subseason_options[index - 100].scaled_cur_pos
                        : &player->inner.main_options[index].scaled_cur_pos;
}

// The index into Player::inner.option_lasers of a shooter's option.
static __forceinline i32 option_laser_index(ShtShooter *shooter, i32 shooter_ref)
{
    i32 option = (i8)shooter->option - 1;
    if (option >= 100)
    {
        option -= 100;
    }
    return ((shooter_ref & 0xf0000) != 0) * 8 + option;
}

// Marisa's laser: follows its option, turns toward the shot angle and
// grows up to 512 pixels; it ends once the shot key is released, the
// option is gone or the power level changed.
// FUNCTION: TH16 0x446260
i32 __fastcall sht_on_tick_446260(PlayerBullet *bullet)
{
    ShtShooter *shooter = g_Player->get_shooter(bullet->shooter_ref);
    Int2 *option = option_pos(g_Player, (i8)shooter->option - 1);
    Float3 pos(option->x / 128.0f, option->y / 128.0f, 0.0f);
    bullet->pos.pos = pos;
    ZunAngle current = bullet->pos.angle;
    ZunAngle target;
    target.value = wrap_angle(g_Player->inner.is_focused ? -ZUN_PI / 2 : shooter->angle);
    if (fabs(angle_sub(current, target).value) < 0.001f)
    {
        bullet->pos.angle.value = wrap_angle(angle_add(current, angle_mul(angle_sub(target, current), 0.1f)).value);
    }
    else
    {
        bullet->pos.angle.value = wrap_angle(target.value);
    }
    if (bullet->state == 2)
    {
        return 0;
    }
    g_SoundManager.sound_buffers[20].buffer->SetPan((i32)(g_Player->inner.pos.x * 1000.0f / 192.0f));
    if (bullet->laser_length < 512.0f)
    {
        bullet->laser_length += 18.0f;
        bullet->damage_source()->unk_14 = bullet->laser_length;
    }
    option = option_pos(g_Player, (i8)shooter->option - 1);
    f32 x = option->x / 128.0f;
    f32 y = option->y / 128.0f;
    Float3 offset;
    player_sincosmul(&offset, bullet->pos.angle.value, bullet->laser_length * 0.5f);
    pos.x = offset.x + x;
    pos.y = offset.y + y;
    pos.z = 0.0f;
    if (get_vm_or_clear(bullet->anm_id) != NULL)
    {
        AnmVm *vm = get_vm_or_clear(bullet->anm_id);
        vm->flags_lo |= ANM_VM_SCALE_CHANGED;
        vm->sprite_size.x = bullet->laser_length;
        vm = get_vm_or_clear(bullet->anm_id);
        vm->flags_lo |= ANM_VM_UV_SCALE_CHANGED;
        vm->uv_scale.x = bullet->laser_length / 512.0f;
    }
    bullet->damage_source()->pos.pos = pos;
    if (bullet->unk_94 == 0 && bullet->state == 1 && bullet->unk_98 == 1)
    {
        AnmManager::interrupt_tree(bullet->anm_id, 3);
        bullet->unk_98 = 0;
    }
    if (bullet->state == 1)
    {
        Player *player = g_Player;
        if (player->inner.shoot_key_short_timer.current < 0 ||
            (!(bullet->shooter_ref & 0xf0000) && (i8)shooter->option - 1 >= player->inner.num_main_options) ||
            ((bullet->shooter_ref & 0xf0000) && (i8)shooter->option - 101 >= player->inner.num_season_options) ||
            player->inner.state == 2 || player->inner.state == 4 ||
            player->inner.option_lasers[option_laser_index(shooter, bullet->shooter_ref)] !=
                g_Globals.power / g_Globals.power_per_level ||
            (g_Gui != NULL && g_Gui->msg != NULL) || g_EnemyManager == NULL)
        {
            bullet->damage_source()->flags &= ~1;
            bullet->state = 2;
            AnmManager::interrupt_tree(bullet->anm_id, 1);
            g_Player->inner.option_lasers[option_laser_index(shooter, bullet->shooter_ref)] = 0;
            g_SoundManager.stop_sound(20);
        }
    }
    bullet->unk_94 = 0;
    return 0;
}

// Marisa's laser hitting an enemy: cut it short at the enemy (pos, and
// size for rectangles, else radius) and spray sparks along it.
// FUNCTION: TH16 0x446870
i32 __fastcall sht_on_hit_446870(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y)
{
    Float3 *enemy_pos = (Float3 *)unk;
    Float3 *enemy_size = (Float3 *)enemy;
    bullet->unk_94 = 1;
    if (bullet->unk_98 == 0)
    {
        AnmManager::interrupt_tree(bullet->anm_id, 2);
        bullet->unk_98 = 1;
    }
    if (enemy_size == NULL)
    {
        PlayerDamageSource *source = bullet->damage_source();
        f32 radius = source->unk_18 * 0.5f + y;
        f32 dx = enemy_pos->x - bullet->pos.pos.x;
        f32 dy = enemy_pos->y - bullet->pos.pos.y;
        f32 angle = -bullet->pos.angle.value;
        f32 s = zun_sinf(angle);
        f32 c = zun_cosf(angle);
        f32 rx = dx * c - dy * s;
        f32 ry = dy * c + dx * s;
        f32 length;
        if (fabs(ry) > radius || -radius > rx || (0.0f > rx && rx * rx + ry * ry > radius * radius))
        {
            length = rx;
        }
        else
        {
            f32 t = ry / radius;
            length = rx - sqrtf(1.0f - t * t) * radius;
        }
        bullet->laser_length = length + 8.0f;
        if (bullet->laser_length < 0.0f)
        {
            bullet->laser_length = 0.0f;
        }
        source->unk_14 = bullet->laser_length;
    }
    else
    {
        PlayerDamageSource *source = bullet->damage_source();
        Float3 *start = &bullet->pos.pos;
        Float3 hit;
        Float3 exit;
        if (collision_ray_rect(&hit, &exit, start, bullet->pos.angle.value, enemy_pos->x, enemy_pos->y,
                               source->unk_18 + enemy_size->x, enemy_size->y + source->unk_18, x))
        {
            f32 angle = atan2f(hit.y - bullet->pos.pos.y, hit.x - bullet->pos.pos.x);
            if (fabs(angle_sub_unwrapped(angle, bullet->pos.angle.value)) < ZUN_PI / 2)
            {
                f32 dx = hit.x - start->x;
                f32 dy = hit.y - start->y;
                bullet->laser_length = sqrtf(dx * dx + dy * dy) + 8.0f;
            }
            else
            {
                bullet->laser_length = 8.0f;
            }
            if (bullet->laser_length < 0.0f)
            {
                bullet->laser_length = 0.0f;
            }
            source->unk_14 = bullet->laser_length;
        }
        else
        {
            hit = bullet->pos.pos;
            f32 dx = enemy_pos->x - hit.x;
            f32 dy = enemy_pos->y - hit.y;
            bullet->laser_length = sqrtf(dx * dx + dy * dy) - 24.0f;
            if (bullet->laser_length < 0.0f)
            {
                bullet->laser_length = 0.0f;
            }
            source->unk_14 = bullet->laser_length + 16.0f;
        }
    }
    if (bullet->timer_c.current != bullet->timer_c.previous && !(bullet->timer_c.current & 1))
    {
        Float3 end;
        player_sincosmul(&end, bullet->pos.angle.value, bullet->laser_length);
        Float3 start = bullet->pos.pos;
        end.x = start.x + end.x;
        end.y = start.y + end.y;
        end.z = start.z + end.z;
        Float3 velocity;
        player_sincosmul(&velocity, bullet->pos.angle.value, 64.0f);
        end.z = 0.0f;
        velocity.z = 0.0f;
        AnmId id;
        if (!(bullet->shooter_ref & 0xf0000))
        {
            id = g_Player->anm_file->create_vm(8, &end, 0.0f, -1, 0);
        }
        else
        {
            id = g_Player->subseason_anm_file->create_vm(2, &end, 0.0f, -1, 0);
        }
        i32 index = g_EffectManager->next_index();
        if (index != -1)
        {
            g_EffectManager->anm_ids[index] = id;
        }
        AnmVm *vm = g_AnmManager->get_vm_with_id(id);
        if (vm != NULL)
        {
            vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
            vm->rotation.z = bullet->pos.angle.value;
        }
        g_AnmManager->get_vm_with_id(id)->set_pos_time(0x14, 4, &g_zero_vec, &velocity);
    }
    if (bullet->timer_c.current != bullet->timer_c.previous && bullet->timer_c.current % 4 == 0)
    {
        return bullet->damage_source()->damage;
    }
    return 0;
}
