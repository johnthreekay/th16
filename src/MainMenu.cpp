#include <process.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MainMenu.h"

#include "GameErrorContext.h"
#include "Input.h"
#include "Globals.h"
#include "LoadingThread.h"
#include "ScreenEffect.h"
#include "StageData.h"
#include "ReplayManager.h"
#include "Scorefile.h"
#include "SoundManager.h"
#include "Supervisor.h"

static_assert(offsetof(TitleInf, state) == 0x18, "TitleInf::state");
static_assert(offsetof(TitleInf, menu) == 0x24, "TitleInf::menu");
static_assert(offsetof(TitleInf, time_in_state) == 0x2ac, "TitleInf::time_in_state");
static_assert(offsetof(TitleInf, anm_ids) == 0x2c0, "TitleInf::anm_ids");
static_assert(offsetof(TitleInf, submenu_ascii_id) == 0x73c, "TitleInf::submenu_ascii_id");
static_assert(offsetof(TitleInf, music_track_count) == 0x7f4, "TitleInf::music_track_count");
static_assert(offsetof(TitleInf, music_filenames) == 0x804, "TitleInf::music_filenames");
static_assert(offsetof(TitleInf, music_titles) == 0x1004, "TitleInf::music_titles");
static_assert(offsetof(TitleInf, music_comments) == 0x1844, "TitleInf::music_comments");
static_assert(offsetof(TitleInf, music_scroll) == 0x5a44, "TitleInf::music_scroll");
static_assert(offsetof(TitleInf, replay_name) == 0x5a48, "TitleInf::replay_name");
static_assert(offsetof(TitleInf, name_entry_menu) == 0x5a5c, "TitleInf::name_entry_menu");
static_assert(offsetof(TitleInf, key_config) == 0x5b34, "TitleInf::key_config");
static_assert(offsetof(TitleInf, unk_5b44) == 0x5b44, "TitleInf::unk_5b44");
static_assert(offsetof(TitleInf, replay_slot) == 0x5b48, "TitleInf::replay_slot");
static_assert(offsetof(TitleInf, replay_stage) == 0x5b4c, "TitleInf::replay_stage");
static_assert(offsetof(TitleInf, replays) == 0x5b50, "TitleInf::replays");
static_assert(offsetof(TitleInf, thread) == 0x5de4, "TitleInf::thread");
static_assert(sizeof(TitleInf) == 0x5e00, "TitleInf");

extern i32 g_cancel_screen_effects;

// GLOBAL: TH16 0x4a6f20
TitleInf *g_MainMenu;

// FUNCTION: TH16 0x44a550
u32 TitleInf::get_size()
{
    return sizeof(TitleInf);
}

// Switches to another TitleState, back to its first substate.
// FUNCTION: TH16 0x44a560
void TitleInf::set_state(i32 state)
{
    prev_state = this->state;
    this->state = state;
    substate = 0;
    time_in_state.reset();
}

// Moves to another step of the current screen.
// The dead double is not ZUN's code but stands in for whatever double math
// the optimizer removed from his body: LTCG's double stack alignment pass
// sees it at the IL level, so every caller (exactly the menu states that
// call set_substate) realigns its frame (and esp, -8) as in the original.
// FUNCTION: TH16 0x44a5e0
HARNESS_CALLED void TitleInf::set_substate(i32 substate)
{
    double unused = substate;
    (void)unused;
    this->substate = substate;
    time_in_state.reset();
}

// FUNCTION: TH16 0x44a650
void TitleInf::interrupt_and_clear(i32 index)
{
    AnmManager::interrupt_tree(anm_ids[index], 1);
    anm_ids[index].id = 0;
}

// FUNCTION: TH16 0x44a680
HARNESS_CALLED void TitleInf::interrupt_child(i32 index, i32 script, i32 interrupt)
{
    AnmVm *vm = find_child_of(anm_ids[index], script);
    vm->interrupt(interrupt);
}

// TODO: the original pads its frame (push ecx): AnmVm::run needs 8-byte alignment there at IL level and do_title_screen provides it (see docs/findings.md, AnmVm::run's alignment); not when HARNESS_CALLED alone.
// FUNCTION: TH16 0x44a700
void TitleInf::interrupt_child_and_run(i32 index, i32 script, i32 interrupt)
{
    AnmVm *vm = find_child_of(anm_ids[index], script);
    vm->interrupt(interrupt);
    vm->run();
}

// FUNCTION: TH16 0x44a790
AnmId TitleInf::find_child_id(i32 index, i32 script)
{
    AnmVm *vm = get_vm_or_clear(anm_ids[index]);
    if (vm == NULL)
    {
        return AnmId();
    }
    AnmVm *found = vm->search_children(script, 0);
    AnmId result;
    result.id = found != NULL ? found->id.id : 0;
    return result;
}

// FUNCTION: TH16 0x44a9f0
TitleInf::TitleInf()
{
    memset(this, 0, sizeof(TitleInf));
    g_MainMenu = this;
    flags |= 2;
}

// The menu's setup thread: initializes the menu, waits for the loading
// screen to have run for 3 seconds, then turns the menu's tick on. If setup
// fails the game quits.
// FUNCTION: TH16 0x44abc0
unsigned __stdcall TitleInf::thread_start()
{
    if (g_MainMenu->initialize() != 0)
    {
        g_Supervisor.gamemode_to_switch_to =
            (g_Supervisor.flags & SUPERVISOR_IDLE_ON_EXIT) ? GAMEMODE_IDLE : GAMEMODE_QUIT;
        return 0;
    }
    if (g_LoadingThread != NULL)
    {
        while (g_LoadingThread->draw_count < 180 &&
               !(g_Supervisor.flags & (SUPERVISOR_QUIT_REQUESTED | SUPERVISOR_FLAG_100)))
        {
            Sleep(16);
        }
        g_AnmManager->unload_anm(ANM_SLOT_SIG);
    }
    g_MainMenu->on_tick_func->flags |= UPDATE_FUNC_ACTIVE;
    g_frame_pacing.mode = FRAME_PACING_MENU;
    return 0;
}

