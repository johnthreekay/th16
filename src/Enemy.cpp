#include "Enemy.h"
#include "Ecl.h"
#include "Fog.h"
#include "AnmManager.h"
#include "BulletManager.h"
#include "CriticalSections.h"
#include "EffectManager.h"
#include "EnemyManager.h"
#include "GameThread.h"
#include "Globals.h"
#include "Gui.h"
#include "Item.h"
#include "Laser.h"
#include "Player.h"
#include <math.h>

#include "Rng.h"
#include "Bomb.h"
#include "Spellcard.h"
#include "SoundManager.h"
#include "Supervisor.h"
#include "UpdateFunc.h"
#include "ZunMath.h"

static_assert(sizeof(PosVel) == 0x44, "PosVel size");
static_assert(sizeof(EnemyBulletShooter) == 0x380, "EnemyBulletShooter size");
static_assert(sizeof(EnemyData) == 0x4530, "EnemyData size");
static_assert(sizeof(EnemyInf) == 0x574c, "EnemyInf size");
static_assert(sizeof(EnemyManager) == 0x190, "EnemyManager size");

// FUNCTION: TH16 0x41a6d0
i32 EnemyLife::receive_damage(i32 damage)
{
    total_damage_including_ignored += damage;
    if (is_spell & 1)
    {
        current_scaled_by_seven -= damage;
        return current = (current_scaled_by_seven - starting_value_for_next_attack * 7) / 7 +
                         starting_value_for_next_attack;
    }
    return current -= damage;
}

// SYNTHETIC: TH16 0x41a760
// EnemyInf::`scalar deleting destructor'

// FUNCTION: TH16 0x41a790
EnemyData::EnemyData()
{
}

// TODO: drops.reset()'s memset arguments (push 0, lea esi, push esi) are scheduled a few
// stores later in the original; ours pushes them right after push 0x50.
// FUNCTION: TH16 0x41b580
EnemyInf::EnemyInf(const char *sub_name)
{
    memset(&enemy, 0, sizeof(enemy));
    reset_run_context();
    enemy.full = this;
    enemy.abs_pos_i.end_time = 0;
    enemy.rel_pos_i.end_time = 0;
    enemy.abs_angle_i.end_time = 0;
    enemy.rel_angle_i.end_time = 0;
    enemy.abs_speed_i.end_time = 0;
    enemy.rel_speed_i.end_time = 0;
    enemy.abs_radial_dist_i.end_time = 0;
    enemy.rel_radial_dist_i.end_time = 0;
    enemy.abs_ellipse_i.end_time = 0;
    enemy.rel_ellipse_i.end_time = 0;
    enemy.hit_sound = -1;
    enemy.death_anm_script = 0;
    memset(&enemy.final_pos, 0, sizeof(PosVel) * 3);
    enemy.hurtbox_size.x = 24.0f;
    enemy.hurtbox_size.y = 24.0f;
    enemy.hitbox_size.x = 24.0f;
    enemy.hitbox_size.y = 24.0f;
    enemy.rotation = 0.0f;
    enemy.own_boss_id = -1;
    enemy.node_in_global_storage.entry = this;
    enemy.node_in_global_storage.next = NULL;
    enemy.node_in_global_storage.prev = NULL;
    enemy.node_in_global_storage.unk_c = NULL;
    enemy.drops.reset();
    enemy.time_in_ecl = 0;
    enemy.time_alive = 0;
    enemy.set_invuln = 0;
    enemy.no_hitbox_dur = 0;
    enemy.anm_layers = 1;
    on_death_callback = NULL;
    enemy.set_death[0] = '\0';
    enemy_id = g_EnemyManager->inner.next_enemy_id;
    g_EnemyManager->inner.last_enemy_id = g_EnemyManager->inner.next_enemy_id;
    // Skip 0, which means "no enemy".
    g_EnemyManager->inner.next_enemy_id =
        g_EnemyManager->inner.next_enemy_id + 1 == 0 ? 1 : g_EnemyManager->inner.next_enemy_id + 1;
    enemy.slowdown = 0.0f;
    unk_5748 = 0;
    file_manager = g_EnemyManager->file_manager;
    context.current_context->cur_location.subroutine_index = file_manager->find_sub_by_name(sub_name);
    context.current_context->cur_location.offset_from_first_instruction = 0;
    context.current_context->time = 0.0f;
    enemy.life.is_spell &= ~2;
    enemy.life.current = 0;
    enemy.life.maximum = 0;
    enemy.life.remaining_for_cur_attack = 0;
    enemy.life.total_damage_including_ignored = 0;
    for (int i = 0; i < 8; i++)
    {
        enemy.interrupts[i].life = -1;
        enemy.interrupts[i].time = -1;
        enemy.interrupts[i].sub_for_set_next[0] = '\0';
    }
    for (int i = 0; i < 16; i++)
    {
        enemy.unk_224[i] = -1;
    }
    enemy.bomb_damage_multiplier = 1.0f;
    enemy.unk_452c = 0;
}

// FUNCTION: TH16 0x41a8c0
HARNESS_CALLED int EnemyManager::get_enemy_count()
{
    int count = 0;
    EnemyList *node = g_EnemyManager->active_enemy_list_head;
    EnemyList *next;
    for (; node != NULL; node = next)
    {
        next = node->next;
        EnemyInf *enemy = node->entry;
        BOOL ignored = (enemy->enemy.flags_low & 0x31) || enemy->enemy.set_invuln.current > 0 ? TRUE : FALSE;
        if (!ignored)
        {
            count++;
        }
    }
    return count;
}

// FUNCTION: TH16 0x41a910
HARNESS_CALLED void EnemyManager::set_boss_id(int index, EnemyInf *enemy)
{
    if (enemy != NULL)
    {
        inner.boss_ids[index] = enemy->enemy_id;
    }
    else
    {
        inner.boss_ids[index] = 0;
    }
}

// FUNCTION: TH16 0x41a950
HARNESS_CALLED void EnemyManager::set_boss_bit(int value)
{
    inner.boss_bit = value;
}

