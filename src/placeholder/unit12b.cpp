// Stand-ins for other units' functions that unit 12b's callers need LTCG
// to see (custom conventions, registers they leave alone). Compiled with
// /GL and not forced alive.
#include "../AnmManager.h"

// The real body: callers keep values in edx across calls to it, which LTCG
// only allows when it can see that the callee never touches edx.
// STUB: TH16 0x46f220
DECOMP_NOINLINE void AnmManager::mark_tree_for_deletion(AnmVm *vm)
{
    if (vm != NULL && !(vm->flags_hi & 0x4000000))
    {
        vm->flags_hi &= ~0x40;
        vm->flags_hi |= 0x20;
        for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
        {
            mark_tree_for_deletion(node->entry);
        }
    }
}

