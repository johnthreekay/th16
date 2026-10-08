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
#include "ZunAsm.h"

// GLOBAL: TH16 0x4a6dd4
GameThread *g_GameThread;

// The runtime (get_runtime) when play time was last added to the score
// file.
// GLOBAL: TH16 0x4a5c00
double g_play_time_runtime;

double LTCG_VECTORCALL get_runtime();

// While nonzero, the fade and pulse screen effects end at once. Set when a
// game ends (so its effects do not carry over), counted down by the game's
// frames and cleared when the next game, the title or an ending page
// starts.
// GLOBAL: TH16 0x4c0f40
i32 g_cancel_screen_effects;
// Credits left (the pause menu's continue counter).
extern i32 g_continues_remaining;
// The practice menu's starting lives choice: 0 for the default (9),
// otherwise one more than the lives.
extern i32 g_practice_lives_key;

// g_Globals' flag word at 0x45c as a whole (flags_lo_45c, game_mode,
// flags_hi_45c): GlobalsFlagsLo masks and GLOBALS_WORD_DEMO_PLAY.
#define GLOBALS_FLAGS_45C (*(u32 *)((u8 *)&g_Globals + 0x45c))

// GameThread::flags as one word, for the GameThreadFlagMask masks.
#define GAME_THREAD_FLAG_WORD(thread) (*(u32 *)&(thread)->flags)

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
    g_frame_pacing.mode = FRAME_PACING_IDLE;
    g_Supervisor.d3d_device->EvictManagedResources();
    g_GameThread = thread;
    thread->replay_mode = replay_mode;
    thread->flags.loading = 1;
    g_Supervisor.start_thread((ThreadStart)thread_start_callback, NULL);
    return thread;
}