// Registers the tick (priority 6) and draw (0x45) callbacks, inactive for
// now, and loads title.anm and title_v.anm.
// FUNCTION: TH16 0x44ac70
HARNESS_CALLED i32 TitleInf::initialize()
{
    UpdateFunc *f;

    f = g_UpdateFuncRegistry->create_func(on_tick_thunk);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 6);
    on_tick_func = f;

    f = g_UpdateFuncRegistry->create_func(on_draw_thunk);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x45);
    on_draw_func = f;

    title_anm = AnmManager::preload_anm(ANM_SLOT_TITLE, "title.anm");
    if (title_anm == NULL)
    {
        // データが壊れています ("The data is corrupted")
        g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    title_v_anm = AnmManager::preload_anm(ANM_SLOT_TITLE_V, "title_v.anm");
    if (title_v_anm == NULL)
    {
        g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    menu.wraps = 1;
    g_cancel_screen_effects = 0;
    return 0;
}

// Waits for the menu's thread, unregisters the callbacks, unloads the
// menu's ANM files and frees the replay list and musiccmt.txt.
// FUNCTION: TH16 0x44ad20
TitleInf::~TitleInf()
{
    thread.join_if_running();
    g_UpdateFuncRegistry->unregister_locked(on_tick_func);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func);
    g_AnmManager->unload_anm(ANM_SLOT_TITLE);
    g_AnmManager->unload_anm(ANM_SLOT_TITLE_V);
    for (i32 i = 0; i < 100; i++)
    {
        delete replays[i];
    }
    AnmManager::interrupt_tree(submenu_ascii_id, 1);
    if (music_comment_file != NULL)
    {
        free(music_comment_file);
        music_comment_file = NULL;
    }
    g_MainMenu = NULL;
}

// FUNCTION: TH16 0x44aee0
TitleInf *TitleInf::create()
{
    TitleInf *menu = new TitleInf;
    g_frame_pacing.mode = FRAME_PACING_IDLE;
    menu->thread.restart((ThreadStart)thread_start, menu);
    return menu;
}

// FUNCTION: TH16 0x44af50
void TitleInf::destroy()
{
    if (g_MainMenu != NULL)
    {
        delete g_MainMenu;
    }
}

// Realigns its frame (and esp, -8) for the draw states it calls, which then
// rely on it (some pass doubles to create_stringf).
// FUNCTION: TH16 0x44b530
i32 TitleInf::on_draw()
{
    switch (state)
    {
    case TITLE_STATE_REPLAY_MENU:
        on_draw__replay();
        break;
    case TITLE_STATE_PLAYER_DATA:
        on_draw__player_data();
        break;
    case TITLE_STATE_SCORE_NAME_ENTRY:
        on_draw__score_name_entry();
        break;
    case TITLE_STATE_REPLAY_SAVE:
        on_draw__replay_save();
        break;
    case TITLE_STATE_PRACTICE_STAGE_SELECT:
        on_draw__practice_stage_select();
        break;
    case TITLE_STATE_SPELL_PRACTICE_ROW_SELECT:
    case TITLE_STATE_SPELL_PRACTICE_DIFFICULTY_SELECT:
        on_draw__spell_practice_histories();
        break;
    }
    return 1;
}

// The original's callback is a jmp to the member function, most likely the
// fastcall invoker of a capture-less lambda; a static thunk compiles the same.
// FUNCTION: TH16 0x44b5d0
i32 __fastcall TitleInf::on_tick_thunk(void *arg)
{
    return ((TitleInf *)arg)->on_tick();
}

// The original's callback is a jmp to the member function, most likely the
// fastcall invoker of a capture-less lambda; a static thunk compiles the same.
// FUNCTION: TH16 0x44b5e0
i32 __fastcall TitleInf::on_draw_thunk(void *arg)
{
    return ((TitleInf *)arg)->on_draw();
}

i32 __stdcall input_pressed_or_repeating(u32 mask);

// input_pressed_or_repeating as the key config screen inlines it: the
// original tests the low bytes of both input words (mov cl/al, byte ptr).
static __forceinline i32 key_config_pressed_or_repeating(u8 mask)
{
    if (*(u8 *)&g_hardware_input_pressed & mask)
    {
        return 1;
    }
    if (*(u8 *)&g_hardware_input_repeat & mask)
    {
        return 1;
    }
    return 0;
}

// FUNCTION: TH16 0x44c570
i32 TitleInf::do_options()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = OPTIONS_ITEM_COUNT;
        menu.set_cursor(0);
        anm_ids[1] = title_anm->create_effect(1, -1, NULL);
        update_options_sprites();
        set_substate(1);
    case 1:
        if (time_in_state.current > 6)
        {
            set_substate(2);
            AnmManager::interrupt_tree_and_run(anm_ids[1], 3);
            AnmManager::interrupt_tree(anm_ids[1], (i16)(menu.next_selection + 0x11));
            update_options_cursor();
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
            AnmManager::interrupt_tree_and_run(anm_ids[1], 3);
            AnmManager::interrupt_tree(anm_ids[1], (i16)(menu.next_selection + 7));
            update_options_cursor();
        }
        if (g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB))
        {
            if (menu.next_selection != OPTIONS_ITEM_QUIT)
            {
                g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                menu.set_cursor(OPTIONS_ITEM_QUIT);
                AnmManager::interrupt_tree_and_run(anm_ids[1], 3);
                AnmManager::interrupt_tree(anm_ids[1], (i16)(menu.next_selection + 7));
                update_options_cursor();
                return 1;
            }
            goto leave;
        }
        if (menu.next_selection == OPTIONS_ITEM_SE_VOLUME && time_in_state.ticked_on_multiple_of(60))
        {
            g_SoundManager.play_sound_centered(SE_PLDEAD00, 0);
        }
        if (input_pressed_or_repeating(INPUT_LEFT))
        {
            switch (menu.next_selection)
            {
            case OPTIONS_ITEM_BGM_VOLUME:
                if (g_Supervisor.config.bgm_volume < 5)
                {
                    g_Supervisor.config.bgm_volume = 0;
                }
                else
                {
                    g_Supervisor.config.bgm_volume -= 5;
                }
                update_options_sprites();
                break;
            case OPTIONS_ITEM_SE_VOLUME:
                if (g_Supervisor.config.se_volume < 5)
                {
                    g_Supervisor.config.se_volume = 0;
                }
                else
                {
                    g_Supervisor.config.se_volume -= 5;
                }
                update_options_sprites();
                break;
            }
        }
        if (input_pressed_or_repeating(INPUT_RIGHT))
        {
            switch (menu.next_selection)
            {
            case OPTIONS_ITEM_BGM_VOLUME:
                g_Supervisor.config.bgm_volume += 5;
                if (g_Supervisor.config.bgm_volume > 100)
                {
                    g_Supervisor.config.bgm_volume = 100;
                }
                update_options_sprites();
                break;
            case OPTIONS_ITEM_SE_VOLUME:
                g_Supervisor.config.se_volume += 5;
                if (g_Supervisor.config.se_volume > 100)
                {
                    g_Supervisor.config.se_volume = 100;
                }
                update_options_sprites();
                break;
            }
        }
        if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
        {
            switch (menu.next_selection)
            {
            case OPTIONS_ITEM_KEY_CONFIG:
                AnmManager::interrupt_tree(anm_ids[1], 6);
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                set_substate(4);
                return 1;
            case OPTIONS_ITEM_DEFAULT:
                g_Supervisor.config.bgm_volume = 100;
                g_Supervisor.config.se_volume = 80;
                g_Supervisor.config.unk_28 = 0;
                update_options_sprites();
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                return 1;
            case OPTIONS_ITEM_QUIT:
            leave:
                AnmManager::interrupt_tree(anm_ids[1], 6);
                g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                set_substate(4);
                return 1;
            }
        }
        break;
    case 4:
        if (time_in_state.current >= 10)
        {
            switch (menu.next_selection)
            {
            case OPTIONS_ITEM_KEY_CONFIG:
                set_state(TITLE_STATE_KEY_CONFIG);
                menu.push();
                break;
            case OPTIONS_ITEM_QUIT:
                set_state(TITLE_STATE_MAIN);
                menu.pop();
                return 1;
            }
        }
        break;
    }
    return 1;
}

