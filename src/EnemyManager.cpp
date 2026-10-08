#include <string.h>

#include "AnmManager.h"
#include "EnemyManager.h"

// Boss i, NULL if there is none (the last enemy of the list if its id is
// gone, like EnemyRef::get).
// FUNCTION: TH16 0x417580
HARNESS_CALLED EnemyInf *EnemyManager::get_boss(i32 i)
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

// Disables the VMs of the enemy ANM files (slots 10 to 15), then deletes every
// enemy. The first loop walks the byte offset of the loaded_anms slots, which
// gives the original's offset counter compared against the end; an index loop
// counts down beside a pointer instead.
// TODO: the original encodes the slot address as [offset + manager] (base and
// index registers swapped).
// FUNCTION: TH16 0x4185b0
void EnemyManager::destroy_all()
{
    for (u32 off = offsetof(AnmManager, loaded_anms[10]); off < offsetof(AnmManager, loaded_anms[16]);
         off += sizeof(AnmLoaded *))
    {
        g_AnmManager->disable_vms_from_anm_file(*(AnmLoaded **)((u8 *)g_AnmManager + off));
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
