// States of the title screen's menus (TitleInf::on_tick dispatches on
// state).
#include <direct.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "MainMenu.h"

#include "EffectManager.h"
#include "FileSystem.h"
#include "GameWindow.h"
#include "Globals.h"
#include "HelpManual.h"
#include "Input.h"
#include "ReplayManager.h"
#include "SoundManager.h"
#include "Scorefile.h"
#include "Spellcard.h"
#include "StageData.h"
#include "Supervisor.h"

i32 __stdcall input_pressed_or_repeating(u32 mask);
extern i32 g_spell_practice_last_stage;
extern i32 g_practice_last_stage;
extern i32 g_spell_practice_last_row;
extern const char g_name_entry_chars[];
BOOL __stdcall spell_practice_row_seen(i32 stage, i32 row);
extern const i32 g_spell_practice_ids[7][13][5];

// input_pressed_or_repeating, inlined.
static __forceinline i32 pressed_or_repeating_inline(u32 mask)
{
    if (g_hardware_input_pressed & mask)
    {
        return 1;
    }
    if (g_hardware_input_repeat & mask)
    {
        return 1;
    }
    return 0;
}

// Small MenuHelper steps that some menus inline.
static __forceinline void menu_save_selection(MenuHelper *m)
{
    m->current_selection = m->next_selection;
}

static __forceinline i32 menu_selection_moved(MenuHelper *m)
{
    return m->current_selection != m->next_selection;
}

static_assert(offsetof(TitleInf, spell_character_menu) == 0x5cec, "TitleInf::spell_character_menu");
static_assert(offsetof(TitleInf, spell_stage) == 0x5dc4, "TitleInf::spell_stage");
static_assert(offsetof(TitleInf, spell_ids) == 0x5dd0, "TitleInf::spell_ids");

// ReplayManager::create_from_file as LTCG inlined it into the replay list
// loading.
static inline ReplayManager *create_replay_inline(const char *filename)
{
    ReplayManager *replay = new ReplayManager;
    replay->mode = REPLAY_LOADED;
    if (replay->read_replay_file(filename) != 0)
    {
        delete replay;
        return NULL;
    }
    return replay;
}

// Loads the numbered replays (th16_01.rpy to th16_25.rpy) and then the
// user replays from the replay directory, until stopped through
// TITLE_STOP_REPLAY_LOADING.
// FUNCTION: TH16 0x451560
void TitleInf::load_replay_list()
{
    char filename[0x40];
    WIN32_FIND_DATAA find_data;
    TitleInf *menu = g_MainMenu;

    for (i32 i = 1; i <= REPLAY_SLOTS; i++)
    {
        sprintf(filename, "th16_%.2d.rpy", i);
        menu->replays[i - 1] = create_replay_inline(filename);
        if (menu->menu_flags & TITLE_STOP_REPLAY_LOADING)
        {
            break;
        }
    }
    _chdir(g_GameWindow.save_dir);
    _chdir("replay");
    HANDLE find = FindFirstFileA("th16_ud????.rpy", &find_data);
    if (find != INVALID_HANDLE_VALUE)
    {
        for (i32 i = REPLAY_SLOTS; i < REPLAY_SLOTS + REPLAY_USER_SLOTS; i++)
        {
            _chdir(g_GameWindow.exe_dir);
            menu->replays[i] = ReplayManager::create_from_file(find_data.cFileName);
            _chdir(g_GameWindow.save_dir);
            _chdir("replay");
            if (menu->menu_flags & TITLE_STOP_REPLAY_LOADING)
            {
                break;
            }
            if (!FindNextFileA(find, &find_data))
            {
                break;
            }
        }
    }
    FindClose(find);
    _chdir(g_GameWindow.exe_dir);
    menu->menu_flags = (menu->menu_flags & ~TITLE_STOP_REPLAY_LOADING) | 8;
}

// Saving the replay after a game: picking a slot, then entering the name.
// The name entry reads the cursor into a local first: indexing
// replay_name with the member itself addresses [cursor + this] where the
// original has [this + cursor].
// TODO: the BOMB path's store still addresses [cursor + this], the last_replay_name copy folds 0x19f8c into the store's displacement (the original adds it to the pointer), and g_stage_table[8] lands on another global here.
// FUNCTION: TH16 0x453c10
i32 TitleInf::do_replay_save()
{
    char path[0x40];
    switch (substate)
    {
    case 0:
        menu.num_choices = REPLAY_SLOTS;
        menu.wraps = 1;
        menu.set_cursor(0);
        g_stage_data = &g_stage_table[8];
        g_Globals.stage_num = 8;
        g_Globals.weird_stage_num = 8;
        for (i32 i = 1; i <= REPLAY_SLOTS; i++)
        {
            sprintf(path, "th16_%.2d.rpy", i);
            replays[i - 1] = ReplayManager::create_from_file(path);
        }
        if (g_AnmManager->get_vm_with_id(anm_ids[0x61]) == NULL)
        {
            anm_ids[0x61] = title_anm->create_effect(0x61, -1, NULL);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
        }
        anm_ids[0x70] = title_anm->create_effect(0x70, -1, NULL);
        set_substate(1);
    case 1:
        if (time_in_state.current > 6)
        {
            set_substate(2);
        }
        break;
    case 2:
        menu.current_selection = menu.next_selection;
        if (pressed_or_repeating_inline(INPUT_UP))
        {
            menu.move_cursor(-1);
        }
        if (pressed_or_repeating_inline(INPUT_DOWN))
        {
            menu.move_cursor(1);
        }
        if (menu.current_selection != menu.next_selection)
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
        }
        else if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            replay_slot = menu.next_selection;
            name_entry_menu.set_cursor(0);
            name_entry_menu.num_choices = NAME_ENTRY_CHOICES;
            name_entry_menu.wraps = 1;
            g_ReplayManager->set_end_stage(1);
            strcpy(replay_name, g_Scorefile->last_replay_name);
            replay_name_cursor = 0;
            if (strcmp(replay_name, "        ") != 0)
            {
                name_entry_menu.move_cursor(-1);
            }
            i32 len;
            for (len = 8; len > 0 && replay_name[len - 1] == ' '; len--)
            {
            }
            replay_name_cursor = len;
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            set_substate(3);
        }
        break;
    case 3:
        menu_save_selection(&name_entry_menu);
        if (pressed_or_repeating_inline(INPUT_UP))
        {
            name_entry_menu.move_cursor(-NAME_ENTRY_COLUMNS);
        }
        if (pressed_or_repeating_inline(INPUT_DOWN))
        {
            name_entry_menu.move_cursor(NAME_ENTRY_COLUMNS);
        }
        if (pressed_or_repeating_inline(INPUT_LEFT))
        {
            i32 selection = name_entry_menu.next_selection;
            if (selection % NAME_ENTRY_COLUMNS != 0)
            {
                name_entry_menu.move_cursor(-1);
            }
            else
            {
                name_entry_menu.move_cursor(NAME_ENTRY_COLUMNS - 1);
            }
        }
        if (pressed_or_repeating_inline(INPUT_RIGHT))
        {
            i32 selection = name_entry_menu.next_selection;
            if (selection % NAME_ENTRY_COLUMNS != NAME_ENTRY_COLUMNS - 1)
            {
                name_entry_menu.move_cursor(1);
            }
            else
            {
                name_entry_menu.move_cursor(-(NAME_ENTRY_COLUMNS - 1));
            }
        }
        if (menu_selection_moved(&name_entry_menu))
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            i32 choice = name_entry_menu.next_selection;
            if (choice < NAME_ENTRY_CHAR_COUNT)
            {
                i32 cursor = replay_name_cursor;
                if (cursor < 8)
                {
                    replay_name[cursor] = g_name_entry_chars[choice];
                    replay_name_cursor++;
                    if (replay_name_cursor >= 8)
                    {
                        name_entry_menu.set_cursor(NAME_ENTRY_END);
                    }
                }
                else
                {
                    replay_name[cursor - 1] = g_name_entry_chars[choice];
                }
            }
            else if (choice == NAME_ENTRY_SPACE)
            {
                i32 cursor = replay_name_cursor;
                if (cursor < 8)
                {
                    replay_name[cursor] = ' ';
                    replay_name_cursor++;
                    if (replay_name_cursor >= 8)
                    {
                        name_entry_menu.set_cursor(NAME_ENTRY_END);
                    }
                }
                else
                {
                    replay_name[cursor - 1] = ' ';
                }
            }
            else if (choice == NAME_ENTRY_BACKSPACE)
            {
                i32 cursor = replay_name_cursor;
                if (cursor == 0)
                {
                    break;
                }
                cursor--;
                replay_name_cursor = cursor;
                replay_name[cursor] = ' ';
            }
            else if (choice == NAME_ENTRY_END)
            {
                g_SoundManager.play_sound_centered(SE_EXTEND, 0);
                sprintf(path, "th16_%.2d.rpy", menu.next_selection + 1);
                ReplayManager::destroy(replays[menu.next_selection]);
                g_ReplayManager->save(path, replay_name, 0, 0);
                // Read before the call: the original keeps the slot in esi
                // across it, which leaves edi for this.
                i32 slot = menu.next_selection;
                replays[slot] = ReplayManager::create_from_file(path);
                strcpy(g_Scorefile->last_replay_name, replay_name);
                set_substate(2);
            }
            g_SoundManager.play_sound_centered(SE_OK00, 0);
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            if (replay_name_cursor == 0)
            {
                set_substate(2);
                break;
            }
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            replay_name_cursor--;
            replay_name[replay_name_cursor] = ' ';
        }
        break;
    case 4:
        if (time_in_state.current >= 6)
        {
            AnmManager::interrupt_tree(anm_ids[0x70], 1);
            anm_ids[0x70].id = 0;
            AnmManager::interrupt_tree(submenu_ascii_id, 1);
            submenu_ascii_id.id = 0;
            set_state(TITLE_STATE_MAIN);
            menu.pop();
            ReplayManager::destroy(g_ReplayManager);
            g_Supervisor.play_bgm_wav(0, "th16_01");
            g_Supervisor.play_bgm(0, 0);
            for (i32 i = 0; i < REPLAY_SLOTS; i++)
            {
                delete replays[i];
            }
            memset(replays, 0, sizeof(replays));
        }
        break;
    }
    return 1;
}

// The difficulty last picked (Normal at first), where the menu starts.
// GLOBAL: TH16 0x49f274
i32 g_last_difficulty = DIFFICULTY_NORMAL;
// The character last picked, where the character select starts.
// GLOBAL: TH16 0x4a6f24
i32 g_last_character;

// Picking the difficulty, or confirming Extra.
// TODO: in case 4's main game branch the original reloads menu.next_selection as [esi + 0x24] after the g_Globals store; ours through ecx, the menu pointer for pop.
// FUNCTION: TH16 0x44fe20
i32 TitleInf::do_difficulty_select()
{
    i32 script = (g_Globals.difficulty >= DIFFICULTY_EXTRA) + 0x7e;
    switch (substate)
    {
    case 0:
        if (submenu_ascii_id.id == 0)
        {
            submenu_ascii_id = g_AsciiManager->get_anm()->create_effect(0x13, -1, NULL);
        }
        // num_choices before wraps: the original reuses the difficulty
        // loaded on entry (reloading it into ecx after the ascii effect).
        menu.num_choices = g_Globals.difficulty < DIFFICULTY_EXTRA ? 4 : 1;
        menu.wraps = 0;
        AnmManager::interrupt_tree(anm_ids[script], 1);
        anm_ids[script].id = 0;
        anm_ids[script] = title_anm->create_effect(script, -1, NULL);
        AnmManager::interrupt_tree_and_run(anm_ids[script], 3);
        AnmManager::interrupt_tree(anm_ids[script], (i16)(menu.next_selection + 13));
        if (g_title_return_point == TITLE_RETURN_PRACTICE)
        {
            menu.set_cursor(g_Globals.difficulty);
            AnmManager::interrupt_tree_and_run(anm_ids[script], 3);
            AnmManager::interrupt_tree_and_run(anm_ids[script], (i16)(menu.next_selection + 7));
            AnmManager::interrupt_tree_and_run(anm_ids[script], 6);
            AnmManager::interrupt_tree(find_child_id(script, menu.next_selection + 0x74), 2);
            goto confirm;
        }
        anm_ids[0x68] = title_anm->create_effect(0x68, -1, NULL);
        set_substate(1);
        if (g_Globals.difficulty < DIFFICULTY_EXTRA)
        {
            if (!g_Scorefile->all_cleared(DIFFICULTY_EASY))
            {
                find_child_id(script, 0x9e).hide_tree();
            }
            if (!g_Scorefile->all_cleared(DIFFICULTY_NORMAL))
            {
                find_child_id(script, 0x9f).hide_tree();
            }
            if (!g_Scorefile->all_cleared(DIFFICULTY_HARD))
            {
                find_child_id(script, 0xa0).hide_tree();
            }
            if (!g_Scorefile->all_cleared(DIFFICULTY_LUNATIC))
            {
                find_child_id(script, 0xa1).hide_tree();
            }
        }
        else if (!g_Scorefile->all_cleared(DIFFICULTY_EXTRA))
        {
            find_child_id(script, 0xa2).hide_tree();
        }
    case 1:
        if (time_in_state.current > 6)
        {
            set_substate(2);
            return 1;
        }
        break;
    case 2:
        if (g_Globals.difficulty < DIFFICULTY_EXTRA)
        {
            menu.current_selection = menu.next_selection;
            if (input_pressed_or_repeating(INPUT_UP) || input_pressed_or_repeating(INPUT_LEFT))
            {
                menu.move_cursor(-1);
            }
            if (input_pressed_or_repeating(INPUT_DOWN) || input_pressed_or_repeating(INPUT_RIGHT))
            {
                menu.move_cursor(1);
            }
            if (menu.current_selection != menu.next_selection)
            {
                g_SoundManager.play_sound_centered(SE_SELECT00, 0);
                AnmManager::interrupt_tree_and_run(anm_ids[script], 3);
                AnmManager::interrupt_tree(anm_ids[script], (i16)(menu.next_selection + 7));
            }
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            interrupt_and_clear(script);
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            AnmManager::interrupt_tree(anm_ids[script], 6);
            if (g_Globals.difficulty < DIFFICULTY_EXTRA)
            {
                AnmManager::interrupt_tree(find_child_id(script, menu.next_selection + 0x74), 2);
            }
            else
            {
                AnmManager::interrupt_tree(find_child_id(script, 0x78), 2);
            }
            set_substate(3);
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            return 1;
        }
        break;
    case 3:
        if (time_in_state.current >= 14)
        {
            AnmManager::interrupt_tree(anm_ids[0x68], 1);
            anm_ids[0x68].id = 0;
        confirm:
            set_state(TITLE_STATE_CHARACTER_SELECT);
            if (g_Globals.difficulty < DIFFICULTY_EXTRA)
            {
                g_Globals.difficulty = menu.next_selection;
                g_last_difficulty = g_Globals.difficulty;
            }
            menu.push();
            menu.wraps = 1;
            menu.num_choices = 4;
            menu.set_cursor(g_last_character);
            g_Globals.character = g_last_character;
            return 1;
        }
        break;
    case 4:
        if (time_in_state.current >= 6)
        {
            AnmManager::interrupt_tree(anm_ids[0x68], 1);
            anm_ids[0x68].id = 0;
            if (g_Globals.game_mode != GAME_MODE_NORMAL)
            {
                g_last_difficulty = menu.next_selection;
                g_Globals.difficulty = g_last_difficulty;
            }
            else if (g_Globals.difficulty < DIFFICULTY_EXTRA)
            {
                set_state(TITLE_STATE_MAIN);
                g_Globals.difficulty = menu.next_selection;
                g_last_difficulty = menu.next_selection;
                menu.pop();
                return 1;
            }
            else
            {
                g_Globals.difficulty = g_last_difficulty;
            }
            set_state(TITLE_STATE_MAIN);
            AnmManager::interrupt_tree(submenu_ascii_id, 1);
            submenu_ascii_id.id = 0;
            set_state(TITLE_STATE_MAIN);
            menu.pop();
        }
        break;
    }
    return 1;
}