// FUNCTION: TH16 0x42d780
void GameThread::destroy()
{
    GameThread *thread = g_GameThread;
    g_frame_pacing.mode = FRAME_PACING_IDLE;
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

// Resets four of the ANM manager's per-frame counters.
// FUNCTION: TH16 0x42db80
i32 __fastcall GameThread::on_draw_callback(GameThread *thread)
{
    AnmManager *anm = g_AnmManager;
    anm->unk_c4 = 0;
    anm->stat_render_state_setups = 0;
    anm->stat_scripts_started = 0;
    anm->stat_draw_calls = 0;
    return UPDATE_FUNC_CONTINUE;
}

// The store order decides how MSVC combines the byte and word stores: the
// original has a word at 0x18, a dword at 0x1a and a word at 0x1e.
// FUNCTION: TH16 0x42e630
void ConfigData::set_defaults()
{
    set_defaults_inline();
}

// Credits per difficulty (none for extra and the sixth entry).
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
    GAME_THREAD_FLAG_WORD(thread) |= GAME_THREAD_LOADING;
    ZUN_ASM_FINIT();
    // Wait for the loading screen's screen copy to finish.
    while (g_AnmManager->screen_copies[0].anm_slot >= 0)
    {
        if (g_Supervisor.flags & (SUPERVISOR_QUIT_REQUESTED | SUPERVISOR_FLAG_100))
        {
            goto fail;
        }
        Sleep(1);
    }
    if (g_Supervisor.new_game_started == 0)
    {
        Sleep(60);
    }
    g_Supervisor.arcade_blit_vm_0f->interrupt(2);
    g_Supervisor.arcade_blit_vm_0f->run();
    g_game_speed = 1.0f;
    GAME_THREAD_FLAG_WORD(g_GameThread) &= ~GAME_THREAD_GAME_CLEARED;
    g_Globals.time_in_stage = 0;
    g_Globals.time_in_chapter = 0;
    if (g_GameThread->replay_mode == 0)
    {
        g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
            .practices[g_Globals.difficulty][g_Globals.stage_num - 1]
            .unlocked = 1;
    }
    // A new game (not the next stage): the hiscore to beat, then score,
    // lives, bombs, power and season power for the mode.
    if (g_Supervisor.new_game_started != 0)
    {
        if (g_Globals.stage_num == 7)
        {
            if (g_Globals.difficulty < DIFFICULTY_EXTRA)
            {
                g_Globals.difficulty = DIFFICULTY_EXTRA;
            }
        }
        if (g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE)
        {
            g_Globals.hiscore = g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
                                    .spells[g_Globals.spell_id]
                                    .practice_score;
            g_Globals.hiscore_continues = 0;
        }
        else if (g_Globals.game_mode != GAME_MODE_NORMAL)
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
        if (!(g_Globals.flags_lo_45c & GLOBALS_CONTINUED))
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
        if (g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE)
        {
            g_Globals.lives = 0;
            g_Globals.bombs = 0;
            if (g_Gui != NULL)
            {
                g_Gui->update_bombs(0, g_Globals.bomb_fragments);
            }
        }
        else if (g_Globals.game_mode == GAME_MODE_NORMAL)
        {
            g_Globals.lives = 2;
        }
        else if (g_practice_lives_key == 0)
        {
            g_Globals.lives = 9;
        }
        else
        {
            g_Globals.lives = g_practice_lives_key - 1;
        }
        if (Player::create() == NULL)
        {
            goto fail;
        }
        // Spell practice, the extra stage and practice from stage 2 on
        // start with 4.00 power; stage 1 with 1.00.
        if (g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE)
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
        GLOBALS_FLAGS_45C &= ~GLOBALS_HISCORE_BEATEN;
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
    thread->start_stage_num = g_stage_data->stage_num;
    // The game objects; going on to the next stage keeps them and only
    // loads the new stage's files.
    if (!(g_Globals.flags_lo_45c & GLOBALS_NEXT_STAGE))
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
    if (!(g_Globals.flags_lo_45c & GLOBALS_STAGE_RESTART_MASK))
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
    // Outside the demo, load the stage and boss themes (stopping the old
    // music unless in spell practice).
    if (!(GLOBALS_FLAGS_45C & GLOBALS_WORD_DEMO_PLAY))
    {
        // The original tests the flag byte in memory and loads the word
        // again for game_mode; a plain read shares one load.
        if (((volatile Globals *)&g_Globals)->game_mode != GAME_MODE_SPELL_PRACTICE)
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
    // Wait for the sound thread to take the music commands.
    while (g_SoundManager.bgm_commands[0].command != BGM_NONE)
    {
        Sleep(16);
    }
    thread->music_restart_delay = 60;
    g_Supervisor.end_stage_load_anms();
    GAME_THREAD_FLAG_WORD(thread) &= ~GAME_THREAD_LOADING;
    GLOBALS_FLAGS_45C &= ~(GLOBALS_SAME_STAGE_AGAIN | GLOBALS_NEXT_STAGE | GLOBALS_CONTINUED);
    g_Supervisor.thread.should_run = FALSE;
    g_Supervisor.thread.stop_requested = TRUE;
    g_cancel_screen_effects = 0;
    g_draw_hook_0f = NULL;
    g_draw_hook_1a = NULL;
    anm_vm_interrupt_2(g_Supervisor.arcade_blit_vm_0f);
    anm_vm_interrupt_2(g_Supervisor.arcade_blit_vm_1a);
    anm_vm_interrupt_2(g_Supervisor.arcade_blit_vm_2c);
    anm_vm_interrupt_2(g_Supervisor.arcade_blit_vm_39);
    if (g_GameThread->replay_mode == 0)
    {
        g_play_time_runtime = get_runtime();
    }
    thread->enable_update_funcs();
    return 0;

fail:
    GAME_THREAD_FLAG_WORD(thread) |= GAME_THREAD_FAILED;
    g_Supervisor.abort_stage_load_anms();
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
// next (GAMEMODE_RESTART or GAMEMODE_RESTART_REPLAY retries,
// GAMEMODE_NEXT_STAGE goes on, GAMEMODE_RETRY_STAGE continues), sets the
// GlobalsFlagsLo bits that say what the next GameThread keeps, and
// deletes the game objects (keeping the stage, GUI and player for the next
// stage) and the update functions.
// FUNCTION: TH16 0x42d200
DECOMP_NOINLINE GameThread::~GameThread()
{
    scorefile_save();
    GLOBALS_FLAGS_45C &= ~(GLOBALS_SAME_STAGE_AGAIN | GLOBALS_NEXT_STAGE);
    g_game_speed = 1.0f;
    g_draw_hook_0f = NULL;
    g_draw_hook_1a = NULL;
    if (g_Supervisor.gamemode_to_switch_to == GAMEMODE_RESTART ||
        g_Supervisor.gamemode_to_switch_to == GAMEMODE_RESTART_REPLAY)
    {
        g_AsciiManager->show_now_loading_inline(480.0f, 392.0f);
        if (g_Globals.weird_stage_num == g_Globals.stage_num)
        {
            GLOBALS_FLAGS_45C |= GLOBALS_SAME_STAGE_AGAIN;
        }
    }
    else if (g_Supervisor.gamemode_to_switch_to == GAMEMODE_NEXT_STAGE)
    {
        g_AsciiManager->show_now_loading(480.0f, 392.0f);
        GLOBALS_FLAGS_45C |= GLOBALS_NEXT_STAGE;
    }
    else if (g_Supervisor.gamemode_to_switch_to == GAMEMODE_TITLE)
    {
        g_AsciiManager->show_now_loading(480.0f, 392.0f);
    }
    else if (g_Supervisor.gamemode_to_switch_to == GAMEMODE_TITLE_SCORE_ENTRY)
    {
        g_AsciiManager->show_now_loading(480.0f, 392.0f);
    }
    else if (g_Supervisor.gamemode_to_switch_to == GAMEMODE_RETRY_STAGE)
    {
        g_AsciiManager->show_now_loading(480.0f, 392.0f);
        if (g_Globals.weird_stage_num != g_Globals.stage_num)
        {
            g_Globals.continues_used++;
            if (g_Globals.continues_used >= 10)
            {
                g_Globals.continues_used = 9;
            }
            GLOBALS_FLAGS_45C |= GLOBALS_CONTINUED;
        }
        GLOBALS_FLAGS_45C |= GLOBALS_SAME_STAGE_AGAIN;
    }
    if (!(GLOBALS_FLAGS_45C & GLOBALS_NEXT_STAGE))
    {
        if (g_Supervisor.gamemode_to_switch_to != GAMEMODE_ENDING &&
            g_Supervisor.gamemode_to_switch_to != GAMEMODE_TITLE_SCORE_ENTRY)
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
        if (replay->fast_forward_func != NULL)
        {
            replay->fast_forward_func->flags &= ~UPDATE_FUNC_ACTIVE;
        }
        if (replay->on_draw_func != NULL)
        {
            replay->on_draw_func->flags &= ~UPDATE_FUNC_ACTIVE;
        }
        g_ItemManager->destroy_all();
    }
    if (!(GLOBALS_FLAGS_45C & GLOBALS_STAGE_RESTART_MASK))
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
    // The music stops unless it goes on: a retry in spell practice, the
    // next stage, or the demo.
    if (!(g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE && (GLOBALS_FLAGS_45C & GLOBALS_SAME_STAGE_AGAIN)) &&
        !(GLOBALS_FLAGS_45C & (GLOBALS_WORD_DEMO_PLAY | GLOBALS_NEXT_STAGE)))
    {
        if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
        {
            g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
        }
        else
        {
            g_SoundManager.modify_bgm(BGM_STOP, 0, "dummy");
        }
        g_SoundManager.bgm_name[0] = '\0';
    }
    SoundManager::pause_sounds();
    g_cancel_screen_effects = 1;
    g_Supervisor.background_color = (GLOBALS_FLAGS_45C & GLOBALS_SAME_STAGE_AGAIN) ? 0 : 0xff000000;
}

// One frame of a game: the ending fade, the stage start and transition,
// the demo's end, the music restart and the timers. UPDATE_FUNC_BREAK
// skips the rest of the frame's on_tick functions: that is how a menu or a
// music restart stops the game.
// FUNCTION: TH16 0x42d7b0
HARNESS_CALLED i32 GameThread::on_tick_body()
{
    // After the last stage: fade out from frame 180, then the ending (15),
    // or for the extra stage the staff roll's mode 16.
    if (GAME_THREAD_FLAG_WORD(this) & GAME_THREAD_GAME_CLEARED)
    {
        if (GAME_THREAD_FLAG_WORD(this) & GAME_THREAD_IN_MENU)
        {
            return UPDATE_FUNC_BREAK;
        }
        fade_timer++;
        if (fade_timer == 180)
        {
            ScreenEffect::create_inline(SCREEN_EFFECT_FADE_OUT_VIEWPORT, 200, 0, 0, 0, 0x54);
        }
        if (fade_timer >= 380)
        {
            if (replay_mode != 0)
            {
                open_replay_end_menu();
            }
            else if (g_Globals.difficulty != DIFFICULTY_EXTRA)
            {
                g_Supervisor.gamemode_to_switch_to =
                    (g_Supervisor.flags & SUPERVISOR_IDLE_ON_EXIT) ? GAMEMODE_IDLE : GAMEMODE_ENDING;
            }
            else
            {
                g_Supervisor.gamemode_to_switch_to =
                    (g_Supervisor.flags & SUPERVISOR_IDLE_ON_EXIT) ? GAMEMODE_IDLE : GAMEMODE_TITLE_SCORE_ENTRY;
            }
        }
    }
    // The game waits for the stage clear bonus's first 120 frames.
    if ((g_Gui->hud_flags & GUI_STAGE_CLEAR_BONUS) && g_Gui->notice_timer.current < 120 && !flags.in_menu)
    {
        return UPDATE_FUNC_CONTINUE;
    }
    if (time_in_stage.current == 0)
    {
        if (begin_stage())
        {
            return UPDATE_FUNC_CONTINUE;
        }
    }
    else if (time_in_stage.current == 30)
    {
        finish_stage_transition();
    }
    // The previous stage's background goes once it has faded out.
    // Tested through a local, deleted through the global: the delete keeps
    // its own null test, as in the original.
    Stage *stage = g_Stage2;
    if (stage != NULL && (stage->stage_flags & STAGE_DISABLED))
    {
        delete g_Stage2;
    }
    if (time_in_stage.current == 5)
    {
        g_frame_pacing.mode = FRAME_PACING_GAME;
    }
    if (GAME_THREAD_FLAG_WORD(this) & GAME_THREAD_LOADING)
    {
        GAME_THREAD_FLAG_WORD(this) |= GAME_THREAD_TICKED_WHILE_LOADING;
        return UPDATE_FUNC_CONTINUE;
    }
    // The demo ends on a key press or a menu, or after 0xf3c frames (with a
    // fade from 0xf00), back to the title.
    if (g_Globals.flags_hi_45c & GLOBALS_HI_DEMO_PLAY)
    {
        if ((g_hardware_input & (INPUT_ENTER | INPUT_MENU | INPUT_BOMB | INPUT_SHOT)) ||
            (GAME_THREAD_FLAG_WORD(this) & (GAME_THREAD_IN_MENU | GAME_THREAD_FLAG_5 | GAME_THREAD_FLAG_6)))
        {
            g_Supervisor.gamemode_to_switch_to =
                (g_Supervisor.flags & SUPERVISOR_IDLE_ON_EXIT) ? GAMEMODE_IDLE : GAMEMODE_TITLE;
        }
        if (time_in_stage.current == 0xf00)
        {
            ScreenEffect::create(SCREEN_EFFECT_FADE_OUT_VIEWPORT, 60, 0, 0, 0, 0x33);
        }
        else if (time_in_stage.current == 0xf3c)
        {
            g_Supervisor.gamemode_to_switch_to =
                (g_Supervisor.flags & SUPERVISOR_IDLE_ON_EXIT) ? GAMEMODE_IDLE : GAMEMODE_TITLE;
        }
    }
    Gui::update_score();
    // Three separate tests, as in the original (one || condition merges
    // them into a single test of the three bits).
    if (GAME_THREAD_FLAG_WORD(this) & GAME_THREAD_IN_MENU)
    {
        return UPDATE_FUNC_BREAK;
    }
    if (GAME_THREAD_FLAG_WORD(this) & GAME_THREAD_FLAG_5)
    {
        return UPDATE_FUNC_BREAK;
    }
    if (GAME_THREAD_FLAG_WORD(this) & GAME_THREAD_FLAG_6)
    {
        return UPDATE_FUNC_BREAK;
    }
    // Before chapter 0x2b the stage music restarts and seeks back to the
    // stage time.
    if (GAME_THREAD_FLAG_WORD(this) & GAME_THREAD_MUSIC_RESTART)
    {
        if (music_restart_time == 0)
        {
            if (g_Globals.chapter < 0x2b)
            {
                g_Supervisor.stop_bgm();
            }
            AnmManager::interrupt_tree(*(AnmId *)&g_Supervisor.config.loading_effect_id, 1);
        }
        music_restart_time++;
        if (music_restart_time < music_restart_delay && music_restart_time > 1)
        {
            return UPDATE_FUNC_BREAK;
        }
        if (music_restart_time >= music_restart_delay)
        {
            if (g_Globals.chapter < 0x2b)
            {
                g_Supervisor.play_bgm(0, g_stage_data->music_ids[0]);
                while (SoundManager::update_sound_thread() != 0)
                {
                }
            }
            // Seeking here, with the double math in this function, makes
            // LTCG realign it early, which gives begin_stage,
            // finish_stage_transition and ReplayManager::begin_stage the
            // original's frames (see start_std_vms_func for its one cost).
            if (g_Globals.chapter < 0x2b)
            {
                ((CStreamingSound *)g_SoundManager.bgm_stream)->seek(g_Globals.time_in_stage / 60.0);
            }
            GAME_THREAD_FLAG_WORD(this) &= ~GAME_THREAD_MUSIC_RESTART;
            music_restart_time = 0;
        }
    }
    g_Supervisor.arcade_blit_vm_0f->run();
    g_Supervisor.arcade_blit_vm_1a->run();
    g_Supervisor.arcade_blit_vm_2c->run();
    g_Supervisor.arcade_blit_vm_39->run();
    g_Globals.time_in_stage++;
    g_Globals.time_in_chapter++;
    if (g_cancel_screen_effects != 0)
    {
        g_cancel_screen_effects--;
    }
    time_in_stage++;
    return UPDATE_FUNC_CONTINUE;
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

// Not while watching a replay, nor when the game is quitting (-1) or
// failed to start (3). The score file counts in hundredths of a second.
// FUNCTION: TH16 0x42dbc0
void GameThread::update_play_time()
{
    if (g_GameThread->replay_mode != 0)
    {
        return;
    }
    if (g_Supervisor.gamemode_to_switch_to == GAMEMODE_INVALID || g_Supervisor.gamemode_to_switch_to == GAMEMODE_QUIT)
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
    // The stage's ECL starts in its "main" sub, run by an invisible enemy.
    EnemyCreateParams params;
    memset(&params, 0, sizeof(params));
    g_EnemyManager->allocate_new_enemy("main", &params, 0);
    Gui::setup_stage_hud();
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

// begin_stage calls Stage::start_std_vms through this helper's function
// pointer, which the optimizer turns back into the original's direct call.
// LTCG builds its call graph before that, so it sees no call edge from the
// realigned begin_stage, and start_std_vms keeps its shrink-wrapped edi; a
// direct call (even from an inline helper) gives it known alignment and
// loses it. start_std_vms is static (it reads g_Stage itself) because an
// address-taken member keeps `this` in ecx.
typedef void (*StageFunc)();
static inline StageFunc start_std_vms_func()
{
    return &Stage::start_std_vms;
}

// allocate_new_enemy's third argument is unused, and LTCG drops it once it
// sees every caller (HARNESS_CALLED, see src/harness/r3b.cpp).
// TODO: the original fills the dropped argument's slot with `push ecx`;
// ours leaves memset's last argument there, as finish_stage_transition does.
// FUNCTION: TH16 0x42dc50
HARNESS_CALLED i32 GameThread::begin_stage()
{
    g_Gui->hide_stage_clear_bonus();
    if (flags.failed)
    {
        g_Supervisor.thread.join_if_running();
        g_Supervisor.gamemode_to_switch_to = (~(g_Supervisor.flags >> 13) & 1) | 2;
        return 1;
    }
    start_std_vms_func()();
    if (g_Stage2 != NULL)
    {
        g_Stage2->start_exit();
        g_Stage->start_enter();
        flags.stage_transition = 1;
        AnmManager::interrupt_tree(g_Gui->spell_notice_id, 1);
        return 0;
    }
    flags.stage_transition = 0;
    restart_stage_objects();
    if (g_Globals.game_mode != GAME_MODE_SPELL_PRACTICE && !(g_Globals.flags_hi_45c & GLOBALS_HI_DEMO_PLAY))
    {
        g_Supervisor.play_bgm(0, g_stage_data->music_ids[0]);
    }
    AnmManager::interrupt_tree(g_AsciiManager->unk_19244, 1);
    g_AsciiManager->hide_now_loading_inline();
    AnmManager::interrupt_tree(*(AnmId *)&g_Supervisor.config.loading_effect_id, 1);
    return 0;
}

// memset's last argument stays on the stack as allocate_new_enemy's dropped
// third one, as in the original.
// TODO: effective match only: the original indexes bgm_unlocked as
// [esi + eax] (base and index swapped; no source form found).
// FUNCTION: TH16 0x42dee0
i32 GameThread::finish_stage_transition()
{
    if (flags.stage_transition)
    {
        flags.stage_transition = 0;
        restart_stage_objects();
        if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
        {
            g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
        }
        else
        {
            g_SoundManager.modify_bgm(BGM_STOP, 0, "dummy");
        }
        i32 music = g_stage_data->music_ids[0];
        if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
        {
            g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
        }
        g_SoundManager.modify_bgm(BGM_PLAY, 0, "dummy");
        g_Scorefile->bgm_unlocked[music] = 1;
        g_AsciiManager->hide_now_loading_inline();
        time_in_stage.reset();
    }
    return 0;
}

// The end of a stage (MSG_STAGE_END, or ECL dialogRead -2): the clear
// bonus, then the next stage (GAMEMODE_NEXT_STAGE), the ending (stage 6 and the
// extra stage, with the play and clear counts and a bonus of 5 million per
// life and 1 million per bomb) or, in practice, the end of the game with
// the practice records updated. Always 0.
// FUNCTION: TH16 0x42e150
i32 stage_clear()
{
    GameThread *thread = g_GameThread;
    if (g_Globals.game_mode != GAME_MODE_SPELL_PRACTICE)
    {
        Gui::show_stage_clear_bonus();
    }
    g_Player->withdraw_options();
    if (g_MainBomb->in_use != 0)
    {
        g_MainBomb->end_at_stage_clear();
    }
    if (g_Globals.game_mode != GAME_MODE_NORMAL)
    {
        if (g_GameThread->replay_mode != 0)
        {
            open_replay_end_menu();
            return 0;
        }
        if (g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE)
        {
            ScorefileSpell *spell =
                &g_Scorefile->characters[g_Globals.subshot + g_Globals.character].spells[g_Globals.spell_id];
            if (spell->practice_score < (i32)(g_Globals.score / 10 * 10))
            {
                spell->practice_score = g_Globals.score / 10 * 10;
            }
        }
        if (g_GameThread->replay_mode == 0 && g_Globals.game_mode != GAME_MODE_SPELL_PRACTICE)
        {
            g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
                .practices[g_Globals.difficulty][g_Globals.stage_num - 1]
                .cleared = 1;
        }
        open_stage_end_menu();
        return 0;
    }
    if (g_Globals.stage_num == 6)
    {
        GAME_THREAD_FLAG_WORD(thread) |= GAME_THREAD_GAME_CLEARED;
        thread->fade_timer = 0;
        g_Gui->hud_flags |= GUI_FLAG_GAME_CLEARED;
        GameThread::update_play_time();
        i32 bonus = 0;
        switch (g_Globals.difficulty)
        {
        case DIFFICULTY_EASY:
            bonus = (g_Globals.lives * 5 + g_Globals.bombs) * 1000000;
            break;
        case DIFFICULTY_NORMAL:
            bonus = (g_Globals.lives * 5 + g_Globals.bombs) * 1000000;
            break;
        case DIFFICULTY_HARD:
            bonus = (g_Globals.lives * 5 + g_Globals.bombs) * 1000000;
            break;
        case DIFFICULTY_LUNATIC:
            bonus = (g_Globals.lives * 5 + g_Globals.bombs) * 1000000;
            break;
        }
        g_Globals.add_to_score(bonus);
        g_Gui->stage_clear_bonus += bonus;
        if (g_GameThread->replay_mode != 0)
        {
            open_replay_end_menu();
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
        g_Gui->hud_flags |= GUI_FLAG_GAME_CLEARED;
        i32 bonus = (g_Globals.lives * 5 + g_Globals.bombs) * 1000000;
        g_Globals.add_to_score(bonus);
        g_Gui->stage_clear_bonus += bonus;
        if (g_GameThread->replay_mode != 0)
        {
            open_replay_end_menu();
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
        GAME_THREAD_FLAG_WORD(thread) |= GAME_THREAD_GAME_CLEARED;
        thread->fade_timer = 0;
        return 0;
    }
    if (g_GameThread->replay_mode == 0)
    {
        g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
            .practices[g_Globals.difficulty][g_Globals.stage_num - 1]
            .cleared = 1;
    }
    g_Supervisor.gamemode_to_switch_to = GAMEMODE_NEXT_STAGE;
    if (g_Globals.stage_num < 7)
    {
        g_Globals.stage_num++;
    }
    g_stage_data = &g_stage_table[g_Globals.stage_num];
    return 0;
}
