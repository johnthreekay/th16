// AnmVm helpers for ECL's anm instructions, placed among the enemy code.
#include "AnmManager.h"
#include "AnmVm.h"
#include "CriticalSections.h"
#include "PosVel.h"

// FUNCTION: TH16 0x425dd0
void AnmVm::fade_alpha2(i32 end_time, i32 method, u8 goal)
{
    alpha2_i.end_time = end_time;
    alpha2_i.method = method;
    alpha2_i.initial = color_2.a;
    alpha2_i.goal = goal;
    alpha2_i.time = 0;
    flags_lo = flags_lo & ~0x40000 | 0x20000;
}

// FUNCTION: TH16 0x425e70
void AnmVm::fade_alpha1(i32 end_time, i32 method, u8 goal)
{
    alpha1_i.end_time = end_time;
    alpha1_i.method = method;
    alpha1_i.initial = color_1.a;
    alpha1_i.bezier_1 = 0;
    alpha1_i.bezier_2 = 0;
    alpha1_i.goal = goal;
    alpha1_i.time = 0;
}

// FUNCTION: TH16 0x425f10
void AnmVm::fade_rgb1(i32 end_time, i32 method, ZunColor *goal)
{
    rgb1_i.end_time = end_time;
    rgb1_i.bezier_1 = Int3(0, 0, 0);
    rgb1_i.bezier_2 = rgb1_i.bezier_1;
    rgb1_i.method = method;
    Int3 from(color_1.b, color_1.g, color_1.r);
    // Filled back to front: the original reads goal's bytes in this order.
    Int3 to;
    to.z = goal->r;
    to.y = goal->g;
    to.x = goal->b;
    rgb1_i.initial = from;
    rgb1_i.goal = to;
    rgb1_i.time = 0;
}

// FUNCTION: TH16 0x426020
HARNESS_CALLED void AnmVm::scale_to(i32 end_time, i32 method, f32 x, f32 y)
{
    scale_i.end_time = end_time;
    scale_i.method = method;
    scale_i.initial = scale;
    scale_i.goal = D3DXVECTOR2(x, y);
    scale_i.time = 0;
}

// FUNCTION: TH16 0x4260d0
HARNESS_CALLED void PosVel::set_ellipse_angle(f32 angle)
{
    ellipse_angle.value = wrap_angle(wrap_angle(angle));
}

// TODO: same as create_vm: the original frame has 4 more bytes and saves
// esi before taking the critical section.
// FUNCTION: TH16 0x426160
HARNESS_CALLED AnmId AnmLoaded::create_vm_front(i32 script, i32 layer, i32 unused)
{
    ENTER_CS(CS_ANM_MANAGER);
    vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    copy_vm(vm, script);
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    if (layer >= 0)
    {
        vm->layer = layer;
        if (layer <= 23)
        {
            vm->flags_hi &= ~ANM_VM_LAYER_UI;
            vm->flags_hi |= ANM_VM_LAYER_SET;
        }
    }
    vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    vm->rotation.z = 0.0f;
    vm->run();
    vm->mode_of_create_child = 2;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_front(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}
