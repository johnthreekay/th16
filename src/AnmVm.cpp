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
    if (extra_data != NULL)
    {
        free(extra_data);
    }
    extra_data = NULL;
    extra_data_size = 0;
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

// TODO: the flags_hi and/or and one pop are scheduled one store later in the original
// (not the bitfield view, a local, one expression, other positions or HARNESS_CALLED).
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
    D3DXMatrixIdentity(&sprite_matrix);
    // A 16-bit store in the original.
    *(u16 *)&flags_lo = 7;
    script_time.clear();
    time_in_script.clear();
    pos_i.end_time = 0;
    rgb1_i.end_time = 0;
    alpha1_i.end_time = 0;
    rotate_i.end_time = 0;
    scale_i.end_time = 0;
    scale_2_i.end_time = 0;
    uv_scale_i.end_time = 0;
    rgb2_i.end_time = 0;
    alpha2_i.end_time = 0;
    u_vel_i.end_time = 0;
    v_vel_i.end_time = 0;
    flags_hi &= ~ANM_VM_FREEZES_AFTER_FIRST_RUN;
    flags_hi |= ANM_VM_FREEZES_WITH_WORLD;
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
    node_in_delete_list.entry = this;
    node_in_delete_list.next = NULL;
    node_in_delete_list.prev = NULL;
    node_in_delete_list.unk_c = NULL;
}

// FUNCTION: TH16 0x406340
HARNESS_CALLED f32 AnmVm::get_slowdown_factor()
{
    if (root_vm != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
    {
        return root_vm->get_slowdown_factor();
    }
    return slowdown;
}

// FUNCTION: TH16 0x4064e0
void AnmVm::alloc_extra_data(u32 size)
{
    extra_data_size = size;
    extra_data = malloc(size);
}

// FUNCTION: TH16 0x406d80
void AnmVm::set_layer(i32 layer)
{
    this->layer = layer;
    if (this->layer >= ANM_LAYER_GAME_FIRST && this->layer <= ANM_LAYER_GAME_LAST)
    {
        flags_hi = flags_hi & ~ANM_VM_ORIGIN_MODE_MASK | ANM_VM_ORIGIN_GAME;
    }
    else if (this->layer >= ANM_LAYER_HUD_FIRST && this->layer <= ANM_LAYER_HUD_LAST)
    {
        flags_hi = flags_hi & ~ANM_VM_ORIGIN_MODE_MASK | ANM_VM_ORIGIN_HUD;
    }
    else
    {
        flags_hi &= ~ANM_VM_ORIGIN_MODE_MASK;
    }
    if (this->layer >= ANM_LAYER_HUD_FIRST && this->layer <= ANM_LAYER_UI_LAST ||
        this->layer >= ANM_LAYER_UI_LIST_FIRST && this->layer <= ANM_LAYER_UI_LIST_LAST)
    {
        flags_hi = flags_hi & ~ANM_VM_RESOLUTION_MODE_MASK | ANM_VM_RESOLUTION_SCALED;
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
    dst->time_in_script = 0;
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
    u32 mode = flags_hi & ANM_VM_RESOLUTION_MODE_MASK;
    if (mode == ANM_VM_RESOLUTION_SCALED || mode == ANM_VM_RESOLUTION_SCALED_3)
    {
        scale = g_screen_coord_scale;
    }
    else if (mode == ANM_VM_RESOLUTION_HALF_SCALED || mode == ANM_VM_RESOLUTION_HALF_SCALED_4)
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
    if (root_vm != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
    {
        if (flags_hi & ANM_VM_ROTATE_WITH_PARENT)
        {
            f32 s = zun_sinf(root_vm->rotation.z);
            f32 c = zun_cosf(root_vm->rotation.z);
            f32 x = pos->x;
            f32 y = pos->y;
            pos->x = x * c - y * s;
            pos->y = y * c + x * s;
        }
        Float3 offset;
        root_vm->get_own_transformed_pos(&offset);
        pos->x += offset.x;
        pos->y += offset.y;
        pos->z += offset.z;
        return pos;
    }
    u32 kind = flags_hi & ANM_VM_ORIGIN_MODE_MASK;
    if (kind != 0)
    {
        if (kind == ANM_VM_ORIGIN_GAME)
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

// FUNCTION: TH16 0x406c40
Float3 *AnmVm::get_own_transformed_pos(Float3 *out)
{
    *out = entity_pos + pos + pos_2;
    transform_coords(out);
    return out;
}
