#pragma once

#include "AnmManager.h"
#include "MenuHelper.h"
#include "UpdateFunc.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "types.h"

struct ReplayManager;

// The in-game pause menu (also shown on game over and after a replay).
// Layout partly from ExpHP's th-re-data (zPauseMenu). Packed to 4 bytes for
// saved_bgm_time.
#pragma pack(push, 4)
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
    i32 unk_1fc;
    // Nonzero once the score name is entered (the keyboard is hidden).
    i32 unk_200;
    u8 unk_204[0x208 - 0x204];
    i32 saved_global_4d9d90;
    ReplayManager *replays[25];
    u8 unk_270[0x2d4 - 0x270];
    // The replay or score name being entered.
    char name[0xc];
    f32 saved_game_speed;
    // The BGM playing when the game ended, to resume after the menu.
    double saved_bgm_time;
    char saved_bgm_name[0x100];
    i32 flags_3ec;
    AnmLoaded *front_anm;

    PauseMenu();
    ~PauseMenu();
    int initialize();
    static PauseMenu *create();
    // Kept alive by its callers rather than /INCLUDE, so that LTCG sees it
    // leaves ecx alone (replay_ended_43f240 counts on that).
    HARNESS_CALLED void set_state(i32 state);
    void set_unk_1f4(i32 value);

    // 0x43f6a0, 0x43f740 and 0x43f790: leaving states 1 (the pause menu),
    // 2 and 3. They restore the game speed and the play time clock; the
    // first also resumes the game and the dialogue.
    void leave_state_1();
    void leave_state_2();
    void leave_state_3();

    int on_tick();
    // 0x43f980 (ExpHP: on_tick_in_pause_menu). The menu's tick while it is
    // open (states 1 to 3).
    void tick_open();
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
    // 0x43ef20 (ExpHP: take_snapshot_for_pause). Copies the game area of
    // the screen into the sprite of a new text.anm VM behind the menu.
    void take_snapshot();
    // 0x43f0f0 (ExpHP: do_open_pause_menu). Pauses the game and opens the
    // menu.
    void open();
    // 0x43f7e0. At the end of a game: records a practice score, or puts
    // the score into the high score table and sets up the name entry.
    void begin_score_entry();
    static int __fastcall on_tick_thunk(void *arg);
    static int __fastcall on_draw_thunk(void *arg);
};
#pragma pack(pop)

extern PauseMenu *g_PauseMenu;

// 0x43f240. Opens the menu shown when a replay ends.
void replay_ended_43f240();
// 0x43f350 (ExpHP: sub_43f350_pause). Opens the menu at the end of a
// game (or ends spell practice's retry loop).
void pause_menu_43f350();
