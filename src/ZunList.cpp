#include <stddef.h>

#include "ZunList.h"
#include "decomp.h"

// FUNCTION: TH16 0x405610
template <> void ZunList<void>::unlink()
{
    if (next != NULL)
    {
        next->prev = prev;
    }
    if (prev != NULL)
    {
        prev->next = next;
    }
    next = NULL;
    prev = NULL;
}

// FUNCTION: TH16 0x425d80
template <> DECOMP_NOINLINE void ZunList<void>::append(ZunList<void> *node)
{
    ZunList<void> *last = this;
    while (last->next != NULL)
    {
        last = last->next;
    }
    last->insert_after(node);
}

// MSVC treats the specialization as inline (the template defines append in
// the class) and only emits it where it is used.
void zunlist_use_append(ZunList<void> *list, ZunList<void> *node)
{
    list->append(node);
}
