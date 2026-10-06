#pragma once

#include "decomp.h"
#include "types.h"

// Partial: only what unit 2 (Stage, Bomb) uses so far.

// Handle to a running ANM VM; 0 means none.
struct AnmId
{
    u32 id;

    AnmId()
    {
        id = 0;
    }
};

struct AnmVm;
struct AnmLoaded;

struct AnmManager
{
    // 0x46efa0
    AnmVm *get_vm_with_id(AnmId id);
    // 0x46f1c0. Marks the VM and its children for deletion. Reaches the
    // manager through g_AnmManager, so LTCG drops the unused this.
    HARNESS_CALLED void delete_vm(AnmId id);
};

extern AnmManager *g_AnmManager;

// Deletes the VM (if still alive) and forgets the id.
inline void delete_vm_and_clear(AnmId &id)
{
    g_AnmManager->delete_vm(id);
    id.id = 0;
}
