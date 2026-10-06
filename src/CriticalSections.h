#pragma once

#include <windows.h>

enum
{
    CS_UPDATE_FUNC_REGISTRY = 0,
    CS_FILE = 2,
    CS_GAME_ERROR_CONTEXT = 3,
    // Guards restarting g_Supervisor.thread.
    CS_SUPERVISOR_THREAD = 6,
    CS_RNG = 10,
    CS_COUNT = 14,
};

// Optional locking; only active when the game runs its threaded mode.
struct CriticalSections
{
    CRITICAL_SECTION cs[CS_COUNT];
    unsigned char depth[CS_COUNT];
    bool enabled;
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
