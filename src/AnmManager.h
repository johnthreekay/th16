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
    u8 unk_108[0x13c - 0x108];

    void set_sprite(AnmVm *vm, i32 sprite);

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
    AnmVm *get_vm_with_id(AnmId id);

    // Members that do not use this; LTCG dropped it (ret N, no ecx).
    static void __stdcall interrupt_tree(AnmId id, i32 interrupt);
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *path);
    // Frees ANM files marked for unloading; nonzero while one is still busy.
    static i32 sub_46d690();
};

extern AnmManager *g_AnmManager;
