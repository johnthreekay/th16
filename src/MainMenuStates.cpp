// States of the title screen's menus (TitleInf::on_tick dispatches on
// state).
#include <direct.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
extern i32 g_spell_practice_last_stage;
extern i32 g_practice_last_stage;
extern i32 g_spell_practice_last_row;
BOOL __stdcall spell_practice_row_seen(i32 stage, i32 row);

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

// Picking the subseason before a game (Extra has only one). In stage
// practice this goes on to the stage select instead of starting.
// TODO: the original realigns its frame (and esp, -8).
// FUNCTION: TH16 0x450af0
i32 TitleInf::do_subseason_select()
{
    i32 script = (g_Globals.difficulty == 4) * 2 + 0x97;
    switch (substate)
    {
    case 0:
        menu.num_choices = 4;
        if (g_Globals.difficulty == 4)
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
        if (g_unk_4a6f1c == 4)
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
        if (g_Globals.difficulty != 4)
        {
            if (input_pressed_or_repeating(INPUT_LEFT))
            {
                g_SoundManager.play_sound_centered(10, 0);
                AnmManager::interrupt_tree_and_run(anm_ids[script], (i16)(menu.next_selection + 25));
                menu.move_cursor(-1);
                AnmManager::interrupt_tree(anm_ids[script], (i16)(menu.next_selection + 13));
            }
            if (input_pressed_or_repeating(INPUT_RIGHT))
            {
                g_SoundManager.play_sound_centered(10, 0);
                AnmManager::interrupt_tree_and_run(anm_ids[script], (i16)(menu.next_selection + 19));
                menu.move_cursor(1);
                AnmManager::interrupt_tree(anm_ids[script], (i16)(menu.next_selection + 7));
            }
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(9, 0);
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            g_SoundManager.play_sound_centered(7, 0);
            set_substate(3);
            g_SoundManager.play_sound_centered(50, 0);
            if (g_Globals.game_mode == 0)
            {
                g_Supervisor.fade_out_bgm(0.05f);
                return 1;
            }
        }
        break;
    case 3:
        if (time_in_state.current == 10)
        {
            if (g_Globals.game_mode != 0)
            {
                goto confirm;
            }
            g_AsciiManager->show_now_loading(480.0f, 392.0f);
            AnmId id;
            id = g_EffectManager->create_ui_effect(0, NULL, NULL);
            g_Supervisor.config.unk_0 = id.id;
            AnmManager::interrupt_tree(id, 7);
        }
        goto start;
    confirm:
        g_Globals.subseason = menu.next_selection;
        menu.push();
        AnmManager::interrupt_tree(anm_ids[script], 1);
        anm_ids[script].id = 0;
        AnmManager::interrupt_tree(anm_ids[0x6a], 1);
        anm_ids[0x6a].id = 0;
        set_state(8);
    start:
        if (time_in_state.current >= 40)
        {
            if (g_Globals.difficulty != 4)
            {
                g_Globals.subseason = menu.next_selection;
            }
            else
            {
                g_Globals.subseason = 4;
            }
            menu.push();
            g_Globals.spell_id = -1;
            set_state(2);
            if (g_Globals.difficulty < 4)
            {
                g_stage_data = &g_stage_table[1];
                g_Globals.stage_num = 1;
                g_Globals.weird_stage_num = 1;
                g_Supervisor.gamemode_to_switch_to = 7;
                return 1;
            }
            g_stage_data = &g_stage_table[7];
            g_Globals.stage_num = 7;
            g_Globals.weird_stage_num = 7;
            g_Supervisor.gamemode_to_switch_to = 7;
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
            set_state(6);
            menu.pop();
        }
        break;
    }
    return 1;
}

// The keyboard state the practice menu reads when a stage is picked.
// GLOBAL: TH16 0x4dfa48
u8 g_practice_keys[0x100];
// Set from the number key held when a practice stage is picked (0 if
// none).
// GLOBAL: TH16 0x4a5bf8
i32 g_unk_4a5bf8;

