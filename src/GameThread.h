#pragma once

#include <string.h>

#include "Supervisor.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

// The 0x64 bytes of settings a game in progress (and a replay, at +0x18)
// carries; the same values as Config from its second field on (the names
// follow Config's). The constructor sets the defaults (0x42e630, ExpHP:
// GameThreadChild64::constructor).
struct ConfigData
{
    u32 version;
    i16 pad_mapping[10];
    i16 deadzone_x;
    i16 deadzone_y;
    u8 color_mode;
    u8 bgm_mode;
    u8 unk_1e;
    u8 window_size;
    u8 frame_skip;
    u8 unk_21;
    u8 bgm_volume;
    u8 se_volume;
    u8 unk_24;
    u8 unk_25;
    u8 unk_26[2];
    u32 flags;
    // The window position (0x80000000: not set).
    u32 window_x;
    u32 window_y;
    u8 unk_34[0x64 - 0x34];

    // Not itself the constructor: it returns nothing.
    ConfigData()
    {
        set_defaults();
    }
    DECOMP_NOINLINE void set_defaults();

    // set_defaults' body, which Supervisor::load_game_config has inline.
    __forceinline void set_defaults_inline()
    {
        memset(this, 0, sizeof(ConfigData));
        flags |= 0x100;
        color_mode = 0;
        bgm_mode = 1;
        version = 0x160002;
        deadzone_x = deadzone_y = 600;
        unk_1e = 1;
        window_size = 5;
        frame_skip = 0;
        memcpy(pad_mapping, g_pad_mapping, sizeof(pad_mapping));
        unk_21 = 2;
        bgm_volume = 100;
        unk_24 = 0;
        unk_25 = 2;
        se_volume = 80;
        window_x = 0x80000000;
        window_y = 0x80000000;
    }
};

// GameThread::flags. Some are written as bitfields and some through the
// GameThreadFlagMask masks of the whole word, as in the original.
struct GameThreadFlags
{
    // Never set in TH16: the game objects freeze, as while loading.
    u32 flag_0 : 1;
    // Never set in TH16: enemies, lasers and the game's ANM VMs freeze
    // (lasers tick with the game speed forced to 0).
    u32 flag_1 : 1;
    // The thread is setting the game up (from create until thread_start is
    // done): items and lasers neither tick nor draw, and replays stop
    // recording.
    u32 loading : 1;
    // thread_start failed: the game goes back to the title.
    u32 failed : 1;
    // The pause menu (or the game over or replay end menu) is open: the
    // current second does not count in FpsCounter's totals.
    u32 in_menu : 1;
    // Never set in TH16; like in_menu, they stop the game.
    u32 flag_5 : 1;
    u32 flag_6 : 1;
    // on_tick ran while loading; FpsCounter clears it once a second.
    u32 ticked_while_loading : 1;
    u32 flag_8 : 2;
    // Never set in TH16: enemies, items and lasers freeze, and bullets are
    // not cancelled.
    u32 flag_10 : 1;
    // begin_stage started a crossfade from the previous stage's background;
    // finish_stage_transition resets the game objects after it.
    u32 stage_transition : 1;
    u32 flag_12 : 2;
    // The last stage was cleared: the ending fade runs (fade_timer).
    u32 game_cleared : 1;
    u32 flag_15 : 1;
    // The stage music is restarting (waiting music_restart_delay frames,
    // then seeking to the stage time); keeps the pause key from opening the
    // pause menu. Nothing in TH16 sets it.
    u32 music_restart : 1;
    u32 flag_17 : 15;
};

// The GameThreadFlags bits as masks of the whole word.
enum GameThreadFlagMask
{
    GAME_THREAD_LOADING = 1 << 2,
    GAME_THREAD_FAILED = 1 << 3,
    GAME_THREAD_IN_MENU = 1 << 4,
    GAME_THREAD_FLAG_5 = 1 << 5,
    GAME_THREAD_FLAG_6 = 1 << 6,
    GAME_THREAD_TICKED_WHILE_LOADING = 1 << 7,
    GAME_THREAD_GAME_CLEARED = 1 << 14,
    GAME_THREAD_MUSIC_RESTART = 1 << 16,
};

// Runs a game in progress: sets it up on a worker thread, then each frame
// handles the stage start, the ending fade, the demo's end and the timers.
// Layout from ExpHP's th-re-data (zGameThread), except that the settings
// start at 0x24, not 0x20.
struct GameThread
{
    u32 unk_0;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    ZunTimer time_in_stage;
    // The stage table's stage number when the thread started.
    u32 start_stage_num;
    // The settings the game runs with (a copy of the Supervisor's, or the
    // replay's).
    ConfigData config;
    GameThreadFlags flags;
    // Frames since the music restart began, and how many to wait (60).
    i32 music_restart_time;
    i32 music_restart_delay;
    // Zeroed by the constructor (before its memset); not otherwise used.
    i32 unk_94;
    u8 unk_98[0xa8 - 0x98];
    // Set by ECL along with Globals::chapter.
    i32 chapter;
    // 0: playing, otherwise watching a replay.
    i32 replay_mode;
    // Frames since the last stage was cleared (game_cleared).
    i32 fade_timer;

    GameThread();
    ~GameThread();

    // Starts a game: the setup runs in thread_start on the Supervisor's
    // worker thread.
    HARNESS_CALLED static GameThread *create(i32 replay_mode);
    static void destroy();
    static i32 thread_start();
    static void thread_start_callback();
    HARNESS_CALLED i32 on_tick_body();
    // 0x418420. Reactivates on_tick and on_draw.
    void enable_update_funcs();
    static i32 __fastcall on_tick_callback(GameThread *thread);
    static i32 __fastcall on_draw_callback(GameThread *thread);
    // Adds the time since the last call to the scorefile's play time.
    static void update_play_time();
    // 0x42dc50. Starts the stage on its first frame: 1 if thread_start
    // failed (the game then switches to mode 3, or 2 with Supervisor flag
    // 0x2000); otherwise starts the background and resets the game
    // objects, or, coming from a previous stage, crossfades from its
    // background and leaves the reset to finish_stage_transition.
    HARNESS_CALLED i32 begin_stage();
    // 0x42dee0. 30 frames in, finishes a stage transition that begin_stage
    // started: resets the game objects, reactivates every manager and
    // starts the stage music.
    i32 finish_stage_transition();
};

extern GameThread *g_GameThread;
