#pragma once

#include <string.h>

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

// The header of one entry of an .anm file (ExpHP: zAnmRawEntry); the
// pointers are offsets from the header.
struct AnmRawEntry
{
    u32 version;
    u16 num_sprites;
    u16 num_scripts;
    u16 unk_8;
    u16 width;
    u16 height;
    u16 format;
    u32 image_path;
    u16 offset_x;
    u16 offset_y;
    u32 memory_priority;
    u32 texture;
    u8 has_data;
    u8 unk_21;
    u8 low_res_scale;
    u8 unk_23;
    u32 offset_to_next;
    u32 unused[6];
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
    // Nonzero while the textures are still being created: the index (plus
    // one) of the next entry to set up.
    i32 load_wait;
    // Set to have sub_46d690 unload the file.
    i32 unload_requested;
    // Bytes of texture memory the file's textures take.
    i32 texture_memory;
    // Counts VMs created from this file.
    i32 vm_count;
    void *unk_138;

    AnmLoaded()
    {
        memset(this, 0, sizeof(AnmLoaded));
    }

    // Points the VM at a sprite: UVs, size and texture matrices. -1 if the
    // file is not loaded.
    i32 set_sprite(AnmVm *vm, i32 sprite);
    // Resets the VM and points it at a script without running it; -1 (with
    // the VM zeroed) if the script does not exist.
    i32 init_script_vm(AnmVm *vm, i32 script);
    // Starts a script on the VM and runs its first frame; zeroes the VM if
    // the script does not exist or the file is still loading.
    void set_vm_script(AnmVm *vm, i32 script);
    // 0x407b20
    void copy_vm(AnmVm *vm, i32 script);
    // 0x40d460
    void copy_vm_and_run(AnmVm *vm, i32 script);
    // 0x40e5c0. Creates a VM running the script at pos (entity_pos), with
    // the given z rotation, on the given layer unless negative.
    HARNESS_CALLED AnmId create_vm(i32 script, Float3 *pos, f32 rotation, i32 layer, i32 unused);
    // 0x406380. Creates a VM running the script at the origin, on the given
    // layer unless negative; also stores the VM in *out if out is not NULL.
    AnmId create_effect(i32 script, i32 layer, AnmVm **out);
    // 0x42c920. Like create_effect, for the UI list.
    AnmId create_ui_effect(i32 script, i32 unused, AnmVm **out);
    // 0x426160. Like create_vm at the origin, but inserted at the front of
    // the world list.
    AnmId create_vm_front(i32 script, i32 layer, i32 unused);
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
    // 0x46cdd0 (ExpHP: AnmManager::do_load_anm). Reads the file and sizes
    // the tables; 0 on success.
    i32 load(const char *path);
    // 0x46d0c0 (ExpHP: load_one_script). Checks an entry and reads its
    // image file unless the texture is embedded.
    i32 load_entry(i32 index, AnmRawEntry *entry);
    // 0x46d8a0. Stores a sprite and works out its UVs and size.
    void load_sprite(i32 index, AnmLoadedSprite *sprite);
};

// A sprite as stored in an .anm entry.
struct AnmRawSprite
{
    i32 id;
    f32 x;
    f32 y;
    f32 width;
    f32 height;
};

// An embedded texture (THTX), the image of entries with has_data set.
struct AnmRawTexture
{
    char magic[4];
    u16 unk_4;
    i16 format;
    i16 width;
    i16 height;
    u32 size;
    u8 data[1];
};

// Bytes per pixel and Direct3D format of each ANM texture format.
extern i32 g_anm_format_bpp[9];
extern D3DFORMAT g_anm_d3d_formats[9];

// A VM from the manager's preallocated pool (ExpHP: zAnmFastVm).
struct AnmFastVm
{
    AnmVm vm;
    ZunList<AnmFastVm> freelist_node;
    bool is_alive;
    u8 unk_60d[3];
    // Index in the pool; the low 13 bits of the VM's id.
    i32 fast_id;

    // 0x46b770 and 0x46b790, which AnmManager's constructor and destructor
    // pass to the vector constructor and destructor iterators.
    AnmFastVm();
    ~AnmFastVm();
};

