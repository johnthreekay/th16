#pragma once

#include <stdlib.h>

#include <d3dx9math.h>

#include "Timer.h"
#include "ZunList.h"
#include "decomp.h"
#include "types.h"

// Handle of an AnmVm owned by the AnmManager; 0 means none. ExpHP: zAnmId.
struct AnmId
{
    i32 id;

    AnmId()
    {
        id = 0;
    }
};

// Interpolated values: initial and goal plus two bezier control values,
// stepped by a timer. Layouts from ExpHP (zInterpFloat etc.).
struct InterpFloat
{
    f32 initial;
    f32 goal;
    f32 bezier_1;
    f32 bezier_2;
    f32 current;
    Timer time;
    i32 end_time;
    i32 method;

    DECOMP_NOINLINE void reset();
    HARNESS_CALLED f32 step();
};

struct InterpFloat2
{
    D3DXVECTOR2 initial;
    D3DXVECTOR2 goal;
    D3DXVECTOR2 bezier_1;
    D3DXVECTOR2 bezier_2;
    D3DXVECTOR2 current;
    Timer time;
    i32 end_time;
    i32 method;
};

struct InterpFloat3
{
    D3DXVECTOR3 initial;
    D3DXVECTOR3 goal;
    D3DXVECTOR3 bezier_1;
    D3DXVECTOR3 bezier_2;
    D3DXVECTOR3 current;
    Timer time;
    i32 end_time;
    i32 method;
};

struct InterpInt
{
    i32 initial;
    i32 goal;
    i32 bezier_1;
    i32 bezier_2;
    i32 current;
    Timer time;
    i32 end_time;
    i32 method;
};

struct InterpInt3
{
    i32 initial[3];
    i32 goal[3];
    i32 bezier_1[3];
    i32 bezier_2[3];
    i32 current[3];
    Timer time;
    i32 end_time;
    i32 method;
};

// The shared interpolation curves (linear, ease in/out, ...): the fraction
// of the way from initial to goal at time t of end_time.
f32 LTCG_VECTORCALL interp_common_methods(i32 method, f32 t, f32 end_time);

struct AnmVm;

// Script callbacks, selected per VM by the index_of_* fields.
typedef i32(__fastcall *AnmVmSwitchFunc)(AnmVm *vm, i32 interrupt);
extern AnmVmSwitchFunc g_anm_on_switch_funcs[4];

// One running ANM script instance. Layout from ExpHP (zAnmVm), flattened.
struct AnmVm
{
    Timer interrupt_return_time;
    i32 interrupt_return_offset;
    u32 layer;
    i32 anm_loaded_index;
    i32 sprite_id;
    i32 script_id;
    i32 instr_offset;
    D3DXVECTOR3 pos;
    D3DXVECTOR3 rotation;
    D3DXVECTOR3 angular_velocity;
    D3DXVECTOR2 scale;
    D3DXVECTOR2 scale_2;
    D3DXVECTOR2 scale_growth;
    D3DXVECTOR2 uv_scale;
    D3DXVECTOR2 sprite_size;
    D3DXVECTOR2 uv_scroll_pos;
    D3DXVECTOR2 anchor_offset;
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
    D3DXVECTOR2 uv_quad_of_sprite[4];
    D3DXVECTOR2 uv_scroll_vel;
    D3DMATRIX matrix_3d0;
    D3DMATRIX matrix_410;
    D3DMATRIX matrix_450;
    i32 pending_interrupt;
    i32 time_of_last_sprite_set;
    i32 unk_498;
    i16 unk_49c;
    u8 unk_49e[2];
    i32 int_script_vars[4];
    f32 float_script_vars[4];
    D3DXVECTOR3 script_vars_33_34_35;
    i32 script_var_8;
    i32 script_var_9;
    f32 rand_scale_one;
    f32 rand_scale_pi;
    i32 num_cycles_in_texture;
    D3DXVECTOR3 pos_2;
    D3DXVECTOR3 last_rendered_quad_in_surface_space[4];
    i32 mode_of_create_child;
    D3DCOLOR color_1;
    D3DCOLOR color_2;
    D3DCOLOR mixed_inherited_color;
    u8 font_dims[2];
    u8 unk_52e[2];
    u32 flags_lo;
    u32 flags_hi;
    // Suffix (ExpHP: zAnmVmSuffix).
    AnmId id;
    u32 fast_id;
    Timer script_time;
    Timer timer_1c;
    ZunList<AnmVm> node_in_global_list;
    ZunList<AnmVm> node_as_child;
    ZunList<AnmVm> list_of_children;
    ZunList<AnmVm> unk_list_598;
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
    D3DXVECTOR3 entity_pos;
    void *associated_game_entity;
    D3DXVECTOR3 rotation_related;

    AnmVm() LTCG_NOTHROW;
    ~AnmVm()
    {
        if (ins_508_extra_data != NULL)
        {
            free(ins_508_extra_data);
        }
        ins_508_extra_data = NULL;
        ins_508_extra_data_size = 0;
        id.id = 0;
        instr_offset = -1;
    }

    void interrupt(i32 n)
    {
        if (index_of_on_interrupt != 0)
        {
            g_anm_on_switch_funcs[index_of_on_interrupt](this, n);
        }
        pending_interrupt = n;
    }

    i32 run();
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
};
