// Shot type callbacks: the functions a .sht file's shooters name by index
// (g_sht_*_funcs), called with the bullet in ecx, and the damage source hit
// callbacks. Also PlayerBullet::create and hit.
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
#include "ZunAsm.h"

// Scripts of the player's ANM files: the sparks along Marisa's laser (in
// pl0X.anm and pl0Xsub.anm), and where a main shot type's bullet scripts
// start in pl0X.anm (ShtShooter::anm_script counts from there).
enum
{
    LASER_SPARK_SCRIPT = 8,
    SEASON_LASER_SPARK_SCRIPT = 2,
    SHOT_SCRIPT_BASE = 5,
};

i32 __fastcall sht_on_init_homing(PlayerBullet *bullet);
i32 __fastcall sht_on_init_laser(PlayerBullet *bullet);
i32 __fastcall sht_on_init_sideways(PlayerBullet *bullet);
i32 __fastcall sht_on_init_piercing(PlayerBullet *bullet);
i32 __fastcall sht_on_init_spread(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_homing(PlayerBullet *bullet);
// safebuffers: see sht_on_tick_laser and sht_on_tick_sideways.
__declspec(safebuffers) i32 __fastcall sht_on_tick_laser(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_accelerate(PlayerBullet *bullet);
__declspec(safebuffers) i32 __fastcall sht_on_tick_sideways(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_accelerate_slow(PlayerBullet *bullet);
i32 __fastcall sht_on_hit_spark_back(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation, f32 radius);
i32 __fastcall sht_on_hit_laser(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation, f32 radius);
i32 __fastcall sht_on_hit_burst_rising(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation,
                                       f32 radius);
i32 __fastcall sht_on_hit_burst(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation, f32 radius);
i32 __fastcall sht_on_hit_spark(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation, f32 radius);
i32 __fastcall sht_on_hit_spark_grow(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation, f32 radius);
i32 __fastcall damage_source_on_hit_bullet(PlayerDamageSource *source, i32 enemy_pos, i32 enemy_size, f32 rotation,
                                           f32 radius);
i32 __fastcall damage_source_on_hit_piercing(PlayerDamageSource *source, i32 enemy_pos, i32 enemy_size,
                                             f32 rotation, f32 radius);

// ShtShooter::func_on_init, func_on_tick and func_on_hit by index.
// GLOBAL: TH16 0x4919c0
ShtBulletFunc const g_sht_on_init_funcs[7] = {
    NULL,
    sht_on_init_homing,
    sht_on_init_laser,
    sht_on_init_sideways,
    sht_on_init_piercing,
    sht_on_init_spread,
    NULL,
};

// GLOBAL: TH16 0x4919a0
ShtBulletFunc const g_sht_on_tick_funcs[8] = {
    NULL,
    sht_on_tick_homing,
    sht_on_tick_laser,
    sht_on_tick_accelerate,
    sht_on_tick_sideways,
    sht_on_tick_accelerate_slow,
    NULL,
    NULL,
};

// GLOBAL: TH16 0x491980
ShtHitFunc const g_sht_on_hit_funcs[8] = {
    NULL,
    sht_on_hit_spark_back,
    sht_on_hit_laser,
    sht_on_hit_burst_rising,
    sht_on_hit_burst,
    sht_on_hit_spark,
    sht_on_hit_spark_grow,
    NULL,
};

// Nothing uses the third callback; its table has only the empty entry
// (ExpHP: SHT_SHOOTER_30_TABLE).
// GLOBAL: TH16 0x4a6f04
ShtBulletFunc g_sht_func_3_table[1];

// PlayerDamageSource::hit_func: 1 for player bullets (their shooter's
// on_hit), 2 for piercing ones.
// GLOBAL: TH16 0x4919dc
DamageSourceHitFunc const g_damage_source_hit_funcs[3] = {
    NULL,
    damage_source_on_hit_bullet,
    damage_source_on_hit_piercing,
};

// Homing: turns toward the nearest enemy, slowing down while the turn is
// sharp and speeding up once it is on course.
// FUNCTION: TH16 0x445ee0
i32 __fastcall sht_on_tick_homing(PlayerBullet *bullet)
{
    if (bullet->state == PLAYER_BULLET_HIT)
    {
        return 0;
    }
    // Stored through an EnemyRef but tested as an int, which gives the
    // original's reload (see docs/findings.md).
    EnemyRef *target = (EnemyRef *)&bullet->target_enemy_id;
    if (g_EnemyManager == NULL)
    {
        target->id = 0;
    }
    else if (target->id == 0)
    {
        Float3 pos = bullet->pos.pos;
        *target = g_EnemyManager->find_closest(&pos, 256.0f);
    }
    if (bullet->target_enemy_id != 0)
    {
        if (!g_EnemyManager->is_enemy_alive(bullet->target_enemy_id))
        {
            bullet->target_enemy_id = 0;
        }
        else
        {
            EnemyInf *enemy = target->get();
            if (!(enemy->enemy.flags_low & ENEMY_FLAGS_UNTARGETABLE))
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
                if (bullet->age.current < 60)
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
i32 __fastcall sht_on_init_homing(PlayerBullet *bullet)
{
    bullet->target_enemy_id = 0;
    return 0;
}

// Marisa's laser starts with length 0, with its sound and a damage source
// that hits every frame and that sht_on_tick_laser places itself.
// FUNCTION: TH16 0x446200
i32 __fastcall sht_on_init_laser(PlayerBullet *bullet)
{
    Player *player = g_Player;
    bullet->hitbox_width_i = 0;
    g_SoundManager.play_sound_at_position(SE_LAZER02, player->inner.pos.x);
    PlayerDamageSource *source = bullet->damage_source();
    source->hit_interval = 1;
    source->width = 0.0f;
    bullet->flags &= ~PLAYER_BULLET_MOVES_DAMAGE_SOURCE;
    return 0;
}

// Speeds up by 1 every frame.
// FUNCTION: TH16 0x446e00
i32 __fastcall sht_on_tick_accelerate(PlayerBullet *bullet)
{
    if (bullet->state == PLAYER_BULLET_ACTIVE)
    {
        bullet->pos.speed += 1.0f;
    }
    return 0;
}

// FUNCTION: TH16 0x4470e0
i32 __fastcall sht_on_init_sideways(PlayerBullet *bullet)
{
    bullet->flags &= ~PLAYER_BULLET_PHASE_MASK;
    bullet->target_enemy_id = 0;
    return 0;
}

// The next enemy of the manager's iteration (NULL at the end). The result
// goes through a local so that a NULL node still joins at the enemy test.
static inline EnemyInf *advance_enemy_iter(EnemyManager *mgr)
{
    mgr->unk_15c = mgr->unk_15c->next;
    EnemyInf *enemy = mgr->unk_15c != NULL ? mgr->unk_15c->entry : NULL;
    return enemy;
}

// Waits for an enemy in the same row, then stops and flies at it
// sideways. (The masks that clear the phase before setting it are ZUN's.)
// `~flags & ENEMY_FLAG_NO_HURTBOX` keeps the original's separate
// not/test al, 1 instead of merging the bit into the next mask test.
// Declared __declspec(safebuffers) (above): without it ours gets a /GS
// cookie for pos (it goes away without the interrupt_tree call) that the
// original does not have.
// pos is copied before the iteration starts, and the enemy advance goes
// through advance_enemy_iter: both give the original's registers. A call
// in each branch for the dash angle (not a ternary argument) keeps 0.0f
// from being hoisted to the top.
// FUNCTION: TH16 0x4470f0
i32 __fastcall sht_on_tick_sideways(PlayerBullet *bullet)
{
    if (bullet->state == PLAYER_BULLET_HIT)
    {
        return 0;
    }
    if (!(bullet->flags & PLAYER_BULLET_PHASE_MASK))
    {
        EnemyManager *mgr = g_EnemyManager;
        if (mgr == NULL)
        {
            bullet->target_enemy_id = 0;
        }
        else if (bullet->target_enemy_id == 0)
        {
            Float3 pos = bullet->pos.pos;
            mgr->unk_15c = mgr->active_enemy_list_head;
            EnemyInf *enemy = mgr->unk_15c->entry;
            while (enemy != NULL)
            {
                if ((~enemy->enemy.flags_low & ENEMY_FLAG_NO_HURTBOX) && !(enemy->enemy.flags_low & ENEMY_FLAGS_UNTARGETABLE) &&
                    pos.y >= enemy->enemy.final_pos.pos.y - 16.0f && enemy->enemy.final_pos.pos.y + 16.0f >= pos.y &&
                    (enemy->enemy.final_pos.pos.x - 16.0f >= pos.x || pos.x >= enemy->enemy.final_pos.pos.x + 16.0f))
                {
                    bullet->flags = (bullet->flags & ~0x38) | PLAYER_BULLET_PHASE_LINED_UP;
                    AnmManager::interrupt_tree(bullet->anm_id, 2);
                    bullet->phase_timer.set_value(0);
                    bullet->pos.speed = 0.0f;
                    bullet->target_pos = enemy->enemy.final_pos.pos;
                    break;
                }
                enemy = advance_enemy_iter(mgr);
            }
        }
    }
    if ((bullet->flags & PLAYER_BULLET_PHASE_MASK) == PLAYER_BULLET_PHASE_LINED_UP)
    {
        if (bullet->phase_timer.current == 4)
        {
            if (bullet->pos.pos.x > bullet->target_pos.x)
            {
                bullet->pos.set_angle(-ZUN_PI);
            }
            else
            {
                bullet->pos.set_angle(0.0f);
            }
            bullet->pos.speed = 14.0f;
            bullet->flags = (bullet->flags & ~0x34) | PLAYER_BULLET_PHASE_DASHING;
        }
        bullet->phase_timer++;
    }
    return 0;
}

// The bullet's damage source becomes piercing.
// FUNCTION: TH16 0x447450
i32 __fastcall sht_on_init_piercing(PlayerBullet *bullet)
{
    bullet->damage_source()->hit_func = 2;
    return 0;
}

// Speeds up by 0.5 every frame.
// FUNCTION: TH16 0x447480
i32 __fastcall sht_on_tick_accelerate_slow(PlayerBullet *bullet)
{
    if (bullet->state == PLAYER_BULLET_ACTIVE)
    {
        bullet->pos.speed += 0.5f;
    }
    return 0;
}

// TODO: the original loads g_Player into ecx before scaling the index and
// reads the old value straight into eax; ours loads g_Player into eax after
// the scaling and moves the value over from ecx.
// Piercing bullets deal their full damage on the first hit and 1 after
// that.
// FUNCTION: TH16 0x4474a0
i32 __fastcall damage_source_on_hit_piercing(PlayerDamageSource *source, i32 enemy_pos, i32 enemy_size,
                                             f32 rotation, f32 radius)
{
    i32 index = source->bullet_index;
    i32 was_hit = g_Player->inner.bullets[index].damage;
    g_Player->inner.bullets[index].damage = 1;
    return was_hit;
}

// Fires up to 15 degrees off the shooter's angle.
// FUNCTION: TH16 0x4474d0
i32 __fastcall sht_on_init_spread(PlayerBullet *bullet)
{
    bullet->pos.angle.value = wrap_angle(bullet->pos.angle.value +
                                         g_replay_safe_rng.randf_neg_1_to_1() * (ZUN_PI / 180.0f) * 15.0f);
    return 0;
}

// A player bullet's damage source hit an enemy: the shooter's on_hit, or
// PlayerBullet::hit. Reading shooter_ref through the bullet at each lookup
// (not into a local) makes the shooter be computed twice from scratch, as
// in the original.
// FUNCTION: TH16 0x445d40
i32 __fastcall damage_source_on_hit_bullet(PlayerDamageSource *source, i32 enemy_pos, i32 enemy_size, f32 rotation,
                                           f32 radius)
{
    Player *player = g_Player;
    PlayerBullet *bullet = &player->inner.bullets[source->bullet_index];
    if (player->get_shooter(bullet->shooter_ref)->func_on_hit != NULL)
    {
        return player->get_shooter(bullet->shooter_ref)->func_on_hit(bullet, enemy_pos, enemy_size, rotation, radius);
    }
    return bullet->hit();
}

// FUNCTION: TH16 0x445e20
i32 PlayerBullet::hit()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    pos.pos.z = 0.1f;
    vm->interrupt(1);
    pos.speed *= 0.125f;
    state = PLAYER_BULLET_HIT;
    vm->entity_pos = pos.pos;
    damage_source()->flags &= ~DAMAGE_SOURCE_ACTIVE;
    i32 result = damage;
    damage_source_index = 0;
    return result;
}

// Bursts into a damage source that grows and drifts upward for 20 frames.
// TODO: the original pushes interrupt_tree's 1 between the stores to the new
// damage source; ours pushes it first.
// FUNCTION: TH16 0x446e20
i32 __fastcall sht_on_hit_burst_rising(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation,
                                       f32 radius)
{
    i32 damage = g_Player->get_shooter(bullet->shooter_ref)->damage;
    PlayerDamageSource *source =
        g_Player->get_damage_source(g_Player->create_damage_source(&bullet->pos.pos, 24.0f, 1.0f, 0x14, damage));
    source->hit_interval = 4;
    source->pos.speed = 0.3f;
    source->pos.angle.value = -ZUN_PI / 2;
    AnmManager::interrupt_tree(bullet->anm_id, 1);
    bullet->state = PLAYER_BULLET_HIT;
    source = bullet->damage_source();
    source->flags &= ~DAMAGE_SOURCE_ACTIVE;
    bullet->damage_source_index = 0;
    bullet->pos.speed = 0.3f;
    source->pos = bullet->pos;
    g_SoundManager.play_sound_at_position(SE_MSL2, bullet->pos.pos.x);
    return bullet->damage;
}

// A randomly tinted spark (effect.anm script 0x98) pointing back the way
// the bullet came, give or take 20 degrees.
// FUNCTION: TH16 0x4460c0
i32 __fastcall sht_on_hit_spark_back(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation, f32 radius)
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

// A spark (effect.anm script 0x98) along the bullet, give or take 20
// degrees.
// FUNCTION: TH16 0x447270
i32 __fastcall sht_on_hit_spark(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation, f32 radius)
{
    f32 angle = wrap_angle(bullet->pos.angle.value + g_replay_unsafe_rng.randf_neg_1_to_1() * 0.34906584f);
    g_EffectManager->effect_anm->create_vm(0x98, &bullet->pos.pos, angle, -1, 0);
    return bullet->hit();
}

// Bursts into a damage source that grows and moves on with the bullet for
// 20 frames.
// g_Player is named at each use (a Player local is reloaded for
// create_damage_source) and the lookup goes through get_damage_source.
// FUNCTION: TH16 0x446f80
i32 __fastcall sht_on_hit_burst(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation, f32 radius)
{
    i32 damage = g_Player->get_shooter(bullet->shooter_ref)->damage;
    PlayerDamageSource *source =
        g_Player->get_damage_source(g_Player->create_damage_source(&bullet->pos.pos, 24.0f, 2.0f, 0x14, damage));
    source->hit_interval = 4;
    AnmManager::interrupt_tree(bullet->anm_id, 1);
    bullet->state = PLAYER_BULLET_HIT;
    bullet->pos.speed = 2.0f;
    source->pos = bullet->pos;
    bullet->damage_source()->flags &= ~DAMAGE_SOURCE_ACTIVE;
    bullet->damage_source_index = 0;
    g_SoundManager.play_sound_at_position(SE_MSL2, bullet->pos.pos.x);
    return bullet->damage;
}

// A randomly tinted spark that grows threefold over 20 frames.
// FUNCTION: TH16 0x447320
i32 __fastcall sht_on_hit_spark_grow(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation, f32 radius)
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

// This file's copy of sincosmul (ZunAsm.h).
// FUNCTION: TH16 0x4476b0
static void __fastcall player_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    ZUN_ASM_SINCOSMUL(dst, angle, radius);
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
    return index >= SEASON_OPTION_INDEX_BASE
               ? &player->inner.subseason_options[index - SEASON_OPTION_INDEX_BASE].scaled_cur_pos
               : &player->inner.main_options[index].scaled_cur_pos;
}

// The index into Player::inner.option_lasers of a shooter's option.
static __forceinline i32 option_laser_index(ShtShooter *shooter, i32 shooter_ref)
{
    i32 option = (i8)shooter->option - 1;
    if (option >= SEASON_OPTION_INDEX_BASE)
    {
        option -= SEASON_OPTION_INDEX_BASE;
    }
    return ((shooter_ref & SHOOTER_REF_SEASON_MASK) != 0) * OPTION_LASER_SEASON_BASE + option;
}

// Marisa's laser: follows its option, turns toward the shot angle and
// grows up to 512 pixels; it ends once the shot key is released, the
// option is gone or the power level changed.
// The VM fields are stored before their flag bits are set, which loads
// the values ahead of the or like the original.
// Declared __declspec(safebuffers) (above): without it ours gets a /GS
// cookie (offset goes to the asm sincosmul) the original does not have.
// The dead double math is not ZUN's code: it makes LTCG realign the frame
// to 8 bytes like the original (a plain dead double did not).
// TODO: ours loads pi before 2pi, hoists -pi out of the angle wrap loops
// and pads a loop head with a nop.
// FUNCTION: TH16 0x446260
i32 __fastcall sht_on_tick_laser(PlayerBullet *bullet)
{
    double unused = 0.0;
    unused = unused * 2.0;
    unused = unused * 2.0;
    unused = unused * 2.0;
    (void)unused;
    ShtShooter *shooter = g_Player->get_shooter(bullet->shooter_ref);
    Int2 *option = option_pos(g_Player, (i8)shooter->option - 1);
    Float3 pos(option->x / 128.0f, option->y / 128.0f, 0.0f);
    bullet->pos.pos = pos;
    ZunAngle current = bullet->pos.angle;
    ZunAngle target;
    target.value = wrap_angle(g_Player->inner.is_focused ? -ZUN_PI / 2 : shooter->angle);
    if ((f32)fabs(angle_sub(current, target).value) < 0.001f)
    {
        bullet->pos.angle.value = wrap_angle(angle_add(current, angle_mul(angle_sub(target, current), 0.1f)).value);
    }
    else
    {
        bullet->pos.angle.value = wrap_angle(target.value);
    }
    if (bullet->state == PLAYER_BULLET_HIT)
    {
        return 0;
    }
    g_SoundManager.sound_buffers[20].buffer->SetPan((i32)(g_Player->inner.pos.x * 1000.0f / 192.0f));
    if (bullet->laser_length < 512.0f)
    {
        bullet->laser_length += 18.0f;
        bullet->damage_source()->width = bullet->laser_length;
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
        vm->sprite_size.x = bullet->laser_length;
        vm->flags_lo |= ANM_VM_SCALE_CHANGED;
        vm = get_vm_or_clear(bullet->anm_id);
        vm->uv_scale.x = bullet->laser_length / 512.0f;
        vm->flags_lo |= ANM_VM_UV_SCALE_CHANGED;
    }
    bullet->damage_source()->pos.pos = pos;
    if (bullet->laser_hitting == 0 && bullet->state == PLAYER_BULLET_ACTIVE && bullet->laser_hit_anim == 1)
    {
        AnmManager::interrupt_tree(bullet->anm_id, 3);
        bullet->laser_hit_anim = 0;
    }
    if (bullet->state == PLAYER_BULLET_ACTIVE)
    {
        Player *player = g_Player;
        if (player->inner.shoot_key_short_timer.current < 0 ||
            (!(bullet->shooter_ref & SHOOTER_REF_SEASON_MASK) &&
             (i8)shooter->option - 1 >= player->inner.num_main_options) ||
            ((bullet->shooter_ref & SHOOTER_REF_SEASON_MASK) &&
             (i8)shooter->option - (SEASON_OPTION_INDEX_BASE + 1) >= player->inner.num_season_options) ||
            player->inner.state == PLAYER_STATE_DEAD || player->inner.state == PLAYER_STATE_HIT ||
            player->inner.option_lasers[option_laser_index(shooter, bullet->shooter_ref)] !=
                g_Globals.power / g_Globals.power_per_level ||
            (g_Gui != NULL && g_Gui->msg != NULL) || g_EnemyManager == NULL)
        {
            bullet->damage_source()->flags &= ~DAMAGE_SOURCE_ACTIVE;
            bullet->state = PLAYER_BULLET_HIT;
            AnmManager::interrupt_tree(bullet->anm_id, 1);
            g_Player->inner.option_lasers[option_laser_index(shooter, bullet->shooter_ref)] = 0;
            g_SoundManager.stop_sound(20);
        }
    }
    bullet->laser_hitting = 0;
    return 0;
}

// Marisa's laser hitting an enemy: cut it short at the enemy (pos, and
// size for rectangles, else radius) and spray sparks along it.
// The damage source is looked up in each branch, as the original does.
// TODO: the rotation products take c and s as their destinations where the
// original multiplies into dx and dy (which also makes it convert ry with
// cvtps2pd), and enemy_pos is reloaded later.
// FUNCTION: TH16 0x446870
i32 __fastcall sht_on_hit_laser(PlayerBullet *bullet, i32 enemy_pos, i32 enemy_size, f32 rotation, f32 radius)
{
    Float3 *pos = (Float3 *)enemy_pos;
    Float3 *size = (Float3 *)enemy_size;
    bullet->laser_hitting = 1;
    if (bullet->laser_hit_anim == 0)
    {
        AnmManager::interrupt_tree(bullet->anm_id, 2);
        bullet->laser_hit_anim = 1;
    }
    if (size == NULL)
    {
        PlayerDamageSource *source = bullet->damage_source();
        f32 reach = source->height * 0.5f + radius;
        f32 dx = pos->x - bullet->pos.pos.x;
        f32 dy = pos->y - bullet->pos.pos.y;
        f32 angle = -bullet->pos.angle.value;
        f32 s = zun_sinf(angle);
        f32 c = zun_cosf(angle);
        f32 rx = dx * c - dy * s;
        f32 ry = dy * c + dx * s;
        f32 length;
        if ((f32)fabs(ry) > reach || -reach > rx || (0.0f > rx && rx * rx + ry * ry > reach * reach))
        {
            length = rx;
        }
        else
        {
            f32 t = ry / reach;
            length = rx - sqrtf(1.0f - t * t) * reach;
        }
        bullet->laser_length = length + 8.0f;
        if (bullet->laser_length < 0.0f)
        {
            bullet->laser_length = 0.0f;
        }
        source->width = bullet->laser_length;
    }
    else
    {
        PlayerDamageSource *source = bullet->damage_source();
        Float3 *start = &bullet->pos.pos;
        Float3 hit;
        Float3 exit;
        if (collision_line_rect((Float2 *)&hit, (Float2 *)&exit, start, bullet->pos.angle.value, pos->x, pos->y,
                               source->height + size->x, size->y + source->height, rotation))
        {
            f32 angle = atan2f(hit.y - bullet->pos.pos.y, hit.x - bullet->pos.pos.x);
            if ((f32)fabs(angle_sub_unwrapped(angle, bullet->pos.angle.value)) < ZUN_PI / 2)
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
            source->width = bullet->laser_length;
        }
        else
        {
            hit = bullet->pos.pos;
            f32 dx = pos->x - hit.x;
            f32 dy = pos->y - hit.y;
            bullet->laser_length = sqrtf(dx * dx + dy * dy) - 24.0f;
            if (bullet->laser_length < 0.0f)
            {
                bullet->laser_length = 0.0f;
            }
            source->width = bullet->laser_length + 16.0f;
        }
    }
    if (bullet->age.current != bullet->age.previous && !(bullet->age.current & 1))
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
        if (!(bullet->shooter_ref & SHOOTER_REF_SEASON_MASK))
        {
            id = g_Player->anm_file->create_vm(LASER_SPARK_SCRIPT, &end, 0.0f, -1, 0);
        }
        else
        {
            id = g_Player->subseason_anm_file->create_vm(SEASON_LASER_SPARK_SCRIPT, &end, 0.0f, -1, 0);
        }
        i32 index = g_EffectManager->next_index();
        if (index != -1)
        {
            g_EffectManager->anm_ids[index] = id;
        }
        AnmVm *vm = g_AnmManager->get_vm_with_id(id);
        if (vm != NULL)
        {
            // The rotation before its flag: the original's store order.
            vm->rotation.z = bullet->pos.angle.value;
            vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
        }
        g_AnmManager->get_vm_with_id(id)->set_pos_time(0x14, 4, &g_zero_vec, &velocity);
    }
    if (bullet->age.current != bullet->age.previous && bullet->age.current % 4 == 0)
    {
        return bullet->damage_source()->damage;
    }
    return 0;
}

// A shooter's on_init callback also gets the shot key timer.
typedef i32(__fastcall *ShtInitFunc)(PlayerBullet *bullet, i32 time);

// Takes the shooter's damage and hitbox, starts at the player or the
// option, picks the angle and speed (see ShtShooter::angle), starts the
// ANM script and a rectangular damage source that hits through the
// shooter's on_hit, then runs on_init and plays the shot sound.
// FUNCTION: TH16 0x444e10
i32 PlayerBullet::create(i32 shooter_ref, i32 time, PlayerInner *inner)
{
    Player *player = g_Player;
    ShtShooter *shooter = player->get_shooter(shooter_ref);
    state = PLAYER_BULLET_ACTIVE;
    this->shooter_ref = shooter_ref;
    age.reset_inline();
    damage = shooter->damage;
    hitbox_width = shooter->hitbox.x;
    hitbox_height = shooter->hitbox.y;
    ((PlayerBulletFlags *)&flags)->focused = player->inner.is_focused;
    memset(&pos, 0, sizeof(pos));
    if (shooter->option == 0)
    {
        pos.pos = inner->pos;
    }
    else
    {
        Int2 *option = option_pos(player, (i8)shooter->option - 1);
        pos.pos = Float3(option->x / 128.0f, option->y / 128.0f, 0.0f);
    }
    if (shooter->kind == SHT_SHOOTER_LASER)
    {
        player->inner.option_lasers[option_laser_index(shooter, shooter_ref)] =
            g_Globals.power / g_Globals.power_per_level;
    }
    pos.speed = shooter->speed;
    if (shooter->angle >= 1000.0f)
    {
        if (shooter->option == 0)
        {
            pos.angle.value = wrap_angle(shooter->angle);
        }
        else
        {
            f32 base = player->get_option((i8)shooter->option - 1)->angle;
            pos.angle.value = wrap_angle(g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI / 12.0f + base);
            pos.speed = g_replay_safe_rng.randf_neg_1_to_1() * 2.0f + shooter->speed;
        }
    }
    else if (shooter->angle >= 995.0f)
    {
        if (shooter->option == 0)
        {
            pos.angle.value = wrap_angle(shooter->angle);
        }
        else
        {
            pos.angle.value = wrap_angle(player->get_option((i8)shooter->option - 1)->angle);
        }
    }
    else
    {
        pos.angle.value = wrap_angle(shooter->angle);
    }
    pos.update_secondary_fields();
    pos.pos.x += shooter->offset_from_option.x - pos.velocity.x;
    pos.pos.y += shooter->offset_from_option.y - pos.velocity.y;
    if (!(shooter_ref & SHOOTER_REF_SEASON_MASK))
    {
        AnmLoaded *anm = g_Player->anm_file;
        anm_id = anm->create_effect(shooter->anm_script + SHOT_SCRIPT_BASE, -1, NULL);
    }
    else
    {
        AnmLoaded *anm = g_Player->subseason_anm_file;
        // Only this call goes through the member pointer: with both direct,
        // create_effect's wish for an aligned stack makes this function
        // realign; with neither, Player::do_shooting does not.
        anm_id = create_effect_via_pointer(anm, shooter->anm_script, -1, NULL);
    }
    AnmVm *vm = get_vm_or_clear(anm_id);
    if (vm->flags_hi & ANM_VM_AUTO_ROTATE)
    {
        vm->rotation.z = shooter->angle;
        vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
    }
    damage_source_index =
        g_Player->create_rect_damage_source(&pos.pos, hitbox_width, hitbox_height, pos.angle.value, 9999999, damage);
    PlayerDamageSource *source = damage_source();
    source->hit_func = 1;
    source->damage_limit = 10000000;
    source->bullet_index = index_of_self;
    flags |= PLAYER_BULLET_MOVES_DAMAGE_SOURCE;
    if (shooter->func_on_init != NULL && ((ShtInitFunc)shooter->func_on_init)(this, time) != 0)
    {
        state = PLAYER_BULLET_FREE;
        delete_vm_and_clear(anm_id);
        source->flags &= ~DAMAGE_SOURCE_ACTIVE;
        return -1;
    }
    if (shooter->sfx_id >= 0)
    {
        g_SoundManager.play_sound_at_position(shooter->sfx_id, pos.pos.x);
    }
    vm->entity_pos = pos.pos;
    return 0;
}
