#include <process.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MainMenu.h"

#include "GameErrorContext.h"
#include "LoadingThread.h"
#include "ReplayManager.h"
#include "Scorefile.h"
#include "Supervisor.h"

static_assert(offsetof(TitleInf, state) == 0x18, "TitleInf::state");
static_assert(offsetof(TitleInf, menu) == 0x24, "TitleInf::menu");
static_assert(offsetof(TitleInf, time_in_state) == 0x2ac, "TitleInf::time_in_state");
static_assert(offsetof(TitleInf, anm_ids) == 0x2c0, "TitleInf::anm_ids");
static_assert(offsetof(TitleInf, anm_id_73c) == 0x73c, "TitleInf::anm_id_73c");
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

// FUNCTION: TH16 0x44a5e0
void TitleInf::set_substate(i32 substate)
{
    this->substate = substate;
    time_in_state.reset();
}

// FUNCTION: TH16 0x44a650
void TitleInf::interrupt_and_clear(i32 index)
{
    AnmManager::interrupt_tree(anm_ids[index], 1);
    anm_ids[index].id = 0;
}

// TODO: the original keeps g_AnmManager in edi across the lookups, which
// needs get_vm_with_id visible to LTCG (an opaque stub for now).
// FUNCTION: TH16 0x44a680
HARNESS_CALLED void TitleInf::interrupt_child(i32 index, i32 script, i32 interrupt)
{
    AnmVm *vm = find_child_of(anm_ids[index], script);
    vm->interrupt(interrupt);
}

// TODO: as interrupt_child; the original also keeps a stack slot (push ecx).
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
        g_AnmManager->unload_anm(1);
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

    title_anm = AnmManager::preload_anm(0x10, "title.anm");
    if (title_anm == NULL)
    {
        // データが壊れています ("The data is corrupted")
        g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    title_v_anm = AnmManager::preload_anm(0x11, "title_v.anm");
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
    g_AnmManager->unload_anm(0x10);
    g_AnmManager->unload_anm(0x11);
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

// TODO: the original realigns its frame (and esp, -8), most likely for its
// callees, which are opaque stubs here.
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

// FUNCTION: TH16 0x44a800
i32 Scorefile::has_cleared(i32 character)
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
    if (has_cleared(0) || has_cleared(1) || has_cleared(2) || has_cleared(3))
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