// AnmId::hide_tree as LTCG inlined it here.
static __forceinline void hide_tree_inline(AnmId id)
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    if (vm != NULL)
    {
        vm->hide_tree_inline();
    }
}

// Picking the character. Extra only offers the characters that cleared the
// main game; characters marked as cleared on this difficulty get a badge.
// TODO: the original keeps script in edi for the clear badges (ours keeps g_AnmManager there) and has the run(3) call twice, the copies cross-jumped; written twice, ours keeps &anm_ids[script] in a register for the whole state.
// FUNCTION: TH16 0x4502c0
i32 TitleInf::do_character_select()
{
    i32 script = g_Globals.difficulty == DIFFICULTY_EXTRA ? 0x98 : 0x96;
    switch (substate)
    {
    case 0:
    {
        menu.num_choices = 4;
        if (g_Globals.difficulty == DIFFICULTY_EXTRA)
        {
            if (!g_Scorefile->has_cleared(menu.next_selection))
            {
                for (i32 i = 0; i < 4; i++)
                {
                    if (g_Scorefile->has_cleared(i))
                    {
                        menu.set_cursor(i);
                        break;
                    }
                }
            }
            for (i32 i = 0; i < 4; i++)
            {
                if (!g_Scorefile->has_cleared_inline(i))
                {
                    menu.disable(i);
                }
            }
        }
        if (g_AnmManager->get_vm_with_id(anm_ids[0x69]) == NULL)
        {
            anm_ids[0x69] = title_anm->create_effect(0x69, -1, NULL);
        }
        if (g_AnmManager->get_vm_with_id(anm_ids[script]) == NULL)
        {
            AnmManager::interrupt_tree(anm_ids[script], 1);
            anm_ids[script].id = 0;
            anm_ids[script] = title_anm->create_effect(script, -1, NULL);
        }
        AnmManager::interrupt_tree_and_run(anm_ids[script], 3);
        AnmManager::interrupt_tree(anm_ids[script], (i16)(menu.next_selection + 7));
        set_substate(1);
        if (g_Scorefile->characters[0].clears[g_Globals.difficulty] == 0)
        {
            hide_tree_inline(find_child_id_inline(anm_ids[script], 0x9a));
        }
        if (g_Scorefile->characters[1].clears[g_Globals.difficulty] == 0)
        {
            hide_tree_inline(find_child_id_inline(anm_ids[script], 0x9b));
        }
        if (g_Scorefile->characters[2].clears[g_Globals.difficulty] == 0)
        {
            hide_tree_inline(find_child_id_inline(anm_ids[script], 0x9c));
        }
        if (g_Scorefile->characters[3].clears[g_Globals.difficulty] == 0)
        {
            hide_tree_inline(find_child_id_inline(anm_ids[script], 0x9d));
        }
        if (g_title_return_point == TITLE_RETURN_PRACTICE)
        {
            menu.set_cursor(g_Globals.character);
            AnmManager::interrupt_tree_and_run(anm_ids[script], (i16)(menu.next_selection + 7));
            AnmManager::interrupt_tree(find_child_id(script, menu.next_selection + 0x80), 6);
            AnmManager::interrupt_tree(find_child_id(script, menu.next_selection + 0x88), 6);
            AnmManager::interrupt_tree(find_child_id_inline(anm_ids[script], 0x58), 6);
            AnmManager::interrupt_tree(find_child_id_inline(anm_ids[script], 0x59), 6);
            goto confirm;
        }
    }
    case 1:
        if (time_in_state.current > 6)
        {
            set_substate(2);
            return 1;
        }
        break;
    case 2:
        menu.current_selection = menu.next_selection;
        if (pressed_or_repeating_inline(INPUT_LEFT))
        {
            AnmManager::interrupt_tree(anm_ids[0x5c], 1);
            anm_ids[0x5c].id = 0;
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[script], (i16)(menu.next_selection + 25));
            menu.move_cursor(-1);
            AnmManager::interrupt_tree(anm_ids[script], (i16)(menu.next_selection + 13));
        }
        if (pressed_or_repeating_inline(INPUT_RIGHT))
        {
            AnmManager::interrupt_tree(anm_ids[0x5c], 1);
            anm_ids[0x5c].id = 0;
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[script], (i16)(menu.next_selection + 19));
            menu.move_cursor(1);
            AnmManager::interrupt_tree(anm_ids[script], (i16)(menu.next_selection + 7));
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            AnmManager::interrupt_tree(find_child_id(script, menu.next_selection + 0x80), 6);
            AnmManager::interrupt_tree(find_child_id(script, menu.next_selection + 0x88), 6);
            AnmManager::interrupt_tree(find_child_id(script, 0x58), 6);
            AnmManager::interrupt_tree(find_child_id(script, 0x59), 6);
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            set_substate(3);
            return 1;
        }
        break;
    case 3:
        if (time_in_state.current >= 14)
        {
        confirm:
            set_state(TITLE_STATE_SUBSEASON_SELECT);
            g_Globals.character = menu.next_selection;
            g_last_character = g_Globals.character;
            // MenuHelper::push, clearing num_disabled last.
            menu.stack_selection[menu.stack_depth] = menu.next_selection;
            menu.stack_num_choices[menu.stack_depth] = menu.num_choices;
            menu.stack_depth++;
            if (menu.stack_depth >= 0x10)
            {
                menu.stack_depth = 0xf;
            }
            menu.num_disabled = 0;
            menu.wraps = 1;
            menu.num_choices = 5;
            menu.set_cursor(g_Globals.character);
            AnmManager::interrupt_tree(anm_ids[script], 6);
            AnmManager::interrupt_tree(anm_ids[0x69], 1);
            anm_ids[0x69].id = 0;
            return 1;
        }
        break;
    case 4:
        if (time_in_state.current >= 6)
        {
            interrupt_and_clear(script);
            AnmManager::interrupt_tree(anm_ids[0x69], 1);
            anm_ids[0x69].id = 0;
            set_state(TITLE_STATE_DIFFICULTY_SELECT);
            g_Globals.character = menu.next_selection;
            menu.pop();
            g_last_character = g_Globals.character;
        }
        break;
    }
    return 1;
}

// Stage names for the practice and replay menus, by stage number
// (PauseMenu.cpp).
extern const char *g_stage_names[10];

// Picking the subseason before a game (Extra has only one). In stage
// practice this goes on to the stage select instead of starting.
// FUNCTION: TH16 0x450af0
i32 TitleInf::do_subseason_select()
{
    // A ternary: written as (difficulty == EXTRA) * 2 + 0x97, script stays
    // in esi; this way the original's spill to a stack slot comes back.
    i32 script = g_Globals.difficulty == DIFFICULTY_EXTRA ? 0x99 : 0x97;
    switch (substate)
    {
    case 0:
        menu.num_choices = 4;
        if (g_Globals.difficulty == DIFFICULTY_EXTRA)
        {
            menu.set_cursor(0);
            menu.num_choices = 1;
        }
        AnmManager::interrupt_tree(anm_ids[script], 1);
        anm_ids[script].id = 0;
        anm_ids[script] = title_anm->create_effect(script, -1, NULL);
        AnmManager::interrupt_tree_and_run(anm_ids[script], 3);
        AnmManager::interrupt_tree_and_run(anm_ids[script], (i16)(menu.next_selection + 7));
        AnmManager::interrupt_tree_and_run(anm_ids[script], (i16)(g_Globals.character + 31));
        set_substate(1);
        anm_ids[0x6a] = title_anm->create_effect(0x6a, -1, NULL);
        if (g_title_return_point == TITLE_RETURN_PRACTICE)
        {
            menu.set_cursor(g_Globals.subseason);
            goto confirm;
        }
    case 1:
        if (time_in_state.current > 6)
        {
            set_substate(2);
            return 1;
        }
        break;
    case 2:
        menu.current_selection = menu.next_selection;
        if (g_Globals.difficulty != DIFFICULTY_EXTRA)
        {
            if (input_pressed_or_repeating(INPUT_LEFT))
            {
                g_SoundManager.play_sound_centered(SE_SELECT00, 0);
                AnmManager::interrupt_tree_and_run(anm_ids[script], (i16)(menu.next_selection + 25));
                menu.move_cursor(-1);
                AnmManager::interrupt_tree(anm_ids[script], (i16)(menu.next_selection + 13));
            }
            if (input_pressed_or_repeating(INPUT_RIGHT))
            {
                g_SoundManager.play_sound_centered(SE_SELECT00, 0);
                AnmManager::interrupt_tree_and_run(anm_ids[script], (i16)(menu.next_selection + 19));
                menu.move_cursor(1);
                AnmManager::interrupt_tree(anm_ids[script], (i16)(menu.next_selection + 7));
            }
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            set_substate(3);
            g_SoundManager.play_sound_centered(SE_BOON00, 0);
            if (g_Globals.game_mode == GAME_MODE_NORMAL)
            {
                g_Supervisor.fade_out_bgm(0.05f);
                return 1;
            }
        }
        break;
    case 3:
        if (time_in_state.current == 10)
        {
            // The goto start at the end of this branch puts the now loading
            // block before confirm, like the original.
            if (g_Globals.game_mode == GAME_MODE_NORMAL)
            {
                g_AsciiManager->show_now_loading(480.0f, 392.0f);
                AnmId id;
                id = g_EffectManager->create_ui_effect(EFFECT_MASKED, NULL, NULL);
                g_Supervisor.config.loading_effect_id = id.id;
                AnmManager::interrupt_tree(id, 7);
                goto start;
            }
            goto confirm;
        }
        goto start;
    confirm:
        g_Globals.subseason = menu.next_selection;
        menu.push();
        AnmManager::interrupt_tree(anm_ids[script], 1);
        anm_ids[script].id = 0;
        AnmManager::interrupt_tree(anm_ids[0x6a], 1);
        anm_ids[0x6a].id = 0;
        set_state(TITLE_STATE_PRACTICE_STAGE_SELECT);
    start:
        if (time_in_state.current >= 40)
        {
            if (g_Globals.difficulty != DIFFICULTY_EXTRA)
            {
                g_Globals.subseason = menu.next_selection;
            }
            else
            {
                g_Globals.subseason = SUBSEASON_DOYOU;
            }
            menu.push();
            g_Globals.spell_id = -1;
            set_state(TITLE_STATE_EXIT);
            // The shared tail after the if/else is duplicated into both
            // branches; written in each, its stores were hoisted above the
            // test instead.
            if (g_Globals.difficulty < DIFFICULTY_EXTRA)
            {
                g_stage_data = &g_stage_table[1];
                g_Globals.stage_num = 1;
                g_Globals.weird_stage_num = 1;
            }
            else
            {
                g_stage_data = &g_stage_table[7];
                g_Globals.stage_num = 7;
                g_Globals.weird_stage_num = 7;
            }
            g_Supervisor.gamemode_to_switch_to = GAMEMODE_GAME;
            return 1;
        }
        break;
    case 4:
        if (time_in_state.current >= 6)
        {
            g_Globals.subseason = menu.next_selection;
            interrupt_and_clear(script);
            AnmManager::interrupt_tree(anm_ids[0x6a], 1);
            anm_ids[0x6a].id = 0;
            set_state(TITLE_STATE_CHARACTER_SELECT);
            menu.pop();
        }
        break;
    }
    return 1;
}

// The keyboard state the practice menu reads when a stage is picked.
// GLOBAL: TH16 0x4dfa48
u8 g_practice_keys[0x100];
// The number key (1 to 9) held when a practice stage is picked, 0 for
// none: the practice then starts with that many lives minus one instead
// of 9 (GameThread).
// GLOBAL: TH16 0x4a5bf8
i32 g_practice_lives_key;

