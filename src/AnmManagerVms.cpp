// AnmManager's VM pool and lists: allocation, ids, lookup and deletion.
#include <stddef.h>
#include <string.h>

#include "AnmManager.h"

static_assert(sizeof(AnmFastVm) == 0x614, "AnmFastVm size");
static_assert(offsetof(AnmManager, fast_array) == 0xec, "AnmManager fast_array");
static_assert(offsetof(AnmManager, freelist_head) == 0x184f4dc, "AnmManager freelist_head");
static_assert(offsetof(AnmManager, loaded_anms) == 0x184f4f0, "AnmManager loaded_anms");
static_assert(offsetof(AnmManager, vertex_buffers) == 0x184fc18, "AnmManager vertex_buffers");
static_assert(offsetof(AnmManager, last_discriminator) == 0x1c7fd84, "AnmManager last_discriminator");
static_assert(sizeof(AnmManager) == 0x1c7fd90, "AnmManager size");

// GLOBAL: TH16 0x4c0f48
AnmManager *g_AnmManager;

// TODO: the original keeps the manager in edx and the node in ecx (ours swaps them).
// FUNCTION: TH16 0x46e7d0
HARNESS_CALLED AnmId AnmManager::insert_in_world_list_back(AnmVm *vm)
{
    vm->node_in_global_list.init(vm);
    if (world_list_head == NULL)
    {
        world_list_head = &vm->node_in_global_list;
    }
    else
    {
        world_list_tail->insert_after(&vm->node_in_global_list);
    }
    world_list_tail = &vm->node_in_global_list;
    last_discriminator++;
    last_discriminator &= 0x7ffff;
    if (last_discriminator == 0)
    {
        last_discriminator++;
    }
    vm->id.id = (vm->fast_id & 0x1fff) | (last_discriminator << 13);
    return vm->id;
}

// TODO: the original keeps the manager in edx and the node in ecx (ours swaps them).
// FUNCTION: TH16 0x46e890
HARNESS_CALLED AnmId AnmManager::insert_in_world_list_front(AnmVm *vm)
{
    vm->node_in_global_list.init(vm);
    if (world_list_head == NULL)
    {
        world_list_tail = &vm->node_in_global_list;
    }
    else
    {
        vm->node_in_global_list.insert_after(world_list_head);
    }
    world_list_head = &vm->node_in_global_list;
    last_discriminator++;
    last_discriminator &= 0x7ffff;
    if (last_discriminator == 0)
    {
        last_discriminator++;
    }
    vm->id.id = (vm->fast_id & 0x1fff) | (last_discriminator << 13);
    return vm->id;
}

// TODO: the original keeps the manager in edx and the node in ecx (ours swaps them).
// FUNCTION: TH16 0x46e940
HARNESS_CALLED AnmId AnmManager::insert_in_ui_list_back(AnmVm *vm)
{
    vm->node_in_global_list.init(vm);
    if (ui_list_head == NULL)
    {
        ui_list_head = &vm->node_in_global_list;
    }
    else
    {
        ui_list_tail->insert_after(&vm->node_in_global_list);
    }
    ui_list_tail = &vm->node_in_global_list;
    last_discriminator++;
    last_discriminator &= 0x7ffff;
    if (last_discriminator == 0)
    {
        last_discriminator++;
    }
    vm->id.id = (vm->fast_id & 0x1fff) | (last_discriminator << 13);
    return vm->id;
}

// TODO: the original keeps the manager in edx and the node in ecx (ours swaps them).
// FUNCTION: TH16 0x46ea00
HARNESS_CALLED AnmId AnmManager::insert_in_ui_list_front(AnmVm *vm)
{
    vm->node_in_global_list.init(vm);
    if (ui_list_head == NULL)
    {
        ui_list_tail = &vm->node_in_global_list;
    }
    else
    {
        vm->node_in_global_list.insert_after(ui_list_head);
    }
    ui_list_head = &vm->node_in_global_list;
    last_discriminator++;
    last_discriminator &= 0x7ffff;
    if (last_discriminator == 0)
    {
        last_discriminator++;
    }
    vm->id.id = (vm->fast_id & 0x1fff) | (last_discriminator << 13);
    return vm->id;
}

