// AnmManager's VM pool and lists: allocation, ids, lookup and deletion.
#include <stddef.h>
#include <string.h>

#include "AnmManager.h"
#include "CriticalSections.h"
#include "GameThread.h"

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
HARNESS_CALLED AnmVm *AnmManager::get_snapshot_vm_with_id(AnmId id)
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
        fast->freelist_node.unlink_inline();
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

// FUNCTION: TH16 0x46e340
i32 __fastcall AnmManager::tick_world(AnmManager *mgr)
{
    ENTER_CS(CS_ANM_MANAGER);
    ZunList<AnmVm> delete_list;
    delete_list.entry = NULL;
    delete_list.next = NULL;
    delete_list.prev = NULL;
    delete_list.unk_c = NULL;
    AnmVm *layer_tails[43];
    for (i32 i = 0; i < 36; i++)
    {
        AnmVm *head = &mgr->layer_list_dummy_heads[i];
        layer_tails[i] = head;
        head->next_in_layer = NULL;
    }
    ZunList<AnmVm> *node = mgr->world_list_head;
    while (node != NULL)
    {
        ZunList<AnmVm> *next = node->next;
        AnmVm *vm = node->entry;
        u32 deletion = vm->flags_hi & (ANM_VM_DELETE | ANM_VM_FLAG_HI_40);
        if (deletion == ANM_VM_DELETE)
        {
            mgr->remove_tree(vm, &delete_list);
        }
        else if (deletion == 0)
        {
            if (vm->run())
            {
                mgr->remove_tree(vm, &delete_list);
            }
            else
            {
                // World VMs on the UI copies of layers 24-30 move back.
                if (vm->layer >= 36 && vm->layer <= 42)
                {
                    vm->layer -= 12;
                }
                layer_tails[vm->layer]->next_in_layer = vm;
                layer_tails[vm->layer] = vm;
                vm->next_in_layer = NULL;
                mgr->useless_count++;
            }
        }
        node = next;
    }
    node = delete_list.next;
    while (node != NULL)
    {
        ZunList<AnmVm> *next = node->next;
        mgr->destroy_possibly_managed_vm(node->entry);
        node = next;
    }
    LEAVE_CS(CS_ANM_MANAGER);
    return 1;
}

// FUNCTION: TH16 0x46e490
i32 __fastcall AnmManager::tick_ui(AnmManager *mgr)
{
    ENTER_CS(CS_ANM_MANAGER);
    // The UI list only uses layers 36-42.
    AnmVm *layer_tails[7];
    for (i32 i = 0; i < 7; i++)
    {
        layer_tails[i] = &mgr->layer_list_dummy_heads[36 + i];
        mgr->layer_list_dummy_heads[36 + i].next_in_layer = NULL;
    }
    mgr->useless_count = 0;
    ZunList<AnmVm> delete_list;
    delete_list.entry = NULL;
    delete_list.next = NULL;
    delete_list.prev = NULL;
    delete_list.unk_c = NULL;
    ZunList<AnmVm> *node = mgr->ui_list_head;
    while (node != NULL)
    {
        ZunList<AnmVm> *next = node->next;
        AnmVm *vm = node->entry;
        u32 deletion = vm->flags_hi & (ANM_VM_DELETE | ANM_VM_FLAG_HI_40);
        if (deletion == ANM_VM_DELETE)
        {
            mgr->remove_tree(vm, &delete_list);
        }
        else if (deletion == 0)
        {
            if (vm->run())
            {
                mgr->remove_tree(vm, &delete_list);
            }
            else
            {
                // UI VMs on layers 24-31 move to their UI copies.
                if (vm->layer >= 24 && vm->layer <= 31)
                {
                    vm->layer += 12;
                }
                else if (vm->layer < 36 || vm->layer > 42)
                {
                    vm->layer = 38;
                }
                layer_tails[vm->layer - 36]->next_in_layer = vm;
                layer_tails[vm->layer - 36] = vm;
                vm->next_in_layer = NULL;
                mgr->useless_count++;
            }
        }
        node = next;
    }
    node = delete_list.next;
    while (node != NULL)
    {
        ZunList<AnmVm> *next = node->next;
        mgr->destroy_possibly_managed_vm(node->entry);
        node = next;
    }
    LEAVE_CS(CS_ANM_MANAGER);
    return 1;
}