// Stage practice: picking the stage.
// TODO: the constant 1 (the cmov and every return 1) lives in esi for the whole function; the original rematerializes it into edx at each use (its esi push is left unused) and stores lives_key = 1 as an immediate.
// FUNCTION: TH16 0x450ef0
i32 TitleInf::do_practice_stage_select()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = 6;
        menu.set_cursor(g_practice_last_stage);
        anm_ids[0x71] = title_anm->create_effect(0x71, -1, NULL);
        set_substate(1);
        if (g_title_return_point == TITLE_RETURN_PRACTICE)
        {
            g_title_return_point = TITLE_RETURN_MAIN;
        }
    case 1:
        if (time_in_state.current > 10)
        {
            set_substate(2);
            return 1;
        }
        break;
    case 2:
        menu.current_selection = menu.next_selection;
        if (pressed_or_repeating_inline(INPUT_UP))
        {
            menu.move_cursor(-1);
        }
        if (pressed_or_repeating_inline(INPUT_DOWN))
        {
            menu.move_cursor(1);
        }
        if (menu.current_selection != menu.next_selection)
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            g_practice_last_stage = menu.next_selection;
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            if (!g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
                     .practices[g_Globals.difficulty][menu.next_selection]
                     .unlocked)
            {
                g_SoundManager.play_sound_centered(SE_INVALID, 0);
                return 1;
            }
            set_substate(3);
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            g_SoundManager.play_sound_centered(SE_BOON00, 0);
            g_practice_last_stage = menu.next_selection;
            g_practice_lives_key = 0;
            if (get_keyboard_state(g_practice_keys) == 0)
            {
                if (g_practice_keys['1'] & 0x80)
                {
                    g_practice_lives_key = 1;
                }
                else if (g_practice_keys['2'] & 0x80)
                {
                    g_practice_lives_key = 2;
                }
                else if (g_practice_keys['3'] & 0x80)
                {
                    g_practice_lives_key = 3;
                }
                else if (g_practice_keys['4'] & 0x80)
                {
                    g_practice_lives_key = 4;
                }
                else if (g_practice_keys['5'] & 0x80)
                {
                    g_practice_lives_key = 5;
                }
                else if (g_practice_keys['6'] & 0x80)
                {
                    g_practice_lives_key = 6;
                }
                else if (g_practice_keys['7'] & 0x80)
                {
                    g_practice_lives_key = 7;
                }
                else if (g_practice_keys['8'] & 0x80)
                {
                    g_practice_lives_key = 8;
                }
                else if (g_practice_keys['9'] & 0x80)
                {
                    g_practice_lives_key = 9;
                }
            }
            else
            {
                if (g_practice_keys[DIK_1] & 0x80)
                {
                    g_practice_lives_key = 1;
                }
                else if (g_practice_keys[DIK_2] & 0x80)
                {
                    g_practice_lives_key = 2;
                }
                else if (g_practice_keys[DIK_3] & 0x80)
                {
                    g_practice_lives_key = 3;
                }
                else if (g_practice_keys[DIK_4] & 0x80)
                {
                    g_practice_lives_key = 4;
                }
                else if (g_practice_keys[DIK_5] & 0x80)
                {
                    g_practice_lives_key = 5;
                }
                else if (g_practice_keys[DIK_6] & 0x80)
                {
                    g_practice_lives_key = 6;
                }
                else if (g_practice_keys[DIK_7] & 0x80)
                {
                    g_practice_lives_key = 7;
                }
                else if (g_practice_keys[DIK_8] & 0x80)
                {
                    g_practice_lives_key = 8;
                }
                else if (g_practice_keys[DIK_9] & 0x80)
                {
                    g_practice_lives_key = 9;
                }
            }
            g_Supervisor.fade_out_bgm(0.05f);
            return 1;
        }
        break;
    case 3:
        if (time_in_state.current == 10)
        {
            g_AsciiManager->show_now_loading(480.0f, 392.0f);
            AnmId id;
            id = g_EffectManager->create_ui_effect(EFFECT_MASKED, NULL, NULL);
            g_Supervisor.config.loading_effect_id = id.id;
            AnmManager::interrupt_tree(id, 7);
        }
        if (time_in_state.current >= 40)
        {
            menu.push();
            set_state(TITLE_STATE_EXIT);
            i32 stage = menu.next_selection + 1;
            g_Supervisor.gamemode_to_switch_to = GAMEMODE_GAME;
            // Stage table pointer first, for matching (see do_spell_practice_difficulty).
            g_stage_data = &g_stage_table[stage];
            g_Globals.stage_num = stage;
            g_Globals.weird_stage_num = stage;
            g_title_return_point = TITLE_RETURN_PRACTICE;
            g_practice_last_stage = menu.next_selection;
            return 1;
        }
        break;
    case 4:
        if (time_in_state.current >= 6)
        {
            AnmManager::interrupt_tree(anm_ids[0x71], 1);
            anm_ids[0x71].id = 0;
            set_state(TITLE_STATE_SUBSEASON_SELECT);
            menu.pop();
        }
        break;
    }
    return 1;
}

// The unlock cheat typed on the player data screen ("ILOVEBEER", as
// DirectInput key codes).
// GLOBAL: TH16 0x4936d8
const i32 g_cheat_code[9] = {0x17, 0x26, 0x18, 0x2f, 0x12, 0x30, 0x12, 0x12, 0x13};
// The keys newly pressed this frame (also scratch space for remapping
// virtual keys to DirectInput key codes).
// GLOBAL: TH16 0x4df940
u8 g_cheat_keys_pressed[0x100];
// Frames since the last correct key, and how much of the code was typed.
// GLOBAL: TH16 0x4dfa40
i32 g_cheat_timer;
// GLOBAL: TH16 0x4dfa44
u32 g_cheat_progress;
// The keyboard this frame and the last.
// GLOBAL: TH16 0x4dfd60
u8 g_cheat_keys[0x100];
// GLOBAL: TH16 0x4dfe60
u8 g_cheat_prev_keys[0x100];

// The player data screen: difficulty (player_data_difficulty_menu) and character (menu)
// records, and pages of spell cards (page_menu, 0 for none). On Extra with
// the fourth character selected it also reads the unlock cheat.
// TODO: the vectorized OR loads the second 16 key bytes first (the original the first; not the operand order, the accumulator type or a reversed loop).
// FUNCTION: TH16 0x452330
i32 TitleInf::do_player_data()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = 4;
        menu.set_cursor(0);
        player_data_difficulty_menu.num_choices = 5;
        player_data_difficulty_menu.set_cursor(1);
        player_data_difficulty_menu.wraps = 1;
        page_menu.num_choices = (count_spells_of_difficulty(player_data_difficulty_menu.next_selection) + 9) / 10 + 1;
        page_menu.set_cursor(0);
        page_menu.wraps = 1;
        if (submenu_ascii_id.id == 0)
        {
            submenu_ascii_id = g_AsciiManager->get_anm()->create_effect(0x13, -1, NULL);
        }
        anm_ids[0x6d] = title_anm->create_effect(0x6d, -1, NULL);
        set_substate(1);
        create_effect(menu.next_selection + 0xa7);
        create_effect(player_data_difficulty_menu.next_selection + 0xaf);
        anm_ids[0xb7] = title_anm->create_effect(0xb7, -1, NULL);
        anm_ids[0xb8] = title_anm->create_effect(0xb8, -1, NULL);
        anm_ids[0xb9] = title_anm->create_effect(0xb9, -1, NULL);
        anm_ids[0xba] = title_anm->create_effect(0xba, -1, NULL);
        anm_ids[0xb4] = title_anm->create_effect(0xb4, -1, NULL);
        anm_ids[0xb5] = title_anm->create_effect(0xb5, -1, NULL);
        anm_ids[0xb6] = title_anm->create_effect(0xb6, -1, NULL);
        anm_ids[0xbb] = title_anm->create_effect(0xbb, -1, NULL);
    case 1:
        if (time_in_state.current > 6)
        {
            set_substate(2);
            return 1;
        }
        break;
    case 2:
        menu_save_selection(&menu);
        menu_save_selection(&player_data_difficulty_menu);
        menu_save_selection(&page_menu);
        if (pressed_or_repeating_inline(INPUT_UP))
        {
            player_data_difficulty_menu.move_cursor(-1);
            AnmManager::interrupt_tree_and_run(anm_ids[0xb9], 2);
        }
        if (pressed_or_repeating_inline(INPUT_DOWN))
        {
            player_data_difficulty_menu.move_cursor(1);
            AnmManager::interrupt_tree_and_run(anm_ids[0xba], 2);
        }
        if (menu_selection_moved(&player_data_difficulty_menu))
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            interrupt_and_clear(player_data_difficulty_menu.current_selection + 0xaf);
            create_effect(player_data_difficulty_menu.next_selection + 0xaf);
            if (page_menu.next_selection > 0)
            {
                page_menu.set_cursor(1);
                draw_spell_card_page();
            }
            page_menu.num_choices =
                (count_spells_of_difficulty(player_data_difficulty_menu.next_selection) + 9) / 10 + 1;
        }
        if (pressed_or_repeating_inline(INPUT_LEFT))
        {
            menu.move_cursor(-1);
            AnmManager::interrupt_tree_and_run(anm_ids[0xb7], 2);
        }
        if (pressed_or_repeating_inline(INPUT_RIGHT))
        {
            menu.move_cursor(1);
            AnmManager::interrupt_tree_and_run(anm_ids[0xb8], 2);
        }
        if (menu_selection_moved(&menu))
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            interrupt_and_clear(menu.current_selection + 0xa7);
            create_effect(menu.next_selection + 0xa7);
            if (page_menu.next_selection > 0)
            {
                draw_spell_card_page();
            }
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            if (page_menu.next_selection == 0)
            {
                for (i32 i = 0; i < 10; i++)
                {
                    text_row_ids[i] = g_Supervisor.text_anm->create_effect(i + 3, -1, NULL);
                }
            }
            page_menu.move_cursor(1);
            if (page_menu.next_selection == 0)
            {
                for (i32 i = 0; i < 10; i++)
                {
                    AnmManager::interrupt_tree(text_row_ids[i], 1);
                }
            }
            else
            {
                draw_spell_card_page();
            }
            g_SoundManager.play_sound_centered(SE_OK00, 0);
        }
        if (player_data_difficulty_menu.next_selection == 4 && menu.next_selection == 3)
        {
            if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_BOMB | INPUT_MENU | INPUT_ENTER))
            {
                g_cheat_progress = 0;
                g_cheat_timer = 0;
            }
            memcpy(g_cheat_prev_keys, g_cheat_keys, sizeof(g_cheat_keys));
            i32 source = get_keyboard_state(g_cheat_keys);
            if (source == 2)
            {
                // GetKeyboardState gives virtual keys; move the letters to
                // their DirectInput codes.
                u8 *remapped = g_cheat_keys_pressed;
                memset(remapped, 0, 0x100);
                remapped[DIK_A] = g_cheat_keys['A'];
                remapped[DIK_B] = g_cheat_keys['B'];
                remapped[DIK_C] = g_cheat_keys['C'];
                remapped[DIK_D] = g_cheat_keys['D'];
                remapped[DIK_E] = g_cheat_keys['E'];
                remapped[DIK_F] = g_cheat_keys['F'];
                remapped[DIK_G] = g_cheat_keys['G'];
                remapped[DIK_H] = g_cheat_keys['H'];
                remapped[DIK_I] = g_cheat_keys['I'];
                remapped[DIK_J] = g_cheat_keys['J'];
                remapped[DIK_K] = g_cheat_keys['K'];
                remapped[DIK_L] = g_cheat_keys['L'];
                remapped[DIK_M] = g_cheat_keys['M'];
                remapped[DIK_N] = g_cheat_keys['N'];
                remapped[DIK_O] = g_cheat_keys['O'];
                remapped[DIK_P] = g_cheat_keys['P'];
                remapped[DIK_Q] = g_cheat_keys['Q'];
                remapped[DIK_R] = g_cheat_keys['R'];
                remapped[DIK_S] = g_cheat_keys['S'];
                remapped[DIK_T] = g_cheat_keys['T'];
                remapped[DIK_U] = g_cheat_keys['U'];
                remapped[DIK_V] = g_cheat_keys['V'];
                remapped[DIK_W] = g_cheat_keys['W'];
                remapped[DIK_X] = g_cheat_keys['X'];
                remapped[DIK_Y] = g_cheat_keys['Y'];
                remapped[DIK_Z] = g_cheat_keys['Z'];
                memcpy(g_cheat_keys, remapped, 0x100);
            }
            else if (source != 1)
            {
                goto tick;
            }
            for (i32 i = 0; i < 0x100; i++)
            {
                g_cheat_keys_pressed[i] = (g_cheat_keys[i] ^ g_cheat_prev_keys[i]) & g_cheat_keys[i];
            }
            if (g_cheat_progress >= 9)
            {
                g_Scorefile->unlock_all();
                g_SoundManager.play_sound_centered(SE_EXTEND, 0);
                g_cheat_progress = 0;
            }
            else if ((i8)g_cheat_keys_pressed[g_cheat_code[g_cheat_progress]] < 0)
            {
                g_cheat_progress++;
                g_cheat_timer = 0;
            }
            else
            {
                i8 any = 0;
                for (i32 i = 0; i < DIK_SPACE; i++)
                {
                    any |= g_cheat_keys_pressed[i];
                }
                if (any < 0)
                {
                    g_cheat_progress = 0;
                }
            }
        tick:
            g_cheat_timer++;
            if (g_cheat_timer > 300)
            {
                g_cheat_progress = 0;
                g_cheat_timer = 0;
            }
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(3);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            interrupt_and_clear(player_data_difficulty_menu.next_selection + 0xaf);
            interrupt_and_clear(menu.next_selection + 0xa7);
            interrupt_and_clear(0xb7);
            interrupt_and_clear(0xb8);
            interrupt_and_clear(0xb9);
            interrupt_and_clear(0xba);
            interrupt_and_clear(0xb4);
            interrupt_and_clear(0xb5);
            interrupt_and_clear(0xb6);
            interrupt_and_clear(0xbb);
            for (i32 i = 0; i < 10; i++)
            {
                AnmManager::interrupt_tree(text_row_ids[i], 1);
            }
            return 1;
        }
        break;
    case 3:
        if (time_in_state.current >= 6)
        {
            interrupt_and_clear(0x6d);
            AnmManager::interrupt_tree(submenu_ascii_id, 1);
            submenu_ascii_id.id = 0;
            set_state(TITLE_STATE_MAIN);
            menu.pop();
        }
        break;
    }
    return 1;
}

// Inline nodes between draw_spell_card_page and the variadic
// AnmManager::draw_text_centered, which realigns its own frame. Called
// directly, the call makes LTCG realign the page's frame too (early
// alignment, see docs/findings.md); the original's does not.
static inline void spell_page_text(AnmVm *vm, D3DCOLOR color, const char *text)
{
    g_AnmManager->draw_text_centered(vm, color, 0, 0, 0, text);
}
static inline void spell_page_seen(AnmVm *vm, D3DCOLOR color, const char *hundreds, const char *tens,
                                   const char *ones, const char *name, i32 captures, i32 attempts)
{
    g_AnmManager->draw_text_centered(vm, color, 0, 0, 0, "No.%s%s%s %s %4d/%4d", hundreds, tens, ones, name, captures,
                                     attempts);
}
static inline void spell_page_unseen(AnmVm *vm, const char *hundreds, const char *tens, const char *ones,
                                     i32 captures, i32 attempts)
{
    g_AnmManager->draw_text_centered(
        vm, 0x808080, 0, 0, 0,
        "No.%s%s%s \x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H"
        "\x81H\x81H\x81H\x81H %4d/%4d",
        hundreds, tens, ones, captures, attempts);
}

