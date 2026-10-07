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
static_assert(offsetof(PauseMenu, menu_flags) == 0x3ec, "PauseMenu::menu_flags");
static_assert(offsetof(Gui, front_anm) == 0x2d8, "Gui::front_anm");

// GLOBAL: TH16 0x4a6ef4
PauseMenu *g_PauseMenu;

// The characters of the name entry grid (MainMenuStates.cpp).
extern const char g_name_entry_chars[];

// Switches to another PauseState, back to its first substate.
// FUNCTION: TH16 0x43e150
HARNESS_CALLED void PauseMenu::set_state(i32 state)
{
    prev_state = this->state;
    this->state = state;
    substate = 0;
    time_in_current_menu.reset();
    time_since_pause_or_unpause.reset();
    item_menu.num_disabled = 0;
}

// Moves to another PauseSubstate, restarting its timer.
// FUNCTION: TH16 0x43e200
void PauseMenu::set_substate(i32 value)
{
    substate = value;
    time_in_current_menu.reset();
}

// Puts the game's score into this character's top ten for the difficulty
// played, with a blank name. Returns the rank, or -1 if it did not make it.
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

// Registers the tick (priority 10) and draw (0x4a) callbacks, inactive for
// now.
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
    for (i32 i = 0; i < REPLAY_SLOTS; i++)
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

