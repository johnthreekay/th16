#pragma once

#include "AnmManager.h"
#include "AsciiManager.h"
#include "MenuHelper.h"
#include "Thread.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

struct ReplayManager;

// The title screen and its menus. ExpHP calls it MainMenu; the name is
// ZUN's, from RTTI. Layout from ExpHP's th-re-data (zMainMenu) and the
// constructor; only what decompiled code needs.
// VTABLE: TH16 0x493730
class TitleInf : public TaskInf
{
  public:
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    AnmLoaded *title_anm;
    AnmLoaded *title_v_anm;
    i32 state;
    i32 prev_state;
    i32 substate;
    MenuHelper menu;
    MenuHelper menu_fc;
    MenuHelper menu_1d4;
    ZunTimer time_in_state;
    AnmId anm_ids[0x11f];
    AnmId anm_id_73c;
    AnmId anm_ids_740[0x24];
    AnmId anm_ids_7d0[9];
    u8 unk_7f4[0x5a48 - 0x7f4];
    // The replay name being entered, and the cursor in it.
    char replay_name[0xc];
    i32 replay_name_cursor;
    i32 unk_5a58;
    MenuHelper menu_5a5c;
    u8 unk_5b34[0x5b48 - 0x5b34];
    // The replay slot being saved to.
    i32 replay_slot;
    u8 unk_5b4c[0x5b50 - 0x5b4c];
    ReplayManager *replays[100];
    // Allocated with malloc.
    void *unk_5ce0;
    i32 unk_5ce4;
    // Bit 2 stops the replay list loading; bit 3 is set once it is done.
    u32 flags_5ce8;
    MenuHelper menu_5cec;
    // Spell practice: the stage and boss attack whose spell cards are
    // listed, and the spell card ids of the listed rows.
    i32 spell_stage;
    i32 spell_row;
    // The row of spell_ids picked.
    i32 spell_index;
    i32 spell_ids[5];
    ThreadInf thread;

    TitleInf();
    ~TitleInf();
    virtual u32 get_size();
    // 0x44aee0 (ExpHP: MainMenu::operator new). Starts thread_start on
    // the menu's thread, which does the rest of the setup.
    static TitleInf *create();
    // 0x44af50
    static void destroy();
    // Members that reach the menu through g_MainMenu; LTCG dropped this.
    HARNESS_CALLED i32 initialize();
    static unsigned __stdcall thread_start();
    // 0x451560. Loads the replay list for the replay menu, on the menu's
    // thread; reaches the menu through g_MainMenu.
    static void load_replay_list();
    // 0x451740. The replay menu's loading thread. Passed to
    // ThreadInf::restart as a ThreadStart, though ZUN declared it cdecl.
    static unsigned replay_list_thread(void *arg);

    void set_state(i32 state);
    void set_substate(i32 substate);
    // Interrupts anm_ids[index] with interrupt 1 and forgets it.
    void interrupt_and_clear(i32 index);
    // Interrupts the first descendant of anm_ids[index] running the
    // script. Every caller passes index 0 and interrupt 29, which LTCG
    // folds (keeping the stack slots).
    HARNESS_CALLED void interrupt_child(i32 index, i32 script, i32 interrupt);
    // The same with any interrupt, running the VM once afterwards.
    void interrupt_child_and_run(i32 index, i32 script, i32 interrupt);
    // The id of the first descendant of anm_ids[index] running the script.
    AnmId find_child_id(i32 index, i32 script);

    i32 on_tick();
    i32 on_draw();
    static i32 __fastcall on_tick_thunk(void *arg);
    static i32 __fastcall on_draw_thunk(void *arg);

    // Not decompiled yet (ExpHP's names).
    HARNESS_CALLED i32 on_draw__practice_stage_select();
    HARNESS_CALLED i32 on_draw__replay();
    HARNESS_CALLED i32 on_draw__player_data();
    HARNESS_CALLED i32 on_draw__4538b0();
    HARNESS_CALLED i32 on_draw__4541b0();
    HARNESS_CALLED i32 on_draw__spell_practice_histories();

    // States of on_tick (ExpHP: do_*).
    i32 do_manual();
    i32 do_spell_practice_stage_select();
    i32 do_spell_practice_character();
    i32 do_spell_practice_row();
    i32 do_spell_practice_subseason();
    i32 do_spell_practice_difficulty();
    // 0x4560b0. Fills spell_ids (and their VMs) with the spell cards of a
    // stage's boss attack. The last argument is the same at every call
    // site; LTCG folded it.
    DECOMP_NOINLINE void load_spell_list(i32 stage, i32 row, i32 *ids, i32 unused);
    i32 highlight_spell_row(i32 selected);
    // The row of replay_slot on its page of 25.
    i32 replay_slot_row()
    {
        return replay_slot % 25;
    }
};

extern TitleInf *g_MainMenu;
