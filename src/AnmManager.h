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
    // 0x406380. Creates a VM running the script at the origin; also stores
    // the VM in *out if out is not NULL.
    AnmId create_effect(i32 script, i32 layer, AnmVm **out);
    // 0x42c920. Like create_effect, for the UI list.
    AnmId create_ui_effect(i32 script, i32 unused, AnmVm **out);
    // 0x426160. Like create_vm at the origin, but inserted at the front of
    // the world list.
    AnmId create_vm_front(i32 script, i32 layer, i32 unused);

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
    // 0x46efa0
    AnmVm *get_vm_with_id(AnmId id);
    // 0x46f1c0. Marks the VM and its children for deletion. Reaches the
    // manager through g_AnmManager, so LTCG drops the unused this (ExpHP:
    // anm_unload_46f1c0).
    HARNESS_CALLED void delete_vm(AnmId id);
    // 0x46f220. Marks the VM and its descendants for deletion (ExpHP:
    // AnmBehemoth::sub_46f220_recursive).
    void mark_tree_for_delete(AnmVm *vm);

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

    // 0x46f270 (ExpHP: AnmBehemoth::disable_vms_from_anm_file).
    void disable_vms_from_anm_file(AnmLoaded *anm);

    // Members that do not use this; LTCG dropped it (ret N, no ecx).
    static void __stdcall interrupt_tree(AnmId id, i32 interrupt);
    // 0x46f130. Like interrupt_tree, also running each VM once.
    static void __stdcall interrupt_tree_and_run(AnmId id, i32 interrupt);
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *path);
    // Frees ANM files marked for unloading; nonzero while one is still busy.
    static i32 sub_46d690();
    // 0x46f600. Reaches the manager through g_AnmManager.
    static AnmVm *allocate_vm();
    // 0x46e7d0. Reaches the manager through g_AnmManager.
    static AnmId __stdcall insert_in_world_list_back(AnmVm *vm);
    // 0x46e940. Reaches the manager through g_AnmManager.
    static AnmId __stdcall insert_in_ui_list_back(AnmVm *vm);
    // 0x46e890. Reaches the manager through g_AnmManager.
    static AnmId __stdcall insert_in_world_list_front(AnmVm *vm);

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