// Player data, spell card page: ten spell cards of the chosen difficulty
// (page page_menu - 1), numbered with full-width digits, with their names
// once seen and the chosen character's captures.
// The text goes through the inline spell_page_* nodes above so that the
// frame is not realigned.
// TODO: ours builds the spell pointer (the original keeps the character's base in a stack slot and indexes it with id * 0x9c per access) and looks up the ones digit before the call (the original keeps id % 10 and indexes at the push).
// FUNCTION: TH16 0x452c30
i32 TitleInf::draw_spell_card_page()
{
    i32 skip = page_menu.next_selection * 10 - 10;
    i32 id = 0;
    for (i32 seen = 0; seen < skip; id++)
    {
        if (g_spell_difficulty[id] == player_data_difficulty_menu.next_selection)
        {
            seen++;
        }
    }
    const char *digits[10] = {"\x82\x4f", "\x82\x50", "\x82\x51", "\x82\x52", "\x82\x53",
                              "\x82\x54", "\x82\x55", "\x82\x56", "\x82\x57", "\x82\x58"};
    char name[0xa5];
    // A variable, not the literal: the original divides by 10 in a
    // register (one idiv for id % 10 and id / 10), where a constant
    // divisor gets the reciprocal multiply.
    i32 ten = 10;
    AnmId *row_id = text_row_ids;
    for (i32 row = 0; row < 10; row++, row_id++)
    {
        for (; id < 0x77; id++)
        {
            if (g_spell_difficulty[id] == player_data_difficulty_menu.next_selection)
            {
                break;
            }
        }
        if (id >= 0x77)
        {
            for (; row < 10; row++)
            {
                spell_page_text(get_vm_or_clear(text_row_ids[row]), 0xffffffff, " ");
            }
            return 0;
        }
        if (g_Scorefile->characters[4].spells[id].attempts[0] != 0)
        {
            strcpy(name, g_Scorefile->characters[4].spells[id].name);
            i32 len = strlen(name);
            while (len < 42)
            {
                strcpy(&name[len], "\x81\x40");
                len += 2;
            }
            name[len] = '\0';
            i32 number = id + 1;
            const char *ones = digits[number % ten];
            const char *tens =
                number / ten % ten == 0 && number / 100 == 0 ? "\x81\x40" : digits[number / ten % ten];
            const char *hundreds = number / 100 != 0 ? digits[number / 100] : "\x81\x40";
            ScorefileSpell *spell = &g_Scorefile->characters[menu.next_selection].spells[id];
            spell_page_seen(get_vm_or_clear(*row_id), spell->captures[0] != 0 ? 0xffff80 : 0xefefef, hundreds, tens, ones,
                            name, spell->captures[0], spell->attempts[0]);
        }
        else
        {
            i32 number = id + 1;
            const char *ones = digits[number % ten];
            const char *tens =
                number / ten % ten == 0 && number / 100 == 0 ? "\x81\x40" : digits[number / ten % ten];
            const char *hundreds = number / 100 != 0 ? digits[number / 100] : "\x81\x40";
            ScorefileSpell *spell = &g_Scorefile->characters[menu.next_selection].spells[id];
            spell_page_unseen(get_vm_or_clear(*row_id), hundreds, tens, ones, spell->captures[0], spell->attempts[0]);
        }
        id++;
    }
    return 0;
}

// The stages and practice high scores of stage practice.
// FUNCTION: TH16 0x4513c0
HARNESS_CALLED i32 TitleInf::on_draw__practice_stage_select()
{
    switch (substate)
    {
    case 2:
    case 3:
    {
        Float3 pos(80.0f, 80.0f, 0.0f);
        g_AsciiManager->draw_shadows = 1;
        if (time_in_state.current >= 10 || substate == 3)
        {
            pos.x = 240.0f;
            pos.y = 192.0f;
            for (i32 stage = 1; stage < 7; stage++)
            {
                Scorefile *scorefile = g_Scorefile;
                if (menu.next_selection == stage - 1)
                {
                    if (scorefile->characters[g_Globals.subshot + g_Globals.character]
                            .practices[g_Globals.difficulty][stage - 1]
                            .unlocked)
                    {
                        if (substate == 3 && time_in_state.current % 4 >= 2)
                        {
                            g_AsciiManager->color.d3d = 0xff000000;
                        }
                        else
                        {
                            g_AsciiManager->color.d3d = 0xffffff00;
                        }
                    }
                    else
                    {
                        g_AsciiManager->color.d3d = 0xffdfdfdf;
                    }
                }
                else
                {
                    g_AsciiManager->color.d3d = 0xff808080;
                }
                ScorefilePractice *practice =
                    &scorefile->characters[g_Globals.subshot + g_Globals.character].practices[g_Globals.difficulty][stage - 1];
                if (!practice->unlocked)
                {
                    g_AsciiManager->create_stringf(&pos, "%s  ---------", g_stage_names[stage]);
                }
                else
                {
                    g_AsciiManager->create_stringf(&pos, "%s  %.8d0", g_stage_names[stage], practice->score);
                }
                pos.y += 18.0f;
            }
        }
        g_AsciiManager->color.d3d = 0xffffffff;
        g_AsciiManager->draw_shadows = 0;
        break;
    }
    }
    return 1;
}

// The capture history of the listed spell cards in spell practice (its own
// and the main game's).
// FUNCTION: TH16 0x456d50
HARNESS_CALLED i32 TitleInf::on_draw__spell_practice_histories()
{
    if (substate > 0 && (substate <= 2 || (substate == 3 && state != TITLE_STATE_SPELL_PRACTICE_DIFFICULTY_SELECT)))
    {
        Float3 pos;
        pos.x = 330.0f;
        pos.y = 191.0f;
        pos.z = 0.0f;
        g_AsciiManager->font_id = 2;
        g_AsciiManager->draw_shadows = 1;
        for (i32 i = 0; i < 5; i++)
        {
            i32 id = spell_ids[i];
            if (id >= -1)
            {
                if (state == TITLE_STATE_SPELL_PRACTICE_DIFFICULTY_SELECT && i == menu.next_selection)
                {
                    g_AsciiManager->color.d3d =
                        g_Scorefile->characters[spell_character_menu.next_selection].spells[id].captures[1] != 0
                            ? 0xff90d0ff
                            : 0xffb0b0b0;
                }
                else
                {
                    g_AsciiManager->color.d3d =
                        g_Scorefile->characters[spell_character_menu.next_selection].spells[id].captures[1] != 0
                            ? 0xff60a0c0
                            : 0xff404040;
                }
                if (g_Scorefile->characters[4].spells[id].attempts[0] == 0 &&
                    g_Scorefile->characters[4].spells[id].attempts[1] == 0)
                {
                    g_AsciiManager->create_stringf(&pos, "SCORE        00  ----/----");
                }
                else
                {
                    ScorefileSpell *spell = &g_Scorefile->characters[spell_character_menu.next_selection].spells[id];
                    g_AsciiManager->create_stringf(&pos, "SCORE %8d0  %4d/%4d", spell->practice_score,
                                                   spell->captures[1], spell->attempts[1]);
                    pos.y += 10.0f;
                    g_AsciiManager->color.d3d =
                        g_Scorefile->characters[spell_character_menu.next_selection].spells[id].captures[0] != 0
                            ? 0xff206060
                            : 0xff404040;
                    if (g_spell_difficulty[id] <= 4)
                    {
                        spell = &g_Scorefile->characters[spell_character_menu.next_selection].spells[id];
                        g_AsciiManager->create_stringf(&pos, "GAME MODE        %4d/%4d", spell->captures[0],
                                                       spell->attempts[0]);
                    }
                    pos.y -= 10.0f;
                }
            }
            pos.y += 44.0f;
        }
        g_AsciiManager->font_id = 0;
        g_AsciiManager->draw_shadows = 0;
        g_AsciiManager->color.d3d = 0xffffffff;
    }
    return 1;
}

// Name tables of the replay lists. The short ones are defined with the
// pause menu's replay list (PauseMenu.cpp).
// GLOBAL: TH16 0x4918c0
const char *const g_season_names[5] = {"Spring", "Summer", "Autumn", "Winter", "Full  "};
// GLOBAL: TH16 0x491950
const char *const g_difficulty_names[6] = {"Easy   ", "Normal ", "Hard   ", "Lunatic", "Extra  ", "O.D.   "};
extern const char *g_chara_names_short[4];
extern const char *g_stage_names_short[10];
// The characters of the name entry grid; the last three are drawn as the
// special glyphs 0x81, 0x7f and 0x80.
// GLOBAL: TH16 0x492840
const char g_name_entry_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-=.,!?@:;[]()_/{}|~^#$%&*   ";

// The replay slot last played, where the replay menu starts.
// GLOBAL: TH16 0x4a6f28
i32 g_last_replay_slot;

// The replay menu: picking a replay (pages of 25) while the list loads on
// the menu's thread, then the stage to start from.
// FUNCTION: TH16 0x451750
i32 TitleInf::do_replay_menu()
{
    switch (substate)
    {
    case 0:
    {
        menu.num_choices = REPLAY_SLOTS;
        i32 last = g_last_replay_slot;
        menu.set_cursor(last % REPLAY_SLOTS);
        page_menu.num_choices = 3;
        page_menu.set_cursor(last / REPLAY_SLOTS);
        page_menu.wraps = 1;
        g_last_replay_slot = 0;
        if (submenu_ascii_id.id == 0)
        {
            submenu_ascii_id = g_AsciiManager->get_anm()->create_effect(0x13, -1, NULL);
        }
        anm_ids[0x6c] = title_anm->create_effect(0x6c, -1, NULL);
        set_substate(1);
        memset(replays, 0, sizeof(replays));
        menu_flags &= ~(TITLE_STOP_REPLAY_LOADING | TITLE_REPLAYS_LOADED);
        unk_5b44 = 0;
        thread.restart((ThreadStart)replay_list_thread, this);
        if (g_AnmManager->get_vm_with_id(anm_ids[0x61]) == NULL)
        {
            anm_ids[0x61] = title_anm->create_effect(0x61, -1, NULL);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
        }
    }
    case 1:
        if (time_in_state.current > 6)
        {
            set_substate(2);
            return 1;
        }
        break;
    case 2:
        menu_save_selection(&menu);
        page_menu.current_selection = page_menu.next_selection;
        if (pressed_or_repeating_inline(INPUT_UP))
        {
            menu.move_cursor(-1);
        }
        if (pressed_or_repeating_inline(INPUT_DOWN))
        {
            menu.move_cursor(1);
        }
        if (pressed_or_repeating_inline(INPUT_LEFT))
        {
            page_menu.move_cursor(-1);
        }
        if (pressed_or_repeating_inline(INPUT_RIGHT))
        {
            page_menu.move_cursor(1);
        }
        if (menu_selection_moved(&page_menu))
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
        }
        if (menu_selection_moved(&menu))
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(5);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            menu_flags |= TITLE_STOP_REPLAY_LOADING;
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            if (replays[page_menu.next_selection * REPLAY_SLOTS + menu.next_selection] == NULL)
            {
                break;
            }
            set_substate(4);
            replay_slot = page_menu.next_selection * REPLAY_SLOTS + menu.next_selection;
            menu.push();
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            menu.num_choices = 7;
            menu.set_cursor(0);
            for (i32 i = 0; i < 7; i++)
            {
                if (replays[replay_slot]->stages[i + 1].gamestate_at_stage_begin == NULL)
                {
                    menu.disable(i);
                }
            }
            menu.move_cursor(-1);
            menu.move_cursor(1);
            if (replays[replay_slot]->info->flags_a & 2)
            {
                goto start;
            }
            return 1;
        }
        break;
    case 4:
        if (time_in_state.current >= 15)
        {
            menu_save_selection(&menu);
            if (input_pressed_or_repeating(INPUT_UP))
            {
                menu.move_cursor(-1);
            }
            if (input_pressed_or_repeating(INPUT_DOWN))
            {
                menu.move_cursor(1);
            }
            if (menu_selection_moved(&menu))
            {
                g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            }
            if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
            {
                menu.pop();
                menu.num_choices = REPLAY_SLOTS;
                menu.num_disabled = 0;
                set_substate(2);
                g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                return 1;
            }
            if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
            {
            start:
                replay_stage = menu.next_selection;
                set_substate(3);
                menu_flags |= TITLE_STOP_REPLAY_LOADING;
                g_SoundManager.play_sound_centered(SE_BOON00, 0);
                g_Supervisor.fade_out_bgm(0.05f);
                return 1;
            }
        }
        break;
    case 3:
        if (time_in_state.current == 2)
        {
            AnmId id;
            id = g_EffectManager->create_ui_effect(EFFECT_MASKED, NULL, NULL);
            g_Supervisor.config.loading_effect_id = id.id;
            AnmManager::interrupt_tree(id, 7);
            g_AsciiManager->show_now_loading(480.0f, 392.0f);
        }
        if (time_in_state.current >= 32 && (menu_flags & TITLE_REPLAYS_LOADED))
        {
            set_state(TITLE_STATE_EXIT);
            i32 stage = replay_stage + 1;
            g_Supervisor.gamemode_to_switch_to = GAMEMODE_START_REPLAY;
            // Stage table pointer first, for matching (see do_spell_practice_difficulty).
            g_stage_data = &g_stage_table[stage];
            g_Globals.stage_num = stage;
            g_Globals.weird_stage_num = stage;
            strcpy(g_current_replay_filename, replays[replay_slot]->filename);
            RpyInfo *info = replays[replay_slot]->info;
            g_Globals.character = info->character;
            g_Globals.subshot = info->subshot;
            g_Globals.subseason = info->subseason;
            g_Globals.difficulty = info->difficulty;
            if (info->flags_a & 2)
            {
                g_Globals.set_game_mode(GAME_MODE_SPELL_PRACTICE);
                g_Globals.spell_id = info->spell_id;
            }
            else
            {
                g_Globals.set_game_mode(GAME_MODE_NORMAL);
                g_Globals.spell_id = -1;
            }
            g_last_replay_slot = replay_slot;
            g_title_return_point = TITLE_RETURN_REPLAY_MENU;
            return 1;
        }
        break;
    case 5:
        if (time_in_state.current >= 6 && (menu_flags & TITLE_REPLAYS_LOADED))
        {
            for (i32 i = 0; i < 100; i++)
            {
                delete replays[i];
            }
            memset(replays, 0, sizeof(replays));
            AnmManager::interrupt_tree(anm_ids[0x6c], 1);
            anm_ids[0x6c].id = 0;
            AnmManager::interrupt_tree(submenu_ascii_id, 1);
            submenu_ascii_id.id = 0;
            set_state(TITLE_STATE_MAIN);
            menu.pop();
        }
        break;
    }
    return 1;
}

// Line formats of the replay list: numbered and user replays, each with
// and without a replay in the slot.
// GLOBAL: TH16 0x493700
const char *const g_replay_list_formats[2][2] = {
    {"No.%.2d %s %.2d/%.2d/%.2d %.2d:%.2d %s %s %s %s %2.1f%%",
     "No.%.2d -------- --/--/-- --:-- ------- ------ ------- --- ---%%"},
    {"%s  %s %.2d/%.2d/%.2d %.2d:%.2d %s %s %s %s %s %2.1f%%",
     "User  -------- --/--/-- --:-- ------- ------ ------- --- ---%%"},
};
// GLOBAL: TH16 0x493710
const char *const g_replay_spell_format[1] = {"No.%.2d %s %.2d/%.2d/%.2d %.2d:%.2d %s %s SpellPr %3d %2.1f%%"};

