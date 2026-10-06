#pragma once

#include "AnmVm.h"
#include "ZunMath.h"
#include "decomp.h"
#include "types.h"

// Only the parts decompiled code needs so far. Layouts from ExpHP's
// th-re-data.

// One loaded ANM file.
struct AnmLoaded
{
    i32 slot_num;
    char name[0x104];
    u8 unk_108[0x134 - 0x108];
    // Counts VMs created from this file.
    i32 vm_count;
    u8 unk_138[0x13c - 0x138];

    void set_sprite(AnmVm *vm, i32 sprite);
    // 0x407b20
    void copy_vm(AnmVm *vm, i32 script);
    // 0x40d460
    void copy_vm_and_run(AnmVm *vm, i32 script);
    // 0x40e5c0. Creates a VM running the script at pos (entity_pos), with
    // the given z rotation, on the given layer unless negative.
    HARNESS_CALLED AnmId create_vm(i32 script, Float3 *pos, f32 rotation, i32 layer, i32 unused);

    void init_vm_with_sprite(AnmVm *vm, i32 sprite)
    {
        vm->wipe();
        vm->anm_loaded_index = slot_num;
        set_sprite(vm, sprite);
    }

    // Frees what the file owns (ExpHP: AnmLoaded::destructor). Not a real
    // destructor: callers reload the pointer for the delete that follows,
    // and that delete has no null check of its own.
    void release();
};

// Loads and runs every ANM file.
struct AnmManager
{
    u8 unk_0[0xc0];
    // Cleared every frame by GameThread's on_draw.
    i32 unk_c0;
    i32 unk_c4;
    i32 unk_c8;
    i32 unk_cc;
    // Copied from the active camera by Supervisor::swap_transform_matrices.
    Float2 camera_unk_fc;
    u8 unk_d8[0x184f4f0 - 0xd8];
    // Indexed by the slot given to preload_anm.
    AnmLoaded *loaded_anms[0x20];

    void flush_sprites();
    void draw_vm(AnmVm *vm);
    // 0x46efa0
    AnmVm *get_vm_with_id(AnmId id);
    // 0x46f1c0. Marks the VM and its children for deletion. Reaches the
    // manager through g_AnmManager, so LTCG drops the unused this.
    HARNESS_CALLED void delete_vm(AnmId id);

    // Members that do not use this; LTCG dropped it (ret N, no ecx).
    static void __stdcall interrupt_tree(AnmId id, i32 interrupt);
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *path);
    // Frees ANM files marked for unloading; nonzero while one is still busy.
    static i32 sub_46d690();
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
