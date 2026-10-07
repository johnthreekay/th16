#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FileSystem.h"
#include "CriticalSections.h"
#include "GameErrorContext.h"
#include "HelpManual.h"
#include "SoundManager.h"
#include "Supervisor.h"

#include "Input.h"

// GLOBAL: TH16 0x4a6dd8
HelpManual *g_HelpManual;

// FUNCTION: TH16 0x42e6a0
HelpManual::HelpManual()
{
    memset(this, 0, sizeof(HelpManual));
    flags |= 2;
    g_HelpManual = this;
}

// Runs on the loading thread.
// FUNCTION: TH16 0x42e760
void help_manual_load_anm()
{
    g_HelpManual->help_anm = AnmManager::preload_anm(0x13, "help.anm");
    if (g_HelpManual->help_anm == NULL)
    {
        // "Screen layout data not found. The data is corrupted."
        g_GameErrorContext.log("\x89\xe6\x96\xca\x8d\\\x90\xac\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82"
                               "\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82"
                               "\xdc\x82\xb7\r\n");
        return;
    }
    g_Supervisor.thread.should_run = FALSE;
    g_Supervisor.thread.stop_requested = TRUE;
    g_HelpManual->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    g_HelpManual->on_draw->flags |= UPDATE_FUNC_ACTIVE;
}

// Runs on the loading thread.
// FUNCTION: TH16 0x42e7c0
void help_manual_read_file()
{
    g_HelpManual->file_data = file_read_all(g_HelpManual->file_name, &g_HelpManual->file_size, 0);
    g_HelpManual->substate = 3;
    g_Supervisor.thread.should_run = FALSE;
    g_Supervisor.thread.stop_requested = TRUE;
}

// FUNCTION: TH16 0x42e810
i32 HelpManual::initialize()
{
    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0xb);
    on_tick = f;
    // create_func, inlined here in the original (only this one).
    f = new UpdateFunc;
    f->flags |= UPDATE_FUNC_HEAP_ALLOCATED;
    f->function = (UpdateFuncCallback)on_draw_callback;
    f->on_registration = NULL;
    f->on_cleanup = NULL;
    f->arg = this;
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    g_UpdateFuncRegistry->register_on_draw(f, 0x48);
    on_draw = f;
    g_Supervisor.start_thread((ThreadStart)help_manual_load_anm, NULL);
    timer.reset();
    state = 0;
    return 0;
}

// FUNCTION: TH16 0x42e910
HelpManual::~HelpManual()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    AnmManager *anm = g_AnmManager;
    if (anm->loaded_anms[0x13] != NULL)
    {
        anm->loaded_anms[0x13]->release();
        delete anm->loaded_anms[0x13];
        anm->loaded_anms[0x13] = NULL;
    }
    g_HelpManual = NULL;
}

// FUNCTION: TH16 0x42ea30
HelpManual *HelpManual::create()
{
    HelpManual *manual = new HelpManual();
    if (manual->initialize() != 0)
    {
        delete manual;
        return NULL;
    }
    return manual;
}

// FUNCTION: TH16 0x42ea80
void HelpManual::destroy()
{
    if (g_HelpManual != NULL)
    {
        delete g_HelpManual;
    }
}

// input_pressed_or_repeating, inlined.
static __forceinline i32 help_pressed_or_repeating(u32 mask)
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

// Shows the page list with the cursor's page highlighted.
static __forceinline void help_highlight_pages(HelpManual *manual)
{
    for (i32 i = 0; i < 9; i++)
    {
        if (manual->menu.next_selection == i)
        {
            AnmManager::interrupt_tree_and_run(manual->page_vms[i], 2);
        }
        else
        {
            AnmManager::interrupt_tree_and_run(manual->page_vms[i], 3);
        }
    }
}

// Creates the page list.
static __forceinline void help_create_pages(HelpManual *manual, D3DXVECTOR3 *pos)
{
    for (i32 i = 0; i < 9; i++)
    {
        manual->page_vms[i] = manual->help_anm->create_ui_vm(i, pos, 0);
        if (manual->menu.next_selection == i)
        {
            AnmManager::interrupt_tree_and_run(manual->page_vms[i], 2);
        }
        else
        {
            AnmManager::interrupt_tree_and_run(manual->page_vms[i], 3);
        }
    }
}

static __forceinline void help_hide_pages(HelpManual *manual)
{
    for (i32 i = 0; i < 9; i++)
    {
        AnmManager::interrupt_tree(manual->page_vms[i], 1);
    }
}

