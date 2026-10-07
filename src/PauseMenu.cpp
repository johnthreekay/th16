#include <string.h>
#include <time.h>

#include "PauseMenu.h"
#include "ReplayManager.h"
#include "AsciiManager.h"
#include "FpsCounter.h"
#include "GameThread.h"
#include "Globals.h"
#include "Gui.h"
#include "Scorefile.h"
#include "Supervisor.h"

// GLOBAL: TH16 0x4a6ef4
PauseMenu *g_PauseMenu;

// FUNCTION: TH16 0x43e150
void PauseMenu::set_state(i32 state)
{
    prev_state = this->state;
    this->state = state;
    unk_1f4 = 0;
    time_in_current_menu.reset();
    time_since_pause_or_unpause.reset();
    menu_34.num_disabled = 0;
}

// FUNCTION: TH16 0x43e200
void PauseMenu::set_unk_1f4(i32 value)
{
    unk_1f4 = value;
    time_in_current_menu.reset();
}

// FUNCTION: TH16 0x43e250
i32 ScorefileChara::insert_score()
{
    ScorefileScore *entry = scores[g_Globals.difficulty];
    i32 i;
    for (i = 0; i < 10; i++)
    {
        if (entry->score <= g_Globals.score)
        {
            break;
        }
        entry++;
    }
    if (i >= 10)
    {
        return -1;
    }
    for (i32 j = 9; j > i; j--)
    {
        scores[g_Globals.difficulty][j] = scores[g_Globals.difficulty][j - 1];
    }
    entry->score = g_Globals.score;
    entry->continues = g_Globals.continues_used;
    entry->stage = g_Globals.stage_num;
    _time64(&entry->date);
    strcpy(entry->name, "        ");
    entry->slowdown = 100.0f - (f32)(g_FpsCounter->total_actual / g_FpsCounter->total_expected) * 100.0f;
    entry->subseason = g_Globals.subseason;
    return i;
}

// FUNCTION: TH16 0x43e350
PauseMenu::PauseMenu()
{
    memset(this, 0, sizeof(PauseMenu));
    flags |= 2;
    g_PauseMenu = this;
}

// FUNCTION: TH16 0x43e3f0
int PauseMenu::initialize()
{
    UpdateFunc *f;

    f = g_UpdateFuncRegistry->create_func(on_tick_thunk);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 10);
    on_tick_func = f;

    f = g_UpdateFuncRegistry->create_func(on_draw_thunk);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x4a);
    on_draw_func = f;

    time_in_current_menu.reset();
    time_since_pause_or_unpause.reset();
    return 0;
}

// FUNCTION: TH16 0x43e4c0
PauseMenu::~PauseMenu()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick_func);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func);
    for (i32 i = 0; i < 25; i++)
    {
        delete replays[i];
    }
    g_PauseMenu = NULL;
}

// FUNCTION: TH16 0x43e5a0
PauseMenu *PauseMenu::create()
{
    PauseMenu *menu = new PauseMenu();
    if (menu->initialize() != 0)
    {
        delete menu;
        return NULL;
    }
    return menu;
}

// The original's callback is a jmp to the member function, most likely the
// fastcall invoker of a capture-less lambda; a static thunk compiles the same.
// FUNCTION: TH16 0x43e720
int __fastcall PauseMenu::on_tick_thunk(void *arg)
{
    return ((PauseMenu *)arg)->on_tick();
}

// FUNCTION: TH16 0x43e730
void PauseMenu::draw_keyboard(Float3 pos)
{
    g_AsciiManager->create_stringf(&pos, "%s", name);
    pos.x += (f32)(name_cursor * 9);
    if (name_cursor == 8)
    {
        pos.x -= 9.0f;
    }
    g_AsciiManager->color.d3d = 0xffffff00;
    g_AsciiManager->create_stringf(&pos, "_");
    g_AsciiManager->color.d3d = 0xffffffff;
    pos.x = 48.0f;
    Float3 key_pos(112.0f, 320.0f, 0.0f);
    for (i32 i = 0; i < 0x5b; i++)
    {
        g_AsciiManager->color.d3d = menu.next_selection == i ? 0xffffff00 : 0xff808080;
        i32 c;
        if (i < 0x58)
        {
            c = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-=.,!?@:;[]()_/{}|~^#$%&*   "[i];
        }
        else if (i == 0x58)
        {
            c = 0x81;
        }
        else
        {
            c = i == 0x59 ? 0x7f : 0x80;
        }
        g_AsciiManager->create_stringf(&key_pos, "%c", c);
        if (i % 13 == 12)
        {
            key_pos.x = 112.0f;
            key_pos.y += 16.0f;
        }
        else
        {
            key_pos.x += 18.0f;
        }
    }
    g_AsciiManager->color.d3d = 0xffffffff;
}