// Rows above the cursor get interrupt 30, rows below it 31; the digits of
// the two volumes follow their rows.
// FUNCTION: TH16 0x44c8c0
void TitleInf::update_options_cursor()
{
    i32 i;
    for (i = 0; i < menu.next_selection; i++)
    {
        interrupt_child_and_run(1, i + 0x17, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run(1, i + 0x1c, TITLE_INTERRUPT_ABOVE_CURSOR);
    }
    for (i++; i < 5; i++)
    {
        interrupt_child_and_run(1, i + 0x17, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(1, i + 0x1c, TITLE_INTERRUPT_BELOW_CURSOR);
    }
    if (menu.next_selection > 0)
    {
        interrupt_child_and_run_inline(anm_ids[1], 0x21, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x22, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x23, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x24, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x25, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x26, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x27, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x28, TITLE_INTERRUPT_ABOVE_CURSOR);
    }
    if (menu.next_selection > 1)
    {
        interrupt_child_and_run_inline(anm_ids[1], 0x29, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x2a, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x2b, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x2c, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x2d, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x2e, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x2f, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run_inline(anm_ids[1], 0x30, TITLE_INTERRUPT_ABOVE_CURSOR);
    }
    else if (menu.next_selection < 1)
    {
        interrupt_child_and_run(1, 0x29, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(1, 0x2a, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(1, 0x2b, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(1, 0x2c, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(1, 0x2d, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(1, 0x2e, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(1, 0x2f, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(1, 0x30, TITLE_INTERRUPT_BELOW_CURSOR);
    }
}

// Applies the volumes and shows them: three digits each, in two layers of
// sprites, with leading zeros hidden.
// FUNCTION: TH16 0x44dc70
void TitleInf::update_options_sprites()
{
    g_SoundManager.bgm_volume = g_Supervisor.config.bgm_volume;
    g_SoundManager.modify_bgm(BGM_RESET_VOLUME, 0, "SetVol");
    g_SoundManager.se_volume = g_Supervisor.config.se_volume;
    if (g_SoundManager.se_volume != 0)
    {
        f32 x = g_SoundManager.bgm_volume / 100.0f;
        f32 t = (1.0f - x) * (1.0f - x);
        f32 u = 1.0f - t * t;
        g_SoundManager.bgm_db = -5000 - (i32)(u * -5000.0f);
    }
    else
    {
        g_SoundManager.bgm_db = -10000;
    }
    set_child_sprite(get_child_vm(anm_ids[1], 0x21), g_Supervisor.config.bgm_volume / 100 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[1], 0x22), g_Supervisor.config.bgm_volume / 10 % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[1], 0x23), g_Supervisor.config.bgm_volume % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[1], 0x25), g_Supervisor.config.bgm_volume / 100 + 0x35);
    set_child_sprite(get_child_vm(anm_ids[1], 0x26), g_Supervisor.config.bgm_volume / 10 % 10 + 0x35);
    set_child_sprite(get_child_vm(anm_ids[1], 0x27), g_Supervisor.config.bgm_volume % 10 + 0x35);
    set_child_sprite(get_child_vm(anm_ids[1], 0x29), g_Supervisor.config.se_volume / 100 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[1], 0x2a), g_Supervisor.config.se_volume / 10 % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[1], 0x2b), g_Supervisor.config.se_volume % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[1], 0x2d), g_Supervisor.config.se_volume / 100 + 0x35);
    set_child_sprite(get_child_vm(anm_ids[1], 0x2e), g_Supervisor.config.se_volume / 10 % 10 + 0x35);
    set_child_sprite(get_child_vm(anm_ids[1], 0x2f), g_Supervisor.config.se_volume % 10 + 0x35);
    if (g_Supervisor.config.bgm_volume < 10)
    {
        get_child_vm(anm_ids[1], 0x21)->flags_lo &= ~ANM_VM_SHOWN;
        get_child_vm(anm_ids[1], 0x22)->flags_lo &= ~ANM_VM_SHOWN;
        get_child_vm(anm_ids[1], 0x25)->flags_lo &= ~ANM_VM_SHOWN;
        get_child_vm(anm_ids[1], 0x26)->flags_lo &= ~ANM_VM_SHOWN;
    }
    else if (g_Supervisor.config.bgm_volume < 100)
    {
        get_vm_or_clear(find_child_id(1, 0x21))->flags_lo &= ~ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x22))->flags_lo |= ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x25))->flags_lo &= ~ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x26))->flags_lo |= ANM_VM_SHOWN;
    }
    else
    {
        get_vm_or_clear(find_child_id(1, 0x21))->flags_lo |= ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x22))->flags_lo |= ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x25))->flags_lo |= ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x26))->flags_lo |= ANM_VM_SHOWN;
    }
    if (g_Supervisor.config.se_volume < 10)
    {
        get_child_vm(anm_ids[1], 0x29)->flags_lo &= ~ANM_VM_SHOWN;
        get_child_vm(anm_ids[1], 0x2a)->flags_lo &= ~ANM_VM_SHOWN;
        get_child_vm(anm_ids[1], 0x2d)->flags_lo &= ~ANM_VM_SHOWN;
        get_child_vm(anm_ids[1], 0x2e)->flags_lo &= ~ANM_VM_SHOWN;
    }
    else if (g_Supervisor.config.se_volume < 100)
    {
        get_vm_or_clear(find_child_id(1, 0x29))->flags_lo &= ~ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x2a))->flags_lo |= ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x2d))->flags_lo &= ~ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x2e))->flags_lo |= ANM_VM_SHOWN;
    }
    else
    {
        get_vm_or_clear(find_child_id(1, 0x29))->flags_lo |= ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x2a))->flags_lo |= ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x2d))->flags_lo |= ANM_VM_SHOWN;
        get_vm_or_clear(find_child_id(1, 0x2e))->flags_lo |= ANM_VM_SHOWN;
    }
}

