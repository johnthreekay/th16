#include "CriticalSections.h"

// GLOBAL: TH16 0x4c0f58
CriticalSections g_CriticalSections;

// FUNCTION: TH16 0x405640
HARNESS_CALLED void CriticalSections::leave(int i)
{
    if (g_CriticalSections.enabled)
    {
        LeaveCriticalSection(&g_CriticalSections.cs[i]);
        g_CriticalSections.depth[i]--;
    }
}