// The 4 characters after "th16_ud" in a user replay's file name.
// GLOBAL: TH16 0x4dfd54
char g_replay_user_number[5];

// The replay menu: a page of 25 replays, then the chosen replay with the
// score at the end of each stage.
// TODO: the empty slot format index: the original uses setne and a scaled index, ours neg/sbb/and.
// FUNCTION: TH16 0x451d50
HARNESS_CALLED i32 TitleInf::on_draw__replay()
{
    Float3 pos;
    switch (substate)
    {
    case 2:
    {
        pos.x = 32.0f;
        pos.y = 80.0f;
        pos.z = 0.0f;
        g_AsciiManager->draw_shadows = 1;
        for (i32 i = page_menu.next_selection * REPLAY_SLOTS; i < (page_menu.next_selection + 1) * REPLAY_SLOTS; i++)
        {
            ReplayManager **replay = &replays[i];
            g_AsciiManager->color.d3d = menu.next_selection == i % REPLAY_SLOTS ? 0xffffff00 : 0xff808080;
            if (*replay != NULL)
            {
                RpyInfo *info = (*replay)->info;
                struct tm *time = localtime(&info->timestamp);
                if (page_menu.next_selection == 0)
                {
                    if (!(info->flags_a & 2))
                    {
                        g_AsciiManager->create_stringf(
                            &pos, g_replay_list_formats[0][0], i + 1, info->name, time->tm_year % 100,
                            time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
                            g_chara_names_short[info->character + info->subshot], g_season_names[info->subseason],
                            g_difficulty_names[info->difficulty], g_stage_names_short[info->stage], info->slowdown);
                    }
                    else
                    {
                        g_AsciiManager->create_stringf(&pos, g_replay_spell_format[0], i + 1, info->name,
                                                       time->tm_year % 100, time->tm_mon + 1, time->tm_mday,
                                                       time->tm_hour, time->tm_min,
                                                       g_chara_names_short[info->character + info->subshot],
                                                       g_season_names[info->subseason], info->spell_id + 1,
                                                       info->slowdown);
                    }
                }
                else
                {
                    memcpy(g_replay_user_number, &(*replay)->filename[7], 4);
                    g_replay_user_number[4] = '\0';
                    g_AsciiManager->create_stringf(
                        &pos, g_replay_list_formats[1][0], g_replay_user_number, info->name,
                        time->tm_year % 100, time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
                        g_chara_names_short[info->character + info->subshot], g_difficulty_names[info->difficulty],
                        g_season_names[info->subseason], g_stage_names_short[info->stage], info->slowdown);
                }
            }
            else
            {
                g_AsciiManager->create_stringf(&pos, g_replay_list_formats[page_menu.next_selection != 0][1], i + 1);
            }
            pos.y += 15.0f;
        }
        g_AsciiManager->color.d3d = 0xffffffff;
        g_AsciiManager->draw_shadows = 0;
        break;
    }
    case 4:
    {
        pos.x = 32.0f;
        pos.y = 80.0f;
        pos.z = 0.0f;
        RpyInfo *info = replays[replay_slot]->info;
        if (time_in_state.current < 10)
        {
            pos.y = (10.0f - time_in_state.current_f) * (f32)(replay_slot_row() * 15) / 10.0f + 80.0f;
        }
        g_AsciiManager->draw_shadows = 1;
        struct tm *time = localtime(&info->timestamp);
        if (page_menu.next_selection == 0)
        {
            if (!(info->flags_a & 2))
            {
                g_AsciiManager->create_stringf(
                    &pos, g_replay_list_formats[0][0], replay_slot + 1, info->name, time->tm_year % 100,
                    time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
                    g_chara_names_short[info->character + info->subshot], g_season_names[info->subseason],
                    g_difficulty_names[info->difficulty], g_stage_names_short[info->stage], info->slowdown);
            }
            else
            {
                g_AsciiManager->create_stringf(&pos, g_replay_spell_format[0], replay_slot + 1, info->name,
                                               time->tm_year % 100, time->tm_mon + 1, time->tm_mday, time->tm_hour,
                                               time->tm_min, g_chara_names_short[info->character + info->subshot],
                                               g_season_names[info->subseason], info->spell_id + 1, info->slowdown);
            }
        }
        else
        {
            memcpy(g_replay_user_number, &replays[replay_slot]->filename[7], 4);
            g_replay_user_number[4] = '\0';
            g_AsciiManager->create_stringf(
                &pos, g_replay_list_formats[1][0], g_replay_user_number, info->name,
                time->tm_year % 100, time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
                g_chara_names_short[info->character + info->subshot], g_difficulty_names[info->difficulty],
                g_stage_names_short[info->stage], g_season_names[info->subseason], info->slowdown);
        }
        if (time_in_state.current >= 10)
        {
            pos.x = 220.0f;
            pos.y = 128.0f;
            for (i32 i = 1; i < 8; i++)
            {
                g_AsciiManager->color.d3d = menu.next_selection == i - 1 ? 0xffffff00 : 0xff808080;
                if (replays[replay_slot]->stages[i].gamestate_at_stage_begin == NULL)
                {
                    g_AsciiManager->create_stringf(&pos, "%s  ---------", g_stage_names[i]);
                }
                else
                {
                    RpyGamestate *next;
                    if (i < 6 && (next = replays[replay_slot]->stages[i + 1].gamestate_at_stage_begin) != NULL)
                    {
                        Globals *globals = (Globals *)next->globals;
                        g_AsciiManager->create_stringf(&pos, "%s  %.8d%d", g_stage_names[i], globals->score,
                                                       globals->continues_used);
                    }
                    else
                    {
                        g_AsciiManager->create_stringf(&pos, "%s  %.8d%d", g_stage_names[i], info->score,
                                                       info->continues_used);
                    }
                }
                pos.y += 18.0f;
            }
        }
        g_AsciiManager->color.d3d = 0xffffffff;
        g_AsciiManager->draw_shadows = 0;
        break;
    }
    }
    return 1;
}

// Player data: the top ten of the chosen character and difficulty, the
// number of games and the play time.
// FUNCTION: TH16 0x453030
HARNESS_CALLED i32 TitleInf::on_draw__player_data()
{
    switch (substate)
    {
    case 2:
        break;
    default:
        return 1;
    }
    Float3 pos;
    pos.x = 22.0f;
    pos.y = 160.0f;
    pos.z = 0.0f;
    i32 difficulty = player_data_difficulty_menu.next_selection;
    g_AsciiManager->draw_shadows = 1;
    if (page_menu.next_selection == 0)
    {
        for (i32 i = 0; i < 10; i++)
        {
            g_AsciiManager->color.d3d = ~(i * 16) | 0xffffff00;
            ScorefileScore *score = &g_Scorefile->characters[menu.next_selection].scores[difficulty][i];
            if (score->date != 0)
            {
                struct tm *time = localtime(&score->date);
                g_AsciiManager->create_stringf(&pos, "%2d  %s  %9ld%d  %.4d/%.2d/%.2d %.2d:%.2d %s  %s  %2.1f%%", i + 1,
                                               score->name, score->score, score->continues,
                                               time->tm_year + 1900, time->tm_mon + 1, time->tm_mday, time->tm_hour,
                                               time->tm_min, g_season_names[score->subseason],
                                               g_stage_names[score->stage], score->slowdown);
            }
            else
            {
                g_AsciiManager->create_stringf(&pos, "%2d  %s  %9ld%d  ----/--/-- --:-- Season  Stage -  ---%%",
                                               i + 1, score->name, score->score, score->continues,
                                               score->slowdown);
            }
            pos.y += 18.0f;
        }
    }
    g_AsciiManager->color.d3d = 0xffffffff;
    pos.x = 328.0f;
    pos.y = 378.0f;
    g_AsciiManager->create_stringf(&pos, "    %5d", g_Scorefile->characters[menu.next_selection].play_count);
    pos.y = 396.0f;
    unsigned __int64 seconds = g_Scorefile->characters[menu.next_selection].play_time / 100;
    unsigned __int64 minutes = seconds / 60;
    unsigned __int64 hours = minutes / 60;
    g_AsciiManager->create_stringf(&pos, "%3lld:%.2lld:%.2lld", hours, minutes - hours * 60, seconds - minutes * 60);
    pos.y = 414.0f;
    g_AsciiManager->create_stringf(&pos, "    %5d",
                                   g_Scorefile->characters[menu.next_selection].play_counts[difficulty]);
    g_AsciiManager->color.d3d = 0xffffffff;
    g_AsciiManager->draw_shadows = 0;
    return 1;
}

// The high score name entry after a game (score_not_ranked is set when the score
// did not make the top ten), then on to saving the replay unless the game
// was continued.
// FUNCTION: TH16 0x4532f0
i32 TitleInf::do_score_name_entry()
{
    switch (substate)
    {
    case 0:
    {
        menu.num_choices = 30;
        g_Supervisor.play_bgm_wav(0, "th128_08");
        g_Supervisor.play_bgm(0, 0x11);
        if (submenu_ascii_id.id == 0)
        {
            submenu_ascii_id = g_AsciiManager->get_anm()->create_effect(0x13, -1, NULL);
        }
        create_effect(0x6f);
        set_substate(1);
        create_effect(g_Globals.subshot + g_Globals.character + 0xa7);
        create_effect(g_Globals.difficulty + 0xaf);
        if (g_AnmManager->get_vm_with_id(anm_ids[0x61]) == NULL)
        {
            create_effect(0x61);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
        }
        g_Globals.stage_num = 8;
        i32 rank = ((ScorefileData *)g_Scorefile)->charas[g_Globals.subshot + g_Globals.character].insert_score();
        g_stage_data = &g_stage_table[0];
        g_Globals.stage_num = 0;
        g_Globals.weird_stage_num = 0;
        if (rank >= 0)
        {
            time_in_state.set_value(0);
            menu.wraps = 1;
            menu.set_cursor(rank);
            name_entry_menu.set_cursor(0);
            name_entry_menu.num_choices = NAME_ENTRY_CHOICES;
            name_entry_menu.wraps = 1;
            strcpy(replay_name, g_Scorefile->last_replay_name);
            if (strcmp(replay_name, "        ") != 0)
            {
                name_entry_menu.move_cursor(-1);
            }
            i32 len;
            for (len = 8; len > 0 && replay_name[len - 1] == ' '; len--)
            {
            }
            replay_name_cursor = len;
            score_not_ranked = 0;
        }
        else
        {
            menu.set_cursor(-1);
            score_not_ranked = 1;
        }
    }
    case 1:
        if (time_in_state.current > 6)
        {
            set_substate(2);
            return 1;
        }
        break;
    case 2:
        if (score_not_ranked == 0)
        {
            menu_save_selection(&name_entry_menu);
            if (input_pressed_or_repeating(INPUT_UP))
            {
                name_entry_menu.move_cursor(-NAME_ENTRY_COLUMNS);
            }
            if (input_pressed_or_repeating(INPUT_DOWN))
            {
                name_entry_menu.move_cursor(NAME_ENTRY_COLUMNS);
            }
            if (input_pressed_or_repeating(INPUT_LEFT))
            {
                i32 selection = name_entry_menu.next_selection;
                if (selection % NAME_ENTRY_COLUMNS != 0)
                {
                    name_entry_menu.move_cursor(-1);
                }
                else
                {
                    name_entry_menu.move_cursor(NAME_ENTRY_COLUMNS - 1);
                }
            }
            if (input_pressed_or_repeating(INPUT_RIGHT))
            {
                i32 selection = name_entry_menu.next_selection;
                if (selection % NAME_ENTRY_COLUMNS != NAME_ENTRY_COLUMNS - 1)
                {
                    name_entry_menu.move_cursor(1);
                }
                else
                {
                    name_entry_menu.move_cursor(-(NAME_ENTRY_COLUMNS - 1));
                }
            }
            if (menu_selection_moved(&name_entry_menu))
            {
                g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            }
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            if (score_not_ranked == 0)
            {
                i32 choice = name_entry_menu.next_selection;
                if (choice < NAME_ENTRY_CHAR_COUNT)
                {
                    if (replay_name_cursor < 8)
                    {
                        replay_name[replay_name_cursor] = g_name_entry_chars[choice];
                        replay_name_cursor++;
                        if (replay_name_cursor >= 8)
                        {
                            name_entry_menu.set_cursor(NAME_ENTRY_END);
                        }
                    }
                    else
                    {
                        replay_name[replay_name_cursor - 1] = g_name_entry_chars[choice];
                    }
                }
                else if (choice == NAME_ENTRY_SPACE)
                {
                    if (replay_name_cursor < 8)
                    {
                        replay_name[replay_name_cursor] = ' ';
                        replay_name_cursor++;
                        if (replay_name_cursor >= 8)
                        {
                            name_entry_menu.set_cursor(NAME_ENTRY_END);
                        }
                    }
                    else
                    {
                        replay_name[replay_name_cursor - 1] = ' ';
                    }
                }
                else if (choice == NAME_ENTRY_BACKSPACE)
                {
                    if (replay_name_cursor == 0)
                    {
                        break;
                    }
                    replay_name_cursor--;
                    replay_name[replay_name_cursor] = ' ';
                }
                else if (choice == NAME_ENTRY_END)
                {
                    strcpy(((ScorefileData *)g_Scorefile)
                               ->charas[g_Globals.subshot + g_Globals.character]
                               .scores[g_Globals.difficulty][menu.next_selection]
                               .name,
                           replay_name);
                    strcpy(g_Scorefile->last_replay_name, replay_name);
                    set_substate(3);
                }
                // The sound in each branch (not once after the if/else):
                // otherwise MSVC reuses the pressed word for the BOMB test
                // below.
                g_SoundManager.play_sound_centered(SE_OK00, 0);
            }
            else
            {
                set_substate(3);
                g_SoundManager.play_sound_centered(SE_OK00, 0);
            }
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            if (score_not_ranked == 0)
            {
                if (replay_name_cursor == 0)
                {
                    break;
                }
                g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                replay_name_cursor--;
                replay_name[replay_name_cursor] = ' ';
                return 1;
            }
            set_substate(3);
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            return 1;
        }
        break;
    case 3:
        if (time_in_state.current >= 6)
        {
            interrupt_and_clear(0x6f);
            interrupt_and_clear(g_Globals.subshot + g_Globals.character + 0xa7);
            interrupt_and_clear(g_Globals.difficulty + 0xaf);
            if (g_Globals.continues_used == 0)
            {
                set_state(TITLE_STATE_REPLAY_SAVE);
                return 1;
            }
            ReplayManager::destroy(g_ReplayManager);
            AnmManager::interrupt_tree(submenu_ascii_id, 1);
            submenu_ascii_id.id = 0;
            menu.pop();
            set_state(TITLE_STATE_MAIN);
            g_Supervisor.play_bgm_wav(0, "th16_01");
            g_Supervisor.play_bgm(0, 0);
        }
        break;
    }
    return 1;
}

