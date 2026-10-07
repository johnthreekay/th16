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
    // The music room: the number of tracks, the comment line being
    // written (one every other frame) and its track, and whether the
    // warning about a track not heard yet is shown.
    i32 music_track_count;
    i32 music_comment_line;
    i32 music_comment_track;
    i32 music_warning;
    // From musiccmt.txt: file name, title and eight comment lines per
    // track.
    char music_filenames[0x20][0x40];
    char music_titles[0x20][0x42];
    char music_comments[0x20][8][0x42];
    // The first track row shown.
    i32 music_scroll;
    // The replay name being entered, and the cursor in it.
    char replay_name[0xc];
    i32 replay_name_cursor;
    i32 unk_5a58;
    MenuHelper menu_5a5c;
    // The key config being edited: the button for each action.
    i16 key_config[6];
    u8 unk_5b40[0x5b44 - 0x5b40];
    i32 unk_5b44;
    // The replay slot being saved to or played.
    i32 replay_slot;
    // The stage picked to start a replay from (minus one).
    i32 replay_stage;
    ReplayManager *replays[100];
    // Allocated with malloc (musiccmt.txt, while the music room is open).
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
    // Starts the title_anm script of the same index in anm_ids[index].
    void create_effect(i32 index)
    {
        anm_ids[index] = title_anm->create_effect(index, -1, NULL);
    }
    // Interrupts the first descendant of anm_ids[index] running the
    // script. Every caller passes index 0 and interrupt 29, which LTCG
    // folds (keeping the stack slots).
    HARNESS_CALLED void interrupt_child(i32 index, i32 script, i32 interrupt);
    // The same with any interrupt, running the VM once afterwards.
    void interrupt_child_and_run(i32 index, i32 script, i32 interrupt);
    // The id of the first descendant of anm_ids[index] running the script.
    AnmId find_child_id(i32 index, i32 script);

    // 0x44ec60. Shows the key config's buttons.
    void update_key_config_sprites();
    // 0x44f710. Binds a button to an action, giving the action's old
    // button to whichever other action had the new one.
    void set_key(i32 action, i32 key);
    // 0x44f810. Moves the key config's highlight to the selected row.
    void update_key_config_cursor();
    // 0x44e930 (ExpHP: MainMenu::do_key_config). The key config screen.
    i32 do_key_config();
    // 0x44dc70. Shows the option values.
    void update_options_sprites();
    // 0x44c8c0. Moves the options screen's highlight to the selected row.
    void update_options_cursor();
    // 0x44c570 (ExpHP: MainMenu::do_options). The options screen.
    i32 do_options();

    i32 on_tick();
    i32 on_draw();
    static i32 __fastcall on_tick_thunk(void *arg);
    static i32 __fastcall on_draw_thunk(void *arg);

    // Draw states of on_draw (ExpHP's names).
    HARNESS_CALLED i32 on_draw__practice_stage_select();
    HARNESS_CALLED i32 on_draw__replay();
    HARNESS_CALLED i32 on_draw__player_data();
    HARNESS_CALLED i32 on_draw__4538b0();
    HARNESS_CALLED i32 on_draw__4541b0();
    HARNESS_CALLED i32 on_draw__spell_practice_histories();

    // States of on_tick (ExpHP: do_*).
    // 0x44fe20
    i32 do_difficulty_select();
    // 0x4502c0
    i32 do_character_select();
    i32 do_subseason_select();
    i32 do_practice_stage_select();
    i32 do_manual();
    i32 do_replay_menu();
    i32 do_replay_save();
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
    // 0x452330 (ExpHP: sub_452330_replay_related). The player data.
    i32 do_player_data();
    // 0x4532f0 (ExpHP: do_menu_sub_4532f0). The high score name entry.
    i32 do_score_name_entry();
    // 0x4546f0 (ExpHP: do_music_room).
    i32 do_music_room();
    // 0x452c30. The spell card page of the player data.
    i32 draw_spell_card_page();
    // The row of replay_slot on its page of 25.
    i32 replay_slot_row()
    {
        return replay_slot % 25;
    }
};

extern TitleInf *g_MainMenu;

// Helpers the menus inline.

// AnmVm::search_children with its first level inlined, as LTCG did for
// some constant scripts.
__forceinline AnmVm *search_children_inline(AnmVm *vm, i32 script, i32 n)
{
    for (ZunList<AnmVm> *node = &vm->list_of_children; node != NULL; node = node->next)
    {
        AnmVm *child = node->entry;
        if (child == NULL || child == vm)
        {
            continue;
        }
        if (child->unk_49c == script || script == -1)
        {
            if (n == 0)
            {
                return child;
            }
            n--;
        }
        if (child->list_of_children.next != NULL)
        {
            AnmVm *found = child->search_children(script, n);
            if (found != NULL)
            {
                return found;
            }
        }
        if (vm->unk_49c == -2 && node->next == NULL)
        {
            return node->entry;
        }
    }
    return NULL;
}

// TitleInf::interrupt_child_and_run with search_children inlined.
__forceinline void interrupt_child_and_run_inline(AnmId &id, i32 script, i32 interrupt)
{
    AnmVm *vm;
    if (get_vm_or_clear(id) == NULL)
    {
        vm = NULL;
    }
    else
    {
        vm = search_children_inline(get_vm_or_clear(id), script, 0);
    }
    vm->interrupt(interrupt);
    vm->run();
}

// TitleInf::find_child_id as LTCG inlined it into some menus (looking the
// parent up twice).
__forceinline AnmId find_child_id_inline(AnmId &parent, i32 script)
{
    AnmVm *child = find_child_of(parent, script);
    AnmId id;
    id.id = child != NULL ? child->id.id : 0;
    return id;
}

// The VM of the first descendant of the parent running the script, looked
// up again through its id.
__forceinline AnmVm *get_child_vm(AnmId &parent, i32 script)
{
    return g_AnmManager->get_vm_with_id(find_child_id_inline(parent, script));
}

// Points the VM at a sprite through the file it came from.
__forceinline void set_child_sprite(AnmVm *vm, i32 sprite)
{
    if (vm != NULL)
    {
        g_AnmManager->loaded_anms[vm->anm_loaded_index]->set_sprite(vm, sprite);
    }
}

