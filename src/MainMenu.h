#pragma once

#include "AnmManager.h"
#include "AsciiManager.h"
#include "MenuHelper.h"
#include "Thread.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

struct ReplayManager;

// TitleInf::state: the screen of the title menu that runs. on_tick calls
// the screen's do_* function (on_draw a draw function for some); set_state
// switches screens and resets substate, the step within the screen. Most
// screens use substate 0 to set up, 1 while their sprites appear, 2 for
// input and 4 (or 3) while they leave.
enum TitleState
{
    // Picks the first screen from g_title_return_point.
    TITLE_STATE_INIT = 0,
    // The title screen and its main menu.
    TITLE_STATE_MAIN = 1,
    // Leaving the title menu: unless a game mode switch was requested at
    // the same time (which deletes the menu first), the game quits.
    TITLE_STATE_EXIT = 2,
    TITLE_STATE_OPTIONS = 3,
    TITLE_STATE_KEY_CONFIG = 4,
    TITLE_STATE_DIFFICULTY_SELECT = 5,
    TITLE_STATE_CHARACTER_SELECT = 6,
    TITLE_STATE_SUBSEASON_SELECT = 7,
    TITLE_STATE_PRACTICE_STAGE_SELECT = 8,
    // Never entered; only stops the BGM.
    TITLE_STATE_UNUSED_9 = 9,
    TITLE_STATE_PLAYER_DATA = 10,
    TITLE_STATE_REPLAY_MENU = 11,
    // Never entered; only stops the BGM.
    TITLE_STATE_UNUSED_12 = 12,
    TITLE_STATE_MUSIC_ROOM = 13,
    // The high score name entry after a game.
    TITLE_STATE_SCORE_NAME_ENTRY = 14,
    // Saving the replay of the game just played.
    TITLE_STATE_REPLAY_SAVE = 15,
    TITLE_STATE_MANUAL = 16,
    TITLE_STATE_SPELL_PRACTICE_STAGE_SELECT = 17,
    TITLE_STATE_SPELL_PRACTICE_ROW_SELECT = 18,
    TITLE_STATE_SPELL_PRACTICE_DIFFICULTY_SELECT = 19,
    TITLE_STATE_SPELL_PRACTICE_SUBSEASON_SELECT = 20,
};

// The interrupts the title menus send to the sprites of their rows (the
// row under the cursor gets a script-specific one instead).
enum TitleRowInterrupt
{
    // Greyed out: a choice that is not available yet.
    TITLE_INTERRUPT_DISABLED = 29,
    // Rows above the cursor, and below it.
    TITLE_INTERRUPT_ABOVE_CURSOR = 30,
    TITLE_INTERRUPT_BELOW_CURSOR = 31,
};

// The title screen's menu items, top to bottom.
enum TitleMenuItem
{
    TITLE_ITEM_START = 0,
    TITLE_ITEM_EXTRA_START = 1,
    TITLE_ITEM_PRACTICE_START = 2,
    TITLE_ITEM_SPELL_PRACTICE = 3,
    TITLE_ITEM_REPLAY = 4,
    TITLE_ITEM_PLAYER_DATA = 5,
    TITLE_ITEM_MUSIC_ROOM = 6,
    TITLE_ITEM_OPTION = 7,
    TITLE_ITEM_MANUAL = 8,
    TITLE_ITEM_QUIT = 9,
    TITLE_ITEM_COUNT = 10,
};

// The options screen's rows.
enum OptionsItem
{
    OPTIONS_ITEM_BGM_VOLUME = 0,
    OPTIONS_ITEM_SE_VOLUME = 1,
    OPTIONS_ITEM_KEY_CONFIG = 2,
    OPTIONS_ITEM_DEFAULT = 3,
    OPTIONS_ITEM_QUIT = 4,
    OPTIONS_ITEM_COUNT = 5,
};

// The key config screen's rows: the five actions a pad button can be
// assigned to (TitleInf::key_config), then the two commands.
enum KeyConfigItem
{
    KEY_CONFIG_SHOT = 0,
    KEY_CONFIG_BOMB = 1,
    KEY_CONFIG_RELEASE = 2,
    KEY_CONFIG_FOCUS = 3,
    KEY_CONFIG_PAUSE = 4,
    // Back to the current assignments.
    KEY_CONFIG_DEFAULT = 5,
    // Save and leave.
    KEY_CONFIG_QUIT = 6,
    KEY_CONFIG_COUNT = 7,
};

// TitleInf::menu_flags.
enum TitleMenuFlags
{
    // Start the title BGM once bgm_start_delay reaches 10 frames.
    TITLE_START_BGM = 1 << 0,
    // The title screen is shown for the first time since startup.
    TITLE_FIRST_SHOW = 1 << 1,
    // Tells load_replay_list to stop.
    TITLE_STOP_REPLAY_LOADING = 1 << 2,
    // load_replay_list is done.
    TITLE_REPLAYS_LOADED = 1 << 3,
};

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
    // TitleState values, and the step within the state.
    i32 state;
    i32 prev_state;
    i32 substate;
    // The cursor of the screen that runs.
    MenuHelper menu;
    // The player data's difficulty.
    MenuHelper player_data_difficulty_menu;
    // The page shown by the player data's spell card list and the replay
    // menu.
    MenuHelper page_menu;
    ZunTimer time_in_state;
    // anm_ids[i] runs title.anm script i, mostly (create_effect).
    AnmId anm_ids[0x11f];
    // ascii.anm script 0x13, which the submenus start when they open and
    // remove when they go back to the title screen.
    AnmId submenu_ascii_id;
    // The text rows (text.anm scripts 3 and up) of the player data, the
    // replay list and the spell practice list.
    AnmId text_row_ids[0x24];
    // 0 to 7: the music room's comment lines; 8: the title_v.anm effect
    // of the title screen.
    AnmId comment_line_ids[9];
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
    // Set when the score did not make the top ten: no name entry.
    i32 score_not_ranked;
    // The name entry's character grid (91 characters, 13 to a row).
    MenuHelper name_entry_menu;
    // The key config being edited: the pad button for each
    // KeyConfigItem action.
    i16 key_config[6];
    u8 unk_5b40[0x5b44 - 0x5b40];
    // Cleared when the replay menu opens; never read.
    i32 unk_5b44;
    // The replay slot being saved to or played.
    i32 replay_slot;
    // The stage picked to start a replay from (minus one).
    i32 replay_stage;
    // The replay menu's list: the numbered slots, then the th16_ud????.rpy
    // files (load_replay_list).
    ReplayManager *replays[100];
    // musiccmt.txt, read while the music room is open.
    void *music_comment_file;
    // Frames since the title screen appeared, until its BGM starts.
    i32 bgm_start_delay;
    // TitleMenuFlags.
    u32 menu_flags;
    // Spell practice: the character.
    MenuHelper spell_character_menu;
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
    HARNESS_CALLED i32 on_draw__score_name_entry();
    HARNESS_CALLED i32 on_draw__replay_save();
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
    // 0x44b5f0 (ExpHP: MainMenu::do_title_screen).
    i32 do_title_screen();
    // 0x4560b0. Fills spell_ids (and their VMs) with the spell cards of a
    // stage's boss attack. The last argument is the same at every call
    // site; LTCG folded it.
    HARNESS_CALLED i32 load_spell_list(i32 stage, i32 row, i32 *ids, i32 selected);
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
// The difficulty names as the menus and the replay info show them.
extern const char *const g_difficulty_names[6];

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
        if (child->script_id_short == script || script == -1)
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
        if (vm->script_id_short == -2 && node->next == NULL)
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

