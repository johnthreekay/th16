#include <process.h>
#include <string.h>

#include "AsciiManager.h"
#include "LoadingThread.h"
#include "Supervisor.h"

// GLOBAL: TH16 0x4a6ee4
LoadingThread *g_LoadingThread;

LoadingThread::LoadingThread()
{
    memset(this, 0, sizeof(LoadingThread));
    flags |= 2;
    g_LoadingThread = this;
}

// FUNCTION: TH16 0x43af60
int LoadingThread::initialize()
{
    UpdateFunc *f;

    f = g_UpdateFuncRegistry->create_func([](void *arg) -> int { return ((LoadingThread *)arg)->on_tick(); });
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 3);
    on_tick_func = f;

    f = g_UpdateFuncRegistry->create_func([](void *arg) -> int { return ((LoadingThread *)arg)->on_draw(); });
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x44);
    on_draw_func = f;

    thread.restart(thread_start, this);
    return 0;
}

// FUNCTION: TH16 0x43b1f0
LoadingThread *LoadingThread::create()
{
    LoadingThread *t = new LoadingThread();
    if (t->initialize() != 0)
    {
        delete t;
        return NULL;
    }
    return t;
}

// FUNCTION: TH16 0x43b290
int LoadingThread::on_tick()
{
    if (flags & 2)
    {
        g_Supervisor.setup_special_anms();
        g_unk_4d9d90 = 1;
        g_AsciiManager->on_tick_func->flags |= UPDATE_FUNC_ACTIVE;
        g_AsciiManager->on_draw_func_1->flags |= UPDATE_FUNC_ACTIVE;
        g_AsciiManager->on_draw_func_2->flags |= UPDATE_FUNC_ACTIVE;
        g_AsciiManager->on_draw_func_3->flags |= UPDATE_FUNC_ACTIVE;
        g_Supervisor.flags &= ~0x2000;
        g_Supervisor.gamemode_to_switch_to = 4;
        flags &= ~2;
    }
    return 1;
}
