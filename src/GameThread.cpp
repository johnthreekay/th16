#include <string.h>

#include "AnmManager.h"
#include "AsciiManager.h"
#include "Bomb.h"
#include "FpsCounter.h"
#include "BulletManager.h"
#include "EffectManager.h"
#include "EnemyManager.h"
#include "GameThread.h"
#include "Globals.h"
#include "Gui.h"
#include "Input.h"
#include "Item.h"
#include "Laser.h"
#include "PauseMenu.h"
#include "Player.h"
#include "PopupManager.h"
#include "ReplayManager.h"
#include "Scorefile.h"
#include "ScreenEffect.h"
#include "SoundManager.h"
#include "Stage.h"
#include "Spellcard.h"
#include "StageData.h"

// GLOBAL: TH16 0x4a6dd4
GameThread *g_GameThread;

// GLOBAL: TH16 0x4a5c00
double g_play_time_runtime;

double LTCG_VECTORCALL get_runtime();

i32 unit5_placeholder(void *object);

extern i32 g_unk_4c0f40;
extern i32 g_continues_remaining;
extern i32 g_unk_4a5bf8;

// g_Globals' flag word at 0x45c as a whole (flags_lo_45c, game_mode, ...).
#define GLOBALS_FLAGS_45C (*(u32 *)((u8 *)&g_Globals + 0x45c))

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

