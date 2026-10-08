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

// Adds the parent's world position. Matching workaround: written in
// world_pos, the parent_pos local (its address goes to the recursive call)
// gives world_pos a /GS cookie the original lacks; in a safebuffers
// forceinline helper it does not (docs/findings.md).
static __declspec(safebuffers) __forceinline void world_pos_add_parent(D3DXVECTOR3 *result, AnmVm *root)
{
    D3DXVECTOR3 parent_pos = root->world_pos();
    *result += parent_pos;
}

// The VM's position plus its root VM's, rotated with the root's rotation if
// the VM asks for it.
// TODO: effective match only: the original loads root_vm for the test after
// storing the result; ours before.
// FUNCTION: TH16 0x40e490
D3DXVECTOR3 AnmVm::world_pos()
{
    // A dead named local, not ZUN's code: with parent_pos moved into the
    // helper, it keeps the count of named variables that gives the
    // original's operand order for the position sums (docs/findings.md).
    i32 unused = 0;
    (void)unused;
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
        // root_vm read again (volatile) after the result stores: the original
        // reloads it there, which the helper's local result no longer forces.
        world_pos_add_parent(&result, *(AnmVm *volatile *)&root_vm);
    }
    return result;
}

// The original realigns this frame (and esp, -8) and passes the wish for
// an aligned stack on to the callers that call it directly
// (PlayerBullet::create and through it Player::do_shooting); most callers
// go through create_effect_via_pointer, which keeps it from them.
// FUNCTION: TH16 0x406380
DECOMP_NOINLINE AnmId AnmLoaded::create_effect(i32 script, i32 layer, AnmVm **out)
{
    // Dead double math, not ZUN's code: it stands in for whatever made LTCG's
    // double alignment pass count this function as wanting an 8-aligned
    // stack. It takes three multiplies to realign the frame itself; with
    // fewer, only the callers realign.
    double unused = 0.0;
    unused = unused * 2.0;
    unused = unused * 2.0;
    unused = unused * 2.0;
    (void)unused;
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

// A dead double, not ZUN's code: it stands in for AnmVm::run wanting an
// aligned stack (docs/findings.md). In this plain inline helper it is a call
// graph node of its own, so create_vm does not realign itself; its callers
// are all aligned and visible (HARNESS_CALLED), so it gets known alignment
// and its frame has the original's 4 unused bytes.
static inline void create_vm_want_aligned_stack()
{
    double unused_double = 0.0;
    (void)unused_double;
}

// FUNCTION: TH16 0x40e5c0
HARNESS_CALLED AnmId AnmLoaded::create_vm(i32 script, D3DXVECTOR3 *pos, f32 rotation, i32 layer, i32 unused)
{
    create_vm_want_aligned_stack();
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