// GLOBAL: TH16 0x491970
const char *g_chara_names_short[4] = {"Reimu  ", "Cirno  ", "Aya    ", "Marisa "};
// GLOBAL: TH16 0x4918ac
const char *g_season_names_short[5] = {"Sp", "Sm", "At", "Wt", "Fl"};
// GLOBAL: TH16 0x4918d4
const char *g_difficulty_names_short[6] = {"E ", "N ", "H ", "L ", "EX", "OD"};
// GLOBAL: TH16 0x4918f0
const char *g_stage_names_short[10] = {"tst", "St1", "St2", "St3", "St4", "St5", "St6", "Ex ", "All", "ExA"};
// GLOBAL: TH16 0x491920
const char *g_stage_names[10] = {"test   ", "Stage 1", "Stage 2", "Stage 3", "Stage 4",
                                 "Stage 5", "Stage 6", "Extra  ", "Clear  ", "ExClear"};

// The body of draw_replay_entry, which draw_replay_list inlines.
static __forceinline void draw_replay_entry_inline(i32 index, Float3 *pos, RpyInfo *info)
{
    tm *time = _localtime64(&info->timestamp);
    if (!(info->flags_a & 2))
    {
        g_AsciiManager->create_stringf(pos, "No.%.2d %s %.2d/%.2d/%.2d %s%s %s %s", index + 1, info->name,
                                       time->tm_year % 100, time->tm_mon + 1, time->tm_mday,
                                       g_chara_names_short[info->character + info->subshot],
                                       g_season_names_short[info->subseason],
                                       g_difficulty_names_short[info->difficulty], g_stage_names_short[info->stage]);
    }
    else
    {
        g_AsciiManager->create_stringf(pos, "No.%.2d %s %.2d/%.2d/%.2d %s%s Sp %3d", index + 1, info->name,
                                       time->tm_year % 100, time->tm_mon + 1, time->tm_mday,
                                       g_chara_names_short[info->character + info->subshot],
                                       g_season_names_short[info->subseason], info->spell_id + 1);
    }
}

// FUNCTION: TH16 0x43e8c0
HARNESS_CALLED void PauseMenu::draw_replay_entry(i32 index, Float3 *pos, RpyInfo *info)
{
    draw_replay_entry_inline(index, pos, info);
}

// FUNCTION: TH16 0x43e9b0
void PauseMenu::draw_replay_list()
{
    Float3 pos(36.0f, 64.0f, 0.0f);
    for (i32 i = 0; i < 25; i++)
    {
        g_AsciiManager->color.d3d = menu_34.next_selection == i ? 0xffffff00 : 0xff808080;
        if (replays[i] != NULL)
        {
            draw_replay_entry_inline(i, &pos, replays[i]->info);
        }
        else
        {
            g_AsciiManager->create_stringf(&pos, "No.%.2d -------- --/--/-- ------ -- -- St-", i + 1);
        }
        pos.y += 15.0f;
    }
    g_AsciiManager->color.d3d = 0xffffffff;
}

// FUNCTION: TH16 0x43eb50
void PauseMenu::draw_replay_name_entry()
{
    Float3 pos;
    pos.z = 0.0f;
    i32 selection = menu_34.next_selection;
    if (time_in_current_menu.current < 10)
    {
        f32 start = selection * 15.0f + 64.0f;
        pos.y = (224.0f - start) * time_in_current_menu.current / 10.0f + start;
    }
    else
    {
        pos.y = 224.0f;
    }
    pos.x = 90.0f;
    draw_keyboard(pos);
    pos.x = 36.0f;
    strcpy(g_ReplayManager->info->name, "        ");
    draw_replay_entry(selection, &pos, g_ReplayManager->info);
}

// FUNCTION: TH16 0x43ec10
void PauseMenu::draw_high_scores()
{
    Float3 pos;
    pos.x = 48.0f;
    pos.z = 0.0f;
    i32 selection = menu_34.next_selection;
    pos.y = 64.0f;
    g_AsciiManager->create_stringf(&pos, "            Score Ranking!!");
    pos.x = 75.0f;
    pos.y = selection * 18.0f + 96.0f;
    if (unk_200 == 0)
    {
        draw_keyboard(pos);
    }
    else
    {
        selection = -1;
    }
    pos.x = 48.0f;
    pos.y = 96.0f;
    for (i32 i = 0; i < 10;)
    {
        g_AsciiManager->color.d3d = i == selection ? 0xffffff00 : 0xff808080;
        ScorefileScore *entry =
            &((ScorefileData *)g_Scorefile)->charas[g_Globals.subshot + g_Globals.character].scores[g_Globals.difficulty][i];
        if (entry->date != 0)
        {
            tm *time = _localtime64(&entry->date);
            i++;
            g_AsciiManager->create_stringf(&pos, "%2d %s %9ld%d %.2d/%.2d/%.2d %s", i, entry->name, entry->score,
                                           entry->continues, time->tm_year % 100, time->tm_mon + 1, time->tm_mday,
                                           g_stage_names[entry->stage]);
        }
        else
        {
            i++;
            g_AsciiManager->create_stringf(&pos, "%2d %s %9ld%d --/--/-- Stage -", i, entry->name, entry->score,
                                           entry->continues);
        }
        pos.y += 18.0f;
    }
    g_AsciiManager->color.d3d = 0xffffffff;
}

