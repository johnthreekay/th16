#include <string.h>

#include "AnmManager.h"
#include "AsciiManager.h"
#include "Bomb.h"
#include "BulletManager.h"
#include "EffectManager.h"
#include "EnemyManager.h"
#include "GameThread.h"
#include "Globals.h"
#include "Gui.h"
#include "Item.h"
#include "Laser.h"
#include "PauseMenu.h"
#include "Player.h"
#include "PopupManager.h"
#include "ReplayManager.h"
#include "Scorefile.h"
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
i32 GameThread::sub_42dc50()
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