// Opens the pause menu on Esc (or after a device reset) once the stage has
// run for 30 frames, unless a demo plays; runs the open menu.
// TODO: the inlined timer ticks use other xmm registers for 1.0, 1.01 and the speed.
// FUNCTION: TH16 0x43e5f0
int PauseMenu::on_tick()
{
    switch (state)
    {
    case PAUSE_CLOSED:
        if (!(g_Globals.flags_hi_45c & GLOBALS_HI_DEMO_PLAY) && !g_GameThread->flags.music_restart &&
            ((g_hardware_input_pressed & INPUT_MENU) || (g_Supervisor.flags & SUPERVISOR_DEVICE_WAS_RESET)) &&
            g_GameThread->on_tick != NULL &&
            (g_GameThread->on_tick->flags & UPDATE_FUNC_ACTIVE) && g_GameThread->time_in_stage.current >= 30)
        {
            open();
        }
        break;
    case PAUSE_PAUSED:
    case PAUSE_GAME_OVER:
    case PAUSE_STAGE_END:
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
            c = i == NAME_ENTRY_BACKSPACE ? 0x7f : 0x80;
        }
        g_AsciiManager->create_stringf(&key_pos, "%c", c);
        if (i % NAME_ENTRY_COLUMNS == NAME_ENTRY_COLUMNS - 1)
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
    for (i32 i = 0; i < REPLAY_SLOTS; i++)
    {
        g_AsciiManager->color.d3d = item_menu.next_selection == i ? 0xffffff00 : 0xff808080;
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
    i32 selection = item_menu.next_selection;
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
    i32 selection = item_menu.next_selection;
    pos.y = 64.0f;
    g_AsciiManager->create_stringf(&pos, "            Score Ranking!!");
    pos.x = 75.0f;
    pos.y = selection * 18.0f + 96.0f;
    if (score_not_ranked == 0)
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

// Draws the text parts over the menu: the replay slots, the name entry, the
// high scores and the remaining credits.
// FUNCTION: TH16 0x43edc0
int PauseMenu::on_draw()
{
    g_AsciiManager->draw_shadows = 1;
    if (get_vm_or_clear(snapshot_id) != NULL)
    {
        AnmVm *vm = find_child_of(snapshot_id, 0x39);
        if (vm != NULL)
        {
            g_Supervisor.arcade_blit_vm_2c->color_1.d3d = vm->color_1.d3d | 0xff000000;
        }
    }
    switch (state)
    {
    case PAUSE_PAUSED:
    case PAUSE_STAGE_END:
        if ((menu_flags & (PAUSE_SHOW_REPLAY_SLOTS | PAUSE_SHOW_REPLAY_NAME)) == PAUSE_SHOW_REPLAY_SLOTS)
        {
            draw_replay_list();
        }
        else if ((menu_flags & (PAUSE_SHOW_REPLAY_SLOTS | PAUSE_SHOW_REPLAY_NAME)) == PAUSE_SHOW_REPLAY_NAME)
        {
            draw_replay_name_entry();
        }
        break;
    case PAUSE_GAME_OVER:
        if (substate == PAUSE_SUB_SCORE_NAME_ENTRY)
        {
            draw_high_scores();
        }
        if ((menu_flags & (PAUSE_SHOW_REPLAY_SLOTS | PAUSE_SHOW_REPLAY_NAME)) == PAUSE_SHOW_REPLAY_SLOTS)
        {
            draw_replay_list();
        }
        else if ((menu_flags & (PAUSE_SHOW_REPLAY_SLOTS | PAUSE_SHOW_REPLAY_NAME)) == PAUSE_SHOW_REPLAY_NAME)
        {
            draw_replay_name_entry();
        }
        else if (substate != PAUSE_SUB_MANUAL)
        {
            g_AsciiManager->create_stringf(&Float3(184.0f, 448.0f, 0.0f), "Credit %d", g_continues_remaining);
        }
        break;
    }
    g_AsciiManager->draw_shadows = 0;
    return 1;
}

// TODO: the original realigns its frame to 8 bytes (whole-program, see docs/findings.md) and keeps vm in a stack slot, not ebx.
// FUNCTION: TH16 0x43ef20
void PauseMenu::take_snapshot()
{
    delete_vm_and_clear(snapshot_id);
    snapshot_id = g_Supervisor.text_anm->create_ui_vm_at_origin(TEXT_SCRIPT_PAUSE_SNAPSHOT, 0);
    // get_vm_or_clear with g_AnmManager read once (see on_draw).
    AnmManager *anm_manager = g_AnmManager;
    AnmVm *vm = anm_manager->get_vm_with_id(snapshot_id);
    if (vm == NULL)
    {
        snapshot_id.id = 0;
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
    set_state(PAUSE_PAUSED);
    GameThread *thread = g_GameThread;
    thread->flags.in_menu = 1;
    front_anm = g_Gui->front_anm;
    delete_vm_and_clear(menu_anm_id);
    if (thread->replay_mode != 0)
    {
        menu_anm_id = front_anm->create_ui_vm_at_origin(PAUSE_SCRIPT_REPLAY_PAUSE_MENU, 0);
    }
    else
    {
        menu_anm_id = front_anm->create_ui_vm_at_origin(PAUSE_SCRIPT_PAUSE_MENU, 0);
    }
    AnmManager::interrupt_tree(menu_anm_id, 3);
    SoundManager::pause_sounds();
    g_SoundManager.play_sound_centered(SE_PAUSE, 0);
    if (g_Globals.game_mode != GAME_MODE_SPELL_PRACTICE)
    {
        g_SoundManager.modify_bgm(BGM_PAUSE, 0, "Pause");
    }
    while (SoundManager::update_sound_thread() != 0)
    {
    }
    take_snapshot();
    saved_game_speed = g_game_speed;
    g_game_speed = 1.0f;
    saved_pacing_mode = g_frame_pacing.mode;
    g_frame_pacing.mode = FRAME_PACING_IDLE;
    Gui *gui = g_Gui;
    if (gui->msg != NULL)
    {
        gui->msg->hide();
    }
    AnmVm *vm = g_AnmManager->get_vm_with_id(gui->overlay_ids[4]);
    if (vm != NULL)
    {
        vm->hide_tree_inline();
    }
    menu_flags &= ~PAUSE_FROM_STAGE_END;
}

// FUNCTION: TH16 0x43f500
void open_stage_end_menu()
{
    PauseMenu *menu = g_PauseMenu;
    GameThread::update_play_time();
    if (g_GameThread->replay_mode == 1)
    {
        g_Supervisor.gamemode_to_switch_to =
            (g_Supervisor.flags & SUPERVISOR_IDLE_ON_EXIT) ? GAMEMODE_IDLE : GAMEMODE_TITLE;
        return;
    }
    g_GameThread->flags.in_menu = 1;
    menu->set_state(PAUSE_STAGE_END);
    menu->set_substate_inline(PAUSE_SUB_OPEN_STAGE_END);
    if (g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE)
    {
        menu->set_substate(PAUSE_SUB_OPEN_SPELL_PRACTICE_END);
    }
    if (g_Globals.game_mode == GAME_MODE_NORMAL)
    {
        menu->snapshot_id = g_Supervisor.text_anm->create_ui_vm_at_origin(TEXT_SCRIPT_PAUSE_SNAPSHOT, 0);
        g_AnmManager->copy_screen_to_sprite(menu->snapshot_id, (i32)(g_screen_coord_scale * 32.0f),
                                            (i32)(g_screen_coord_scale * 16.0f), (i32)(g_screen_coord_scale * 384.0f),
                                            (i32)(g_screen_coord_scale * 448.0f));
    }
    Gui *gui = g_Gui;
    menu->front_anm = gui->front_anm;
    menu->stage_finished = 1;
    menu->saved_game_speed = g_game_speed;
    g_game_speed = 1.0f;
    menu->saved_pacing_mode = g_frame_pacing.mode;
    g_frame_pacing.mode = FRAME_PACING_MENU;
    if (gui->msg != NULL)
    {
        gui->msg->hide();
    }
    Gui::hide_chapter_result_vm();
    menu->menu_flags |= PAUSE_FROM_STAGE_END;
}

// TODO: ours folds the character offset into the practice index (one imul by 0xa63, scaled by 8); the original adds it to the pointer.
// FUNCTION: TH16 0x43f7e0
void PauseMenu::begin_score_entry()
{
    if (g_Globals.game_mode != GAME_MODE_SPELL_PRACTICE)
    {
        if (g_Globals.game_mode != GAME_MODE_NORMAL)
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
            if (g_Globals.stage_num == 7 && stage_finished != 0)
            {
                g_Globals.stage_num = 9;
            }
            i32 rank = ((ScorefileData *)g_Scorefile)->charas[g_Globals.subshot + g_Globals.character].insert_score();
            if (g_Globals.stage_num == 9 && stage_finished != 0)
            {
                g_Globals.stage_num = 7;
            }
            if (rank >= 0)
            {
                item_menu.num_choices = 25;
                item_menu.wraps = 1;
                item_menu.set_cursor(rank);
                name_entry_menu.set_cursor(0);
                name_entry_menu.num_choices = NAME_ENTRY_CHOICES;
                name_entry_menu.wraps = 1;
                strcpy(name, ((ScorefileData *)g_Scorefile)->status.name);
                name_cursor = 0;
                if (strcmp(name, "        ") != 0)
                {
                    name_entry_menu.move_cursor(-1);
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
                score_not_ranked = 0;
                return;
            }
        }
    }
    score_not_ranked = 1;
}

// FUNCTION: TH16 0x43f240
void open_replay_end_menu()
{
    PauseMenu *menu = g_PauseMenu;
    menu->set_state(PAUSE_PAUSED);
    menu->set_substate_inline(PAUSE_SUB_OPEN_REPLAY_END);
    g_GameThread->flags.in_menu = 1;
    menu->take_snapshot();
    menu->front_anm = g_Gui->front_anm;
    delete_vm_and_clear(menu->menu_anm_id);
    menu->menu_anm_id = menu->front_anm->create_ui_vm_at_origin(PAUSE_SCRIPT_REPLAY_END_MENU, 0);
    AnmManager::interrupt_tree(menu->menu_anm_id, 3);
    SoundManager::pause_sounds();
    g_SoundManager.modify_bgm(BGM_PAUSE, 0, "Pause");
    menu->saved_game_speed = g_game_speed;
    g_game_speed = 1.0f;
    menu->saved_pacing_mode = g_frame_pacing.mode;
    g_frame_pacing.mode = FRAME_PACING_IDLE;
    menu->menu_flags &= ~PAUSE_FROM_STAGE_END;
}

// TODO: the original keeps an ebp frame with a 4-byte pad (push ebp; push ecx), most likely known entry alignment through its callers (Player::on_tick_body, Gui::start_dialogue); ours has no frame (HARNESS_CALLED does not change it).
// FUNCTION: TH16 0x43f350
void open_game_over_menu()
{
    PauseMenu *menu = g_PauseMenu;
    GameThread::update_play_time();
    if (g_GameThread->replay_mode == 1)
    {
        g_Supervisor.gamemode_to_switch_to =
            (g_Supervisor.flags & SUPERVISOR_IDLE_ON_EXIT) ? GAMEMODE_IDLE : GAMEMODE_TITLE;
        return;
    }
    menu->set_state(PAUSE_GAME_OVER);
    menu->set_substate_inline(PAUSE_SUB_OPEN_GAME_OVER);
    g_GameThread->flags.in_menu = 1;
    SoundManager::pause_sounds();
    g_SoundManager.play_sound_centered(SE_PAUSE, 0);
    if (g_Globals.game_mode != GAME_MODE_SPELL_PRACTICE)
    {
        g_SoundManager.modify_bgm(BGM_PAUSE, 0, "Pause");
    }
    while (SoundManager::update_sound_thread() != 0)
    {
    }
    menu->take_snapshot();
    menu->front_anm = g_Gui->front_anm;
    if (g_Globals.game_mode != GAME_MODE_SPELL_PRACTICE)
    {
        strcpy(menu->saved_bgm_name, g_SoundManager.get_bgm_name());
        menu->saved_bgm_time = g_SoundManager.bgm_play_time();
        g_Supervisor.play_bgm_wav(0, "th128_08");
        if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
        {
            g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
        }
        g_SoundManager.modify_bgm(BGM_PLAY, 0, "dummy");
        g_Scorefile->bgm_unlocked[0] = 1;
    }
    menu->stage_finished = 0;
    menu->saved_game_speed = g_game_speed;
    g_game_speed = 1.0f;
    menu->saved_pacing_mode = g_frame_pacing.mode;
    g_frame_pacing.mode = FRAME_PACING_MENU;
    menu->menu_flags &= ~PAUSE_FROM_STAGE_END;
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

// TODO: the original pads its frame (push ecx) for the alignment its only caller, tick_open, provides; ours does not, even HARNESS_CALLED (also with GuiMsgVm::show or get_runtime HARNESS_CALLED).
// FUNCTION: TH16 0x43f6a0
void PauseMenu::leave_paused()
{
    if (g_GameThread->replay_mode == 0)
    {
        g_play_time_runtime = get_runtime();
    }
    g_GameThread->flags.in_menu = 0;
    g_game_speed = saved_game_speed;
    Gui *gui = g_Gui;
    if (gui->msg != NULL)
    {
        gui->msg->show();
    }
    AnmVm *vm = g_AnmManager->get_vm_with_id(gui->overlay_ids[4]);
    if (vm != NULL)
    {
        vm->show_tree_inline();
    }
    g_frame_pacing.mode = saved_pacing_mode;
}

// FUNCTION: TH16 0x43f740
void PauseMenu::leave_game_over()
{
    if (g_GameThread->replay_mode == 0)
    {
        g_play_time_runtime = get_runtime();
    }
    AnmManager::interrupt_tree(snapshot_id, 1);
    AnmManager::interrupt_tree(menu_anm_id, 1);
    g_game_speed = saved_game_speed;
}

// FUNCTION: TH16 0x43f790
void PauseMenu::leave_stage_end()
{
    if (g_GameThread->replay_mode == 0)
    {
        g_play_time_runtime = get_runtime();
    }
    AnmManager::interrupt_tree(snapshot_id, 1);
    AnmManager::interrupt_tree(menu_anm_id, 1);
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

// Small MenuHelper steps the menu code inlines (as in MainMenuStates.cpp);
// they address the fields through the menu pointer.
static __forceinline void menu_save_selection(MenuHelper *m)
{
    m->current_selection = m->next_selection;
}

static __forceinline i32 menu_selection_moved(MenuHelper *m)
{
    return m->current_selection != m->next_selection;
}

// TODO: ours keeps the Q key's shared tail in case 6 (the original's is in case 7), and around the replay save loads the slot index after create_from_file (the original before, keeping it in esi where ours keeps name's address).
// FUNCTION: TH16 0x43f980
void PauseMenu::tick_open()
{
    char path[0x40];

    switch (substate)
    {
    case PAUSE_SUB_OPEN_PAUSE:
        // The pause menu opens.
        if (time_in_current_menu.current < 10)
        {
            break;
        }
        set_substate(PAUSE_SUB_CHOOSE);
        item_menu.num_choices = PAUSE_ITEM_COUNT;
        if (g_GameThread->replay_mode != 0)
        {
            item_menu.disable(PAUSE_ITEM_SAVE_REPLAY);
            item_menu.disable(PAUSE_ITEM_MANUAL);
        }
        if (g_Globals.continues_used > 0)
        {
            item_menu.disable(PAUSE_ITEM_SAVE_REPLAY);
            AnmManager::interrupt_tree(menu_anm_id, (i16)(item_menu.next_selection + 7));
            AnmManager::interrupt_tree(menu_anm_id.search_children(0x82, 0), 5);
            AnmManager::interrupt_tree(menu_anm_id.search_children(0x8b, 0), 5);
            AnmManager::interrupt_tree(menu_anm_id.search_children(0x97, 0), 5);
        }
        item_menu.wraps = 1;
        item_menu.set_cursor(PAUSE_ITEM_RESUME);
        AnmManager::interrupt_tree(menu_anm_id, (i16)(item_menu.next_selection + 7));
        replay_ended = 0;
        return;
    case PAUSE_SUB_OPEN_REPLAY_END:
        // The menu shown when a replay ends opens.
        if (time_in_current_menu.current < 10)
        {
            break;
        }
        set_substate(PAUSE_SUB_CHOOSE);
        item_menu.num_choices = PAUSE_ITEM_COUNT;
        item_menu.disable(PAUSE_ITEM_MANUAL);
        item_menu.disable(PAUSE_ITEM_SAVE_REPLAY);
        item_menu.disable(PAUSE_ITEM_RESUME);
        item_menu.wraps = 1;
        item_menu.set_cursor(PAUSE_ITEM_QUIT);
        AnmManager::interrupt_tree(menu_anm_id, (i16)(item_menu.next_selection + 7));
        replay_ended = 1;
        return;
    case PAUSE_SUB_OPEN_GAME_OVER:
    case PAUSE_SUB_OPEN_STAGE_END:
    case PAUSE_SUB_OPEN_4:
    case PAUSE_SUB_OPEN_SPELL_PRACTICE_END:
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
        if (score_not_ranked != 0)
        {
            // No name to enter.
            goto score_entered;
        }
        set_substate(PAUSE_SUB_SCORE_NAME_ENTRY);
        menu_anm_id.hide_tree();
        return;
    case PAUSE_SUB_CHOOSE:
        // The menu itself.
        menu_save_selection(&item_menu);
        if (input_pressed_or_repeating(INPUT_UP))
        {
            item_menu.move_cursor(-1);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            item_menu.move_cursor(1);
        }
        if (menu_selection_moved(&item_menu))
        {
            AnmManager::interrupt_tree(menu_anm_id, (i16)(item_menu.next_selection + 7));
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
        }
        if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
        {
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            switch (item_menu.next_selection)
            {
            case PAUSE_ITEM_RESUME:
                AnmManager::interrupt_tree(snapshot_id, 1);
                AnmManager::interrupt_tree(menu_anm_id, 1);
                set_substate(PAUSE_SUB_CLOSE);
                break;
            case PAUSE_ITEM_QUIT:
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x81, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x8a, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x92, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x94, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x96, 0), 6);
                if (g_GameThread->replay_mode == 0 && state == PAUSE_PAUSED)
                {
                    set_substate(PAUSE_SUB_CONFIRM);
                }
                else
                {
                    set_substate(PAUSE_SUB_CLOSE);
                }
                break;
            case PAUSE_ITEM_SAVE_REPLAY:
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x82, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x8b, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x97, 0), 6);
                if (state == PAUSE_PAUSED)
                {
                    set_substate(PAUSE_SUB_CONFIRM_SAVE);
                }
                else
                {
                    set_substate(PAUSE_SUB_OPEN_REPLAY_SLOTS);
                }
                break;
            case PAUSE_ITEM_MANUAL:
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x83, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x8c, 0), 6);
                set_substate(PAUSE_SUB_MANUAL);
                break;
            case PAUSE_ITEM_RESTART:
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x84, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x8d, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x93, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x95, 0), 6);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x98, 0), 6);
                if (g_GameThread->replay_mode == 0 && state == PAUSE_PAUSED)
                {
                    set_substate(PAUSE_SUB_CONFIRM);
                }
                else
                {
                    set_substate(PAUSE_SUB_CLOSE);
                }
                break;
            }
            time_in_current_menu.set_value(0);
        }
        if (replay_ended == 0)
        {
            if (g_hardware_input_pressed & INPUT_R)
            {
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x84, 0), 6);
                AnmManager::interrupt_tree(snapshot_id, 1);
                item_menu.set_cursor(PAUSE_ITEM_RESTART);
                set_substate(PAUSE_SUB_CLOSE);
            }
            if (g_hardware_input_pressed & INPUT_MENU)
            {
                goto resume;
            }
        }
        if (g_hardware_input_pressed & INPUT_Q)
        {
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            AnmManager::interrupt_tree(menu_anm_id.search_children(0x81, 0), 6);
            item_menu.set_cursor(PAUSE_ITEM_QUIT);
            set_substate(PAUSE_SUB_CLOSE);
        }
        break;
    case PAUSE_SUB_CONFIRM:
    case PAUSE_SUB_CONFIRM_SAVE:
        // "Really?" before quitting or restarting, or before saving the
        // replay (which ends the game). The cursor starts on "no" (1).
        if (time_in_current_menu.current < 20)
        {
            break;
        }
        if (time_in_current_menu.current == 20)
        {
            item_menu.push();
            item_menu.num_choices = 2;
            item_menu.wraps = 1;
            item_menu.set_cursor(1);
            AnmManager::interrupt_tree(menu_anm_id, 14);
        }
        if (time_in_current_menu.current < 30)
        {
            break;
        }
        if (time_in_current_menu.current == 30)
        {
            AnmManager::interrupt_tree(menu_anm_id, (i16)(item_menu.next_selection + 15));
        }
        menu_save_selection(&item_menu);
        if (input_pressed_or_repeating(INPUT_UP))
        {
            item_menu.move_cursor(-1);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            item_menu.move_cursor(1);
        }
        if (menu_selection_moved(&item_menu))
        {
            AnmManager::interrupt_tree(menu_anm_id, (i16)(item_menu.next_selection + 15));
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
        }
        if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
        {
            switch (item_menu.next_selection)
            {
            case 0:
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x9a, 0), 6);
                if (substate == PAUSE_SUB_CONFIRM_SAVE)
                {
                    set_substate(PAUSE_SUB_OPEN_REPLAY_SLOTS);
                }
                else
                {
                    set_substate(PAUSE_SUB_CONFIRM_CLOSE);
                }
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                break;
            case 1:
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x9b, 0), 6);
                set_substate(PAUSE_SUB_CONFIRM_CLOSE);
                g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                break;
            }
        }
        if (g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB))
        {
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            switch (item_menu.next_selection)
            {
            case 0:
                item_menu.set_cursor(1);
                AnmManager::interrupt_tree(menu_anm_id, (i16)(item_menu.next_selection + 15));
                break;
            case 1:
                AnmManager::interrupt_tree(menu_anm_id.search_children(0x9b, 0), 6);
                set_substate(PAUSE_SUB_CONFIRM_CLOSE);
                break;
            }
        }
        if (g_hardware_input_pressed & INPUT_MENU)
        {
        resume:
            AnmManager::interrupt_tree(snapshot_id, 1);
            AnmManager::interrupt_tree(menu_anm_id, 1);
            item_menu.set_cursor(PAUSE_ITEM_RESUME);
            set_substate(PAUSE_SUB_CLOSE);
            return;
        }
        break;
    case PAUSE_SUB_CONFIRM_CLOSE:
        // Leaving the "Really?" question.
        if (time_in_current_menu.current < 20)
        {
            break;
        }
        switch (item_menu.next_selection)
        {
        case 0:
            AnmManager::interrupt_tree(menu_anm_id, 1);
            item_menu.pop();
            set_substate(PAUSE_SUB_CLOSE);
            return;
        case 1:
            item_menu.pop();
            if (g_Globals.continues_used > 0)
            {
                item_menu.disable(PAUSE_ITEM_SAVE_REPLAY);
            }
            AnmManager::interrupt_tree(menu_anm_id, (i16)(item_menu.next_selection + 7));
            set_substate(PAUSE_SUB_CHOOSE);
            return;
        }
        break;
    case PAUSE_SUB_OPEN_REPLAY_SLOTS:
        // Opening the replay slots.
        if (time_in_current_menu.current < 20)
        {
            break;
        }
        menu_flags &= ~PAUSE_SHOW_REPLAY_NAME;
        menu_flags |= PAUSE_SHOW_REPLAY_SLOTS;
        set_substate(PAUSE_SUB_REPLAY_SLOT_SELECT);
        menu_anm_id.hide_tree();
        item_menu.push();
        item_menu.num_choices = REPLAY_SLOTS;
        item_menu.wraps = 1;
        item_menu.set_cursor(0);
        for (i32 i = 1; i <= REPLAY_SLOTS; i++)
        {
            sprintf(path, "th16_%.2d.rpy", i);
            replays[i - 1] = ReplayManager::create_from_file(path);
        }
        return;
    case PAUSE_SUB_REPLAY_NAME_ENTRY:
    case PAUSE_SUB_SCORE_NAME_ENTRY:
        // Entering the replay name or the score name.
        if (time_in_current_menu.current < 10)
        {
            break;
        }
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
        if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
        {
            i32 choice = name_entry_menu.next_selection;
            if (choice < NAME_ENTRY_CHAR_COUNT)
            {
                if (name_cursor < 8)
                {
                    name[name_cursor] = g_name_entry_chars[choice];
                    name_cursor++;
                    if (name_cursor >= 8)
                    {
                        name_entry_menu.set_cursor(NAME_ENTRY_END);
                    }
                }
                else
                {
                    name[name_cursor - 1] = g_name_entry_chars[choice];
                }
            }
            else if (choice == NAME_ENTRY_SPACE)
            {
                if (name_cursor < 8)
                {
                    name[name_cursor] = ' ';
                    name_cursor++;
                    if (name_cursor >= 8)
                    {
                        name_entry_menu.set_cursor(NAME_ENTRY_END);
                    }
                }
                else
                {
                    name[name_cursor - 1] = ' ';
                }
            }
            else if (choice == NAME_ENTRY_BACKSPACE)
            {
                if (name_cursor == 0)
                {
                    break;
                }
                name_cursor--;
                name[name_cursor] = ' ';
                g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                return;
            }
            else if (choice == NAME_ENTRY_END)
            {
                if (substate == PAUSE_SUB_REPLAY_NAME_ENTRY)
                {
                    menu_flags &= ~PAUSE_SHOW_REPLAY_NAME;
                    menu_flags |= PAUSE_SHOW_REPLAY_SLOTS;
                    g_SoundManager.play_sound_centered(SE_EXTEND, 0);
                    sprintf(path, "th16_%.2d.rpy", item_menu.next_selection + 1);
                    ReplayManager::destroy(replays[item_menu.next_selection]);
                    g_ReplayManager->save(path, name, 0, 1);
                    replays[item_menu.next_selection] = ReplayManager::create_from_file(path);
                    set_substate(PAUSE_SUB_REPLAY_SLOT_SELECT);
                    strcpy(g_Scorefile->last_replay_name, name);
                    g_SoundManager.play_sound_centered(SE_OK00, 0);
                    return;
                }
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                strcpy(((ScorefileData *)g_Scorefile)
                           ->charas[g_Globals.subshot + g_Globals.character]
                           .scores[g_Globals.difficulty][item_menu.next_selection]
                           .name,
                       name);
                strcpy(g_Scorefile->last_replay_name, name);
            score_entered:
                set_substate(PAUSE_SUB_CHOOSE);
                item_menu.num_choices = PAUSE_ITEM_COUNT;
                item_menu.wraps = 1;
                if (!(menu_flags & PAUSE_FROM_STAGE_END) && g_Globals.game_mode == GAME_MODE_NORMAL)
                {
                    menu_anm_id = front_anm->create_ui_vm_at_origin(PAUSE_SCRIPT_GAME_OVER_MENU, 0);
                    if (g_Globals.continues_used > 0)
                    {
                        item_menu.disable(PAUSE_ITEM_SAVE_REPLAY);
                    }
                    if (g_continues_remaining <= 0)
                    {
                        item_menu.disable(PAUSE_ITEM_RESUME);
                        item_menu.set_cursor(PAUSE_ITEM_QUIT);
                    }
                    else
                    {
                        item_menu.set_cursor(PAUSE_ITEM_RESUME);
                    }
                }
                else
                {
                    menu_anm_id = front_anm->create_ui_vm_at_origin(PAUSE_SCRIPT_PRACTICE_END_MENU, 0);
                    item_menu.disable(PAUSE_ITEM_RESUME);
                    item_menu.disable(PAUSE_ITEM_MANUAL);
                    item_menu.set_cursor(PAUSE_ITEM_RESUME);
                    if (!(menu_flags & PAUSE_FROM_STAGE_END))
                    {
                        item_menu.set_cursor(PAUSE_ITEM_RESTART);
                    }
                }
                AnmManager::interrupt_tree_and_run(menu_anm_id, 3);
                AnmManager::interrupt_tree(menu_anm_id, (i16)(item_menu.next_selection + 7));
                return;
            }
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            return;
        }
        if (g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB))
        {
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            if (name_cursor == 0)
            {
                if (substate == PAUSE_SUB_REPLAY_NAME_ENTRY)
                {
                    menu_flags &= ~PAUSE_SHOW_REPLAY_NAME;
                    menu_flags |= PAUSE_SHOW_REPLAY_SLOTS;
                    set_substate(PAUSE_SUB_REPLAY_SLOT_SELECT);
                    return;
                }
                break;
            }
            name_cursor--;
            name[name_cursor] = ' ';
            return;
        }
        break;
    case PAUSE_SUB_REPLAY_SLOT_SELECT:
        // Choosing the replay slot.
        if (time_in_current_menu.current < 10)
        {
            break;
        }
        menu_save_selection(&item_menu);
        if (input_pressed_or_repeating(INPUT_UP))
        {
            item_menu.move_cursor(-1);
        }
        if (input_pressed_or_repeating(INPUT_DOWN))
        {
            item_menu.move_cursor(1);
        }
        if (menu_selection_moved(&item_menu))
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
        }
        if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
        {
            menu_flags &= ~PAUSE_SHOW_REPLAY_SLOTS;
            menu_flags |= PAUSE_SHOW_REPLAY_NAME;
            set_substate(PAUSE_SUB_REPLAY_NAME_ENTRY);
            name_entry_menu.set_cursor(0);
            name_entry_menu.num_choices = NAME_ENTRY_CHOICES;
            name_entry_menu.wraps = 1;
            if (stage_finished != 0 && g_Globals.game_mode == GAME_MODE_NORMAL)
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
                name_entry_menu.move_cursor(-1);
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
            g_SoundManager.play_sound_centered(SE_OK00, 0);
            return;
        }
        if (g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB))
        {
            menu_flags &= ~(PAUSE_SHOW_REPLAY_SLOTS | PAUSE_SHOW_REPLAY_NAME);
            item_menu.pop();
            item_menu.num_choices = PAUSE_ITEM_COUNT;
            item_menu.wraps = 1;
            AnmManager::interrupt_tree(menu_anm_id, (i16)(item_menu.next_selection + 7));
            for (i32 i = 0; i < REPLAY_SLOTS; i++)
            {
                ReplayManager::destroy(replays[i]);
                replays[i] = NULL;
            }
            if (state == PAUSE_PAUSED)
            {
                set_substate(PAUSE_SUB_CLOSE);
                item_menu.set_cursor(PAUSE_ITEM_QUIT);
                g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                return;
            }
            set_substate(PAUSE_SUB_CHOOSE);
            menu_anm_id.show_tree();
            if (g_Globals.game_mode != GAME_MODE_NORMAL)
            {
                item_menu.disable(PAUSE_ITEM_RESUME);
                item_menu.disable(PAUSE_ITEM_MANUAL);
            }
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            return;
        }
        break;
    case PAUSE_SUB_MANUAL:
        // The manual.
        if (time_in_current_menu.current == 20)
        {
            menu_anm_id.hide_tree();
            HelpManual::create();
            g_HelpManual->x_offset = 32.0f;
        }
        if (g_HelpManual != NULL && g_HelpManual->closed != 0)
        {
            HelpManual::destroy();
            set_substate(PAUSE_SUB_CHOOSE);
            menu_anm_id.show_tree();
            return;
        }
        break;
    case PAUSE_SUB_CLOSE:
        // Leaving the menu: resume, retry, quit or continue.
        if (time_in_current_menu.current < 12)
        {
            break;
        }
        if (state == PAUSE_PAUSED)
        {
            leave_paused();
        }
        else if (state == PAUSE_GAME_OVER)
        {
            leave_game_over();
        }
        else if (state == PAUSE_STAGE_END)
        {
            leave_stage_end();
        }
        switch (item_menu.next_selection)
        {
        case PAUSE_ITEM_RESUME:
            if (state == PAUSE_PAUSED)
            {
                SoundManager::resume_sounds();
                g_SoundManager.modify_bgm(BGM_UNPAUSE, 0, "UnPause");
            }
            else if (state == PAUSE_GAME_OVER)
            {
                if (stage_finished != 0)
                {
                    g_Supervisor.gamemode_to_switch_to = GAMEMODE_RESTART;
                }
                else if (g_Globals.stage_num == 7)
                {
                    g_Supervisor.gamemode_to_switch_to = GAMEMODE_RETRY_STAGE;
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
                    g_GameThread->flags.in_menu = 0;
                    SoundManager::resume_sounds();
                    g_SoundManager.modify_bgm(BGM_PLAY, -1, saved_bgm_name);
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
                    Gui::show_chapter_result_vm();
                    g_frame_pacing.mode = saved_pacing_mode;
                }
            }
            break;
        case PAUSE_ITEM_QUIT:
            AnmManager::interrupt_tree(snapshot_id, 1);
            AnmManager::interrupt_tree(menu_anm_id, 1);
            g_Supervisor.gamemode_to_switch_to =
                (g_Supervisor.flags & SUPERVISOR_IDLE_ON_EXIT) ? GAMEMODE_IDLE : GAMEMODE_TITLE;
            break;
        case PAUSE_ITEM_RESTART:
            delete_vm_and_clear(snapshot_id);
            delete_vm_and_clear(menu_anm_id);
            g_Supervisor.gamemode_to_switch_to =
                g_GameThread->replay_mode != 0 ? GAMEMODE_RESTART_REPLAY : GAMEMODE_RESTART;
            break;
        }
        set_state(PAUSE_CLOSED);
        break;
    }
}