// TODO: the original returns NULL for id 0 straight away instead of jumping to a shared return.
// FUNCTION: TH16 0x46efa0
AnmVm *AnmManager::get_vm_with_id(AnmId id)
{
    if (id.id == 0)
    {
        return NULL;
    }
    i32 fast_id = id.id & 0x1fff;
    if (fast_id == 0x1fff)
    {
        for (ZunList<AnmVm> *node = world_list_head; node != NULL; node = node->next)
        {
            if (node->entry->id.id == id.id)
            {
                return node->entry;
            }
        }
        for (ZunList<AnmVm> *node = ui_list_head; node != NULL; node = node->next)
        {
            if (node->entry->id.id == id.id)
            {
                return node->entry;
            }
        }
    }
    else if (fast_array[fast_id].is_alive && fast_array[fast_id].vm.id.id == id.id)
    {
        return &fast_array[fast_id].vm;
    }
    return NULL;
}

// FUNCTION: TH16 0x46f040
AnmVm *AnmManager::get_snapshot_vm_with_id(AnmId id)
{
    if (id.id == 0)
    {
        return NULL;
    }
    AnmVm *vm = NULL;
    i32 fast_id = id.id & 0x1fff;
    if (fast_id == 0x1fff)
    {
        for (ZunList<AnmVm> *node = &snapshot_list_head; node != NULL; node = node->next)
        {
            if (node->entry->id.id == id.id)
            {
                vm = node->entry;
                break;
            }
        }
    }
    else
    {
        vm = &snapshot_fast_array[fast_id].vm;
    }
    return vm;
}

// FUNCTION: TH16 0x46f0b0
void __stdcall AnmManager::interrupt_tree(AnmId id, i32 interrupt)
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    if (vm == NULL)
    {
        return;
    }
    vm->interrupt(interrupt);
    for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
    {
        node->entry->interrupt(interrupt);
    }
}

// TODO: the original aligns its frame to 8 bytes and reserves a slot.
// FUNCTION: TH16 0x46f130
void __stdcall AnmManager::interrupt_tree_and_run(AnmId id, i32 interrupt)
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    if (vm == NULL)
    {
        return;
    }
    vm->interrupt(interrupt);
    vm->run();
    for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
    {
        node->entry->interrupt(interrupt);
        node->entry->run();
    }
}

// TODO: the original loads the first child after storing the flags.
// FUNCTION: TH16 0x46f1c0
HARNESS_CALLED void AnmManager::delete_vm(AnmId id)
{
    mark_tree_for_deletion(get_vm_with_id(id));
}

// FUNCTION: TH16 0x46f220
HARNESS_CALLED_INLINABLE void AnmManager::mark_tree_for_deletion(AnmVm *vm)
{
    if (vm == NULL || (vm->flags_hi & ANM_VM_SNAPSHOT))
    {
        return;
    }
    vm->mark_for_deletion();
    for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
    {
        mark_tree_for_deletion(node->entry);
    }
}

// TODO: the original keeps the VM in ecx and the next node in edx (ours swaps them).
// FUNCTION: TH16 0x46f270
void AnmManager::disable_vms_from_anm_file(AnmLoaded *anm)
{
    if (anm == NULL)
    {
        return;
    }
    ZunList<AnmVm> *node = world_list_head;
    while (node != NULL)
    {
        ZunList<AnmVm> *next;
        AnmVm *vm = node->entry;
        next = node->next;
        if (vm->anm_loaded_index == anm->slot_num)
        {
            node->entry->mark_for_deletion();
        }
        node = next;
    }
    node = ui_list_head;
    while (node != NULL)
    {
        ZunList<AnmVm> *next;
        AnmVm *vm = node->entry;
        next = node->next;
        if (vm->anm_loaded_index == anm->slot_num)
        {
            node->entry->mark_for_deletion();
        }
        node = next;
    }
}

// FUNCTION: TH16 0x46f2e0
AnmVm *AnmId::find_or_clear()
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(*this);
    if (vm == NULL)
    {
        id = 0;
    }
    return vm;
}

// FUNCTION: TH16 0x46f300
void AnmId::set_ins_316_flag_recursively()
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(*this);
    if (vm != NULL)
    {
        vm->set_ins_316_flag_recursively();
    }
}

// FUNCTION: TH16 0x46f340
void AnmId::clear_ins_316_flag_recursively()
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(*this);
    if (vm != NULL)
    {
        vm->clear_ins_316_flag_recursively();
    }
}

// FUNCTION: TH16 0x46f380
HARNESS_CALLED_INLINABLE void AnmVm::set_ins_316_flag_recursively()
{
    flags_lo |= 2;
    for (ZunList<AnmVm> *node = list_of_children.next; node != NULL; node = node->next)
    {
        node->entry->set_ins_316_flag_recursively();
    }
}

