#pragma once

#include <d3dx9math.h>

#include "decomp.h"
#include "types.h"

// Partial: only what unit 2 (Stage, Bomb) uses so far. Layouts from ExpHP.

// Handle to a running ANM VM; 0 means none.
struct AnmId
{
    u32 id;

    AnmId()
    {
        id = 0;
    }
};

enum AnmVmFlagsHi
{
    ANM_VM_CREATED_BY_GAME = 1 << 10,
    ANM_VM_NO_PARENT_POS = 1 << 16,
    ANM_VM_LAYER_SET = 1 << 18,
    ANM_VM_LAYER_UI = 1 << 19,
    ANM_VM_ROTATE_WITH_PARENT = 1 << 23,
};

struct AnmVm
{
    u8 unk_0[0x18];
    u32 layer;
    u8 unk_1c[0x2c - 0x1c];
    D3DXVECTOR3 pos;
    D3DXVECTOR3 rotation;
    u8 unk_44[0x50 - 0x44];
    D3DXVECTOR2 scale;
    u8 unk_58[0x4a0 - 0x58];
    i32 int_vars[4];
    f32 float_vars[4];
    u8 unk_4c0[0x4e0 - 0x4c0];
    D3DXVECTOR3 pos_2;
    u8 unk_4ec[0x51c - 0x4ec];
    i32 mode_of_create_child;
    u8 unk_520[0x530 - 0x520];
    u32 flags_lo;
    u32 flags_hi;
    AnmId id;
    u8 unk_53c[0x5ac - 0x53c];
    AnmVm *parent;
    AnmVm *unk_5b0;
    u8 unk_5b4[0x5e0 - 0x5b4];
    // Position of the game object the VM belongs to.
    D3DXVECTOR3 entity_pos;
    u8 unk_5ec[0x5fc - 0x5ec];

    // 0x40e490. Position including entity_pos and every parent's.
    D3DXVECTOR3 world_pos();
    // 0x45f980
    void run();
};

struct AnmLoaded
{
    u8 unk_0[0x134];
    // Counts VMs created from this file.
    i32 vm_count;
    u8 unk_138[0x13c - 0x138];

    // 0x407b20
    void copy_vm(AnmVm *vm, i32 script);
    // 0x40d460
    void copy_vm_and_run(AnmVm *vm, i32 script);
    // 0x40e5c0. Creates a VM running the script at pos (entity_pos), with
    // the given z rotation, on the given layer unless negative.
    HARNESS_CALLED AnmId create_vm(i32 script, D3DXVECTOR3 *pos, f32 rotation, i32 layer, i32 unused);
};

struct AnmManager
{
    // 0x46efa0
    AnmVm *get_vm_with_id(AnmId id);
    // 0x46f1c0. Marks the VM and its children for deletion. Reaches the
    // manager through g_AnmManager, so LTCG drops the unused this.
    HARNESS_CALLED void delete_vm(AnmId id);
    // 0x46f600. Reaches the manager through g_AnmManager.
    static AnmVm *allocate_vm();
    // 0x46e7d0. Reaches the manager through g_AnmManager.
    static AnmId __stdcall insert_in_world_list_back(AnmVm *vm);
};

extern AnmManager *g_AnmManager;

// Deletes the VM (if still alive) and forgets the id.
inline void delete_vm_and_clear(AnmId &id)
{
    g_AnmManager->delete_vm(id);
    id.id = 0;
}

// Looks the VM up and forgets the id if it is gone.
inline AnmVm *get_vm_or_clear(AnmId &id)
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    if (vm == NULL)
    {
        id.id = 0;
    }
    return vm;
}