// GLOBAL: TH16 0x4a5bfc
i32 g_continues_remaining;

// FUNCTION: TH16 0x43edc0
int PauseMenu::on_draw()
{
    g_AsciiManager->draw_shadows = 1;
    // get_vm_or_clear and find_child_of, with g_AnmManager read once: the
    // original's LTCG knew get_vm_with_id leaves it alone, which the opaque
    // stub hides from ours.
    AnmManager *anm_manager = g_AnmManager;
    if (anm_manager->get_vm_with_id(anm_id_1e8) == NULL)
    {
        anm_id_1e8.id = 0;
    }
    else if (anm_manager->get_vm_with_id(anm_id_1e8) == NULL)
    {
        anm_id_1e8.id = 0;
    }
    else
    {
        AnmVm *vm = anm_manager->get_vm_with_id(anm_id_1e8);
        if (vm == NULL)
        {
            anm_id_1e8.id = 0;
        }
        vm = vm->search_children(0x39, 0);
        if (vm != NULL)
        {
            g_Supervisor.vm_1c4->color_1.d3d = vm->color_1.d3d | 0xff000000;
        }
    }
    switch (state)
    {
    case 1:
    case 3:
        if ((flags_3ec & 3) == 1)
        {
            draw_replay_list();
        }
        else if ((flags_3ec & 3) == 2)
        {
            draw_replay_name_entry();
        }
        break;
    case 2:
        if (unk_1f4 == 15)
        {
            draw_high_scores();
        }
        if ((flags_3ec & 3) == 1)
        {
            draw_replay_list();
        }
        else if ((flags_3ec & 3) == 2)
        {
            draw_replay_name_entry();
        }
        else if (unk_1f4 != 14)
        {
            g_AsciiManager->create_stringf(&Float3(184.0f, 448.0f, 0.0f), "Credit %d", g_continues_remaining);
        }
        break;
    }
    g_AsciiManager->draw_shadows = 0;
    return 1;
}

// The original's callback is a jmp to the member function, most likely the
// fastcall invoker of a capture-less lambda; a static thunk compiles the same.
// FUNCTION: TH16 0x43ef10
int __fastcall PauseMenu::on_draw_thunk(void *arg)
{
    return ((PauseMenu *)arg)->on_draw();
}

extern double g_play_time_runtime;
double LTCG_VECTORCALL get_runtime();

// TODO: the original saves ecx and edi on entry (most likely an LTCG
// convention its caller, the undecompiled pause menu tick, asks for); ours
// saves edi only around the dialogue part.
// FUNCTION: TH16 0x43f6a0
void PauseMenu::leave_state_1()
{
    if (g_GameThread->replay_mode == 0)
    {
        g_play_time_runtime = get_runtime();
    }
    g_GameThread->flags.flag_4 = 0;
    g_game_speed = saved_game_speed;
    Gui *gui = g_Gui;
    if (gui->msg != NULL)
    {
        gui->msg->show();
    }
    AnmVm *vm = g_AnmManager->get_vm_with_id(gui->ids_11c[4]);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
    g_unk_4d9d90 = saved_global_4d9d90;
}

// FUNCTION: TH16 0x43f740
void PauseMenu::leave_state_2()
{
    if (g_GameThread->replay_mode == 0)
    {
        g_play_time_runtime = get_runtime();
    }
    AnmManager::interrupt_tree(anm_id_1e8, 1);
    AnmManager::interrupt_tree(anm_id_1e4, 1);
    g_game_speed = saved_game_speed;
}

// FUNCTION: TH16 0x43f790
void PauseMenu::leave_state_3()
{
    if (g_GameThread->replay_mode == 0)
    {
        g_play_time_runtime = get_runtime();
    }
    AnmManager::interrupt_tree(anm_id_1e8, 1);
    AnmManager::interrupt_tree(anm_id_1e4, 1);
    g_game_speed = saved_game_speed;
}
