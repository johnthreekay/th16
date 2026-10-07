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

// TODO: ours realigns its frame to 8 bytes (caused by the call to update_play_time; the original does not).
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

// TODO: the original realigns its frame to 8 bytes (ebx-based form); the body matches.
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

// Matches but for its frame (the original does not realign it to 8 bytes),
// and it stops LTCG from folding play_sound_centered's this everywhere:
// any caller of CStreamingSound::get_play_time that also calls
// play_sound_centered does that in our build while get_play_time does not
// realign its own frame. Kept out until that is solved; the stub in
// src/stub/unit34b.cpp stands in (0x43f350).
#if 0
HARNESS_CALLED void pause_menu_43f350()
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
        strcpy(menu->saved_bgm_name, g_SoundManager.bgm_name);
        menu->saved_bgm_time = ((CStreamingSound *)g_SoundManager.bgm_stream)->get_play_time();
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
#endif

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