// Stage practice: picking the stage.
// TODO: the original realigns its frame (and esp, -8) and keeps both input words in registers for the cursor tests.
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
        if (g_unk_4a6f1c == 4)
        {
            g_unk_4a6f1c = 1;
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
        if ((g_hardware_input_pressed & INPUT_UP) || (g_hardware_input_repeat & INPUT_UP))
        {
            menu.move_cursor(-1);
        }
        if ((g_hardware_input_pressed & INPUT_DOWN) || (g_hardware_input_repeat & INPUT_DOWN))
        {
            menu.move_cursor(1);
        }
        if (menu.current_selection != menu.next_selection)
        {
            g_SoundManager.play_sound_centered(10, 0);
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(9, 0);
            g_practice_last_stage = menu.next_selection;
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            if (!g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
                     .practice[g_Globals.difficulty][menu.next_selection + 1]
                     .unlocked)
            {
                g_SoundManager.play_sound_centered(16, 0);
                return 1;
            }
            set_substate(3);
            g_SoundManager.play_sound_centered(7, 0);
            g_SoundManager.play_sound_centered(50, 0);
            g_practice_last_stage = menu.next_selection;
            g_unk_4a5bf8 = 0;
            if (get_keyboard_state(g_practice_keys) == 0)
            {
                if (g_practice_keys['1'] & 0x80)
                {
                    g_unk_4a5bf8 = 1;
                }
                else if (g_practice_keys['2'] & 0x80)
                {
                    g_unk_4a5bf8 = 2;
                }
                else if (g_practice_keys['3'] & 0x80)
                {
                    g_unk_4a5bf8 = 3;
                }
                else if (g_practice_keys['4'] & 0x80)
                {
                    g_unk_4a5bf8 = 4;
                }
                else if (g_practice_keys['5'] & 0x80)
                {
                    g_unk_4a5bf8 = 5;
                }
                else if (g_practice_keys['6'] & 0x80)
                {
                    g_unk_4a5bf8 = 6;
                }
                else if (g_practice_keys['7'] & 0x80)
                {
                    g_unk_4a5bf8 = 7;
                }
                else if (g_practice_keys['8'] & 0x80)
                {
                    g_unk_4a5bf8 = 8;
                }
                else if (g_practice_keys['9'] & 0x80)
                {
                    g_unk_4a5bf8 = 9;
                }
            }
            else
            {
                if (g_practice_keys[DIK_1] & 0x80)
                {
                    g_unk_4a5bf8 = 1;
                }
                else if (g_practice_keys[DIK_2] & 0x80)
                {
                    g_unk_4a5bf8 = 2;
                }
                else if (g_practice_keys[DIK_3] & 0x80)
                {
                    g_unk_4a5bf8 = 3;
                }
                else if (g_practice_keys[DIK_4] & 0x80)
                {
                    g_unk_4a5bf8 = 4;
                }
                else if (g_practice_keys[DIK_5] & 0x80)
                {
                    g_unk_4a5bf8 = 5;
                }
                else if (g_practice_keys[DIK_6] & 0x80)
                {
                    g_unk_4a5bf8 = 6;
                }
                else if (g_practice_keys[DIK_7] & 0x80)
                {
                    g_unk_4a5bf8 = 7;
                }
                else if (g_practice_keys[DIK_8] & 0x80)
                {
                    g_unk_4a5bf8 = 8;
                }
                else if (g_practice_keys[DIK_9] & 0x80)
                {
                    g_unk_4a5bf8 = 9;
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
            id = g_EffectManager->create_ui_effect(0, NULL, NULL);
            g_Supervisor.config.unk_0 = id.id;
            AnmManager::interrupt_tree(id, 7);
        }
        if (time_in_state.current >= 40)
        {
            menu.push();
            set_state(2);
            i32 stage = menu.next_selection + 1;
            g_Supervisor.gamemode_to_switch_to = 7;
            g_Globals.stage_num = stage;
            g_Globals.weird_stage_num = stage;
            g_unk_4a6f1c = 4;
            g_stage_data = &g_stage_table[stage];
            g_practice_last_stage = menu.next_selection;
            return 1;
        }
        break;
    case 4:
        if (time_in_state.current >= 6)
        {
            AnmManager::interrupt_tree(anm_ids[0x71], 1);
            anm_ids[0x71].id = 0;
            set_state(7);
            menu.pop();
        }
        break;
    }
    return 1;
}

// Player data, spell card page: ten spell cards of the chosen difficulty
// (page menu_1d4 - 1), numbered with full-width digits, with their names
// once seen and the chosen character's captures.
// TODO: ours realigns the frame (and esp, -8) and divides by 10 with a multiply; the original uses idiv by a register.
// FUNCTION: TH16 0x452c30
i32 TitleInf::draw_spell_card_page()
{
    i32 skip = menu_1d4.next_selection * 10 - 10;
    i32 id = 0;
    for (i32 seen = 0; seen < skip; id++)
    {
        if (g_spell_difficulty[id] == menu_fc.next_selection)
        {
            seen++;
        }
    }
    const char *digits[10] = {"\x82\x4f", "\x82\x50", "\x82\x51", "\x82\x52", "\x82\x53",
                              "\x82\x54", "\x82\x55", "\x82\x56", "\x82\x57", "\x82\x58"};
    char name[0xa5];
    AnmId *row_id = anm_ids_740;
    for (i32 row = 0; row < 10; row++, row_id++)
    {
        for (; id < 0x77; id++)
        {
            if (g_spell_difficulty[id] == menu_fc.next_selection)
            {
                break;
            }
        }
        if (id >= 0x77)
        {
            for (; row < 10; row++)
            {
                g_AnmManager->draw_text_centered(get_vm_or_clear(anm_ids_740[row]), 0xffffffff, 0, 0, 0, " ");
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
            id++;
            const char *ones = digits[id % 10];
            const char *tens = id / 10 % 10 == 0 && id / 100 == 0 ? "\x81\x40" : digits[id / 10 % 10];
            const char *hundreds = id / 100 != 0 ? digits[id / 100] : "\x81\x40";
            ScorefileSpell *spell = &g_Scorefile->characters[menu.next_selection].spells[id - 1];
            g_AnmManager->draw_text_centered(get_vm_or_clear(*row_id), spell->captures[0] != 0 ? 0xffff80 : 0xefefef, 0,
                                             0, 0, "No.%s%s%s %s %4d/%4d", hundreds, tens, ones, name,
                                             spell->captures[0], spell->attempts[0]);
        }
        else
        {
            id++;
            const char *ones = digits[id % 10];
            const char *tens = id / 10 % 10 == 0 && id / 100 == 0 ? "\x81\x40" : digits[id / 10 % 10];
            const char *hundreds = id / 100 != 0 ? digits[id / 100] : "\x81\x40";
            ScorefileSpell *spell = &g_Scorefile->characters[menu.next_selection].spells[id - 1];
            g_AnmManager->draw_text_centered(
                get_vm_or_clear(*row_id), 0x808080, 0, 0, 0,
                "No.%s%s%s \x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H\x81H"
                "\x81H\x81H\x81H\x81H %4d/%4d",
                hundreds, tens, ones, spell->captures[0], spell->attempts[0]);
        }
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
// FUNCTION: TH16 0x456d50
HARNESS_CALLED i32 TitleInf::on_draw__spell_practice_histories()
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

// Name tables of the replay lists.
// GLOBAL: TH16 0x4918c0
const char *const g_season_names[5] = {"Spring", "Summer", "Autumn", "Winter", "Full  "};
// GLOBAL: TH16 0x4918d4
const char *const g_difficulty_letters[6] = {"E ", "N ", "H ", "L ", "EX", "OD"};
// GLOBAL: TH16 0x4918f0
const char *const g_stage_short_names[8] = {"tst", "St1", "St2", "St3", "St4", "St5", "St6", "Ex "};
// GLOBAL: TH16 0x491910
const char *const g_all_stages_names[2] = {"All", "ExA"};
// GLOBAL: TH16 0x491950
const char *const g_difficulty_names[6] = {"Easy   ", "Normal ", "Hard   ", "Lunatic", "Extra  ", "O.D.   "};
// GLOBAL: TH16 0x491970
const char *const g_character_names[4] = {"Reimu  ", "Cirno  ", "Aya    ", "Marisa "};
// The characters of the name entry grid; the last three are drawn as the
// special glyphs 0x81, 0x7f and 0x80.
// GLOBAL: TH16 0x492840
const char g_name_entry_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-=.,!?@:;[]()_/{}|~^#$%&*   ";

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
        for (i32 i = menu_1d4.next_selection * 25; i < (menu_1d4.next_selection + 1) * 25; i++)
        {
            ReplayManager **replay = &replays[i];
            g_AsciiManager->color.d3d = menu.next_selection == i % 25 ? 0xffffff00 : 0xff808080;
            if (*replay != NULL)
            {
                RpyInfo *info = (*replay)->info;
                struct tm *time = localtime(&info->timestamp);
                if (menu_1d4.next_selection == 0)
                {
                    if (!(info->unk_0[0xa] & 2))
                    {
                        g_AsciiManager->create_stringf(
                            &pos, g_replay_list_formats[0][0], i + 1, (const char *)info->unk_0, time->tm_year % 100,
                            time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
                            g_character_names[info->character + info->subshot], g_season_names[info->subseason],
                            g_difficulty_names[info->difficulty], g_stage_short_names[info->stage], info->slowdown);
                    }
                    else
                    {
                        g_AsciiManager->create_stringf(&pos, g_replay_spell_format[0], i + 1, (const char *)info->unk_0,
                                                       time->tm_year % 100, time->tm_mon + 1, time->tm_mday,
                                                       time->tm_hour, time->tm_min,
                                                       g_character_names[info->character + info->subshot],
                                                       g_season_names[info->subseason], info->spell_id + 1,
                                                       info->slowdown);
                    }
                }
                else
                {
                    memcpy(g_replay_user_number, &(*replay)->filename[7], 4);
                    g_replay_user_number[4] = '\0';
                    g_AsciiManager->create_stringf(
                        &pos, g_replay_list_formats[1][0], g_replay_user_number, (const char *)info->unk_0,
                        time->tm_year % 100, time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
                        g_character_names[info->character + info->subshot], g_difficulty_names[info->difficulty],
                        g_season_names[info->subseason], g_stage_short_names[info->stage], info->slowdown);
                }
            }
            else
            {
                g_AsciiManager->create_stringf(&pos, g_replay_list_formats[menu_1d4.next_selection != 0][1], i + 1);
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
        if (menu_1d4.next_selection == 0)
        {
            if (!(info->unk_0[0xa] & 2))
            {
                g_AsciiManager->create_stringf(
                    &pos, g_replay_list_formats[0][0], replay_slot + 1, (const char *)info->unk_0, time->tm_year % 100,
                    time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
                    g_character_names[info->character + info->subshot], g_season_names[info->subseason],
                    g_difficulty_names[info->difficulty], g_stage_short_names[info->stage], info->slowdown);
            }
            else
            {
                g_AsciiManager->create_stringf(&pos, g_replay_spell_format[0], replay_slot + 1, (const char *)info->unk_0,
                                               time->tm_year % 100, time->tm_mon + 1, time->tm_mday, time->tm_hour,
                                               time->tm_min, g_character_names[info->character + info->subshot],
                                               g_season_names[info->subseason], info->spell_id + 1, info->slowdown);
            }
        }
        else
        {
            memcpy(g_replay_user_number, &replays[replay_slot]->filename[7], 4);
            g_replay_user_number[4] = '\0';
            g_AsciiManager->create_stringf(
                &pos, g_replay_list_formats[1][0], g_replay_user_number, (const char *)info->unk_0,
                time->tm_year % 100, time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
                g_character_names[info->character + info->subshot], g_difficulty_names[info->difficulty],
                g_stage_short_names[info->stage], g_season_names[info->subseason], info->slowdown);
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
    i32 difficulty = menu_fc.next_selection;
    g_AsciiManager->draw_shadows = 1;
    if (menu_1d4.next_selection == 0)
    {
        for (i32 i = 0; i < 10; i++)
        {
            g_AsciiManager->color.d3d = ~(i * 16) | 0xffffff00;
            ScorefileScore *score = &g_Scorefile->characters[menu.next_selection].scores[difficulty][i];
            if (score->timestamp != 0)
            {
                struct tm *time = localtime(&score->timestamp);
                g_AsciiManager->create_stringf(&pos, "%2d  %s  %9ld%d  %.4d/%.2d/%.2d %.2d:%.2d %s  %s  %2.1f%%", i + 1,
                                               score->name, score->score, score->continues_used,
                                               time->tm_year + 1900, time->tm_mon + 1, time->tm_mday, time->tm_hour,
                                               time->tm_min, g_season_names[score->subseason],
                                               g_stage_names[score->stage], score->slowdown);
            }
            else
            {
                g_AsciiManager->create_stringf(&pos, "%2d  %s  %9ld%d  ----/--/-- --:-- Season  Stage -  ---%%",
                                               i + 1, score->name, score->score, score->continues_used,
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
    __int64 seconds = g_Scorefile->characters[menu.next_selection].play_time / 100;
    __int64 minutes = seconds / 60;
    __int64 hours = minutes / 60;
    g_AsciiManager->create_stringf(&pos, "%3lld:%.2lld:%.2lld", hours, minutes - hours * 60, seconds - minutes * 60);
    pos.y = 414.0f;
    g_AsciiManager->create_stringf(&pos, "    %5d",
                                   g_Scorefile->characters[menu.next_selection].difficulty_play_counts[difficulty]);
    g_AsciiManager->color.d3d = 0xffffffff;
    g_AsciiManager->draw_shadows = 0;
    return 1;
}

// The high score name entry after a game: the top ten of the character and
// difficulty played, and while a name is entered, the name and the
// character grid.
// TODO: register allocation: the original keeps the index in esi and g_AsciiManager in edx (swapped).
// FUNCTION: TH16 0x4538b0
HARNESS_CALLED i32 TitleInf::on_draw__4538b0()
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
        g_AsciiManager->color.d3d =
            unk_5a58 != 0 ? ~(i * 16) | 0xffffff00 : (menu.next_selection != i ? 0xff808040 : 0xffffffff);
        ScorefileScore *score =
            &g_Scorefile->characters[g_Globals.subshot + g_Globals.character].scores[difficulty][i];
        if (score->timestamp != 0)
        {
            struct tm *time = localtime(&score->timestamp);
            g_AsciiManager->create_stringf(&pos, "%2d  %s  %9ld%d  %.4d/%.2d/%.2d %.2d:%.2d %s  %s  %2.1f%%", i + 1,
                                           score->name, score->score, score->continues_used, time->tm_year + 1900,
                                           time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
                                           g_season_names[score->subseason], g_stage_names[score->stage],
                                           score->slowdown);
        }
        else
        {
            g_AsciiManager->create_stringf(&pos, "%2d  %s  %9ld%d  ----/--/-- --:-- Season  Stage -  ---%%", i + 1,
                                           score->name, score->score, score->continues_used, score->slowdown);
        }
        pos.y += 18.0f;
    }
    if (unk_5a58 != 0)
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
    for (i32 i = 0; i < 91; i++)
    {
        g_AsciiManager->color.d3d = menu_5a5c.next_selection == i ? 0xffffff00 : 0xff808080;
        i32 c;
        if (i < 88)
        {
            c = g_name_entry_chars[i];
        }
        else if (i == 88)
        {
            c = 0x81;
        }
        else
        {
            c = (i != 89) + 0x7f;
        }
        g_AsciiManager->create_stringf(&pos, "%c", c);
        if (i % 13 == 12)
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
HARNESS_CALLED i32 TitleInf::on_draw__4541b0()
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
        for (i32 i = 0; i < 25; replay++)
        {
            g_AsciiManager->color.d3d = menu.next_selection == i ? 0xffffff00 : 0xff808080;
            if (*replay != NULL)
            {
                RpyInfo *info = (*replay)->info;
                struct tm *time = localtime(&info->timestamp);
                i++;
                g_AsciiManager->create_stringf(&pos, "No.%.2d %s %.2d/%.2d/%.2d %.2d:%.2d %s %s %s %2.1f%%", i,
                                               (const char *)info->unk_0, time->tm_year % 100, time->tm_mon + 1,
                                               time->tm_mday, time->tm_hour, time->tm_min,
                                               g_character_names[info->character + info->subshot],
                                               g_difficulty_names[info->difficulty],
                                               g_stage_short_names[info->stage], info->slowdown);
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
                                       g_character_names[info->character + info->subshot],
                                       g_difficulty_names[info->difficulty], g_all_stages_names[0],
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
            for (i32 i = 0; i < 91; i++)
            {
                g_AsciiManager->color.d3d = menu_5a5c.next_selection == i ? 0xffffff00 : 0xff808080;
                i32 c;
                if (i < 88)
                {
                    c = g_name_entry_chars[i];
                }
                else if (i == 88)
                {
                    c = 0x81;
                }
                else
                {
                    c = (i != 89) + 0x7f;
                }
                g_AsciiManager->create_stringf(&pos, "%c", c);
                if (i % 13 == 12)
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

// Spell practice: picking the stage. Coming back from a game goes straight
// on to the spell card list of the last stage.
// TODO: the original realigns its frame (and esp, -8) and keeps both input words in registers for the cursor tests.
// FUNCTION: TH16 0x4553d0
i32 TitleInf::do_spell_practice_stage_select()
{
    switch (substate)
    {
    case 0:
        if (anm_id_73c.id == 0)
        {
            anm_id_73c = g_AsciiManager->ascii_anm->create_effect(0x13, -1, NULL);
        }
        menu.num_choices = 7;
        if (get_vm_or_clear(anm_ids[0x11c]) == NULL)
        {
            anm_ids[0x11c] = title_anm->create_effect(0x11c, -1, NULL);
        }
        if (get_vm_or_clear(anm_ids[0xd7]) == NULL)
        {
            anm_ids[0xd7] = title_anm->create_effect(0xd7, -1, NULL);
        }
        set_substate(1);
        if (g_spell_practice_last_stage >= 0)
        {
            menu.set_cursor(g_spell_practice_last_stage);
            g_spell_practice_last_stage = -1;
            menu_5cec.wraps = 1;
            menu_5cec.num_choices = 4;
            menu_5cec.set_cursor(g_Globals.character + g_Globals.subshot);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd7], 3);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd7], (i16)(menu.next_selection + 7));
            AnmManager::interrupt_tree_and_run(anm_ids[0xd7], 6);
            AnmManager::interrupt_tree_and_run(anm_ids[0x11c], 3);
            AnmManager::interrupt_tree(anm_ids[0x11c], (i16)(menu_5cec.next_selection + 7));
            AnmManager::interrupt_tree(anm_ids[0x71], 1);
            anm_ids[0x71].id = 0;
            set_state(18);
            spell_stage = menu.next_selection;
            menu.push();
            menu.set_cursor(0);
            return 1;
        }
        anm_ids[0x71] = title_anm->create_effect(0x71, -1, NULL);
    case 1:
        if (time_in_state.current > 10)
        {
            set_substate(2);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd7], 3);
            AnmManager::interrupt_tree(anm_ids[0xd7], (i16)(menu.next_selection + 7));
            AnmManager::interrupt_tree_and_run(anm_ids[0x11c], 3);
            AnmManager::interrupt_tree(anm_ids[0x11c], (i16)(menu_5cec.next_selection + 7));
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
            AnmManager::interrupt_tree_and_run(anm_ids[0xd7], 3);
            AnmManager::interrupt_tree(anm_ids[0xd7], (i16)(menu.next_selection + 7));
        }
        do_spell_practice_character();
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(9, 0);
            g_practice_last_stage = menu.next_selection;
            return 1;
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            AnmManager::interrupt_tree(anm_ids[0xd7], 6);
            set_substate(3);
            g_SoundManager.play_sound_centered(7, 0);
            g_practice_last_stage = menu.next_selection;
            return 1;
        }
        break;
    case 3:
        if (time_in_state.current >= 20)
        {
            AnmManager::interrupt_tree(anm_ids[0x71], 1);
            anm_ids[0x71].id = 0;
            set_state(18);
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
            set_state(1);
            menu.pop();
            AnmManager::interrupt_tree(anm_id_73c, 1);
            anm_id_73c.id = 0;
        }
        break;
    }
    return 1;
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

// Spell practice: picking the boss attack (the row of spell cards) of the
// stage.
// TODO: the original realigns its frame (and esp, -8) and keeps both input words in registers for the cursor tests.
// FUNCTION: TH16 0x455900
i32 TitleInf::do_spell_practice_row()
{
    i32 row_counts[7] = {2, 3, 4, 3, 6, 9, 13};
    switch (substate)
    {
    case 0:
        menu.num_choices = row_counts[spell_stage];
        if (get_vm_or_clear(anm_ids[0x6b]) == NULL)
        {
            anm_ids[0x6b] = title_anm->create_effect(0x6b, -1, NULL);
        }
        if (get_vm_or_clear(anm_ids[0xd8]) == NULL)
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
            load_spell_list(spell_stage, menu.next_selection, spell_ids, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd8], 3);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd8], (i16)(menu.next_selection + 7));
            AnmManager::interrupt_tree_and_run(anm_ids[0xd8], 6);
            goto start_list;
        }
        load_spell_list(spell_stage, menu.next_selection, spell_ids, 0);
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
            g_SoundManager.play_sound_centered(10, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[0xd8], 3);
            AnmManager::interrupt_tree(anm_ids[0xd8], (i16)(menu.next_selection + 7));
            load_spell_list(spell_stage, menu.next_selection, spell_ids, 0);
        }
        do_spell_practice_character();
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            set_substate(4);
            g_SoundManager.play_sound_centered(9, 0);
        }
        else if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            if (!spell_practice_row_seen(spell_stage, menu.next_selection))
            {
                g_SoundManager.play_sound_centered(16, 0);
            }
            else
            {
                AnmManager::interrupt_tree(anm_ids[0xd8], 6);
                set_substate(3);
                g_SoundManager.play_sound_centered(7, 0);
            }
        }
        break;
    case 3:
        if (time_in_state.current >= 14)
        {
        start_list:
            set_state(19);
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
            for (i32 i = 0; i < 5; i++)
            {
                AnmManager::interrupt_tree(anm_ids_740[i], 1);
            }
            for (i32 i = 0; i < 7; i++)
            {
                g_AnmManager->delete_vm(anm_ids[0x10d + i]);
            }
            AnmManager::interrupt_tree(anm_ids[0x6b], 1);
            anm_ids[0x6b].id = 0;
            AnmManager::interrupt_tree(anm_ids[0xd8], 1);
            anm_ids[0xd8].id = 0;
            AnmManager::interrupt_tree(anm_ids[0xd7], 1);
            anm_ids[0xd7].id = 0;
            set_state(17);
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