// The high score name entry after a game: the top ten of the character and
// difficulty played, and while a name is entered, the name and the
// character grid.
// FUNCTION: TH16 0x4538b0
HARNESS_CALLED i32 TitleInf::on_draw__score_name_entry()
{
    switch (substate)
    {
    case 2:
        break;
    default:
        return 1;
    }
    Float3 pos;
    pos.x = 22.0f;
    pos.y = 160.0f;
    pos.z = 0.0f;
    i32 difficulty = g_Globals.difficulty;
    g_AsciiManager->draw_shadows = 1;
    for (i32 i = 0; i < 10; i++)
    {
        // An if/else, not a ternary: gives the original's registers (the
        // ASCII manager in edx, i in esi).
        if (score_not_ranked != 0)
        {
            g_AsciiManager->color.d3d = ~(i * 16) | 0xffffff00;
        }
        else
        {
            g_AsciiManager->color.d3d = menu.next_selection != i ? 0xff808040 : 0xffffffff;
        }
        ScorefileScore *score =
            &g_Scorefile->characters[g_Globals.subshot + g_Globals.character].scores[difficulty][i];
        if (score->date != 0)
        {
            struct tm *time = localtime(&score->date);
            g_AsciiManager->create_stringf(&pos, "%2d  %s  %9ld%d  %.4d/%.2d/%.2d %.2d:%.2d %s  %s  %2.1f%%", i + 1,
                                           score->name, score->score, score->continues, time->tm_year + 1900,
                                           time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
                                           g_season_names[score->subseason], g_stage_names[score->stage],
                                           score->slowdown);
        }
        else
        {
            g_AsciiManager->create_stringf(&pos, "%2d  %s  %9ld%d  ----/--/-- --:-- Season  Stage -  ---%%", i + 1,
                                           score->name, score->score, score->continues, score->slowdown);
        }
        pos.y += 18.0f;
    }
    if (score_not_ranked != 0)
    {
        return 1;
    }
    pos.x = 58.0f;
    pos.y = menu.next_selection * 18.0f + 160.0f;
    pos.z = 0.0f;
    g_AsciiManager->color.d3d = 0xffffffff;
    g_AsciiManager->create_stringf(&pos, "%s", replay_name);
    pos.x = replay_name_cursor * 9 + 58.0f;
    if (replay_name_cursor == 8)
    {
        pos.x -= 9.0f;
    }
    g_AsciiManager->color.d3d = 0xffffff00;
    g_AsciiManager->create_stringf(&pos, "_");
    pos.x = 212.0f;
    g_AsciiManager->color.d3d = 0xffffffff;
    pos.y = 360.0f;
    pos.z = 0.0f;
    for (i32 i = 0; i < NAME_ENTRY_CHOICES; i++)
    {
        g_AsciiManager->color.d3d = name_entry_menu.next_selection == i ? 0xffffff00 : 0xff808080;
        i32 c;
        if (i < NAME_ENTRY_CHAR_COUNT)
        {
            c = g_name_entry_chars[i];
        }
        else if (i == NAME_ENTRY_SPACE)
        {
            c = 0x81;
        }
        else
        {
            c = (i != NAME_ENTRY_BACKSPACE) + 0x7f;
        }
        g_AsciiManager->create_stringf(&pos, "%c", c);
        if (i % NAME_ENTRY_COLUMNS == NAME_ENTRY_COLUMNS - 1)
        {
            pos.x = 212.0f;
            pos.y += 16.0f;
        }
        else
        {
            pos.x += 18.0f;
        }
    }
    g_AsciiManager->color.d3d = 0xffffffff;
    return 1;
}

// The replay save screen: the 25 slots, then the chosen slot with the name
// being entered and the character grid.
// FUNCTION: TH16 0x4541b0
HARNESS_CALLED i32 TitleInf::on_draw__replay_save()
{
    Float3 pos;
    switch (substate)
    {
    case 2:
    {
        pos.x = 58.0f;
        pos.y = 80.0f;
        pos.z = 0.0f;
        g_AsciiManager->draw_shadows = 1;
        ReplayManager **replay = replays;
        for (i32 i = 0; i < REPLAY_SLOTS; replay++)
        {
            g_AsciiManager->color.d3d = menu.next_selection == i ? 0xffffff00 : 0xff808080;
            if (*replay != NULL)
            {
                RpyInfo *info = (*replay)->info;
                struct tm *time = localtime(&info->timestamp);
                i++;
                g_AsciiManager->create_stringf(&pos, "No.%.2d %s %.2d/%.2d/%.2d %.2d:%.2d %s %s %s %2.1f%%", i,
                                               info->name, time->tm_year % 100, time->tm_mon + 1,
                                               time->tm_mday, time->tm_hour, time->tm_min,
                                               g_chara_names_short[info->character + info->subshot],
                                               g_difficulty_names[info->difficulty],
                                               g_stage_names_short[info->stage], info->slowdown);
            }
            else
            {
                i++;
                g_AsciiManager->create_stringf(
                    &pos, "No.%.2d -------- --/--/-- --:-- ------- ------- --- ---%%", i);
            }
            pos.y += 15.0f;
        }
        break;
    }
    case 3:
    {
        i32 slot = replay_slot;
        pos.x = 58.0f;
        pos.y = 240.0f;
        pos.z = 0.0f;
        if (time_in_state.current < 10)
        {
            pos.y = (10.0f - time_in_state.current_f) * ((f32)(slot * 15 + 80) - 240.0f) / 10.0f + 240.0f;
        }
        RpyInfo *info = g_ReplayManager->info;
        struct tm *time = localtime(&info->timestamp);
        g_AsciiManager->create_stringf(&pos, "No.%.2d %s %.2d/%.2d/%.2d %.2d:%.2d %s %s %s %2.1f%%", slot + 1,
                                       "        ", time->tm_year % 100, time->tm_mon + 1, time->tm_mday,
                                       time->tm_hour, time->tm_min,
                                       g_chara_names_short[info->character + info->subshot],
                                       g_difficulty_names[info->difficulty], g_stage_names_short[8],
                                       info->slowdown);
        if (time_in_state.current >= 10)
        {
            pos.x = 112.0f;
            pos.y = 240.0f;
            pos.z = 0.0f;
            g_AsciiManager->color.d3d = 0xffffffff;
            g_AsciiManager->create_stringf(&pos, "%s", replay_name);
            pos.x = replay_name_cursor * 9 + 112.0f;
            if (replay_name_cursor == 8)
            {
                pos.x -= 9.0f;
            }
            g_AsciiManager->color.d3d = 0xffffff00;
            g_AsciiManager->create_stringf(&pos, "_");
            pos.x = 212.0f;
            g_AsciiManager->color.d3d = 0xffffffff;
            pos.y = 360.0f;
            pos.z = 0.0f;
            for (i32 i = 0; i < NAME_ENTRY_CHOICES; i++)
            {
                g_AsciiManager->color.d3d = name_entry_menu.next_selection == i ? 0xffffff00 : 0xff808080;
                i32 c;
                if (i < NAME_ENTRY_CHAR_COUNT)
                {
                    c = g_name_entry_chars[i];
                }
                else if (i == NAME_ENTRY_SPACE)
                {
                    c = 0x81;
                }
                else
                {
                    c = (i != NAME_ENTRY_BACKSPACE) + 0x7f;
                }
                g_AsciiManager->create_stringf(&pos, "%c", c);
                if (i % NAME_ENTRY_COLUMNS == NAME_ENTRY_COLUMNS - 1)
                {
                    pos.x = 212.0f;
                    pos.y += 16.0f;
                }
                else
                {
                    pos.x += 18.0f;
                }
            }
            g_AsciiManager->color.d3d = 0xffffffff;
        }
        break;
    }
    default:
        return 1;
    }
    g_AsciiManager->color.d3d = 0xffffffff;
    g_AsciiManager->draw_shadows = 0;
    return 1;
}

// The manual (help.anm), shown until HelpManual says it is done.
// FUNCTION: TH16 0x4545a0
i32 TitleInf::do_manual()
{
    switch (substate)
    {
    case 0:
        if (submenu_ascii_id.id == 0)
        {
            submenu_ascii_id = g_AsciiManager->get_anm()->create_effect(0x13, -1, NULL);
        }
        anm_ids[0x72] = title_anm->create_effect(0x72, -1, NULL);
        HelpManual::create();
        substate = 1;
        time_in_state.reset();
        g_HelpManual->x_offset = 128.0f;
        break;
    case 1:
        if (g_HelpManual->closed != 0)
        {
            AnmManager::interrupt_tree(submenu_ascii_id, 1);
            submenu_ascii_id.id = 0;
            interrupt_and_clear(0x72);
            set_state(TITLE_STATE_MAIN);
            if (g_HelpManual != NULL)
            {
                delete g_HelpManual;
            }
            return 0;
        }
        break;
    }
    return 0;
}

char *__fastcall skip_line(char *p, i32 *remaining);
char *__fastcall read_line(char *dst, char *src, i32 *remaining);

// The comment shown for a track not heard in the game yet, with the first
// press of the shot button.
// GLOBAL: TH16 0x4936b8
const char *g_music_room_warning[8] = {
    "\x81@",
    "\x81@\x81@\x81\x96\x81\x96\x91I\x91\xf0\x82\xb5\x82\xbd\x8b\xc8\x82\xcd\x82\xdc\x82\xbe\x83Q\x81[\x83\x80\x92\x86\x82"
    "\xc5\x8d\xc4\x90\xb6\x82\xb3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb9\x82\xf1\x81\x96\x81\x96",
    " ",
    "\x81@\x81@\x81@\x81@\x8b\xc8\x82\xcc\x83R\x83\x81\x83\x93\x83g\x82\xaa\x83l\x83^\x83o\x83\x8c\x82\xc9\x82\xc8\x82"
    "\xe9\x8b\xb0\x82\xea\x82\xaa\x82\xa0\x82\xe8\x82\xdc\x82\xb7\x81"
    "B",
    "\x81@\x81@\x81@\x81@\x81@\x81@\x81@\x81@\x81@\x82\xbb\x82\xea\x82\xc5\x82\xe0\x8d\xc4\x90\xb6\x82\xb5\x82\xdc\x82"
    "\xb7\x82\xa9\x81H",
    "\x81@",
    "\x81@\x81@\x81@\x8d\xc4\x90\xb6\x82\xb5\x82\xbd\x82\xa2\x95\xfb\x82\xcd\x81"
    "A\x82\xe0\x82\xa4\x88\xea\x93x\x8c\x88\x92\xe8\x83{\x83^\x83\x93\x82\xf0\x89\x9f\x82\xb5\x82\xc4\x82\xad\x82\xbe"
    "\x82\xb3\x82\xa2\x81"
    "B",
    "\x81@\x81@\x81@\x8d\xc4\x90\xb6\x82\xb5\x82\xbd\x82\xad\x82\xc8\x82\xa2\x95\xfb\x82\xcd\x81"
    "A\x83J\x81[\x83\\\x83\x8b\x82\xf0\x88\xda\x93\xae\x82\xb5\x82\xc4\x82\xad\x82\xbe\x82\xb3\x82\xa2\x81"
    "B",
};

// Writes the next line of the chosen track's comment (or of the warning).
static __forceinline void music_room_comment_step(TitleInf *menu)
{
    if (!(menu->time_in_state.current & 1) && menu->music_comment_line < 8)
    {
        AnmVm *vm = menu->comment_line_ids[menu->music_comment_line].find_or_clear();
        if (!g_Scorefile->bgm_unlocked[menu->music_comment_track] && menu->music_warning != 0)
        {
            g_AnmManager->draw_text(vm, 0x8080ff, 0, 0, 0, 0, g_music_room_warning[menu->music_comment_line]);
        }
        else
        {
            g_AnmManager->draw_text(vm, 0xffffff, 0, 0, 0, 0,
                                    menu->music_comments[menu->music_comment_track][menu->music_comment_line]);
        }
        vm->interrupt_out_of_line(2);
        menu->music_comment_line++;
    }
}

