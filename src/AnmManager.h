#pragma once

#include "AnmVm.h"
#include "Thread.h"
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

// The texture of one entry of a loaded .anm file (ExpHP: zAnmLoadedD3D).
struct AnmLoadedD3D
{
    IDirect3DTexture9 *texture;
    void *src_data;
    u32 src_data_size;
    i32 bytes_per_pixel;
    void *entry;
    i32 flags;

    // 0x46f490. Fills the top level of the texture with zeroes.
    void clear_texture();
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
    // One per entry.
    AnmLoadedD3D *d3d;
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
    // 0x406380. A VM at the origin, on the given layer unless negative;
    // stores the VM in *out_vm when that is not NULL.
    AnmId create_effect(i32 script, i32 layer, AnmVm **out_vm);
    // 0x46ed60. A child of parent; mode bits 1 and 2 pick the list (see
    // AnmVm::mode_of_create_child).
    AnmId create_managed_child(i32 script, AnmVm *parent, i32 mode);
    // 0x46eea0. A root VM placed like the given one.
    AnmId create_managed_root(i32 script, AnmVm *like, i32 unused);

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

// A VM from the manager's preallocated pool (ExpHP: zAnmFastVm).
struct AnmFastVm
{
    AnmVm vm;
    ZunList<AnmFastVm> freelist_node;
    bool is_alive;
    u8 unk_60d[3];
    // Index in the pool; the low 13 bits of the VM's id.
    i32 fast_id;
};

// One vertex of a textured sprite (ExpHP: zRenderVertex144).
struct AnmSpriteVertex
{
    D3DXVECTOR4 pos;
    D3DCOLOR color;
    Float2 uv;
};

// One vertex of an untextured primitive (ExpHP: zRenderVertex044).
struct AnmPrimitiveVertex
{
    D3DXVECTOR4 pos;
    D3DCOLOR color;
};

// Batched vertices waiting for a draw call (ExpHP: zAnmVertexBuffers).
struct AnmVertexBuffers
{
    i32 unrendered_sprite_count;
    AnmSpriteVertex sprite_vertex_data[0x20000];
    AnmSpriteVertex *sprite_write_cursor;
    AnmSpriteVertex *sprite_render_cursor;
    i32 unrendered_primitive_count;
    AnmPrimitiveVertex primitive_vertex_data[0x8000];
    AnmPrimitiveVertex *primitive_write_cursor;
    AnmPrimitiveVertex *primitive_render_cursor;
};

// Loads and runs every ANM file.
struct AnmManager
{
    ThreadInf thread;
    u8 unk_1c[0xc0 - 0x1c];
    // Cleared every frame by GameThread's on_draw.
    i32 unk_c0;
    i32 unk_c4;
    i32 unk_c8;
    i32 unk_cc;
    // Copied from the active camera by Supervisor::swap_transform_matrices.
    Float2 camera_unk_fc;
    i32 useless_count;
    // VMs created for the game world and for the UI, in tick order.
    ZunList<AnmVm> *world_list_head;
    ZunList<AnmVm> *world_list_tail;
    ZunList<AnmVm> *ui_list_head;
    ZunList<AnmVm> *ui_list_tail;
    AnmFastVm fast_array[0x1fff];
    // Snapshots of VMs (ExpHP: __lolk_*), kept apart from the live ones.
    i32 next_snapshot_fast_id;
    i32 next_snapshot_discriminator;
    ZunList<AnmVm> snapshot_list_head;
    AnmFastVm snapshot_fast_array[0x1fff];
    // Unused entries of fast_array.
    ZunList<AnmFastVm> freelist_head;
    u8 unk_184f4ec[4];
    // Indexed by the slot given to preload_anm.
    AnmLoaded *loaded_anms[0x1f];
    D3DMATRIX matrix_184f56c;
    AnmVm vm_184f5ac;
    u8 unk_184fba8[0x184fc18 - 0x184fba8];
    AnmVertexBuffers vertex_buffers;
    AnmVm layer_list_dummy_heads[0x2b];
    // The upper 19 bits of the next VM id.
    volatile i32 last_discriminator;
    u8 unk_1c7fd88[0x1c7fd90 - 0x1c7fd88];

