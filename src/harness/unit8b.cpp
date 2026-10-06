// Stand-in callers for unit 8b (0x469000-0x474c00) functions whose shape
// depends on how the rest of the game calls them.
#include "../AnmManager.h"

// Like AnmLoaded::create_managed_child (0x46ed60), which picks the list by
// its mode flags. Every caller goes through g_AnmManager.
AnmId harness_unit8b_insert(AnmVm *vm, i32 mode)
{
    if ((mode & 6) == 6)
    {
        return g_AnmManager->insert_in_ui_list_front(vm);
    }
    if (mode & 4)
    {
        return g_AnmManager->insert_in_ui_list_back(vm);
    }
    if (mode & 2)
    {
        return g_AnmManager->insert_in_world_list_front(vm);
    }
    return g_AnmManager->insert_in_world_list_back(vm);
}

// Supervisor creates the manager and stores it here.
void harness_unit8b_set_anm_manager(AnmManager *anm)
{
    g_AnmManager = anm;
}

// Like the manager's tick, which marks finished trees.
void harness_unit8b_mark_tree(AnmVm *vm)
{
    g_AnmManager->mark_tree_for_deletion(vm);
}

// The callers of AnmId::search_children (ANM and ECL instructions) always
// pass n = 0, and the recursive AnmVm helpers are called from several
// places in the original.
AnmId harness_unit8b_search_children(AnmId *id, i32 unk)
{
    return id->search_children(unk, 0);
}

void harness_unit8b_ins_316(AnmVm *vm, i32 set)
{
    if (set)
    {
        vm->set_ins_316_flag_recursively();
    }
    else
    {
        vm->clear_ins_316_flag_recursively();
    }
}

// VMs that did not come from the pool are deleted in several places
// (0x43b900 has many callers), so LTCG keeps the scalar deleting
// destructor out of line.
void harness_unit8b_delete_vm_1(AnmVm *vm)
{
    delete vm;
}

void harness_unit8b_delete_vm_2(AnmVm *vm)
{
    delete vm;
}

void harness_unit8b_delete_vm_3(AnmVm *vm)
{
    delete vm;
}