// The music room: the track list (ten rows shown, sliding in two at a time
// at first) and the comment of the track last picked. Tracks not heard in
// the game yet show as numbers, and playing one asks for a second press.
// TODO: case 0 stores pos.x = 0 after pos.y and pos.z (the original first).
// Declared __declspec(safebuffers) (MainMenu.h): without it ours adds a /GS
// cookie for pos (a D3DXVECTOR3 in memory) that the original lacks.
// FUNCTION: TH16 0x4546f0
i32 TitleInf::do_music_room()
{
    Float3 pos;
    switch (substate)
    {
    case 0:
        if (time_in_state.current == 1)
        {
            menu.num_choices = 6;
            menu.set_cursor(0);
            if (submenu_ascii_id.id == 0)
            {
                submenu_ascii_id = g_AsciiManager->get_anm()->create_effect(0x13, -1, NULL);
            }
            create_effect(0x6e);
            i32 count = 0;
            i32 size;
            char *p = (char *)file_read_all("musiccmt.txt", &size, 0);
            music_comment_file = p;
            if (p == NULL)
            {
                goto leave;
            }
            while (size > 0)
            {
                if (*p == '#')
                {
                    p = skip_line(p, &size);
                }
                else if (*p == '@')
                {
                    p = read_line(music_filenames[count], p + 1, &size);
                    p = read_line(music_titles[count], p, &size);
                    for (i32 i = 0; i < 8; i++)
                    {
                        p = read_line(music_comments[count][i], p, &size);
                    }
                    count++;
                }
                else
                {
                    p = skip_line(p, &size);
                }
            }
            menu.num_choices = count;
            menu.set_cursor(0);
            music_scroll = 0;
            music_track_count = count;
            pos.x = 0.0f;
            pos.y = 0.0f;
            pos.z = 0.0f;
            for (i32 i = 0; i < 8; i++)
            {
                comment_line_ids[i] = g_Supervisor.text_anm->create_vm_inline(i + 0x13, &pos, 0.0f, -1);
            }
            music_comment_line = 0;
            music_comment_track = 0;
            music_warning = 0;
        }
        if (time_in_state.current < 10)
        {
            pos.x = 128.0f;
            pos.y = (96.0f - music_scroll * 20.0f + (time_in_state.current * 40 - 40)) * 2.0f;
            pos.z = 0.0f;
            for (i32 i = time_in_state.current * 2 - 2; i < time_in_state.current * 2; i++)
            {
                if (i >= music_track_count)
                {
                    break;
                }
                text_row_ids[0x10 + i] = title_anm->create_vm_inline(i + 0xbc, NULL, 0.0f, -1);
                AnmVm *vm = get_vm_or_clear(text_row_ids[0x10 + i]);
                if (g_Scorefile->bgm_unlocked[i])
                {
                    g_AnmManager->draw_text(vm, 0xffffff, 0, 0, 0, 0, music_titles[i]);
                }
                else
                {
                    g_AnmManager->draw_text(vm, 0xffffff, 0, 0, 0, 0,
                                            "No.%2d  \x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H", i + 1);
                }
                if (i >= music_scroll && i < music_scroll + 10)
                {
                    vm->show_tree_inline();
                }
                else
                {
                    vm->hide_tree_inline();
                }
                if (i == menu.next_selection)
                {
                    pos.x -= 8.0;
                }
                vm->set_pos_time(4, 0, &vm->pos, &pos);
                if (i == menu.next_selection)
                {
                    pos.x += 8.0;
                }
                pos.y += 40.0f;
                vm->interrupt(i != menu.next_selection ? 3 : 2);
            }
        }
        if (time_in_state.current >= 10)
        {
            substate = 1;
            time_in_state.reset();
            return 0;
        }
        break;
    case 1:
        music_room_comment_step(this);
        if (time_in_state.current > 4)
        {
            set_substate(2);
            return 0;
        }
        break;
    case 2:
        music_room_comment_step(this);
        // Through the MenuHelper helpers: on the member, MSVC kept &menu in
        // a register for the rest of the case.
        menu_save_selection(&menu);
        if (pressed_or_repeating_inline(INPUT_UP))
        {
            menu.move_cursor(-1);
        }
        if (pressed_or_repeating_inline(INPUT_DOWN))
        {
            menu.move_cursor(1);
        }
        if (menu_selection_moved(&menu))
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            if (menu.next_selection < music_scroll)
            {
                music_scroll = menu.next_selection;
            }
            else if (menu.next_selection >= music_scroll + 10)
            {
                music_scroll = menu.next_selection - 9;
            }
            pos.x = 128.0f;
            pos.y = (96.0f - music_scroll * 20.0f) * 2.0f;
            pos.z = 0.0f;
            for (i32 i = 0; i < music_track_count; i++)
            {
                AnmVm *vm = get_vm_or_clear(text_row_ids[0x10 + i]);
                if (i >= music_scroll && i < music_scroll + 10)
                {
                    vm->show_tree();
                }
                else
                {
                    vm->hide_tree_inline();
                }
                if (i == menu.next_selection)
                {
                    pos.x -= 8.0;
                }
                if ((f32)fabs(vm->pos.y - pos.y) < 80.0f)
                {
                    vm->set_pos_time(4, 0, &vm->pos, &pos);
                }
                else
                {
                    vm->pos = pos;
                }
                if (i == menu.next_selection)
                {
                    pos.x += 8.0;
                }
                pos.y += 40.0f;
                vm->interrupt(i != menu.next_selection ? 3 : 2);
            }
            if (music_comment_line >= 8)
            {
                music_warning = 0;
            }
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            for (i32 i = 0; i < 8; i++)
            {
                AnmManager::interrupt_tree(comment_line_ids[i], 3);
            }
            music_comment_track = menu.next_selection;
            music_comment_line = 0;
            time_in_state.reset();
            if (!g_Scorefile->bgm_unlocked[music_comment_track] && music_warning == 0)
            {
                if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
                {
                    g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
                }
                else
                {
                    g_SoundManager.modify_bgm(BGM_STOP, 0, "dummy");
                }
                music_warning = 1;
                return 0;
            }
            g_Supervisor.play_bgm_wav(0, music_filenames[menu.next_selection]);
            if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
            {
                g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
            }
            g_SoundManager.modify_bgm(BGM_PLAY, 0, "dummy");
            g_Scorefile->bgm_unlocked[0] = 1;
            music_warning = 0;
            return 0;
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
        leave:
            if (music_comment_file != NULL)
            {
                free(music_comment_file);
                music_comment_file = NULL;
            }
            music_comment_file = NULL;
            for (i32 i = 0; i < music_track_count; i++)
            {
                AnmManager::interrupt_tree(text_row_ids[0x10 + i], 1);
            }
            for (i32 i = 0; i < 8; i++)
            {
                AnmManager::interrupt_tree(comment_line_ids[i], 1);
            }
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            substate = 3;
            time_in_state.reset();
            return 0;
        }
        break;
    case 3:
        if (time_in_state.current >= 10)
        {
            interrupt_and_clear(0x6e);
            AnmManager::interrupt_tree(submenu_ascii_id, 1);
            submenu_ascii_id.id = 0;
            set_state(TITLE_STATE_MAIN);
            g_Supervisor.play_bgm_wav(0, "th16_01");
            g_Supervisor.play_bgm(0, 0);
            menu.pop();
        }
        break;
    }
    return 0;
}

// Spell practice: picking the stage. Coming back from a game goes straight
// on to the spell card list of the last stage.
// FUNCTION: TH16 0x4553d0
i32 TitleInf::do_spell_practice_stage_select()
{
    switch (substate)
    {
    case 0:
        if (submenu_ascii_id.id == 0)
        {
            submenu_ascii_id = g_AsciiManager->get_anm()->create_effect(0x13, -1, NULL);
        }
        menu.num_choices = 7;
        if (g_AnmManager->get_vm_with_id(anm_ids[0x11c]) == NULL)
        {
            anm_ids[0x11c] = title_anm->create_effect(0x11c, -1, NULL);
        }
        if (g_AnmManager->get_vm_with_id(anm_ids[0xd7]) == NULL)
        {
            anm_ids[0xd7] = title_anm->create_effect(0xd7, -1, NULL);
        }
        set_substate(1);
        if (g_spell_practice_last_stage >= 0)
        {
            menu.set_cursor(g_spell_practice_last_stage);
            g_spell_practice_last_stage = -1;
            spell_character_menu.wraps = 1;
            spell_character_menu.num_choices = 4;
            spell_character_menu.set_cursor(g_Globals.subshot + g_Globals.character);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd7], 3);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd7], (i16)(menu.next_selection + 7));
            AnmManager::interrupt_tree_and_run(anm_ids[0xd7], 6);
            AnmManager::interrupt_tree_and_run(anm_ids[0x11c], 3);
            AnmManager::interrupt_tree(anm_ids[0x11c], (i16)(spell_character_menu.next_selection + 7));
            goto start_rows;
        }
        anm_ids[0x71] = title_anm->create_effect(0x71, -1, NULL);
    case 1:
        if (time_in_state.current > 10)
        {
            set_substate(2);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd7], 3);
            AnmManager::interrupt_tree(anm_ids[0xd7], (i16)(menu.next_selection + 7));
            AnmManager::interrupt_tree_and_run(anm_ids[0x11c], 3);
            AnmManager::interrupt_tree(anm_ids[0x11c], (i16)(spell_character_menu.next_selection + 7));
            return 1;
        }
        break;
    case 2:
        menu.current_selection = menu.next_selection;
        if (input_pressed_or_repeating(INPUT_UP))
        {
            menu.move_cursor(-1);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            menu.move_cursor(1);
        }
        if (menu.current_selection != menu.next_selection)
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd7], 3);
            AnmManager::interrupt_tree(anm_ids[0xd7], (i16)(menu.next_selection + 7));
        }
        do_spell_practice_character();
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            g_practice_last_stage = menu.next_selection;
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            AnmManager::interrupt_tree(anm_ids[0xd7], 6);
            set_substate(3);
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            g_practice_last_stage = menu.next_selection;
            return 1;
        }
        break;
    case 3:
        if (time_in_state.current >= 20)
        {
        start_rows:
            AnmManager::interrupt_tree(anm_ids[0x71], 1);
            anm_ids[0x71].id = 0;
            set_state(TITLE_STATE_SPELL_PRACTICE_ROW_SELECT);
            spell_stage = menu.next_selection;
            menu.push();
            menu.set_cursor(0);
            return 1;
        }
        break;
    case 4:
        if (time_in_state.current >= 6)
        {
            AnmManager::interrupt_tree(anm_ids[0x71], 1);
            anm_ids[0x71].id = 0;
            AnmManager::interrupt_tree(anm_ids[0xd7], 1);
            anm_ids[0xd7].id = 0;
            AnmManager::interrupt_tree(anm_ids[0x11c], 1);
            anm_ids[0x11c].id = 0;
            set_state(TITLE_STATE_MAIN);
            menu.pop();
            AnmManager::interrupt_tree(submenu_ascii_id, 1);
            submenu_ascii_id.id = 0;
        }
        break;
    }
    return 1;
}

// Spell practice: picking the character, which reloads the spell list.
// FUNCTION: TH16 0x455790
i32 TitleInf::do_spell_practice_character()
{
    MenuHelper *character_menu = &spell_character_menu;
    character_menu->current_selection = character_menu->next_selection;
    if (input_pressed_or_repeating(INPUT_LEFT))
    {
        g_SoundManager.play_sound_centered(SE_SELECT00, 0);
        AnmManager::interrupt_tree_and_run(anm_ids[0x11c], (i16)(character_menu->next_selection + 25));
        character_menu->move_cursor(-1);
        AnmManager::interrupt_tree_and_run(anm_ids[0x11c], (i16)(character_menu->next_selection + 13));
        g_Globals.character = character_menu->next_selection;
        if (state == TITLE_STATE_SPELL_PRACTICE_ROW_SELECT)
        {
            load_spell_list(spell_stage, menu.next_selection, spell_ids, -1);
        }
        else if (state == TITLE_STATE_SPELL_PRACTICE_DIFFICULTY_SELECT)
        {
            load_spell_list(spell_stage, spell_row, spell_ids, -1);
            highlight_spell_row(menu.next_selection);
        }
    }
    if (input_pressed_or_repeating(INPUT_RIGHT))
    {
        g_SoundManager.play_sound_centered(SE_SELECT00, 0);
        AnmManager::interrupt_tree_and_run(anm_ids[0x11c], (i16)(character_menu->next_selection + 19));
        character_menu->move_cursor(1);
        AnmManager::interrupt_tree_and_run(anm_ids[0x11c], (i16)(character_menu->next_selection + 7));
        g_Globals.character = character_menu->next_selection;
        if (state == TITLE_STATE_SPELL_PRACTICE_ROW_SELECT)
        {
            load_spell_list(spell_stage, menu.next_selection, spell_ids, -1);
        }
        else if (state == TITLE_STATE_SPELL_PRACTICE_DIFFICULTY_SELECT)
        {
            load_spell_list(spell_stage, spell_row, spell_ids, -1);
            highlight_spell_row(menu.next_selection);
        }
    }
    return 0;
}

// The id at a byte offset into an array of ids.
static __forceinline AnmId id_at_offset(AnmId *ids, i32 offset)
{
    return *(AnmId *)((u8 *)ids + offset);
}

// Spell practice: picking the boss attack (the row of spell cards) of the
// stage.
// FUNCTION: TH16 0x455900
i32 TitleInf::do_spell_practice_row()
{
    i32 row_counts[7] = {2, 3, 4, 3, 6, 9, 13};
    switch (substate)
    {
    case 0:
        menu.num_choices = row_counts[spell_stage];
        if (g_AnmManager->get_vm_with_id(anm_ids[0x6b]) == NULL)
        {
            anm_ids[0x6b] = title_anm->create_effect(0x6b, -1, NULL);
        }
        if (g_AnmManager->get_vm_with_id(anm_ids[0xd8]) == NULL)
        {
            anm_ids[0xd8] = title_anm->create_effect(0xd8, -1, NULL);
        }
        set_substate(1);
        for (i32 i = 0; i < 13 - row_counts[spell_stage]; i++)
        {
            AnmManager::interrupt_tree_and_run(anm_ids[0xd8], (i16)(39 - i));
        }
        if (g_spell_practice_last_row >= 0)
        {
            menu.set_cursor(g_spell_practice_last_row);
            g_spell_practice_last_row = -1;
            load_spell_list(spell_stage, menu.next_selection, spell_ids, -1);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd8], 3);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd8], (i16)(menu.next_selection + 7));
            AnmManager::interrupt_tree_and_run(anm_ids[0xd8], 6);
            goto start_list;
        }
        load_spell_list(spell_stage, menu.next_selection, spell_ids, -1);
    case 1:
        if (time_in_state.current > 10)
        {
            set_substate(2);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd8], 3);
            AnmManager::interrupt_tree(anm_ids[0xd8], (i16)(menu.next_selection + 7));
        }
        break;
    case 2:
        menu.current_selection = menu.next_selection;
        if (input_pressed_or_repeating(INPUT_UP))
        {
            menu.move_cursor(-1);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            menu.move_cursor(1);
        }
        if (menu.current_selection != menu.next_selection)
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd8], 3);
            AnmManager::interrupt_tree(anm_ids[0xd8], (i16)(menu.next_selection + 7));
            load_spell_list(spell_stage, menu.next_selection, spell_ids, -1);
        }
        do_spell_practice_character();
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
        }
        else if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            if (!spell_practice_row_seen(spell_stage, menu.next_selection))
            {
                g_SoundManager.play_sound_centered(SE_INVALID, 0);
            }
            else
            {
                AnmManager::interrupt_tree(anm_ids[0xd8], 6);
                set_substate(3);
                g_SoundManager.play_sound_centered(SE_OK00, 0);
            }
        }
        break;
    case 3:
        if (time_in_state.current >= 14)
        {
        start_list:
            set_state(TITLE_STATE_SPELL_PRACTICE_DIFFICULTY_SELECT);
            spell_row = menu.next_selection;
            menu.push_inline();
            AnmManager::interrupt_tree(anm_ids[0x6b], 1);
            anm_ids[0x6b].id = 0;
            menu.set_cursor(0);
        }
        break;
    case 4:
        if (time_in_state.current >= 6)
        {
            // Counting byte offsets (not indices) makes this the base
            // register of the loads like the original ([edi + esi + disp]).
            for (i32 offset = 0; offset < 5 * (i32)sizeof(AnmId); offset += sizeof(AnmId))
            {
                AnmManager::interrupt_tree(id_at_offset(text_row_ids, offset), 1);
            }
            for (i32 offset = 0; offset < 7 * (i32)sizeof(AnmId); offset += sizeof(AnmId))
            {
                g_AnmManager->delete_vm(id_at_offset(&anm_ids[0x10d], offset));
            }
            AnmManager::interrupt_tree(anm_ids[0x6b], 1);
            anm_ids[0x6b].id = 0;
            AnmManager::interrupt_tree(anm_ids[0xd8], 1);
            anm_ids[0xd8].id = 0;
            AnmManager::interrupt_tree(anm_ids[0xd7], 1);
            anm_ids[0xd7].id = 0;
            set_state(TITLE_STATE_SPELL_PRACTICE_STAGE_SELECT);
            menu.pop_inline();
        }
        break;
    }
    return 1;
}

