// Stand-ins for callees from other ranges whose bodies LTCG must see: the
// original callers keep values in registers across these calls, which LTCG
// only allows when it knows the callee leaves them alone. These are the
// real (small, recursive) bodies, kept out of line.
#include "../AnmManager.h"
#include "../AnmVm.h"

// STUB: TH16 0x46f220
DECOMP_NOINLINE void AnmManager::mark_tree_for_delete(AnmVm *vm)
{
    if (vm == NULL)
    {
        return;
    }
    if (vm->flags_hi & ANM_VM_FLAG_HI_4000000)
    {
        return;
    }
    vm->flags_hi = vm->flags_hi & ~ANM_VM_FLAG_HI_40 | ANM_VM_DELETE_PENDING;
    for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
    {
        mark_tree_for_delete(node->entry);
    }
}

// STUB: TH16 0x46f380
DECOMP_NOINLINE void AnmVm::set_flag_lo_2_tree()
{
    flags_lo |= ANM_VM_FLAG_LO_2;
    for (ZunList<AnmVm> *node = list_of_children.next; node != NULL; node = node->next)
    {
        node->entry->set_flag_lo_2_tree();
    }
}

// STUB: TH16 0x46f3b0
DECOMP_NOINLINE void AnmVm::clear_flag_lo_2_tree()
{
    flags_lo &= ~ANM_VM_FLAG_LO_2;
    for (ZunList<AnmVm> *node = list_of_children.next; node != NULL; node = node->next)
    {
        node->entry->clear_flag_lo_2_tree();
    }
}
