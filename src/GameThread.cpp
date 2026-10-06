#include <string.h>

#include "AnmManager.h"
#include "GameThread.h"

// GLOBAL: TH16 0x4a6dd4
GameThread *g_GameThread;

extern i32 g_unk_4d9d90;
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

// TODO: esi and edi swapped (replay_mode and thread); also pushes 0 for the
// folded argument of start_thread until 0x43c5b0 is compiled with /GL.
// FUNCTION: TH16 0x42d700
HARNESS_CALLED GameThread *GameThread::create(i32 replay_mode)
{
    GameThread *thread = new GameThread();
    g_unk_4d9d90 = 0;
    // Supervisor::d3d is really the device: this is EvictManagedResources.
    ((IDirect3DDevice9 *)g_Supervisor.d3d)->EvictManagedResources();
    thread->flags.flag_2 = 1;
    g_GameThread = thread;
    thread->replay_mode = replay_mode;
    Supervisor::start_thread(thread_start_callback, NULL);
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

// TODO: stores 0x18-0x1f are combined differently (dwords at 0x18 and 0x1c,
// where the original has a word at 0x18, a dword at 0x1a and a word at 0x1e).
// FUNCTION: TH16 0x42e630
void ConfigData::set_defaults()
{
    memset(this, 0, sizeof(ConfigData));
    flags |= 0x100;
    deadzone_x = deadzone_y = 600;
    unk_1c = 0;
    unk_1d = 1;
    version = 0x160002;
    unk_1e = 1;
    unk_1f = 5;
    unk_20 = 0;
    memcpy(pad_mapping, g_pad_mapping, sizeof(pad_mapping));
    unk_21 = 2;
    unk_22 = 100;
    unk_24 = 0;
    unk_25 = 2;
    unk_23 = 80;
    unk_2c = 0x80000000;
    unk_30 = 0x80000000;
}

// Placeholder for 0x42cb60 (not decompiled yet).
DECOMP_NOINLINE void GameThread::thread_start()
{
    unit5_placeholder(g_GameThread);
}

// Placeholder for 0x42d200 (not decompiled yet).
DECOMP_NOINLINE GameThread::~GameThread()
{
    unit5_placeholder(this);
}

// Placeholder for 0x42d7b0 (not decompiled yet).
DECOMP_NOINLINE i32 GameThread::on_tick_body()
{
    return unit5_placeholder(this);
}
