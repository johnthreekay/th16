#include <math.h>

#include "AnmManager.h"
#include "CriticalSections.h"

// FUNCTION: TH16 0x40d460
void AnmLoaded::copy_vm_and_run(AnmVm *vm, i32 script)
{
    copy_vm(vm, script);
    vm->parent_vm = NULL;
    vm->root_vm = NULL;
    vm->run();
}

// TODO: only difference: ours adds a /GS cookie for the recursive call's parent_pos (with the body in an inline helper the cookie goes, but parent stays cached in edi).
// FUNCTION: TH16 0x40e490
D3DXVECTOR3 AnmVm::world_pos()
{
    D3DXVECTOR3 result;
    result = entity_pos + pos + pos_2;
    if (root_vm != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
    {
        if (flags_hi & ANM_VM_ROTATE_WITH_PARENT)
        {
            f32 s = zun_sinf(root_vm->rotation.z);
            f32 c = zun_cosf(root_vm->rotation.z);
            f32 x = result.x;
            f32 y = result.y;
            result.x = x * c - y * s;
            result.y = y * c + x * s;
        }
        D3DXVECTOR3 parent_pos = root_vm->world_pos();
        result += parent_pos;
    }
    return result;
}

// TODO: the original realigns the frame (and esp, -8; sub esp, 0x10) and so saves esi in the prologue; ours pushes esi after entering the critical section.
// FUNCTION: TH16 0x406380
DECOMP_NOINLINE AnmId AnmLoaded::create_effect(i32 script, i32 layer, AnmVm **out)
{
    ENTER_CS(CS_ANM_MANAGER);
    vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    if (out != NULL)
    {
        *out = vm;
    }
    copy_vm(vm, script);
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    if (layer >= 0)
    {
        vm->layer = layer;
        if (layer <= ANM_LAYER_HUD_LAST)
        {
            vm->flags_hi &= ~ANM_VM_ORIGIN_HUD;
            vm->flags_hi |= ANM_VM_ORIGIN_GAME;
        }
    }
    vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    vm->rotation.z = 0.0f;
    vm->run();
    vm->mode_of_create_child = ANM_CREATE_WORLD_BACK;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_back(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}

// FUNCTION: TH16 0x40e5c0
HARNESS_CALLED AnmId AnmLoaded::create_vm(i32 script, D3DXVECTOR3 *pos, f32 rotation, i32 layer, i32 unused)
{
    ENTER_CS(CS_ANM_MANAGER);
    vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    copy_vm(vm, script);
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    if (layer >= 0)
    {
        vm->layer = layer;
        if (layer <= ANM_LAYER_HUD_LAST)
        {
            vm->flags_hi &= ~ANM_VM_ORIGIN_HUD;
            vm->flags_hi |= ANM_VM_ORIGIN_GAME;
        }
    }
    if (pos == NULL)
    {
        vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        vm->entity_pos = *pos;
    }
    vm->rotation.z = rotation;
    vm->run();
    vm->mode_of_create_child = ANM_CREATE_WORLD_BACK;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_back(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}
