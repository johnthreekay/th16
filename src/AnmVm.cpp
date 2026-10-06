#include <stdlib.h>
#include <string.h>

#include "AnmVm.h"

// FUNCTION: TH16 0x4093f0
AnmVm::AnmVm()
{
    memset(this, 0, sizeof(AnmVm));
    sprite_id = -1;
    instr_offset = -1;
}

// FUNCTION: TH16 0x4093b0
AnmVm::~AnmVm()
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

// TODO: the flags_hi and/or and one pop are scheduled one store later in the original.
// FUNCTION: TH16 0x4090f0
void AnmVm::initialize()
{
    Float3 saved_entity_pos = entity_pos;
    u32 saved_layer = layer;
    u32 saved_fast_id = fast_id;
    memset(this, 0, sizeof(AnmVm));
    fast_id = saved_fast_id;
    layer = saved_layer;
    entity_pos = saved_entity_pos;

    scale.x = 1.0f;
    scale.y = 1.0f;
    scale_2.x = 1.0f;
    scale_2.y = 1.0f;
    uv_scale.x = 1.0f;
    uv_scale.y = 1.0f;
    color_1 = 0xffffffff;
    D3DXMatrixIdentity(&matrix_3d0);
    // A 16-bit store in the original.
    *(u16 *)&flags_lo = 7;
    script_time.reset();
    timer_1c.reset();
    pos_i.end_time = 0;
    rgb1_i.end_time = 0;
    alpha1_i.end_time = 0;
    rotate_i.end_time = 0;
    scale_i.end_time = 0;
    op_434_i.end_time = 0;
    uv_scale_i.end_time = 0;
    rgb2_i.end_time = 0;
    alpha2_i.end_time = 0;
    u_vel_i.end_time = 0;
    v_vel_i.end_time = 0;
    flags_hi &= ~ANM_VM_FLAG_HI_8000;
    flags_hi |= ANM_VM_FLAG_HI_4000;
    rand_scale_one = 1.0f;
    rand_scale_pi = ZUN_PI;
    num_cycles_in_texture = 0x10000;
    root_vm = NULL;
    parent_vm = NULL;
    node_in_global_list.entry = this;
    node_in_global_list.next = NULL;
    node_in_global_list.prev = NULL;
    node_in_global_list.unk_c = NULL;
    node_as_child.entry = this;
    node_as_child.next = NULL;
    node_as_child.prev = NULL;
    node_as_child.unk_c = NULL;
    list_of_children.entry = this;
    list_of_children.next = NULL;
    list_of_children.prev = NULL;
    list_of_children.unk_c = NULL;
    list_60.entry = this;
    list_60.next = NULL;
    list_60.prev = NULL;
    list_60.unk_c = NULL;
}

// FUNCTION: TH16 0x406340
HARNESS_CALLED f32 AnmVm::get_slowdown_factor()
{
    if (root_vm != NULL && !(flags_hi & ANM_VM_FLAG_HI_OWN_SLOWDOWN))
    {
        return root_vm->get_slowdown_factor();
    }
    return slowdown;
}

// FUNCTION: TH16 0x4064e0
void AnmVm::alloc_extra_data(u32 size)
{
    ins_508_extra_data_size = size;
    ins_508_extra_data = malloc(size);
}
