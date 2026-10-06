#include <string.h>

#include "AnmManager.h"
#include "EnemyManager.h"

// FUNCTION: TH16 0x417580
EnemyInf *EnemyManager::get_boss(i32 i)
{
    EnemyInf *e = NULL;
    i32 id = g_EnemyManager->boss_ids[i];
    if (!id)
    {
        return NULL;
    }
    ZunList<EnemyInf> *node;
    for (node = g_EnemyManager->active_enemy_list_head; node != NULL; node = node->next)
    {
        e = node->entry;
        if (e->id == id)
        {
            return e;
        }
    }
    return e;
}

// TODO: the original keeps g_AnmManager in a register across the loop; LTCG
// knows the (here undecompiled) callee leaves it alone.
// FUNCTION: TH16 0x4185b0
void EnemyManager::destroy_all()
{
    for (u32 i = 10; i < 16; i++)
    {
        g_AnmManager->disable_vms_from_anm_file(g_AnmManager->preloaded[i]);
    }
    ZunList<EnemyInf> *node = active_enemy_list_head;
    while (node != NULL)
    {
        ZunList<EnemyInf> *next = node->next;
        delete node->entry;
        node = next;
    }
    for (i32 i = 0; i < 16; i++)
    {
        boss_ids[i] = 0;
    }
    node = owned_list_188;
    while (node != NULL)
    {
        ZunList<EnemyInf> *next = node->next;
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