// Vertex formats of the batched sprites and primitives (ExpHP:
// zRenderVertex144, zRenderVertex044).
struct RenderVertex144
{
    D3DXVECTOR4 pos;
    D3DCOLOR diffuse;
    Float2 uv;
};

struct RenderVertex044
{
    D3DXVECTOR4 pos;
    D3DCOLOR diffuse;
};

// A request to copy part of the back buffer into the texture of a loaded
// .anm entry (the pause menu's snapshot of the game screen). anm_slot < 0
// marks a free entry.
struct AnmScreenCopy
{
    i32 anm_slot;
    i32 entry;
    i32 src_x;
    i32 src_y;
    i32 src_width;
    i32 src_height;
    i32 dst_x;
    i32 dst_y;
    i32 dst_width;
    i32 dst_height;
};

// Loads and runs every ANM file.
struct AnmManager
{
    ThreadInf thread;
    u8 unk_1c[0x20 - 0x1c];
    AnmScreenCopy screen_copies[4];
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
    u8 unk_184fba8[0x184fbb0 - 0x184fba8];
    // The D3D state the sprite code last set, compared before setting it
    // again (so that it only changes, and flushes the batch, when needed).
    // Code that draws without the sprite code resets these so the next
    // sprite sets everything.
    i32 render_cache_184fbb0;
    u8 last_blend_mode;
    u8 render_cache_184fbb5;
    u8 render_cache_184fbb6;
    u8 render_cache_184fbb7;
    u8 render_cache_184fbb8;
    u8 unk_184fbb9;
    u8 last_filter_point;
    u8 last_color_op;
    u8 last_address_u;
    u8 last_address_v;
    u8 unk_184fbbe[2];
    i32 render_cache_184fbc0;
    u8 unk_184fbc4[0x184fc18 - 0x184fbc4];
    // Sprites waiting for flush_sprites, six vertices each (ExpHP:
    // zAnmVertexBuffers).
    i32 unrendered_sprite_count;
    RenderVertex144 sprite_vertex_data[0x20000];
    RenderVertex144 *sprite_write_cursor;
    RenderVertex144 *sprite_render_cursor;
    i32 unrendered_primitive_count;
    RenderVertex044 primitive_vertex_data[0x8000];
    RenderVertex044 *primitive_write_cursor;
    RenderVertex044 *primitive_render_cursor;
    AnmVm layer_list_dummy_heads[0x2b];
    // The upper 19 bits of the next VM id.
    volatile i32 last_discriminator;
    u8 unk_1c7fd88[0x1c7fd90 - 0x1c7fd88];

    // 0x46b7d0. Destroys every VM still alive.
    ~AnmManager();

    // Never inlined in the original (over 100 call sites).
    DECOMP_NOINLINE void flush_sprites();
    void draw_vm(AnmVm *vm);
    // Sets blending, filtering and texture addressing for a VM, flushing
    // the batch first when they change.
    void setup_render_state_for_vm(AnmVm *vm);
    // Adds a quad (as two triangles) to the sprite batch; 1 if it is full.
    i32 write_sprite(RenderVertex144 *vertices);
    // Empties both vertex batches.
    HARNESS_CALLED void reset_vertex_buffers();
    // 0x466f00. Fills g_sprite_temp_buffer for a VM drawn in 2D.
    void render_sub_466f00(AnmVm *vm);
    // 0x465280. Draws the quad in g_sprite_temp_buffer for a VM.
    i32 render_sprite_2d(AnmVm *vm, i32 unk);
    // Render mode 5.
    void draw_vm__mode_5(AnmVm *vm);
    // 0x468350. Draws vertex_count vertices (a triangle fan, in screen
    // space) with the VM's texture and blending.
    i32 draw_vm__mode_11(AnmVm *vm, RenderVertex144 *vertices, i32 vertex_count);
    // 0x46efa0
    AnmVm *get_vm_with_id(AnmId id);
    // 0x46f1c0. Marks the VM and its children for deletion. Reaches the
    // manager through g_AnmManager, so LTCG drops the unused this (ExpHP:
    // anm_unload_46f1c0).
    HARNESS_CALLED void delete_vm(AnmId id);
    // 0x46f220. Marks the VM and its descendants for deletion (ExpHP:
    // AnmBehemoth::sub_46f220_recursive). Callers keep values in registers
    // across it, so LTCG has to see the body; never inlined.
    HARNESS_CALLED void mark_tree_for_delete(AnmVm *vm);