// TODO: for the up/down tests the original loads the pressed word as a byte
// (mov cl, byte ptr); ours loads the dword (the byte reads only narrow the
// repeat load).
// FUNCTION: TH16 0x44e930
i32 TitleInf::do_key_config()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = KEY_CONFIG_COUNT;
        menu.set_cursor(0);
        anm_ids[2] = title_anm->create_effect(2, -1, NULL);
        set_substate(1);
        key_config[KEY_CONFIG_SHOT] = g_pad_mapping[PAD_SHOT];
        key_config[KEY_CONFIG_BOMB] = g_pad_mapping[PAD_BOMB];
        key_config[KEY_CONFIG_RELEASE] = g_pad_mapping[PAD_RELEASE];
        key_config[KEY_CONFIG_FOCUS] = g_pad_mapping[PAD_FOCUS];
        key_config[KEY_CONFIG_PAUSE] = g_pad_mapping[PAD_PAUSE];
        update_key_config_sprites();
    case 1:
        if (time_in_state.current > 6)
        {
            set_substate(2);
            AnmManager::interrupt_tree_and_run(anm_ids[2], 3);
            AnmManager::interrupt_tree(anm_ids[2], (i16)(menu.next_selection + 0x11));
            update_key_config_cursor();
            return 1;
        }
        break;
    case 2:
    {
        menu.current_selection = menu.next_selection;
        if (key_config_pressed_or_repeating(INPUT_UP))
        {
            menu.move_cursor(-1);
        }
        if (key_config_pressed_or_repeating(INPUT_DOWN))
        {
            menu.move_cursor(1);
        }
        if (menu.current_selection != menu.next_selection)
        {
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[2], 3);
            AnmManager::interrupt_tree(anm_ids[2], (i16)(menu.next_selection + 7));
            update_key_config_cursor();
        }
        u8 *pad = get_controller_state();
        for (i32 i = 0; i < 0x1f; i++)
        {
            if ((i8)pad[i] < 0)
            {
                if (menu.next_selection <= KEY_CONFIG_PAUSE)
                {
                    set_key(menu.next_selection, i);
                }
                break;
            }
        }
        if ((g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB)) && menu.next_selection == KEY_CONFIG_QUIT)
        {
            key_config[KEY_CONFIG_SHOT] = g_pad_mapping[PAD_SHOT];
            key_config[KEY_CONFIG_BOMB] = g_pad_mapping[PAD_BOMB];
            key_config[KEY_CONFIG_RELEASE] = g_pad_mapping[PAD_RELEASE];
            key_config[KEY_CONFIG_FOCUS] = g_pad_mapping[PAD_FOCUS];
            key_config[KEY_CONFIG_PAUSE] = g_pad_mapping[PAD_PAUSE];
            update_key_config_sprites();
        }
        else
        {
            if (!(g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT)))
            {
                break;
            }
            switch (menu.next_selection)
            {
            case KEY_CONFIG_DEFAULT:
                key_config[KEY_CONFIG_SHOT] = g_pad_mapping[PAD_SHOT];
                key_config[KEY_CONFIG_BOMB] = g_pad_mapping[PAD_BOMB];
                key_config[KEY_CONFIG_RELEASE] = g_pad_mapping[PAD_RELEASE];
                key_config[KEY_CONFIG_FOCUS] = g_pad_mapping[PAD_FOCUS];
                key_config[KEY_CONFIG_PAUSE] = g_pad_mapping[PAD_PAUSE];
                update_key_config_sprites();
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                return 1;
            case KEY_CONFIG_QUIT:
                g_pad_mapping[PAD_SHOT] = key_config[KEY_CONFIG_SHOT];
                g_pad_mapping[PAD_BOMB] = key_config[KEY_CONFIG_BOMB];
                g_pad_mapping[PAD_RELEASE] = key_config[KEY_CONFIG_RELEASE];
                g_pad_mapping[PAD_FOCUS] = key_config[KEY_CONFIG_FOCUS];
                g_pad_mapping[PAD_PAUSE] = key_config[KEY_CONFIG_PAUSE];
                memcpy(g_Supervisor.config.pad_mapping, g_pad_mapping, sizeof(g_pad_mapping));
                break;
            default:
                return 1;
            }
        }
        g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
        AnmManager::interrupt_tree(anm_ids[2], 6);
        set_substate(4);
        return 1;
    }
    case 4:
        if (time_in_state.current >= 10)
        {
            set_state(TITLE_STATE_OPTIONS);
            menu.pop();
        }
        break;
    }
    return 1;
}

// Two digits per action, in two layers of sprites.
// FUNCTION: TH16 0x44ec60
void TitleInf::update_key_config_sprites()
{
    set_child_sprite(get_child_vm(anm_ids[2], 0x3f), key_config[KEY_CONFIG_SHOT] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x40), key_config[KEY_CONFIG_SHOT] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x49), key_config[KEY_CONFIG_SHOT] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4a), key_config[KEY_CONFIG_SHOT] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x41), key_config[KEY_CONFIG_BOMB] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x42), key_config[KEY_CONFIG_BOMB] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4b), key_config[KEY_CONFIG_BOMB] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4c), key_config[KEY_CONFIG_BOMB] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x43), key_config[KEY_CONFIG_RELEASE] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x44), key_config[KEY_CONFIG_RELEASE] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4d), key_config[KEY_CONFIG_RELEASE] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4e), key_config[KEY_CONFIG_RELEASE] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x45), key_config[KEY_CONFIG_FOCUS] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x46), key_config[KEY_CONFIG_FOCUS] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4f), key_config[KEY_CONFIG_FOCUS] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x50), key_config[KEY_CONFIG_FOCUS] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x47), key_config[KEY_CONFIG_PAUSE] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x48), key_config[KEY_CONFIG_PAUSE] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x51), key_config[KEY_CONFIG_PAUSE] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x52), key_config[KEY_CONFIG_PAUSE] % 10 + 0x2a);
}

// FUNCTION: TH16 0x44f710
void TitleInf::set_key(i32 action, i32 key)
{
    if (key_config[action] == key)
    {
        return;
    }
    for (i32 i = 0; i < 6; i++)
    {
        if (i != action && key_config[i] == key)
        {
            key_config[i] = key_config[action];
        }
    }
    key_config[action] = key;
    update_key_config_sprites();
    g_SoundManager.play_sound_centered(SE_OK00, 0);
}

