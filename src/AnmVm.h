#pragma once

#include <d3dx9math.h>

#include "Interp.h"
#include "Timer.h"
#include "ZunMath.h"
#include "decomp.h"
#include "types.h"

struct AnmVm;

struct Int3
{
    i32 x;
    i32 y;
    i32 z;
};

// Handle of a VM in the ANM manager; 0 when unused.
struct AnmId
{
    i32 id;

    AnmId()
    {
        id = 0;
    }
};

// Intrusive list node; entry points back at the owner.
struct AnmVmList
{
    AnmVm *entry;
    AnmVmList *next;
    AnmVmList *prev;
    AnmVmList *unk_c;
};

// Value interpolated from initial to goal over end_time frames along an
// InterpMode curve. Layouts from ExpHP's th-re-data (zInterpFloat etc.).
template <typename T> struct Interp
{
    T initial;
    T goal;
    T bezier_1;
    T bezier_2;
    T current;
    Timer time;
    i32 end_time;
    i32 method;
};

typedef Interp<f32> InterpFloat;
typedef Interp<Float2> InterpFloat2;
typedef Interp<Float3> InterpFloat3;
typedef Interp<i32> InterpInt;
typedef Interp<Int3> InterpInt3;

enum AnmVmFlagsHi
{
    ANM_VM_FLAG_HI_4000 = 1 << 14,
    ANM_VM_FLAG_HI_8000 = 1 << 15,
    // get_slowdown_factor stops walking up the parents at a VM with this.
    ANM_VM_FLAG_HI_OWN_SLOWDOWN = 1 << 16,
    // Two bits: 1 for layers 3-19, 2 for layers 20-23, else 0.
    ANM_VM_FLAG_HI_LAYER_KIND_MASK = 3 << 18,
    ANM_VM_FLAG_HI_LAYER_KIND_1 = 1 << 18,
    ANM_VM_FLAG_HI_LAYER_KIND_2 = 2 << 18,
    // Three bits; set_layer sets it to 1 for layers 20-31 and 36-42.
    ANM_VM_FLAG_HI_COORD_MODE_MASK = 7 << 20,
    ANM_VM_FLAG_HI_COORD_MODE_1 = 1 << 20,
};

// An ANM script interpreter: one sprite (or a tree of them) with its
// transform, color and interpolators. Layout from ExpHP's th-re-data
// (zAnmVm = zAnmVmPrefix + zAnmVmSuffix).
struct AnmVm
{
    // --- prefix (0x0) ---
    Timer interrupt_return_time;
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
    i32 unk_494;
    i32 unk_498;
    i16 unk_49c;
    u8 unk_49e[2];
    i32 int_script_vars[4];
    f32 float_script_vars[4];
    Float3 script_vars_33_34_35;
    i32 script_var_8;
    i32 script_var_9;
    f32 rand_scale_one;
    f32 rand_scale_pi;
    i32 num_cycles_in_texture;
    Float3 pos_2;
    Float3 last_rendered_quad_in_surface_space[4];
    i32 mode_of_create_child;
    D3DCOLOR color_1;
    D3DCOLOR color_2;
    D3DCOLOR mixed_inherited_color;
    u8 font_dims[2];
    u8 unk_52e[2];
    u32 flags_lo;
    u32 flags_hi;
    // --- suffix (0x538) ---
    AnmId id;
    u32 fast_id;
    Timer script_time;
    Timer timer_1c;
    AnmVmList node_in_global_list;
    AnmVmList node_as_child;
    AnmVmList list_of_children;
    AnmVmList list_60;
    AnmVm *next_in_layer;
    AnmVm *root_vm;
    AnmVm *parent_vm;
    f32 slowdown;
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
    Float3 entity_pos;
    void *associated_game_entity;
    Float3 rotation_related;

    AnmVm();
    ~AnmVm();
    void initialize();
    HARNESS_CALLED f32 get_slowdown_factor();
    void alloc_extra_data(u32 size);
    void set_layer(i32 layer);
    void set_alpha1_time(i32 end_time, i32 method, u8 initial, u8 goal);
    // Clears the suffix except for the fields that identify the VM.
    void wipe_suffix();
};

// One loaded .anm file. Layout from ExpHP's th-re-data (zAnmLoaded).
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
    void *sprites;
    u8 **scripts;
    void *d3d;
    i32 load_wait;
    u8 unk_12c[8];
    i32 unk_134;
    u8 unk_138[4];

    // noexcept(false) stands in for something in the real one that makes
    // ~AsciiInf keep a noexcept guard frame (FuncInfo EHFlags 5, no states).
    // Revisit once 0x46d770 is decompiled.
    ~AnmLoaded() noexcept(false);
    void copy_vm(AnmVm *dst, i32 script);
    void set_sprite(AnmVm *vm, i32 sprite);

    // Resets vm and shows one of this file's sprites with it.
    void setup_vm(AnmVm *vm, i32 sprite)
    {
        vm->initialize();
        vm->anm_loaded_index = slot_num;
        set_sprite(vm, sprite);
    }
};