// TODO: ours gets a /GS cookie where the original realigns the frame, and
// reads the input globals in a different order.
// FUNCTION: TH16 0x42eab0
DECOMP_NOINLINE i32 HelpManual::on_tick_body()
{
    D3DXVECTOR3 pos;
    pos.y = 0.0f;
    pos.z = 0.0f;
    pos.x = unk_128;
    switch (state)
    {
    case 0:
        state = 1;
        break;
    case 1:
        switch (substate)
        {
        case 0:
            menu.num_choices = 9;
            menu.set_cursor(0);
            menu.wraps = 1;
            help_create_pages(this, &pos);
            help_anm->d3d[1].clear_texture();
            substate = 1;
        case 1:
            if (timer.current < 20)
            {
                break;
            }
            menu.current_selection = menu.next_selection;
            if (help_pressed_or_repeating(0x10))
            {
                menu.move_cursor(-1);
            }
            if (help_pressed_or_repeating(0x20))
            {
                menu.move_cursor(1);
            }
            if (menu.current_selection != menu.next_selection)
            {
                g_SoundManager.play_sound_centered(10, 0);
                help_highlight_pages(this);
            }
            if (g_hardware_input_pressed & 0x80001)
            {
                g_SoundManager.play_sound_centered(7, 0);
                goto open_page;
            }
            if (g_hardware_input_pressed & 0x102)
            {
                g_SoundManager.play_sound_centered(9, 0);
                help_hide_pages(this);
                state = 2;
                substate = 0;
                timer.reset();
            }
            break;
        case 2:
            break;
        case 3:
            g_AnmManager->reload_texture(&help_anm->d3d[1], file_data, file_size, 0, 0, 0);
            if (file_data != NULL)
            {
                free(file_data);
                file_data = NULL;
            }
            file_data = NULL;
            help_anm->d3d[1].texture->PreLoad();
            page_vms[9] = help_anm->create_ui_vm(9, &pos, 0);
            substate = 4;
            timer.set_value(0);
        case 4:
            if (timer.current < 20)
            {
                break;
            }
            if ((g_hardware_input_pressed & 0x20) && menu.next_selection < 8)
            {
                substate = 5;
                timer.set_value(0);
                g_SoundManager.play_sound_centered(7, 0);
                menu.move_cursor(1);
                AnmManager::interrupt_tree_and_run(page_vms[9], 7);
            }
            else if ((g_hardware_input_pressed & 0x10) && menu.next_selection > 0)
            {
                substate = 5;
                timer.set_value(0);
                g_SoundManager.play_sound_centered(7, 0);
                menu.move_cursor(-1);
                AnmManager::interrupt_tree_and_run(page_vms[9], 8);
            }
            else
            {
                if (g_hardware_input_pressed & 0x80103)
                {
                    g_SoundManager.play_sound_centered(9, 0);
                    substate = 1;
                    timer.set_value(0);
                    AnmManager::interrupt_tree(page_vms[9], 1);
                    help_create_pages(this, &pos);
                }
                break;
            }
        case 5:
            if (timer.current < 20)
            {
                break;
            }
        open_page:
            substate = 2;
            timer.set_value(0);
            sprintf(file_name, "help_%.2d.png", menu.next_selection + 1);
            g_Supervisor.start_thread((ThreadStart)help_manual_read_file, NULL);
            help_hide_pages(this);
            break;
        }
        break;
    case 2:
        if (timer.current >= 30)
        {
            unk_124 = 1;
        }
        break;
    }
    timer.tick();
    return 1;
}

// FUNCTION: TH16 0x42ef90
i32 __fastcall HelpManual::on_tick_callback(HelpManual *manual)
{
    return manual->on_tick_body();
}

// FUNCTION: TH16 0x42efa0
i32 __fastcall HelpManual::on_draw_callback(HelpManual *manual)
{
    return 1;
}

// TODO: same frame difference as create_vm (4 more bytes, esi saved before the critical section).
// FUNCTION: TH16 0x42efb0
HARNESS_CALLED AnmId AnmLoaded::create_ui_vm(i32 script, D3DXVECTOR3 *pos, i32 unused)
{
    ENTER_CS(CS_ANM_MANAGER);
    vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    copy_vm(vm, script);
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    if (pos == NULL)
    {
        vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        vm->entity_pos = *pos;
    }
    vm->rotation.z = 0.0f;
    vm->run();
    vm->mode_of_create_child = ANM_CREATE_UI;
    AnmId id;
    id = g_AnmManager->insert_in_ui_list_back(vm);
    vm->flags_hi &= ~(ANM_VM_FREEZES_WITH_WORLD | ANM_VM_FREEZES_AFTER_FIRST_RUN);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}