// The spell practice choice that the menu returns to.
// GLOBAL: TH16 0x49f2d8
i32 g_spell_practice_last_index = -1;
// GLOBAL: TH16 0x49f2dc
i32 g_spell_practice_last_row = -1;
// GLOBAL: TH16 0x4a2970
i32 g_spell_practice_last_stage = -1;
// The stage last picked in stage practice.
// GLOBAL: TH16 0x4a2974
i32 g_practice_last_stage = -1;

// Spell practice: picking the subseason, then starting the game.
// FUNCTION: TH16 0x455d50
i32 TitleInf::do_spell_practice_subseason()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = 4;
        menu.set_cursor(0);
        if (g_AnmManager->get_vm_with_id(anm_ids[0xd9]) == NULL)
        {
            anm_ids[0xd9] = title_anm->create_effect(0xd9, -1, NULL);
        }
        set_substate(1);
    case 1:
        if (time_in_state.current > 10)
        {
            set_substate(2);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd9], 3);
            AnmManager::interrupt_tree(anm_ids[0xd9], (i16)(menu.next_selection + 7));
            return 1;
        }
        break;
    case 2:
        menu.current_selection = menu.next_selection;
        if (input_pressed_or_repeating(INPUT_UP))
        {
            menu.move_cursor(-1);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            menu.move_cursor(1);
        }
        if (menu.current_selection != menu.next_selection)
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd9], 3);
            AnmManager::interrupt_tree(anm_ids[0xd9], (i16)(menu.next_selection + 7));
        }
        do_spell_practice_character();
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            AnmManager::interrupt_tree(anm_ids[0xd9], 6);
            set_substate(3);
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            g_Supervisor.fade_out_bgm(0.05f);
            g_SoundManager.play_sound_centered(SE_BOON00, 0);
            return 1;
        }
        break;
    case 3:
        if (time_in_state.current == 10)
        {
            g_AsciiManager->show_now_loading(480.0f, 392.0f);
            AnmId id;
            id = g_EffectManager->create_ui_effect(EFFECT_MASKED, NULL, NULL);
            g_Supervisor.config.loading_effect_id = id.id;
            AnmManager::interrupt_tree(id, 7);
        }
        if (time_in_state.current >= 40)
        {
            menu.push();
            set_state(TITLE_STATE_EXIT);
            g_title_return_point = TITLE_RETURN_SPELL_PRACTICE;
            i32 stage = spell_stage + 1;
            // Stage table pointer first, for matching (see do_spell_practice_difficulty).
            g_stage_data = &g_stage_table[stage];
            g_Globals.stage_num = stage;
            g_Globals.weird_stage_num = stage;
            g_Globals.spell_id = spell_ids[spell_index];
            g_Globals.character = spell_character_menu.next_selection;
            g_Globals.subshot = 0;
            g_Globals.subseason = menu.next_selection;
            g_Globals.difficulty = g_spell_difficulty[spell_ids[spell_index]];
            g_Supervisor.gamemode_to_switch_to = GAMEMODE_GAME;
            g_spell_practice_last_stage = spell_stage;
            g_spell_practice_last_row = spell_row;
            g_spell_practice_last_index = spell_index;
            return 1;
        }
        break;
    case 4:
        if (time_in_state.current >= 6)
        {
            AnmManager::interrupt_tree(anm_ids[0x6a], 1);
            anm_ids[0x6a].id = 0;
            AnmManager::interrupt_tree(anm_ids[0xd9], 1);
            anm_ids[0xd9].id = 0;
            set_state(TITLE_STATE_SPELL_PRACTICE_DIFFICULTY_SELECT);
            menu.pop();
        }
        break;
    }
    return 1;
}

// Spell practice: picking the spell card (the difficulty row). Extra stage
// cards start the game right away; the others go on to the subseason.
// The stage table pointer is assigned before the two stage numbers: that keeps stage + 1 in ecx with the multiply ahead of the stores, as in the original.
// FUNCTION: TH16 0x456a20
i32 TitleInf::do_spell_practice_difficulty()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = 5;
        for (i32 i = 0; i < 5; i++)
        {
            if (spell_ids[i] < 0)
            {
                menu.disable(i);
            }
        }
        menu.set_cursor(0);
        if (g_spell_practice_last_index >= 0)
        {
            menu.set_cursor(g_spell_practice_last_index);
            g_spell_practice_last_index = -1;
        }
        highlight_spell_row(menu.next_selection);
        set_substate(1);
    case 1:
        if (time_in_state.current > 10)
        {
            set_substate(2);
            return 1;
        }
        break;
    case 2:
        menu.current_selection = menu.next_selection;
        if (input_pressed_or_repeating(INPUT_UP))
        {
            menu.move_cursor(-1);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            menu.move_cursor(1);
        }
        if (menu.current_selection != menu.next_selection)
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            highlight_spell_row(menu.next_selection);
        }
        do_spell_practice_character();
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            AnmManager::interrupt_tree(text_row_ids[menu.next_selection], 6);
            set_substate(3);
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            if (spell_stage == 6)
            {
                g_Supervisor.fade_out_bgm(0.05f);
                g_SoundManager.play_sound_centered(SE_BOON00, 0);
                return 1;
            }
        }
        break;
    case 3:
        if (spell_stage == 6)
        {
            if (time_in_state.current == 10)
            {
                g_AsciiManager->show_now_loading(480.0f, 392.0f);
                AnmId id;
                id = g_EffectManager->create_ui_effect(EFFECT_MASKED, NULL, NULL);
                g_Supervisor.config.loading_effect_id = id.id;
                AnmManager::interrupt_tree(id, 7);
            }
            if (time_in_state.current >= 40)
            {
                menu.push();
                set_state(TITLE_STATE_EXIT);
                g_title_return_point = TITLE_RETURN_SPELL_PRACTICE;
                i32 stage = spell_stage + 1;
                g_stage_data = &g_stage_table[stage];
                g_Globals.stage_num = stage;
                g_Globals.weird_stage_num = stage;
                g_Globals.spell_id = spell_ids[menu.next_selection];
                g_Globals.character = spell_character_menu.next_selection;
                g_Globals.subshot = 0;
                g_Globals.difficulty = g_spell_difficulty[spell_ids[menu.next_selection]];
                g_Globals.subseason = SUBSEASON_DOYOU;
                g_Supervisor.gamemode_to_switch_to = GAMEMODE_GAME;
                g_spell_practice_last_stage = spell_stage;
                g_spell_practice_last_row = spell_row;
                g_spell_practice_last_index = menu.next_selection;
                return 1;
            }
        }
        else if (time_in_state.current >= 14)
        {
            spell_index = menu.next_selection;
            set_state(TITLE_STATE_SPELL_PRACTICE_SUBSEASON_SELECT);
            menu.push();
            interrupt_and_clear(0x6b);
            menu.set_cursor(0);
            return 1;
        }
        break;
    case 4:
        if (time_in_state.current >= 6)
        {
            AnmManager::interrupt_tree(anm_ids[0xd8], 1);
            anm_ids[0xd8].id = 0;
            set_state(TITLE_STATE_SPELL_PRACTICE_ROW_SELECT);
            menu.pop();
        }
        break;
    }
    return 1;
}

// AnmManager::interrupt_tree as LTCG inlined it into load_spell_list.
static __forceinline void interrupt_tree_inline(AnmId id, i32 interrupt)
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    if (vm == NULL)
    {
        return;
    }
    vm->interrupt(interrupt);
    for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
    {
        node->entry->interrupt(interrupt);
    }
}

// Whether the first count spell cards of a row are captured (in any mode),
// which unlocks the row's overdrive card.
static __forceinline i32 spell_row_captured(const i32 *row, i32 count)
{
    for (i32 i = 0; i < count; i++, row++)
    {
        if (g_Scorefile->characters[4].spells[*row].captures[0] == 0 &&
            g_Scorefile->characters[4].spells[*row].captures[1] == 0)
        {
            return 0;
        }
    }
    return 1;
}

// Spell practice: lists the spell cards of a stage's boss attack, one row
// per difficulty (Extra and the overdrive card share the last rows), with
// their names once attempted and the chosen character's captures
// highlighted. Rows are highlighted with interrupt 2 when they are the
// selected one (-1 at every call site; LTCG folded it).
// TODO: the original keeps g_AnmManager in edi across the first lookups and this in ebx in the main loop.
// FUNCTION: TH16 0x4560b0
HARNESS_CALLED i32 TitleInf::load_spell_list(i32 stage, i32 row, i32 *ids, i32 selected)
{
    char name[0xc1];
    ids[0] = -2;
    ids[1] = -2;
    ids[2] = -2;
    ids[3] = -2;
    ids[4] = -2;
    for (i32 i = 0; i < 7; i++)
    {
        g_AnmManager->delete_vm_inline(anm_ids[0x10d + i]);
    }
    for (i32 i = 0; i < 5; i++)
    {
        if (get_vm_or_clear(text_row_ids[i]) == NULL)
        {
            text_row_ids[i] = title_anm->create_vm_inline(i + 0xd2, NULL, 0.0f, -1);
        }
        interrupt_tree_inline(text_row_ids[i], 2);
        g_AnmManager->draw_text(get_vm_or_clear(text_row_ids[i]), 0x808080, 0, 0, 0, 0, " Nothing ... ");
    }
    if (stage != 6)
    {
        for (i32 i = 0; i < 4; i++)
        {
            anm_ids[0x10d + i] = title_anm->create_effect(i + 0x10d, -1, NULL);
            AnmManager::interrupt_tree_and_run(anm_ids[0x10d + i], 3);
        }
    }
    i32 last_slot = 0;
    const i32 *row_ids = g_spell_practice_ids[stage][row];
    const i32 *next_id = row_ids;
    for (i32 i = 0; i < 5; i++, next_id++)
    {
        i32 id = *next_id;
        if (id < 0)
        {
            break;
        }
        i8 difficulty;
        i32 slot;
        if (stage == 6)
        {
            if (g_spell_difficulty[id] == 4)
            {
                anm_ids[0x111] = title_anm->create_effect(0x111, -1, NULL);
            }
            else if (g_spell_difficulty[id] == 5)
            {
                anm_ids[0x113] = title_anm->create_effect(0x113, -1, NULL);
            }
            difficulty = g_spell_difficulty[id];
            slot = difficulty != DIFFICULTY_EXTRA;
        }
        else
        {
            if (g_spell_difficulty[id] >= 5)
            {
                anm_ids[0x112] = title_anm->create_effect(0x112, -1, NULL);
                AnmManager::interrupt_tree_and_run(anm_ids[0x112], 3);
            }
            difficulty = g_spell_difficulty[id];
            slot = difficulty >= 5 ? 4 : difficulty;
        }
        i32 overdrive_open = 0;
        if (difficulty >= 5)
        {
            overdrive_open = spell_row_captured(row_ids, i);
        }
        if (g_Scorefile->characters[4].spells[id].attempts[0] == 0 && !overdrive_open)
        {
            ids[slot] = -1;
            g_AnmManager->draw_text(get_vm_or_clear(text_row_ids[slot]), 0xb0b0b0, 0, 0, 0, 0,
                                    " No.%3d  \x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H", id + 1);
        }
        else
        {
            ids[slot] = id;
            if (g_Scorefile->characters[4].spells[id].attempts[0] == 0 &&
                g_Scorefile->characters[4].spells[id].attempts[1] == 0)
            {
                if (difficulty >= 5)
                {
                    // オーバードライブモード挑戦可能！ ("Overdrive mode open!")
                    strcpy(name, "\x83I\x81[\x83o\x81[\x83h\x83\x89\x83"
                                 "C\x83u\x83\x82\x81[\x83h\x92\xa7\x90\xed\x89\xc2\x94\\\x81I");
                }
                else
                {
                    // 挑戦可能！ ("Open!")
                    strcpy(name, "\x92\xa7\x90\xed\x89\xc2\x94\\\x81I");
                }
            }
            else
            {
                strcpy(name, g_Scorefile->characters[4].spells[id].name);
            }
            i32 len = strlen(name);
            for (; len < 42; len++)
            {
                name[len] = ' ';
            }
            name[len] = '\0';
            ScorefileCharacter *character = &g_Scorefile->characters[spell_character_menu.next_selection];
            AnmVm *vm = get_vm_or_clear(text_row_ids[slot]);
            g_AnmManager->draw_text(vm, character->spells[id].captures[1] != 0 ? 0xffff80 : 0xefefef, 0, 0, 0, 0,
                                    " No.%3d  %s", id + 1, name);
        }
        interrupt_tree_inline(text_row_ids[slot], 2);
        if (last_slot < slot)
        {
            last_slot = slot;
        }
    }
    if (stage != 6)
    {
        for (i32 i = 0; i < 5; i++)
        {
            if (i == selected)
            {
                interrupt_tree_inline(text_row_ids[i], 2);
            }
            else
            {
                interrupt_tree_inline(text_row_ids[i], 3);
            }
        }
    }
    else
    {
        for (i32 i = 0; i < 2; i++)
        {
            if (i == selected)
            {
                interrupt_tree_inline(text_row_ids[i], 2);
            }
            else
            {
                interrupt_tree_inline(text_row_ids[i], 3);
            }
        }
    }
    for (i32 i = last_slot + 1; i < 5; i++)
    {
        delete_vm_inline_and_clear(text_row_ids[i]);
    }
    return 0;
}

// Highlights the selected row of the spell list (interrupt 2) and dims the
// others (3), with the difficulty icon of each row's spell card.
// FUNCTION: TH16 0x4569a0
i32 TitleInf::highlight_spell_row(i32 selected)
{
    for (i32 i = 0; i < 5; i++)
    {
        if (i == selected)
        {
            if (get_vm_or_clear(text_row_ids[i]) != NULL)
            {
                AnmManager::interrupt_tree(text_row_ids[i], 2);
            }
            AnmManager::interrupt_tree_and_run(anm_ids[0x10d + g_spell_difficulty[spell_ids[i]]], 2);
        }
        else
        {
            if (get_vm_or_clear(text_row_ids[i]) != NULL)
            {
                AnmManager::interrupt_tree(text_row_ids[i], 3);
            }
            AnmManager::interrupt_tree_and_run(anm_ids[0x10d + g_spell_difficulty[spell_ids[i]]], 3);
        }
    }
    return 0;
}
