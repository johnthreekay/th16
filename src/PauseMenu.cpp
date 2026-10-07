#include <string.h>

#include "PauseMenu.h"
#include "ReplayManager.h"
#include "GameThread.h"
#include "Gui.h"
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
