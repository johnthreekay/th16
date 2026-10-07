#include "CriticalSections.h"

// GLOBAL: TH16 0x4c0f58
CriticalSections g_CriticalSections;

// The body is the LEAVE_CS macro, which names g_CriticalSections rather
// than this, so LTCG drops the unused this (callers never set ecx).
// FUNCTION: TH16 0x405640
HARNESS_CALLED void CriticalSections::leave(int i)
{
    LEAVE_CS(i);
}
