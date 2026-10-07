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
static_assert(offsetof(TitleInf, anm_id_73c) == 0x73c, "TitleInf::anm_id_73c");
static_assert(offsetof(TitleInf, music_track_count) == 0x7f4, "TitleInf::music_track_count");
static_assert(offsetof(TitleInf, music_filenames) == 0x804, "TitleInf::music_filenames");
static_assert(offsetof(TitleInf, music_titles) == 0x1004, "TitleInf::music_titles");
static_assert(offsetof(TitleInf, music_comments) == 0x1844, "TitleInf::music_comments");
static_assert(offsetof(TitleInf, music_scroll) == 0x5a44, "TitleInf::music_scroll");
static_assert(offsetof(TitleInf, replay_name) == 0x5a48, "TitleInf::replay_name");
static_assert(offsetof(TitleInf, menu_5a5c) == 0x5a5c, "TitleInf::menu_5a5c");
static_assert(offsetof(TitleInf, key_config) == 0x5b34, "TitleInf::key_config");
static_assert(offsetof(TitleInf, unk_5b44) == 0x5b44, "TitleInf::unk_5b44");
static_assert(offsetof(TitleInf, replay_slot) == 0x5b48, "TitleInf::replay_slot");
static_assert(offsetof(TitleInf, replay_stage) == 0x5b4c, "TitleInf::replay_stage");
static_assert(offsetof(TitleInf, replays) == 0x5b50, "TitleInf::replays");
static_assert(offsetof(TitleInf, thread) == 0x5de4, "TitleInf::thread");
static_assert(sizeof(TitleInf) == 0x5e00, "TitleInf");

extern i32 g_unk_4c0f40;

// GLOBAL: TH16 0x4a6f20
TitleInf *g_MainMenu;

// FUNCTION: TH16 0x44a550
u32 TitleInf::get_size()
{
    return sizeof(TitleInf);
}

// FUNCTION: TH16 0x44a560
void TitleInf::set_state(i32 state)
{
    prev_state = this->state;
    this->state = state;
    substate = 0;
    time_in_state.reset();
}

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

// TODO: the original pads its frame (push ecx): AnmVm::run needs 8-byte alignment there at IL level and do_title_screen provides it (see README, AnmVm::run's alignment); not when HARNESS_CALLED alone.
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

// FUNCTION: TH16 0x44abc0
unsigned __stdcall TitleInf::thread_start()
{
    if (g_MainMenu->initialize() != 0)
    {
        g_Supervisor.gamemode_to_switch_to = (g_Supervisor.flags & 0x2000) ? 2 : 3;
        return 0;
    }
    if (g_LoadingThread != NULL)
    {
        while (g_LoadingThread->count_638 < 180 && !(g_Supervisor.flags & 0x180))
        {
            Sleep(16);
        }
        g_AnmManager->unload_anm(ANM_SLOT_SIG);
    }
    g_MainMenu->on_tick_func->flags |= UPDATE_FUNC_ACTIVE;
    g_unk_4d9d90 = 1;
    return 0;
}

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
    g_unk_4c0f40 = 0;
    return 0;
}

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
    AnmManager::interrupt_tree(anm_id_73c, 1);
    if (unk_5ce0 != NULL)
    {
        free(unk_5ce0);
        unk_5ce0 = NULL;
    }
    g_MainMenu = NULL;
}