// FUNCTION: TH16 0x46e660
void AnmManager::remove_tree(AnmVm *vm, ZunList<AnmVm> *delete_list)
{
    ZunList<AnmVm> *node = vm->list_of_children.next;
    while (node != NULL)
    {
        ZunList<AnmVm> *next = node->next;
        remove_tree(node->entry, delete_list);
        node = next;
    }
    if ((vm->flags_hi & (ANM_VM_DELETE | ANM_VM_FLAG_HI_40)) != ANM_VM_FLAG_HI_40)
    {
        vm->unk_list_598.init(vm);
        delete_list->insert_after(&vm->unk_list_598);
    }
    vm->flags_hi &= ~ANM_VM_DELETE;
    vm->flags_hi |= ANM_VM_FLAG_HI_40;
    vm->parent = NULL;
    vm->unk_5b0 = NULL;
}

// FUNCTION: TH16 0x46e710
i32 __fastcall AnmManager::on_tick_21(AnmManager *mgr)
{
    if (g_GameThread != NULL && (g_GameThread->flags.flag_0 | g_GameThread->flags.paused) &&
        g_GameThread->flags.flag_1)
    {
        return 1;
    }
    return tick_world(mgr);
}

// FUNCTION: TH16 0x46e740
i32 __fastcall AnmManager::on_tick_09(AnmManager *mgr)
{
    return tick_ui(mgr);
}

// TODO: LTCG inlines the scalar deleting destructor here; the original calls 0x43b900.
// FUNCTION: TH16 0x46eab0
i32 AnmManager::destroy_possibly_managed_vm(AnmVm *vm)
{
    if (&vm->node_in_global_list == world_list_tail)
    {
        world_list_tail = vm->node_in_global_list.prev;
    }
    if (&vm->node_in_global_list == world_list_head)
    {
        world_list_head = vm->node_in_global_list.next;
    }
    if (&vm->node_in_global_list == ui_list_tail)
    {
        ui_list_tail = vm->node_in_global_list.prev;
    }
    if (&vm->node_in_global_list == ui_list_head)
    {
        ui_list_head = vm->node_in_global_list.next;
    }
    if (vm->index_of_on_destroy != 0)
    {
        g_anm_on_destroy_funcs[vm->index_of_on_destroy](vm);
    }
    vm->node_in_global_list.unlink_inline();
    vm->unk_list_598.unlink_inline();
    vm->node_as_child.unlink_inline();
    vm->parent = NULL;
    vm->unk_5b0 = NULL;
    if (vm >= &fast_array[0].vm && vm < &fast_array[0x1fff].vm)
    {
        fast_array[vm->fast_id].is_alive = false;
        freelist_head.insert_after(&fast_array[vm->fast_id].freelist_node);
        if (vm->ins_508_extra_data != NULL)
        {
            free(vm->ins_508_extra_data);
        }
        vm->ins_508_extra_data = NULL;
        vm->ins_508_extra_data_size = 0;
        vm->instr_offset = -1;
        vm->id.id = 0;
        return 0;
    }
    delete vm;
    return 0;
}

// TODO: LTCG inlines the scalar deleting destructor here; the original calls 0x43b900.
// FUNCTION: TH16 0x46ec90
i32 AnmManager::destroy_possibly_managed_snapshot_vm(AnmVm *vm)
{
    if (vm->index_of_on_destroy != 0)
    {
        g_anm_on_destroy_funcs[vm->index_of_on_destroy](vm);
    }
    vm->node_in_global_list.unlink_inline();
    if (vm >= &snapshot_fast_array[0].vm && vm < &snapshot_fast_array[0x1fff].vm)
    {
        snapshot_fast_array[vm->fast_id].is_alive = false;
        vm->~AnmVm();
        return 0;
    }
    delete vm;
    return 0;
}

