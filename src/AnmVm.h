#pragma once

#include <stdlib.h>

#include <d3d9.h>
#include <d3dx9math.h>

#include "Interp.h"
#include "ZunList.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

// Handle to a running VM, 0 when none. A class in ZUN's code: it is returned
// through a hidden pointer and constructed to 0 before its owner's memset.
struct AnmId
{
    i32 id;

    AnmId()
    {
        id = 0;
    }
};

// A D3DCOLOR whose channels can be reached one by one.
union ZunColor
{
    D3DCOLOR d3d;
    struct
    {
        u8 b;
        u8 g;
        u8 r;
        u8 a;
    };
};

enum AnmVmFlagsLo
{
    ANM_VM_VISIBLE = 1 << 0,
    // Rotation or scale changed; the matrix needs a rebuild.
    ANM_VM_ROTATION_CHANGED = 1 << 2,
    ANM_VM_SCALE_CHANGED = 1 << 3,
};

enum AnmVmFlagsHi
{
    // Rotate the sprite to the owner's movement angle.
    ANM_VM_AUTO_ROTATE = 1 << 7,
    ANM_VM_CREATED_BY_GAME = 1 << 10,
    ANM_VM_FLAG_HI_4000 = 1 << 14,
    ANM_VM_FLAG_HI_8000 = 1 << 15,
    // world_pos and get_slowdown_factor stop walking up the parents at a VM
    // with this.
    ANM_VM_NO_PARENT_POS = 1 << 16,
    // Two bits of layer kind: LAYER_SET for layers 3-19, LAYER_UI for
    // layers 20-23, neither for the rest.
    ANM_VM_LAYER_SET = 1 << 18,
    ANM_VM_LAYER_UI = 1 << 19,
    ANM_VM_LAYER_KIND_MASK = ANM_VM_LAYER_SET | ANM_VM_LAYER_UI,
    // Three bits; set_layer sets it to 1 for layers 20-31 and 36-42.
    ANM_VM_COORD_MODE_MASK = 7 << 20,
    ANM_VM_COORD_MODE_1 = 1 << 20,
    ANM_VM_ROTATE_WITH_PARENT = 1 << 23,
};

struct AnmVm;

// Script callbacks, selected per VM by the index_of_* fields.
typedef i32(__fastcall *AnmVmSwitchFunc)(AnmVm *vm, i32 interrupt);
extern AnmVmSwitchFunc g_anm_on_switch_funcs[4];

// One running ANM script (layout: ExpHP's zAnmVm, flattened, 0x5fc bytes).
struct AnmVm
{
    ZunTimer interrupt_return_time;
    i32 interrupt_return_offset;
    i32 layer;
    i32 anm_loaded_index;
    i32 sprite_id;
    i32 script_id;
    i32 instr_offset;
    Float3 pos;
    Float3 rotation;
    Float3 angular_velocity;
    Float2 scale;
    Float2 scale_2;
    Float2 scale_growth;
    Float2 uv_scale;
    Float2 sprite_size;
    Float2 uv_scroll_pos;
    Float2 anchor_offset;
    u8 unk_88[4];
    InterpFloat3 pos_i;
    InterpInt3 rgb1_i;
    InterpInt alpha1_i;
    InterpFloat3 rotate_i;
    InterpFloat rotate_2d_i;
    InterpFloat2 scale_i;
    InterpFloat2 op_434_i;
    InterpFloat2 uv_scale_i;
    InterpInt3 rgb2_i;
    InterpInt alpha2_i;
    InterpFloat u_vel_i;
    InterpFloat v_vel_i;
    Float2 uv_quad_of_sprite[4];
    Float2 uv_scroll_vel;
    D3DXMATRIX matrix_3d0;
    D3DXMATRIX matrix_410;
    D3DXMATRIX matrix_450;
    i32 pending_interrupt;
    i32 time_of_last_sprite_set;
    i32 unk_498;
    i16 unk_49c;
    u8 unk_49e[2];
    // ANM script variables (ExpHP: int_script_vars, float_script_vars).
    i32 int_vars[4];
    f32 float_vars[4];
    Float3 script_vars_33_34_35;
    i32 script_var_8;
    i32 script_var_9;
    f32 rand_scale_one;
    f32 rand_scale_pi;
    i32 num_cycles_in_texture;
    Float3 pos_2;
    Float3 last_rendered_quad_in_surface_space[4];
    i32 mode_of_create_child;
    ZunColor color_1;
    ZunColor color_2;
    ZunColor mixed_inherited_color;
    u8 font_dims[2];
    u8 unk_52e[2];
    u32 flags_lo;
    u32 flags_hi;
    // Suffix (ExpHP: zAnmVmSuffix).
    AnmId id;
    u32 fast_id;
    ZunTimer script_time;
    ZunTimer timer_1c;
    ZunList<AnmVm> node_in_global_list;
    ZunList<AnmVm> node_as_child;
    ZunList<AnmVm> list_of_children;
    ZunList<AnmVm> unk_list_598;
    AnmVm *next_in_layer;
    // ExpHP: __root_vm__or_maybe_not.
    AnmVm *parent;
    // ExpHP: parent_vm.
    AnmVm *unk_5b0;
    f32 slowdown;
    // Allocated by ANM instruction 508.
    void *ins_508_extra_data;
    u32 ins_508_extra_data_size;
    i32 index_of_on_wait;
    i32 index_of_on_tick;
    i32 index_of_on_draw;
    i32 index_of_on_destroy;
    i32 index_of_on_interrupt;
    i32 index_of_on_copy_1;
    i32 index_of_on_copy_2;
    i32 index_of_sprite_mapping_func;
    // Position of the game object the VM belongs to.
    Float3 entity_pos;
    void *associated_game_entity;
    Float3 rotation_related;

    // Callers compile with EH cleanup for this, which LTCG then removes
    // because it sees the body cannot throw.
    DECOMP_NOINLINE AnmVm();
    // Inlined into every owner's destructor; the original also has an
    // out-of-line copy at 0x4093b0.
    ~AnmVm();
    // Resets the VM, keeping layer, fast_id and entity_pos (ExpHP:
    // AnmVm::initialize).
    void wipe();
    // 0x40e490. Position including entity_pos and every parent's.
    Float3 world_pos();
    // 0x45f980
    void run();
    HARNESS_CALLED f32 get_slowdown_factor();
    void alloc_extra_data(u32 size);
    void set_layer(i32 layer);
    void set_alpha1_time(i32 end_time, i32 method, u8 initial, u8 goal);
    // Clears the suffix except for the fields that identify the VM.
    void wipe_suffix();
    // Switches to another sprite of the same file, changing only the UVs.
    void set_sprite_uvs(i32 sprite);

    void interrupt(i32 n)
    {
        if (index_of_on_interrupt != 0)
        {
            g_anm_on_switch_funcs[index_of_on_interrupt](this, n);
        }
        pending_interrupt = n;
    }
};
