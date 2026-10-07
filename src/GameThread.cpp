#include <string.h>

#include "AnmManager.h"
#include "GameThread.h"
#include "Globals.h"
#include "Scorefile.h"

// GLOBAL: TH16 0x4a6dd4
GameThread *g_GameThread;

// GLOBAL: TH16 0x4a5c00
double g_play_time_runtime;

double LTCG_VECTORCALL get_runtime();

i32 unit5_placeholder(void *object);

// FUNCTION: TH16 0x42d1f0
void GameThread::thread_start_callback()
{
    thread_start();
}

GameThread::GameThread()
{
    unk_94 = 0;
    memset(this, 0, sizeof(GameThread));
}

// TODO: esi and edi swapped (replay_mode and thread).
// FUNCTION: TH16 0x42d700
HARNESS_CALLED GameThread *GameThread::create(i32 replay_mode)
{
    GameThread *thread = new GameThread();
    g_unk_4d9d90 = 0;
    g_Supervisor.d3d_device->EvictManagedResources();
    thread->flags.paused = 1;
    g_GameThread = thread;
    thread->replay_mode = replay_mode;
    g_Supervisor.start_thread((ThreadStart)thread_start_callback, NULL);
    return thread;
}

// FUNCTION: TH16 0x42d780
void GameThread::destroy()
{
    GameThread *thread = g_GameThread;
    g_unk_4d9d90 = 0;
    if (thread != NULL)
    {
        delete thread;
    }
}

// FUNCTION: TH16 0x42db70
i32 __fastcall GameThread::on_tick_callback(GameThread *thread)
{
    return thread->on_tick_body();
}

// FUNCTION: TH16 0x42db80
i32 __fastcall GameThread::on_draw_callback(GameThread *thread)
{
    AnmManager *anm = g_AnmManager;
    anm->unk_c4 = 0;
    anm->unk_c8 = 0;
    anm->unk_c0 = 0;
    anm->unk_cc = 0;
    return 1;
}

// The store order decides how MSVC combines the byte and word stores: the
// original has a word at 0x18, a dword at 0x1a and a word at 0x1e.
// FUNCTION: TH16 0x42e630
void ConfigData::set_defaults()
{
    set_defaults_inline();
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x42cb60
DECOMP_NOINLINE void GameThread::thread_start()
{
    unit5_placeholder(g_GameThread);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x42d200
DECOMP_NOINLINE GameThread::~GameThread()
{
    unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x42d7b0
DECOMP_NOINLINE i32 GameThread::on_tick_body()
{
    return unit5_placeholder(this);
}

// FUNCTION: TH16 0x418420
void GameThread::enable_update_funcs()
{
    if (on_tick != NULL)
    {
        on_tick->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (on_draw != NULL)
    {
        on_draw->flags |= UPDATE_FUNC_ACTIVE;
    }
}

// TODO: register allocation: the original keeps the converted time in
// eax:edx and computes the index in ecx, saving only esi/edi.
// FUNCTION: TH16 0x42dbc0
void GameThread::update_play_time()
{
    if (g_GameThread->replay_mode != 0)
    {
        return;
    }
    if (g_Supervisor.gamemode_to_switch_to == -1 || g_Supervisor.gamemode_to_switch_to == 3)
    {
        return;
    }
    double elapsed = get_runtime() - g_play_time_runtime;
    if (elapsed >= 0.0)
    {
        Scorefile *scorefile = g_Scorefile;
        __int64 time = (__int64)(elapsed * 100.0);
        scorefile->play_time += time;
        scorefile->characters[g_Globals.subshot + g_Globals.character].play_time += time;
    }
    g_play_time_runtime = get_runtime();
}