// FUNCTION: TH16 0x42d700
HARNESS_CALLED GameThread *GameThread::create(i32 replay_mode)
{
    GameThread *thread = new GameThread();
    g_unk_4d9d90 = 0;
    g_Supervisor.d3d_device->EvictManagedResources();
    g_GameThread = thread;
    thread->replay_mode = replay_mode;
    thread->flags.paused = 1;
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

// Credits per difficulty.
// GLOBAL: TH16 0x492278
static const i32 g_continues_per_difficulty[6] = {5, 5, 5, 5, 0, 0};

// The maximum point item value per difficulty, divided by 100.
// GLOBAL: TH16 0x492290
static const i32 g_max_piv_per_difficulty[6] = {500000, 500000, 500000, 500000, 500000, 1000000};

// The starting point item value per difficulty, divided by 100.
// GLOBAL: TH16 0x4922a8
static const i32 g_initial_piv_per_difficulty[6] = {10000, 10000, 10000, 10000, 10000, 100000};

// Takes on_tick_callback's address for thread_start. In the original the
// callback is not entered with known 8-byte alignment (it jumps to
// on_tick_body, which realigns itself), although thread_start realigns;
// taking the address in an inline helper node keeps LTCG from handing the
// alignment down to it (on_tick_body's callees would get padded frames).
static inline UpdateFuncCallback game_thread_on_tick_callback()
{
    return (UpdateFuncCallback)GameThread::on_tick_callback;
}

// The game thread: waits for the loading screen, sets up a new game (or
// the next stage) and creates the game objects. 0 on success; -1 (with the
// thread flagged as failed) if something could not be created.
// It realigns its frame (and esp, -8) for the `zero` double local below,
// which hands known alignment to the managers' create functions (padded
// frames in Stage::create, Gui::initialize, LaserManager::initialize, ...).
// FUNCTION: TH16 0x42cb60
i32 GameThread::thread_start()
{
    GameThread *thread = g_GameThread;
    *(u32 *)&thread->flags |= 4;
    __asm finit;
    while (g_AnmManager->screen_copies[0].anm_slot >= 0)
    {
        if (g_Supervisor.flags & 0x180)
        {
            goto fail;
        }
        Sleep(1);
    }
    if (g_Supervisor.unk_700 == 0)
    {
        Sleep(60);
    }
    g_Supervisor.vm_1bc->interrupt(2);
    g_Supervisor.vm_1bc->run();
    g_game_speed = 1.0f;
    *(u32 *)&g_GameThread->flags &= ~0x4000;
    g_Globals.time_in_stage = 0;
    g_Globals.time_in_chapter = 0;
    if (g_GameThread->replay_mode == 0)
    {
        g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
            .practices[g_Globals.difficulty][g_Globals.stage_num - 1]
            .unlocked = 1;
    }
    if (g_Supervisor.unk_700 != 0)
    {
        if (g_Globals.stage_num == 7)
        {
            if (g_Globals.difficulty < 4)
            {
                g_Globals.difficulty = 4;
            }
        }
        if (g_Globals.game_mode == 2)
        {
            g_Globals.hiscore = g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
                                    .spells[g_Globals.spell_id]
                                    .practice_score;
            g_Globals.hiscore_continues = 0;
        }
        else if (g_Globals.game_mode != 0)
        {
            g_Globals.hiscore = g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
                                    .practices[g_Globals.difficulty][g_Globals.stage_num - 1]
                                    .score;
            g_Globals.hiscore_continues = 0;
        }
        else
        {
            ScorefileScore *best =
                &g_Scorefile->characters[g_Globals.subshot + g_Globals.character].scores[g_Globals.difficulty][0];
            g_Globals.hiscore = best->score;
            g_Globals.hiscore_continues = best->continues;
        }
        if (!(g_Globals.flags_lo_45c & 8))
        {
            g_Globals.continues_used = 0;
        }
        g_Globals.graze = 0;
        g_Globals.score = 0;
        g_Globals.reset_for_new_game();
        g_Globals.initial_piv = g_initial_piv_per_difficulty[g_Globals.difficulty] * 100;
        g_Globals.max_piv = g_max_piv_per_difficulty[g_Globals.difficulty] * 100;
        g_Globals.piv = (f32)g_Globals.initial_piv / 100.0f * 100.0f;
        g_continues_remaining = g_continues_per_difficulty[g_Globals.difficulty];
        if (g_Globals.game_mode == 2)
        {
            g_Globals.lives = 0;
            g_Globals.bombs = 0;
            if (g_Gui != NULL)
            {
                g_Gui->update_bombs(0, g_Globals.bomb_fragments);
            }
        }
        else if (g_Globals.game_mode == 0)
        {
            g_Globals.lives = 2;
        }
        else if (g_unk_4a5bf8 == 0)
        {
            g_Globals.lives = 9;
        }
        else
        {
            g_Globals.lives = g_unk_4a5bf8 - 1;
        }
        if (Player::create() == NULL)
        {
            goto fail;
        }
        if (g_Globals.game_mode == 2)
        {
            i32 power = g_Globals.power_per_level * 4;
            if (power > g_Globals.max_power)
            {
                g_Globals.power = g_Globals.max_power;
            }
            else
            {
                g_Globals.power = power < g_Globals.power_per_level ? g_Globals.power_per_level : power;
            }
            g_Globals.season_power = 390;
            if (g_Globals.max_season_power < 390)
            {
                g_Globals.season_power = g_Globals.max_season_power;
            }
        }
        else if (g_Globals.stage_num <= 1)
        {
            g_Globals.power = g_Globals.power_per_level > g_Globals.max_power ? g_Globals.max_power
                                                                               : g_Globals.power_per_level;
            g_Globals.season_power = 0;
            if (g_Globals.max_season_power < 0)
            {
                g_Globals.season_power = g_Globals.max_season_power;
            }
        }
        else if (g_Globals.stage_num == 7)
        {
            i32 power = g_Globals.power_per_level * 4;
            if (power > g_Globals.max_power)
            {
                g_Globals.power = g_Globals.max_power;
            }
            else
            {
                g_Globals.power = power < g_Globals.power_per_level ? g_Globals.power_per_level : power;
            }
            g_Globals.season_power = 0;
            if (g_Globals.max_season_power < 0)
            {
                g_Globals.season_power = g_Globals.max_season_power;
            }
        }
        else
        {
            i32 power = g_Globals.power_per_level * 4;
            if (power > g_Globals.max_power)
            {
                g_Globals.power = g_Globals.max_power;
            }
            else
            {
                g_Globals.power = power < g_Globals.power_per_level ? g_Globals.power_per_level : power;
            }
            g_Globals.season_power = 0;
            if (g_Globals.max_season_power < 0)
            {
                g_Globals.season_power = g_Globals.max_season_power;
            }
        }
        g_Player->inner.repopulate_options();
        GLOBALS_FLAGS_45C &= ~4;
        if (thread->replay_mode == 0)
        {
            i32 *play_count = &g_Scorefile->characters[g_Globals.subshot + g_Globals.character].play_count;
            if (*play_count < 9999999)
            {
                (*play_count)++;
            }
        }
    }
    else if (g_Globals.score > (u32)g_Globals.hiscore)
    {
        g_Globals.hiscore = g_Globals.score;
    }

    {
        UpdateFunc *f = g_UpdateFuncRegistry->create_func(game_thread_on_tick_callback());
        f->flags &= ~UPDATE_FUNC_ACTIVE;
        f->arg = thread;
        g_UpdateFuncRegistry->register_on_tick(f, 0xf);
        thread->on_tick = f;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_callback);
        f->flags &= ~UPDATE_FUNC_ACTIVE;
        f->arg = thread;
        g_UpdateFuncRegistry->register_on_draw(f, 2);
        thread->on_draw = f;
    }
    memcpy(&thread->config, (u8 *)&g_Supervisor.config + 4, sizeof(ConfigData));
    thread->unk_20 = g_stage_data->stage_num;
    if (!(g_Globals.flags_lo_45c & 2))
    {
        if (ReplayManager::create(thread->replay_mode) == NULL)
        {
            goto fail;
        }
        if (Stage::create(g_stage_data->std_filename) == NULL)
        {
            goto fail;
        }
        if (Gui::create() == NULL)
        {
            goto fail;
        }
        if (BulletManager::create() == NULL)
        {
            goto fail;
        }
        if (ItemManager::create() == NULL)
        {
            goto fail;
        }
        if (LaserManager::create() == NULL)
        {
            goto fail;
        }
        if (PauseMenu::create() == NULL)
        {
            goto fail;
        }
        if (PopupManager::create() == NULL)
        {
            goto fail;
        }
    }
    else
    {
        g_ReplayManager->start_stage();
        g_Gui->load_stage_files();
        if (Stage::create(g_stage_data->std_filename) == NULL)
        {
            goto fail;
        }
    }
    if (!(g_Globals.flags_lo_45c & 9))
    {
        if (EnemyManager::create(g_stage_data->ecl_filename) == NULL)
        {
            goto fail;
        }
    }
    else
    {
        g_EnemyManager->reset_for_stage(0);
    }
    if (BombInf::create() == NULL)
    {
        goto fail;
    }
    if (Spellcard::create() == NULL)
    {
        goto fail;
    }
    if (!(GLOBALS_FLAGS_45C & 0x40))
    {
        // The original tests the flag byte in memory and loads the word
        // again for game_mode; a plain read shares one load.
        if (((volatile Globals *)&g_Globals)->game_mode != 2)
        {
            g_Supervisor.stop_bgm();
        }
        g_Supervisor.play_bgm_wav(0, g_stage_data->music_names[0]);
        g_Supervisor.play_bgm_wav(1, g_stage_data->music_names[1]);
    }
    double zero = 0.0;
    g_FpsCounter->total_actual = zero;
    g_FpsCounter->total_expected = zero;
    thread->time_in_stage.set_value(0);
    (&g_Globals.unk_204)[g_Globals.stage_num] = 0;
    g_Globals.unk_224 = 0;
    while (g_SoundManager.bgm_commands[0].command != 0)
    {
        Sleep(16);
    }
    thread->unk_90 = 60;
    g_Supervisor.sub_43c630();
    *(u32 *)&thread->flags &= ~4;
    GLOBALS_FLAGS_45C &= ~0xb;
    g_Supervisor.thread.should_run = FALSE;
    g_Supervisor.thread.stop_requested = TRUE;
    g_unk_4c0f40 = 0;
    g_draw_hook_4a6eec = NULL;
    g_draw_hook_4a6ee8 = NULL;
    anm_vm_interrupt_2(g_Supervisor.vm_1bc);
    anm_vm_interrupt_2(g_Supervisor.vm_1c0);
    anm_vm_interrupt_2(g_Supervisor.vm_1c4);
    anm_vm_interrupt_2(g_Supervisor.vm_1c8);
    if (g_GameThread->replay_mode == 0)
    {
        g_play_time_runtime = get_runtime();
    }
    thread->enable_update_funcs();
    return 0;

fail:
    *(u32 *)&thread->flags |= 8;
    g_Supervisor.sub_43c6a0();
    g_Supervisor.thread.should_run = FALSE;
    g_Supervisor.thread.stop_requested = TRUE;
    if (thread->on_tick != NULL)
    {
        thread->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (thread->on_draw != NULL)
    {
        thread->on_draw->flags |= UPDATE_FUNC_ACTIVE;
    }
    return -1;
}



// Ends a game: saves the score file, shows "now loading" for what comes
// next, and deletes the game objects (keeping the stage, GUI and player
// across a stage transition, flag 2) and the update functions.
// FUNCTION: TH16 0x42d200
DECOMP_NOINLINE GameThread::~GameThread()
{
    scorefile_save_449a00();
    GLOBALS_FLAGS_45C &= ~3;
    g_game_speed = 1.0f;
    g_draw_hook_4a6eec = NULL;
    g_draw_hook_4a6ee8 = NULL;
    if (g_Supervisor.gamemode_to_switch_to == 10 || g_Supervisor.gamemode_to_switch_to == 11)
    {
        g_AsciiManager->show_now_loading_inline(480.0f, 392.0f);
        if (g_Globals.weird_stage_num == g_Globals.stage_num)
        {
            GLOBALS_FLAGS_45C |= 1;
        }
    }
    else if (g_Supervisor.gamemode_to_switch_to == 12)
    {
        g_AsciiManager->show_now_loading(480.0f, 392.0f);
        GLOBALS_FLAGS_45C |= 2;
    }
    else if (g_Supervisor.gamemode_to_switch_to == 4)
    {
        g_AsciiManager->show_now_loading(480.0f, 392.0f);
    }
    else if (g_Supervisor.gamemode_to_switch_to == 16)
    {
        g_AsciiManager->show_now_loading(480.0f, 392.0f);
    }
    else if (g_Supervisor.gamemode_to_switch_to == 14)
    {
        g_AsciiManager->show_now_loading(480.0f, 392.0f);
        if (g_Globals.weird_stage_num != g_Globals.stage_num)
        {
            g_Globals.continues_used++;
            if (g_Globals.continues_used >= 10)
            {
                g_Globals.continues_used = 9;
            }
            GLOBALS_FLAGS_45C |= 8;
        }
        GLOBALS_FLAGS_45C |= 1;
    }
    if (!(GLOBALS_FLAGS_45C & 2))
    {
        if (g_Supervisor.gamemode_to_switch_to != 15 && g_Supervisor.gamemode_to_switch_to != 16)
        {
            delete g_ReplayManager;
        }
        delete g_Stage;
        delete g_Stage2;
        delete g_PauseMenu;
        delete g_Gui;
        delete g_Player;
        delete g_BulletManager;
        delete g_ItemManager;
        delete g_LaserManager;
        delete g_PopupManager;
    }
    else
    {
        g_Gui->release_msg();
        delete g_Stage2;
        g_Stage2 = g_Stage;
        PauseMenu *pause = g_PauseMenu;
        if (pause->on_tick_func != NULL)
        {
            pause->on_tick_func->flags &= ~UPDATE_FUNC_ACTIVE;
        }
        if (pause->on_draw_func != NULL)
        {
            pause->on_draw_func->flags &= ~UPDATE_FUNC_ACTIVE;
        }
        BulletManager *bullets = g_BulletManager;
        if (bullets->on_tick != NULL)
        {
            bullets->on_tick->flags &= ~UPDATE_FUNC_ACTIVE;
        }
        if (bullets->on_draw != NULL)
        {
            bullets->on_draw->flags &= ~UPDATE_FUNC_ACTIVE;
        }
        ReplayManager *replay = g_ReplayManager;
        if (replay->on_tick_22_func != NULL)
        {
            replay->on_tick_22_func->flags &= ~UPDATE_FUNC_ACTIVE;
        }
        if (replay->on_draw_func != NULL)
        {
            replay->on_draw_func->flags &= ~UPDATE_FUNC_ACTIVE;
        }
        g_ItemManager->destroy_all();
    }
    if (!(GLOBALS_FLAGS_45C & 9))
    {
        delete g_EnemyManager;
    }
    else
    {
        g_EnemyManager->destroy_all();
    }
    g_AnmManager->disable_vms_from_anm_file(g_EffectManager->effect_anm);
    g_AnmManager->disable_vms_from_anm_file(g_EffectManager->bullet_anm);
    BombInf::destroy_all();
    delete g_Spellcard;
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    g_GameThread = NULL;
    if (!(g_Globals.game_mode == 2 && (GLOBALS_FLAGS_45C & 1)) && !(GLOBALS_FLAGS_45C & 0x42))
    {
        if (g_Supervisor.config.flags_2c & 0x10)
        {
            g_SoundManager.modify_bgm(4, 0, "dummy");
        }
        else
        {
            g_SoundManager.modify_bgm(3, 0, "dummy");
        }
        g_SoundManager.bgm_name[0] = '\0';
    }
    SoundManager::pause_sounds();
    g_unk_4c0f40 = 1;
    g_Supervisor.background_color = (GLOBALS_FLAGS_45C & 1) ? 0 : 0xff000000;
}

// The original seeks inline in on_tick_body. Written out there, the double
// math makes LTCG realign on_tick_body early enough to hand the alignment
// down to sub_42dc50's callees (Stage::start_std_vms, 0x40add0, loses its
// shrink-wrapped edi); in a plain inline helper the double belongs to the
// helper's node, and on_tick_body realigns late like the original.
static inline void seek_bgm_to_stage_time()
{
    ((CStreamingSound *)g_SoundManager.bgm_stream)->seek(g_Globals.time_in_stage / 60.0);
}

// One frame of a game: the ending fade, the stage restart and intro
// timing, the demo's end, the music restart after a pause and the timers.
// TODO: the original keeps the return 3 epilogue at the top and a second null test around the inlined delete of g_Stage2.
// FUNCTION: TH16 0x42d7b0
HARNESS_CALLED i32 GameThread::on_tick_body()
{
    if (*(u32 *)&flags & 0x4000)
    {
        if (*(u32 *)&flags & 0x10)
        {
            return 3;
        }
        fade_timer++;
        if (fade_timer == 180)
        {
            ScreenEffect::create_inline(5, 200, 0, 0, 0, 0x54);
        }
        if (fade_timer >= 380)
        {
            if (replay_mode != 0)
            {
                replay_ended_43f240();
            }
            else if (g_Globals.difficulty != 4)
            {
                g_Supervisor.gamemode_to_switch_to = (g_Supervisor.flags & 0x2000) ? 2 : 15;
            }
            else
            {
                g_Supervisor.gamemode_to_switch_to = (g_Supervisor.flags & 0x2000) ? 2 : 16;
            }
        }
    }
    if ((g_Gui->flags_1ac & 0x100) && g_Gui->timer_1b0.current < 120 && !flags.flag_4)
    {
        return 1;
    }
    if (time_in_stage.current == 0)
    {
        if (sub_42dc50())
        {
            return 1;
        }
    }
    else if (time_in_stage.current == 30)
    {
        sub_42dee0();
    }
    if (g_Stage2 != NULL && (g_Stage2->stage_flags & 8))
    {
        delete g_Stage2;
    }
    if (time_in_stage.current == 5)
    {
        g_unk_4d9d90 = 2;
    }
    if (*(u32 *)&flags & 4)
    {
        *(u32 *)&flags |= 0x80;
        return 1;
    }
    if (g_Globals.flags_hi_45c & 1)
    {
        if ((g_hardware_input & 0x80103) || (*(u32 *)&flags & 0x70))
        {
            g_Supervisor.gamemode_to_switch_to = (g_Supervisor.flags & 0x2000) ? 2 : 4;
        }
        if (time_in_stage.current == 0xf00)
        {
            ScreenEffect::create(5, 60, 0, 0, 0, 0x33);
        }
        else if (time_in_stage.current == 0xf3c)
        {
            g_Supervisor.gamemode_to_switch_to = (g_Supervisor.flags & 0x2000) ? 2 : 4;
        }
    }
    Gui::update_score();
    if ((*(u32 *)&flags & 0x10) || (*(u32 *)&flags & 0x20) || (*(u32 *)&flags & 0x40))
    {
        return 3;
    }
    if (*(u32 *)&flags & 0x10000)
    {
        if (unk_8c == 0)
        {
            if (g_Globals.chapter < 0x2b)
            {
                g_Supervisor.stop_bgm();
            }
            AnmManager::interrupt_tree(*(AnmId *)&g_Supervisor.config.unk_0, 1);
        }
        unk_8c++;
        if (unk_8c < unk_90 && unk_8c > 1)
        {
            return 3;
        }
        if (unk_8c >= unk_90)
        {
            if (g_Globals.chapter < 0x2b)
            {
                g_Supervisor.play_bgm(0, g_stage_data->music_ids[0]);
                while (SoundManager::update_sound_thread() != 0)
                {
                }
            }
            if (g_Globals.chapter < 0x2b)
            {
                seek_bgm_to_stage_time();
            }
            *(u32 *)&flags &= ~0x10000;
            unk_8c = 0;
        }
    }
    g_Supervisor.vm_1bc->run();
    g_Supervisor.vm_1c0->run();
    g_Supervisor.vm_1c4->run();
    g_Supervisor.vm_1c8->run();
    g_Globals.time_in_stage++;
    g_Globals.time_in_chapter++;
    if (g_unk_4c0f40 != 0)
    {
        g_unk_4c0f40--;
    }
    time_in_stage++;
    return 1;
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
        // The total's address is taken before the conversion: the original
        // loads g_Scorefile before the __dtoul3 call and keeps it in edi.
        Scorefile *scorefile = g_Scorefile;
        unsigned __int64 *total = &scorefile->play_time;
        __int64 time = (unsigned __int64)(elapsed * 100.0);
        *total += time;
        scorefile->characters[g_Globals.subshot + g_Globals.character].play_time += time;
    }
    g_play_time_runtime = get_runtime();
}

