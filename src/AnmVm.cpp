#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
#include "AnmVm.h"
#include "Supervisor.h"

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

// FUNCTION: TH16 0x43b900
HARNESS_CALLED void *AnmVm::scalar_delete(u32 flags)
{
    this->~AnmVm();
    if (flags & 1)
    {
        operator delete(this, sizeof(AnmVm));
    }
    return this;
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

// FUNCTION: TH16 0x447550
void AnmVm::set_scale_interp(i32 end_time, i32 method, D3DXVECTOR2 *initial, D3DXVECTOR2 *goal)
{
    scale_i.end_time = end_time;
    scale_i.method = method;
    scale_i.initial = *initial;
    scale_i.goal = *goal;
    scale_i.time.reset();
}

// FUNCTION: TH16 0x406240
void AnmVm::set_pos_bezier(i32 end_time, Float3 *initial, Float3 *bezier_1, Float3 *goal, Float3 *bezier_2)
{
    pos_i.end_time = end_time;
    pos_i.method = INTERP_BEZIER;
    pos_i.initial = *initial;
    pos_i.goal = *goal;
    pos_i.bezier_1 = *bezier_1;
    pos_i.bezier_2 = *bezier_2;
    pos_i.time.reset();
}

// FUNCTION: TH16 0x4174d0
void AnmVm::interrupt_out_of_line(i32 n)
{
    interrupt(n);
}

// FUNCTION: TH16 0x4173f0
HARNESS_CALLED void AnmVm::set_pos_time(i32 end_time, i32 method, Float3 *initial, Float3 *goal)
{
    pos_i.end_time = end_time;
    pos_i.bezier_1 = g_zero_vec;
    pos_i.bezier_2 = g_zero_vec;
    pos_i.method = method;
    pos_i.initial = *initial;
    pos_i.goal = *goal;
    pos_i.time.reset();
}

// The real body only touches pos itself and its own local, so LTCG's /GS
// analysis leaves callers that pass a local's address without a cookie.
// TODO: ours aligns the frame (the original's callers align theirs for it),
// multiplies and adds with swapped operands and shares one epilogue.
// FUNCTION: TH16 0x406a70
HARNESS_CALLED Float3 *AnmVm::transform_coords(Float3 *pos)
{
    f32 scale;
    u32 mode = flags_hi & ANM_VM_COORD_MODE_MASK;
    if (mode == 1 << 20 || mode == 3 << 20)
    {
        scale = g_screen_coord_scale;
    }
    else if (mode == 2 << 20 || mode == 4 << 20)
    {
        scale = g_screen_coord_scale * 0.5f;
    }
    else
    {
        goto scaled;
    }
    pos->x *= scale;
    pos->y *= scale;
    pos->z *= scale;
scaled:
    if (parent != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
    {
        if (flags_hi & ANM_VM_ROTATE_WITH_PARENT)
        {
            f32 s = zun_sinf(parent->rotation.z);
            f32 c = zun_cosf(parent->rotation.z);
            f32 x = pos->x;
            f32 y = pos->y;
            pos->x = x * c - y * s;
            pos->y = y * c + x * s;
        }
        Float3 offset;
        parent->get_own_transformed_pos(&offset);
        pos->x += offset.x;
        pos->y += offset.y;
        pos->z += offset.z;
        return pos;
    }
    u32 kind = flags_hi & ANM_VM_LAYER_KIND_MASK;
    if (kind != 0)
    {
        if (kind == ANM_VM_LAYER_SET)
        {
            pos->x += g_game_2d_origin_x;
            pos->y += g_game_2d_origin_y;
        }
        else
        {
            pos->x += g_arcade_hud_origin_x;
            pos->y += g_arcade_hud_origin_y;
        }
    }
    return pos;
}

// TODO: the original aligns the frame (for transform_coords) and adds
// pos + entity_pos with the operands the other way round.
// FUNCTION: TH16 0x406c40
HARNESS_CALLED Float3 *AnmVm::get_own_transformed_pos(Float3 *out)
{
    *out = pos + entity_pos + pos_2;
    transform_coords(out);
    return out;
}
