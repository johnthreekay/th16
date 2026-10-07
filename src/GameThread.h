#pragma once

#include <string.h>

#include "Supervisor.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

// th16.cfg: the 0x64 bytes of settings that Config holds from its version
// field on, as a struct of its own. A game in progress (and a replay, at
// +0x18) carries a copy. Field meanings are Config's. The constructor sets
// the defaults (0x42e630, ExpHP: GameThreadChild64::constructor).
struct ConfigData
{
    u32 version;
    i16 pad_mapping[10];
    i16 deadzone_x;
    i16 deadzone_y;
    u8 color_mode;
    u8 bgm_mode;
    u8 se_enabled;
    u8 window_size;
    u8 frame_skip;
    u8 unk_21;
    u8 bgm_volume;
    u8 se_volume;
    u8 unk_24;
    u8 frame_pacing;
    u8 unk_26[2];
    u32 flags;
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
        flags |= CONFIG_SHOW_STARTUP_DIALOG;
        color_mode = 0;
        bgm_mode = 1;
        version = CONFIG_VERSION;
        deadzone_x = deadzone_y = 600;
        se_enabled = 1;
        window_size = WINDOW_SIZE_WINDOWED_1280;
        frame_skip = 0;
        memcpy(pad_mapping, g_pad_mapping, sizeof(pad_mapping));
        unk_21 = 2;
        bgm_volume = 100;
        unk_24 = 0;
        frame_pacing = 2;
        se_volume = 80;
        window_x = 0x80000000;
        window_y = 0x80000000;
    }
};

struct GameThreadFlags
{
    u32 flag_0 : 1;
    // Lasers tick with the game speed forced to 0.
    u32 flag_1 : 1;
    // Paused (also set while the stage loads): items and lasers neither
    // tick nor draw, and replays stop recording.
    u32 paused : 1;
    u32 flag_3 : 1;
    // Excludes the current second from FpsCounter's totals.
    u32 flag_4 : 1;
    u32 flag_5 : 1;
    u32 flag_6 : 1;
    // Cleared by FpsCounter once a second.
    u32 flag_7 : 1;
    u32 flag_8 : 2;
    u32 flag_10 : 1;
    // Set while the stage restarts after its intro (sub_42dc50/sub_42dee0).
    u32 flag_11 : 1;
    u32 flag_12 : 4;
    // Keeps the pause key from opening the pause menu.
    u32 flag_16 : 1;
    u32 flag_17 : 15;
};

// Runs a game in progress. Layout from ExpHP's th-re-data (zGameThread),
// except that the settings start at 0x24, not 0x20.
struct GameThread
{
    u32 unk_0;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    ZunTimer time_in_stage;
    u32 unk_20;
    ConfigData config;
    GameThreadFlags flags;
    // Frames since the music restart flag (0x10000) was set, and how many
    // to wait.
    i32 unk_8c;
    i32 unk_90;
    i32 unk_94;
    u8 unk_98[0xa8 - 0x98];
    i32 chapter;
    i32 replay_mode;
    i32 fade_timer;

    GameThread();
    ~GameThread();

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
    // 0x42dc50. Restarts the stage after its intro: 1 once the game
    // should switch modes, otherwise resets the game objects (or leaves
    // that to sub_42dee0 when there is a second stage).
    HARNESS_CALLED i32 sub_42dc50();
    // 0x42dee0. Finishes a stage restart that sub_42dc50 began: resets the
    // game objects, reactivates every manager and restarts the music.
    i32 sub_42dee0();
};

extern GameThread *g_GameThread;