// Rows above the cursor get interrupt 30, rows below it 31; the five
// remappable actions have two more pairs of sprites each.
// FUNCTION: TH16 0x44f810
void TitleInf::update_key_config_cursor()
{
    i32 i;
    for (i = 0; i < menu.next_selection; i++)
    {
        interrupt_child_and_run(2, i + 0x31, TITLE_INTERRUPT_ABOVE_CURSOR);
        interrupt_child_and_run(2, i + 0x38, TITLE_INTERRUPT_ABOVE_CURSOR);
        if (i < 5)
        {
            interrupt_child_and_run(2, i * 2 + 0x3f, TITLE_INTERRUPT_ABOVE_CURSOR);
            interrupt_child_and_run(2, i * 2 + 0x40, TITLE_INTERRUPT_ABOVE_CURSOR);
            interrupt_child_and_run(2, i * 2 + 0x49, TITLE_INTERRUPT_ABOVE_CURSOR);
            interrupt_child_and_run(2, i * 2 + 0x4a, TITLE_INTERRUPT_ABOVE_CURSOR);
        }
    }
    for (i++; i < 7; i++)
    {
        interrupt_child_and_run(2, i + 0x31, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(2, i + 0x38, TITLE_INTERRUPT_BELOW_CURSOR);
    }
    for (i = menu.next_selection + 1; i < 5; i++)
    {
        interrupt_child_and_run(2, i * 2 + 0x3f, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(2, i * 2 + 0x40, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(2, i * 2 + 0x49, TITLE_INTERRUPT_BELOW_CURSOR);
        interrupt_child_and_run(2, i * 2 + 0x4a, TITLE_INTERRUPT_BELOW_CURSOR);
    }
}

// FUNCTION: TH16 0x44a800
HARNESS_CALLED i32 Scorefile::has_cleared(i32 character)
{
    if (characters[character].clears[0] != 0 || characters[character].clears[1] != 0 ||
        characters[character].clears[2] != 0 || characters[character].clears[3] != 0)
    {
        return 1;
    }
    return 0;
}

// FUNCTION: TH16 0x44a850
HARNESS_CALLED i32 Scorefile::any_cleared()
{
    if (has_cleared_inline(0) || has_cleared_inline(1) || has_cleared_inline(2) || has_cleared(3))
    {
        return 1;
    }
    return 0;
}

// FUNCTION: TH16 0x44a8e0
HARNESS_CALLED i32 Scorefile::all_cleared(i32 difficulty)
{
    if (characters[0].clears[difficulty] != 0 && characters[3].clears[difficulty] != 0 &&
        characters[1].clears[difficulty] != 0 && characters[2].clears[difficulty] != 0)
    {
        return 1;
    }
    return 0;
}

// The loops run past the arrays: six difficulties of five-entry arrays, and
// a second spell counter 0x5210 bytes past each spell of the total entry,
// beyond the end of the score data.
// FUNCTION: TH16 0x44a930
HARNESS_CALLED void Scorefile::unlock_all()
{
    memset(endings_seen, 0x11, 16);
    for (i32 i = 0; i < 0x77; i++)
    {
        ScorefileSpell *spell = &characters[4].spells[i];
        if (spell->attempts[0] < 99999)
        {
            spell->attempts[0]++;
        }
        i32 *count = (i32 *)((u8 *)spell + 0x5210);
        if (*count < 99999)
        {
            (*count)++;
        }
    }
    for (i32 c = 0; c < 4; c++)
    {
        for (i32 d = 0; d < 6; d++)
        {
            characters[c].play_counts[d]++;
            characters[c].clears[d]++;
            for (i32 s = 0; s < 8; s++)
            {
                characters[c].practices[d][s].unlocked = 1;
            }
        }
    }
}

// Helpers of the music room and spell practice menus.

// The spell cards of each stage (Extra last) in spell practice: one row per
// boss attack, one id per difficulty, -1 after the last.
// GLOBAL: TH16 0x490ee0
extern const i32 g_spell_practice_ids[7][13][5] = {
    {{0, 1, 2, 3, -1}, {4, 5, 6, 7, -1}},
    {{8, 9, 10, 11, -1}, {12, 13, 14, 15, -1}, {16, 17, 18, 19, -1}},
    {{20, 21, -1}, {22, 23, 24, 25, -1}, {26, 27, 28, 29, -1}, {30, 31, 32, 33, -1}},
    {{34, 35, 36, 37, -1}, {38, 39, 40, 41, -1}, {42, 43, 44, 45, -1}},
    {{46, 47, 48, 49, -1},
     {50, 51, 52, 53, -1},
     {54, 55, 56, 57, -1},
     {58, 59, 60, 61, -1},
     {62, 63, 64, 65, -1},
     {66, 67, 68, 69, -1}},
    {{70, 71, 72, 73, -1},
     {74, 75, 76, 77, -1},
     {78, 79, 80, 81, -1},
     {82, 83, 84, 85, -1},
     {86, 87, 88, 89, -1},
     {90, 91, 92, 93, -1},
     {94, 95, 96, 97, -1},
     {98, 99, 100, 101, -1},
     {102, 103, 104, 105, -1}},
    {{106, -1},
     {107, -1},
     {108, -1},
     {109, -1},
     {110, -1},
     {111, -1},
     {112, -1},
     {113, -1},
     {114, -1},
     {115, -1},
     {116, -1},
     {117, -1},
     {118, -1}},
};

// Skips to the end of the line and past the line breaks, counting the
// bytes left down; stops when none are left.
// FUNCTION: TH16 0x455330
char *__fastcall skip_line(char *p, i32 *remaining)
{
    while (*p != '\n' && *p != '\r')
    {
        if (*remaining == 0)
        {
            return p;
        }
        p++;
        (*remaining)--;
    }
    if (*remaining == 0)
    {
        return p;
    }
    while (*p == '\n' || *p == '\r')
    {
        if (*remaining == 0)
        {
            return p;
        }
        p++;
        (*remaining)--;
    }
    return p;
}

// Copies the line starting at src into dst and returns the start of the
// next line.
// TODO: ours pads the second loop's head with a nop to 16 bytes; the original does not align it.
// FUNCTION: TH16 0x455370
char *__fastcall read_line(char *dst, char *src, i32 *remaining)
{
    char *p = src;
    while (*p != '\n' && *p != '\r')
    {
        if (*remaining == 0)
        {
            return p;
        }
        p++;
        (*remaining)--;
    }
    i32 left = *remaining;
    if (left == 0)
    {
        return p;
    }
    *p = '\0';
    strcpy(dst, src);
    p++;
    for (i32 n = left - 1;; n--)
    {
        char c = *p;
        *remaining = n;
        if (c != '\n' && c != '\r')
        {
            break;
        }
        if (n == 0)
        {
            return p;
        }
        p++;
    }
    return p;
}

// Whether the main game has seen any spell card of a spell practice row,
// which makes the row selectable.
// FUNCTION: TH16 0x456060
BOOL __stdcall spell_practice_row_seen(i32 stage, i32 row)
{
    for (i32 i = 0; i < 5; i++)
    {
        i32 id = g_spell_practice_ids[stage][row][i];
        if (id < 0)
        {
            break;
        }
        if (g_Scorefile->characters[4].spells[id].attempts[0] != 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// FUNCTION: TH16 0x451740
unsigned TitleInf::replay_list_thread(void *arg)
{
    load_replay_list();
    return 0;
}

// GLOBAL: TH16 0x4a5bf0
i32 g_title_idle_frames;
// GLOBAL: TH16 0x4a5bf4
i32 g_demo_replay_index;

// GLOBAL: TH16 0x49371c
const char *const g_demo_replay_names[3] = {"demo/demo1.rpy", "demo/demo2.rpy", "demo/demo3.rpy"};

// Plays a demo replay after 30 idle seconds on the title screen, starts the
// title BGM a few frames after it appears, and runs the current screen.
// The input test masks the low word (a u16 cast compares the word in
// memory) and the timer ticks as tick_mixed.
// TODO: the original computes the demo index as x % -3 would (imul 0x55555555; sub; sar 1); ours uses idiv, or the /3 magic through a local (also with % -3, x / -3 * 3, or % through an inline helper); it also calls the Supervisor members without this and keeps the replay info in ecx.
// FUNCTION: TH16 0x44af80
i32 TitleInf::on_tick()
{
    if (state == TITLE_STATE_MAIN)
    {
        g_title_idle_frames++;
        if (g_hardware_input & 0xffff)
        {
            g_title_idle_frames = 0;
        }
        else if (g_title_idle_frames >= 1800)
        {
            // Idle on the title screen for 30 seconds: play a demo replay.
            g_Globals.flags_hi_45c = (g_Globals.flags_hi_45c & ~GLOBALS_HI_2) | GLOBALS_HI_DEMO_PLAY;
            strcpy(g_current_replay_filename, g_demo_replay_names[g_demo_replay_index]);
            ReplayManager *replay = ReplayManager::create_from_file(g_current_replay_filename);
            g_demo_replay_index = (g_demo_replay_index + 1) % 3;
            i32 stage;
            for (stage = 0; stage < 8; stage++)
            {
                if (replay->stages[stage].gamestate_at_stage_begin != NULL)
                {
                    break;
                }
            }
            // Stage table pointer first, for matching (see do_spell_practice_difficulty).
            g_stage_data = &g_stage_table[stage];
            g_Globals.stage_num = stage;
            g_Globals.weird_stage_num = stage;
            g_Supervisor.gamemode_to_switch_to = GAMEMODE_START_REPLAY;
            RpyInfo *info = replay->info;
            g_Globals.character = info->character;
            g_Globals.subshot = info->subshot;
            g_Globals.subseason = info->subseason;
            g_Globals.difficulty_before_demo = g_Globals.difficulty;
            g_Globals.difficulty = info->difficulty;
            replay->~ReplayManager();
            operator delete(replay, sizeof(ReplayManager));
            g_title_return_point = TITLE_RETURN_MAIN;
            g_title_idle_frames = 0;
        }
    }
    if (menu_flags & TITLE_START_BGM)
    {
        bgm_start_delay++;
        if (bgm_start_delay >= 10)
        {
            if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
            {
                g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
            }
            else
            {
                g_SoundManager.modify_bgm(BGM_STOP, 0, "dummy");
            }
            g_SoundManager.bgm_name[0] = 0;
            g_Supervisor.play_bgm_wav(0, "th16_01");
            if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
            {
                g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
            }
            g_SoundManager.modify_bgm(BGM_PLAY, 0, "dummy");
            g_Scorefile->bgm_unlocked[0] = 1;
            menu_flags &= ~TITLE_START_BGM;
            bgm_start_delay = 0;
        }
    }
    switch (state)
    {
    case TITLE_STATE_INIT:
    {
        AnmManager *anm = g_AnmManager;
        anm->disable_vms_from_anm_file(anm->loaded_anms[ANM_SLOT_FRONT]);
        anm->disable_vms_from_anm_file(anm->loaded_anms[ANM_SLOT_BULLET]);
        anm->disable_vms_from_anm_file(anm->loaded_anms[ANM_SLOT_ASCII]);
        anm->disable_vms_from_anm_file(anm->loaded_anms[ANM_SLOT_TEXT]);
        g_AsciiManager->hide_now_loading();
        if (g_title_return_point == TITLE_RETURN_SCORE_ENTRY)
        {
            menu.num_choices = 10;
            menu.set_cursor(0);
            menu.push();
            set_state(TITLE_STATE_SCORE_NAME_ENTRY);
            g_title_return_point = TITLE_RETURN_MAIN;
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_score_name_entry();
            break;
        }
        if (!(g_Globals.flags_hi_45c & GLOBALS_HI_DEMO_PLAY))
        {
            menu_flags |= TITLE_START_BGM;
            bgm_start_delay = 0;
        }
        else
        {
            menu_flags &= ~TITLE_START_BGM;
            g_Globals.difficulty = g_Globals.difficulty_before_demo;
        }
        g_Globals.flags_hi_45c &= ~GLOBALS_HI_DEMO_PLAY;
        if (g_title_return_point == TITLE_RETURN_FIRST)
        {
            menu_flags |= TITLE_FIRST_SHOW;
            set_state(TITLE_STATE_MAIN);
            g_title_return_point = TITLE_RETURN_MAIN;
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_title_screen();
            break;
        }
        menu_flags &= ~TITLE_FIRST_SHOW;
        if (g_title_return_point == TITLE_RETURN_MAIN)
        {
            if (g_Globals.difficulty == DIFFICULTY_EXTRA)
            {
                menu.set_cursor(TITLE_ITEM_EXTRA_START);
            }
            set_state(TITLE_STATE_MAIN);
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_title_screen();
        }
        else if (g_title_return_point == TITLE_RETURN_REPLAY_MENU)
        {
            g_Supervisor.play_bgm_wav(0, "th16_01");
            g_Supervisor.play_bgm(0, 0);
            g_Globals.set_game_mode(GAME_MODE_NORMAL);
            menu.num_choices = TITLE_ITEM_COUNT;
            menu.set_cursor(TITLE_ITEM_REPLAY);
            menu.push();
            set_state(TITLE_STATE_REPLAY_MENU);
            g_title_return_point = TITLE_RETURN_MAIN;
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_replay_menu();
        }
        else if (g_title_return_point == TITLE_RETURN_SPELL_PRACTICE)
        {
            ScreenEffect::create(SCREEN_EFFECT_HOLD, 30, 0, 0, 0, 0x54);
            anm_ids[0x61] = title_anm->create_effect(0x61, -1, NULL);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(TITLE_STATE_SPELL_PRACTICE_STAGE_SELECT);
            g_title_return_point = TITLE_RETURN_MAIN;
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_spell_practice_stage_select();
        }
        else if (g_title_return_point == TITLE_RETURN_PRACTICE)
        {
            ScreenEffect::create(SCREEN_EFFECT_HOLD, 30, 0, 0, 0, 0x54);
            anm_ids[0x61] = title_anm->create_effect(0x61, -1, NULL);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(TITLE_STATE_DIFFICULTY_SELECT);
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_difficulty_select();
        }
        else
        {
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_title_screen();
        }
        break;
    }
    case TITLE_STATE_MAIN:
        on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
        do_title_screen();
        break;
    case TITLE_STATE_DIFFICULTY_SELECT:
        do_difficulty_select();
        break;
    case TITLE_STATE_REPLAY_MENU:
        do_replay_menu();
        break;
    case TITLE_STATE_SCORE_NAME_ENTRY:
        do_score_name_entry();
        break;
    case TITLE_STATE_SPELL_PRACTICE_STAGE_SELECT:
        do_spell_practice_stage_select();
        break;
    case TITLE_STATE_OPTIONS:
        do_options();
        break;
    case TITLE_STATE_KEY_CONFIG:
        do_key_config();
        break;
    case TITLE_STATE_EXIT:
        g_Supervisor.gamemode_to_switch_to =
            (g_Supervisor.flags & SUPERVISOR_IDLE_ON_EXIT) ? GAMEMODE_IDLE : GAMEMODE_QUIT;
        g_Supervisor.stop_bgm();
        break;
    case TITLE_STATE_UNUSED_9:
        g_Supervisor.stop_bgm();
        break;
    case TITLE_STATE_UNUSED_12:
        g_Supervisor.stop_bgm();
        break;
    case TITLE_STATE_CHARACTER_SELECT:
        do_character_select();
        break;
    case TITLE_STATE_SUBSEASON_SELECT:
        do_subseason_select();
        break;
    case TITLE_STATE_PRACTICE_STAGE_SELECT:
        do_practice_stage_select();
        break;
    case TITLE_STATE_MUSIC_ROOM:
        do_music_room();
        break;
    case TITLE_STATE_SPELL_PRACTICE_ROW_SELECT:
        do_spell_practice_row();
        break;
    case TITLE_STATE_SPELL_PRACTICE_DIFFICULTY_SELECT:
        do_spell_practice_difficulty();
        break;
    case TITLE_STATE_SPELL_PRACTICE_SUBSEASON_SELECT:
        do_spell_practice_subseason();
        break;
    case TITLE_STATE_PLAYER_DATA:
        do_player_data();
        break;
    case TITLE_STATE_MANUAL:
        do_manual();
        break;
    case TITLE_STATE_REPLAY_SAVE:
        do_replay_save();
        break;
    }
    time_in_state.tick_mixed();
    return 1;
}

extern i32 g_last_difficulty;
extern i32 g_last_character;

// interrupt_child and interrupt_child_and_run as LTCG inlined them into
// the title screen.
static __forceinline void title_interrupt_child(TitleInf *menu, i32 script, i32 interrupt)
{
    AnmVm *vm = find_child_of(menu->anm_ids[0], script);
    vm->interrupt(interrupt);
}

// title_interrupt_child with search_children inlined too, as for the
// greyed out Extra Start items.
static __forceinline void title_interrupt_child_inline(TitleInf *menu, i32 script, i32 interrupt)
{
    AnmVm *vm;
    if (get_vm_or_clear(menu->anm_ids[0]) == NULL)
    {
        vm = NULL;
    }
    else
    {
        vm = search_children_inline(get_vm_or_clear(menu->anm_ids[0]), script, 0);
    }
    vm->interrupt(interrupt);
}

static __forceinline void title_interrupt_child_and_run(TitleInf *menu, i32 script, i32 interrupt)
{
    AnmVm *vm = find_child_of(menu->anm_ids[0], script);
    vm->interrupt(interrupt);
    vm->run();
}

// Lights up the selected title menu item and dims the others (scripts 3
// and up are the items, 13 and up their shadows); without a clear, Extra
// Start and its shadow stay grey.
static __forceinline void title_highlight_inline(TitleInf *menu)
{
    i32 i;
    for (i = 0; i < menu->menu.next_selection; i++)
    {
        title_interrupt_child_and_run(menu, i + 3, TITLE_INTERRUPT_ABOVE_CURSOR);
        title_interrupt_child_and_run(menu, i + 13, TITLE_INTERRUPT_ABOVE_CURSOR);
    }
    for (i++; i < 10; i++)
    {
        title_interrupt_child_and_run(menu, i + 3, TITLE_INTERRUPT_BELOW_CURSOR);
        title_interrupt_child_and_run(menu, i + 13, TITLE_INTERRUPT_BELOW_CURSOR);
    }
}

// The first "no clear" greying inlines search_children, the second calls it.
// TODO: 92%; some locals sit 4 bytes off the original's stack slots.
// FUNCTION: TH16 0x44b5f0
i32 TitleInf::do_title_screen()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = TITLE_ITEM_COUNT;
        menu.wraps = 1;
        if (!g_Scorefile->any_cleared())
        {
            menu.disable(TITLE_ITEM_EXTRA_START);
        }
        if (g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE)
        {
            menu.set_cursor(TITLE_ITEM_SPELL_PRACTICE);
            g_Globals.set_game_mode(GAME_MODE_NORMAL);
        }
        else if (g_Globals.game_mode != GAME_MODE_NORMAL)
        {
            menu.set_cursor(TITLE_ITEM_PRACTICE_START);
            g_Globals.set_game_mode(GAME_MODE_NORMAL);
        }
        set_substate(1);
        if (menu_flags & TITLE_FIRST_SHOW)
        {
            anm_ids[0x61] = title_anm->create_effect(0x61, -1, NULL);
            anm_ids[0x65] = title_anm->create_effect(0x65, -1, NULL);
            menu_flags &= ~TITLE_FIRST_SHOW;
        }
        else
        {
            if (g_AnmManager->get_vm_with_id(anm_ids[0x61]) == NULL)
            {
                anm_ids[0x61] = title_anm->create_effect(0x61, -1, NULL);
                AnmManager::interrupt_tree_and_run(anm_ids[0x61], 2);
            }
            if (g_AnmManager->get_vm_with_id(anm_ids[0x65]) == NULL)
            {
                anm_ids[0x65] = title_anm->create_effect(0x65, -1, NULL);
                AnmManager::interrupt_tree_and_run(anm_ids[0x65], 2);
            }
            if (prev_state != TITLE_STATE_OPTIONS)
            {
                AnmManager::interrupt_tree_and_run(anm_ids[0x61], 2);
            }
            time_in_state.set_value(120);
        }
    case 1:
        if (time_in_state.current == 120)
        {
            anm_ids[0] = title_anm->create_effect(0, -1, NULL);
            if (get_vm_or_clear(comment_line_ids[8]) == NULL)
            {
                comment_line_ids[8] = title_v_anm->create_effect(0, -1, NULL);
            }
            i32 i;
            for (i = 0; i < menu.next_selection; i++)
            {
                interrupt_child_and_run(0, i + 3, TITLE_INTERRUPT_ABOVE_CURSOR);
                interrupt_child_and_run(0, i + 13, TITLE_INTERRUPT_ABOVE_CURSOR);
            }
            for (i++; i < 10; i++)
            {
                title_interrupt_child_and_run(this, i + 3, TITLE_INTERRUPT_BELOW_CURSOR);
                title_interrupt_child_and_run(this, i + 13, TITLE_INTERRUPT_BELOW_CURSOR);
            }
            if (!g_Scorefile->any_cleared())
            {
                interrupt_child(0, 4, TITLE_INTERRUPT_DISABLED);
                interrupt_child(0, 14, TITLE_INTERRUPT_DISABLED);
            }
        }
        if (time_in_state.current > 130)
        {
            substate = 2;
            time_in_state.reset_inline();
            AnmManager::interrupt_tree_and_run(anm_ids[0], 3);
            AnmManager::interrupt_tree(anm_ids[0], (i16)(menu.next_selection + 17));
            title_highlight_inline(this);
            if (!g_Scorefile->any_cleared())
            {
                title_interrupt_child_inline(this, 4, TITLE_INTERRUPT_DISABLED);
                title_interrupt_child_inline(this, 14, TITLE_INTERRUPT_DISABLED);
            }
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
            g_SoundManager.play_sound_centered(SE_SELECT00, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[0], 3);
            AnmManager::interrupt_tree(anm_ids[0], (i16)(menu.next_selection + 7));
            i32 i;
            for (i = 0; i < menu.next_selection; i++)
            {
                interrupt_child_and_run(0, i + 3, TITLE_INTERRUPT_ABOVE_CURSOR);
                interrupt_child_and_run(0, i + 13, TITLE_INTERRUPT_ABOVE_CURSOR);
            }
            for (i++; i < 10; i++)
            {
                interrupt_child_and_run(0, i + 3, TITLE_INTERRUPT_BELOW_CURSOR);
                interrupt_child_and_run(0, i + 13, TITLE_INTERRUPT_BELOW_CURSOR);
            }
            if (!g_Scorefile->any_cleared())
            {
                interrupt_child(0, 4, TITLE_INTERRUPT_DISABLED);
                interrupt_child(0, 14, TITLE_INTERRUPT_DISABLED);
            }
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            if (menu.next_selection == TITLE_ITEM_QUIT)
            {
                g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                set_substate(4);
                return 1;
            }
            g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
            menu.set_cursor(TITLE_ITEM_QUIT);
            AnmManager::interrupt_tree_and_run(anm_ids[0], 3);
            AnmManager::interrupt_tree(anm_ids[0], (i16)(menu.next_selection + 7));
            title_highlight_inline(this);
            if (!g_Scorefile->any_cleared())
            {
                title_interrupt_child(this, 4, TITLE_INTERRUPT_DISABLED);
                title_interrupt_child(this, 14, TITLE_INTERRUPT_DISABLED);
            }
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            AnmManager::interrupt_tree(anm_ids[0], 6);
            switch (menu.next_selection)
            {
            case TITLE_ITEM_START:
            case TITLE_ITEM_EXTRA_START:
            case TITLE_ITEM_PRACTICE_START:
            case TITLE_ITEM_SPELL_PRACTICE:
            case TITLE_ITEM_REPLAY:
            case TITLE_ITEM_PLAYER_DATA:
            case TITLE_ITEM_MUSIC_ROOM:
            case TITLE_ITEM_MANUAL:
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                AnmManager::interrupt_tree(anm_ids[0x65], 1);
                anm_ids[0x65].id = 0;
                AnmManager::interrupt_tree(comment_line_ids[8], 1);
                set_substate(4);
                return 1;
            case TITLE_ITEM_OPTION:
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                set_substate(4);
                return 1;
            case TITLE_ITEM_QUIT:
                g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                set_substate(4);
                return 1;
            }
        }
        break;
    case 4:
        if (time_in_state.current < 20)
        {
            break;
        }
        switch (menu.next_selection)
        {
        case TITLE_ITEM_START:
            g_Globals.set_game_mode(GAME_MODE_NORMAL);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(TITLE_STATE_DIFFICULTY_SELECT);
            menu.push();
            menu.set_cursor(g_last_difficulty);
            g_Globals.difficulty = g_last_difficulty;
            return 1;
        case TITLE_ITEM_EXTRA_START:
            g_Globals.set_game_mode(GAME_MODE_NORMAL);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(TITLE_STATE_DIFFICULTY_SELECT);
            menu.push();
            g_Globals.difficulty = DIFFICULTY_EXTRA;
            menu.set_cursor(0);
            return 1;
        case TITLE_ITEM_PRACTICE_START:
            g_Globals.set_game_mode(GAME_MODE_STAGE_PRACTICE);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            menu.push();
            menu.set_cursor(g_last_difficulty);
            set_state(TITLE_STATE_DIFFICULTY_SELECT);
            g_Globals.difficulty = g_last_difficulty;
            return 1;
        case TITLE_ITEM_SPELL_PRACTICE:
            g_Globals.set_game_mode(GAME_MODE_SPELL_PRACTICE);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(TITLE_STATE_SPELL_PRACTICE_STAGE_SELECT);
            menu.push();
            spell_character_menu.wraps = 1;
            spell_character_menu.num_choices = 4;
            spell_character_menu.set_cursor(g_last_character);
            menu.set_cursor(0);
            return 1;
        case TITLE_ITEM_REPLAY:
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(TITLE_STATE_REPLAY_MENU);
            menu.push();
            return 1;
        case TITLE_ITEM_PLAYER_DATA:
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(TITLE_STATE_PLAYER_DATA);
            menu.push();
            return 1;
        case TITLE_ITEM_MUSIC_ROOM:
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(TITLE_STATE_MUSIC_ROOM);
            menu.push();
            return 1;
        case TITLE_ITEM_OPTION:
            set_state(TITLE_STATE_OPTIONS);
            menu.push();
            return 1;
        case TITLE_ITEM_MANUAL:
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(TITLE_STATE_MANUAL);
            menu.push();
            return 1;
        case TITLE_ITEM_QUIT:
            set_state(TITLE_STATE_EXIT);
            break;
        }
        break;
    }
    return 1;
}