    // delete_vm's body. LTCG inlined delete_vm into some callers, with the
    // manager pointer loaded once for several of them.
    void delete_vm_inline(AnmId id)
    {
        AnmVm *vm = get_vm_with_id(id);
        if (vm != NULL && !(vm->flags_hi & ANM_VM_FLAG_HI_4000000))
        {
            vm->flags_hi = vm->flags_hi & ~ANM_VM_FLAG_HI_40 | ANM_VM_DELETE_PENDING;
            for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
            {
                mark_tree_for_delete(node->entry);
            }
        }
    }

    // 0x440c60. Queues a copy of the back buffer rectangle into an entry's
    // texture; dropped when all four entries are in use.
    DECOMP_NOINLINE i32 queue_screen_copy(i32 anm_slot, i32 entry, i32 src_x, i32 src_y, i32 src_width, i32 src_height,
                          i32 dst_x, i32 dst_y, i32 dst_width, i32 dst_height);
    // 0x440cd0. The same into the sprite a VM shows. Every caller goes
    // through g_AnmManager.
    HARNESS_CALLED i32 copy_screen_to_sprite(AnmId id, i32 src_x, i32 src_y, i32 src_width, i32 src_height);

    // 0x46f270 (ExpHP: AnmBehemoth::disable_vms_from_anm_file).
    void disable_vms_from_anm_file(AnmLoaded *anm);

