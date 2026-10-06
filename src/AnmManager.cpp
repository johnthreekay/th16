#include <math.h>

#include "AnmManager.h"
#include "CriticalSections.h"

// FUNCTION: TH16 0x40d460
void AnmLoaded::copy_vm_and_run(AnmVm *vm, i32 script)
{
    copy_vm(vm, script);
    vm->unk_5b0 = NULL;
    vm->parent = NULL;
    vm->run();
}

// TODO: our build adds a /GS cookie for the parent position temporary and
// sums the vectors in a different register order.
// FUNCTION: TH16 0x40e490
D3DXVECTOR3 AnmVm::world_pos()
{
    D3DXVECTOR3 result;
    result.x = pos.x + entity_pos.x + pos_2.x;
    result.y = pos.y + entity_pos.y + pos_2.y;
    result.z = pos.z + entity_pos.z + pos_2.z;
    if (parent != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
    {
        if (flags_hi & ANM_VM_ROTATE_WITH_PARENT)
        {
            f32 s = sinf(parent->rotation.z);
            f32 c = cosf(parent->rotation.z);
            f32 x = result.x;
            f32 y = result.y;
            result.x = x * c - y * s;
            result.y = y * c + x * s;
        }
        result += parent->world_pos();
    }
    return result;
}

// TODO: the original frame has 4 more bytes and saves esi before taking
// the critical section.
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
        if (layer <= 23)
        {
            vm->flags_hi &= ~ANM_VM_LAYER_UI;
            vm->flags_hi |= ANM_VM_LAYER_SET;
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
    vm->mode_of_create_child = 0;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_back(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}