// FUNCTION: TH16 0x46ed60
AnmId AnmLoaded::create_managed_child(i32 script, AnmVm *parent, i32 mode)
{
    ENTER_CS(CS_ANM_MANAGER);
    vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    vm->layer = parent->layer;
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    copy_vm(vm, script);
    vm->flags_hi = (vm->flags_hi & ~ANM_VM_FLAG_HI_2000000) | (parent->flags_hi & ANM_VM_FLAG_HI_2000000);
    vm->unk_5b0 = parent;
    vm->parent = parent->parent != NULL ? parent->parent : parent;
    vm->run();
    vm->mode_of_create_child = mode;
    AnmId id;
    if ((mode & 6) == 6)
    {
        id = g_AnmManager->insert_in_ui_list_front(vm);
    }
    else if (mode & 4)
    {
        id = g_AnmManager->insert_in_ui_list_back(vm);
    }
    else if (mode & 2)
    {
        id = g_AnmManager->insert_in_world_list_front(vm);
    }
    else
    {
        id = g_AnmManager->insert_in_world_list_back(vm);
    }
    parent->list_of_children.insert_after(&vm->node_as_child);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}

// FUNCTION: TH16 0x46eea0
AnmId AnmLoaded::create_managed_root(i32 script, AnmVm *like, i32 unused)
{
    ENTER_CS(CS_ANM_MANAGER);
    vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    copy_vm(vm, script);
    vm->layer = like->layer;
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    vm->flags_hi = (vm->flags_hi & ~ANM_VM_FLAG_HI_2000000) | (like->flags_hi & ANM_VM_FLAG_HI_2000000);
    vm->entity_pos = like->entity_pos;
    vm->rotation.x = like->rotation.x;
    vm->rotation.y = like->rotation.y;
    vm->rotation.z = like->rotation.z;
    vm->pos_2 = like->pos;
    vm->run();
    vm->mode_of_create_child = 0;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_back(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}

// FUNCTION: TH16 0x46fd50
void AnmVm::copy_from(const AnmVm &other, i32 arg)
{
    memcpy(this, &other, offsetof(AnmVm, id));
    ZunTimer timer = other.script_time;
    script_time = timer.current;
    timer = other.timer_1c;
    timer_1c = timer.current;
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
    next_in_layer = NULL;
    parent = NULL;
    unk_5b0 = NULL;
    slowdown = other.slowdown;
    entity_pos = other.entity_pos;
    associated_game_entity = other.associated_game_entity;
    index_of_sprite_mapping_func = other.index_of_sprite_mapping_func;
    index_of_on_wait = other.index_of_on_wait;
    index_of_on_tick = other.index_of_on_tick;
    index_of_on_draw = other.index_of_on_draw;
    index_of_on_destroy = other.index_of_on_destroy;
    index_of_on_interrupt = other.index_of_on_interrupt;
    index_of_on_copy_1 = other.index_of_on_copy_1;
    index_of_on_copy_2 = other.index_of_on_copy_2;
    if (other.ins_508_extra_data != NULL)
    {
        ins_508_extra_data_size = other.ins_508_extra_data_size;
        ins_508_extra_data = malloc(ins_508_extra_data_size);
        memcpy(ins_508_extra_data, other.ins_508_extra_data, ins_508_extra_data_size);
        if (other.index_of_on_copy_1 != 0)
        {
            g_anm_on_copy_funcs[other.index_of_on_copy_1](this, &other, arg);
        }
    }
}

// TODO: the original copies the new discriminator to ecx before storing it (slow path).
// FUNCTION: TH16 0x46f720
AnmVm *AnmManager::allocate_snapshot_vm(i32 *id)
{
    // Snapshot ids have the top bit set and an 18-bit discriminator.
    if (next_snapshot_fast_id >= 0x1fff)
    {
        next_snapshot_discriminator = (next_snapshot_discriminator + 1) & 0x3ffff;
        if (next_snapshot_discriminator == 0)
        {
            next_snapshot_discriminator = 1;
        }
        *id = (next_snapshot_discriminator << 13) | 0x80001fff;
        AnmVm *vm = new AnmVm;
        vm->wipe();
        vm->fast_id = 0x1fff;
        return vm;
    }
    AnmVm *vm = &snapshot_fast_array[next_snapshot_fast_id].vm;
    vm->wipe();
    snapshot_fast_array[next_snapshot_fast_id].is_alive = true;
    next_snapshot_discriminator = (next_snapshot_discriminator + 1) & 0x3ffff;
    if (next_snapshot_discriminator == 0)
    {
        next_snapshot_discriminator = 1;
    }
    i32 fast_id = next_snapshot_fast_id;
    *id = 0x80000000 | (next_snapshot_discriminator << 13) | fast_id;
    snapshot_fast_array[fast_id].is_alive = true;
    next_snapshot_fast_id++;
    return vm;
}

// FUNCTION: TH16 0x46f810
AnmId AnmManager::store_snapshot_of_vm(AnmVm *vm, AnmVm *parent, i32 unused)
{
    if (vm == NULL)
    {
        AnmId none;
        none.id = (i32)vm;
        return none;
    }
    i32 id;
    AnmVm *copy = allocate_snapshot_vm(&id);
    copy->copy_from(*vm, 0);
    copy->flags_hi |= ANM_VM_SNAPSHOT;
    copy->id.id = id;
    snapshot_list_head.insert_after(&copy->node_in_global_list);
    if (parent != NULL)
    {
        ((ZunList<void> *)&parent->list_of_children)->append((ZunList<void> *)&copy->node_as_child);
        if (parent->parent != NULL)
        {
            copy->parent = parent->parent;
        }
        copy->unk_5b0 = parent;
    }
    for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
    {
        store_snapshot_of_vm(node->entry, copy, 0);
    }
    AnmId result;
    result.id = id;
    return result;
}

// TODO: the original keeps the critical section flag in bl across the lookup.
// FUNCTION: TH16 0x46f8f0
HARNESS_CALLED AnmId AnmManager::restore_snapshot(AnmId id)
{
    if (id.id == 0)
    {
        return AnmId();
    }
    ENTER_CS(CS_ANM_MANAGER);
    AnmVm *snapshot = get_snapshot_vm_with_id(id);
    LEAVE_CS(CS_ANM_MANAGER);
    return restore_snapshot_vm(snapshot, NULL);
}

// TODO: the original stores unk_5b0 after loading the parent's parent.
// FUNCTION: TH16 0x46f970
AnmId AnmManager::restore_snapshot_vm(AnmVm *snapshot, AnmVm *parent)
{
    if (snapshot == NULL)
    {
        AnmId none;
        none.id = (i32)snapshot;
        return none;
    }
    ENTER_CS(CS_ANM_MANAGER);
    AnmVm *vm = g_AnmManager->allocate_vm();
    vm->copy_from(*snapshot, 1);
    vm->flags_hi &= ~ANM_VM_SNAPSHOT;
    i32 mode = vm->mode_of_create_child;
    AnmId id;
    if ((mode & 6) == 6)
    {
        id = g_AnmManager->insert_in_ui_list_front(vm);
        vm->flags_hi &= ~(ANM_VM_FLAG_HI_4000 | ANM_VM_FLAG_HI_8000);
    }
    else if (mode & 4)
    {
        id = g_AnmManager->insert_in_ui_list_back(vm);
        vm->flags_hi &= ~(ANM_VM_FLAG_HI_4000 | ANM_VM_FLAG_HI_8000);
    }
    else if (mode & 2)
    {
        id = g_AnmManager->insert_in_world_list_front(vm);
    }
    else
    {
        id = g_AnmManager->insert_in_world_list_back(vm);
    }
    if (parent != NULL)
    {
        vm->unk_5b0 = parent;
        vm->parent = parent->parent != NULL ? parent->parent : parent;
        parent->list_of_children.insert_after(&vm->node_as_child);
    }
    LEAVE_CS(CS_ANM_MANAGER);
    for (ZunList<AnmVm> *node = snapshot->list_of_children.next; node != NULL; node = node->next)
    {
        restore_snapshot_vm(node->entry, vm);
    }
    return id;
}
