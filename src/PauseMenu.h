#pragma once

#include "AnmManager.h"
#include "MenuHelper.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

struct ReplayManager;

// The in-game pause menu (also shown on game over and after a replay).
// Layout partly from ExpHP's th-re-data (zPauseMenu).
struct PauseMenu
{
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    ZunTimer time_in_current_menu;
    ZunTimer time_since_pause_or_unpause;
    MenuHelper menu_34;
    MenuHelper menu;
    AnmId anm_id_1e4;
    AnmId anm_id_1e8;
    i32 state;
    i32 prev_state;
    i32 unk_1f4;
    u8 unk_1f8[0x208 - 0x1f8];
    i32 saved_global_4d9d90;
    ReplayManager *replays[25];
    u8 unk_270[0x2e0 - 0x270];
    f32 saved_game_speed;
    u8 unk_2e4[0x3ec - 0x2e4];
    i32 flags_3ec;
    AnmLoaded *front_anm;

    PauseMenu();
    ~PauseMenu();
    int initialize();
    static PauseMenu *create();
    void set_state(i32 state);
    void set_unk_1f4(i32 value);

    // 0x43f6a0, 0x43f740 and 0x43f790: leaving states 1 (the pause menu),
    // 2 and 3. They restore the game speed and the play time clock; the
    // first also resumes the game and the dialogue.
    void leave_state_1();
    void leave_state_2();
    void leave_state_3();

    int on_tick();
    int on_draw();
    static int __fastcall on_tick_thunk(void *arg);
    static int __fastcall on_draw_thunk(void *arg);
};

extern PauseMenu *g_PauseMenu;