// Resets the stage's game objects and reactivates every manager; the
// stage restart code shares it.
static __forceinline void restart_stage_objects()
{
    BulletManager::destroy_all();
    g_Player->reset();
    g_ItemManager->destroy_all();
    g_EnemyManager->destroy_all();
    g_LaserManager->destroy_all();
    g_Globals.time_in_stage = 0;
    g_Globals.time_in_chapter = 0;
    g_ReplayManager->begin_stage();
    EnemyCreateParams params;
    memset(&params, 0, sizeof(params));
    g_EnemyManager->allocate_new_enemy("main", &params, 0);
    Gui::sub_426d70();
    PauseMenu *pause = g_PauseMenu;
    if (pause->on_tick_func != NULL)
    {
        pause->on_tick_func->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (pause->on_draw_func != NULL)
    {
        pause->on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
    }
    Player *player = g_Player;
    player->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    player->on_draw->flags |= UPDATE_FUNC_ACTIVE;
    player->inner.repopulate_options();
    BulletManager *bullets = g_BulletManager;
    if (bullets->on_tick != NULL)
    {
        bullets->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (bullets->on_draw != NULL)
    {
        bullets->on_draw->flags |= UPDATE_FUNC_ACTIVE;
    }
    EnemyManager *enemies = g_EnemyManager;
    if (enemies->on_tick != NULL)
    {
        enemies->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (enemies->on_draw != NULL)
    {
        enemies->on_draw->flags |= UPDATE_FUNC_ACTIVE;
    }
    ItemManager *items = g_ItemManager;
    if (items->on_tick != NULL)
    {
        items->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (items->on_draw_1 != NULL)
    {
        items->on_draw_1->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (items->on_draw_2 != NULL)
    {
        items->on_draw_2->flags |= UPDATE_FUNC_ACTIVE;
    }
    LaserManager *lasers = g_LaserManager;
    if (lasers->on_tick != NULL)
    {
        lasers->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (lasers->on_draw != NULL)
    {
        lasers->on_draw->flags |= UPDATE_FUNC_ACTIVE;
    }
    EffectManager *effects = g_EffectManager;
    effects->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    effects->on_draw->flags |= UPDATE_FUNC_ACTIVE;
    BombInf *bomb = g_MainBomb;
    if (bomb->on_tick_func != NULL)
    {
        bomb->on_tick_func->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (bomb->on_draw_func != NULL)
    {
        bomb->on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
    }
    PopupManager *popups = g_PopupManager;
    popups->on_tick_func->flags |= UPDATE_FUNC_ACTIVE;
    popups->on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
    Spellcard *spellcard = g_Spellcard;
    if (spellcard->on_tick != NULL)
    {
        spellcard->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (spellcard->on_draw != NULL)
    {
        spellcard->on_draw->flags |= UPDATE_FUNC_ACTIVE;
    }
}

// TODO: the original realigns the frame (ebx frame, and esp, -8), has 4
// more bytes of locals, and folds allocate_new_enemy's unused argument
// (push ecx).
// FUNCTION: TH16 0x42dc50
HARNESS_CALLED i32 GameThread::sub_42dc50()
{
    g_Gui->sub_42c1b0();
    if (flags.flag_3)
    {
        g_Supervisor.thread.join_if_running();
        g_Supervisor.gamemode_to_switch_to = (~(g_Supervisor.flags >> 13) & 1) | 2;
        return 1;
    }
    g_Stage->start_std_vms();
    if (g_Stage2 != NULL)
    {
        g_Stage2->start_fade_in();
        g_Stage->start_fade_out();
        flags.flag_11 = 1;
        AnmManager::interrupt_tree(g_Gui->id_c8, 1);
        return 0;
    }
    flags.flag_11 = 0;
    restart_stage_objects();
    if (g_Globals.game_mode != 2 && !(g_Globals.flags_hi_45c & 1))
    {
        g_Supervisor.play_bgm(0, g_stage_data->music_ids[0]);
    }
    AnmManager::interrupt_tree(g_AsciiManager->unk_19244, 1);
    g_AsciiManager->hide_now_loading_inline();
    AnmManager::interrupt_tree(*(AnmId *)&g_Supervisor.config.unk_0, 1);
    return 0;
}

// TODO: the original saves esi in the prologue, has 4 more bytes of locals,
// leaves memset's last argument as allocate_new_enemy's unused one, and
// indexes bgm_unlocked as [esi + eax].
// FUNCTION: TH16 0x42dee0
i32 GameThread::sub_42dee0()
{
    if (flags.flag_11)
    {
        flags.flag_11 = 0;
        restart_stage_objects();
        if (g_Supervisor.config.flags_2c & 0x10)
        {
            g_SoundManager.modify_bgm(BGM_STOP_4, 0, "dummy");
        }
        else
        {
            g_SoundManager.modify_bgm(BGM_STOP, 0, "dummy");
        }
        i32 music = g_stage_data->music_ids[0];
        if (g_Supervisor.config.flags_2c & 0x10)
        {
            g_SoundManager.modify_bgm(BGM_STOP_4, 0, "dummy");
        }
        g_SoundManager.modify_bgm(BGM_PLAY, 0, "dummy");
        g_Scorefile->bgm_unlocked[music] = 1;
        g_AsciiManager->hide_now_loading_inline();
        time_in_stage.reset();
    }
    return 0;
}

// The end of a stage: the clear bonus, then the next stage, the ending (stage
// 6 and the extra stage, with their clear counts) or, in practice, the end
// of the game with the practice records updated. Always 0.
// FUNCTION: TH16 0x42e150
i32 stage_clear_42e150()
{
    GameThread *thread = g_GameThread;
    if (g_Globals.game_mode != 2)
    {
        Gui::show_stage_clear_bonus();
    }
    g_Player->resume_options();
    if (g_MainBomb->in_use != 0)
    {
        g_MainBomb->method_14();
    }
    if (g_Globals.game_mode != 0)
    {
        if (g_GameThread->replay_mode != 0)
        {
            replay_ended_43f240();
            return 0;
        }
        if (g_Globals.game_mode == 2)
        {
            ScorefileSpell *spell =
                &g_Scorefile->characters[g_Globals.subshot + g_Globals.character].spells[g_Globals.spell_id];
            if (spell->practice_score < (i32)(g_Globals.score / 10 * 10))
            {
                spell->practice_score = g_Globals.score / 10 * 10;
            }
        }
        if (g_GameThread->replay_mode == 0 && g_Globals.game_mode != 2)
        {
            g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
                .practices[g_Globals.difficulty][g_Globals.stage_num - 1]
                .cleared = 1;
        }
        game_over_43f500();
        return 0;
    }
    if (g_Globals.stage_num == 6)
    {
        *(u32 *)&thread->flags |= 0x4000;
        thread->fade_timer = 0;
        g_Gui->flags_1ac |= 0x10;
        GameThread::update_play_time();
        i32 bonus = 0;
        switch (g_Globals.difficulty)
        {
        case 0:
            bonus = (g_Globals.lives * 5 + g_Globals.bombs) * 1000000;
            break;
        case 1:
            bonus = (g_Globals.lives * 5 + g_Globals.bombs) * 1000000;
            break;
        case 2:
            bonus = (g_Globals.lives * 5 + g_Globals.bombs) * 1000000;
            break;
        case 3:
            bonus = (g_Globals.lives * 5 + g_Globals.bombs) * 1000000;
            break;
        }
        g_Globals.add_to_score(bonus);
        g_Gui->stage_clear_bonus += bonus;
        if (g_GameThread->replay_mode != 0)
        {
            replay_ended_43f240();
            return 0;
        }
        if (g_Scorefile->characters[g_Globals.subshot + g_Globals.character].play_counts[g_Globals.difficulty] < 99999)
        {
            g_Scorefile->characters[g_Globals.subshot + g_Globals.character].play_counts[g_Globals.difficulty]++;
        }
        if (g_Globals.continues_used == 0 &&
            g_Scorefile->characters[g_Globals.subshot + g_Globals.character].clears[g_Globals.difficulty] < 99999)
        {
            g_Scorefile->characters[g_Globals.subshot + g_Globals.character].clears[g_Globals.difficulty]++;
        }
        return 0;
    }
    if (g_Globals.stage_num == 7)
    {
        g_Gui->flags_1ac |= 0x10;
        i32 bonus = (g_Globals.lives * 5 + g_Globals.bombs) * 1000000;
        g_Globals.add_to_score(bonus);
        g_Gui->stage_clear_bonus += bonus;
        if (g_GameThread->replay_mode != 0)
        {
            replay_ended_43f240();
        }
        else
        {
            i32 *play_count =
                &g_Scorefile->characters[g_Globals.subshot + g_Globals.character].play_counts[g_Globals.difficulty];
            if (*play_count < 99999)
            {
                (*play_count)++;
            }
            if (g_Globals.continues_used == 0 &&
                g_Scorefile->characters[g_Globals.subshot + g_Globals.character].clears[g_Globals.difficulty] < 99999)
            {
                g_Scorefile->characters[g_Globals.subshot + g_Globals.character].clears[g_Globals.difficulty]++;
            }
        }
        GameThread::update_play_time();
        *(u32 *)&thread->flags |= 0x4000;
        thread->fade_timer = 0;
        return 0;
    }
    if (g_GameThread->replay_mode == 0)
    {
        g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
            .practices[g_Globals.difficulty][g_Globals.stage_num - 1]
            .cleared = 1;
    }
    g_Supervisor.gamemode_to_switch_to = 12;
    if (g_Globals.stage_num < 7)
    {
        g_Globals.stage_num++;
    }
    g_stage_data = &g_stage_table[g_Globals.stage_num];
    return 0;
}