// FUNCTION: TH16 0x41a980
HARNESS_CALLED BOOL EnemyManager::is_enemy_alive(int id)
{
    if (id == 0)
    {
        return FALSE;
    }
    for (EnemyList *node = g_EnemyManager->active_enemy_list_head; node != NULL; node = node->next)
    {
        if (node->entry->enemy_id == id)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// FUNCTION: TH16 0x41a9c0
EnemyInf *EnemyManager::find_enemy_by_id(int id)
{
    EnemyInf *enemy = NULL;
    if (id == 0)
    {
        return NULL;
    }
    EnemyList *node = g_EnemyManager->active_enemy_list_head;
    while (node != NULL)
    {
        enemy = node->entry;
        if (enemy->enemy_id == id)
        {
            return enemy;
        }
        node = node->next;
    }
    return enemy;
}

// FUNCTION: TH16 0x41ae70
int EnemyManager::initialize(const char *ecl_filename)
{
    anim_statement_anms[0] = g_BulletManager->bullet_anm;
    anim_statement_anms[1] = g_EffectManager->effect_anm;
    file_manager = new EclResourceInf;
    file_manager->load_file(ecl_filename);

    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x1a);
    on_tick = f;

    // create_func, inlined here in the original.
    f = new UpdateFunc();
    f->flags |= UPDATE_FUNC_HEAP_ALLOCATED;
    f->function = (UpdateFuncCallback)on_draw_callback;
    f->on_registration = NULL;
    f->on_cleanup = NULL;
    f->arg = this;
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    g_UpdateFuncRegistry->register_on_draw(f, 0x17);
    on_draw = f;

    inner.time_in_stage = 0;
    inner.enemy_limit = 99999;
    return 0;
}

// FUNCTION: TH16 0x41b1a0
EnemyManager::~EnemyManager()
{
    destroy_all();
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    for (int i = 0; i < 0x20; i++)
    {
        if (file_manager->file_data_pointers[i] != NULL)
        {
            free(file_manager->file_data_pointers[i]);
        }
    }
    delete file_manager;
    file_manager = NULL;
    for (int i = 0; i < 6; i++)
    {
        g_AnmManager->unload_anm(i + 10);
    }
    g_EnemyManager = NULL;
}

// FUNCTION: TH16 0x41b340
HARNESS_CALLED EnemyManager *EnemyManager::create(const char *ecl_filename)
{
    EnemyManager *mgr = new EnemyManager();
    if (mgr->initialize(ecl_filename) != 0)
    {
        delete mgr;
        return NULL;
    }
    return mgr;
}

// FUNCTION: TH16 0x41ade0
HARNESS_CALLED void EnemyManager::remove_from_active_list(EnemyInf *enemy)
{
    EnemyList *node = &enemy->enemy.node_in_global_storage;
    if (active_enemy_list_head == node)
    {
        active_enemy_list_head = enemy->enemy.node_in_global_storage.next;
    }
    if (active_enemy_list_tail == node)
    {
        active_enemy_list_tail = enemy->enemy.node_in_global_storage.prev;
    }
    if (owned_list_188 == node)
    {
        owned_list_188 = enemy->enemy.node_in_global_storage.next;
    }
    if (node->next != NULL)
    {
        node->next->prev = node->prev;
    }
    if (node->prev != NULL)
    {
        node->prev->next = node->next;
    }
    node->next = NULL;
    node->prev = NULL;
    if (!(enemy->enemy.flags_high & 4))
    {
        enemy_count_real--;
    }
}

// TODO: register allocation differs in the inlined ZunTimer::tick: the original loads 1.0f into
// xmm2 at the damage_multiplier store and keeps 1.01f in xmm1 (tick_mixed does not change it).
// FUNCTION: TH16 0x41b3d0
int EnemyManager::update()
{
    inner.unk_a0[0] = 0;
    inner.unk_a0[1] = 0;
    EnemyList *next;
    for (EnemyList *node = active_enemy_list_head; node != NULL; node = next)
    {
        next = node->next;
        if (!(node->entry->enemy.flags_low & 0x2000000) && node->entry->on_tick() == 0)
        {
            node->entry->enemy.flags_low &= ~0x40000;
        }
        else
        {
            delete node->entry;
        }
    }
    if (g_Player->damage_multiplier > 1.01f)
    {
        g_Player->inner.flags |= PLAYER_FLAG_DAMAGE_BOOSTED;
    }
    else
    {
        g_Player->inner.flags &= ~PLAYER_FLAG_DAMAGE_BOOSTED;
    }
    g_Player->damage_multiplier = 1.0f;
    inner.time_in_stage.tick();
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x41b4f0
int __fastcall EnemyManager::on_tick_callback(EnemyManager *mgr)
{
    if (g_GameThread == NULL)
    {
        return UPDATE_FUNC_CONTINUE;
    }
    if (g_GameThread->flags.flag_0 | g_GameThread->flags.paused)
    {
        return UPDATE_FUNC_CONTINUE;
    }
    if (g_GameThread->flags.flag_10)
    {
        return UPDATE_FUNC_CONTINUE;
    }
    if (g_GameThread->flags.flag_1)
    {
        return UPDATE_FUNC_CONTINUE;
    }
    return mgr->update();
}

// FUNCTION: TH16 0x41b530
int __fastcall EnemyManager::on_draw_callback(EnemyManager *mgr)
{
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x41b540
EnemyInf *EnemyRef::get()
{
    i32 id = this->id;
    EnemyInf *enemy = NULL;
    if (id == 0)
    {
        return NULL;
    }
    EnemyList *node = g_EnemyManager->active_enemy_list_head;
    while (node != NULL)
    {
        enemy = node->entry;
        if (enemy->enemy_id == id)
        {
            return enemy;
        }
        node = node->next;
    }
    return enemy;
}

// FUNCTION: TH16 0x424e40
void EnemyInf::set_interrupt(int index, int time, const char *sub)
{
    enemy.interrupts[index].time = time;
    if (sub != NULL)
    {
        strcpy(enemy.interrupts[index].sub_for_set_next, sub);
        strcpy(enemy.interrupts[index].sub_for_set_timeout, sub);
    }
    else
    {
        enemy.interrupts[index].sub_for_set_next[0] = '\0';
        enemy.interrupts[index].sub_for_set_timeout[0] = '\0';
    }
}

// FUNCTION: TH16 0x424eb0
void EnemyInf::set_timeout(int index, const char *sub)
{
    if (sub != NULL)
    {
        strcpy(enemy.interrupts[index].sub_for_set_timeout, sub);
    }
    else
    {
        enemy.interrupts[index].sub_for_set_timeout[0] = '\0';
    }
}

// Only ecl_run_over_300 calls the EnemyData getters, and LTCG inlined them
// there until the function's inline budget ran out; the other callers of
// the original read arguments through the run context directly.
// FUNCTION: TH16 0x4251d0
DECOMP_NOINLINE i32 EnemyData::get_int_arg(int index)
{
    return full->context.current_context->get_int_arg(index);
}

// FUNCTION: TH16 0x4251f0
HARNESS_CALLED i32 *EnemyData::get_int_arg_ptr(int index)
{
    return full->context.current_context->get_int_arg_ptr(index);
}

// FUNCTION: TH16 0x425200
HARNESS_CALLED f32 EnemyData::get_float_arg(int index)
{
    return full->context.current_context->get_float_arg(index);
}

// FUNCTION: TH16 0x425220
DECOMP_NOINLINE f32 *EnemyData::get_float_arg_ptr(int index)
{
    return full->context.current_context->get_float_arg_ptr(index);
}

// FUNCTION: TH16 0x425240
HARNESS_CALLED EnemyRef EnemyManager::find_closest(D3DXVECTOR3 *pos, f32 max_dist)
{
    EnemyInf *closest = NULL;
    f32 closest_dist_sq = max_dist * max_dist;
    EnemyList *next;
    for (EnemyList *node = g_EnemyManager->active_enemy_list_head; node != NULL; node = next)
    {
        next = node->next;
        EnemyInf *enemy = node->entry;
        if (enemy->enemy.flags_low & 0xc000021)
        {
            continue;
        }
        f32 dist_sq = (pos->x - enemy->enemy.final_pos.pos.x) * (pos->x - enemy->enemy.final_pos.pos.x) +
                      (pos->y - enemy->enemy.final_pos.pos.y) * (pos->y - enemy->enemy.final_pos.pos.y);
        if (dist_sq < closest_dist_sq)
        {
            closest_dist_sq = dist_sq;
            closest = enemy;
        }
    }
    EnemyRef ref;
    ref.id = closest != NULL ? closest->enemy_id : 0;
    return ref;
}

// ECL funcset 1 (the snowman card): cancels the bullets within
// ecl_float_vars[0] of the player.
// TODO: ours jumps straight out of the loop when iter_current is NULL; the original goes through
// the "entry or NULL" join and tests again.
// FUNCTION: TH16 0x4252d0
int __fastcall ecl_funcset_cancel_near_player(EnemyData *enemy)
{
    BulletManager *mgr = g_BulletManager;
    Bullet *b = mgr->iter_first();
    while (b != NULL)
    {
        if (b->unk_c4c == 1)
        {
            f32 dy = g_Player->inner.pos.y - b->pos.y;
            f32 dx = g_Player->inner.pos.x - b->pos.x;
            if (enemy->ecl_float_vars[0] * enemy->ecl_float_vars[0] > dx * dx + dy * dy)
            {
                b->unk_c60 = 8;
                b->unk_c4c = 2;
                b->active_ex_flags = 0;
            }
        }
        b = mgr->iter_advance();
    }
    return 0;
}

// ECL funcset 2 (Okina's last spell): takes all power away.
// FUNCTION: TH16 0x4253b0
int __fastcall ecl_funcset_zero_power(EnemyData *enemy)
{
    g_Globals.season_power = 0;
    if (g_Globals.season_power > g_Globals.max_season_power)
    {
        g_Globals.season_power = g_Globals.max_season_power;
    }
    g_Globals.power = 0;
    if (g_Globals.power > g_Globals.max_power)
    {
        g_Globals.power = g_Globals.max_power;
    }
    g_Player->inner.repopulate_options();
    Gui::update_season_gauge();
    return 0;
}

// FUNCTION: TH16 0x4253f0
int __fastcall ecl_ext_damage_stored(EnemyData *enemy, int damage)
{
    if (enemy->ecl_int_vars[3] > 0)
    {
        int result = enemy->ecl_int_vars[3] + damage;
        enemy->ecl_int_vars[3] = 0;
        return result;
    }
    return damage;
}

// The third damage hook: the damage the player deals to the hurtboxes
// around the VM in anm_ids[1] (a rotated bar and a circle below it), on top
// of the damage the enemy took itself.
// TODO: the original loads vm->rotation.z into xmm3 after pushing enemy_id; ours before the size.x store
// (separate size.x/size.y stores and get_vm_or_clear do not change it).
// FUNCTION: TH16 0x425410
int __fastcall ecl_ext_damage_anm_hurtbox(EnemyData *enemy, int damage)
{
    int bar_damage = 0;
    int circle_damage = 0;
    i32 hit = 0;
    Float3 pos = enemy->final_pos.pos;
    AnmVm *vm = g_AnmManager->get_vm_with_id(enemy->anm_ids[1]);
    if (vm == NULL)
    {
        enemy->anm_ids[1].id = 0;
    }
    else
    {
        pos.y += 24.0f;
        Float2 size(vm->scale.x * 192.0f, vm->scale.y * 32.0f);
        bar_damage = g_Player->compute_damage_to_enemy(&pos, (Float3 *)&size, vm->rotation.z, 0.0f, &hit, NULL, 0,
                                                       enemy->full->enemy_id);
        pos.y += 32.0f;
        circle_damage =
            g_Player->compute_damage_to_enemy(&pos, NULL, 0.0f, 48.0f, &hit, NULL, 0, enemy->full->enemy_id);
    }
    return damage + bar_damage + circle_damage;
}

// The hit sound of a damaged enemy: quieter unless it is a boss whose
// attack is nearly over.
static inline void enemy_play_hit_sound(EnemyData *enemy, i32 low_life_spell, i32 low_life)
{
    u32 spell_flags = g_Spellcard->flags;
    if ((enemy->flags_low & 0x40800000) && (spell_flags & 9) != 9 &&
        (((spell_flags & 1) && enemy->full->enemy.life.remaining_for_cur_attack < low_life_spell) ||
         (!(spell_flags & 1) && enemy->full->enemy.life.remaining_for_cur_attack < low_life)))
    {
        g_SoundManager.play_sound_at_position(0x23, enemy->final_pos.pos.x);
    }
    else
    {
        g_SoundManager.play_sound_at_position(0x22, enemy->final_pos.pos.x);
    }
}

// Bomb shields, damage from the player's shots and bombs (with the life
// and time interrupts it can trigger), collision with the player and the
// damage flash of the main VM.
// TODO: functionally complete; register allocation and block order differ.
// FUNCTION: TH16 0x41c330
int EnemyData::step_logic()
{
    if ((flags_low & 0x10000000) && (g_MainBomb->in_use == 1 || g_SubseasonBomb->in_use == 1) &&
        !(flags_low & 0x20000000))
    {
        anm_set_main = bombshield_on_anm_main;
        anm_ids[0].replace_with_effect(bombshield_on_anm_main);
        flags_low |= 0x20000001;
    }
    else if (g_MainBomb->in_use != 1 && g_SubseasonBomb->in_use != 1 && (flags_low & 0x20000000))
    {
        anm_set_main = bombshield_off_anm_main;
        anm_ids[0].replace_with_effect(bombshield_off_anm_main);
        flags_low &= ~0x20000001;
    }
    if (flags_low & 0x800)
    {
        i32 hit = 0;
        i32 result;
        if (!(flags_low & 0x1000))
        {
            result = g_Player->compute_damage_to_enemy(&final_pos.pos, NULL, 0.0f, hurtbox_size.x * 0.5f, &hit,
                                                       &last_damage_pos, 1, full->enemy_id);
        }
        else
        {
            result = g_Player->compute_damage_to_enemy(&final_pos.pos, (Float3 *)&hurtbox_size, rotation, 0.0f, &hit,
                                                       &last_damage_pos, 1, full->enemy_id);
        }
        if (result != 0 && hit != 0 && full->die() != 0)
        {
            return 1;
        }
    }
    const char *sub = full->check_time_interrupts();
    if (sub != NULL)
    {
        time_in_ecl = 0;
        full->free_all_async();
        full->reset_run_context();
        EnemyInf *inf = full;
        inf->context.current_context->cur_location.subroutine_index = inf->file_manager->find_sub_by_name(sub);
        inf->context.current_context->cur_location.offset_from_first_instruction = 0;
        inf->context.current_context->time = 0.0f;
        if (full->run_ecl(*g_timer_speed_ptrs[time_in_ecl.speed_index]) != 0)
        {
            return -1;
        }
    }
    flags_low &= ~0x200000;
    i32 hit = 0;
    if (!(flags_low & 0x21))
    {
        i32 damage = 0;
        if (hurtbox_size.x > 0.0f)
        {
            if (!(flags_low & 0x1000))
            {
                damage = g_Player->compute_damage_to_enemy(&final_pos.pos, NULL, 0.0f, hurtbox_size.x * 0.5f, &hit,
                                                           &last_damage_pos, 0, full->enemy_id);
            }
            else
            {
                damage = g_Player->compute_damage_to_enemy(&final_pos.pos, (Float3 *)&hurtbox_size, rotation, 0.0f, &hit,
                                                           &last_damage_pos, 0, full->enemy_id);
            }
            damage = damage * g_Player->damage_multiplier;
        }
        if (func_from_ecl_flag_ext_dmg != NULL)
        {
            damage += ((EnemyExtDamageFunc)func_from_ecl_flag_ext_dmg)(this, damage);
        }
        if (unk_3fe0 > 0)
        {
            damage += unk_3fe0;
            unk_3fe0 = 0;
        }
        if (g_Player->inner.state == PLAYER_STATE_DEAD || g_Player->inner.state == PLAYER_STATE_RESPAWNING)
        {
            damage /= 5;
        }
        i32 dealt = g_Gui->msg == NULL ? damage : 0;
        if (dealt > 0)
        {
            if (hit)
            {
                if (dealt >= life.current)
                {
                    g_EnemyManager->inner.unk_a0[0] += (dealt - life.current) / 4 + life.current;
                }
                else
                {
                    g_EnemyManager->inner.unk_a0[0] += dealt;
                }
            }
            else
            {
                g_EnemyManager->inner.unk_a0[1] += dealt;
            }
        }
        i32 life_damage = dealt;
        if (g_MainBomb->in_use == 1 && bomb_damage_multiplier < 1.0f)
        {
            if (dealt != 0 && 0.0f >= bomb_damage_multiplier)
            {
                g_SoundManager.play_sound_at_position(0x24, final_pos.pos.x);
            }
            life_damage = dealt * bomb_damage_multiplier;
        }
        if (life_damage != 0)
        {
            if ((g_Spellcard->flags & 0x21) == 0x21)
            {
                life_damage /= 30;
            }
            if (!(flags_low & 0x10) && set_invuln.current <= 0)
            {
                life.receive_damage(life_damage);
            }
            else
            {
                life.total_damage_including_ignored += life_damage;
            }
            if (drop_season.damage_per_season_drop > 0)
            {
                while (drop_season.damage_accounted_for_season_drops < life.total_damage_including_ignored)
                {
                    drop_season.damage_accounted_for_season_drops += drop_season.damage_per_season_drop;
                    g_ItemManager->spawn_item(0x10, &final_pos.pos, 0, g_replay_safe_rng.randf_neg_pi_to_pi(),
                                              g_replay_safe_rng.randf_0_to_1() + 1.2f, 0, 0);
                }
            }
            unk_4024.set_value(30);
            sub = full->check_life_interrupts();
            if (sub != NULL)
            {
                time_in_ecl.set_value(0);
                full->free_all_async();
                full->reset_run_context();
                full->load_sub_by_name(sub);
                if (full->run_ecl(*g_timer_speed_ptrs[time_in_ecl.speed_index]) != 0)
                {
                    return -1;
                }
            }
            if ((life.current <= 0) & ~(flags_low >> 7))
            {
                if (full->die() != 0)
                {
                    return 1;
                }
            }
            flags_low |= 0x200000;
        }
    }
    if (life.is_spell & 2)
    {
        if (full->die() != 0)
        {
            return 1;
        }
    }
    if (!(flags_low & 0x22) && no_hitbox_dur.current <= 0 && !(flags_low & 0x4000000))
    {
        if (func_from_ecl_unknown_634 != NULL)
        {
            ((EnemyFuncSetFunc)func_from_ecl_unknown_634)(this);
        }
        else
        {
            i32 result;
            if (!(flags_low & 0x1000))
            {
                result = g_Player->check_hit_circle(&final_pos.pos, hitbox_size.x * 0.5f, 0);
            }
            else
            {
                AnmVm *vm = anm_ids[0].find_or_clear();
                f32 half = hitbox_size.y * 0.5f;
                f32 x = 0.0f;
                f32 y = half;
                if (vm != NULL)
                {
                    f32 angle = normalize_angle(vm->rotation.z + ZUN_PI / 2);
                    f32 s = zun_sinf(angle);
                    f32 c = zun_cosf(angle);
                    x = c * 0.0f - s * half;
                    y = c * half + s * 0.0f;
                }
                D3DXVECTOR3 pos;
                pos.x = x + final_pos.pos.x;
                pos.y = final_pos.pos.y + y;
                pos.z = final_pos.pos.z + 0.0f;
                result = g_Player->check_hit_rotated_rect(&pos, rotation, hitbox_size.x, hitbox_size.y, 0);
            }
            if ((flags_low & 0x200) && result == 2 && time_in_ecl.current % 6 == 0)
            {
                g_Player->do_graze(&g_Player->inner.pos);
            }
        }
    }
    AnmVm *vm = g_AnmManager->get_vm_with_id(anm_ids[0]);
    if (vm == NULL)
    {
        anm_ids[0].id = 0;
    }
    else if (unk_3ff0 == 0)
    {
        if (flags_low >= 0x80000000)
        {
            if (time_in_ecl.current % 4 == 0)
            {
                vm->color_2.d3d = 0xffff00ff;
                vm->flags_lo = (vm->flags_lo & ~0x40000) | 0x20000;
            }
            else
            {
                vm->flags_lo &= ~0x60000;
            }
        }
        if ((flags_low & 0x200000) && !(flags_low & 0x2000))
        {
            vm->color_2.d3d = 0xff0000ff;
            vm->flags_lo = (vm->flags_lo & ~0x40000) | 0x20000;
            unk_3ff0 = 4;
            if (hit_sound < 0)
            {
                enemy_play_hit_sound(this, 200, 900);
            }
            else
            {
                g_SoundManager.play_sound_at_position(hit_sound, final_pos.pos.x);
            }
        }
        else if (time_in_ecl.current % 4 == 0)
        {
            u32 spell_flags = g_Spellcard->flags;
            if ((flags_low & 0x40800000) && (spell_flags & 9) != 9 &&
                (((spell_flags & 1) && full->enemy.life.remaining_for_cur_attack < 100) ||
                 (!(spell_flags & 1) && full->enemy.life.remaining_for_cur_attack < 500)))
            {
                vm->color_2.d3d = 0xff0000ff;
                vm->flags_lo = (vm->flags_lo & ~0x40000) | 0x20000;
            }
        }
        else
        {
            vm->flags_lo &= ~0x60000;
        }
    }
    else
    {
        vm->flags_lo &= ~0x60000;
        unk_3ff0--;
    }
    if (unk_4024.current > 0)
    {
        unk_4024--;
    }
    return 0;
}

// GLOBAL: TH16 0x4a6dc0
EnemyManager *g_EnemyManager;

// FUNCTION: TH16 0x41ba10
EnemyInf::~EnemyInf()
{
    g_EnemyManager->remove_from_active_list(this);
    if (!(enemy.flags_high & 4))
    {
        if (enemy.flags_low & 0x800000)
        {
            g_EnemyManager->inner.boss_ids[enemy.own_boss_id] = 0;
        }
        AnmManager *anm = g_AnmManager;
        for (i32 i = 0; i < 16; i++)
        {
            anm->delete_vm_inline(enemy.anm_ids[i]);
            enemy.anm_ids[i].id = 0;
        }
    }
    if (enemy.fog.fog_ptr != NULL)
    {
        delete (Fog *)enemy.fog.fog_ptr;
    }
    enemy.fog.fog_ptr = NULL;
}

// out->x, out->y = (rx cos angle, ry sin angle): sincosmul with separate
// radii. A per-file copy, like sincosmul.
// FUNCTION: TH16 0x426240
static void __fastcall sincosmul_ellipse(Float3 *dst, f32 angle, f32 rx, f32 ry)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul rx
        fstp [eax]
        fmul ry
        fstp [eax+4]
    }
}

// FUNCTION: TH16 0x41a720
void EnemyDrop::eject_all_drops(D3DXVECTOR3 *pos)
{
    if (main_type != 0)
    {
        g_ItemManager->spawn_item(main_type, pos, 0, -ZUN_PI / 2, 2.2f, 0, 0);
    }
    eject_extra_drops(pos);
    main_type = 0;
}

// TODO: the original multiplies x as dist * x with dist loaded into a register; ours loads x
// (the operand order in the source does not change it).
// FUNCTION: TH16 0x41d700
void EnemyDrop::eject_extra_drops(D3DXVECTOR3 *pos)
{
    f32 angle = g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
    for (i32 i = 0; i < 16; i++)
    {
        if (i != 15)
        {
            for (i32 j = 0; j < extra_counts[i]; j++)
            {
                Float3 item_pos;
                sincosmul_ellipse(&item_pos, angle, area.x, area.y);
                f32 dist = g_replay_safe_rng.randf_0_to_1() * 0.5f + 0.5f;
                Float3 offset(item_pos.x * dist, dist * item_pos.y, 0.0f);
                item_pos.x = pos->x + offset.x;
                item_pos.y = pos->y + offset.y;
                item_pos.z = pos->z + offset.z;
                g_ItemManager->spawn_item(i + 1, &item_pos, 0, -ZUN_PI / 2, 2.2f, 0, 0);
                angle = wrap_angle(angle + ZUN_PI / 2 + g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI * 0.25f);
            }
        }
        else
        {
            for (i32 j = 0; j < extra_counts[15]; j++)
            {
                g_ItemManager->spawn_item(0x10, pos, 0, g_replay_safe_rng.randf_neg_pi_to_pi(),
                                          g_replay_safe_rng.randf_0_to_1() * 1.9f + 0.2f, 0, 0);
            }
        }
    }
    memset(extra_counts, 0, sizeof(extra_counts));
}

// FUNCTION: TH16 0x41aa00
HARNESS_CALLED i32 EffectManager::track(AnmId id)
{
    i32 index = next_index();
    if (index == -1)
    {
        return 0;
    }
    anm_ids[index] = id;
    return index | 0x80000000;
}

// FUNCTION: TH16 0x41aa40
HARNESS_CALLED LaserDataInf *LaserManager::find_by_id(i32 id, i32 unused)
{
    LaserDataInf *laser = list_head.next;
    if (id == 0)
    {
        return NULL;
    }
    while (laser != NULL)
    {
        if (laser->id == id)
        {
            return laser;
        }
        laser = laser->next;
    }
    return NULL;
}

// GLOBAL: TH16 0x4917b8
extern EnemyFuncSetFunc const g_ecl_func_sets[3] = {NULL, ecl_funcset_cancel_near_player, ecl_funcset_zero_power};

// FUNCTION: TH16 0x41d1e0
int EnemyInf::on_tick()
{
    if (enemy.slowdown <= 0.0f)
    {
        if (enemy.flags_high & 1)
        {
            for (i32 i = 0; i < 16; i++)
            {
                AnmVm *vm = get_vm_or_clear(enemy.anm_ids[i]);
                if (vm != NULL)
                {
                    vm->slowdown = 0.0f;
                }
            }
        }
        return enemy.on_tick();
    }
    f32 game_speed = g_game_speed;
    f32 speed = game_speed - enemy.slowdown * game_speed;
    speed = 0.0f > speed ? 0.0f : speed;
    g_game_speed = 1.0f < speed ? 1.0f : speed;
    for (i32 i = 0; i < 16; i++)
    {
        AnmVm *vm = get_vm_or_clear(enemy.anm_ids[i]);
        if (vm != NULL)
        {
            vm->slowdown = enemy.slowdown;
        }
    }
    int result = enemy.on_tick();
    g_game_speed = game_speed;
    enemy.flags_high |= 1;
    return result;
}

// TODO: ours realigns the frame (and esp, -8) because of the direct zun_atan2f call; the
// original calls it without realigning (GameThread::thread_start's aligned
// EnemyManager::create call and HARNESS_CALLED update/on_tick do not change it). Separate float
// locals for the summed position keep it in registers and avoid a /GS cookie.
// FUNCTION: TH16 0x41d2e0
int EnemyData::on_tick()
{
    if (flags_low & 0x40000)
    {
        return 0;
    }
    flags_low |= 0x40000;
    if (step_interpolators() != 0)
    {
        return -1;
    }
    if (full->run_ecl(*g_timer_speed_ptrs[time_in_ecl.speed_index]) != 0)
    {
        return -1;
    }
    if (func_from_ecl_func_set != NULL && ((EnemyFuncSetFunc)func_from_ecl_func_set)(this) != 0)
    {
        return -1;
    }
    if (step_logic() != 0)
    {
        return -1;
    }
    update_fog();
    if (!(flags_low & 0x4000000))
    {
        for (i32 i = 0; i < 14; i++)
        {
            AnmVm *vm = get_vm_or_clear(anm_ids[i]);
            if (vm == NULL)
            {
                continue;
            }
            f32 x = anm_pos_array[i].x + final_pos.pos.x;
            f32 y = anm_pos_array[i].y + final_pos.pos.y;
            f32 z = anm_pos_array[i].z + final_pos.pos.z;
            if (unk_224[i] >= 0)
            {
                AnmVm *base = anm_ids[unk_224[i]].find_or_clear();
                if (base != NULL)
                {
                    x += base->pos.x;
                    y += base->pos.y;
                    z += base->pos.z;
                }
            }
            vm->entity_pos.x = x;
            vm->entity_pos.y = y;
            vm->entity_pos.z = z;
            if (vm->flags_hi & ANM_VM_AUTO_ROTATE)
            {
                vm->rotation.z = zun_atan2f(final_pos.velocity.y, final_pos.velocity.x);
                vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
                rotation = vm->rotation.z;
            }
        }
    }
    else
    {
        for (i32 i = 0; i < 14; i++)
        {
            AnmVm *vm = g_AnmManager->get_vm_with_id(anm_ids[i]);
            if (vm != NULL)
            {
                vm->entity_pos = final_pos.pos;
            }
        }
    }
    if (set_invuln.current > 0)
    {
        set_invuln--;
    }
    if (no_hitbox_dur.current > 0)
    {
        no_hitbox_dur--;
    }
    time_alive++;
    time_in_ecl++;
    if (drop_season.bonus_timer.current > 0)
    {
        drop_season.bonus_timer--;
    }
    return 0;
}

// FUNCTION: TH16 0x41c1f0
void EnemyData::update_final_pos()
{
    final_pos.velocity = abs_pos.pos + rel_pos.pos - final_pos.pos;
    final_pos.step();
    if (flags_low & 0x20000)
    {
        f32 half = move_limit_size.x * 0.5f;
        if (move_limit_center.x - half > final_pos.pos.x)
        {
            final_pos.pos.x = move_limit_center.x - half;
        }
        else if (final_pos.pos.x > move_limit_center.x + half)
        {
            final_pos.pos.x = move_limit_center.x + half;
        }
        half = move_limit_size.y * 0.5f;
        if (move_limit_center.y - half > final_pos.pos.y)
        {
            final_pos.pos.y = move_limit_center.y - half;
        }
        else if (final_pos.pos.y > move_limit_center.y + half)
        {
            final_pos.pos.y = move_limit_center.y + half;
        }
        abs_pos.pos = final_pos.pos - rel_pos.pos;
    }
}

// TODO: ours saves ebx/esi after the death sound (shrink-wrapped) and reuses the loaded
// positions for the atan2 arguments; the original reloads them.
// FUNCTION: TH16 0x41d520
int EnemyInf::die()
{
    if (enemy.death_sound >= 0)
    {
        g_SoundManager.play_sound_at_position(enemy.death_sound, enemy.final_pos.pos.x);
    }
    if (enemy.death_anm_script >= 0)
    {
        f32 angle = -ZUN_PI / 2;
        Float3 *pos = &enemy.final_pos.pos;
        if (!(0.04f > (enemy.last_damage_pos.x - pos->x) * (enemy.last_damage_pos.x - pos->x) +
                          (enemy.last_damage_pos.y - pos->y) * (enemy.last_damage_pos.y - pos->y)))
        {
            angle = zun_atan2f(pos->y - enemy.last_damage_pos.y, pos->x - enemy.last_damage_pos.x);
        }
        g_EffectManager->track_inline(g_EnemyManager->anim_statement_anms[enemy.death_anm_index]->create_vm(
            enemy.death_anm_script, pos, angle, 3, 0));
    }
    if (enemy.drop_season.bonus_timer.current <= 0)
    {
        enemy.drops.extra_counts[15] = enemy.drop_season.min_count;
    }
    else
    {
        enemy.drops.extra_counts[15] =
            (enemy.drops.extra_counts[15] - enemy.drop_season.min_count) * enemy.drop_season.bonus_timer.current /
                enemy.drop_season.max_time +
            enemy.drop_season.min_count;
    }
    enemy.drops.eject_all_drops(&enemy.final_pos.pos);
    if (enemy.unk_452c > 0 && enemy.own_chapter == g_Globals.chapter)
    {
        g_Globals.enemies_destroyed_in_chapter += enemy.unk_452c;
        enemy.unk_452c = 0;
    }
    if (enemy.set_death[0] != '\0')
    {
        free_all_async();
        reset_run_context();
        context.current_context->cur_location.subroutine_index = file_manager->find_sub_by_name(enemy.set_death);
        context.current_context->cur_location.offset_from_first_instruction = 0;
        context.current_context->time = 0.0f;
        run_ecl(0.0f);
        enemy.set_death[0] = '\0';
    }
    if (on_death_callback != NULL)
    {
        ((void(__fastcall *)(EnemyInf *))on_death_callback)(this);
    }
    return 1;
}

// TODO: the inlined tick (tick_mixed) adds speed and current_f the other way round (register choice).
// FUNCTION: TH16 0x41d900
void EnemyManager::kill_all()
{
    EnemyManager *mgr = g_EnemyManager;
    EnemyList *next;
    for (EnemyList *node = mgr->active_enemy_list_head; node != NULL; node = next)
    {
        next = node->next;
        EnemyInf *enemy = node->entry;
        if (!(enemy->enemy.flags_low & 0xc004a0) || enemy->enemy.flags_low & 0x100)
        {
            enemy->enemy.drops.reset();
            enemy->enemy.last_damage_pos.x = 0.0f;
            enemy->enemy.last_damage_pos.y = 192.0f;
            enemy->enemy.unk_452c = 0;
            enemy->die();
            enemy->enemy.flags_low |= 0x2000000;
        }
    }
    mgr->inner.time_in_stage.tick_mixed();
}

// TODO: register allocation: the original keeps value in ebx and spills next to the argument slot.
// FUNCTION: TH16 0x41da30
void __stdcall EnemyManager::kill_all_with_unk_278(i32 value)
{
    EnemyManager *mgr = g_EnemyManager;
    EnemyList *node = mgr->active_enemy_list_head;
    while (node != NULL)
    {
        EnemyList *next = node->next;
        EnemyInf *enemy = node->entry;
        if ((!(enemy->enemy.flags_low & 0xc004a0) || enemy->enemy.flags_low & 0x100) && enemy->enemy.unk_278 == value)
        {
            enemy->enemy.drops.reset();
            enemy->enemy.last_damage_pos.x = 0.0f;
            enemy->enemy.last_damage_pos.y = 192.0f;
            enemy->enemy.unk_452c = 0;
            enemy->die();
            enemy->enemy.flags_low |= 0x2000000;
        }
        node = next;
    }
    mgr->inner.time_in_stage.tick();
}

// TODO: in the inlined tick the original adds current_f into the speed's xmm1; ours loads
// current_f into xmm0 and adds the speed, the opposite of kill_all (tick or tick_mixed alike).
// FUNCTION: TH16 0x41db70
void EnemyManager::kill_all_no_set_death()
{
    EnemyManager *mgr = g_EnemyManager;
    EnemyList *next;
    for (EnemyList *node = mgr->active_enemy_list_head; node != NULL; node = next)
    {
        next = node->next;
        EnemyInf *enemy = node->entry;
        if (!(enemy->enemy.flags_low & 0xc004a0) || enemy->enemy.flags_low & 0x100)
        {
            enemy->enemy.drops.reset();
            enemy->enemy.last_damage_pos.x = 0.0f;
            enemy->enemy.last_damage_pos.y = 192.0f;
            enemy->enemy.unk_452c = 0;
            enemy->enemy.set_death[0] = '\0';
            enemy->die();
            enemy->enemy.flags_low |= 0x2000000;
        }
    }
    mgr->inner.time_in_stage.tick();
}

// FUNCTION: TH16 0x424f00
const char *EnemyInf::check_life_interrupts()
{
    i32 life = enemy.life.current;
    enemy.life.remaining_for_cur_attack = life;
    enemy.life.starting_value_for_next_attack = 0;
    for (u32 i = 0; i < 8; i++)
    {
        if (enemy.interrupts[i].life < 0)
        {
            continue;
        }
        enemy.life.remaining_for_cur_attack = life - enemy.interrupts[i].life;
        enemy.life.starting_value_for_next_attack = enemy.interrupts[i].life;
        if (life > enemy.interrupts[i].life)
        {
            return NULL;
        }
        if (enemy.unk_452c != 0 && enemy.own_chapter == g_Globals.chapter)
        {
            g_Globals.enemies_destroyed_in_chapter += enemy.unk_452c;
            enemy.unk_452c = 0;
        }
        enemy.life.current = enemy.interrupts[i].life;
        enemy.interrupts[i].life = -1;
        enemy.time_in_ecl.reset();
        enemy.flags_low &= ~0x1000000;
        return enemy.interrupts[i].sub_for_set_next;
    }
    return NULL;
}

// TODO: the original divides by 60 with one idiv (quotient and remainder) and keeps i in a stack
// slot; ours strength-reduces the division.
// FUNCTION: TH16 0x425010
const char *EnemyInf::check_time_interrupts()
{
    for (u32 i = 0; i < 8; i++)
    {
        if (enemy.interrupts[i].life < 0 || enemy.interrupts[i].time <= 0)
        {
            continue;
        }
        if (enemy.flags_low & 0x800000)
        {
            i32 remaining = enemy.interrupts[i].time - enemy.time_in_ecl.current;
            i32 seconds = remaining / 60;
            i32 hundredths = remaining % 60 * 100 / 60;
            if (seconds > 99)
            {
                seconds = 99;
                hundredths = 99;
            }
            g_Gui->unk_1d0 = seconds;
            g_Gui->unk_1d4 = hundredths;
        }
        if (enemy.time_in_ecl.current < enemy.interrupts[i].time)
        {
            return NULL;
        }
        enemy.life.current = enemy.interrupts[i].life;
        enemy.interrupts[i].life = -1;
        enemy.time_in_ecl.reset();
        enemy.flags_low |= 0x1000000;
        Spellcard *spellcard = g_Spellcard;
        if (!(spellcard->flags & 8))
        {
            enemy.flags_low &= ~0x1000000;
            spellcard->flags |= 0x80;
            if (spellcard->flags & 1)
            {
                if (spellcard->time.current >= 60)
                {
                    spellcard->bonus = 0;
                    spellcard->flags &= ~0x22;
                }
                else if (g_MainBomb->in_use == 1)
                {
                    spellcard->flags |= 0x20;
                }
            }
            g_EnemyManager->inner.can_still_capture_spell = 0;
        }
        else if ((spellcard->flags & 9) == 9)
        {
            g_Globals.enemies_destroyed_in_chapter += enemy.unk_452c;
        }
        enemy.unk_452c = 0;
        return enemy.interrupts[i].sub_for_set_timeout;
    }
    return NULL;
}

// FUNCTION: TH16 0x423260
int EnemyData::ecl_anm_set_sprite()
{
    i32 slot = full->context.current_context->get_int_arg(0);
    i32 script = full->context.current_context->get_int_arg(1);
    delete_vm_and_clear(anm_ids[slot]);
    if (script < 0)
    {
        return 0;
    }
    script = full->context.current_context->get_int_arg(1);
    AnmLoaded *file = g_EnemyManager->anim_statement_anms[selected_anm_index];
    anm_ids[slot] = file->create_vm_front(script, anm_layers + 7, 0);
    if (slot == 0)
    {
        anm_slot_0_script = full->context.current_context->get_int_arg(1);
        anm_slot_0_anm_index = selected_anm_index;
    }
    AnmVm *vm = get_vm_or_clear(anm_ids[slot]);
    if (slot == 0)
    {
        final_sprite_size.x = vm->scale.y * vm->sprite_size.y;
        final_sprite_size.y = vm->scale.x * vm->sprite_size.x;
    }
    if (flags_low & 0x20)
    {
        vm = get_vm(anm_ids[slot]);
        if (vm != NULL)
        {
            vm->clear_flag_lo_2_tree_inline();
        }
    }
    return 0;
}

// FUNCTION: TH16 0x41aa70
EnemyInf *EnemyManager::allocate_new_enemy(const char *sub_name, EnemyCreateParams *params, i32 unused)
{
    EnemyInf *enemy = new EnemyInf(sub_name);
    enemy->enemy.abs_pos.pos = params->pos;
    enemy->enemy.score_reward = params->score_reward;
    enemy->enemy.life.current = params->life;
    enemy->enemy.life.maximum = params->life;
    enemy->enemy.drops.main_type = params->item_drop;
    enemy->enemy.drops.extra_counts[15] = 10;
    enemy->enemy.drop_season.bonus_timer = 60;
    enemy->enemy.drop_season.max_time = 60;
    enemy->enemy.drop_season.min_count = 1;
    enemy->enemy.drop_season.damage_per_season_drop = 0;
    ((EnemyFlagsLow *)&enemy->enemy.flags_low)->mirrored = params->mirrored;
    enemy->context.primary_context.difficulty_mask = 1 << g_Globals.difficulty;
    memcpy(enemy->enemy.ecl_int_vars, params->ecl_int_vars, sizeof(params->ecl_int_vars) + sizeof(params->ecl_float_vars));
    enemy->enemy.set_invuln = 2;
    ((EnemyFlagsLow *)&enemy->enemy.flags_low)->flag_4000000 = params->flag_4000000;
    enemy->enemy.unk_278 = 0;
    enemy->unk_5744 = params->parent_enemy_id;
    if (params->life >= 1000)
    {
        ((EnemyFlagsLow *)&enemy->enemy.flags_low)->flag_40000000 = 1;
    }
    enemy->enemy.own_chapter = g_Globals.chapter;
    enemy->on_tick();
    enemy->enemy.death_sound = (enemy->enemy_id & 1) + 3;
    if (enemy->enemy.death_anm_script == 0)
    {
        enemy->enemy.death_anm_script = 0x2c;
        if (enemy->enemy.anm_slot_0_anm_index == 2)
        {
            switch (enemy->enemy.anm_slot_0_script)
            {
            case 0:
            case 20:
            case 59:
            case 62:
            case 87:
                enemy->enemy.death_anm_script = 0x2c;
                break;
            case 5:
            case 25:
            case 53:
            case 79:
                enemy->enemy.death_anm_script = 0x28;
                break;
            case 15:
            case 91:
                enemy->enemy.death_anm_script = 0x34;
                break;
            case 10:
            case 56:
            case 83:
                enemy->enemy.death_anm_script = 0x30;
                break;
            case 30:
                enemy->enemy.death_anm_script = 0x3a;
                break;
            case 35:
                enemy->enemy.death_anm_script = 0x39;
                break;
            case 40:
                enemy->enemy.death_anm_script = 0x38;
                break;
            }
        }
        enemy->enemy.death_anm_index = 1;
    }
    if (active_enemy_list_head == NULL)
    {
        enemy_count_real++;
        active_enemy_list_tail = &enemy->enemy.node_in_global_storage;
        active_enemy_list_head = &enemy->enemy.node_in_global_storage;
        return enemy;
    }
    active_enemy_list_tail->insert_after(&enemy->enemy.node_in_global_storage);
    enemy_count_real++;
    active_enemy_list_tail = &enemy->enemy.node_in_global_storage;
    return enemy;
}

// TODO: in the inlined current_instr the original loads the offset into ecx and the
// subroutine index into edx; ours swaps them. The rest matches.
// FUNCTION: TH16 0x423050
int EnemyData::ecl_enm_create()
{
    if (g_EnemyManager->enemy_count_real >= g_EnemyManager->inner.enemy_limit)
    {
        return 0;
    }
    EnemyInf *vm = full;
    EclRawInstr *instr = vm->context.current_context->current_instr();
    i32 n = (instr->args[0].i + 4) / 4;
    EnemyCreateParams params;
    memset(&params, 0, sizeof(params));
    params.pos.x = vm->context.current_context->get_float_arg_given_value(1, instr->args[n].f);
    params.pos.y = this->full->context.current_context->get_float_arg_given_value(2, instr->args[n + 1].f);
    if (instr->opcode == 300 || instr->opcode == 309 || instr->opcode == 321 || instr->opcode == 311 ||
        instr->opcode == 304)
    {
        params.pos.x += final_pos.pos.x;
        params.pos.y += final_pos.pos.y;
    }
    if (instr->opcode == 311 || instr->opcode == 304 || instr->opcode == 312 || instr->opcode == 305)
    {
        params.mirrored = 1;
    }
    if (flags_low & 0x80000)
    {
        params.pos.x *= -1.0f;
        params.mirrored ^= 1;
    }
    params.life = this->full->context.current_context->get_int_arg_given_value(3, instr->args[n + 2].i);
    params.score_reward = this->full->context.current_context->get_int_arg_given_value(4, instr->args[n + 3].i);
    params.item_drop = this->full->context.current_context->get_int_arg_given_value(5, instr->args[n + 4].i);
    memcpy(params.ecl_int_vars, ecl_int_vars, sizeof(ecl_int_vars) + sizeof(ecl_float_vars));
    params.parent_enemy_id = this->full->enemy_id;
    g_EnemyManager->allocate_new_enemy((const char *)&instr->args[1], &params, 0);
    return 0;
}

// ECL variable numbers (ExpHP's truth). Only the ones read here.
enum EclVar
{
    ECL_VAR_I0 = -9985,
    ECL_VAR_I1 = -9984,
    ECL_VAR_I2 = -9983,
    ECL_VAR_I3 = -9982,
    ECL_VAR_MISS_COUNT = -9949,
    ECL_VAR_BOMB_COUNT = -9948,
    ECL_VAR_CAN_STILL_CAPTURE = -9947,
    ECL_VAR_BOSS_I0 = -9943,
    ECL_VAR_BOSS_I1 = -9942,
    ECL_VAR_BOSS_I2 = -9941,
    ECL_VAR_BOSS_I3 = -9940,
    ECL_VAR_GI0 = -9926,
    ECL_VAR_GI1 = -9925,
    ECL_VAR_GI2 = -9924,
    ECL_VAR_GI3 = -9923,
    ECL_VAR_ABS_X = -9995,
    ECL_VAR_ABS_Y = -9994,
    ECL_VAR_REL_X = -9993,
    ECL_VAR_REL_Y = -9992,
    ECL_VAR_F0 = -9981,
    ECL_VAR_F1 = -9980,
    ECL_VAR_F2 = -9979,
    ECL_VAR_F3 = -9978,
    ECL_VAR_BOSS_F0 = -9939,
    ECL_VAR_BOSS_F1 = -9938,
    ECL_VAR_BOSS_F2 = -9937,
    ECL_VAR_BOSS_F3 = -9936,
    ECL_VAR_F4 = -9935,
    ECL_VAR_F5 = -9934,
    ECL_VAR_F6 = -9933,
    ECL_VAR_F7 = -9932,
    ECL_VAR_GF0 = -9922,
    ECL_VAR_GF1 = -9921,
    ECL_VAR_GF2 = -9920,
    ECL_VAR_GF3 = -9919,
    ECL_VAR_GF4 = -9918,
    ECL_VAR_GF5 = -9917,
    ECL_VAR_GF6 = -9916,
    ECL_VAR_GF7 = -9915,
};

// FUNCTION: TH16 0x423f80
int *EnemyInf::get_int_global_ptr(int var)
{
    EnemyInf *boss;
    switch (var)
    {
    case ECL_VAR_I0:
        return &enemy.ecl_int_vars[0];
    case ECL_VAR_I1:
        return &enemy.ecl_int_vars[1];
    case ECL_VAR_I2:
        return &enemy.ecl_int_vars[2];
    case ECL_VAR_I3:
        return &enemy.ecl_int_vars[3];
    case ECL_VAR_MISS_COUNT:
        return &g_EnemyManager->inner.miss_count;
    case ECL_VAR_BOMB_COUNT:
        return &g_EnemyManager->inner.bomb_count;
    case ECL_VAR_CAN_STILL_CAPTURE:
        return &g_EnemyManager->inner.can_still_capture_spell;
    case ECL_VAR_BOSS_I0:
        boss = g_EnemyManager->get_boss(0);
        if (boss == NULL)
        {
            return &enemy.ecl_int_vars[0];
        }
        return &boss->enemy.ecl_int_vars[0];
    case ECL_VAR_BOSS_I1:
        boss = g_EnemyManager->get_boss(0);
        if (boss == NULL)
        {
            return &enemy.ecl_int_vars[1];
        }
        return &boss->enemy.ecl_int_vars[1];
    case ECL_VAR_BOSS_I2:
        boss = g_EnemyManager->get_boss(0);
        if (boss == NULL)
        {
            return &enemy.ecl_int_vars[2];
        }
        return &boss->enemy.ecl_int_vars[2];
    case ECL_VAR_BOSS_I3:
        boss = g_EnemyManager->get_boss(0);
        if (boss == NULL)
        {
            return &enemy.ecl_int_vars[3];
        }
        return &boss->enemy.ecl_int_vars[3];
    case ECL_VAR_GI0:
        return &g_EnemyManager->inner.ecl_int_vars[0];
    case ECL_VAR_GI1:
        return &g_EnemyManager->inner.ecl_int_vars[1];
    case ECL_VAR_GI2:
        return &g_EnemyManager->inner.ecl_int_vars[2];
    case ECL_VAR_GI3:
        return &g_EnemyManager->inner.ecl_int_vars[3];
    }
    return NULL;
}

// FUNCTION: TH16 0x424c10
f32 *EnemyInf::get_float_global_ptr(int var)
{
    EnemyInf *boss;
    switch (var)
    {
    case ECL_VAR_F0:
        return &enemy.ecl_float_vars[0];
    case ECL_VAR_F1:
        return &enemy.ecl_float_vars[1];
    case ECL_VAR_F2:
        return &enemy.ecl_float_vars[2];
    case ECL_VAR_F3:
        return &enemy.ecl_float_vars[3];
    case ECL_VAR_F4:
        return &enemy.ecl_float_vars[4];
    case ECL_VAR_F5:
        return &enemy.ecl_float_vars[5];
    case ECL_VAR_F6:
        return &enemy.ecl_float_vars[6];
    case ECL_VAR_F7:
        return &enemy.ecl_float_vars[7];
    case ECL_VAR_BOSS_F0:
        boss = g_EnemyManager->get_boss(0);
        if (boss == NULL)
        {
            return &enemy.ecl_float_vars[0];
        }
        return &boss->enemy.ecl_float_vars[0];
    case ECL_VAR_BOSS_F1:
        boss = g_EnemyManager->get_boss(0);
        if (boss == NULL)
        {
            return &enemy.ecl_float_vars[1];
        }
        return &boss->enemy.ecl_float_vars[1];
    case ECL_VAR_BOSS_F2:
        boss = g_EnemyManager->get_boss(0);
        if (boss == NULL)
        {
            return &enemy.ecl_float_vars[2];
        }
        return &boss->enemy.ecl_float_vars[2];
    case ECL_VAR_BOSS_F3:
        boss = g_EnemyManager->get_boss(0);
        if (boss == NULL)
        {
            return &enemy.ecl_float_vars[3];
        }
        return &boss->enemy.ecl_float_vars[3];
    case ECL_VAR_GF0:
        return &g_EnemyManager->inner.ecl_float_vars[0];
    case ECL_VAR_GF1:
        return &g_EnemyManager->inner.ecl_float_vars[1];
    case ECL_VAR_GF2:
        return &g_EnemyManager->inner.ecl_float_vars[2];
    case ECL_VAR_GF3:
        return &g_EnemyManager->inner.ecl_float_vars[3];
    case ECL_VAR_GF4:
        return &g_EnemyManager->inner.ecl_float_vars[4];
    case ECL_VAR_GF5:
        return &g_EnemyManager->inner.ecl_float_vars[5];
    case ECL_VAR_GF6:
        return &g_EnemyManager->inner.ecl_float_vars[6];
    case ECL_VAR_GF7:
        return &g_EnemyManager->inner.ecl_float_vars[7];
    case ECL_VAR_ABS_X:
        return &enemy.abs_pos.pos.x;
    case ECL_VAR_ABS_Y:
        return &enemy.abs_pos.pos.y;
    case ECL_VAR_REL_X:
        return &enemy.rel_pos.pos.x;
    case ECL_VAR_REL_Y:
        return &enemy.rel_pos.pos.y;
    }
    return NULL;
}

// FUNCTION: TH16 0x423810
int EnemyInf::get_int_global(int var)
{
    EnemyInf *boss;
    f32 dx;
    f32 dy;
    switch (var)
    {
    case -10000:
        return g_replay_safe_rng.rand_u32() & 0x7fffffff;
    case -9999:
        return (i32)g_replay_safe_rng.randf_0_to_1();
    case -9987:
        return (i32)g_replay_safe_rng.randf_neg_1_to_1();
    case -9997:
    case -9977:
        return (i32)enemy.final_pos.pos.x;
    case -9996:
    case -9976:
        return (i32)enemy.final_pos.pos.y;
    case -9995:
    case -9975:
        return (i32)enemy.abs_pos.pos.x;
    case -9994:
    case -9974:
        return (i32)enemy.abs_pos.pos.y;
    case -9993:
    case -9973:
        return (i32)enemy.rel_pos.pos.x;
    case -9992:
    case -9972:
        return (i32)enemy.rel_pos.pos.y;
    case -9991:
    case -9965:
        return (i32)g_Player->inner.pos.x;
    case -9990:
    case -9964:
        return (i32)g_Player->inner.pos.y;
    case -9988:
        return enemy.time_in_ecl.current;
    case -9986:
        return ((EnemyFlagsLow *)&enemy.flags_low)->flag_1000000;
    case -9971:
        return (i32)enemy.abs_pos.angle.value;
    case -9970:
        return (i32)enemy.rel_pos.angle.value;
    case -9958:
        return (i32)zun_atan2f(enemy.final_pos.velocity.y, enemy.final_pos.velocity.x);
    case -9969:
        return (i32)enemy.abs_pos.speed;
    case -9968:
        return (i32)enemy.rel_pos.speed;
    case -9967:
        return (i32)enemy.abs_pos.radial_dist;
    case -9966:
        return (i32)enemy.rel_pos.radial_dist;
    case -9963:
        return (i32)g_EnemyManager->get_boss(0)->enemy.final_pos.pos.x;
    case -9962:
        return (i32)g_EnemyManager->get_boss(0)->enemy.final_pos.pos.y;
    case -9961:
        return enemy.anm_ids[0].find_or_clear()->unk_49c;
    case -9960:
        return g_Globals.rank;
    case -9959:
        return g_Globals.difficulty;
    case -9954:
        return enemy.life.current;
    case -9953:
        return g_Globals.difficulty == DIFFICULTY_EASY;
    case -9952:
        return g_Globals.difficulty == DIFFICULTY_NORMAL;
    case -9951:
        return g_Globals.difficulty == DIFFICULTY_HARD;
    case -9950:
        return g_Globals.difficulty == DIFFICULTY_LUNATIC;
    case -9949:
        return g_EnemyManager->inner.miss_count;
    case -9948:
        return g_EnemyManager->inner.bomb_count;
    case -9947:
        return g_EnemyManager->inner.can_still_capture_spell;
    case -9946:
        return g_EnemyManager->enemy_count_real;
    case -9908:
        return g_EnemyManager->get_enemy_count();
    case -9945:
        return g_Globals.subshot + g_Globals.character;
    case -9944:
        dy = enemy.final_pos.pos.y - g_Player->inner.pos.y;
        dx = enemy.final_pos.pos.x - g_Player->inner.pos.x;
        return (i32)sqrtf(dx * dx + dy * dy);
    case -9931:
        return g_EnemyManager->inner.last_enemy_id;
    case -9930:
        return g_Globals.power;
    case -9927:
        if (g_GameThread->replay_mode == 0 && g_Supervisor.unk_700 != 0)
        {
            return 1;
        }
        break;
    case -9957:
        return 1;
    case -9943:
        boss = g_EnemyManager->get_boss(0);
        if (boss != NULL)
        {
            return boss->enemy.ecl_int_vars[0];
        }
        break;
    case -9942:
        boss = g_EnemyManager->get_boss(0);
        if (boss != NULL)
        {
            return boss->enemy.ecl_int_vars[1];
        }
        break;
    case -9941:
        boss = g_EnemyManager->get_boss(0);
        if (boss != NULL)
        {
            return boss->enemy.ecl_int_vars[2];
        }
        break;
    case -9940:
        boss = g_EnemyManager->get_boss(0);
        if (boss != NULL)
        {
            return boss->enemy.ecl_int_vars[3];
        }
        break;
    case -9939:
        boss = g_EnemyManager->get_boss(0);
        if (boss != NULL)
        {
            return (i32)boss->enemy.ecl_float_vars[0];
        }
        break;
    case -9938:
        boss = g_EnemyManager->get_boss(0);
        if (boss != NULL)
        {
            return (i32)boss->enemy.ecl_float_vars[1];
        }
        break;
    case -9937:
        boss = g_EnemyManager->get_boss(0);
        if (boss != NULL)
        {
            return (i32)boss->enemy.ecl_float_vars[2];
        }
        break;
    case -9936:
        boss = g_EnemyManager->get_boss(0);
        if (boss != NULL)
        {
            return (i32)boss->enemy.ecl_float_vars[3];
        }
        break;
    case -9911:
        boss = g_EnemyManager->get_boss(0);
        if (boss != NULL)
        {
            return (i32)zun_atan2f(boss->enemy.final_pos.velocity.y, boss->enemy.final_pos.velocity.x);
        }
        break;
    case -9910:
        boss = g_EnemyManager->get_boss(0);
        if (boss != NULL)
        {
            return (i32)boss->enemy.abs_pos.speed;
        }
        break;
    case -9909:
        return unk_5744;
    case -9985:
        return enemy.ecl_int_vars[0];
    case -9984:
        return enemy.ecl_int_vars[1];
    case -9983:
        return enemy.ecl_int_vars[2];
    case -9982:
        return enemy.ecl_int_vars[3];
    case -9981:
        return (i32)enemy.ecl_float_vars[0];
    case -9980:
        return (i32)enemy.ecl_float_vars[1];
    case -9979:
        return (i32)enemy.ecl_float_vars[2];
    case -9978:
        return (i32)enemy.ecl_float_vars[3];
    case -9935:
        return (i32)enemy.ecl_float_vars[4];
    case -9934:
        return (i32)enemy.ecl_float_vars[5];
    case -9933:
        return (i32)enemy.ecl_float_vars[6];
    case -9932:
        return (i32)enemy.ecl_float_vars[7];
    case -9926:
        return g_EnemyManager->inner.ecl_int_vars[0];
    case -9925:
        return g_EnemyManager->inner.ecl_int_vars[1];
    case -9924:
        return g_EnemyManager->inner.ecl_int_vars[2];
    case -9923:
        return g_EnemyManager->inner.ecl_int_vars[3];
    case -9922:
        return (i32)g_EnemyManager->inner.ecl_float_vars[0];
    case -9921:
        return (i32)g_EnemyManager->inner.ecl_float_vars[1];
    case -9920:
        return (i32)g_EnemyManager->inner.ecl_float_vars[2];
    case -9919:
        return (i32)g_EnemyManager->inner.ecl_float_vars[3];
    case -9918:
        return (i32)g_EnemyManager->inner.ecl_float_vars[4];
    case -9917:
        return (i32)g_EnemyManager->inner.ecl_float_vars[5];
    case -9916:
        return (i32)g_EnemyManager->inner.ecl_float_vars[6];
    case -9915:
        return (i32)g_EnemyManager->inner.ecl_float_vars[7];
    case -9914:
        return enemy_id;
    case -9907:
        return g_Globals.spell_id;
    case -9906:
        return ((EnemyFlagsLow *)&enemy.flags_low)->mirrored;
    case -9905:
        return g_Globals.chapter;
    case -9904:
        return g_Globals.miss_count;
    case -9903:
        return g_Globals.subseason;
    }
    return 0;
}

// FUNCTION: TH16 0x424110
f32 EnemyInf::get_float_global(int var)
{
    EnemyInf *boss;
    f32 dx;
    f32 dy;
    switch (var)
    {
    case -10000:
        return g_replay_safe_rng.rand_u32() & 0x7fffffff;
    case -9999:
        return g_replay_safe_rng.randf_0_to_1();
    case -9987:
        return g_replay_safe_rng.randf_neg_1_to_1();
    case -9998:
        return g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
    case -9997:
    case -9977:
        return enemy.final_pos.pos.x;
    case -9996:
    case -9976:
        return enemy.final_pos.pos.y;
    case -9995:
    case -9975:
        return enemy.abs_pos.pos.x;
    case -9994:
    case -9974:
        return enemy.abs_pos.pos.y;
    case -9993:
    case -9973:
        return enemy.rel_pos.pos.x;
    case -9992:
    case -9972:
        return enemy.rel_pos.pos.y;
    case -9991:
    case -9965:
        return g_Player->inner.pos.x;
    case -9990:
    case -9964:
        return g_Player->inner.pos.y;
    case -9989:
        return g_Player->angle_to_player(&enemy.final_pos.pos);
    case -9988:
        return enemy.time_in_ecl.current_f;
    case -9986:
        return ((EnemyFlagsLow *)&enemy.flags_low)->flag_1000000;
    case -9985:
        return enemy.ecl_int_vars[0];
    case -9984:
        return enemy.ecl_int_vars[1];
    case -9983:
        return enemy.ecl_int_vars[2];
    case -9982:
        return enemy.ecl_int_vars[3];
    case -9981:
        return enemy.ecl_float_vars[0];
    case -9980:
        return enemy.ecl_float_vars[1];
    case -9979:
        return enemy.ecl_float_vars[2];
    case -9978:
        return enemy.ecl_float_vars[3];
    case -9935:
        return enemy.ecl_float_vars[4];
    case -9934:
        return enemy.ecl_float_vars[5];
    case -9933:
        return enemy.ecl_float_vars[6];
    case -9932:
        return enemy.ecl_float_vars[7];
    case -9943:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.ecl_int_vars[0] : 0.0f;
    case -9942:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.ecl_int_vars[1] : 0.0f;
    case -9941:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.ecl_int_vars[2] : 0.0f;
    case -9940:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.ecl_int_vars[3] : 0.0f;
    case -9939:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.ecl_float_vars[0] : 0.0f;
    case -9938:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.ecl_float_vars[1] : 0.0f;
    case -9937:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.ecl_float_vars[2] : 0.0f;
    case -9936:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.ecl_float_vars[3] : 0.0f;
    case -9911:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? zun_atan2f(boss->enemy.final_pos.velocity.y, boss->enemy.final_pos.velocity.x) : 0.0f;
    case -9910:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.abs_pos.speed : 0.0f;
    case -9971:
        return enemy.abs_pos.angle.value;
    case -9970:
        return enemy.rel_pos.angle.value;
    case -9958:
        return zun_atan2f(enemy.final_pos.velocity.y, enemy.final_pos.velocity.x);
    case -9969:
        return enemy.abs_pos.speed;
    case -9968:
        return enemy.rel_pos.speed;
    case -9967:
        return enemy.abs_pos.radial_dist;
    case -9966:
        return enemy.rel_pos.radial_dist;
    case -9963:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.final_pos.pos.x : 0.0f;
    case -9962:
        boss = g_EnemyManager->get_boss(0);
        return boss != NULL ? boss->enemy.final_pos.pos.y : 128.0f;
    case -9960:
        return g_Globals.rank;
    case -9959:
        return g_Globals.difficulty;
    case -9957:
        return 1.0f;
    case -9956:
        return g_Player->angle_to_player(&enemy.abs_pos.pos);
    case -9955:
        return g_Player->angle_to_player(&enemy.rel_pos.pos);
    case -9954:
        return enemy.life.current;
    case -9953:
        return g_Globals.difficulty == 0.0f;
    case -9952:
        return g_Globals.difficulty == 1.0f;
    case -9951:
        return g_Globals.difficulty == 2.0f;
    case -9950:
        return g_Globals.difficulty == 3.0f;
    case -9949:
        return g_EnemyManager->inner.miss_count;
    case -9948:
        return g_EnemyManager->inner.bomb_count;
    case -9947:
        return g_EnemyManager->inner.can_still_capture_spell;
    case -9946:
        return g_EnemyManager->enemy_count_real;
    case -9908:
        return g_EnemyManager->get_enemy_count();
    case -9945:
        return g_Globals.subshot + g_Globals.character;
    case -9944:
        dy = enemy.final_pos.pos.y - g_Player->inner.pos.y;
        dx = enemy.final_pos.pos.x - g_Player->inner.pos.x;
        return sqrtf(dx * dx + dy * dy);
    case -9931:
        return (u32)g_EnemyManager->inner.last_enemy_id;
    case -9930:
        return g_Globals.power;
    case -9927:
        return (f32)(g_GameThread->replay_mode == 0) && g_Supervisor.unk_700 != 0;
    case -9909:
        return (u32)unk_5744;
    case -9926:
        return g_EnemyManager->inner.ecl_int_vars[0];
    case -9925:
        return g_EnemyManager->inner.ecl_int_vars[1];
    case -9924:
        return g_EnemyManager->inner.ecl_int_vars[2];
    case -9923:
        return g_EnemyManager->inner.ecl_int_vars[3];
    case -9922:
        return g_EnemyManager->inner.ecl_float_vars[0];
    case -9921:
        return g_EnemyManager->inner.ecl_float_vars[1];
    case -9920:
        return g_EnemyManager->inner.ecl_float_vars[2];
    case -9919:
        return g_EnemyManager->inner.ecl_float_vars[3];
    case -9918:
        return g_EnemyManager->inner.ecl_float_vars[4];
    case -9917:
        return g_EnemyManager->inner.ecl_float_vars[5];
    case -9916:
        return g_EnemyManager->inner.ecl_float_vars[6];
    case -9915:
        return g_EnemyManager->inner.ecl_float_vars[7];
    case -9914:
        return (u32)enemy_id;
    case -9907:
        return g_Globals.spell_id;
    case -9906:
        return (enemy.flags_low >> 19) & 1;
    case -9905:
        return g_Globals.chapter;
    case -9903:
        return g_Globals.subseason;
    }
    return 0.0f;
}

// FUNCTION: TH16 0x41dca0
int EnemyInf::run_over_300()
{
    return enemy.ecl_run_over_300();
}

static inline void anm_set_rgb1(AnmVm *vm, i32 r, i32 g, i32 b)
{
    vm->color_1.r = r;
    vm->color_1.g = g;
    vm->color_1.b = b;
}

static inline void anm_set_scale(AnmVm *vm, f32 x, f32 y)
{
    vm->flags_lo |= ANM_VM_SCALE_CHANGED;
    vm->scale.x = x;
    vm->scale.y = y;
}

static inline void anm_set_scale_2(AnmVm *vm, f32 x, f32 y)
{
    vm->flags_lo |= ANM_VM_SCALE_CHANGED;
    vm->scale_2.x = x;
    vm->scale_2.y = y;
}

// TODO: register allocation: the original keeps full in ecx (reloading the context from it) and
// spills vm, with this in edi and vm in ebx.
// FUNCTION: TH16 0x4233a0
void EnemyData::ecl_anm_vm_instr()
{
    EclRawInstr *instr = full->context.current_context->current_instr();
    i32 slot = full->context.current_context->get_int_arg(0);
    if ((u32)slot > 15)
    {
        return;
    }
    AnmVm *vm = get_vm_or_clear(anm_ids[slot]);
    if (vm == NULL)
    {
        return;
    }
    switch ((i16)instr->opcode)
    {
    case 319:
        vm->rotation.z = full->context.current_context->get_float_arg(1);
        vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
        break;
    case 329:
        anm_set_scale(vm, full->context.current_context->get_float_arg(1), full->context.current_context->get_float_arg(2));
        break;
    case 335:
        anm_set_scale_2(vm, full->context.current_context->get_float_arg(1),
                        full->context.current_context->get_float_arg(2));
        break;
    case 330:
        vm->scale_to(full->context.current_context->get_int_arg(1), full->context.current_context->get_int_arg(2),
                     full->context.current_context->get_float_arg(3), full->context.current_context->get_float_arg(4));
        break;
    case 325:
        anm_set_rgb1(vm, full->context.current_context->get_int_arg(1), full->context.current_context->get_int_arg(2),
                     full->context.current_context->get_int_arg(3));
        break;
    case 326:
    {
        ZunColor color;
        color.r = full->context.current_context->get_int_arg(3);
        color.g = full->context.current_context->get_int_arg(4);
        color.b = full->context.current_context->get_int_arg(5);
        vm->fade_rgb1(full->context.current_context->get_int_arg(1), full->context.current_context->get_int_arg(2),
                      &color);
        break;
    }
    case 327:
        vm->color_1.a = full->context.current_context->get_int_arg(1);
        break;
    case 328:
        vm->fade_alpha1(full->context.current_context->get_int_arg(1), full->context.current_context->get_int_arg(2),
                        full->context.current_context->get_int_arg(3));
        break;
    case 331:
        vm->color_2.a = full->context.current_context->get_int_arg(1);
        break;
    case 332:
        vm->fade_alpha2(full->context.current_context->get_int_arg(1), full->context.current_context->get_int_arg(2),
                        full->context.current_context->get_int_arg(3));
        break;
    case 333:
    {
        Float3 goal(full->context.current_context->get_float_arg(3), full->context.current_context->get_float_arg(4),
                    0.0f);
        vm->set_pos_time(full->context.current_context->get_int_arg(1), full->context.current_context->get_int_arg(2),
                         &anm_ids[full->context.current_context->get_int_arg(0)].find_or_clear()->entity_pos, &goal);
        break;
    }
    case 336:
        vm->set_layer(full->context.current_context->get_int_arg(1));
        break;
    case 337:
        ((AnmVmFlagsLoBits *)&vm->flags_lo)->blend_mode = (u8)full->context.current_context->get_int_arg(1);
        break;
    }
}

// TODO: frame layout (the original keeps the zero vector higher up), &rel_pos stays in esi, and
// ours combines the two flag tests of the vertical off-screen check.
// FUNCTION: TH16 0x41bb50
int EnemyData::step_interpolators()
{
    prev_final_pos = final_pos;
    if (abs_angle_i.end_time != 0 && (abs_pos.flags & 0xf) != POSVEL_MODE_CIRCLE &&
        (abs_pos.flags & 0xf) != POSVEL_MODE_ELLIPSE)
    {
        abs_pos.angle.value = wrap_angle(wrap_angle(abs_angle_i.step()));
    }
    if (abs_speed_i.end_time != 0)
    {
        abs_pos.speed = abs_speed_i.step();
    }
    if (rel_angle_i.end_time != 0 && (rel_pos.flags & 0xf) != POSVEL_MODE_CIRCLE &&
        (rel_pos.flags & 0xf) != POSVEL_MODE_ELLIPSE)
    {
        rel_pos.angle.value = wrap_angle(wrap_angle(rel_angle_i.step()));
    }
    if (rel_speed_i.end_time != 0)
    {
        rel_pos.speed = rel_speed_i.step();
    }
    if (abs_radial_dist_i.end_time != 0)
    {
        D3DXVECTOR2 v = abs_radial_dist_i.step_radial_dist();
        abs_pos.radial_dist = v.x;
        abs_pos.radial_speed = v.y;
    }
    if (rel_radial_dist_i.end_time != 0)
    {
        D3DXVECTOR2 v = rel_radial_dist_i.step_radial_dist();
        rel_pos.radial_dist = v.x;
        rel_pos.radial_speed = v.y;
    }
    if (abs_pos_i.end_time != 0)
    {
        abs_pos.velocity = abs_pos_i.step() - abs_pos.pos;
    }
    else
    {
        abs_pos.update_secondary_fields();
    }
    if (rel_pos_i.end_time != 0)
    {
        rel_pos.velocity = rel_pos_i.step() - rel_pos.pos;
    }
    else
    {
        rel_pos.update_secondary_fields();
    }
    abs_pos.step();
    if (flags_low & 0x4000000)
    {
        rel_pos.pos.x += g_Supervisor.cameras[0].unk_104.x;
        rel_pos.pos.y += g_Supervisor.cameras[0].unk_104.y;
        rel_pos.pos.z += g_Supervisor.cameras[0].unk_104.z;
    }
    rel_pos.step();
    update_final_pos();
    if (((EnemyFlagsLow *)&flags_low)->directional_anm)
    {
        i32 dir = -0.03f > final_pos.velocity.x ? -1 : final_pos.velocity.x > 0.03f;
        if (unk_274 != dir)
        {
            i32 script_offset = 0;
            switch (unk_274)
            {
            case -1:
                script_offset = dir != 0 ? 2 : 3;
                break;
            case 0:
                script_offset = (dir != -1) + 1;
                break;
            case 1:
                script_offset = dir == 0 ? 4 : 1;
                break;
            }
            AnmVm *vm = get_vm_or_clear(anm_ids[0]);
            AnmLoaded *file = g_EnemyManager->anim_statement_anms[anm_slot_0_anm_index];
            Float3 zero(0.0f, 0.0f, 0.0f);
            Float3 pos;
            if (vm != NULL)
            {
                pos = vm->pos;
                delete_vm_and_clear(anm_ids[0]);
            }
            else
            {
                pos = zero;
            }
            i32 layer = anm_layers + 7;
            i32 script = anm_set_main + script_offset;
            ENTER_CS(CS_ANM_MANAGER);
            file->vm_count++;
            AnmVm *new_vm = g_AnmManager->allocate_vm();
            file->copy_vm(new_vm, script);
            new_vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
            if (layer >= 0)
            {
                new_vm->layer = layer;
                if (layer <= 23)
                {
                    new_vm->flags_hi &= ~ANM_VM_LAYER_UI;
                    new_vm->flags_hi |= ANM_VM_LAYER_SET;
                }
            }
            new_vm->entity_pos = pos;
            new_vm->rotation.z = 0.0f;
            new_vm->run();
            new_vm->mode_of_create_child = 8;
            AnmId id;
            id = g_AnmManager->insert_in_world_list_back(new_vm);
            LEAVE_CS(CS_ANM_MANAGER);
            anm_ids[0] = id;
            unk_274 = dir;
        }
    }
    AnmVm *vm = get_vm_or_clear(anm_ids[0]);
    if (vm != NULL)
    {
        final_sprite_size.x = fabsf(vm->scale.y * vm->sprite_size.y);
        final_sprite_size.y = fabsf(vm->scale.x * vm->sprite_size.x);
    }
    EnemyFlagsLow *flags = (EnemyFlagsLow *)&flags_low;
    f32 half = final_sprite_size.x * 0.5f;
    if (-192.0f > final_pos.pos.x + half || final_pos.pos.x - half > 192.0f)
    {
        if (flags->was_on_screen && !flags->no_offscreen_delete_x)
        {
            return -1;
        }
    }
    else
    {
        half = final_sprite_size.y * 0.5f;
        if (0.0f > final_pos.pos.y + half || final_pos.pos.y - half > 448.0f)
        {
            if (flags->was_on_screen)
            {
                if (!flags->no_offscreen_delete_y)
                {
                    return -1;
                }
            }
        }
        else
        {
            flags->was_on_screen = 1;
        }
    }
    return 0;
}
