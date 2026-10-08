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

// The number of pages; help.anm scripts 0-8 are the list's entries, and
// script 9 shows the page's picture.
#define HELP_PAGE_COUNT 9
#define HELP_ANM_PAGE_SCRIPT 9

// GLOBAL: TH16 0x4a6dd8
HelpManual *g_HelpManual;

// FUNCTION: TH16 0x42e6a0
HelpManual::HelpManual()
{
    memset(this, 0, sizeof(HelpManual));
    flags |= 2;
    g_HelpManual = this;
}

// Runs on the Supervisor's worker thread; starts the manual once help.anm
// is loaded.
// FUNCTION: TH16 0x42e760
void help_manual_load_anm()
{
    g_HelpManual->help_anm = AnmManager::preload_anm(ANM_SLOT_HELP, "help.anm");
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

// Runs on the Supervisor's worker thread: reads the page's picture.
// FUNCTION: TH16 0x42e7c0
void help_manual_read_file()
{
    g_HelpManual->file_data = file_read_all(g_HelpManual->file_name, &g_HelpManual->file_size, 0);
    g_HelpManual->substate = HELP_SUBSTATE_LOADED;
    g_Supervisor.thread.should_run = FALSE;
    g_Supervisor.thread.stop_requested = TRUE;
}

// Registers the update functions (inactive until help.anm is loaded) and
// loads help.anm on the Supervisor's worker thread.
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
    state = HELP_STATE_START;
    return 0;
}

