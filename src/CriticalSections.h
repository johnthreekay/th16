#pragma once

#include <windows.h>

#include "decomp.h"

enum
{
    CS_UPDATE_FUNC_REGISTRY = 0,
    CS_FILE = 2,
    CS_GAME_ERROR_CONTEXT = 3,
    CS_RNG = 10,
    CS_COUNT = 14,
};

// Optional locking; only active when the game runs its threaded mode.
struct CriticalSections
{
    CRITICAL_SECTION cs[CS_COUNT];
    unsigned char depth[CS_COUNT];
    bool enabled;

    // LEAVE_CS as a function. LTCG kept an out-of-line copy for a few
    // callers (file loading among them) and folded `this` into it.
    void leave(int i);
};

extern CriticalSections g_CriticalSections;

#define ENTER_CS(i) \
    if (g_CriticalSections.enabled) \
    { \
        EnterCriticalSection(&g_CriticalSections.cs[i]); \
        g_CriticalSections.depth[i]++; \
    }

#define LEAVE_CS(i) \
    if (g_CriticalSections.enabled) \
    { \
        LeaveCriticalSection(&g_CriticalSections.cs[i]); \
        g_CriticalSections.depth[i]--; \
    }