    void flush_sprites();
    void draw_vm(AnmVm *vm);
    // 0x46efa0
    DECOMP_NOINLINE AnmVm *get_vm_with_id(AnmId id);
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
    // 0x46f600. A VM from the pool, or a new one when the pool is used up.
    // Every caller goes through g_AnmManager (see the list inserts).
    HARNESS_CALLED AnmVm *allocate_vm();
    // 0x46f720. The same for snapshots; hands out the snapshot's id.
    AnmVm *allocate_snapshot_vm(AnmId *id);
    // 0x46f810. Copies the VM and its children into snapshots.
    AnmId store_snapshot_of_vm(AnmVm *vm, AnmVm *parent, i32 unused);
    // 0x46e7d0 and the next three. Every caller goes through g_AnmManager,
    // so LTCG replaced this with a load of the global (and kept its stack
    // slot). They hand out the VM's new id.
    HARNESS_CALLED AnmId insert_in_world_list_back(AnmVm *vm);
    HARNESS_CALLED AnmId insert_in_world_list_front(AnmVm *vm);
    HARNESS_CALLED AnmId insert_in_ui_list_back(AnmVm *vm);
    HARNESS_CALLED AnmId insert_in_ui_list_front(AnmVm *vm);
    static void __stdcall interrupt_tree_and_run(AnmId id, i32 interrupt);
    // Marks the VM and its whole tree for deletion (ExpHP:
    // AnmBehemoth::sub_46f220_recursive).
    HARNESS_CALLED_INLINABLE void mark_tree_for_deletion(AnmVm *vm);
    // get_vm_with_id for snapshots.
    AnmVm *get_snapshot_vm_with_id(AnmId id);
    // 0x46e750. Draws the VMs of one layer; returns 1 for the callbacks.
    i32 render_layer(i32 layer);
    // UpdateFunc callbacks that draw one layer each, named after their
    // priority.
    static int __fastcall on_draw_05_layer_00(AnmManager *mgr);
    static int __fastcall on_draw_0a_layer_03(AnmManager *mgr);
    static int __fastcall on_draw_2d_layer_20(AnmManager *mgr);
    static int __fastcall on_draw_3a_layer_24(AnmManager *mgr);
    static int __fastcall on_draw_40_layer_28(AnmManager *mgr);
    static int __fastcall on_draw_37_layer_36(AnmManager *mgr);
    static int __fastcall on_draw_41_layer_39(AnmManager *mgr);
    static int __fastcall on_draw_07_layer_01(AnmManager *mgr);
    static int __fastcall on_draw_09_layer_02(AnmManager *mgr);
    static int __fastcall on_draw_0b_layer_04(AnmManager *mgr);
    static int __fastcall on_draw_0d_layer_05(AnmManager *mgr);
    static int __fastcall on_draw_10_layer_06(AnmManager *mgr);
    static int __fastcall on_draw_12_layer_07(AnmManager *mgr);
    static int __fastcall on_draw_14_layer_08(AnmManager *mgr);
    static int __fastcall on_draw_15_layer_09(AnmManager *mgr);
    static int __fastcall on_draw_16_layer_10(AnmManager *mgr);
    static int __fastcall on_draw_18_layer_11(AnmManager *mgr);
    static int __fastcall on_draw_1c_layer_13(AnmManager *mgr);
    static int __fastcall on_draw_1f_layer_14(AnmManager *mgr);
    static int __fastcall on_draw_20_layer_15(AnmManager *mgr);
    static int __fastcall on_draw_22_layer_16(AnmManager *mgr);
    static int __fastcall on_draw_24_layer_17(AnmManager *mgr);
    static int __fastcall on_draw_27_layer_18(AnmManager *mgr);
    static int __fastcall on_draw_1b_layer_12(AnmManager *mgr);
    static int __fastcall on_draw_2a_layer_19(AnmManager *mgr);
    static int __fastcall on_draw_2e_layer_21(AnmManager *mgr);
    static int __fastcall on_draw_34_layer_22(AnmManager *mgr);
    static int __fastcall on_draw_36_layer_23(AnmManager *mgr);
    static int __fastcall on_draw_4f_layer_30(AnmManager *mgr);
    static int __fastcall on_draw_52_layer_31(AnmManager *mgr);
    static int __fastcall on_draw_4d_layer_29(AnmManager *mgr);
    static int __fastcall on_draw_3d_layer_26(AnmManager *mgr);
    static int __fastcall on_draw_3e_layer_27(AnmManager *mgr);
    static int __fastcall on_draw_3b_layer_25(AnmManager *mgr);
    static int __fastcall on_draw_3c_layer_37(AnmManager *mgr);
    static int __fastcall on_draw_3f_layer_38(AnmManager *mgr);
    static int __fastcall on_draw_4e_layer_40(AnmManager *mgr);
    static int __fastcall on_draw_50_layer_41(AnmManager *mgr);
    static int __fastcall on_draw_53_layer_42(AnmManager *mgr);

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
