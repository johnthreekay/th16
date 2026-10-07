#pragma once

#include <windows.h>

#include "decomp.h"

// The game's critical sections (g_CriticalSections.cs). The ones not
// listed (1, 4, 7, 8 and 13) are never entered in TH16.
enum CriticalSectionId
{
    // Guards g_UpdateFuncRegistry's chains.
    CS_UPDATE_FUNC_REGISTRY = 0,
    // Guards file access: file_read_all, file_exists and file_write, and
    // file_open/file_create until file_close (FileSystem.h).
    CS_FILE = 2,
    // Guards g_GameErrorContext's log buffer.
    CS_GAME_ERROR_CONTEXT = 3,
    // Held while Supervisor::switch_gamemodes switches.
    // GameWindow::get_runtime also takes it around its timer state.
    CS_SUPERVISOR_GAMEMODE = 5,
    // Guards restarting g_Supervisor.thread.
    CS_SUPERVISOR_THREAD = 6,
    // Guards AnmManager's loaded files and VM lists.
    CS_ANM_MANAGER = 9,
    // Guards Rng::rand_u16 and rand_u32.
    CS_RNG = 10,
    // Guards SoundManager's BGM command queue.
    CS_SOUND = 11,
    // Guards the streaming BGM buffer (DSUtil.cpp), shared with the sound
    // thread.
    CS_BGM_STREAM = 12,
    CS_COUNT = 14,
};

// The game's locks, entered with ENTER_CS and left with LEAVE_CS. Locking
// is optional: WinMain initializes the sections and sets enabled, and the
// macros do nothing while it is clear. depth counts how often each section
// is held.
struct CriticalSections
{
    CRITICAL_SECTION cs[CS_COUNT];
    unsigned char depth[CS_COUNT];
    bool enabled;

    // LEAVE_CS as a function. LTCG kept an out-of-line copy for a few
    // callers (file loading among them) and folded `this` into it. Written
    // against g_CriticalSections rather than this: our build stops folding
    // `this` once callers in several objects pass it.
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