// FUNCTION: TH16 0x44aee0
TitleInf *TitleInf::create()
{
    TitleInf *menu = new TitleInf;
    g_unk_4d9d90 = 0;
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
    case 11:
        on_draw__replay();
        break;
    case 10:
        on_draw__player_data();
        break;
    case 14:
        on_draw__4538b0();
        break;
    case 15:
        on_draw__4541b0();
        break;
    case 8:
        on_draw__practice_stage_select();
        break;
    case 18:
    case 19:
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

// TODO: the volume clamps use al/ecx where ours uses cl/eax (also with an if instead of the ternary).
// FUNCTION: TH16 0x44c570
i32 TitleInf::do_options()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = 5;
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
            g_SoundManager.play_sound_centered(10, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[1], 3);
            AnmManager::interrupt_tree(anm_ids[1], (i16)(menu.next_selection + 7));
            update_options_cursor();
        }
        if (g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB))
        {
            if (menu.next_selection != 4)
            {
                g_SoundManager.play_sound_centered(9, 0);
                menu.set_cursor(4);
                AnmManager::interrupt_tree_and_run(anm_ids[1], 3);
                AnmManager::interrupt_tree(anm_ids[1], (i16)(menu.next_selection + 7));
                update_options_cursor();
                return 1;
            }
            goto leave;
        }
        if (menu.next_selection == 1 && time_in_state.ticked_on_multiple_of(60))
        {
            g_SoundManager.play_sound_centered(2, 0);
        }
        if (input_pressed_or_repeating(INPUT_LEFT))
        {
            switch (menu.next_selection)
            {
            case 0:
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
            case 1:
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
            case 0:
            {
                i8 volume = g_Supervisor.config.bgm_volume + 5;
                g_Supervisor.config.bgm_volume = volume > 100 ? 100 : volume;
                update_options_sprites();
                break;
            }
            case 1:
            {
                i8 volume = g_Supervisor.config.se_volume + 5;
                g_Supervisor.config.se_volume = volume > 100 ? 100 : volume;
                update_options_sprites();
                break;
            }
            }
        }
        if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
        {
            switch (menu.next_selection)
            {
            case 2:
                AnmManager::interrupt_tree(anm_ids[1], 6);
                g_SoundManager.play_sound_centered(7, 0);
                set_substate(4);
                return 1;
            case 3:
                g_Supervisor.config.bgm_volume = 100;
                g_Supervisor.config.se_volume = 80;
                g_Supervisor.config.unk_28 = 0;
                update_options_sprites();
                g_SoundManager.play_sound_centered(7, 0);
                return 1;
            case 4:
            leave:
                AnmManager::interrupt_tree(anm_ids[1], 6);
                g_SoundManager.play_sound_centered(9, 0);
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
            case 2:
                set_state(4);
                menu.push();
                break;
            case 4:
                set_state(1);
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
// TODO: the original keeps g_AnmManager in esi/ebx across the lookups (get_vm_with_id is an opaque stub here), which changes the inlined searches' registers.
// FUNCTION: TH16 0x44c8c0
void TitleInf::update_options_cursor()
{
    i32 i;
    for (i = 0; i < menu.next_selection; i++)
    {
        interrupt_child_and_run(1, i + 0x17, 0x1e);
        interrupt_child_and_run(1, i + 0x1c, 0x1e);
    }
    for (i++; i < 5; i++)
    {
        interrupt_child_and_run(1, i + 0x17, 0x1f);
        interrupt_child_and_run(1, i + 0x1c, 0x1f);
    }
    if (menu.next_selection > 0)
    {
        interrupt_child_and_run_inline(anm_ids[1], 0x21, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x22, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x23, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x24, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x25, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x26, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x27, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x28, 0x1e);
    }
    if (menu.next_selection > 1)
    {
        interrupt_child_and_run_inline(anm_ids[1], 0x29, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x2a, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x2b, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x2c, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x2d, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x2e, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x2f, 0x1e);
        interrupt_child_and_run_inline(anm_ids[1], 0x30, 0x1e);
    }
    else if (menu.next_selection < 1)
    {
        interrupt_child_and_run(1, 0x29, 0x1f);
        interrupt_child_and_run(1, 0x2a, 0x1f);
        interrupt_child_and_run(1, 0x2b, 0x1f);
        interrupt_child_and_run(1, 0x2c, 0x1f);
        interrupt_child_and_run(1, 0x2d, 0x1f);
        interrupt_child_and_run(1, 0x2e, 0x1f);
        interrupt_child_and_run(1, 0x2f, 0x1f);
        interrupt_child_and_run(1, 0x30, 0x1f);
    }
}

// Applies the volumes and shows them: three digits each, in two layers of
// sprites, with leading zeros hidden.
// FUNCTION: TH16 0x44dc70
void TitleInf::update_options_sprites()
{
    g_SoundManager.bgm_volume = g_Supervisor.config.bgm_volume;
    g_SoundManager.modify_bgm(8, 0, "SetVol");
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

// TODO: for the up/down tests the original loads the pressed and repeat words as bytes (mov cl/al); ours loads dwords (byte casts only narrow the repeat load).
// FUNCTION: TH16 0x44e930
i32 TitleInf::do_key_config()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = 7;
        menu.set_cursor(0);
        anm_ids[2] = title_anm->create_effect(2, -1, NULL);
        set_substate(1);
        key_config[0] = g_pad_mapping[0];
        key_config[1] = g_pad_mapping[1];
        key_config[2] = g_pad_mapping[9];
        key_config[3] = g_pad_mapping[2];
        key_config[4] = g_pad_mapping[3];
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
            AnmManager::interrupt_tree_and_run(anm_ids[2], 3);
            AnmManager::interrupt_tree(anm_ids[2], (i16)(menu.next_selection + 7));
            update_key_config_cursor();
        }
        u8 *pad = get_controller_state();
        for (i32 i = 0; i < 0x1f; i++)
        {
            if ((i8)pad[i] < 0)
            {
                if (menu.next_selection <= 4)
                {
                    set_key(menu.next_selection, i);
                }
                break;
            }
        }
        if ((g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB)) && menu.next_selection == 6)
        {
            key_config[0] = g_pad_mapping[0];
            key_config[1] = g_pad_mapping[1];
            key_config[2] = g_pad_mapping[9];
            key_config[3] = g_pad_mapping[2];
            key_config[4] = g_pad_mapping[3];
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
            case 5:
                key_config[0] = g_pad_mapping[0];
                key_config[1] = g_pad_mapping[1];
                key_config[2] = g_pad_mapping[9];
                key_config[3] = g_pad_mapping[2];
                key_config[4] = g_pad_mapping[3];
                update_key_config_sprites();
                g_SoundManager.play_sound_centered(7, 0);
                return 1;
            case 6:
                g_pad_mapping[0] = key_config[0];
                g_pad_mapping[1] = key_config[1];
                g_pad_mapping[9] = key_config[2];
                g_pad_mapping[2] = key_config[3];
                g_pad_mapping[3] = key_config[4];
                memcpy(g_Supervisor.config.pad_mapping_copy, g_pad_mapping, sizeof(g_pad_mapping));
                break;
            default:
                return 1;
            }
        }
        g_SoundManager.play_sound_centered(9, 0);
        AnmManager::interrupt_tree(anm_ids[2], 6);
        set_substate(4);
        return 1;
    }
    case 4:
        if (time_in_state.current >= 10)
        {
            set_state(3);
            menu.pop();
        }
        break;
    }
    return 1;
}

