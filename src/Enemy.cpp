#include "Enemy.h"
#include "Fog.h"
#include "AnmManager.h"
#include "BulletManager.h"
#include "CriticalSections.h"
#include "EffectManager.h"
#include "EnemyManager.h"
#include "GameThread.h"
#include "Player.h"
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
i32 *EnemyData::get_int_arg_ptr(int index)
{
    return full->context.current_context->get_int_arg_ptr(index);
}

// FUNCTION: TH16 0x425200
f32 EnemyData::get_float_arg(int index)
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
