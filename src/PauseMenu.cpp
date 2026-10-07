#include <stddef.h>
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
#include "SoundManager.h"
#include "Supervisor.h"

static_assert(offsetof(PauseMenu, name) == 0x2d4, "PauseMenu::name");
static_assert(offsetof(PauseMenu, saved_bgm_time) == 0x2e4, "PauseMenu::saved_bgm_time");
static_assert(offsetof(PauseMenu, flags_3ec) == 0x3ec, "PauseMenu::flags_3ec");
static_assert(offsetof(Gui, front_anm) == 0x2d8, "Gui::front_anm");

// GLOBAL: TH16 0x4a6ef4
PauseMenu *g_PauseMenu;

// The characters of the name entry grid (MainMenuStates.cpp).
extern const char g_name_entry_chars[];

// FUNCTION: TH16 0x43e150
HARNESS_CALLED void PauseMenu::set_state(i32 state)
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

#include "Input.h"

// TODO: the inlined timer ticks use other xmm registers for 1.0, 1.01 and the speed.
// FUNCTION: TH16 0x43e5f0
int PauseMenu::on_tick()
{
    switch (state)
    {
    case 0:
        if (!(g_Globals.flags_hi_45c & 1) && !g_GameThread->flags.flag_16 &&
            ((g_hardware_input_pressed & 0x100) || (g_Supervisor.flags & 0x10)) && g_GameThread->on_tick != NULL &&
            (g_GameThread->on_tick->flags & UPDATE_FUNC_ACTIVE) && g_GameThread->time_in_stage.current >= 30)
        {
            open();
        }
        break;
    case 1:
    case 2:
    case 3:
        tick_open();
        break;
    }
    time_in_current_menu.tick();
    time_since_pause_or_unpause.tick();
    return 1;
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
            c = g_name_entry_chars[i];
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
    if (get_vm_or_clear(anm_id_1e8) != NULL)
    {
        AnmVm *vm = find_child_of(anm_id_1e8, 0x39);
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

// TODO: the original realigns its frame to 8 bytes (whole-program, see README) and keeps vm in a stack slot, not ebx.
// FUNCTION: TH16 0x43ef20
void PauseMenu::take_snapshot()
{
    delete_vm_and_clear(anm_id_1e8);
    anm_id_1e8 = g_Supervisor.text_anm->create_ui_vm_at_origin(0x34, 0);
    // get_vm_or_clear with g_AnmManager read once (see on_draw).
    AnmManager *anm_manager = g_AnmManager;
    AnmVm *vm = anm_manager->get_vm_with_id(anm_id_1e8);
    if (vm == NULL)
    {
        anm_id_1e8.id = 0;
    }
    AnmLoadedSprite *sprite = &anm_manager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id];
    RECT dst;
    dst.left = (i32)sprite->start_pixel_inclusive.x;
    dst.top = (i32)sprite->start_pixel_inclusive.y;
    dst.right = (i32)(sprite->sprite_width + sprite->start_pixel_inclusive.x);
    dst.bottom = (i32)(sprite->sprite_height + sprite->start_pixel_inclusive.y);
    RECT src;
    src.left = (i32)(g_arcade_hud_origin_x - g_screen_coord_scale * 384.0f * 0.5f);
    src.top = g_arcade_hud_origin_y;
    src.right = (i32)(g_screen_coord_scale * 384.0f * 0.5f + g_arcade_hud_origin_x);
    src.bottom = (i32)(g_screen_coord_scale * 448.0f + g_arcade_hud_origin_y);
    AnmLoaded *loaded = anm_manager->loaded_anms[vm->anm_loaded_index];
    i32 entry = loaded->sprites[vm->sprite_id].image_file_num_in_anm;
    i32 slot = loaded->slot_num;
    IDirect3DSurface9 *src_surface = g_Supervisor.arcade_surface_1;
    if (anm_manager->loaded_anms[slot]->d3d[entry].texture != NULL)
    {
        anm_manager->flush_sprites();
        IDirect3DSurface9 *surface;
        if (anm_manager->loaded_anms[slot]->d3d[entry].texture->GetSurfaceLevel(0, &surface) == D3D_OK)
        {
            if (D3DXLoadSurfaceFromSurface(surface, NULL, &dst, src_surface, NULL, &src, D3DX_FILTER_POINT, 0) ==
                D3D_OK)
            {
            }
            surface->Release();
        }
    }
}

// FUNCTION: TH16 0x43f0f0
void PauseMenu::open()
{
    GameThread::update_play_time();
    set_state(1);
    GameThread *thread = g_GameThread;
    thread->flags.flag_4 = 1;
    front_anm = g_Gui->front_anm;
    delete_vm_and_clear(anm_id_1e4);
    if (thread->replay_mode != 0)
    {
        anm_id_1e4 = front_anm->create_ui_vm_at_origin(0x9e, 0);
    }
    else
    {
        anm_id_1e4 = front_anm->create_ui_vm_at_origin(0x9c, 0);
    }
    AnmManager::interrupt_tree(anm_id_1e4, 3);
    SoundManager::pause_sounds();
    g_SoundManager.play_sound_centered(0xe, 0);
    if (g_Globals.game_mode != 2)
    {
        g_SoundManager.modify_bgm(6, 0, "Pause");
    }
    while (SoundManager::update_sound_thread() != 0)
    {
    }
    take_snapshot();
    saved_game_speed = g_game_speed;
    g_game_speed = 1.0f;
    saved_global_4d9d90 = g_unk_4d9d90;
    g_unk_4d9d90 = 0;
    Gui *gui = g_Gui;
    if (gui->msg != NULL)
    {
        gui->msg->hide();
    }
    AnmVm *vm = g_AnmManager->get_vm_with_id(gui->ids_11c[4]);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
    flags_3ec &= ~4;
}

// FUNCTION: TH16 0x43f500
void game_over_43f500()
{
    PauseMenu *menu = g_PauseMenu;
    GameThread::update_play_time();
    if (g_GameThread->replay_mode == 1)
    {
        g_Supervisor.gamemode_to_switch_to = (g_Supervisor.flags & 0x2000) ? 2 : 4;
        return;
    }
    g_GameThread->flags.flag_4 = 1;
    menu->set_state(3);
    menu->set_unk_1f4_inline(3);
    if (g_Globals.game_mode == 2)
    {
        menu->set_unk_1f4(5);
    }
    if (g_Globals.game_mode == 0)
    {
        menu->anm_id_1e8 = g_Supervisor.text_anm->create_ui_vm_at_origin(0x34, 0);
        g_AnmManager->copy_screen_to_sprite(menu->anm_id_1e8, (i32)(g_screen_coord_scale * 32.0f),
                                            (i32)(g_screen_coord_scale * 16.0f), (i32)(g_screen_coord_scale * 384.0f),
                                            (i32)(g_screen_coord_scale * 448.0f));
    }
    Gui *gui = g_Gui;
    menu->front_anm = gui->front_anm;
    menu->unk_1fc = 1;
    menu->saved_game_speed = g_game_speed;
    g_game_speed = 1.0f;
    menu->saved_global_4d9d90 = g_unk_4d9d90;
    g_unk_4d9d90 = 1;
    if (gui->msg != NULL)
    {
        gui->msg->hide();
    }
    Gui::sub_42c580();
    menu->flags_3ec |= 4;
}

// TODO: ours folds the character offset into the practice index (one imul by 0xa63, scaled by 8); the original adds it to the pointer.
// FUNCTION: TH16 0x43f7e0
void PauseMenu::begin_score_entry()
{
    if (g_Globals.game_mode != 2)
    {
        if (g_Globals.game_mode != 0)
        {
            ScorefilePractice *practice = &g_Scorefile->characters[g_Globals.subshot + g_Globals.character]
                                               .practices[g_Globals.difficulty][g_Globals.stage_num - 1];
            if (practice->score < g_Globals.score)
            {
                practice->score = g_Globals.score;
            }
        }
        else
        {
            if (g_Globals.stage_num == 7 && unk_1fc != 0)
            {
                g_Globals.stage_num = 9;
            }
            i32 rank = ((ScorefileData *)g_Scorefile)->charas[g_Globals.subshot + g_Globals.character].insert_score();
            if (g_Globals.stage_num == 9 && unk_1fc != 0)
            {
                g_Globals.stage_num = 7;
            }
            if (rank >= 0)
            {
                menu_34.num_choices = 25;
                menu_34.wraps = 1;
                menu_34.set_cursor(rank);
                menu.set_cursor(0);
                menu.num_choices = 0x5b;
                menu.wraps = 1;
                strcpy(name, ((ScorefileData *)g_Scorefile)->status.name);
                name_cursor = 0;
                if (strcmp(name, "        ") != 0)
                {
                    menu.move_cursor(-1);
                }
                i32 i;
                for (i = 8; i > 0; i--)
                {
                    if (name[i - 1] != ' ')
                    {
                        break;
                    }
                }
                name_cursor = i;
                unk_200 = 0;
                return;
            }
        }
    }
    unk_200 = 1;
}

// FUNCTION: TH16 0x43f240
void replay_ended_43f240()
{
    PauseMenu *menu = g_PauseMenu;
    menu->set_state(1);
    menu->set_unk_1f4_inline(1);
    g_GameThread->flags.flag_4 = 1;
    menu->take_snapshot();
    menu->front_anm = g_Gui->front_anm;
    delete_vm_and_clear(menu->anm_id_1e4);
    menu->anm_id_1e4 = menu->front_anm->create_ui_vm_at_origin(0x9f, 0);
    AnmManager::interrupt_tree(menu->anm_id_1e4, 3);
    SoundManager::pause_sounds();
    g_SoundManager.modify_bgm(6, 0, "Pause");
    menu->saved_game_speed = g_game_speed;
    g_game_speed = 1.0f;
    menu->saved_global_4d9d90 = g_unk_4d9d90;
    g_unk_4d9d90 = 0;
    menu->flags_3ec &= ~4;
}

// TODO: the original keeps an ebp frame with a 4-byte pad (push ebp; push ecx), most likely known entry alignment through its callers (Player::on_tick_body, Gui::start_dialogue); ours has no frame (HARNESS_CALLED does not change it).
// FUNCTION: TH16 0x43f350
void pause_menu_43f350()
{
    PauseMenu *menu = g_PauseMenu;
    GameThread::update_play_time();
    if (g_GameThread->replay_mode == 1)
    {
        g_Supervisor.gamemode_to_switch_to = (g_Supervisor.flags & 0x2000) ? 2 : 4;
        return;
    }
    menu->set_state(2);
    menu->set_unk_1f4_inline(2);
    g_GameThread->flags.flag_4 = 1;
    SoundManager::pause_sounds();
    g_SoundManager.play_sound_centered(0xe, 0);
    if (g_Globals.game_mode != 2)
    {
        g_SoundManager.modify_bgm(6, 0, "Pause");
    }
    while (SoundManager::update_sound_thread() != 0)
    {
    }
    menu->take_snapshot();
    menu->front_anm = g_Gui->front_anm;
    if (g_Globals.game_mode != 2)
    {
        strcpy(menu->saved_bgm_name, g_SoundManager.get_bgm_name());
        menu->saved_bgm_time = g_SoundManager.bgm_play_time();
        g_Supervisor.play_bgm_wav(0, "th128_08");
        if (g_Supervisor.config.flags_2c & 0x10)
        {
            g_SoundManager.modify_bgm(4, 0, "dummy");
        }
        g_SoundManager.modify_bgm(2, 0, "dummy");
        g_Scorefile->bgm_unlocked[0] = 1;
    }
    menu->unk_1fc = 0;
    menu->saved_game_speed = g_game_speed;
    g_game_speed = 1.0f;
    menu->saved_global_4d9d90 = g_unk_4d9d90;
    g_unk_4d9d90 = 1;
    menu->flags_3ec &= ~4;
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

#include <stdio.h>

#include "HelpManual.h"
#include "Player.h"

// ReplayManager::set_end_stage where LTCG inlined it.
static __forceinline void replay_set_end_stage_inline(ReplayManager *replay, i32 extra_stage)
{
    _time64(&replay->info->timestamp);
    replay->info->stage = extra_stage != 0 ? extra_stage + 7 : g_Globals.stage_num;
}

i32 __stdcall input_pressed_or_repeating(u32 mask);

// TODO: ours keeps the Q key's shared tail in case 6 (the original's is in case 7), uses idiv for % 13, and keeps name's address in esi around the replay save.
// FUNCTION: TH16 0x43f980
void PauseMenu::tick_open()
{
    char path[0x40];

    switch (unk_1f4)
    {
    case 0:
        // The pause menu opens.
        if (time_in_current_menu.current < 10)
        {
            break;
        }
        set_unk_1f4(6);
        menu_34.num_choices = 5;
        if (g_GameThread->replay_mode != 0)
        {
            menu_34.disable(2);
            menu_34.disable(3);
        }
        if (g_Globals.continues_used > 0)
        {
            menu_34.disable(2);
            AnmManager::interrupt_tree(anm_id_1e4, (i16)(menu_34.next_selection + 7));
            AnmManager::interrupt_tree(anm_id_1e4.search_children(0x82, 0), 5);
            AnmManager::interrupt_tree(anm_id_1e4.search_children(0x8b, 0), 5);
            AnmManager::interrupt_tree(anm_id_1e4.search_children(0x97, 0), 5);
        }
        menu_34.wraps = 1;
        menu_34.set_cursor(0);
        AnmManager::interrupt_tree(anm_id_1e4, (i16)(menu_34.next_selection + 7));
        unk_204 = 0;
        return;
    case 1:
        // The menu shown when a replay ends opens.
        if (time_in_current_menu.current < 10)
        {
            break;
        }
        set_unk_1f4(6);
        menu_34.num_choices = 5;
        menu_34.disable(3);
        menu_34.disable(2);
        menu_34.disable(0);
        menu_34.wraps = 1;
        menu_34.set_cursor(1);
        AnmManager::interrupt_tree(anm_id_1e4, (i16)(menu_34.next_selection + 7));
        unk_204 = 1;
        return;
    case 2:
    case 3:
    case 4:
    case 5:
        // The game ended: the score goes into the table first.
        if (time_in_current_menu.current < 10)
        {
            break;
        }
        {
            u32 score = g_Globals.score;
            g_Gui->current_score = score;
            if ((u32)g_Globals.hiscore < score)
            {
                g_Globals.hiscore = score;
            }
        }
        begin_score_entry();
        if (unk_200 != 0)
        {
            // No name to enter.
            goto score_entered;
        }
        set_unk_1f4(15);
        anm_id_1e4.clear_flag_lo_2_tree();
        return;
    case 6:
        // The menu itself.
        menu_34.current_selection = menu_34.next_selection;
        if (input_pressed_or_repeating(INPUT_UP))
        {
            menu_34.move_cursor(-1);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            menu_34.move_cursor(1);
        }
        if (menu_34.current_selection != menu_34.next_selection)
        {
            AnmManager::interrupt_tree(anm_id_1e4, (i16)(menu_34.next_selection + 7));
            g_SoundManager.play_sound_centered(10, 0);
        }
        if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
        {
            g_SoundManager.play_sound_centered(7, 0);
            switch (menu_34.next_selection)
            {
            case 0:
                AnmManager::interrupt_tree(anm_id_1e8, 1);
                AnmManager::interrupt_tree(anm_id_1e4, 1);
                set_unk_1f4(16);
                break;
            case 1:
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x81, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x8a, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x92, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x94, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x96, 0), 6);
                if (g_GameThread->replay_mode == 0 && state == 1)
                {
                    set_unk_1f4(7);
                }
                else
                {
                    set_unk_1f4(16);
                }
                break;
            case 2:
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x82, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x8b, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x97, 0), 6);
                if (state == 1)
                {
                    set_unk_1f4(9);
                }
                else
                {
                    set_unk_1f4(10);
                }
                break;
            case 3:
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x83, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x8c, 0), 6);
                set_unk_1f4(14);
                break;
            case 4:
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x84, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x8d, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x93, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x95, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x98, 0), 6);
                if (g_GameThread->replay_mode == 0 && state == 1)
                {
                    set_unk_1f4(7);
                }
                else
                {
                    set_unk_1f4(16);
                }
                break;
            }
            time_in_current_menu.set_value(0);
        }
        if (unk_204 == 0)
        {
            if (g_hardware_input_pressed & INPUT_R)
            {
                g_SoundManager.play_sound_centered(7, 0);
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x84, 0), 6);
                AnmManager::interrupt_tree(anm_id_1e8, 1);
                menu_34.set_cursor(4);
                set_unk_1f4(16);
            }
            if (g_hardware_input_pressed & INPUT_MENU)
            {
                goto resume;
            }
        }
        if (g_hardware_input_pressed & INPUT_Q)
        {
            g_SoundManager.play_sound_centered(7, 0);
            AnmManager::interrupt_tree(anm_id_1e4.search_children(0x81, 0), 6);
            menu_34.set_cursor(1);
            set_unk_1f4(16);
        }
        break;
    case 7:
    case 9:
        // "Really?" for quitting (7) or retrying (9).
        if (time_in_current_menu.current < 20)
        {
            break;
        }
        if (time_in_current_menu.current == 20)
        {
            menu_34.push();
            menu_34.num_choices = 2;
            menu_34.wraps = 1;
            menu_34.set_cursor(1);
            AnmManager::interrupt_tree(anm_id_1e4, 14);
        }
        if (time_in_current_menu.current < 30)
        {
            break;
        }
        if (time_in_current_menu.current == 30)
        {
            AnmManager::interrupt_tree(anm_id_1e4, (i16)(menu_34.next_selection + 15));
        }
        menu_34.current_selection = menu_34.next_selection;
        if (input_pressed_or_repeating(INPUT_UP))
        {
            menu_34.move_cursor(-1);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            menu_34.move_cursor(1);
        }
        if (menu_34.current_selection != menu_34.next_selection)
        {
            AnmManager::interrupt_tree(anm_id_1e4, (i16)(menu_34.next_selection + 15));
            g_SoundManager.play_sound_centered(10, 0);
        }
        if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
        {
            switch (menu_34.next_selection)
            {
            case 0:
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x9a, 0), 6);
                if (unk_1f4 == 9)
                {
                    set_unk_1f4(10);
                }
                else
                {
                    set_unk_1f4(8);
                }
                g_SoundManager.play_sound_centered(7, 0);
                break;
            case 1:
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x9b, 0), 6);
                set_unk_1f4(8);
                g_SoundManager.play_sound_centered(9, 0);
                break;
            }
        }
        if (g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB))
        {
            g_SoundManager.play_sound_centered(9, 0);
            switch (menu_34.next_selection)
            {
            case 0:
                menu_34.set_cursor(1);
                AnmManager::interrupt_tree(anm_id_1e4, (i16)(menu_34.next_selection + 15));
                break;
            case 1:
                AnmManager::interrupt_tree(anm_id_1e4.search_children(0x9b, 0), 6);
                set_unk_1f4(8);
                break;
            }
        }
        if (g_hardware_input_pressed & INPUT_MENU)
        {
        resume:
            AnmManager::interrupt_tree(anm_id_1e8, 1);
            AnmManager::interrupt_tree(anm_id_1e4, 1);
            menu_34.set_cursor(0);
            set_unk_1f4(16);
            return;
        }
        break;
    case 8:
        // Leaving the "Really?" question.
        if (time_in_current_menu.current < 20)
        {
            break;
        }
        switch (menu_34.next_selection)
        {
        case 0:
            AnmManager::interrupt_tree(anm_id_1e4, 1);
            menu_34.pop();
            set_unk_1f4(16);
            return;
        case 1:
            menu_34.pop();
            if (g_Globals.continues_used > 0)
            {
                menu_34.disable(2);
            }
            AnmManager::interrupt_tree(anm_id_1e4, (i16)(menu_34.next_selection + 7));
            set_unk_1f4(6);
            return;
        }
        break;
    case 10:
        // Opening the replay slots.
        if (time_in_current_menu.current < 20)
        {
            break;
        }
        flags_3ec &= ~2;
        flags_3ec |= 1;
        set_unk_1f4(11);
        anm_id_1e4.clear_flag_lo_2_tree();
        menu_34.push();
        menu_34.num_choices = 25;
        menu_34.wraps = 1;
        menu_34.set_cursor(0);
        for (i32 i = 1; i <= 25; i++)
        {
            sprintf(path, "th16_%.2d.rpy", i);
            replays[i - 1] = ReplayManager::create_from_file(path);
        }
        return;
    case 12:
    case 15:
        // Entering the replay name (12) or the score name (15).
        if (time_in_current_menu.current < 10)
        {
            break;
        }
        menu.current_selection = menu.next_selection;
        if (input_pressed_or_repeating(INPUT_UP))
        {
            menu.move_cursor(-13);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            menu.move_cursor(13);
        }
        if (input_pressed_or_repeating(INPUT_LEFT))
        {
            if (menu.next_selection % 13 != 0)
            {
                menu.move_cursor(-1);
            }
            else
            {
                menu.move_cursor(12);
            }
        }
        if (input_pressed_or_repeating(INPUT_RIGHT))
        {
            if (menu.next_selection % 13 != 12)
            {
                menu.move_cursor(1);
            }
            else
            {
                menu.move_cursor(-12);
            }
        }
        if (menu.current_selection != menu.next_selection)
        {
            g_SoundManager.play_sound_centered(10, 0);
        }
        if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
        {
            i32 choice = menu.next_selection;
            if (choice < 0x58)
            {
                if (name_cursor < 8)
                {
                    name[name_cursor] = g_name_entry_chars[choice];
                    name_cursor++;
                    if (name_cursor >= 8)
                    {
                        menu.set_cursor(0x5a);
                    }
                }
                else
                {
                    name[name_cursor - 1] = g_name_entry_chars[choice];
                }
            }
            else if (choice == 0x58)
            {
                if (name_cursor < 8)
                {
                    name[name_cursor] = ' ';
                    name_cursor++;
                    if (name_cursor >= 8)
                    {
                        menu.set_cursor(0x5a);
                    }
                }
                else
                {
                    name[name_cursor - 1] = ' ';
                }
            }
            else if (choice == 0x59)
            {
                if (name_cursor == 0)
                {
                    break;
                }
                name_cursor--;
                name[name_cursor] = ' ';
                g_SoundManager.play_sound_centered(9, 0);
                return;
            }
            else if (choice == 0x5a)
            {
                if (unk_1f4 == 12)
                {
                    flags_3ec &= ~2;
                    flags_3ec |= 1;
                    g_SoundManager.play_sound_centered(0x11, 0);
                    sprintf(path, "th16_%.2d.rpy", menu_34.next_selection + 1);
                    ReplayManager::destroy(replays[menu_34.next_selection]);
                    g_ReplayManager->save(path, name, 0, 1);
                    replays[menu_34.next_selection] = ReplayManager::create_from_file(path);
                    set_unk_1f4(11);
                    strcpy(g_Scorefile->last_replay_name, name);
                    g_SoundManager.play_sound_centered(7, 0);
                    return;
                }
                g_SoundManager.play_sound_centered(7, 0);
                strcpy(((ScorefileData *)g_Scorefile)
                           ->charas[g_Globals.subshot + g_Globals.character]
                           .scores[g_Globals.difficulty][menu_34.next_selection]
                           .name,
                       name);
                strcpy(g_Scorefile->last_replay_name, name);
            score_entered:
                set_unk_1f4(6);
                menu_34.num_choices = 5;
                menu_34.wraps = 1;
                if (!(flags_3ec & 4) && g_Globals.game_mode == 0)
                {
                    anm_id_1e4 = front_anm->create_ui_vm_at_origin(0xa0, 0);
                    if (g_Globals.continues_used > 0)
                    {
                        menu_34.disable(2);
                    }
                    if (g_continues_remaining <= 0)
                    {
                        menu_34.disable(0);
                        menu_34.set_cursor(1);
                    }
                    else
                    {
                        menu_34.set_cursor(0);
                    }
                }
                else
                {
                    anm_id_1e4 = front_anm->create_ui_vm_at_origin(0xa1, 0);
                    menu_34.disable(0);
                    menu_34.disable(3);
                    menu_34.set_cursor(0);
                    if (!(flags_3ec & 4))
                    {
                        menu_34.set_cursor(4);
                    }
                }
                AnmManager::interrupt_tree_and_run(anm_id_1e4, 3);
                AnmManager::interrupt_tree(anm_id_1e4, (i16)(menu_34.next_selection + 7));
                return;
            }
            g_SoundManager.play_sound_centered(7, 0);
            return;
        }
        if (g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB))
        {
            g_SoundManager.play_sound_centered(9, 0);
            if (name_cursor == 0)
            {
                if (unk_1f4 == 12)
                {
                    flags_3ec &= ~2;
                    flags_3ec |= 1;
                    set_unk_1f4(11);
                    return;
                }
                break;
            }
            name_cursor--;
            name[name_cursor] = ' ';
            return;
        }
        break;
    case 11:
        // Choosing the replay slot.
        if (time_in_current_menu.current < 10)
        {
            break;
        }
        menu_34.current_selection = menu_34.next_selection;
        if (input_pressed_or_repeating(INPUT_UP))
        {
            menu_34.move_cursor(-1);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            menu_34.move_cursor(1);
        }
        if (menu_34.current_selection != menu_34.next_selection)
        {
            g_SoundManager.play_sound_centered(10, 0);
        }
        if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
        {
            flags_3ec &= ~1;
            flags_3ec |= 2;
            set_unk_1f4(12);
            menu.set_cursor(0);
            menu.num_choices = 0x5b;
            menu.wraps = 1;
            if (unk_1fc != 0 && g_Globals.game_mode == 0)
            {
                g_ReplayManager->set_end_stage(1);
            }
            else
            {
                replay_set_end_stage_inline(g_ReplayManager, 0);
            }
            strcpy(name, g_Scorefile->last_replay_name);
            name_cursor = 0;
            if (strcmp(name, "        ") != 0)
            {
                menu.move_cursor(-1);
            }
            i32 i;
            for (i = 8; i > 0; i--)
            {
                if (name[i - 1] != ' ')
                {
                    break;
                }
            }
            name_cursor = i;
            g_SoundManager.play_sound_centered(7, 0);
            return;
        }
        if (g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB))
        {
            flags_3ec &= ~3;
            menu_34.pop();
            menu_34.num_choices = 5;
            menu_34.wraps = 1;
            AnmManager::interrupt_tree(anm_id_1e4, (i16)(menu_34.next_selection + 7));
            for (i32 i = 0; i < 25; i++)
            {
                ReplayManager::destroy(replays[i]);
                replays[i] = NULL;
            }
            if (state == 1)
            {
                set_unk_1f4(16);
                menu_34.set_cursor(1);
                g_SoundManager.play_sound_centered(9, 0);
                return;
            }
            set_unk_1f4(6);
            anm_id_1e4.set_flag_lo_2_tree();
            if (g_Globals.game_mode != 0)
            {
                menu_34.disable(0);
                menu_34.disable(3);
            }
            g_SoundManager.play_sound_centered(9, 0);
            return;
        }
        break;
    case 14:
        // The manual.
        if (time_in_current_menu.current == 20)
        {
            anm_id_1e4.clear_flag_lo_2_tree();
            HelpManual::create();
            g_HelpManual->unk_128 = 32.0f;
        }
        if (g_HelpManual != NULL && g_HelpManual->unk_124 != 0)
        {
            HelpManual::destroy();
            set_unk_1f4(6);
            anm_id_1e4.set_flag_lo_2_tree();
            return;
        }
        break;
    case 16:
        // Leaving the menu: resume, retry, quit or continue.
        if (time_in_current_menu.current < 12)
        {
            break;
        }
        if (state == 1)
        {
            leave_state_1();
        }
        else if (state == 2)
        {
            leave_state_2();
        }
        else if (state == 3)
        {
            leave_state_3();
        }
        switch (menu_34.next_selection)
        {
        case 0:
            if (state == 1)
            {
                SoundManager::resume_sounds();
                g_SoundManager.modify_bgm(7, 0, "UnPause");
            }
            else if (state == 2)
            {
                if (unk_1fc != 0)
                {
                    g_Supervisor.gamemode_to_switch_to = 10;
                }
                else if (g_Globals.stage_num == 7)
                {
                    g_Supervisor.gamemode_to_switch_to = 14;
                }
                else
                {
                    // Continue.
                    g_Globals.lives = 2;
                    g_Globals.life_fragments = 0;
                    g_Globals.bombs = 2;
                    if (g_Gui != NULL)
                    {
                        g_Gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
                    }
                    g_Globals.bomb_fragments = 0;
                    g_Globals.power = 0;
                    if (g_Globals.power > g_Globals.max_power)
                    {
                        g_Globals.power = g_Globals.max_power;
                    }
                    else if (g_Globals.power < g_Globals.power_per_level)
                    {
                        g_Globals.power = g_Globals.power_per_level;
                    }
                    g_Globals.add_power(g_Globals.power_per_level * 4);
                    g_Player->inner.repopulate_options();
                    g_Gui->update_lives(g_Globals.lives, g_Globals.life_fragments);
                    g_Gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
                    g_Globals.score = 0;
                    g_Globals.continues_used++;
                    if (g_Globals.continues_used >= 10)
                    {
                        g_Globals.continues_used = 9;
                    }
                    g_continues_remaining--;
                    g_GameThread->flags.flag_4 = 0;
                    SoundManager::resume_sounds();
                    g_SoundManager.modify_bgm(2, -1, saved_bgm_name);
                    while (SoundManager::update_sound_thread() != 0)
                    {
                    }
                    g_SoundManager.seek_bgm(saved_bgm_time);
                    g_game_speed = saved_game_speed;
                    Gui *gui = g_Gui;
                    if (gui->msg != NULL)
                    {
                        gui->msg->show();
                    }
                    Gui::sub_42c5c0();
                    g_unk_4d9d90 = saved_global_4d9d90;
                }
            }
            break;
        case 1:
            AnmManager::interrupt_tree(anm_id_1e8, 1);
            AnmManager::interrupt_tree(anm_id_1e4, 1);
            g_Supervisor.gamemode_to_switch_to = (g_Supervisor.flags & 0x2000) ? 2 : 4;
            break;
        case 4:
            delete_vm_and_clear(anm_id_1e8);
            delete_vm_and_clear(anm_id_1e4);
            g_Supervisor.gamemode_to_switch_to = g_GameThread->replay_mode != 0 ? 11 : 10;
            break;
        }
        set_state(0);
        break;
    }
}
