#pragma once

#include "AnmVm.h"
#include "ZunMath.h"
#include "decomp.h"
#include "types.h"

// Only the parts decompiled code needs so far. Layouts from ExpHP's
// th-re-data.

// A sprite of a loaded .anm file (ExpHP: zAnmLoadedSprite).
struct AnmLoadedSprite
{
    i32 unk_0;
    i32 image_file_num_in_anm;
    i32 image_file_num_in_all;
    Float2 start_pixel_inclusive;
    Float2 end_pixel_exclusive;
    f32 bitmap_height;
    f32 bitmap_width;
    Float2 uv_start;
    Float2 uv_end;
    f32 sprite_height;
    f32 sprite_width;
    Float2 unk_3c;
};

// One loaded ANM file.
struct AnmLoaded
{
    i32 slot_num;
    char name[0x104];
    void *anm_file;
    // One prototype VM per script.
    AnmVm *vms;
    i32 entry_count;
    i32 script_count;
    i32 sprite_count;
    AnmLoadedSprite *sprites;
    u8 **scripts;
    void *d3d;
    i32 load_wait;
    u8 unk_12c[0x134 - 0x12c];
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
    AnmLoaded *loaded_anms[0x1f];
    u8 unk_184f56c[0x184fbb0 - 0x184f56c];
    // The D3D state the sprite code last set, compared before setting it
    // again. Code that draws without the sprite code resets these so the
    // next sprite sets everything.
    i32 render_cache_184fbb0;
    u8 render_cache_184fbb4;
    u8 render_cache_184fbb5;
    u8 render_cache_184fbb6;
    u8 render_cache_184fbb7;
    u8 render_cache_184fbb8;
    u8 unk_184fbb9;
    u8 render_cache_184fbba;
    u8 render_cache_184fbbb;
    u8 render_cache_184fbbc;
    u8 render_cache_184fbbd;
    u8 unk_184fbbe[2];
    i32 render_cache_184fbc0;

    void flush_sprites();
    void draw_vm(AnmVm *vm);
    // 0x46efa0
    AnmVm *get_vm_with_id(AnmId id);
    // 0x46f1c0. Marks the VM and its children for deletion. Reaches the
    // manager through g_AnmManager, so LTCG drops the unused this (ExpHP:
    // anm_unload_46f1c0).
    HARNESS_CALLED void delete_vm(AnmId id);
    // 0x46f270 (ExpHP: AnmBehemoth::disable_vms_from_anm_file).
    void disable_vms_from_anm_file(AnmLoaded *anm);

    // Members that do not use this; LTCG dropped it (ret N, no ecx).
    static void __stdcall interrupt_tree(AnmId id, i32 interrupt);
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *path);
    // Frees ANM files marked for unloading; nonzero while one is still busy.
    static i32 sub_46d690();
    // 0x46f600. Reaches the manager through g_AnmManager.
    static AnmVm *allocate_vm();
    // 0x46e7d0. Reaches the manager through g_AnmManager.
    static AnmId __stdcall insert_in_world_list_back(AnmVm *vm);

    // Frees the ANM file in a slot, if one is loaded there.
    void unload_anm(i32 slot)
    {
        if (slot < 0 || slot >= sizeof(loaded_anms) / sizeof(loaded_anms[0]))
        {
            return;
        }
        if (loaded_anms[slot] != NULL)
        {
            loaded_anms[slot]->release();
            delete loaded_anms[slot];
            loaded_anms[slot] = NULL;
        }
    }
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
