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

// Helpers of the music room and spell practice menus.

// The spell cards of each stage (Extra last) in spell practice: one row per
// boss attack, one id per difficulty, -1 after the last.
// GLOBAL: TH16 0x490ee0
const i32 g_spell_practice_ids[7][13][5] = {
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
// TODO: the second loop reloads *remaining each time; the original keeps the count in ecx and stores it at the loop top.
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
    *remaining = left - 1;
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
