#pragma once

#include "Supervisor.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

// The 0x64 bytes of settings a game in progress (and a replay, at +0x18)
// carries; the same values as Config from its second field on. The constructor
// sets the defaults (0x42e630, ExpHP: GameThreadChild64::constructor).
struct ConfigData
{
    u32 version;
    i16 pad_mapping[10];
    i16 deadzone_x;
    i16 deadzone_y;
    u8 unk_1c;
    u8 unk_1d;
    u8 unk_1e;
    u8 unk_1f;
    u8 unk_20;
    u8 unk_21;
    u8 unk_22;
    u8 unk_23;
    u8 unk_24;
    u8 unk_25;
    u8 unk_26[2];
    u32 flags;
    u32 unk_2c;
    u32 unk_30;
    u8 unk_34[0x64 - 0x34];

    // Not itself the constructor: it returns nothing.
    ConfigData()
    {
        set_defaults();
    }
    DECOMP_NOINLINE void set_defaults();
};

struct GameThreadFlags
{
    u32 flag_0 : 1;
    // Lasers tick with the game speed forced to 0.
    u32 flag_1 : 1;
    // Paused (also set while the stage loads): items and lasers neither
    // tick nor draw, and replays stop recording.
    u32 paused : 1;
    u32 flag_3 : 7;
    u32 flag_10 : 1;
    u32 flag_11 : 21;
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
    u8 unk_8c[0x94 - 0x8c];
    i32 unk_94;
    u8 unk_98[0xa8 - 0x98];
    i32 chapter;
    i32 replay_mode;
    i32 fade_timer;

    GameThread();
    ~GameThread();

    HARNESS_CALLED static GameThread *create(i32 replay_mode);
    static void destroy();
    static void thread_start();
    static void thread_start_callback();
    i32 on_tick_body();
    static i32 __fastcall on_tick_callback(GameThread *thread);
    static i32 __fastcall on_draw_callback(GameThread *thread);
};

extern GameThread *g_GameThread;