// Two digits per action, in two layers of sprites.
// TODO: the original keeps g_AnmManager in edi across the lookups (get_vm_with_id is an opaque stub here).
// FUNCTION: TH16 0x44ec60
void TitleInf::update_key_config_sprites()
{
    set_child_sprite(get_child_vm(anm_ids[2], 0x3f), key_config[0] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x40), key_config[0] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x49), key_config[0] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4a), key_config[0] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x41), key_config[1] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x42), key_config[1] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4b), key_config[1] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4c), key_config[1] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x43), key_config[2] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x44), key_config[2] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4d), key_config[2] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4e), key_config[2] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x45), key_config[3] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x46), key_config[3] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x4f), key_config[3] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x50), key_config[3] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x47), key_config[4] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x48), key_config[4] % 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x51), key_config[4] / 10 + 0x2a);
    set_child_sprite(get_child_vm(anm_ids[2], 0x52), key_config[4] % 10 + 0x2a);
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
    g_SoundManager.play_sound_centered(7, 0);
}

// Rows above the cursor get interrupt 30, rows below it 31; the five
// remappable actions have two more pairs of sprites each.
// FUNCTION: TH16 0x44f810
void TitleInf::update_key_config_cursor()
{
    i32 i;
    for (i = 0; i < menu.next_selection; i++)
    {
        interrupt_child_and_run(2, i + 0x31, 0x1e);
        interrupt_child_and_run(2, i + 0x38, 0x1e);
        if (i < 5)
        {
            interrupt_child_and_run(2, i * 2 + 0x3f, 0x1e);
            interrupt_child_and_run(2, i * 2 + 0x40, 0x1e);
            interrupt_child_and_run(2, i * 2 + 0x49, 0x1e);
            interrupt_child_and_run(2, i * 2 + 0x4a, 0x1e);
        }
    }
    for (i++; i < 7; i++)
    {
        interrupt_child_and_run(2, i + 0x31, 0x1f);
        interrupt_child_and_run(2, i + 0x38, 0x1f);
    }
    for (i = menu.next_selection + 1; i < 5; i++)
    {
        interrupt_child_and_run(2, i * 2 + 0x3f, 0x1f);
        interrupt_child_and_run(2, i * 2 + 0x40, 0x1f);
        interrupt_child_and_run(2, i * 2 + 0x49, 0x1f);
        interrupt_child_and_run(2, i * 2 + 0x4a, 0x1f);
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

// TODO: the original computes the demo index as x % -3 would (imul 0x55555555; sub; sar 1); ours uses idiv, or the /3 magic through a local (also with % -3); it also calls the Supervisor members without this and keeps the replay info in ecx.
// FUNCTION: TH16 0x44af80
i32 TitleInf::on_tick()
{
    if (state == 1)
    {
        g_title_idle_frames++;
        if ((u16)g_hardware_input != 0)
        {
            g_title_idle_frames = 0;
        }
        else if (g_title_idle_frames >= 1800)
        {
            // Idle on the title screen for 30 seconds: play a demo replay.
            g_Globals.flags_hi_45c = (g_Globals.flags_hi_45c & ~2) | 1;
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
            g_Globals.stage_num = stage;
            g_Globals.weird_stage_num = stage;
            g_Supervisor.gamemode_to_switch_to = 13;
            RpyInfo *info = replay->info;
            g_stage_data = &g_stage_table[stage];
            g_Globals.character = info->character;
            g_Globals.subshot = info->subshot;
            g_Globals.subseason = info->subseason;
            g_Globals.difficulty_before_demo = g_Globals.difficulty;
            g_Globals.difficulty = info->difficulty;
            replay->~ReplayManager();
            operator delete(replay, sizeof(ReplayManager));
            g_unk_4a6f1c = 1;
            g_title_idle_frames = 0;
        }
    }
    if (flags_5ce8 & 1)
    {
        unk_5ce4++;
        if (unk_5ce4 >= 10)
        {
            if (g_Supervisor.config.flags_2c & 0x10)
            {
                g_SoundManager.modify_bgm(4, 0, "dummy");
            }
            else
            {
                g_SoundManager.modify_bgm(3, 0, "dummy");
            }
            g_SoundManager.bgm_name[0] = 0;
            g_Supervisor.play_bgm_wav(0, "th16_01");
            if (g_Supervisor.config.flags_2c & 0x10)
            {
                g_SoundManager.modify_bgm(4, 0, "dummy");
            }
            g_SoundManager.modify_bgm(2, 0, "dummy");
            g_Scorefile->bgm_unlocked[0] = 1;
            flags_5ce8 &= ~1;
            unk_5ce4 = 0;
        }
    }
    switch (state)
    {
    case 0:
    {
        AnmManager *anm = g_AnmManager;
        anm->disable_vms_from_anm_file(anm->loaded_anms[ANM_SLOT_FRONT]);
        anm->disable_vms_from_anm_file(anm->loaded_anms[ANM_SLOT_BULLET]);
        anm->disable_vms_from_anm_file(anm->loaded_anms[ANM_SLOT_ASCII]);
        anm->disable_vms_from_anm_file(anm->loaded_anms[ANM_SLOT_TEXT]);
        g_AsciiManager->hide_now_loading();
        if (g_unk_4a6f1c == 3)
        {
            menu.num_choices = 10;
            menu.set_cursor(0);
            menu.push();
            set_state(14);
            g_unk_4a6f1c = 1;
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_score_name_entry();
            break;
        }
        if (!(g_Globals.flags_hi_45c & 1))
        {
            flags_5ce8 |= 1;
            unk_5ce4 = 0;
        }
        else
        {
            flags_5ce8 &= ~1;
            g_Globals.difficulty = g_Globals.difficulty_before_demo;
        }
        g_Globals.flags_hi_45c &= ~1;
        if (g_unk_4a6f1c == 0)
        {
            flags_5ce8 |= 2;
            set_state(1);
            g_unk_4a6f1c = 1;
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_title_screen();
            break;
        }
        flags_5ce8 &= ~2;
        if (g_unk_4a6f1c == 1)
        {
            if (g_Globals.difficulty == 4)
            {
                menu.set_cursor(1);
            }
            set_state(1);
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_title_screen();
        }
        else if (g_unk_4a6f1c == 2)
        {
            g_Supervisor.play_bgm_wav(0, "th16_01");
            g_Supervisor.play_bgm(0, 0);
            g_Globals.set_game_mode(0);
            menu.num_choices = 10;
            menu.set_cursor(4);
            menu.push();
            set_state(11);
            g_unk_4a6f1c = 1;
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_replay_menu();
        }
        else if (g_unk_4a6f1c == 5)
        {
            ScreenEffect::create(SCREEN_EFFECT_HOLD, 30, 0, 0, 0, 0x54);
            anm_ids[0x61] = title_anm->create_effect(0x61, -1, NULL);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(17);
            g_unk_4a6f1c = 1;
            on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
            do_spell_practice_stage_select();
        }
        else if (g_unk_4a6f1c == 4)
        {
            ScreenEffect::create(SCREEN_EFFECT_HOLD, 30, 0, 0, 0, 0x54);
            anm_ids[0x61] = title_anm->create_effect(0x61, -1, NULL);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(5);
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
    case 1:
        on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
        do_title_screen();
        break;
    case 5:
        do_difficulty_select();
        break;
    case 11:
        do_replay_menu();
        break;
    case 14:
        do_score_name_entry();
        break;
    case 17:
        do_spell_practice_stage_select();
        break;
    case 3:
        do_options();
        break;
    case 4:
        do_key_config();
        break;
    case 2:
        g_Supervisor.gamemode_to_switch_to = (g_Supervisor.flags & SUPERVISOR_FLAG_2000) ? 2 : 3;
        g_Supervisor.stop_bgm();
        break;
    case 9:
        g_Supervisor.stop_bgm();
        break;
    case 12:
        g_Supervisor.stop_bgm();
        break;
    case 6:
        do_character_select();
        break;
    case 7:
        do_subseason_select();
        break;
    case 8:
        do_practice_stage_select();
        break;
    case 13:
        do_music_room();
        break;
    case 18:
        do_spell_practice_row();
        break;
    case 19:
        do_spell_practice_difficulty();
        break;
    case 20:
        do_spell_practice_subseason();
        break;
    case 10:
        do_player_data();
        break;
    case 16:
        do_manual();
        break;
    case 15:
        do_replay_save();
        break;
    }
    time_in_state.tick();
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
        title_interrupt_child_and_run(menu, i + 3, 30);
        title_interrupt_child_and_run(menu, i + 13, 30);
    }
    for (i++; i < 10; i++)
    {
        title_interrupt_child_and_run(menu, i + 3, 31);
        title_interrupt_child_and_run(menu, i + 13, 31);
    }
}

// TODO: functionally complete; register allocation and the choice of inlined vs called menu highlight copies differ in places.
// FUNCTION: TH16 0x44b5f0
i32 TitleInf::do_title_screen()
{
    switch (substate)
    {
    case 0:
        menu.num_choices = 10;
        menu.wraps = 1;
        if (!g_Scorefile->any_cleared())
        {
            menu.disable(1);
        }
        if (g_Globals.game_mode == 2)
        {
            menu.set_cursor(3);
            g_Globals.set_game_mode(0);
        }
        else if (g_Globals.game_mode != 0)
        {
            menu.set_cursor(2);
            g_Globals.set_game_mode(0);
        }
        set_substate(1);
        if (flags_5ce8 & 2)
        {
            anm_ids[0x61] = title_anm->create_effect(0x61, -1, NULL);
            anm_ids[0x65] = title_anm->create_effect(0x65, -1, NULL);
            flags_5ce8 &= ~2;
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
            if (prev_state != 3)
            {
                AnmManager::interrupt_tree_and_run(anm_ids[0x61], 2);
            }
            time_in_state.set_value(120);
        }
    case 1:
        if (time_in_state.current == 120)
        {
            anm_ids[0] = title_anm->create_effect(0, -1, NULL);
            if (get_vm_or_clear(anm_ids_7d0[8]) == NULL)
            {
                anm_ids_7d0[8] = title_v_anm->create_effect(0, -1, NULL);
            }
            i32 i;
            for (i = 0; i < menu.next_selection; i++)
            {
                interrupt_child_and_run(0, i + 3, 30);
                interrupt_child_and_run(0, i + 13, 30);
            }
            for (i++; i < 10; i++)
            {
                title_interrupt_child_and_run(this, i + 3, 31);
                title_interrupt_child_and_run(this, i + 13, 31);
            }
            if (!g_Scorefile->any_cleared())
            {
                interrupt_child(0, 4, 29);
                interrupt_child(0, 14, 29);
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
                title_interrupt_child(this, 4, 29);
                title_interrupt_child(this, 14, 29);
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
            g_SoundManager.play_sound_centered(10, 0);
            AnmManager::interrupt_tree_and_run(anm_ids[0], 3);
            AnmManager::interrupt_tree(anm_ids[0], (i16)(menu.next_selection + 7));
            i32 i;
            for (i = 0; i < menu.next_selection; i++)
            {
                interrupt_child_and_run(0, i + 3, 30);
                interrupt_child_and_run(0, i + 13, 30);
            }
            for (i++; i < 10; i++)
            {
                interrupt_child_and_run(0, i + 3, 31);
                interrupt_child_and_run(0, i + 13, 31);
            }
            if (!g_Scorefile->any_cleared())
            {
                interrupt_child(0, 4, 29);
                interrupt_child(0, 14, 29);
            }
        }
        if (g_hardware_input_pressed & (INPUT_BOMB | INPUT_MENU))
        {
            if (menu.next_selection == 9)
            {
                g_SoundManager.play_sound_centered(9, 0);
                set_substate(4);
                return 1;
            }
            g_SoundManager.play_sound_centered(9, 0);
            menu.set_cursor(9);
            AnmManager::interrupt_tree_and_run(anm_ids[0], 3);
            AnmManager::interrupt_tree(anm_ids[0], (i16)(menu.next_selection + 7));
            title_highlight_inline(this);
            if (!g_Scorefile->any_cleared())
            {
                title_interrupt_child(this, 4, 29);
                title_interrupt_child(this, 14, 29);
            }
        }
        if (g_hardware_input_pressed & (INPUT_SHOT | INPUT_ENTER))
        {
            AnmManager::interrupt_tree(anm_ids[0], 6);
            switch (menu.next_selection)
            {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 8:
                g_SoundManager.play_sound_centered(7, 0);
                AnmManager::interrupt_tree(anm_ids[0x65], 1);
                anm_ids[0x65].id = 0;
                AnmManager::interrupt_tree(anm_ids_7d0[8], 1);
                set_substate(4);
                return 1;
            case 7:
                g_SoundManager.play_sound_centered(7, 0);
                set_substate(4);
                return 1;
            case 9:
                g_SoundManager.play_sound_centered(9, 0);
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
        case 0:
            g_Globals.set_game_mode(0);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(5);
            menu.push();
            menu.set_cursor(g_last_difficulty);
            g_Globals.difficulty = g_last_difficulty;
            return 1;
        case 1:
            g_Globals.set_game_mode(0);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(5);
            menu.push();
            g_Globals.difficulty = 4;
            menu.set_cursor(0);
            return 1;
        case 2:
            g_Globals.set_game_mode(1);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            menu.push();
            menu.set_cursor(g_last_difficulty);
            set_state(5);
            g_Globals.difficulty = g_last_difficulty;
            return 1;
        case 3:
            g_Globals.set_game_mode(2);
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(17);
            menu.push();
            menu_5cec.wraps = 1;
            menu_5cec.num_choices = 4;
            menu_5cec.set_cursor(g_last_character);
            menu.set_cursor(0);
            return 1;
        case 4:
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(11);
            menu.push();
            return 1;
        case 5:
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(10);
            menu.push();
            return 1;
        case 6:
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(13);
            menu.push();
            return 1;
        case 7:
            set_state(3);
            menu.push();
            return 1;
        case 8:
            AnmManager::interrupt_tree_and_run(anm_ids[0x61], 3);
            set_state(16);
            menu.push();
            return 1;
        case 9:
            set_state(2);
            break;
        }
        break;
    }
    return 1;
}
