#include <string.h>

#include "AnmManager.h"
#include "EnemyManager.h"

// FUNCTION: TH16 0x417580
EnemyInf *EnemyManager::get_boss(i32 i)
{
    EnemyInf *e = NULL;
    i32 id = g_EnemyManager->inner.boss_ids[i];
    if (!id)
    {
        return NULL;
    }
    EnemyList *node;
    for (node = g_EnemyManager->active_enemy_list_head; node != NULL; node = node->next)
    {
        e = node->entry;
        if (e->enemy_id == id)
        {
            return e;
        }
    }
    return e;
}

// FUNCTION: TH16 0x42ca00
HARNESS_CALLED i32 EnemyManager::reset_for_stage(i32 unused)
{
    inner.time_in_stage.reset();
    active_enemy_list_head = NULL;
    active_enemy_list_tail = NULL;
    return 0;
}

// TODO: the original keeps g_AnmManager in a register across the loop; LTCG
// knows the (here undecompiled) callee leaves it alone.
// FUNCTION: TH16 0x4185b0
void EnemyManager::destroy_all()
{
    for (u32 i = 10; i < 16; i++)
    {
        g_AnmManager->disable_vms_from_anm_file(g_AnmManager->loaded_anms[i]);
    }
    EnemyList *node = active_enemy_list_head;
    while (node != NULL)
    {
        EnemyList *next = node->next;
        delete node->entry;
        node = next;
    }
    for (i32 i = 0; i < 16; i++)
    {
        inner.boss_ids[i] = 0;
    }
    node = owned_list_188;
    while (node != NULL)
    {
        EnemyList *next = node->next;
        delete node->entry;
        node = next;
    }
    if (on_tick != NULL)
    {
        on_tick->flags &= ~UPDATE_FUNC_ACTIVE;
    }
    if (on_draw != NULL)
    {
        on_draw->flags &= ~UPDATE_FUNC_ACTIVE;
    }
}
