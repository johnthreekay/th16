#pragma once

#include "AnmManager.h"
#include "Thread.h"
#include "UpdateFunc.h"
#include "types.h"

// Loads the shared resources at startup (sig.anm, the ASCII manager,
// text.anm, the BGM, the shared anm files and the score file) on a worker
// thread while the loading screen shows the logo, then goes to the title.
// Layout from ExpHP's th-re-data (zLoadingThread).
struct LoadingThread
{
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    ThreadInf thread;
    u8 unk_28[4];
    // Not used.
    AnmVm vm;
    // The sig.anm logo.
    AnmId anm_id;
    AnmLoaded *sig_anm;
    // 1 once the thread has loaded sig.anm (on_draw then shows the logo
    // and makes it 2), and likewise for the "now loading" text once the
    // ASCII manager is up.
    i32 logo_step;
    i32 now_loading_step;
    // Frames drawn; the title waits for 180 of them.
    i32 draw_count;

    LoadingThread();
    ~LoadingThread();
    int initialize();
    static LoadingThread *create();

    // Started through ThreadInf::restart; like the other loaders a plain
    // cdecl function. On failure the game quits (game mode 3).
    static int thread_start(void *arg);
    // Registered through jmp thunks (ExpHP's "__stub" functions).
    DECOMP_NOINLINE int on_tick();
    DECOMP_NOINLINE int on_draw();
    static int __fastcall on_tick_thunk(void *arg);
    static int __fastcall on_draw_thunk(void *arg);
};

extern LoadingThread *g_LoadingThread;
