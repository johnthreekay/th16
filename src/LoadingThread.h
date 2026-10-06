#pragma once

#include "AnmManager.h"
#include "Thread.h"
#include "UpdateFunc.h"
#include "types.h"

// Loads the shared resources (sig.anm, text.anm, the BGM format table) on a
// worker thread while the loading screen animates. Layout from ExpHP's
// th-re-data (zLoadingThread).
struct LoadingThread
{
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    ThreadInf thread;
    u8 unk_28[4];
    AnmVm vm;
    AnmId anm_id;
    AnmLoaded *sig_anm;
    i32 count_630;
    i32 count_634;
    i32 count_638;

    LoadingThread();
    ~LoadingThread();
    int initialize();
    static LoadingThread *create();

    static unsigned __stdcall thread_start(void *arg);
    static int __fastcall on_tick(void *arg);
    static int __fastcall on_draw(void *arg);
};

extern LoadingThread *g_LoadingThread;
