// States of the title screen's menus (TitleInf::on_tick dispatches on
// state).
#include <direct.h>
#include <stddef.h>
#include <stdio.h>

#include "MainMenu.h"

#include "EffectManager.h"
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

extern u32 g_hardware_input_repeat;
extern u32 g_hardware_input_pressed;
i32 __stdcall input_pressed_or_repeating(u32 mask);

static_assert(offsetof(TitleInf, menu_5cec) == 0x5cec, "TitleInf::menu_5cec");
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
// user replays from the replay directory, until stopped through bit 2 of
// flags_5ce8.
// FUNCTION: TH16 0x451560
void TitleInf::load_replay_list()
{
    char filename[0x40];
    WIN32_FIND_DATAA find_data;
    TitleInf *menu = g_MainMenu;

    for (i32 i = 1; i <= 25; i++)
    {
        sprintf(filename, "th16_%.2d.rpy", i);
        menu->replays[i - 1] = create_replay_inline(filename);
        if (menu->flags_5ce8 & 4)
        {
            break;
        }
    }
    _chdir(g_GameWindow.save_dir);
    _chdir("replay");
    HANDLE find = FindFirstFileA("th16_ud????.rpy", &find_data);
    if (find != INVALID_HANDLE_VALUE)
    {
        for (i32 i = 25; i < 75; i++)
        {
            _chdir(g_GameWindow.exe_dir);
            menu->replays[i] = ReplayManager::create_from_file(find_data.cFileName);
            _chdir(g_GameWindow.save_dir);
            _chdir("replay");
            if (menu->flags_5ce8 & 4)
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
    menu->flags_5ce8 = (menu->flags_5ce8 & ~4) | 8;
}

// Stage names for the practice and replay menus, by stage number.
// GLOBAL: TH16 0x491920
const char *const g_stage_names[10] = {"test   ",  "Stage 1", "Stage 2", "Stage 3", "Stage 4",
                                       "Stage 5", "Stage 6", "Extra  ", "Clear  ", "ExClear"};

// The stages and practice high scores of stage practice.
// TODO: the original frame has an unused 4-byte slot and saves ebx/esi on entry rather than in the branch.
// FUNCTION: TH16 0x4513c0
i32 TitleInf::on_draw__practice_stage_select()
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
                            .practice[g_Globals.difficulty][stage]
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
                    &scorefile->characters[g_Globals.subshot + g_Globals.character].practice[g_Globals.difficulty][stage];
                if (!practice->unlocked)
                {
                    g_AsciiManager->create_stringf(&pos, "%s  ---------", g_stage_names[stage]);
                }
                else
                {
                    g_AsciiManager->create_stringf(&pos, "%s  %.8d0", g_stage_names[stage], practice->high_score);
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
// TODO: ours saves esi/edi after the substate checks (shrink-wrapped); the original saves them in the prologue.
// FUNCTION: TH16 0x456d50
i32 TitleInf::on_draw__spell_practice_histories()
{
    if (substate > 0 && (substate <= 2 || (substate == 3 && state != 19)))
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
                if (state == 19 && i == menu.next_selection)
                {
                    g_AsciiManager->color.d3d =
                        g_Scorefile->characters[menu_5cec.next_selection].spells[id].captures[1] != 0 ? 0xff90d0ff
                                                                                                      : 0xffb0b0b0;
                }
                else
                {
                    g_AsciiManager->color.d3d =
                        g_Scorefile->characters[menu_5cec.next_selection].spells[id].captures[1] != 0 ? 0xff60a0c0
                                                                                                      : 0xff404040;
                }
                if (g_Scorefile->characters[4].spells[id].attempts[0] == 0 &&
                    g_Scorefile->characters[4].spells[id].attempts[1] == 0)
                {
                    g_AsciiManager->create_stringf(&pos, "SCORE        00  ----/----");
                }
                else
                {
                    ScorefileSpell *spell = &g_Scorefile->characters[menu_5cec.next_selection].spells[id];
                    g_AsciiManager->create_stringf(&pos, "SCORE %8d0  %4d/%4d", spell->practice_score,
                                                   spell->captures[1], spell->attempts[1]);
                    pos.y += 10.0f;
                    g_AsciiManager->color.d3d =
                        g_Scorefile->characters[menu_5cec.next_selection].spells[id].captures[0] != 0 ? 0xff206060
                                                                                                      : 0xff404040;
                    if (g_spell_difficulty[id] <= 4)
                    {
                        spell = &g_Scorefile->characters[menu_5cec.next_selection].spells[id];
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

// The manual (help.anm), shown until HelpManual says it is done.
// TODO: the first create_effect call swaps eax and ecx (g_AsciiManager and the result slot).
// FUNCTION: TH16 0x4545a0
i32 TitleInf::do_manual()
{
    switch (substate)
    {
    case 0:
        if (anm_id_73c.id == 0)
        {
            anm_id_73c = g_AsciiManager->ascii_anm->create_effect(0x13, -1, NULL);
        }
        anm_ids[0x72] = title_anm->create_effect(0x72, -1, NULL);
        HelpManual::create();
        substate = 1;
        time_in_state.reset();
        g_HelpManual->unk_128 = 128.0f;
        break;
    case 1:
        if (g_HelpManual->unk_124 != 0)
        {
            AnmManager::interrupt_tree(anm_id_73c, 1);
            anm_id_73c.id = 0;
            interrupt_and_clear(0x72);
            set_state(1);
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

// Spell practice: picking the character, which reloads the spell list.
// TODO: the original reserves a dead 4-byte local (push ecx) and tests the input after the store.
// FUNCTION: TH16 0x455790
i32 TitleInf::do_spell_practice_character()
{
    MenuHelper *character_menu = &menu_5cec;
    character_menu->current_selection = character_menu->next_selection;
    if (input_pressed_or_repeating(INPUT_LEFT))
    {
        g_SoundManager.play_sound_centered(10, 0);
        AnmManager::interrupt_tree_and_run(anm_ids[0x11c], (i16)(character_menu->next_selection + 25));
        character_menu->move_cursor(-1);
        AnmManager::interrupt_tree_and_run(anm_ids[0x11c], (i16)(character_menu->next_selection + 13));
        g_Globals.character = character_menu->next_selection;
        if (state == 18)
        {
            load_spell_list(spell_stage, menu.next_selection, spell_ids, 0);
        }
        else if (state == 19)
        {
            load_spell_list(spell_stage, spell_row, spell_ids, 0);
            highlight_spell_row(menu.next_selection);
        }
    }
    if (input_pressed_or_repeating(INPUT_RIGHT))
    {
        g_SoundManager.play_sound_centered(10, 0);
        AnmManager::interrupt_tree_and_run(anm_ids[0x11c], (i16)(character_menu->next_selection + 19));
        character_menu->move_cursor(1);
        AnmManager::interrupt_tree_and_run(anm_ids[0x11c], (i16)(character_menu->next_selection + 7));
        g_Globals.character = character_menu->next_selection;
        if (state == 18)
        {
            load_spell_list(spell_stage, menu.next_selection, spell_ids, 0);
        }
        else if (state == 19)
        {
            load_spell_list(spell_stage, spell_row, spell_ids, 0);
            highlight_spell_row(menu.next_selection);
        }
    }
    return 0;
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
// TODO: the original realigns its frame (and esp, -8) and keeps both input words in registers for the cursor tests.
// FUNCTION: TH16 0x455d50
i32 TitleInf::do_spell_practice_subseason()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = 4;
        menu.set_cursor(0);
        if (get_vm_or_clear(anm_ids[0xd9]) == NULL)
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
            g_SoundManager.play_sound_centered(10, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd9], 3);
            AnmManager::interrupt_tree(anm_ids[0xd9], (i16)(menu.next_selection + 7));
        }
        do_spell_practice_character();
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(9, 0);
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            AnmManager::interrupt_tree(anm_ids[0xd9], 6);
            set_substate(3);
            g_SoundManager.play_sound_centered(7, 0);
            g_Supervisor.fade_out_bgm(0.05f);
            g_SoundManager.play_sound_centered(50, 0);
            return 1;
        }
        break;
    case 3:
        if (time_in_state.current == 10)
        {
            g_AsciiManager->show_now_loading(480.0f, 392.0f);
            AnmId id;
            id = g_EffectManager->create_ui_effect(0, NULL, NULL);
            g_Supervisor.config.unk_0 = id.id;
            AnmManager::interrupt_tree(id, 7);
        }
        if (time_in_state.current >= 40)
        {
            menu.push();
            set_state(2);
            g_unk_4a6f1c = 5;
            i32 stage = spell_stage + 1;
            g_Globals.stage_num = stage;
            g_Globals.weird_stage_num = stage;
            g_stage_data = &g_stage_table[stage];
            g_Globals.spell_id = spell_ids[spell_index];
            g_Globals.character = menu_5cec.next_selection;
            g_Globals.subshot = 0;
            g_Globals.subseason = menu.next_selection;
            g_Supervisor.gamemode_to_switch_to = 7;
            g_Globals.difficulty = g_spell_difficulty[spell_ids[spell_index]];
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
            set_state(19);
            menu.pop();
        }
        break;
    }
    return 1;
}

// Spell practice: picking the spell card (the difficulty row). Extra stage
// cards start the game right away; the others go on to the subseason.
// TODO: the original realigns its frame (and esp, -8) and keeps both input words in registers for the cursor tests.
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
            g_SoundManager.play_sound_centered(10, 0);
            highlight_spell_row(menu.next_selection);
        }
        do_spell_practice_character();
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(9, 0);
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            AnmManager::interrupt_tree(anm_ids_740[menu.next_selection], 6);
            set_substate(3);
            g_SoundManager.play_sound_centered(7, 0);
            if (spell_stage == 6)
            {
                g_Supervisor.fade_out_bgm(0.05f);
                g_SoundManager.play_sound_centered(50, 0);
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
                id = g_EffectManager->create_ui_effect(0, NULL, NULL);
                g_Supervisor.config.unk_0 = id.id;
                AnmManager::interrupt_tree(id, 7);
            }
            if (time_in_state.current >= 40)
            {
                menu.push();
                set_state(2);
                g_unk_4a6f1c = 5;
                i32 stage = spell_stage + 1;
                g_Globals.stage_num = stage;
                g_Globals.weird_stage_num = stage;
                g_stage_data = &g_stage_table[stage];
                g_Globals.spell_id = spell_ids[menu.next_selection];
                g_Globals.character = menu_5cec.next_selection;
                g_Globals.subshot = 0;
                g_Globals.subseason = 4;
                g_Supervisor.gamemode_to_switch_to = 7;
                g_Globals.difficulty = g_spell_difficulty[spell_ids[menu.next_selection]];
                g_spell_practice_last_stage = spell_stage;
                g_spell_practice_last_row = spell_row;
                g_spell_practice_last_index = menu.next_selection;
                return 1;
            }
        }
        else if (time_in_state.current >= 14)
        {
            spell_index = menu.next_selection;
            set_state(20);
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
            set_state(18);
            menu.pop();
        }
        break;
    }
    return 1;
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
            if (get_vm_or_clear(anm_ids_740[i]) != NULL)
            {
                AnmManager::interrupt_tree(anm_ids_740[i], 2);
            }
            AnmManager::interrupt_tree_and_run(anm_ids[0x10d + g_spell_difficulty[spell_ids[i]]], 2);
        }
        else
        {
            if (get_vm_or_clear(anm_ids_740[i]) != NULL)
            {
                AnmManager::interrupt_tree(anm_ids_740[i], 3);
            }
            AnmManager::interrupt_tree_and_run(anm_ids[0x10d + g_spell_difficulty[spell_ids[i]]], 3);
        }
    }
    return 0;
}
