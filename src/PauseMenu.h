#pragma once

#include "AnmManager.h"
#include "MenuHelper.h"
#include "UpdateFunc.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "types.h"

struct ReplayManager;

// PauseMenu::state: which menu is open.
enum PauseState
{
    PAUSE_CLOSED = 0,
    // The pause menu (Esc), or the menu shown when a replay ends.
    PAUSE_PAUSED = 1,
    // Game over: continue, or quit (also how spell practice ends).
    PAUSE_GAME_OVER = 2,
    // A practice or spell practice stage was finished.
    PAUSE_STAGE_END = 3,
};

// PauseMenu::substate: the step of the open menu (tick_open).
enum PauseSubstate
{
    // The menus appearing, by how they were opened.
    PAUSE_SUB_OPEN_PAUSE = 0,
    PAUSE_SUB_OPEN_REPLAY_END = 1,
    PAUSE_SUB_OPEN_GAME_OVER = 2,
    PAUSE_SUB_OPEN_STAGE_END = 3,
    // Never set.
    PAUSE_SUB_OPEN_4 = 4,
    PAUSE_SUB_OPEN_SPELL_PRACTICE_END = 5,
    // Choosing a PauseMenuItem.
    PAUSE_SUB_CHOOSE = 6,
    // "Really?" before quitting or restarting.
    PAUSE_SUB_CONFIRM = 7,
    // Leaving the "Really?" question.
    PAUSE_SUB_CONFIRM_CLOSE = 8,
    // "Really?" before saving the replay (which ends the game).
    PAUSE_SUB_CONFIRM_SAVE = 9,
    // Opening the replay slots.
    PAUSE_SUB_OPEN_REPLAY_SLOTS = 10,
    PAUSE_SUB_REPLAY_SLOT_SELECT = 11,
    PAUSE_SUB_REPLAY_NAME_ENTRY = 12,
    PAUSE_SUB_MANUAL = 14,
    PAUSE_SUB_SCORE_NAME_ENTRY = 15,
    // Closing the menu and acting on the choice.
    PAUSE_SUB_CLOSE = 16,
};

// The menu's items, top to bottom (the game over menu's first item is
// "continue").
enum PauseMenuItem
{
    PAUSE_ITEM_RESUME = 0,
    PAUSE_ITEM_QUIT = 1,
    PAUSE_ITEM_SAVE_REPLAY = 2,
    PAUSE_ITEM_MANUAL = 3,
    PAUSE_ITEM_RESTART = 4,
    PAUSE_ITEM_COUNT = 5,
};

// The menus' scripts in front.anm (menu_anm_id), and the snapshot's in
// text.anm (snapshot_id).
enum PauseMenuScripts
{
    PAUSE_SCRIPT_PAUSE_MENU = 0x9c,
    PAUSE_SCRIPT_REPLAY_PAUSE_MENU = 0x9e,
    PAUSE_SCRIPT_REPLAY_END_MENU = 0x9f,
    // After a game over in the main game (with "continue").
    PAUSE_SCRIPT_GAME_OVER_MENU = 0xa0,
    // After practice or spell practice ends (no "continue").
    PAUSE_SCRIPT_PRACTICE_END_MENU = 0xa1,
    TEXT_SCRIPT_PAUSE_SNAPSHOT = 0x34,
};

// PauseMenu::menu_flags.
enum PauseMenuFlags
{
    // on_draw shows the replay slots or the replay name entry.
    PAUSE_SHOW_REPLAY_SLOTS = 1 << 0,
    PAUSE_SHOW_REPLAY_NAME = 1 << 1,
    // Opened by open_stage_end_menu.
    PAUSE_FROM_STAGE_END = 1 << 2,
};

// The in-game pause menu (also shown on game over and after a replay).
// Layout partly from ExpHP's th-re-data (zPauseMenu). Packed to 4 bytes for
// saved_bgm_time.
#pragma pack(push, 4)
struct PauseMenu
{
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    // Time in the current substate, and since the menu opened.
    ZunTimer time_in_current_menu;
    ZunTimer time_since_pause_or_unpause;
    // The cursor on the menu items, the "Really?" question, the replay
    // slots and the high score table.
    MenuHelper item_menu;
    // The name entry's character grid (91 characters, 13 to a row).
    MenuHelper name_entry_menu;
    // The menu's front.anm UI VM (scripts 0x9c to 0xa1, by menu).
    AnmId menu_anm_id;
    // text.anm script 0x34 behind the menu, showing a snapshot of the game
    // area (take_snapshot).
    AnmId snapshot_id;
    // PauseState and PauseSubstate values.
    i32 state;
    i32 prev_state;
    i32 substate;
    // Where the next character of the replay or score name goes.
    i32 name_cursor;
    // Set when the menu opened because a stage was finished
    // (open_stage_end_menu, only for practice in TH16), 0 for a game over.
    // The tests for a main game's clear (an Extra clear recorded as stage
    // 9, ...) are never reached.
    i32 stage_finished;
    // Set when the score did not make the top ten: no name entry.
    i32 score_not_ranked;
    // Nonzero in the menu shown when a replay ends, where the R (retry) and
    // Esc (resume) shortcuts do nothing.
    i32 replay_ended;
    // g_frame_pacing.mode while the menu is open.
    i32 saved_pacing_mode;
    ReplayManager *replays[25];
    u8 unk_270[0x2d4 - 0x270];
    // The replay or score name being entered.
    char name[0xc];
    f32 saved_game_speed;
    // The BGM playing when the game ended, to resume after the name_entry_menu.
    double saved_bgm_time;
    char saved_bgm_name[0x100];
    // PauseMenuFlags.
    i32 menu_flags;
    AnmLoaded *front_anm;

    PauseMenu();
    ~PauseMenu();
    int initialize();
    static PauseMenu *create();
    // Kept alive by its callers rather than /INCLUDE, so that LTCG sees it
    // leaves ecx alone (open_replay_end_menu counts on that).
    HARNESS_CALLED void set_state(i32 state);
    // 0x43e200
    DECOMP_NOINLINE void set_substate(i32 value);
    // set_substate where LTCG inlined it.
    __forceinline void set_substate_inline(i32 value)
    {
        substate = value;
        time_in_current_menu.reset();
    }

    // 0x43f6a0, 0x43f740 and 0x43f790: closing the menu of each state.
    // They restore the game speed and the play time clock; the first also
    // resumes the game and the dialogue.
    void leave_paused();
    void leave_game_over();
    void leave_stage_end();

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
    // the screen into the sprite of a new text.anm VM behind the name_entry_menu.
    void take_snapshot();
    // 0x43f0f0 (ExpHP: do_open_pause_menu). Pauses the game and opens the
    // name_entry_menu.
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
void open_replay_end_menu();
// 0x43f500. Opens the menu shown when a practice (or spell practice) stage
// is finished (retry or quit); a replay just goes back to the title.
void open_stage_end_menu();
// 0x43f350 (ExpHP: sub_43f350_pause). Opens the game over menu (continue
// or quit), which also ends a spell practice; a replay just goes back to
// the title.
void open_game_over_menu();
