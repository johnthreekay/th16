#pragma once

#include "AnmManager.h"
#include "MenuHelper.h"
#include "UpdateFunc.h"
#include "ZunMath.h"
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
    // Where the next character of the replay or score name goes.
    i32 name_cursor;
    u8 unk_1fc[0x200 - 0x1fc];
    // Nonzero once the score name is entered (the keyboard is hidden).
    i32 unk_200;
    u8 unk_204[0x208 - 0x204];
    i32 saved_global_4d9d90;
    ReplayManager *replays[25];
    u8 unk_270[0x2d4 - 0x270];
    // The replay or score name being entered.
    char name[0xc];
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
    // 0x43e730. The name entry keyboard; the name is drawn at pos.
    void draw_keyboard(Float3 pos);
    // 0x43e8c0. One line of the replay list. Does not use this; LTCG
    // dropped it.
    HARNESS_CALLED void draw_replay_entry(i32 index, Float3 *pos, struct RpyInfo *info);
    // 0x43e9b0
    void draw_replay_list();
    // 0x43eb50. Naming the replay to save.
    void draw_replay_name_entry();
    // 0x43ec10. The high score table of the game that just ended, with the
    // keyboard for the name.
    void draw_high_scores();
    static int __fastcall on_tick_thunk(void *arg);
    static int __fastcall on_draw_thunk(void *arg);
};

extern PauseMenu *g_PauseMenu;
