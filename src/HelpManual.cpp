#include <string.h>

#include "FileSystem.h"
#include "GameErrorContext.h"
#include "HelpManual.h"
#include "Supervisor.h"

// GLOBAL: TH16 0x4a6dd8
HelpManual *g_HelpManual;

// FUNCTION: TH16 0x42e6a0
HelpManual::HelpManual()
{
    memset(this, 0, sizeof(HelpManual));
    flags |= 2;
    g_HelpManual = this;
}

// Runs on the loading thread.
// FUNCTION: TH16 0x42e760
void help_manual_load_anm()
{
    g_HelpManual->help_anm = AnmManager::preload_anm(0x13, "help.anm");
    if (g_HelpManual->help_anm == NULL)
    {
        // "Screen layout data not found. The data is corrupted."
        g_GameErrorContext.log("\x89\xe6\x96\xca\x8d\\\x90\xac\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82"
                               "\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82"
                               "\xdc\x82\xb7\r\n");
        return;
    }
    g_Supervisor.thread.should_run = FALSE;
    g_Supervisor.thread.stop_requested = TRUE;
    g_HelpManual->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    g_HelpManual->on_draw->flags |= UPDATE_FUNC_ACTIVE;
}

// Runs on the loading thread.
// FUNCTION: TH16 0x42e7c0
void help_manual_read_file()
{
    g_HelpManual->file_data = file_read_all(g_HelpManual->file_name, &g_HelpManual->file_size, 0);
    g_HelpManual->substate = 3;
    g_Supervisor.thread.should_run = FALSE;
    g_Supervisor.thread.stop_requested = TRUE;
}

// TODO: the original inlines the second create_func (only that one); ours
// calls both, since create_func is HARNESS_CALLED and so never inlined.
// FUNCTION: TH16 0x42e810
i32 HelpManual::initialize()
{
    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0xb);
    on_tick = f;
    f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x48);
    on_draw = f;
    g_Supervisor.start_thread((ThreadStart)help_manual_load_anm, NULL);
    timer.reset();
    state = 0;
    return 0;
}

// FUNCTION: TH16 0x42e910
HelpManual::~HelpManual()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    AnmManager *anm = g_AnmManager;
    if (anm->loaded_anms[0x13] != NULL)
    {
        anm->loaded_anms[0x13]->release();
        delete anm->loaded_anms[0x13];
        anm->loaded_anms[0x13] = NULL;
    }
    g_HelpManual = NULL;
}

// FUNCTION: TH16 0x42ea30
HelpManual *HelpManual::create()
{
    HelpManual *manual = new HelpManual();
    if (manual->initialize() != 0)
    {
        delete manual;
        return NULL;
    }
    return manual;
}

// FUNCTION: TH16 0x42ea80
void HelpManual::destroy()
{
    if (g_HelpManual != NULL)
    {
        delete g_HelpManual;
    }
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x42eab0
DECOMP_NOINLINE i32 HelpManual::on_tick_body()
{
    return state;
}

// FUNCTION: TH16 0x42ef90
i32 __fastcall HelpManual::on_tick_callback(HelpManual *manual)
{
    return manual->on_tick_body();
}

// FUNCTION: TH16 0x42efa0
i32 __fastcall HelpManual::on_draw_callback(HelpManual *manual)
{
    return 1;
}
