// Stand-in callers and readers for unit 6 (Supervisor, Player, menus,
// replays). Link-time code generation drops stores to globals nothing reads,
// so globals whose readers are not decompiled yet are read here.
#include "../Supervisor.h"
#include "../UpdateFunc.h"
#include "../Globals.h"
#include "../LoadingThread.h"
#include "../PauseMenu.h"
#include "../PopupManager.h"

int harness_unit6_read_globals()
{
    return g_unk_4a6ef0 + g_arcade_width + g_arcade_height + g_game_2d_origin_x + g_game_2d_origin_y +
           (int)g_PauseMenu + (int)g_PopupManager + (int)g_LoadingThread + g_frame_pacing.mode;
}

static unsigned __stdcall harness_thread_proc(void *arg)
{
    return 0;
}

static unsigned __stdcall harness_thread_proc_2(void *arg)
{
    return 1;
}

// Like GameThread::operator new and HelpManual (0x42d76e, 0x42e8b1): every
// caller passes NULL as the thread argument, which LTCG folds.
void harness_start_thread()
{
    g_Supervisor.start_thread(harness_thread_proc, NULL);
    g_Supervisor.start_thread(harness_thread_proc_2, NULL);
}

// Like GuiMsgVm::run and the menus.
void harness_fade_out_bgm(f32 seconds)
{
    g_Supervisor.fade_out_bgm(seconds);
    g_Supervisor.fade_out_bgm(1.0f);
}

// The registry is called with many different priorities in the original.
void harness_register(UpdateFunc *f, int priority)
{
    g_UpdateFuncRegistry->register_on_tick(f, priority);
    g_UpdateFuncRegistry->register_on_draw(f, priority);
}

// Like ItemManager::on_tick_1d__body, PauseMenu and the season gauge.
f32 harness_globals(i32 amount)
{
    g_Globals.add_to_score(amount);
    g_Globals.add_to_score(10);
    g_Globals.add_power(amount);
    return get_season_gauge_fill_ratio();
}


#include "../ReplayManager.h"
#include "../Scorefile.h"

// Like GameThread::thread_start, PauseMenu and MainMenu.
void harness_replay(i32 mode, const char *filename)
{
    ReplayManager::destroy(ReplayManager::create(mode));
    ReplayManager::destroy(ReplayManager::create_from_file(filename));
    ReplayManager::destroy(ReplayManager::create_from_file(g_current_replay_filename));
    g_Globals.set_game_mode(mode);
    g_Globals.set_game_mode(0);
    g_ReplayManager->new_chunk(mode);
    g_ReplayManager->free_chunks(mode);
}

u32 harness_checksum(ScorefileSection *section, i32 size)
{
    return section->compute_checksum(0x5318) + section->compute_checksum(size);
}