// FUNCTION: TH16 0x46f3b0
HARNESS_CALLED_INLINABLE void AnmVm::clear_ins_316_flag_recursively()
{
    flags_lo &= ~2;
    for (ZunList<AnmVm> *node = list_of_children.next; node != NULL; node = node->next)
    {
        node->entry->clear_ins_316_flag_recursively();
    }
}

// FUNCTION: TH16 0x46f3e0
void AnmId::set_entity_pos(D3DXVECTOR3 *pos)
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(*this);
    if (vm != NULL)
    {
        vm->entity_pos = *pos;
    }
}

// FUNCTION: TH16 0x46f410
void AnmVm::set_sprite(i32 sprite)
{
    AnmLoaded *anm = g_AnmManager->loaded_anms[anm_loaded_index];
    if (anm != NULL)
    {
        anm->set_sprite(this, sprite);
    }
}

// FUNCTION: TH16 0x46f440
void AnmId::replace_with_effect(i32 script)
{
    AnmManager *mgr = g_AnmManager;
    AnmVm *vm = mgr->get_vm_with_id(*this);
    if (vm == NULL)
    {
        return;
    }
    AnmLoaded *anm = mgr->loaded_anms[vm->anm_loaded_index];
    vm->flags_lo &= ~ANM_VM_VISIBLE;
    vm->instr_offset = -1;
    *this = anm->create_effect(script, -1, NULL);
}

// FUNCTION: TH16 0x46f490
void AnmLoadedD3D::clear_texture()
{
    IDirect3DSurface9 *surface;
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT rect;
    texture->GetSurfaceLevel(0, &surface);
    surface->GetDesc(&desc);
    surface->LockRect(&rect, NULL, 0);
    memset(rect.pBits, 0, rect.Pitch * desc.Height);
    surface->UnlockRect();
    if (surface != NULL)
    {
        surface->Release();
    }
}

// FUNCTION: TH16 0x46f510
AnmVm *AnmVm::search_children(i32 unk_49c, i32 n)
{
    for (ZunList<AnmVm> *node = &list_of_children; node != NULL; node = node->next)
    {
        AnmVm *child = node->entry;
        if (child == NULL || child == this)
        {
            continue;
        }
        if (child->unk_49c == unk_49c || unk_49c == -1)
        {
            if (n == 0)
            {
                return child;
            }
            n--;
        }
        if (child->list_of_children.next != NULL)
        {
            AnmVm *found = child->search_children(unk_49c, n);
            if (found != NULL)
            {
                return found;
            }
        }
        if (this->unk_49c == -2 && node->next == NULL)
        {
            return node->entry;
        }
    }
    return NULL;
}

// FUNCTION: TH16 0x46f5a0
HARNESS_CALLED AnmId AnmId::search_children(i32 unk_49c, i32 n)
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(*this);
    if (vm == NULL)
    {
        id = 0;
        return AnmId();
    }
    AnmVm *found = vm->search_children(unk_49c, n);
    AnmId result;
    result.id = found != NULL ? found->id.id : 0;
    return result;
}

// FUNCTION: TH16 0x46f600
HARNESS_CALLED AnmVm *AnmManager::allocate_vm()
{
    if (freelist_head.next != NULL)
    {
        AnmFastVm *fast = freelist_head.next->entry;
        ZunList<AnmFastVm> *node = &fast->freelist_node;
        if (node->next != NULL)
        {
            node->next->prev = node->prev;
        }
        if (node->prev != NULL)
        {
            node->prev->next = node->next;
        }
        node->next = NULL;
        node->prev = NULL;
        fast->vm.fast_id = fast->fast_id;
        fast->is_alive = true;
        fast->vm.parent = NULL;
        fast->vm.unk_5b0 = NULL;
        // ZUN's code stores the manager here; whoever links the node in
        // overwrites it.
        fast->vm.node_in_global_list.entry = (AnmVm *)this;
        fast->vm.node_in_global_list.next = NULL;
        fast->vm.node_in_global_list.prev = NULL;
        fast->vm.node_in_global_list.unk_c = NULL;
        fast->vm.node_as_child.entry = (AnmVm *)this;
        fast->vm.node_as_child.next = NULL;
        fast->vm.node_as_child.prev = NULL;
        fast->vm.node_as_child.unk_c = NULL;
        fast->vm.list_of_children.entry = (AnmVm *)this;
        fast->vm.list_of_children.next = NULL;
        fast->vm.list_of_children.prev = NULL;
        fast->vm.list_of_children.unk_c = NULL;
        return &fast->vm;
    }
    AnmVm *vm = new AnmVm;
    vm->wipe();
    vm->fast_id = 0x1fff;
    return vm;
}
