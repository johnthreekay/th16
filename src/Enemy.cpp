#include "Enemy.h"
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
#include "Rng.h"
#include "SoundManager.h"
#include "Supervisor.h"
#include "UpdateFunc.h"

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

// TODO: the memset arguments for drops are pushed a few stores later in the original, and
// next_enemy_id is read twice. The latter changed once set_boss_id got its harness caller.
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
int EnemyManager::get_enemy_count()
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
BOOL EnemyManager::is_enemy_alive(int id)
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

// TODO: create_func/register_on_* still get this in ecx here (LTCG drops it in the original),
// and the inlined UpdateFunc constructor keeps its stores in the original.
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
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
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

// TODO: register allocation differs in the inlined ZunTimer::tick (the original keeps 1.0f in xmm2).
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
        g_Player->inner.flags |= 0x20;
    }
    else
    {
        g_Player->inner.flags &= ~0x20;
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

// FUNCTION: TH16 0x4251d0
i32 EnemyData::get_int_arg(int index)
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
f32 *EnemyData::get_float_arg_ptr(int index)
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

// GLOBAL: TH16 0x4a6dc0
EnemyManager *g_EnemyManager;

// TODO: inlined delete_vm loads the child list before storing the flags.
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

// TODO: the original multiplies x as dist * x with dist loaded into a register; ours loads x.
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
EnemyFuncSetFunc const g_ecl_func_sets[3] = {NULL, ecl_funcset_cancel_near_player, ecl_funcset_zero_power};

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

// TODO: the original keeps the summed position in xmm1-3 across find_or_clear (LTCG knows the
// stubbed get_vm_with_id leaves them alone) and saves ebx/edi up front.
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
            Float3 pos = anm_pos_array[i] + final_pos.pos;
            if (unk_224[i] >= 0)
            {
                AnmVm *base = anm_ids[unk_224[i]].find_or_clear();
                if (base != NULL)
                {
                    pos += base->pos;
                }
            }
            vm->entity_pos = pos;
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

// TODO: the original frame has 4 more bytes (as in other functions with Float3 temporaries).
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

// TODO: the inlined tick adds speed and current_f the other way round (register choice).
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
    mgr->inner.time_in_stage.tick();
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

// TODO: the inlined tick adds speed and current_f the other way round (register choice).
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
