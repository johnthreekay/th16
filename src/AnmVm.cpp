#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
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
void AnmVm::wipe()
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
    color_1.d3d = 0xffffffff;
    D3DXMatrixIdentity(&matrix_3d0);
    // A 16-bit store in the original.
    *(u16 *)&flags_lo = 7;
    script_time.clear();
    timer_1c.clear();
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
    parent = NULL;
    unk_5b0 = NULL;
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
    unk_list_598.entry = this;
    unk_list_598.next = NULL;
    unk_list_598.prev = NULL;
    unk_list_598.unk_c = NULL;
}

// FUNCTION: TH16 0x406340
HARNESS_CALLED f32 AnmVm::get_slowdown_factor()
{
    if (parent != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
    {
        return parent->get_slowdown_factor();
    }
    return slowdown;
}

// FUNCTION: TH16 0x4064e0
void AnmVm::alloc_extra_data(u32 size)
{
    ins_508_extra_data_size = size;
    ins_508_extra_data = malloc(size);
}

// FUNCTION: TH16 0x406d80
void AnmVm::set_layer(i32 layer)
{
    this->layer = layer;
    if (this->layer >= 3 && this->layer <= 19)
    {
        flags_hi = flags_hi & ~ANM_VM_LAYER_KIND_MASK | ANM_VM_LAYER_SET;
    }
    else if (this->layer >= 20 && this->layer <= 23)
    {
        flags_hi = flags_hi & ~ANM_VM_LAYER_KIND_MASK | ANM_VM_LAYER_UI;
    }
    else
    {
        flags_hi &= ~ANM_VM_LAYER_KIND_MASK;
    }
    if (this->layer >= 20 && this->layer <= 31 || this->layer >= 36 && this->layer <= 42)
    {
        flags_hi = flags_hi & ~ANM_VM_COORD_MODE_MASK | ANM_VM_COORD_MODE_1;
    }
}

// FUNCTION: TH16 0x406ce0
void AnmVm::set_alpha1_time(i32 end_time, i32 method, u8 initial, u8 goal)
{
    alpha1_i.end_time = end_time;
    alpha1_i.method = method;
    alpha1_i.initial = initial;
    alpha1_i.bezier_1 = 0;
    alpha1_i.bezier_2 = 0;
    alpha1_i.goal = goal;
    alpha1_i.time = 0;
}

// TODO: fast_id is restored before entity_pos in ours (scheduling).
// FUNCTION: TH16 0x407a50
void AnmVm::wipe_suffix()
{
    Float3 saved_entity_pos = entity_pos;
    u32 saved_layer = layer;
    u32 saved_fast_id = fast_id;
    memset(&id, 0, sizeof(AnmVm) - offsetof(AnmVm, id));
    layer = saved_layer;
    fast_id = saved_fast_id;
    entity_pos = saved_entity_pos;
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
}

// FUNCTION: TH16 0x407b20
void AnmLoaded::copy_vm(AnmVm *dst, i32 script)
{
    dst->wipe_suffix();
    memcpy(dst, &vms[script], offsetof(AnmVm, id));
    dst->timer_1c = 0;
    dst->script_time = 0;
}
