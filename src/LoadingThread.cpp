#include <process.h>
#include <string.h>

#include "AsciiManager.h"
#include "FileSystem.h"
#include "GameErrorContext.h"
#include "LoadingThread.h"
#include "Scorefile.h"
#include "SoundManager.h"
#include "Supervisor.h"

// GLOBAL: TH16 0x4a6ee4
LoadingThread *g_LoadingThread;

LoadingThread::LoadingThread()
{
    memset(this, 0, sizeof(LoadingThread));
    flags |= 2;
    g_LoadingThread = this;
}

i32 load_shared_anms();

// sig.anm goes in ANM slot 1 and text.anm in slot 0. thbgm.dat is opened
// here unless config flag 0x10 is set (then only its name is kept).
// FUNCTION: TH16 0x43adc0
int LoadingThread::thread_start(void *arg)
{
    LoadingThread *t = g_LoadingThread;
    t->sig_anm = AnmManager::preload_anm(1, "sig.anm");
    if (t->sig_anm != NULL)
    {
        t->on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
        t->logo_step = 1;
        AsciiInf *ascii = new AsciiInf();
        if (ascii->initialize() != 0)
        {
            delete ascii;
            ascii = NULL;
        }
        if (ascii == NULL)
        {
            // "error : Failed to initialize the text"
            g_GameErrorContext.log("error : \x95\xb6\x8e\x9a\x82\xcc\x8f\x89\x8a\xfa\x89\xbb\x82\xc9\x8e\xb8\x94s\x82\xb5\x82\xdc\x82\xb5\x82\xbd\r\n");
        }
        else
        {
            t->now_loading_step = 1;
            g_Supervisor.text_anm = AnmManager::preload_anm(0, "text.anm");
            if (g_Supervisor.text_anm != NULL)
            {
                g_SoundManager.bgm_format = (ThBgmFormat *)file_read_all("../../bgm/thbgm.fmt", NULL, 0);
                if (g_SoundManager.bgm_format == NULL)
                {
                    // "error : Failed to initialize the BGM"
                    g_GameErrorContext.log("error : BGM \x82\xcc\x8f\x89\x8a\xfa\x89\xbb\x82\xc9\x8e\xb8\x94s\x82\xb5\x82\xdc\x82\xb5\x82\xbd\r\n");
                }
                g_SoundManager.reset();
                if (file_exists("thbgm.dat"))
                {
                    if (!(g_Supervisor.config.flags_2c & 0x10))
                    {
                        g_SoundManager.open_bgm("thbgm.dat");
                    }
                    else
                    {
                        strcpy(g_SoundManager.bgm_dat_name, "thbgm.dat");
                    }
                }
                load_shared_anms();
                t->on_tick_func->flags |= UPDATE_FUNC_ACTIVE;
                g_Scorefile = new Scorefile;
                return 0;
            }
        }
    }
    g_Supervisor.gamemode_to_switch_to = 3;
    t->on_tick_func->flags |= UPDATE_FUNC_ACTIVE;
    return 0;
}

// Registers the update functions and starts the thread.
// FUNCTION: TH16 0x43af60
int LoadingThread::initialize()
{
    UpdateFunc *f;

    f = g_UpdateFuncRegistry->create_func(on_tick_thunk);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 3);
    on_tick_func = f;

    f = g_UpdateFuncRegistry->create_func(on_draw_thunk);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x44);
    on_draw_func = f;

    thread.restart((ThreadStart)thread_start, this);
    return 0;
}

i32 unload_shared_anms();

// Unloads what thread_start loaded (sig.anm, the ASCII manager, text.anm)
// and saves and frees the score file.
// FUNCTION: TH16 0x43afe0
LoadingThread::~LoadingThread()
{
    thread.join_if_running();
    g_UpdateFuncRegistry->unregister_locked(on_tick_func);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func);
    unload_shared_anms();
    AnmManager *anm = g_AnmManager;
    if (anm->loaded_anms[1] != NULL)
    {
        anm->loaded_anms[1]->release();
        delete anm->loaded_anms[1];
        anm->loaded_anms[1] = NULL;
    }
    g_LoadingThread = NULL;
    if (g_AsciiManager != NULL)
    {
        delete g_AsciiManager;
    }
    anm = g_AnmManager;
    if (anm->loaded_anms[0] != NULL)
    {
        anm->loaded_anms[0]->release();
        delete anm->loaded_anms[0];
        anm->loaded_anms[0] = NULL;
    }
    scorefile_save_449a00();
    if (g_Scorefile != NULL)
    {
        delete g_Scorefile;
    }
    g_Scorefile = NULL;
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

// Once the thread has loaded everything (on_tick only becomes active
// then), sets up the special ANM VMs, starts the ASCII manager and goes to
// the title (game mode 4).
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
    return UPDATE_FUNC_CONTINUE;
}

// Shows the sig.anm logo and then the "now loading" text.
// FUNCTION: TH16 0x43b300
int LoadingThread::on_draw()
{
    if (logo_step == 1)
    {
        anm_id = sig_anm->create_effect(0, -1, NULL);
        logo_step++;
    }
    if (now_loading_step == 1)
    {
        AsciiInf *ascii = g_AsciiManager;
        D3DXVECTOR3 pos(960.0f, 784.0f, 0.0f);
        if (ascii->now_loading_id.id == 0)
        {
            ascii->now_loading_id = ascii->ascii_anm->create_vm(0x11, &pos, 0.0f, -1, 0);
        }
        now_loading_step++;
    }
    draw_count++;
    return UPDATE_FUNC_CONTINUE;
}

// The original's callback is a jmp to the member function, most likely the
// fastcall invoker of a capture-less lambda; a static thunk compiles the same.
// FUNCTION: TH16 0x43b3b0
int __fastcall LoadingThread::on_tick_thunk(void *arg)
{
    return ((LoadingThread *)arg)->on_tick();
}

// The original's callback is a jmp to the member function, most likely the
// fastcall invoker of a capture-less lambda; a static thunk compiles the same.
// FUNCTION: TH16 0x43b3c0
int __fastcall LoadingThread::on_draw_thunk(void *arg)
{
    return ((LoadingThread *)arg)->on_draw();
}
