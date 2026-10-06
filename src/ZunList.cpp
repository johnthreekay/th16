#include <stddef.h>

#include "ZunList.h"

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