// FUNCTION: TH16 0x42e910
HelpManual::~HelpManual()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    AnmManager *anm = g_AnmManager;
    if (anm->loaded_anms[ANM_SLOT_HELP] != NULL)
    {
        anm->loaded_anms[ANM_SLOT_HELP]->release();
        delete anm->loaded_anms[ANM_SLOT_HELP];
        anm->loaded_anms[ANM_SLOT_HELP] = NULL;
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

// Shows the page list with the cursor's page highlighted (interrupt 2; the
// others get 3).
static __forceinline void help_highlight_pages(HelpManual *manual)
{
    for (i32 i = 0; i < HELP_PAGE_COUNT; i++)
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
    for (i32 i = 0; i < HELP_PAGE_COUNT; i++)
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
    for (i32 i = 0; i < HELP_PAGE_COUNT; i++)
    {
        AnmManager::interrupt_tree(manual->page_vms[i], 1);
    }
}

// The menu helpers the list inlines; they address the menu through a
// pointer, as the original's code does ([edi + 4] for the selection).
static __forceinline void help_menu_save(MenuHelper *m)
{
    m->current_selection = m->next_selection;
}
static __forceinline i32 help_menu_moved(MenuHelper *m)
{
    return m->current_selection != m->next_selection;
}

// The manual's frame: choose a page from the list (up and down; shot or
// enter opens it, bomb or menu closes the manual), read its picture on the
// worker thread, then show it (up and down turn the page, cancel goes back
// to the list). Sounds 10, 7 and 9 are the cursor, select and cancel ones.
// safebuffers (on the declaration) drops the /GS cookie ours gets for pos,
// whose address goes to create_ui_vm; the original has none. The timers are
// the inlined reset and the tick_goto form, as the original inlines them.
// FUNCTION: TH16 0x42eab0
DECOMP_NOINLINE i32 HelpManual::on_tick_body()
{
    // A dead double: it makes LTCG realign this frame (and esp, -8) early,
    // as the original does, which also gives create_ui_vm its padded frame.
    // It stands in for AnmVm::run wanting an aligned stack (docs/findings.md).
    double unused = 0.0;
    (void)unused;
    D3DXVECTOR3 pos;
    pos.y = 0.0f;
    pos.z = 0.0f;
    pos.x = x_offset;
    switch (state)
    {
    case HELP_STATE_START:
        state = HELP_STATE_RUN;
        break;
    case HELP_STATE_RUN:
        switch (substate)
        {
        case HELP_SUBSTATE_SETUP:
            menu.num_choices = HELP_PAGE_COUNT;
            menu.set_cursor(0);
            menu.wraps = 1;
            help_create_pages(this, &pos);
            help_anm->d3d[1].clear_texture();
            substate = HELP_SUBSTATE_LIST;
        case HELP_SUBSTATE_LIST:
            if (timer.current < 20)
            {
                break;
            }
            help_menu_save(&menu);
            if (help_pressed_or_repeating(INPUT_UP))
            {
                menu.move_cursor(-1);
            }
            if (help_pressed_or_repeating(INPUT_DOWN))
            {
                menu.move_cursor(1);
            }
            if (help_menu_moved(&menu))
            {
                g_SoundManager.play_sound_centered(SE_SELECT00, 0);
                help_highlight_pages(this);
            }
            if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT))
            {
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                goto open_page;
            }
            if (g_hardware_input_pressed & (INPUT_MENU | INPUT_BOMB))
            {
                g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                help_hide_pages(this);
                state = HELP_STATE_CLOSE;
                substate = HELP_SUBSTATE_SETUP;
                timer.reset_inline();
            }
            break;
        case HELP_SUBSTATE_LOADING:
            break;
        case HELP_SUBSTATE_LOADED:
            g_AnmManager->reload_texture(&help_anm->d3d[1], file_data, file_size, 0, 0, 0);
            if (file_data != NULL)
            {
                free(file_data);
                file_data = NULL;
            }
            file_data = NULL;
            help_anm->d3d[1].texture->PreLoad();
            page_vms[HELP_PAGE_COUNT] = help_anm->create_ui_vm(HELP_ANM_PAGE_SCRIPT, &pos, 0);
            substate = HELP_SUBSTATE_PAGE;
            timer.set_value(0);
        case HELP_SUBSTATE_PAGE:
            if (timer.current < 20)
            {
                break;
            }
            if ((g_hardware_input_pressed & INPUT_DOWN) && menu.next_selection < HELP_PAGE_COUNT - 1)
            {
                substate = HELP_SUBSTATE_TURNING;
                timer.set_value(0);
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                menu.move_cursor(1);
                AnmManager::interrupt_tree_and_run(page_vms[HELP_PAGE_COUNT], 7);
            }
            else if ((g_hardware_input_pressed & INPUT_UP) && menu.next_selection > 0)
            {
                substate = HELP_SUBSTATE_TURNING;
                timer.set_value(0);
                g_SoundManager.play_sound_centered(SE_OK00, 0);
                menu.move_cursor(-1);
                AnmManager::interrupt_tree_and_run(page_vms[HELP_PAGE_COUNT], 8);
            }
            else
            {
                if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_MENU | INPUT_BOMB | INPUT_SHOT))
                {
                    g_SoundManager.play_sound_centered(SE_CANCEL00, 0);
                    substate = HELP_SUBSTATE_LIST;
                    timer.set_value(0);
                    AnmManager::interrupt_tree(page_vms[HELP_PAGE_COUNT], 1);
                    help_create_pages(this, &pos);
                }
                break;
            }
        case HELP_SUBSTATE_TURNING:
            if (timer.current < 20)
            {
                break;
            }
        open_page:
            substate = HELP_SUBSTATE_LOADING;
            timer.set_value(0);
            sprintf(file_name, "help_%.2d.png", menu.next_selection + 1);
            g_Supervisor.start_thread((ThreadStart)help_manual_read_file, NULL);
            help_hide_pages(this);
            break;
        }
        break;
    case HELP_STATE_CLOSE:
        if (timer.current >= 30)
        {
            closed = 1;
        }
        break;
    }
    timer.tick_goto();
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x42ef90
i32 __fastcall HelpManual::on_tick_callback(HelpManual *manual)
{
    return manual->on_tick_body();
}

// FUNCTION: TH16 0x42efa0
i32 __fastcall HelpManual::on_draw_callback(HelpManual *manual)
{
    return UPDATE_FUNC_CONTINUE;
}

// Creates a VM running script at pos in the UI list (like create_ui_effect,
// with a position).
// TODO: the original frame has 4 more bytes: padded for the alignment its caller
// provides, which needs AnmVm::run to want an aligned stack (see create_vm).
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