    // Members that do not use this; LTCG dropped it (ret N, no ecx).
    DECOMP_NOINLINE static void __stdcall interrupt_tree(AnmId id, i32 interrupt);
    // 0x46f130. Like interrupt_tree, also running each VM once.
    DECOMP_NOINLINE static void __stdcall interrupt_tree_and_run(AnmId id, i32 interrupt);
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *path);
    // 0x46cf80. Loads a file into a slot without waiting for its textures.
    AnmLoaded *do_preload_anm(i32 slot, const char *path);
    // 0x46d1c0. Creates the textures of the next entry, or the prototype
    // VMs once all are done.
    static AnmLoaded *__stdcall load_next_entry(AnmLoaded *anm);
    // 0x46d3b0. Creates the texture, sprites and script table of one entry.
    static i32 __stdcall setup_entry(AnmLoaded *anm, i32 index, i32 first_sprite, i32 first_script,
                                     AnmRawEntry *entry);
    // Texture creation for setup_entry. They return the bytes the texture
    // takes (0 for render targets), or a negative value on failure.
    // 0x46cd80. A render target ("@R" entries).
    static i32 __stdcall create_render_target(AnmLoadedD3D *d3d, i32 width, i32 height);
    // 0x46cd30. An empty texture ("@" entries).
    static i32 __stdcall create_empty_texture(AnmLoadedD3D *d3d, i32 width, i32 height, i32 format);
    // 0x46c920. From the image file read by AnmLoaded::load_entry, cropped
    // to the entry's size. The third argument is the same at every call
    // site; LTCG folded it.
    static i32 __stdcall load_texture_from_file(AnmLoadedD3D *d3d, i32 format, i32 unused, i32 width, i32 height,
                                                i32 offset_x, i32 offset_y);
    // 0x46cb60. From a texture embedded in the .anm file.
    static i32 __stdcall load_texture_from_data(AnmLoadedD3D *d3d, AnmRawTexture *raw, i32 format, i32 width,
                                                i32 height);
    // 0x46c0d0. Fixes up the pixels of a freshly loaded texture.
    static void __stdcall convert_texture(IDirect3DTexture9 *texture);
    // Frees ANM files marked for unloading; nonzero while one is still busy.
    // Every caller goes through g_AnmManager (see the list inserts).
    HARNESS_CALLED i32 sub_46d690();
    // 0x46f600. A VM from the pool, or a new one when the pool is used up.
    // Every caller goes through g_AnmManager (see the list inserts).
    HARNESS_CALLED AnmVm *allocate_vm();
    // 0x46f720. The same for snapshots; hands out the snapshot's id.
    AnmVm *allocate_snapshot_vm(i32 *id);
    // 0x46f810. Copies the VM and its children into snapshots.
    AnmId store_snapshot_of_vm(AnmVm *vm, AnmVm *parent, i32 unused);
    // 0x46f8f0. Brings a stored snapshot back to life as a new VM tree.
    // Every caller goes through g_AnmManager (see the list inserts).
    HARNESS_CALLED AnmId restore_snapshot(AnmId id);
    // 0x46f970. Copies a snapshot and its children back into live VMs.
    AnmId restore_snapshot_vm(AnmVm *snapshot, AnmVm *parent);
    // 0x46fac0. Writes a VM, its extra data and its children to dst, adding
    // the bytes used to *size.
    HARNESS_CALLED void save_vm_tree(AnmVm *dst, AnmVm *src, i32 *size);
    // 0x46fc30. Reads a tree written by save_vm_tree back into snapshot VMs.
    AnmId load_vm_tree(AnmVm *src, AnmVm *parent, i32 *size);
    // 0x46e7d0 and the next three. Every caller goes through g_AnmManager,
    // so LTCG replaced this with a load of the global (and kept its stack
    // slot). They hand out the VM's new id.
    HARNESS_CALLED AnmId insert_in_world_list_back(AnmVm *vm);
    HARNESS_CALLED AnmId insert_in_world_list_front(AnmVm *vm);
    HARNESS_CALLED AnmId insert_in_ui_list_back(AnmVm *vm);
    HARNESS_CALLED AnmId insert_in_ui_list_front(AnmVm *vm);
    // get_vm_with_id for snapshots.
    HARNESS_CALLED AnmVm *get_snapshot_vm_with_id(AnmId id);
    // UpdateFunc callbacks that run the VMs of each list and rebuild the
    // per-layer draw lists.
    DECOMP_NOINLINE static i32 __fastcall tick_world(AnmManager *mgr);
    DECOMP_NOINLINE static i32 __fastcall tick_ui(AnmManager *mgr);
    static i32 __fastcall on_tick_21(AnmManager *mgr);
    static i32 __fastcall on_tick_09(AnmManager *mgr);
    // Moves the VM and its children onto delete_list, once each.
    void remove_tree(AnmVm *vm, ZunList<AnmVm> *delete_list);
    // 0x46eab0. Unlinks a VM and returns it to the pool or frees it.
    i32 destroy_possibly_managed_vm(AnmVm *vm);
    // 0x46ec90. The same for snapshots.
    i32 destroy_possibly_managed_snapshot_vm(AnmVm *vm);
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

    // 0x46d720. unload_anm as LTCG kept it out of line for one caller (an
    // ECL instruction).
    void unload_anm_out_of_line(i32 slot);
    // 0x46c8b0. Loads an image file in memory into the top level of an
    // existing texture. The last three arguments are the same at every call
    // site; LTCG folded them. Does not use this.
    HARNESS_CALLED i32 reload_texture(AnmLoadedD3D *d3d, void *data, u32 size, i32 unk_3, i32 unk_4, i32 unk_5);

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

// The quad being built by the draw functions.
extern RenderVertex144 g_sprite_temp_buffer[4];

// Deletes the VM (if still alive) and forgets the id.
inline void delete_vm_and_clear(AnmId &id)
{
    g_AnmManager->delete_vm(id);
    id.id = 0;
}

// The same with delete_vm inlined, as LTCG does in some loops.
inline void delete_vm_inline_and_clear(AnmId &id)
{
    g_AnmManager->delete_vm_inline(id);
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

// The first descendant of the VM running the script, or NULL if the VM is
// gone (forgetting the id then).
inline AnmVm *find_child_of(AnmId &id, i32 script)
{
    if (get_vm_or_clear(id) == NULL)
    {
        return NULL;
    }
    return get_vm_or_clear(id)->search_children(script, 0);
}
